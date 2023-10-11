#include <iostream>
#include <rdma/rsocket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int pktSize = 1000;
int count = 100;

int caltime(struct timespec start, struct timespec end)
{
    return (end.tv_sec * 1e6 + end.tv_nsec / 1000) - (start.tv_sec * 1e6 + start.tv_nsec / 1000);
}
void rsocket_test()
{
    int ret, sockfd = 0;
    static int flags = 0;
    struct rdma_addrinfo *rai = NULL;
    struct rdma_addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_port_space = RDMA_PS_TCP;
    static const char *ip = "192.168.30.130";
    static const char *port = "5679";

    rdma_getaddrinfo(ip, port, &hints, &rai);
    sockfd = rsocket(rai->ai_family, SOCK_STREAM, 0);

    //uint32_t bufsize = 65536;
    //socklen_t buflen = sizeof(bufsize);
    //std::cout << rsetsockopt(sockfd, SOL_RDMA, RDMA_SQSIZE, &bufsize, buflen) << "\n";
    //std::cout << rsetsockopt(sockfd, SOL_RDMA, RDMA_RQSIZE, &bufsize, buflen) << "\n";
    //   std::cout << bufsize << "\n";
    //   std::cout << bufsize << "\n";
    //    rsetsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &bufsize, buflen);
    //    rsetsoc kopt(sockfd, SOL_SOCKET, SO_RCVBUF, &bufsize, buflen);

    //rgetsockopt(sockfd, SOL_RDMA, RDMA_RQSIZE, &bufsize, &buflen);
    //std::cout << bufsize << "\n";
    ret = rconnect(sockfd, rai->ai_dst_addr, rai->ai_dst_len);
    // std::cout << ret;

    rdma_freeaddrinfo(rai);
    void *buf;
    buf = malloc(pktSize);
    memset(buf, '*', pktSize);

    struct timespec start, end;
    int offset;
    int sum = 0;
    for (int i = 0; i < count; i++)
    {
        clock_gettime(CLOCK_REALTIME, &start);
        ret = rsend(sockfd, buf, pktSize, 0);
        // usleep(10);
        for (offset = 0; offset < pktSize;)
        {
            ret = rrecv(sockfd, buf + offset, pktSize - offset, 0);
            if (ret > 0)
            {
                offset += ret;
            }
        }
        clock_gettime(CLOCK_REALTIME, &end);
        sum += caltime(start, end);
    }
    std::cout << strlen((char *)buf) << "\n";
    std::cout << "RDMA Avg Time: " << sum / count << "\tus\n";
    rclose(sockfd);
}

void socket_test()
{
    int ret, sockfd = 0;
    static int flags = 0;

    struct addrinfo *ai = NULL;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    static const char *ip = "192.168.30.130";
    static const char *port = "5678";
    getaddrinfo(ip, port, &hints, &ai);
    sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    int bufsize = 262144;
    socklen_t buflen = sizeof(bufsize);
    setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &bufsize, buflen);
    getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &bufsize, &buflen);
    std::cout << bufsize << "\n";
    ret = connect(sockfd, ai->ai_addr, ai->ai_addrlen);
    freeaddrinfo(ai);

    void *buf;
    buf = malloc(pktSize);
    memset(buf, '*', pktSize);

    // getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &bufsize, &buflen);
    // std::cout << bufsize << "\n";

    struct timespec start, end;
    int offset;
    int sum = 0;
    for (int i = 0; i < count; i++)
    {
        clock_gettime(CLOCK_REALTIME, &start);
        send(sockfd, buf, pktSize, 0);
        for (offset = 0; offset < pktSize;)
        {
            ret = recv(sockfd, buf + offset, pktSize - offset, 0);
            if (ret > 0)
            {
                offset += ret;
            }
        }
        clock_gettime(CLOCK_REALTIME, &end);
        sum += caltime(start, end);
    }
    std::cout << strlen((char *)buf) << "\n";
    std::cout << "SOCKET Avg Time: " << sum / count << "\tus\n";
    close(sockfd);
}

int main(int argc, char *argv[])
{
    int option;
    int mode = 0;
    while ((option = getopt(argc, argv, "s:c:m:")) != -1)
    {
        switch (option)
        {
        case 's':
            std::cout << "Packet Size: " << atoi(optarg) << "\n";
            pktSize = atoi(optarg);
            break;
        case 'c':
            std::cout << "iter times: " << atoi(optarg) << "\n";
            count = atoi(optarg);
            break;
        case 'm':
            if (!strcmp(optarg, "R"))
            {
                std::cout << "Set Mode: RDMA.\n";
                mode = 0;
            }
            else if (!strcmp(optarg, "S"))
            {
                std::cout << "Set Mode: Socket.\n";
                mode = 1;
            }
            break;
        }
    }
    if (!mode)
        rsocket_test();
    else
        socket_test();
    return 0;
}
