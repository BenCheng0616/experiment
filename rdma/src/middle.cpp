#include <iostream>
#include <string.h>
#include <unistd.h>
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

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
    }

    void waitforClient()
    {
    }

    void communicate()
    {
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