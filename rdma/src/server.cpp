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
    hints.ai_flags = RAI_PASSIVE;
    hints.ai_port_space = RDMA_PS_TCP;
    static const char *ip = "0.0.0.0";
    static const char *port = "5679";

    rdma_getaddrinfo(ip, port, &hints, &rai);
    sockfd = rsocket(rai->ai_family, SOCK_STREAM, 0);
    int val = 1;
    //int bufsize = 65536;
    //socklen_t buflen = sizeof(bufsize);
    rsetsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    //rsetsockopt(sockfd, SOL_RDMA, RDMA_SQSIZE, &bufsize, buflen);
    //rsetsockopt(sockfd, SOL_RDMA, RDMA_RQSIZE, &bufsize, buflen);
    rbind(sockfd, rai->ai_src_addr, rai->ai_src_len);

    rlisten(sockfd, 3);
    std::cout << "RDMA waiting for client.\n";
    rdma_freeaddrinfo(rai);
    struct sockaddr_in client;
    int len = sizeof(client);
    int clientSockfd = raccept(sockfd, (struct sockaddr *)&client, (socklen_t *)&len);
    //rsetsockopt(clientSockfd, SOL_RDMA, RDMA_SQSIZE, &bufsize, buflen);
    //rsetsockopt(clientSockfd, SOL_RDMA, RDMA_RQSIZE, &bufsize, buflen);
    std::cout << "Client connected.\tip: " << inet_ntoa(client.sin_addr) << "\n";

    void *buf;
    buf = malloc(pktSize);
    struct timespec start, end;
    int offset;
    int sum = 0;

    for (int i = 0; i < count; i++)
    {
        clock_gettime(CLOCK_REALTIME, &start);
        for (offset = 0; offset < pktSize;)
        {
            ret = rrecv(clientSockfd, buf + offset, pktSize - offset, 0);
            // std::cout << "Recv Size: " << ret << "\n";
            if (ret > 0)
            {
                offset += ret;
            }
        }

        ret = rsend(clientSockfd, buf, pktSize, 0);
        // usleep(10);
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
    hints.ai_flags = AI_PASSIVE;
    static const char *ip = "0.0.0.0";
    static const char *port = "5678";
    getaddrinfo(ip, port, &hints, &ai);
    sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    bind(sockfd, ai->ai_addr, ai->ai_addrlen);
    listen(sockfd, 3);
    std::cout << "SOCKET waiting for client.\n";
    struct sockaddr_in client;
    int len = sizeof(client);
    int clientSockfd = accept(sockfd, (struct sockaddr *)&client, (socklen_t *)&len);
    std::cout << "Client connected.\tip: " << inet_ntoa(client.sin_addr) << "\n";

    void *buf;
    buf = malloc(pktSize);
    struct timespec start, end;
    int offset;
    int sum = 0;
    for (int i = 0; i < count; i++)
    {
        clock_gettime(CLOCK_REALTIME, &start);
        for (offset = 0; offset < pktSize;)
        {
            ret = recv(clientSockfd, buf + offset, pktSize - offset, 0);
            if (ret > 0)
            {
                offset += ret;
            }
        }
        send(clientSockfd, buf, pktSize, 0);
        // std::cout << ret << "\n";
        clock_gettime(CLOCK_REALTIME, &end);
        sum += caltime(start, end);
    }
    std::cout << strlen((char *)buf) << "\n";
    std::cout << "SOCKET Avg Time: " << sum / count << "\tus\n";
    // close(sockfd);
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
            pktSize = atoi(optarg);
            break;
        case 'c':
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
