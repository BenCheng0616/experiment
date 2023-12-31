#include "parseargs.hpp"
#include "common.hpp"

struct rdma_event_channel *cm_event_channel = NULL;
struct rdma_cm_id *cm_server_id = NULL, *cm_client_id = NULL;
struct ibv_pd *pd = NULL;
struct ibv_comp_channel *io_completion_channel = NULL;
struct ibv_cq *cq = NULL;
struct ibv_qp_init_attr qp_init_attr;
struct ibv_qp *client_qp;
struct ibv_mr *client_metadata_mr = NULL,
              *server_buffer_mr = NULL,
              *server_metadata_mr = NULL,
              *signal_mr = NULL;
struct rdma_buffer_attr client_metadata_attr, server_metadata_attr;
struct ibv_send_wr server_send_wr, *bad_server_send_wr = NULL;
struct ibv_send_wr server_send_comp_wr, *bad_server_send_comp_wr = NULL;
struct ibv_recv_wr client_recv_wr, *bad_client_recv_wr = NULL;
struct ibv_recv_wr client_recv_comp_wr, *bad_client_recv_comp_wr = NULL;
struct ibv_sge client_recv_sge, server_send_sge;
Arguments args;
void *src = NULL;
char signal = NULL;

int setup_client_resouces()
{
    int ret = -1;
    if (!cm_client_id)
    {
        return -EINVAL;
    }
    pd = ibv_alloc_pd(cm_client_id->verbs);
    if (!pd)
    {
        return -errno;
    }

    io_completion_channel = ibv_create_comp_channel(cm_client_id->verbs);
    if (!io_completion_channel)
    {
        return -errno;
    }

    cq = ibv_create_cq(cm_client_id->verbs,
                       CQ_CAPACITY,
                       NULL,
                       io_completion_channel,
                       0);
    if (!cq)
    {
        return -errno;
    }

    ret = ibv_req_notify_cq(cq, 0);
    if (ret)
    {
        return errno;
    }

    bzero(&qp_init_attr, sizeof(qp_init_attr));
    qp_init_attr.cap.max_recv_sge = MAX_SGE;
    qp_init_attr.cap.max_recv_wr = MAX_WR;
    qp_init_attr.cap.max_send_sge = MAX_SGE;
    qp_init_attr.cap.max_send_wr = MAX_WR;
    qp_init_attr.qp_type = IBV_QPT_RC;
    qp_init_attr.recv_cq = cq;
    qp_init_attr.send_cq = cq;

    ret = rdma_create_qp(cm_client_id, pd, &qp_init_attr);
    if (ret)
    {
        return -errno;
    }
    client_qp = cm_client_id->qp;

    return ret;
}

int start_rdma_server(struct sockaddr_in *server_addr)
{
    struct rdma_cm_event *cm_event = NULL;
    int ret = -1;
    cm_event_channel = rdma_create_event_channel();
    if (!cm_event_channel)
    {
        return -errno;
    }

    ret = rdma_create_id(cm_event_channel, &cm_server_id, NULL, RDMA_PS_TCP);
    if (ret)
    {
        return -errno;
    }

    ret = rdma_bind_addr(cm_server_id, (struct sockaddr *)server_addr);
    if (ret)
    {
        return -errno;
    }

    ret = rdma_listen(cm_server_id, 5);
    if (ret)
    {
        return -errno;
    }
    printf("Server is listening at: %s, port %d\n",
           inet_ntoa(server_addr->sin_addr),
           ntohs(server_addr->sin_port));

    ret = process_rdma_cm_event(cm_event_channel,
                                RDMA_CM_EVENT_CONNECT_REQUEST,
                                &cm_event);
    if (ret)
    {
        return ret;
    }

    cm_client_id = cm_event->id;

    ret = rdma_ack_cm_event(cm_event);
    if (ret)
    {
        return -errno;
    }
    return ret;
}

int accept_client_connection()
{
    struct rdma_conn_param conn_param;
    struct rdma_cm_event *cm_event = NULL;
    struct sockaddr_in remote_sockaddr;
    int ret = -1;

    if (!cm_client_id || !client_qp)
    {
        return -EINVAL;
    }
    client_metadata_mr = rdma_buffer_register(pd,
                                              &client_metadata_attr,
                                              sizeof(client_metadata_attr),
                                              IBV_ACCESS_LOCAL_WRITE);

    if (!client_metadata_mr)
    {
        return -ENOMEM;
    }
    signal_mr = rdma_buffer_register(pd,
                                     &signal, sizeof(char), IBV_ACCESS_LOCAL_WRITE);

    client_recv_sge.addr = (uint64_t)client_metadata_mr->addr;
    client_recv_sge.length = client_metadata_mr->length;
    client_recv_sge.lkey = client_metadata_mr->lkey;
    bzero(&client_recv_wr, sizeof(client_recv_wr));
    client_recv_wr.sg_list = &client_recv_sge;
    client_recv_wr.num_sge = 1;

    ret = ibv_post_recv(client_qp,
                        &client_recv_wr,
                        &bad_client_recv_wr);

    if (ret)
    {
        return ret;
    }

    memset(&conn_param, 0, sizeof(conn_param));
    conn_param.initiator_depth = 3;
    conn_param.responder_resources = 3;

    ret = rdma_accept(cm_client_id, &conn_param);
    if (ret)
    {
        return -errno;
    }

    ret = process_rdma_cm_event(cm_event_channel,
                                RDMA_CM_EVENT_ESTABLISHED,
                                &cm_event);
    if (ret)
    {
        return -errno;
    }
    ret = rdma_ack_cm_event(cm_event);
    if (ret)
    {
        return -errno;
    }

    memcpy(&remote_sockaddr,
           rdma_get_peer_addr(cm_client_id), sizeof(struct sockaddr_in));
    printf("A new connection is accepted from %s\n", inet_ntoa(remote_sockaddr.sin_addr));

    return ret;
}

int server_xchange_metadata_with_client()
{
    struct ibv_wc wc;
    int ret = -1;
    ret = process_work_completion_events(io_completion_channel, &wc, 1);
    if (ret != 1)
    {
        return ret;
    }
    printf("Client side buffer information recieved...\n");
    show_rdma_buffer_attr(&client_metadata_attr);

    server_buffer_mr = rdma_buffer_register(pd,
                                            src,
                                            args.size,
                                            (ibv_access_flags)(IBV_ACCESS_LOCAL_WRITE |
                                                               IBV_ACCESS_REMOTE_WRITE |
                                                               IBV_ACCESS_REMOTE_READ));

    if (!server_buffer_mr)
    {
        return -ENOMEM;
    }

    server_metadata_attr.address = (uint64_t)server_buffer_mr->addr;
    server_metadata_attr.length = (uint32_t)server_buffer_mr->length;
    server_metadata_attr.stag.local_stag = (uint32_t)server_buffer_mr->rkey;

    server_metadata_mr = rdma_buffer_register(pd,
                                              &server_metadata_attr,
                                              sizeof(server_metadata_attr),
                                              IBV_ACCESS_LOCAL_WRITE);
    if (!server_metadata_mr)
    {
        return -ENOMEM;
    }

    server_send_sge.addr = (uint64_t)&server_metadata_attr;
    server_send_sge.length = sizeof(server_metadata_attr);
    server_send_sge.lkey = server_metadata_mr->lkey;
    bzero(&server_send_wr, sizeof(server_send_wr));
    server_send_wr.sg_list = &server_send_sge;
    server_send_wr.num_sge = 1;
    server_send_wr.opcode = IBV_WR_SEND;
    server_send_wr.send_flags = IBV_SEND_SIGNALED;

    ret = ibv_post_send(client_qp, &server_send_wr, &bad_server_send_wr);
    if (ret)
    {
        return -errno;
    }

    ret = process_work_completion_events(io_completion_channel, &wc, 1);
    if (ret != 1)
    {
        return ret;
    }

    struct ibv_qp_attr qp_attr;

    qp_attr.qp_state = IBV_QPS_RTS;
    ibv_modify_qp(client_qp, &qp_attr, IBV_QP_STATE);
    printf("QP: %d\n", client_qp->state);

    return 0;
}

int disconnect_and_cleanup()
{
    struct rdma_cm_event *cm_event = NULL;
    int ret = -1;
    ret = process_rdma_cm_event(cm_event_channel,
                                RDMA_CM_EVENT_DISCONNECTED,
                                &cm_event);
    if (ret)
    {
        return ret;
    }
    ret = rdma_ack_cm_event(cm_event);
    if (ret)
    {
        return -errno;
    }
    rdma_destroy_qp(cm_client_id);
    ret = rdma_destroy_id(cm_client_id);
    ret = ibv_destroy_cq(cq);
    ret = ibv_destroy_comp_channel(io_completion_channel);

    rdma_buffer_deregister(server_buffer_mr);
    rdma_buffer_deregister(server_metadata_mr);
    rdma_buffer_deregister(client_metadata_mr);
    rdma_buffer_deregister(signal_mr);

    ret = ibv_dealloc_pd(pd);
    ret = rdma_destroy_id(cm_server_id);
    free(src);
    rdma_destroy_event_channel(cm_event_channel);
    printf("server closed.\n");
    return 0;
}

int server_remote_memory_ops()
{
    struct ibv_wc wc;
    int ret = -1, i;
    uint32_t len;
    // config rdma write wr
    server_send_sge.addr = (uint64_t)server_buffer_mr->addr;
    server_send_sge.length = (uint32_t)server_buffer_mr->length;
    server_send_sge.lkey = server_buffer_mr->lkey;

    bzero(&server_send_wr, sizeof(server_send_wr));
    server_send_wr.sg_list = &server_send_sge;
    server_send_wr.num_sge = 1;
    server_send_wr.opcode = IBV_WR_RDMA_WRITE;
    // server_send_wr.send_flags = IBV_SEND_SIGNALED;

    server_send_wr.wr.rdma.rkey = client_metadata_attr.stag.remote_stag; // remote key
    server_send_wr.wr.rdma.remote_addr = client_metadata_attr.address;   // remote address

    // config completion wr send to server
    bzero(&server_send_comp_wr, sizeof(server_send_comp_wr));
    server_send_comp_wr.sg_list = NULL;
    server_send_comp_wr.num_sge = 0;
    server_send_comp_wr.opcode = IBV_WR_SEND;
    server_send_comp_wr.send_flags = IBV_SEND_SIGNALED;

    // MUST prepost receive before client send
    // config completion wr recv from server
    client_recv_sge.addr = (uint64_t)signal_mr->addr;
    client_recv_sge.length = (uint32_t)signal_mr->length;
    client_recv_sge.lkey = signal_mr->lkey;
    bzero(&client_recv_comp_wr, sizeof(client_recv_comp_wr));
    client_recv_comp_wr.sg_list = &client_recv_sge;
    client_recv_comp_wr.num_sge = 1;
    ibv_post_recv(client_qp, &client_recv_comp_wr, &bad_client_recv_comp_wr);

    for (i = 0; i < args.count; ++i)
    {
        process_work_completion_events(io_completion_channel, &wc, 1); // wait for receive completion signal from client
        ibv_post_recv(client_qp, &client_recv_comp_wr, &bad_client_recv_comp_wr);

        ibv_post_send(client_qp, &server_send_wr, &bad_server_send_wr);

        ibv_post_send(client_qp, &server_send_comp_wr, &bad_server_send_comp_wr); // send completion signal to client
        process_work_completion_events(io_completion_channel, &wc, 1);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    struct sockaddr_in server_sockaddr;
    int ret;
    bzero(&server_sockaddr, sizeof(server_sockaddr));
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    parseArguments(&args, argc, argv);

    src = malloc(args.size);
    memset(src, 0, args.size);
    if (!src)
    {
        return -ENOMEM;
    }

    ret = get_addr(args.ip, (struct sockaddr *)&server_sockaddr);
    if (ret)
    {
        return ret;
    }

    server_sockaddr.sin_port = htons(args.port);
    ret = start_rdma_server(&server_sockaddr);
    if (ret)
    {
        return ret;
    }
    ret = setup_client_resouces();
    if (ret)
    {
        return ret;
    }
    ret = accept_client_connection();
    if (ret)
    {
        return ret;
    }
    ret = server_xchange_metadata_with_client();
    if (ret)
    {
        return ret;
    }

    ret = server_remote_memory_ops();
    if (ret)
    {
        return ret;
    }

    ret = disconnect_and_cleanup();
    if (ret)
    {
        return ret;
    }

    return 0;
}