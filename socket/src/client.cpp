#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>

#include "parseargs.hpp"

class Client
{
public:
    Client(Arguments args)
    {
        _args = args;
        _sockfd = 0;
        std::cout << _args.count << "\n";
    }

    ~Client()
    {
        close(_sockfd);
    }

    void init()
    {
        int res = 0;
        struct addrinfo *serverInfo;
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        res = getaddrinfo(_args.ip, _args.port, &hints, &serverInfo);
        if ((_sockfd = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol)) == -1)
        {
            exit(EXIT_FAILURE);
        }
        int opt = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
        if ((res = connect(_sockfd, serverInfo->ai_addr, serverInfo->ai_addrlen)) == -1)
        {
            exit(EXIT_FAILURE);
        }
    }

    void communicate()
    {
        void *buffer;
        int rv;
        buffer = malloc(_args.size);
        memset(buffer, '0', _args.size);
        for (int i = 0; i < _args.count; ++i)
        {
            send(_sockfd, buffer, _args.size, 0);
            recv(_sockfd, buffer, _args.size, MSG_WAITALL);
            // sleep(1);
        }
        std::cout << "done.\n";
        free(buffer);
    }

private:
    Arguments _args;
    int _sockfd;
};

int main(int argc, char *argv[])
{
    Arguments args;

    parseArguments(&args, argc, argv);
    Client client(args);
    client.init();
    client.communicate();
    return 0;
}
