#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>

#include "benchmarks.hpp"
#include "parseargs.hpp"
class Client
{
public:
    Client(Arguments *args)
    {
        _args = args;
        _sockfd = 0;
    }

    ~Client()
    {
        stop();
    }

    void init()
    {
        // connect to server.
        int res = 0;
        struct addrinfo *serverInfo = NULL;
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        res = getaddrinfo(_args->ip, std::to_string(_args->port).c_str(), &hints, &serverInfo);
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
        freeaddrinfo(serverInfo);
    }

    void communicate()
    {
        void *buffer;
        int ret;
        buffer = malloc(_args->size);
        memset(buffer, '0', _args->size);
        Benchmark bench(_args);
        for (int count = 0; count < _args->count; ++count)
        {
            bench.singleStart();
            send(_sockfd, buffer, _args->size, 0);

            ret = recv(_sockfd, buffer, _args->size, MSG_WAITALL);

            bench.benchmark();
        }
        bench.evaluate(_args);
        std::cout << "done.\n";
        free(buffer);
    }

    void stop()
    {
        close(_sockfd);
    }

private:
    Arguments *_args;
    int _sockfd;
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
