#ifndef PARSEARGS_H
#define PARSEARGS_H

#include <stdlib.h>
#include <getopt.h>
#include <arpa/inet.h>

typedef struct Arguments
{
    uint32_t size;
    int port;
    int count;
    char *ip;
} Arguments;

void parseArguments(Arguments *arg, int argc, char *argv[])
{
    int opt = 0;
    arg->size = 4096;
    arg->port = 8600;
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
            arg->port = atoi(optarg);
            break;
        case 'c':
            arg->count = atoi(optarg);
            break;
        case 'i':
            arg->ip = optarg;
            break;
        default:
            continue;
        }
    }
}

#endif