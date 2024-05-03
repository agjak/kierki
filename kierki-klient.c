#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "err.h"
#include "kierki-common.h"

void load_client_arguments(int argc, char *argv[], uint16_t *port, char const **host, bool *is_IPv4, bool *is_IPv6, bool *is_automatic, char *wanted_place, bool *port_declared);

int main(int argc, char *argv[]) {

    uint16_t port;
    char const *host;
    bool is_IPv4, is_IPv6, is_automatic, port_declared;
    char wanted_place;

    load_client_arguments(argc, argv, &port, &host, &is_IPv4, &is_IPv6, &is_automatic, &wanted_place, &port_declared);

    struct sockaddr_in server_address = get_server_address(host, port, is_IPv4, is_IPv6);
    printf("Input values are port %d, wanted place %c.\n", port, wanted_place);
    
    return 0;

}

void load_client_arguments(int argc, char *argv[], uint16_t *port, char const **host, bool *is_IPv4, bool *is_IPv6, bool *is_automatic, char *wanted_place, bool *port_declared)
{
    *port = 0;
    *is_IPv4 = false;
    *is_IPv6 = false;
    *is_automatic = false;
    *port_declared = false;
    *wanted_place = '0';

    for(int i=1; i<argc; i++)
    {
        if(strcmp(argv[i],"-h")==0)
        {
            *host = argv[i+1];
            i++;
        }
        else if(strcmp(argv[i],"-p")==0)
        {
            *port = read_port(argv[i+1]);
            *port_declared = true;
            i++;
        }
        else if(strcmp(argv[i],"-4")==0)
        {
            *is_IPv4 = true;
            *is_IPv6 = false;
        }
        else if(strcmp(argv[i],"-6")==0)
        {
            *is_IPv4 = false;
            *is_IPv6 = true;
        }
        else if(strcmp(argv[i],"-N")==0 || strcmp(argv[i],"-E")==0 || strcmp(argv[i],"-S")==0 || strcmp(argv[i],"-W")==0)
        {
            *wanted_place=argv[i][1];
        }
        else if(strcmp(argv[i],"-a")==0)
        {
            *is_automatic = true;
        }
        else
        {
            fatal("Incorrect argument where there should be -h, -p, -4, -6, -N, -E, -S, -W, or -a.", argv[0]);
        }
    }

    if(*host == NULL)
    {
        fatal("Host not declared.");
    }
    if(*port_declared == false)
    {
        fatal("Port not declared.");
    }
    if(*wanted_place == '0')
    {
        fatal("Wanted place not declared.");
    }
}