#include <iostream>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <byteswap.h>
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

#include "benchmarks.hpp"
#include "parseargs.hpp"

typedef struct pdata
{
    uint64_t buf_va;
    uint32_t buf_rkey;
} pdata;
class Client
{
public:
    Client(Arguments *args)
    {
        this->args = args;
        buffer = malloc(args->size);
        memset(buffer, '0', args->size);
    }
    ~Client()
    {
    }

    void init()
    {
        pdata repdata;
        struct addrinfo *res, *t;
        struct addrinfo hints = {
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM};

        int n, err;
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

        n = getaddrinfo(args->ip, std::to_string(args->port).c_str(), &hints, &res);
        if (n < 0)
            return;

        err = rdma_resolve_addr(server, NULL, res->ai_addr, 5000);
        if (err)
            return;

        err = rdma_get_cm_event(ec, &event);
        if (err)
            return;

        if (event->event != RDMA_CM_EVENT_ADDR_RESOLVED)
            return;

        rdma_ack_cm_event(event);

        err = rdma_resolve_route(server, 5000);
        if (err)
            return;

        err = rdma_get_cm_event(ec, &event);
        if (err)
            return;

        if (event->event != RDMA_CM_EVENT_ROUTE_RESOLVED)
            return;

        pd = ibv_alloc_pd(server->verbs);
        if (!pd)
        {
            fprintf(stderr, "alloc pd failed.\n");
            return;
        }

        cc = ibv_create_comp_channel(server->verbs);
        if (!cc)
        {
            fprintf(stderr, "create comp channel failed.\n");
            return;
        }

        send_cq = ibv_create_cq(server->verbs, 512, NULL, cc, 0);
        if (!send_cq)
        {
            fprintf(stderr, "cannot create cq.\n");
            return;
        }
        recv_cq = ibv_create_cq(server->verbs, 512, NULL, cc, 0);
        if (!send_cq)
        {
            fprintf(stderr, "cannot create cq.\n");
            return;
        }

        if (ibv_req_notify_cq(send_cq, 0))
            return;
        if (ibv_req_notify_cq(recv_cq, 0))
            return;

        mr = rdma_reg_write(server, buffer, args->size);

        qp_attr.cap.max_send_wr = args->size;
        qp_attr.cap.max_send_sge = 1;
        qp_attr.cap.max_recv_wr = args->size;
        qp_attr.cap.max_recv_sge = 1;

        qp_attr.send_cq = send_cq;
        qp_attr.recv_cq = recv_cq;
        qp_attr.qp_type = IBV_QPT_RC;

        err = rdma_create_qp(server, pd, &qp_attr);
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
        conn_param.initiator_depth = 1;
        conn_param.retry_count = 7;

        err = rdma_connect(server, &conn_param);
        if (err)
            return;
        err = rdma_get_cm_event(ec, &event);
        if (err)
        {
            fprintf(stderr, "rdma get cm event failed.\n");
        }
        if (event->event != RDMA_CM_EVENT_ESTABLISHED)
            return;
        memcpy(&server_pdata, event->param.conn.private_data, sizeof(server_pdata));
        std::cout << bswap_64(server_pdata.buf_va) << "," << ntohl(server_pdata.buf_rkey) << "\n";
        rdma_ack_cm_event(event);
    }

    void communicate()
    {
        uint8_t *notification = (uint8_t *)calloc(1, sizeof(uint8_t));
        struct ibv_mr *mr_notify = rdma_reg_msgs(server, notification, sizeof(uint8_t) + 20);
        Benchmark bench(args);
        rdma_post_recv(server, NULL, buffer, args->size, mr);
        for (int count = 0; count < args->count; ++count)
        {
            bench.singleStart();
            rdma_post_send(server, NULL, buffer, args->size, mr, IBV_SEND_SIGNALED);
            rdma_get_send_comp(server, &wc);

            rdma_get_recv_comp(server, &wc);
            rdma_post_recv(server, NULL, buffer, args->size, mr);

            /*
            // write data from local memory to remote memory.
            rdma_post_write(server, NULL, buffer, args->size, mr, 0, bswap_64(server_pdata.buf_va), ntohl(server_pdata.buf_rkey));
            rdma_get_send_comp(server, &wc);
            // notify remote host WRITE operation complete.
            rdma_post_send(server, NULL, notification, sizeof(uint8_t), mr_notify, 0);

            // wait for remote data write into memory.
            rdma_get_send_comp(server, &wc);

            rdma_get_recv_comp(server, &wc);
            rdma_post_recv(server, NULL, notification, sizeof(uint8_t), mr_notify);
            */

            bench.benchmark();
        }
        bench.evaluate(args);
        rdma_dereg_mr(mr_notify);
        free(notification);
    }

    void stop()
    {
        rdma_disconnect(server);
        rdma_destroy_qp(server);
        rdma_dereg_mr(mr);
        free(buffer);
        rdma_destroy_id(server);
        rdma_destroy_event_channel(ec);
    }

private:
    void *buffer;
    Arguments *args;
    struct rdma_event_channel *ec = NULL;
    struct rdma_cm_event *event = NULL;
    struct rdma_cm_id *server = NULL;
    struct rdma_conn_param conn_param = {};
    struct ibv_pd *pd;
    struct ibv_comp_channel *cc;
    struct ibv_cq *send_cq;
    struct ibv_cq *recv_cq;
    struct ibv_mr *mr;
    struct ibv_qp_init_attr qp_attr = {};
    struct ibv_wc wc;

    pdata server_pdata;
};

int main(int argc, char *argv[])
{
    Arguments args;
    parseArguments(&args, argc, argv);

    Client client(&args);
    client.init();
    client.communicate();
    client.stop();
    return 0;
}