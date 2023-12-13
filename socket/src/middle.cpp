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
        struct sockaddr_in serverInfo, middleInfo;
        bzero(&serverInfo, sizeof(serverInfo));
        if ((_serverSockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        {
            exit(EXIT_FAILURE);
        }

        serverInfo.sin_family = PF_INET;
        serverInfo.sin_addr.s_addr = inet_addr(_args->ip);
        serverInfo.sin_port = htons(_args->port);

        if ((res = connect(_serverSockfd, (struct sockaddr *)&serverInfo, sizeof(serverInfo))) == -1)
        {
            exit(EXIT_FAILURE);
        }

        bzero(&middleInfo, sizeof(middleInfo));
        middleInfo.sin_family = PF_INET;
        middleInfo.sin_addr.s_addr = INADDR_ANY;
        middleInfo.sin_port = htons(_args->port + 1);
        if ((_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        {
            exit(EXIT_FAILURE);
        }
        bind(_sockfd, (struct sockaddr *)&middleInfo, sizeof(middleInfo));
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
    void stop()
    {
        close(_serverSockfd);
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
    middle.stop();
    return 0;
}