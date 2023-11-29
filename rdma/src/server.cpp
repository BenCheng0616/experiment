#include <iostream>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <byteswap.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

#include "parseargs.hpp"

typedef struct pdata
{
    uint64_t buf_va;
    uint32_t buf_rkey;
} pdata;

class Server
{
public:
    Server(Arguments *args)
    {
        this->args = args;
        buffer = malloc(args->size);
        memset(buffer, '0', args->size);
    }

    ~Server()
    {
    }

    void init()
    {
        int err;
        ec = rdma_create_event_channel();
        if (!ec)
        {
            fprintf(stderr, "create event channel error.\n");
            return;
        }

        err = rdma_create_id(ec, &server, NULL, RDMA_PS_TCP);
        if (err)
        {
            fprintf(stderr, "create cm id failed.\n");
            return;
        }

        sin.sin_family = AF_INET;
        sin.sin_port = htons(args->port);
        sin.sin_addr.s_addr = INADDR_ANY;

        err = rdma_bind_addr(server, (struct sockaddr *)&sin);
        if (err)
        {
            fprintf(stderr, "cannot bind addr.\n");
            return;
        }

        err = rdma_listen(server, 1);
        if (err)
        {
            fprintf(stderr, "server listen failed.\n");
        }
    }

    void waitforClient()
    {
        int err;
        pdata repdata;
        err = rdma_get_cm_event(ec, &event);
        if (err)
        {
            fprintf(stderr, "rdma get cm event failed.\n");
            return;
        }
        if (event->event != RDMA_CM_EVENT_CONNECT_REQUEST)
            return;

        client = event->id;
        memcpy(&client_pdata, event->param.conn.private_data, sizeof(client_pdata));
        rdma_ack_cm_event(event);

        pd = ibv_alloc_pd(client->verbs);
        if (!pd)
        {
            fprintf(stderr, "alloc pd failed.\n");
            return;
        }

        cc = ibv_create_comp_channel(client->verbs);
        if (!cc)
        {
            fprintf(stderr, "create comp channel failed.\n");
            return;
        }
        send_cq = ibv_create_cq(client->verbs, 512, NULL, cc, 0);
        if (!send_cq)
        {
            fprintf(stderr, "cannot create cq.\n");
            return;
        }
        recv_cq = ibv_create_cq(client->verbs, 512, NULL, cc, 0);
        if (!recv_cq)
        {
            fprintf(stderr, "cannot create cq.\n");
            return;
        }

        if (ibv_req_notify_cq(send_cq, 0))
            return;
        if (ibv_req_notify_cq(recv_cq, 0))
            return;

        mr = rdma_reg_write(client, buffer, args->size);
        // mr = ibv_reg_mr(pd, buffer, args->size, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
        if (!mr)
        {
            fprintf(stderr, "register memory region failed.\n");
            return;
        }

        memset(&qp_attr, 0, sizeof(qp_attr));
        qp_attr.cap.max_send_wr = args->size;
        qp_attr.cap.max_send_sge = 1;
        qp_attr.cap.max_recv_wr = args->size;
        qp_attr.cap.max_recv_sge = 1;

        qp_attr.send_cq = send_cq;
        qp_attr.recv_cq = recv_cq;
        qp_attr.qp_type = IBV_QPT_RC;

        err = rdma_create_qp(client, pd, &qp_attr);
        if (err)
        {
            fprintf(stderr, "rdma cm create qp error.\n");
            return;
        }
        repdata.buf_va = bswap_64((uintptr_t)buffer);
        repdata.buf_rkey = htonl(mr->rkey);
        conn_param.responder_resources = 1;
        conn_param.private_data = &repdata;
        conn_param.private_data_len = sizeof(repdata);

        err = rdma_accept(client, &conn_param);
        if (err)
            return;
        err = rdma_get_cm_event(ec, &event);
        if (err)
        {
            fprintf(stderr, "rdma get cm event failed.\n");
        }
        if (event->event != RDMA_CM_EVENT_ESTABLISHED)
            return;
        std::cout << bswap_64(repdata.buf_va) << "," << ntohl(repdata.buf_rkey) << "\n";
        rdma_ack_cm_event(event);
    }

    void communicate()
    {
        uint8_t *notification = (uint8_t *)calloc(1, sizeof(uint8_t));
        struct ibv_mr *mr_notify = rdma_reg_msgs(client, notification, sizeof(uint8_t));
        // struct ibv_mr *mr_notify = ibv_reg_mr(pd, notification, sizeof(uint8_t), IBV_ACCESS_LOCAL_WRITE);
        rdma_post_recv(client, NULL, notification, sizeof(uint8_t), mr_notify);
        for (int count = 0; count < args->count; ++count)
        {
            rdma_get_recv_comp(client, &wc);
            rdma_post_recv(client, NULL, buffer, args->size, mr);

            rdma_post_send(client, NULL, buffer, args->size, mr, IBV_SEND_SIGNALED);
            rdma_get_send_comp(client, &wc);
            /*
            rdma_get_recv_comp(client, &wc);
            rdma_post_recv(client, NULL, notification, sizeof(uint8_t), mr_notify);
            // get write-in complete notification
            std::cout << "16\n";
            // write to remote host
            rdma_post_write(client, NULL, buffer, args->size, mr, IBV_SEND_SIGNALED, bswap_64(client_pdata.buf_va), ntohl(client_pdata.buf_rkey));
            // ibv_post_send(client->qp, &send_wr, &bad_send_wr);

            rdma_get_send_comp(client, &wc); // stock until rdma write complete
            // notify remote host memory write complete

            rdma_post_send(client, NULL, notification, sizeof(uint8_t), mr_notify, IBV_SEND_SIGNALED);
            // ibv_post_send(client->qp, &send_wr_notify, &bad_send_wr_notify);
            rdma_get_send_comp(client, &wc);
            */
        }
    }

    void stop()
    {
        int err = rdma_get_cm_event(ec, &event);
        if (event->event == RDMA_CM_EVENT_DISCONNECTED)
        {
            rdma_destroy_qp(client);
            rdma_dereg_mr(mr);
            free(buffer);
            rdma_destroy_id(server);
        }
        rdma_destroy_event_channel(ec);
    }

private:
    void *buffer;
    Arguments *args;
    struct rdma_event_channel *ec = NULL;
    struct rdma_cm_event *event = NULL;
    struct rdma_cm_id *server = NULL;
    struct rdma_cm_id *client = NULL;
    struct rdma_conn_param conn_param = {};
    struct ibv_pd *pd;
    struct ibv_comp_channel *cc;
    struct ibv_cq *send_cq;
    struct ibv_cq *recv_cq;
    struct ibv_mr *mr;
    struct ibv_qp_init_attr qp_attr = {};
    struct ibv_wc wc;
    struct sockaddr_in sin;

    pdata client_pdata;
};

int main(int argc, char *argv[])
{
    Arguments args;
    parseArguments(&args, argc, argv);
    Server server(&args);
    server.init();
    sleep(1);
    server.waitforClient();
    server.communicate();

    return 0;
}