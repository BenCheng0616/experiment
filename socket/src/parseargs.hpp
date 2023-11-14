#ifndef PARSEARGS_H
#define PARSEATGS_H
#include <stdlib.h>
#include <getopt.h>
#include <arpa/inet.h>

typedef struct Arguments
{
    int size;
    char *port;
    int count;
    char *ip;
} Arguments;

void parseArguments(Arguments *arg, int argc, char *argv[])
{
    int opt = 0;
    arg->size = 4096;
    arg->port = "8600";
    arg->count = 1000;
    arg->ip = (char *)malloc(sizeof(char) * 16);

    while ((opt = getopt(argc, argv, "+:s:p:c:i::")) != -1)
    {
        switch (opt)
        {
        case -1:
            return;
        case 's':
            arg->size = atoi(optarg);
            break;
        case 'p':
            arg->port = optarg;
            break;
        case 'c':
            arg->count = atoi(optarg);
            std::cout << arg->count << "\n";
            break;
        case 'i':
            arg->ip = optarg;
            std::cout << arg->ip << "\n";
            break;
        default:
            continue;
        }
    }
}

#endif