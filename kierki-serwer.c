#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "err.h"
#include "kierki-common.h"

void load_server_arguments(int argc, char *argv[], uint16_t *port, int *timeout, char **file_name);

int main(int argc, char *argv[]) {

    uint16_t port = 0;
    int timeout = 5;
    char* file_name = NULL;
    load_server_arguments(argc, argv, &port, &timeout, &file_name);

    int socket_fd = socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd < 0) 
    {
        syserr("cannot create a socket, errno %d", errno);
    }

    return 0;
}

void load_server_arguments(int argc, char *argv[], uint16_t *port, int *timeout, char **file_name)
{
    for(int i=1; i<argc; i=i+2)
    {
        if(strcmp(argv[i],"-p")==0)
        {
            *port = read_port(argv[i+1]);
        }
        else if(strcmp(argv[i],"-f")==0)
        {
            *file_name = argv[i+1];
        }
        else if(strcmp(argv[i],"-t")==0)
        {
            *timeout = read_timeout(argv[i+1]);
        }
        else
        {
            fatal("All odd arguments to %s should be -p, -f, or -t", argv[0]);
        }
    }
    if(*file_name == NULL)
    {
        fatal("File name was not given in arguments");
    }
}