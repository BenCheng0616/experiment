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
        _args = args;
        _serverID = NULL;
        _clientID = NULL;
        _buffer = malloc(_args->size);
        memset(_buffer, 0, _args->size);
    }

    ~Server()
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
        hints.ai_flags = RAI_PASSIVE;
        res = rdma_getaddrinfo(NULL, std::to_string(_args->port).c_str(), &hints, &serverInfo);
        struct ibv_qp_init_attr attr;
        memset(&attr, 0, sizeof(attr));
        attr.cap.max_send_wr = 4;
        attr.cap.max_recv_wr = 1;
        attr.cap.max_send_sge = 1;
        attr.cap.max_recv_sge = 1;
        attr.cap.max_inline_data = 0;
        attr.sq_sig_all = 1;

        if ((res = rdma_create_ep(&_serverID, serverInfo, NULL, &attr)) == -1)
        {
            exit(EXIT_FAILURE);
        }
        rdma_listen(_serverID, 5);
        rdma_freeaddrinfo(serverInfo);
    }

    void waitforClient()
    {
        int err;
        struct rdma_cm_event *event;
        struct rdma_conn_param conn = {};
        pdata rep_data;
        std::cout << "wait for client connect\n";

        // establish connection and exchange rkey & mem addr with client
        err = rdma_get_cm_event(_serverID->channel, &event);
        if (err)
        {
            exit(EXIT_FAILURE);
        }
        if (event->event != RDMA_CM_EVENT_CONNECT_REQUEST)
        {
            exit(0);
        }
        rdma_ack_cm_event(event);
        _clientID = event->id;
        _client_mr = rdma_reg_msgs(_clientID, &_buffer, _args->size);
        rep_data.buf_va = bswap_64((uintptr_t)_buffer);
        rep_data.buf_rkey = htonl(_client_mr->rkey);
        conn.private_data = &rep_data;
        conn.private_data_len = sizeof(rep_data);
        memcpy(&_client_pdata, event->param.conn.private_data, sizeof(_client_pdata));
        rdma_accept(_clientID, &conn);
        err = rdma_get_cm_event(_serverID->channel, &event);
        if (err)
        {
            exit(EXIT_FAILURE);
        }
        if (event->event != RDMA_CM_EVENT_ESTABLISHED)
        {
            exit(0);
        }
        rdma_ack_cm_event(event);
    }

    void communicate()
    {
        int rv;
        struct ibv_wc wc;
        uint8_t *buf = (uint8_t *)calloc(1, sizeof(uint8_t));
        struct ibv_mr *doorbell = rdma_reg_msgs(_clientID, &buf, sizeof(uint8_t));
        for (int count = 0; count < _args->count; ++count)
        {
            // wait for remote data write into memory.
            rdma_get_recv_comp(_clientID, &wc);
            // write data from local memory to remote memory.
            rdma_post_write(_clientID, NULL, _buffer, _args->size, _client_mr, 0, bswap_64(_client_pdata.buf_va), ntohl(_client_pdata.buf_rkey));
            rdma_get_send_comp(_clientID, &wc);
            // notify remote host WRITE operation complete.
            rdma_post_send(_clientID, NULL, &buf, sizeof(uint8_t), doorbell, 0);
        }
    }

private:
    void *_buffer;
    Arguments *_args;
    struct rdma_cm_id *_serverID;
    struct rdma_cm_id *_clientID;
    struct ibv_mr *_client_mr;
    pdata _client_pdata;
};

int main(int argc, char *argv[])
{
    Arguments args;
    parseArguments(&args, argc, argv);

    Server server(&args);
    server.init();
    server.waitforClient();
    server.communicate();

    return 0;
}