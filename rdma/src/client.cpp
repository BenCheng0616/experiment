
#include "benchmarks.hpp"
#include "parseargs.hpp"
#include "common.hpp"

struct rdma_event_channel *cm_event_channel = NULL;
struct rdma_cm_id *cm_client_id = NULL;
struct ibv_pd *pd = NULL;
struct ibv_comp_channel *io_completion_channel = NULL;
struct ibv_cq *client_cq = NULL;
struct ibv_qp_init_attr qp_init_attr;
struct ibv_qp *client_qp;
struct ibv_mr *client_metadata_mr = NULL,
              *client_src_mr = NULL,
              *client_dst_mr = NULL,
              *server_metadata_mr = NULL;
struct rdma_buffer_attr client_metadata_attr, server_metadata_attr;
struct ibv_send_wr client_send_wr, *bad_client_send_wr = NULL;
struct ibv_send_wr client_send_comp_wr, *bad_client_send_comp_wr = NULL;
struct ibv_recv_wr server_recv_wr, *bad_server_recv_wr = NULL;
struct ibv_recv_wr server_recv_comp_wr, *bad_server_recv_comp_wr = NULL;
struct ibv_sge client_send_sge, server_recv_sge;
Arguments args;
void *src = NULL;

int client_prepare_connection(struct sockaddr_in *s_addr)
{
    struct rdma_cm_event *cm_event = NULL;
    int ret = -1;

    cm_event_channel = rdma_create_event_channel();
    if (!cm_event_channel)
    {
        return -errno;
    }

    ret = rdma_create_id(cm_event_channel, &cm_client_id, NULL, RDMA_PS_TCP);
    if (ret)
    {
        return -errno;
    }

    ret = rdma_resolve_addr(cm_client_id, NULL, (struct sockaddr *)s_addr, 2000);
    if (ret)
    {
        return -errno;
    }

    ret = process_rdma_cm_event(cm_event_channel,
                                RDMA_CM_EVENT_ADDR_RESOLVED,
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

    ret = rdma_resolve_route(cm_client_id, 2000);
    if (ret)
    {
        return -errno;
    }

    ret = process_rdma_cm_event(cm_event_channel,
                                RDMA_CM_EVENT_ROUTE_RESOLVED,
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
    printf("Trying to connect to server at: %s, port %u\n",
           inet_ntoa(s_addr->sin_addr),
           ntohs(s_addr->sin_port));

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

    client_cq = ibv_create_cq(cm_client_id->verbs,
                              CQ_CAPACITY, NULL, io_completion_channel, 0);
    if (!client_cq)
    {
        return -errno;
    }

    ret = ibv_req_notify_cq(client_cq, 0);
    if (ret)
    {
        return -errno;
    }

    bzero(&qp_init_attr, sizeof(qp_init_attr));
    qp_init_attr.cap.max_recv_sge = MAX_SGE;
    qp_init_attr.cap.max_recv_wr = MAX_WR;
    qp_init_attr.cap.max_send_sge = MAX_SGE;
    qp_init_attr.cap.max_send_wr = MAX_WR;
    qp_init_attr.qp_type = IBV_QPT_RC;

    qp_init_attr.recv_cq = client_cq;
    qp_init_attr.send_cq = client_cq;

    ret = rdma_create_qp(cm_client_id, pd, &qp_init_attr);
    if (ret)
    {
        return -errno;
    }
    client_qp = cm_client_id->qp;
    return 0;
}

int client_pre_post_recv_buffer()
{
    int ret = -1;
    server_metadata_mr = rdma_buffer_register(pd,
                                              &server_metadata_attr,
                                              sizeof(server_metadata_attr),
                                              (IBV_ACCESS_LOCAL_WRITE));
    if (!server_metadata_mr)
    {
        return -ENOMEM;
    }

    server_recv_sge.addr = (uint64_t)server_metadata_mr->addr;
    server_recv_sge.length = (uint32_t)server_metadata_mr->length;
    server_recv_sge.lkey = (uint32_t)server_metadata_mr->lkey;

    bzero(&server_recv_wr, sizeof(server_recv_wr));
    server_recv_wr.sg_list = &server_recv_sge;
    server_recv_wr.num_sge = 1;
    ret = ibv_post_recv(client_qp, &server_recv_wr, &bad_server_recv_wr);
    return 0;
}

int client_connect_to_server()
{
    struct rdma_conn_param conn_param;
    struct rdma_cm_event *cm_event = NULL;
    int ret = -1;

    bzero(&server_recv_comp_wr, sizeof(server_recv_comp_wr));
    server_recv_comp_wr.sg_list = NULL;
    server_recv_comp_wr.num_sge = 0;

    ret = ibv_post_recv(client_qp, &server_recv_comp_wr, &bad_server_recv_comp_wr);

    bzero(&conn_param, sizeof(conn_param));
    conn_param.initiator_depth = 3;
    conn_param.responder_resources = 3;
    conn_param.retry_count = 3;

    ret = rdma_connect(cm_client_id, &conn_param);
    if (ret)
    {
        return -errno;
    }
    printf("check1\n");
    ret = process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_ESTABLISHED, &cm_event);
    if (ret)
    {
        return ret;
    }

    ret = rdma_ack_cm_event(cm_event);
    if (ret)
    {
        return -errno;
    }
    printf("The client is connected.\n");
    return 0;
}

int client_xchange_metadata_with_server()
{
    struct ibv_wc wc[2];
    int ret = -1;
    client_src_mr = rdma_buffer_register(pd,
                                         src,
                                         args.size,
                                         (enum ibv_access_flags)(IBV_ACCESS_LOCAL_WRITE |
                                                                 IBV_ACCESS_REMOTE_READ |
                                                                 IBV_ACCESS_REMOTE_WRITE));
    if (!client_src_mr)
    {
        return ret;
    }

    client_metadata_attr.address = (uint64_t)client_src_mr->addr;
    client_metadata_attr.length = (uint32_t)client_src_mr->length;
    client_metadata_attr.stag.local_stag = client_src_mr->rkey;

    client_metadata_mr = rdma_buffer_register(pd,
                                              &client_metadata_attr,
                                              sizeof(client_metadata_attr), IBV_ACCESS_LOCAL_WRITE);
    if (!client_metadata_mr)
    {
        return ret;
    }

    client_send_sge.addr = (uint64_t)client_metadata_mr->addr;
    client_send_sge.length = (uint32_t)client_metadata_mr->length;
    client_send_sge.lkey = client_metadata_mr->lkey;

    bzero(&client_send_wr, sizeof(client_send_wr));
    client_send_wr.sg_list = &client_send_sge;
    client_send_wr.num_sge = 1;
    client_send_wr.opcode = IBV_WR_SEND;
    client_send_wr.send_flags = IBV_SEND_SIGNALED;

    ret = ibv_post_send(client_qp,
                        &client_send_wr,
                        &bad_client_send_wr);
    if (ret)
    {
        return -errno;
    }
    ret = process_work_completion_events(io_completion_channel,
                                         wc, 2);
    if (ret != 2)
    {
        return ret;
    }
    show_rdma_buffer_attr(&server_metadata_attr);
    return 0;
}

int client_remote_memory_ops()
{
    struct ibv_wc wc[2];
    int ret = -1, i;

    client_send_sge.addr = (uint64_t)client_src_mr->addr;
    client_send_sge.length = (uint32_t)client_src_mr->length;
    client_send_sge.lkey = client_src_mr->lkey;

    client_send_wr.sg_list = &client_send_sge;
    client_send_wr.num_sge = 1;
    client_send_wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
    client_send_wr.imm_data = args.size;
    client_send_wr.send_flags = IBV_SEND_SIGNALED;

    client_send_wr.wr.rdma.rkey = server_metadata_attr.stag.remote_stag;
    client_send_wr.wr.rdma.remote_addr = server_metadata_attr.address;

    bzero(&client_send_comp_wr, sizeof(client_send_comp_wr));
    client_send_comp_wr.sg_list = NULL;
    client_send_comp_wr.num_sge = 0;
    client_send_comp_wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
    client_send_comp_wr.imm_data = args.size;
    client_send_comp_wr.send_flags = IBV_SEND_SIGNALED;

    Benchmark bench(&args);
    for (i = 0; i < args.count; i++)
    {
        bench.singleStart();

        ret = ibv_post_send(client_qp,
                            &client_send_wr,
                            &bad_client_send_wr);

        // rdma write complete
        ret = process_work_completion_events(io_completion_channel, wc, 2);
        /*
        ret = ibv_post_send(client_qp,
                            &client_send_comp_wr,
                            &bad_client_send_comp_wr);

        ret = process_work_completion_events(io_completion_channel, wc, 2);
        */
        ret = ibv_post_recv(client_qp,
                            &server_recv_comp_wr,
                            &bad_server_recv_comp_wr);

        bench.benchmark();
    }
    bench.evaluate(&args);
    return 0;
}

int client_disconnect_and_clean()
{
    struct rdma_cm_event *cm_event = NULL;
    int ret = -1;
    ret = rdma_disconnect(cm_client_id);

    ret = process_rdma_cm_event(cm_event_channel,
                                RDMA_CM_EVENT_DISCONNECTED,
                                &cm_event);

    ret = rdma_ack_cm_event(cm_event);

    rdma_destroy_qp(cm_client_id);
    ret = rdma_destroy_id(cm_client_id);

    ret = ibv_destroy_cq(client_cq);

    ret = ibv_destroy_comp_channel(io_completion_channel);

    rdma_buffer_deregister(server_metadata_mr);
    rdma_buffer_deregister(client_metadata_mr);
    rdma_buffer_deregister(client_src_mr);
    rdma_buffer_deregister(client_dst_mr);

    free(src);

    ret = ibv_dealloc_pd(pd);
    rdma_destroy_event_channel(cm_event_channel);
    return 0;
}

int main(int argc, char *argv[])
{
    struct sockaddr_in server_sockaddr;
    int ret;
    bzero(&server_sockaddr, sizeof(server_sockaddr));
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    parseArguments(&args, argc, argv);

    src = NULL;
    src = malloc(args.size);
    memset(src, '1', args.size);
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

    ret = client_prepare_connection(&server_sockaddr);
    if (ret)
    {
        return ret;
    }

    ret = client_pre_post_recv_buffer();
    if (ret)
    {
        return ret;
    }

    ret = client_connect_to_server();
    if (ret)
    {
        return ret;
    }

    ret = client_xchange_metadata_with_server();
    if (ret)
    {
        return ret;
    }

    ret = client_remote_memory_ops();
    if (ret)
    {
        return ret;
    }

    printf("SUCCESS\n");
    ret = client_disconnect_and_clean();

    return ret;
}