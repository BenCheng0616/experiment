#include <iostream>
#include <string.h>
#include <unistd.h>
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
        _args = args;
        _serverID = NULL;
        _clientID = NULL;
        _buffer = malloc(_args->size);
        memset(_buffer, 0, _args->size);
    }
    ~Client()
    {
    }

    void init()
    {
        int res = 0;
        struct rdma_addrinfo *serverInfo;
        struct rdma_addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_port_space = RDMA_PS_TCP;
        res = rdma_getaddrinfo(_args->ip, std::to_string(_args->port).c_str(), &hints, &serverInfo);

        struct ibv_qp_init_attr attr;
        memset(&attr, 0, sizeof(attr));
        attr.cap.max_send_wr = 4;
        attr.cap.max_recv_wr = 1;
        attr.cap.max_send_sge = 1;
        attr.cap.max_recv_sge = 1;
        attr.cap.max_inline_data = 0;
        attr.sq_sig_all = 1;
        std::cout << "server found.\n";
        if ((res = rdma_create_ep(&_serverID, serverInfo, NULL, &attr)) == -1)
        {
            std::cout << "error\n";
            exit(EXIT_FAILURE);
        }

        rdma_freeaddrinfo(serverInfo);
        int err;
        struct rdma_cm_event *event;
        struct rdma_conn_param conn = {};
        pdata rep_data;

        _server_mr = rdma_reg_msgs(_serverID, &_buffer, _args->size);
        rep_data.buf_va = bswap_64((uintptr_t)_buffer);
        rep_data.buf_rkey = htonl(_server_mr->rkey);
        conn.responder_resources = 1;
        conn.private_data = &rep_data;
        conn.private_data_len = sizeof(rep_data);
        rdma_connect(_serverID, &conn);
        std::cout << "server connected.\n";
        // establish connection and exchange rkey & mem addr with client
        err = rdma_get_cm_event(_serverID->channel, &event);
        if (err)
        {
            exit(EXIT_FAILURE);
        }
        if (event->event != RDMA_CM_EVENT_ESTABLISHED)
        {
            exit(0);
        }
        memcpy(&_server_pdata, event->param.conn.private_data, sizeof(_server_pdata));
        rdma_ack_cm_event(event);
    }

    void communicate()
    {
        int rv;
        struct ibv_wc wc;
        uint8_t *buf = (uint8_t *)calloc(1, sizeof(uint8_t));
        struct ibv_mr *doorbell = rdma_reg_msgs(_clientID, &buf, sizeof(uint8_t));
        Benchmark bench(_args);
        for (int count = 0; count < _args->count; ++count)
        {
            bench.singleStart();
            // write data from local memory to remote memory.
            rdma_post_write(_serverID, NULL, _buffer, _args->size, _server_mr, 0, bswap_64(_server_pdata.buf_va), ntohl(_server_pdata.buf_rkey));
            rdma_get_send_comp(_serverID, &wc);
            // notify remote host WRITE operation complete.
            rdma_post_send(_serverID, NULL, &buf, sizeof(uint8_t), doorbell, 0);
            // wait for remote data write into memory.
            rdma_get_recv_comp(_serverID, &wc);
            bench.benchmark();
        }
        bench.evaluate(_args);
    }

    void stop()
    {
        rdma_disconnect(_serverID);
        rdma_dereg_mr(_server_mr);
        free(_buffer);
        rdma_destroy_ep(_serverID);
    }

private:
    void *_buffer;
    Arguments *_args;
    struct rdma_cm_id *_serverID;
    struct rdma_cm_id *_clientID;
    struct ibv_mr *_server_mr;
    pdata _server_pdata;
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