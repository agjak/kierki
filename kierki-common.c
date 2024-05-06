#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <limits.h>
#include <inttypes.h>
#include <errno.h>
#include <stdlib.h>

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

//Code from labs, from file sum-common.c
#include <unistd.h>

// Following two functions come from Stevens' "UNIX Network Programming" book.

// Read n bytes from a descriptor. Use in place of read() when fd is a stream socket.
ssize_t readn(int fd, void *vptr, size_t n) {
    ssize_t nleft, nread;
    char *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) < 0)
            return nread;     // When error, return < 0.
        else if (nread == 0)
            break;            // EOF

        nleft -= nread;
        ptr += nread;
    }
    return n - nleft;         // return >= 0
}

// Write n bytes to a descriptor.
ssize_t writen(int fd, const void *vptr, size_t n){
    ssize_t nleft, nwritten;
    const char *ptr;

    ptr = vptr;               // Can't do pointer arithmetic on void*.
    nleft = n;
    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) <= 0)
            return nwritten;  // error

        nleft -= nwritten;
        ptr += nwritten;
    }
    return n;
}

int writen_data_packet(int client_fd, void* to_write, size_t size)
{
    int written_length = writen(client_fd, to_write, size);
    if ((size_t) written_length < size) {
        error("Error with writen; couldn't write the whole structure");
        return -1;
    }
    return 0;
}

int readn_data_packet(int client_fd, void* result, size_t size)
{
    ssize_t read_length;
    read_length = readn(client_fd, result, size);
    if (read_length < 0) {
        if (errno == EAGAIN) {
            error("Timeout while readn");
        }
        else {
            error("Error with readn; errno %d", errno);
        }
        return -1;
    }
    else if (read_length == 0) {
        error("Connection closed while readn");
        return -1;
    }
    else if ((size_t) read_length < size) {
        error("Connection closed without providing full data structure while readn");
        return -1;
    }
    return 0;
}

int readn_message(int client_fd, void* result, size_t max_size)
{
    ssize_t read_length;
    ssize_t current_location = 0;
    bool end_of_message = false;
    while(!end_of_message)
    {
        read_length = readn(client_fd, &result[current_location], 1);
        if (read_length < 0) {
            if (errno == EAGAIN) {
                error("Timeout while readn");
            }
            else {
                error("Error with readn; errno %d", errno);
            }
            return -1;
        }
        else if (read_length == 0) {
            error("Connection closed while readn");
            return -1;
        }
        
        if(result[current_location] == '\n')
        {
            end_of_message = true;
            result[current_location+1] = '\0';
        }
        if(current_location==max_size-1)
        {
            error("Message bigger than expected");
            return -1;
        }
        current_location++;
    }
    
    return 0;
}