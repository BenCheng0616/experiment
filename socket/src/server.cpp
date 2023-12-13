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
        struct sockaddr_in serverInfo;
        bzero(&serverInfo, sizeof(serverInfo));

        serverInfo.sin_family = PF_INET;
        serverInfo.sin_addr.s_addr = INADDR_ANY;
        serverInfo.sin_port = htons(_args->port);
        _sockfd = socket(AF_INET, SOCK_STREAM, 0);
        bind(_sockfd, (struct sockaddr *)&serverInfo, sizeof(serverInfo));
        listen(_sockfd, 5);
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
            recv(_clientSockfd, buffer, _args->size, MSG_WAITALL);
            send(_clientSockfd, buffer, _args->size, 0);
        }
        free(buffer);
        printf("%d\n", ret);
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
