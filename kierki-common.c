#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <limits.h>
#include <inttypes.h>
#include <errno.h>

#include "kierki-common.h"
#include "err.h"

uint16_t read_port(char const *string) {
    char *endptr;
    errno = 0;
    unsigned long port = strtoul(string, &endptr, 10);
    if (errno != 0 || *endptr != 0 || port > UINT16_MAX) {
        fatal("%s is not a valid port number", string);
    }
    return (uint16_t) port;
}

int read_timeout(char const *string) {
    char *endptr;
    errno = 0;
    unsigned long timeout = strtoul(string, &endptr, 10);
    if (errno != 0 || *endptr != 0 ) {
        fatal("%s is not a valid timeout value", string);
    }
    return (int) timeout;
}

struct sockaddr_in get_server_address(char const *host, uint16_t port, bool is_IPv4, bool is_IPv6) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    if(is_IPv4)
    {
        hints.ai_family = AF_INET; // IPv4
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
    }
    else if(is_IPv6)
    {
        hints.ai_family = AF_INET6; // IPv6
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
    }
    else
    {
        hints.ai_family = AF_UNSPEC; // unspecified
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
    }

    struct addrinfo *address_result;
    int errcode = getaddrinfo(host, NULL, &hints, &address_result);
    if (errcode != 0) {
        fatal("getaddrinfo: %s", gai_strerror(errcode));
    }

    struct sockaddr_in send_address;
    send_address.sin_family = address_result->ai_family;   // IP address family
    send_address.sin_addr.s_addr =       // IP address
            ((struct sockaddr_in *) (address_result->ai_addr))->sin_addr.s_addr;
    send_address.sin_port = htons(port); // port from the command line

    freeaddrinfo(address_result);

    return send_address;
}