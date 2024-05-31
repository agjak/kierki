#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <limits.h>
#include <inttypes.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h> 
#include <sys/time.h>
#include <signal.h>
#include <arpa/inet.h>

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

void get_server_address(char const *host, uint16_t port, bool *is_IPv4, bool *is_IPv6, struct sockaddr_in *server_address_ipv4, struct sockaddr_in6 *server_address_ipv6) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    if(*is_IPv4)
    {
        hints.ai_family = AF_INET; // IPv4
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
    }
    else if(*is_IPv6)
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

    if(address_result->ai_family == AF_INET)
    {
        *is_IPv4 = true;
        server_address_ipv4->sin_family = address_result->ai_family;   // IP address family
        server_address_ipv4->sin_addr.s_addr =       // IP address
                ((struct sockaddr_in *) (address_result->ai_addr))->sin_addr.s_addr;
        server_address_ipv4->sin_port = htons(port); // port from the command line
        freeaddrinfo(address_result);
    }
    else
    {
        *is_IPv6 = true;
        server_address_ipv6->sin6_family = AF_INET6;   // IP address family
        inet_pton(AF_INET6, host, &server_address_ipv6->sin6_addr);
        
        server_address_ipv6->sin6_port = htons(port); // port from the command line
        freeaddrinfo(address_result);
    }
    

    
}

void install_signal_handler(int signal, void (*handler)(int), int flags) {
    struct sigaction action;
    sigset_t block_mask;

    sigemptyset(&block_mask);
    action.sa_handler = handler;
    action.sa_mask = block_mask;
    action.sa_flags = flags;

    if (sigaction(signal, &action, NULL) < 0 ){
        syserr("sigaction");
    }
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

void take_card_out_of_hand(hand *client_hand, card* card_to_take_out)
{
    bool card_found=false;
    for(int i=0; i<13; i++)
    {
        if(!card_found)
        {
            if(client_hand->cards[i].rank == card_to_take_out->rank && client_hand->cards[i].suit == card_to_take_out->suit)
            {
                card_found=true;
                client_hand->cards[i].rank=0;
                client_hand->cards[i].suit='0';
            }
        }
        else
        {
            client_hand->cards[i-1].rank=client_hand->cards[i].rank;
            client_hand->cards[i-1].suit=client_hand->cards[i].suit;
            client_hand->cards[i].rank=0;
            client_hand->cards[i].suit='0';
        }
    }
}

int cards_amount(hand *client_hand)
{
    for(int i=0; i<13; i++)
    {
        if(client_hand->cards[i].rank == 0 && client_hand->cards[i].suit == '0')
        {
            return i;
        }
    }
    return 13;
}

void print_out_card(card card_to_print)
{
    if(card_to_print.rank<=10)
    {
        printf("%d", card_to_print.rank);
    }
    else if(card_to_print.rank==11)
    {
        printf("J");
    }
    else if(card_to_print.rank==12)
    {
        printf("Q");
    }
    else if(card_to_print.rank==13)
    {
        printf("K");
    }
    else if(card_to_print.rank==14)
    {
        printf("A");
    }
    printf("%c", card_to_print.suit);
}

void print_out_cards_on_hand(hand *hand_to_print)
{
    for(int i = 0; i<cards_amount(hand_to_print); i++)
    {
        print_out_card(hand_to_print->cards[i]);
    }
}

void print_out_cards_played(hand *hand_to_print)
{
    if(hand_to_print->played_cards[0][0].suit=='0' && hand_to_print->played_cards[0][0].rank==0)
    {
        printf("No cards played yet!\n");
        return;
    }
    for(int i=0; i<13; i++)
    {
        if(hand_to_print->played_cards[i][0].suit=='0' && hand_to_print->played_cards[i][0].rank==0)
        {
            printf("\n");
            return;
        }
        else
        {
            for(int j=0; j<4; j++)
            {
                print_out_card(hand_to_print->played_cards[i][j]);
            }
            printf("\n");
        }
    }

}


void write_out_raport(char *message, char *sender_adress_and_port, char *receiver_address_and_port)
{
    struct timeval tv;
    gettimeofday(&tv,NULL);

    struct tm *timeTM;
    timeTM = localtime(&tv.tv_sec);
    char *time_string = malloc(sizeof(char)*40);
    strftime(time_string, 40, "%Y-%m-%dT%X", timeTM);
    int miliseconds = (tv.tv_usec % 1000000) /1000;
    printf("[%s,%s,%s.%d] %s", sender_adress_and_port, receiver_address_and_port, time_string, miliseconds, message);
    free(time_string);
}