#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>

#include "parseargs.hpp"

class Server
{
public:
    Server(Arguments *args)
    {
        _args = args;
        _sockfd = 0;
        _clientSockfd = 0;
    }

    ~Server()
    {
    }

    void init()
    {
        int res = 0;
        struct addrinfo *serverInfo = NULL;
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE;
        res = getaddrinfo(NULL, std::to_string(_args->port).c_str(), &hints, &serverInfo);
        if ((_sockfd = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol)) == -1)
        {
            exit(EXIT_FAILURE);
        }
        int opt = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
        bind(_sockfd, serverInfo->ai_addr, serverInfo->ai_addrlen);
        listen(_sockfd, 5);

        freeaddrinfo(serverInfo);
    }

    void waitforClient()
    {
        struct sockaddr_in clientInfo;
        int addrlen = sizeof(clientInfo);
        if ((_clientSockfd = accept(_sockfd, (struct sockaddr *)&clientInfo, (socklen_t *)&addrlen)) == -1)
        {
            exit(EXIT_FAILURE);
        }
    }

    void communicate()
    {
        void *buffer;
        int ret;
        buffer = malloc(_args->size);
        for (int count = 0; count < _args->count; ++count)
        {
            ret = recv(_clientSockfd, buffer, _args->size, MSG_WAITALL);
            send(_clientSockfd, buffer, _args->size, 0);
        }
        free(buffer);
    }

private:
    Arguments *_args;
    int _sockfd, _clientSockfd;
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
