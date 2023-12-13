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
        struct sockaddr_in serverInfo;
        bzero(&serverInfo, sizeof(serverInfo));
        serverInfo.sin_family = PF_INET;
        serverInfo.sin_addr.s_addr = inet_addr(_args->ip);
        serverInfo.sin_port = htons(_args->port);
        if ((_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        {
            exit(EXIT_FAILURE);
        }
        if ((res = connect(_sockfd, (struct sockaddr *)&serverInfo, sizeof(serverInfo))) == -1)
        {
            exit(EXIT_FAILURE);
        }
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

            recv(_sockfd, buffer, _args->size, MSG_WAITALL);

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
