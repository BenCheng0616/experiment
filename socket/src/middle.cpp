#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>

#include "parseargs.hpp"

class Middle
{
public:
    Middle(Arguments *args)
    {
        _args = args;
        _sockfd = 0;
        _serverSockfd = 0;
        _clientSockfd = 0;
    }
    ~Middle()
    {
    }

    void init()
    {
        // connect to server
        int res = 0;
        struct addrinfo *serverInfo = NULL;
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        res = getaddrinfo(_args->ip, std::to_string(_args->port).c_str(), &hints, &serverInfo);
        if ((_serverSockfd = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol)) == -1)
        {
            exit(EXIT_FAILURE);
        }
        int opt = 1;
        setsockopt(_serverSockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
        if ((res = connect(_serverSockfd, serverInfo->ai_addr, serverInfo->ai_addrlen)) == -1)
        {
            exit(EXIT_FAILURE);
        }
        freeaddrinfo(serverInfo);

        // build socket for client
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE;
        res = getaddrinfo(NULL, std::to_string(_args->port + 1).c_str(), &hints, &serverInfo);
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
        buffer = malloc(_args->size);
        for (int count = 0; count < _args->count; count++)
        {

            // client -- middle -- server
            recv(_clientSockfd, buffer, _args->size, MSG_WAITALL);
            send(_serverSockfd, buffer, _args->size, 0);
            recv(_serverSockfd, buffer, _args->size, MSG_WAITALL);
            send(_clientSockfd, buffer, _args->size, 0);
        }
    }

private:
    Arguments *_args;
    int _sockfd, _serverSockfd, _clientSockfd;
};

int main(int argc, char *argv[])
{
    Arguments args;

    parseArguments(&args, argc, argv);

    Middle middle(&args);
    middle.init();
    middle.waitforClient();
    middle.communicate();
    return 0;
}