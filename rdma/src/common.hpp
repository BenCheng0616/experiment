#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

#define CQ_CAPACITY 16
#define MAX_SGE 2
#define MAX_WR 8

struct __attribute((packed)) rdma_buffer_attr
{
    uint64_t address;
    uint32_t length;
    union stag
    {
        uint32_t local_stag;
        uint32_t remote_stag;
    } stag;
};

void show_rdma_cmid(struct rdma_cm_id *id)
{
    if (!id)
    {
        return;
    }
    printf("RDMA cm id at %p \n", id);
    if (id->verbs && id->verbs->device)
        printf("dev_ctx: %p (device name: %s) \n", id->verbs, id->verbs->device->name);

    if (id->channel)
        printf("cm event channel %p\n", id->channel);

    printf("QP: %p, port_space %x, port_num %u \n", id->qp, id->ps, id->port_num);
}

void show_rdma_buffer_attr(struct rdma_buffer_attr *attr)
{
    if (!attr)
    {
        return;
    }
    printf("-----------------------------------\n");
    printf("buffer attr, addr: %p, len: %u, stag: 0x%x \n",
           (void *)attr->address,
           (unsigned int)attr->length,
           attr->stag.remote_stag);
    printf("-----------------------------------\n");
}

struct ibv_mr *rdma_buffer_register(struct ibv_pd *pd,
                                    void *addr, uint32_t length,
                                    enum ibv_access_flags permission)
{
    struct ibv_mr *mr = NULL;
    if (!pd)
    {
        return NULL;
    }
    mr = ibv_reg_mr(pd, addr, length, permission);
    if (!mr)
    {
        return NULL;
    }
    return mr;
}

struct ibv_mr *rdma_buffer_alloc(struct ibv_pd *pd, uint32_t size, enum ibv_access_flags permission)
{
    struct ibv_mr *mr = NULL;
    if (!pd)
    {
        return NULL;
    }
    void *buf = calloc(1, size);
    if (!buf)
    {
        return NULL;
    }
    // debug("Buffer allocated: %p, len: %u \n", buf, size);
    mr = rdma_buffer_register(pd, buf, size, permission);
    if (!mr)
    {
        free(buf);
    }
    return mr;
}

void rdma_buffer_deregister(struct ibv_mr *mr)
{
    if (!mr)
    {
        return;
    }
    ibv_dereg_mr(mr);
}

void rdma_buffer_free(struct ibv_mr *mr)
{
    if (!mr)
    {
        return;
    }
    void *to_free = mr->addr;
    rdma_buffer_deregister(mr);
    free(to_free);
}

int process_rdma_cm_event(struct rdma_event_channel *echannel,
                          enum rdma_cm_event_type expected_event,
                          struct rdma_cm_event **cm_event)
{
    int ret = 1;
    ret = rdma_get_cm_event(echannel, cm_event);
    if (ret)
    {
        return -errno;
    }
    if (0 != (*cm_event)->status)
    {
        ret = -((*cm_event)->status);
        rdma_ack_cm_event(*cm_event);
        return ret;
    }

    if ((*cm_event)->event != expected_event)
    {
        rdma_ack_cm_event(*cm_event);
        return -1;
    }

    return ret;
}

int process_work_completion_events(struct ibv_comp_channel *comp_channel,
                                   struct ibv_wc *wc, int max_wc)
{
    struct ibv_cq *cq_ptr = NULL;
    void *context = NULL;
    int ret = -1, i, total_wc = 0;

    ret = ibv_get_cq_event(comp_channel, &cq_ptr, &context);
    if (ret)
    {
        return -errno;
    }

    ret = ibv_req_notify_cq(cq_ptr, 0);
    if (ret)
    {
        return -errno;
    }

    total_wc = 0;
    do
    {
        ret = ibv_poll_cq(cq_ptr,
                          max_wc - total_wc,
                          wc + total_wc);

        if (ret < 0)
        {
            return ret;
        }
        total_wc += ret;
    } while (total_wc < max_wc);

    for (i = 0; i < total_wc; i++)
    {
        if (wc[i].status != IBV_WC_SUCCESS)
        {

            return -(wc[i].status);
        }
    }

    ibv_ack_cq_events(cq_ptr, 1);

    return total_wc;
}

int get_addr(char *dst, struct sockaddr *addr)
{
    struct addrinfo *res;
    int ret = -1;
    ret = getaddrinfo(dst, NULL, NULL, &res);
    if (ret)
    {
        return ret;
    }
    memcpy(addr, res->ai_addr, sizeof(struct sockaddr_in));
    freeaddrinfo(res);

    return ret;
};

#endif