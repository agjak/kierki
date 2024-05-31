#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <poll.h>
#include <signal.h>

#include "err.h"
#include "kierki-common.h"

#define TIMEOUT       5000

void load_client_arguments(int argc, char *argv[], uint16_t *port, char const **host, bool *is_IPv4, bool *is_IPv6, bool *is_automatic, char *wanted_place, bool *port_declared);
int connect_to_server_ipv4(struct sockaddr_in *server_address);
int connect_to_server_ipv6(struct sockaddr_in6 *server_address);
int send_IAM(int server_fd, char wanted_place, bool is_automatic);
int receive_BUSY_or_DEAL(int server_fd, hand *current_hand, bool is_automatic, int *deal_type);
int print_out_busy_message(char *buffer);
int print_out_deal_message(char *buffer);
int print_out_wrong_message(int current_round);
int print_out_taken_message(char *buffer, int current_round);
int print_out_total_or_score_message(char *buffer, bool is_total);
int print_out_trick_message(char *buffer, hand *current_hand, int current_round);
int load_starting_hand(char *buffer, hand* current_hand);
int receive_TAKEN_or_TRICK_or_SCORE(int server_fd, hand *current_hand, trick *current_trick, int *current_points, int current_round, bool is_automatic, int deal_type, bool *is_taken, bool *is_score, char place_at_table);
int receive_TAKEN_or_WRONG(int server_fd, hand *current_hand, int *current_points, int current_round, bool is_automatic, int deal_type, char place_at_table, bool *is_wrong, trick *current_trick);
int receive_TRICK_or_SCORE(int server_fd, trick *current_trick, int current_round, bool is_automatic, bool *is_score, hand *current_hand);
int load_contents_of_TAKEN(char *buffer, hand *current_hand, int *current_points, int current_round, int deal_type, char place_at_table);
int load_round_number_TAKEN_or_TRICK(char *buffer, int current_round, int *buffer_place);
int load_card_list_TAKEN(char *buffer, hand *current_hand, int *buffer_place, card *cards_taken, int current_round);
int load_client_taking_and_count_points_TAKEN(char *buffer, int current_round, int deal_type, int *current_points, char place_at_table, int *buffer_place, card *cards_taken);
int load_contents_of_TRICK(char *buffer, trick *current_trick, int current_round);
int load_card_from_this_buffer_place(char *buffer, int *buffer_place, card *loaded_card);
int respond_to_TRICK(int server_fd, hand *current_hand, bool is_automatic, int current_round, int deal_type, trick *current_trick);
int receive_TOTAL(int server_fd, bool is_automatic, hand *current_hand);
int receive_SCORE(int server_fd, bool is_automatic, hand *current_hand);
int choose_a_card(int server_fd, card *chosen_card, hand *current_hand, bool is_automatic, int current_round, int deal_type, trick *current_trick);
bool is_this_trick(char *buffer, trick* trick, int current_round);
int readn_message(int server_fd, char* result, size_t max_size, bool is_automatic, char *server_address_and_port, char *client_address_and_port, hand *current_hand);

char *server_address_and_port;
char *client_address_and_port;

int main(int argc, char *argv[]) {

    uint16_t port;
    char const *host;
    bool is_IPv4, is_IPv6, is_automatic, port_declared;
    char wanted_place;

    load_client_arguments(argc, argv, &port, &host, &is_IPv4, &is_IPv6, &is_automatic, &wanted_place, &port_declared);

    struct sockaddr_in *server_address_ipv4=malloc(sizeof(struct sockaddr_in));
    struct sockaddr_in6 *server_address_ipv6=malloc(sizeof(struct sockaddr_in6));

    get_server_address(host, port, &is_IPv4, &is_IPv6, server_address_ipv4, server_address_ipv6);

    int server_fd=0;

    if(is_IPv4)
    {
        server_address_and_port = malloc(sizeof(char) * 40);
        char const *server_ip = inet_ntoa(server_address_ipv4->sin_addr);
        uint16_t server_port = ntohs(server_address_ipv4->sin_port);
        snprintf(server_address_and_port, 40, "%s:%" PRIu16 , server_ip, server_port);
        server_fd = connect_to_server_ipv4(server_address_ipv4);
    }
    else if(is_IPv6)
    {
        server_address_and_port = malloc(sizeof(char) * 40);
        char *server_ip = malloc(25*sizeof(char));
        inet_ntop(AF_INET6, &server_address_ipv6->sin6_addr, server_ip, 25);
        uint16_t server_port = ntohs(server_address_ipv6->sin6_port);
        snprintf(server_address_and_port, 40, "%s:%" PRIu16 , server_ip, server_port);
        free(server_ip);
        server_fd = connect_to_server_ipv6(server_address_ipv6);
    }
    else
    {
        error("Address family is neither ipv4 nor ipv6.");
    }


    struct sockaddr_in my_addr;
    socklen_t my_addr_len = sizeof(my_addr);
    if (getsockname(server_fd, (struct sockaddr *) &my_addr, &my_addr_len) != -1)
    {
        client_address_and_port = malloc(sizeof(char) * 40);
        char const *client_ip = inet_ntoa(my_addr.sin_addr);
        uint16_t client_port = ntohs(my_addr.sin_port);
        snprintf(client_address_and_port, 40, "%s:%" PRIu16 , client_ip, client_port);
    }
    else
    {
        free(server_address_and_port);
        free(client_address_and_port);
        return 1;
    }

    if(send_IAM(server_fd, wanted_place, is_automatic)<0)
    {
        free(server_address_and_port);
        free(client_address_and_port);
        return 1;
    }

    int deal_type, current_points, current_round;
    
    while(true)
    {
        new_hand_dealt:
        deal_type = 0;
        current_points = 0;
        current_round = 0;

        hand *current_hand = malloc(sizeof(hand));
        if(current_hand==NULL)
        {
            free(current_hand);
            free(server_address_and_port);
            free(client_address_and_port);
            syserr("Syserr in malloc");
            return 1;
        }
        current_hand->cards[0].suit='0';
        current_hand->cards[0].rank=0;
        for(int i=0; i<13; i++)
        {
            current_hand->played_cards[i][0].suit='0';
            current_hand->played_cards[i][0].rank=0;
        }
        
        int busy_or_deal_result = receive_BUSY_or_DEAL(server_fd, current_hand, is_automatic, &deal_type);
        if(busy_or_deal_result<0)
        {
            free(current_hand);
            free(server_address_and_port);
            free(client_address_and_port);
            return 1;
        }
        else if(busy_or_deal_result==1)
        {
            free(current_hand);
            free(server_address_and_port);
            free(client_address_and_port);
            return 0;
        }

        trick *current_trick = malloc(sizeof(trick));
        if(current_trick==NULL)
        {
            free(current_trick);
            free(current_hand);
            free(server_address_and_port);
            free(client_address_and_port);
            syserr("Syserr in malloc");
            return 1;
        }

        bool is_taken = true;
        bool is_score = false;
        while(is_taken)
        {
            current_round++;
            int taken_or_trick_or_score_result = receive_TAKEN_or_TRICK_or_SCORE(server_fd, current_hand, current_trick, &current_points, current_round, is_automatic, deal_type, &is_taken, &is_score, wanted_place);
            if(taken_or_trick_or_score_result<0)
            {
                free(current_hand);
                free(current_trick);
                free(server_address_and_port);
                free(client_address_and_port);
                return 1;
            }
            if(is_score)
            {
                free(current_trick);
                int total_result = receive_TOTAL(server_fd, is_automatic, current_hand);
                free(current_hand);
                if(total_result<0)
                {
                    free(server_address_and_port);
                    free(client_address_and_port);
                    return 1;
                }
                goto new_hand_dealt;
            }
        }
        int trick_result;
        trick_response_first:
        trick_result = respond_to_TRICK(server_fd, current_hand, is_automatic, current_round, deal_type, current_trick);
        if(trick_result<0)
        {
            free(current_hand);
            free(current_trick);
            free(server_address_and_port);
            free(client_address_and_port);
            return 1;
        }
        bool is_wrong = false;
        int taken_result = receive_TAKEN_or_WRONG(server_fd, current_hand, &current_points, current_round, is_automatic, deal_type, wanted_place, &is_wrong, current_trick);
        if(taken_result<0)
        {
            free(current_hand);
            free(current_trick);
            free(server_address_and_port);
            free(client_address_and_port);
            return 1;
        }
        if(is_wrong)
        {
            goto trick_response_first;
        }

        while(current_round<13)
        {
            current_round++;
            bool is_score = false;
            int trick_result = receive_TRICK_or_SCORE(server_fd, current_trick, current_round, is_automatic, &is_score, current_hand);
            if(trick_result<0)
            {
                free(current_hand);
                free(current_trick);
                free(server_address_and_port);
                free(client_address_and_port);
                return 1;
            }
            if(is_score)
            {
                free(current_trick);
                int total_result = receive_TOTAL(server_fd, is_automatic, current_hand);
                free(current_hand);
                if(total_result<0)
                {
                    free(server_address_and_port);
                    free(client_address_and_port);
                    return 1;
                }
                goto new_hand_dealt;
            }
            trick_response_loop:
            trick_result = respond_to_TRICK(server_fd, current_hand, is_automatic, current_round, deal_type, current_trick);
            if(trick_result<0)
            {
                free(current_hand);
                free(current_trick);
                free(server_address_and_port);
                free(client_address_and_port);
                return 1;
            }
            bool is_wrong = false;
            int taken_result = receive_TAKEN_or_WRONG(server_fd, current_hand, &current_points, current_round, is_automatic, deal_type, wanted_place, &is_wrong, current_trick);
            if(taken_result<0)
            {
                free(current_hand);
                free(current_trick);
                free(server_address_and_port);
                free(client_address_and_port);
                return 1;
            }
            if(is_wrong)
            {
                goto trick_response_loop;
            }
        }

        
        int score_result = receive_SCORE(server_fd, is_automatic, current_hand);
        if(score_result<0)
        {
            free(current_hand);
            free(current_trick);
            free(server_address_and_port);
            free(client_address_and_port);
            return 1;
        }

        int total_result = receive_TOTAL(server_fd, is_automatic, current_hand);
        if(total_result<0)
        {
            free(current_hand);
            free(current_trick);
            free(server_address_and_port);
            free(client_address_and_port);
            return 1;
        }
        free(current_hand);
        free(current_trick);
    }
    free(server_address_and_port);
    free(client_address_and_port);
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
    *host = "none";

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

    if(strcmp(*host, "none") == 0)
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

int connect_to_server_ipv4(struct sockaddr_in *server_address)
{
    int server_fd = socket(server_address->sin_family, SOCK_STREAM, 0);
    if (server_fd < 0) {
        syserr("cannot create a socket, errno %d", errno);
    }
    // Connect to the server.
    if (connect(server_fd, (struct sockaddr *) server_address,
                (socklen_t) sizeof(*server_address)) < 0) {
        syserr("cannot connect to the server, errno %d", errno);
    }
    return server_fd;
}

int connect_to_server_ipv6(struct sockaddr_in6 *server_address)
{
    int server_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd < 0) {
        syserr("cannot create a socket, errno %d", errno);
    }
    // Connect to the server.
    if (connect(server_fd, (struct sockaddr *) server_address,
                (socklen_t) sizeof(*server_address)) < 0) {
        syserr("cannot connect to the server, errno %d", errno);
    }
    return server_fd;
}

int send_IAM(int server_fd, char wanted_place, bool is_automatic)
{
    char *buffer=malloc(7*sizeof(char));
    if(buffer==NULL)
    {
        free(buffer);
        syserr("Syserr in malloc");
    }
    snprintf(buffer, 7, "IAM%c\r\n", wanted_place);
    int result = writen_data_packet(server_fd, buffer, 6);
    if(is_automatic)
    {
        write_out_raport(buffer, client_address_and_port, server_address_and_port);
    }
    free(buffer);
    return result;
}

// results:
// -2: received BUSY
// -1: error
// 0: received DEAL
int receive_BUSY_or_DEAL(int server_fd, hand *current_hand, bool is_automatic, int *deal_type)
{
    char *buffer = malloc(40*sizeof(char));
    if(buffer == NULL)
    {
        syserr("Malloc error");
        free(buffer);
        return -1;
    }
    int readn_message_result = readn_message(server_fd, buffer, 40, is_automatic, server_address_and_port, client_address_and_port, current_hand);
    if(readn_message_result!=0)
    {
        free(buffer);
        return readn_message_result;
    }

    if(buffer[0] == 'B' && buffer[1] == 'U' && buffer[2] == 'S' && buffer[3] == 'Y')
    {
        if(!is_automatic)
        {
            // TODO: check if the contents of the busy message are as they should be even if the client is automatic
            if(print_out_busy_message(buffer) == -1)
            {
                free(buffer);
                return -1;
            }
        }
        free(buffer);
        return -2;
    }
    else if(buffer[0] == 'D' && buffer[1] == 'E' && buffer[2] == 'A' && buffer[3] == 'L')
    {
        if(buffer[4]>='1' && buffer[4]<='7') {
            *deal_type = buffer[4]-'0';
        }
        else {
            error("Wrong contents of DEAL message. Message content : %s", buffer);
            free(buffer);
            return -1;
        }

        if(!(buffer[5] == 'N' || buffer[5] == 'E' || buffer[5] == 'S' || buffer[5] == 'W')){
            error("Wrong contents of DEAL message. Message content : %s", buffer);
            free(buffer);
            return -1;
        }

        if(load_starting_hand(buffer, current_hand)==-1)
        {
            free(buffer);
            return -1;
        }
        if(!is_automatic)
        {
            if(print_out_deal_message(buffer) == -1)
            {
                free(buffer);
                return -1;
            }
        }
    }
    else
    {
        error("Unrecognised type of message, expected BUSY or DEAL. Message content : %s", buffer);
        free(buffer);
        return -1;
    }
    free(buffer);
    return 0;
}

int print_out_busy_message(char *buffer)
{
    int buffer_place = 4;

    bool has_north = false;
    bool has_west = false;
    bool has_south = false;
    bool has_east = false;

    while(buffer[buffer_place]!='\n' && buffer[buffer_place]!='\r')
    {
        if((buffer[buffer_place]=='N' && !has_north))
        {
            has_north = true;
        }
        else if(buffer[buffer_place]=='W' && !has_west)
        {
            has_west = true;
        }
        else if(buffer[buffer_place]=='S' && !has_south)
        {
            has_south = true;
        }
        else if(buffer[buffer_place]=='E' && !has_east)
        {
            has_east = true;
        }
        else
        {
            error("Wrong contents of the BUSY message. Message content : %s", buffer);
            return -1;
        }
        buffer_place++;
    }

    printf("BUSY");
    for(int i=4; i<buffer_place; i++)
    {
        printf("%c", buffer[i]);
    }
    printf("\n");

    printf("Place busy, list of busy places received: ");
    for(int i=4; i<buffer_place; i++)
    {
        printf("%c", buffer[i]);
    }
    printf(".\n\n");

    return 0;
}

int print_out_deal_message(char *buffer)
{
    char deal_type = buffer[4];
    char starting_place = buffer[5];
    int card_list_start = 6;
    int card_list_end = 0;

    while(buffer[card_list_end] != '\r')
        card_list_end++;

    for(int i=0; i<card_list_end; i++)
    {
        printf("%c", buffer[i]);
    }
    printf("\n");
    printf("New deal %c: staring place %c, your cards: ", deal_type, starting_place);

    int i = card_list_start;
    card *card_to_print=malloc(sizeof(card));
    while(i!=card_list_end)
    {
        load_card_from_this_buffer_place(buffer, &i, card_to_print);
        print_out_card(*card_to_print);
        if(i!=card_list_end)
        {
            printf(", ");
        }
    }
    free(card_to_print);
    printf(".\n\n");
    return 0;
}

int print_out_wrong_message(int current_round)
{
    printf("WRONG%i\nWrong message received in trick %i.\n\n", current_round, current_round);
    return 0;
}

int print_out_taken_message(char *buffer, int current_round)
{
    int card_list_start;
    if(current_round<10)
    {
        card_list_start = 6;
    }
    else
    {
        card_list_start = 7;
    }
    int taking_client_location = card_list_start;

    while(buffer[taking_client_location] != '\r')
    {
        taking_client_location++;
    }
    taking_client_location--;
    for(int i=0; i<=taking_client_location; i++)
    {
        printf("%c", buffer[i]);
    }
    printf("\n");
    printf("A trick %d is taken by %c, cards ", current_round, buffer[taking_client_location]);

    int i = card_list_start;
    card *card_to_print=malloc(sizeof(card));
    while(i!=taking_client_location)
    {
        load_card_from_this_buffer_place(buffer, &i, card_to_print);
        print_out_card(*card_to_print);
        if(i!=taking_client_location)
        {
            printf(", ");
        }
    }
    free(card_to_print);

    printf(".\n\n");
    return 0;
}

int print_out_total_or_score_message(char *buffer, bool is_total)
{
    int buffer_place = 0;
    while(buffer[buffer_place] != '\r' && buffer[buffer_place] != '\n')
    {
        printf("%c", buffer[buffer_place]);
        buffer_place++;
    }
    printf("\n");

    if(is_total)
    {
        printf("The total scores are:\n");
    }
    else
    {
        printf("The scores are:\n");
    }
    
    buffer_place = 5;
    while(buffer[buffer_place] != '\r' && buffer[buffer_place] != '\n')
    {
        printf("%c | ", buffer[buffer_place]);
        buffer_place++;
        while(buffer[buffer_place] >= '0' && buffer[buffer_place] <= '9')
        {
            printf("%c", buffer[buffer_place]);
            buffer_place++;
        }
        printf("\n");
    }
    printf("\n");
    return 0;
}

int print_out_trick_message(char *buffer, hand *current_hand, int current_round)
{
    int buffer_place = 0;
    while(buffer[buffer_place]!='\r')
    {
        printf("%c", buffer[buffer_place]);
        buffer_place++;
    }
    printf("\n");

    buffer_place = 6;
    if(current_round >= 10)
    {
        buffer_place = 7;
    }

    printf("Trick: (%d) ", current_round);
    card *card_to_print=malloc(sizeof(card));
    while(buffer[buffer_place]!='\r')
    {
        load_card_from_this_buffer_place(buffer, &buffer_place, card_to_print);
        print_out_card(*card_to_print);
        if(buffer[buffer_place]!='\r')
        {
            printf(", ");
        }
    }
    printf("\n");
    free(card_to_print);
    
    printf("Available: ");
    print_out_cards_on_hand(current_hand);
    printf("\n\n");
    return 0;
}

int load_starting_hand(char *buffer, hand* current_hand)
{
    int currently_loading_card = 0;
    int buffer_place = 6;
    while(buffer[buffer_place]!='\n' && buffer[buffer_place]!='\r')
    {
        int loading_result = load_card_from_this_buffer_place(buffer, &buffer_place, &current_hand->cards[currently_loading_card]);
        if(loading_result<0)
            return loading_result;

        currently_loading_card++;
    }
    return 0;
}

int receive_TAKEN_or_TRICK_or_SCORE(int server_fd, hand *current_hand, trick *current_trick, int *current_points, int current_round, bool is_automatic, int deal_type, bool *is_taken, bool *is_score, char place_at_table)
{
    char *buffer = malloc(25*sizeof(char));
    if(buffer == NULL)
    {
        syserr("Malloc error");
        free(buffer);
        return -1;
    }

    int readn_message_result = readn_message(server_fd, buffer, 25, is_automatic, server_address_and_port, client_address_and_port, current_hand);
    if(readn_message_result!=0)
    {
        free(buffer);
        return -1;
    }
        

    if(buffer[0] == 'T' && buffer[1] == 'A' && buffer[2] == 'K' && buffer[3] == 'E' && buffer[4] == 'N')
    {
        *is_taken = true;
        int result = load_contents_of_TAKEN(buffer, current_hand, current_points, current_round, deal_type, place_at_table);
        if(!is_automatic && result == 0)
        {
            if(print_out_taken_message(buffer, current_round) == -1)
            {
                free(buffer);
                result = -1;
            }
        }
        free(buffer);
        return result;
    }
    else if(buffer[0] == 'T' && buffer[1] == 'R' && buffer[2] == 'I' && buffer[3] == 'C' && buffer[4] == 'K')
    {
        *is_taken = false;
        int result = load_contents_of_TRICK(buffer, current_trick, current_round);
        if(!is_automatic && result == 0)
        {
            if(print_out_trick_message(buffer, current_hand, current_round) == -1)
            {
                free(buffer);
                result = -1;
            }
        }
        free(buffer);
        return result;
    }
    else if(buffer[0] == 'S' && buffer[1] == 'C' && buffer[2] == 'O' && buffer[3] == 'R' && buffer[4] == 'E')
    {
        *is_taken = false;
        *is_score = true;
        int result = 0;
        if(!is_automatic)
        {
            result=print_out_total_or_score_message(buffer,false);
        }
        free(buffer);
        return result;
    }
    else
    {
        error("Unrecognised type of message, expected TAKEN or TRICK or SCORE. Message content : %s", buffer);
        free(buffer);
        return -1;
    }
}

int receive_TAKEN_or_WRONG(int server_fd, hand *current_hand, int *current_points, int current_round, bool is_automatic, int deal_type, char place_at_table, bool *is_wrong, trick *current_trick)
{
    char *buffer = malloc(25*sizeof(char));
    if(buffer == NULL)
    {
        syserr("Malloc error");
        return -1;
    }

    int readn_message_result = readn_message(server_fd, buffer, 25, is_automatic, server_address_and_port, client_address_and_port, current_hand);
    if(readn_message_result!=0)
    {
        free(buffer);
        return -1;
    }

    if(buffer[0] == 'T' && buffer[1] == 'A' && buffer[2] == 'K' && buffer[3] == 'E' && buffer[4] == 'N')
    {
        int result = load_contents_of_TAKEN(buffer, current_hand, current_points, current_round, deal_type, place_at_table);
        if(!is_automatic && result == 0)
        {
            if(print_out_taken_message(buffer, current_round) == -1)
            {
                free(buffer);
                result = -1;
            }
        }
        free(buffer);
        return result;
    }
    else if(buffer[0] == 'W' && buffer[1] == 'R' && buffer[2] == 'O' && buffer[3] == 'N' && buffer[4] == 'G')
    {
        *is_wrong = true;
        if(current_round<10)
        {
            if(buffer[5] - '0' == current_round && buffer[6] == '\r' && buffer[7] == '\n')
            {
                free(buffer);
                if(!is_automatic)
                {
                    if(print_out_wrong_message(current_round) == -1)
                        return -1;
                }
                return 0;
            }
            else
            {
                error("Wrong contents of WRONG message. Message content : %s", buffer);
                free(buffer);
                return -1;
            }
        }
        else
        {
            if(buffer[5] == '1' && buffer[6] - '0' == current_round - 10 && buffer[7] == '\r' && buffer[8] == '\n')
            {
                free(buffer);
                if(!is_automatic)
                {
                    if(print_out_wrong_message(current_round) == -1)
                        return -1;
                }
                return 0;
            }
            else
            {
                error("Wrong contents of WRONG message. Message content : %s", buffer);
                free(buffer);
                return -1;
            }
        }
    }
    else if(buffer[0] == 'T' && buffer[1] == 'R' && buffer[2] == 'I' && buffer[3] == 'C' && buffer[4] == 'K')
    {
        // Additional trick sent while client was thinking how to respond
        if(is_this_trick(buffer, current_trick, current_round))
        {
            if(print_out_trick_message(buffer, current_hand, current_round) == -1)
            {
                free(buffer);
                return -1;
            }
            free(buffer);
            return receive_TAKEN_or_WRONG(server_fd, current_hand, current_points, current_round, is_automatic, deal_type, place_at_table, is_wrong, current_trick);
        }
        else
        {
            error("Wrong type of message, expected TAKEN or WRONG, received a TRICK message that is different from the last one sent. Message content : %s", buffer);
            free(buffer);
            return -1;
        }
    }
    else
    {
        error("Unrecognised type of message, expected TAKEN or WRONG. Message content : %s", buffer);
        free(buffer);
        return -1;
    }
}

int receive_TRICK_or_SCORE(int server_fd, trick *current_trick, int current_round, bool is_automatic, bool *is_score, hand *current_hand)
{
    char *buffer = malloc(25*sizeof(char));
    if(buffer == NULL)
    {
        free(buffer);
        syserr("Malloc error");
        return -1;
    }
    
    int readn_message_result = readn_message(server_fd, buffer, 25, is_automatic, server_address_and_port, client_address_and_port, current_hand);
    if(readn_message_result!=0)
    {
        free(buffer);
        return -1;
    }

    if(buffer[0] == 'T' && buffer[1] == 'R' && buffer[2] == 'I' && buffer[3] == 'C' && buffer[4] == 'K')
    {
        int result = load_contents_of_TRICK(buffer, current_trick, current_round);
        if(!is_automatic && result == 0)
        {
            if(print_out_trick_message(buffer, current_hand, current_round) == -1)
            {
                free(buffer);
                result = -1;
            }
        }
        free(buffer);
        return result;
    }
    else if(buffer[0] == 'S' && buffer[1] == 'C' && buffer[2] == 'O' && buffer[3] == 'R' && buffer[4] == 'E')
    {
        *is_score = true;
        int result = 0;
        if(!is_automatic)
        {
            result=print_out_total_or_score_message(buffer,false);
        }
        free(buffer);
        return result;
    }
    else
    {
        error("Unrecognised type of message, expected TRICK or SCORE. Message content : %s", buffer);
        free(buffer);
        return -1;
    }
}

int load_contents_of_TAKEN(char *buffer, hand *current_hand, int *current_points, int current_round, int deal_type, char place_at_table)
{
    int buffer_place=5;
    int load_round_number_results = load_round_number_TAKEN_or_TRICK(buffer, current_round, &buffer_place);
    if(load_round_number_results < 0)
        return load_round_number_results;
    
    card *cards_taken = malloc(4*sizeof(card));
    if(cards_taken == NULL)
    {
        syserr("Malloc error");
        free(cards_taken);
        return -1;
    }
    int load_card_list_results = load_card_list_TAKEN(buffer, current_hand, &buffer_place, cards_taken, current_round);
    if(load_card_list_results < 0)
    {
        free(cards_taken);
        return load_card_list_results;
    }
        

    int load_client_taking_result = load_client_taking_and_count_points_TAKEN(buffer, current_round, deal_type, current_points, place_at_table, &buffer_place, cards_taken);
    free(cards_taken);
    if(load_client_taking_result < 0)
        return load_card_list_results;
    
    if(buffer[buffer_place]!='\r' || buffer[buffer_place+1]!='\n')
    {
        error("Wrong contents of a message. Message content : %s", buffer);
        return -1;
    } 
    return 0;
}

int load_round_number_TAKEN_or_TRICK(char *buffer, int current_round, int *buffer_place)
{
    if(current_round<10)
    {
        if(buffer[*buffer_place]-'0' == current_round)
        {
            *buffer_place = *buffer_place+1;
            return 0;
        }
        else
        {
            error("Wrong round number in the message: %s", buffer);
            return -1;
        }
    }
    else
    {
        if(buffer[*buffer_place] == '1' && buffer[*buffer_place + 1]-'0' == current_round-10)
        {
            *buffer_place = *buffer_place+2;
            return 0;
        }
        else
        {
            error("Wrong round number in the message: %s", buffer);
            return -1;
        }
    }
}

int load_card_list_TAKEN(char *buffer, hand *current_hand, int *buffer_place, card *cards_taken, int current_round)
{
    int current_card_amount = cards_amount(current_hand);
    for(int i=0; i<4; i++)
    {
        int load_card_results = load_card_from_this_buffer_place(buffer, buffer_place, &cards_taken[i]);
        if(load_card_results<0)
            return load_card_results;

        take_card_out_of_hand(current_hand, &cards_taken[i]);
        current_hand->played_cards[current_round - 1][i].rank = cards_taken[i].rank;
        current_hand->played_cards[current_round - 1][i].suit = cards_taken[i].suit;
    }
    if(cards_amount(current_hand) != current_card_amount-1)
    {
        error("This taken message had no, or too many, cards belonging to this client: %s", buffer);
        return -1;
    }
    return 0;
}

int load_client_taking_and_count_points_TAKEN(char *buffer, int current_round, int deal_type, int *current_points, char place_at_table, int *buffer_place, card *cards_taken)
{
    if(buffer[*buffer_place] == 'N' || buffer[*buffer_place] == 'E' || buffer[*buffer_place] == 'S' || buffer[*buffer_place] == 'W')
    {
        *buffer_place = *buffer_place + 1;
        if(place_at_table == buffer[*buffer_place])
        {
            if(deal_type == 1 || deal_type == 7)
            {
                *current_points = *current_points + 1;
            }
            if(deal_type == 2 || deal_type == 7)
            {
                for(int i=0; i<4; i++)
                {
                    if(cards_taken[i].suit == 'H')
                    {
                        *current_points = *current_points + 1;
                    }
                }
            }
            if(deal_type == 3 || deal_type == 7)
            {
                for(int i=0; i<4; i++)
                {
                    if(cards_taken[i].rank == 12)
                    {
                        *current_points = *current_points + 5;
                    }
                }
            }
            if(deal_type == 4 || deal_type == 7)
            {
                for(int i=0; i<4; i++)
                {
                    if(cards_taken[i].rank == 11 || cards_taken[i].rank == 13)
                    {
                        *current_points = *current_points + 2;
                    }
                }
            }
            if(deal_type == 5 || deal_type == 7)
            {
                for(int i=0; i<4; i++)
                {
                    if(cards_taken[i].suit == 'H' || cards_taken[i].rank == 13)
                    {
                        *current_points = *current_points + 18;
                    }
                }
            }
            if(deal_type == 6 || deal_type == 7)
            {
                if(current_round == 7 || current_round == 13)
                {
                    *current_points = *current_points + 10;
                }
            }
        }

    }
    else
    {
        error("Wrong contents of a message. Message content : %s", buffer);
        return -1;
    }
    return 0;
}

int load_contents_of_TRICK(char *buffer, trick *current_trick, int current_round)
{
    int buffer_place=5;
    int load_round_number_results = load_round_number_TAKEN_or_TRICK(buffer, current_round, &buffer_place);
    if(load_round_number_results < 0)
        return load_round_number_results;

    current_trick->amount=0;
    while(buffer[buffer_place]!='\r' && current_trick->amount < 3)
    {
        int load_card_result = load_card_from_this_buffer_place(buffer, &buffer_place, &(current_trick->cards[current_trick->amount]));
        if (load_card_result < 0)
        {
            return load_card_result;
        }
        else
        {
            current_trick->amount++;
        }
    }

    if(buffer[buffer_place]!='\r' || buffer[buffer_place+1]!='\n')
    {
        error("Wrong contents of a message. Message content : %s", buffer);
        return -1;
    } 
    return 0;
}

int load_card_from_this_buffer_place(char *buffer, int *buffer_place, card *loaded_card)
{
    if(buffer[*buffer_place]>='2' && buffer[*buffer_place]<='9')
    {
        loaded_card->rank = buffer[*buffer_place] - '0';
        *buffer_place = *buffer_place+1;
    }
    else if(buffer[*buffer_place] == '1' && buffer[*buffer_place+1] == '0')
    {
        loaded_card->rank = 10;
        *buffer_place = *buffer_place + 2;
    }
    else if(buffer[*buffer_place] == 'J')
    {
        loaded_card->rank = 11;
        *buffer_place = *buffer_place+1;
    }
    else if(buffer[*buffer_place] == 'Q')
    {
        loaded_card->rank = 12;
        *buffer_place = *buffer_place+1;
    }
    else if(buffer[*buffer_place] == 'K')
    {
        loaded_card->rank = 13;
        *buffer_place = *buffer_place+1;
    }
    else if(buffer[*buffer_place] == 'A')
    {
        loaded_card->rank = 14;
        *buffer_place = *buffer_place+1;
    }
    else
    {
        error("Wrong contents of a message. Message content : %s", buffer);
        return -1;
    }

    if(buffer[*buffer_place] == 'C' || buffer[*buffer_place] == 'D' || buffer[*buffer_place] == 'H' || buffer[*buffer_place] == 'S')
    {
        loaded_card->suit = buffer[*buffer_place];
        *buffer_place = *buffer_place+1;
    }
    else 
    {
        error("Wrong contents of a message. Message content : %s", buffer);
        return -1;
    }
    return 0;
}

int respond_to_TRICK(int server_fd, hand *current_hand, bool is_automatic, int current_round, int deal_type, trick *current_trick)
{
    card *chosen_card = malloc(sizeof(card));
    if(chosen_card == NULL)
    {
        syserr("Malloc error");
        free(chosen_card);
        return -1;
    }
    if(choose_a_card(server_fd, chosen_card, current_hand, is_automatic, current_round, deal_type, current_trick)<0)
    {
        return -1;
    }

    int msg_size = 10;
    int rank_size = 1;
    if(current_round > 9)
        msg_size++;

    if(chosen_card->rank == 10)
    {
        msg_size++;
        rank_size++;
    }

    char *chosen_rank = malloc((rank_size+1) * sizeof(char));
    if(chosen_rank == NULL)
    {
        syserr("Malloc error");
        free(chosen_card);
        free(chosen_rank);
        return -1;
    }

    if(chosen_card->rank == 10){
        chosen_rank[0] = '1';
        chosen_rank[1] = '0';
        chosen_rank[2] = 0;
    }
    else
    {
        if(chosen_card->rank >= 2 && chosen_card->rank <=9){
        chosen_rank[0] = '0' + chosen_card->rank;
        }
        if(chosen_card->rank == 11){
            chosen_rank[0] = 'J';
        }
        if(chosen_card->rank == 12){
            chosen_rank[0] = 'Q';
        }
        if(chosen_card->rank == 13){
            chosen_rank[0] = 'K';
        }
        if(chosen_card->rank == 14){
            chosen_rank[0] = 'A';
        }
        chosen_rank[1] = 0;
    }
    msg_size++;
    char *message = malloc(msg_size * sizeof(char));
    if(message == NULL)
    {
        syserr("Malloc error");
        free(message);
        free(chosen_card);
        free(chosen_rank);
        return -1;
    }
    snprintf(message, msg_size, "TRICK%d%s%c\r\n", current_round, chosen_rank, chosen_card->suit);
    int result = writen_data_packet(server_fd, message, msg_size);
    if(is_automatic)
    {
        write_out_raport(message, client_address_and_port, server_address_and_port);
    }
    free(message);
    free(chosen_rank);
    free(chosen_card);
    return result;

}

int receive_TOTAL(int server_fd, bool is_automatic, hand *current_hand)
{
    char *buffer = malloc(25*sizeof(char));
    if(buffer == NULL)
    {
        syserr("Malloc error");
        return -1;
    }
    
    int readn_message_result = readn_message(server_fd, buffer, 25, is_automatic, server_address_and_port, client_address_and_port, current_hand);
    if(readn_message_result!=0)
    {
        free(buffer);
        return -1;
    }

    if(buffer[0] == 'T' && buffer[1] == 'O' && buffer[2] == 'T' && buffer[3] == 'A' && buffer[4] == 'L')
    {
        int result = 0;
        if(!is_automatic)
        {
            result=print_out_total_or_score_message(buffer,true);
        }
        free(buffer);
        return result;
    }
    else
    {
        error("Unrecognised type of message, expected TOTAL. Message content : %s", buffer);
        free(buffer);
        return -1;
    }
}

int receive_SCORE(int server_fd, bool is_automatic, hand *current_hand)
{
    char *buffer = malloc(25*sizeof(char));
    if(buffer == NULL)
    {
        syserr("Malloc error");
        return -1;
    }
    
    int readn_message_result = readn_message(server_fd, buffer, 25, is_automatic, server_address_and_port, client_address_and_port, current_hand);
    if(readn_message_result!=0)
    {
        free(buffer);
        return -1;
    }

    if(buffer[0] == 'S' && buffer[1] == 'C' && buffer[2] == 'O' && buffer[3] == 'R' && buffer[4] == 'E')
    {
        int result = 0;
        if(!is_automatic)
        {
            result=print_out_total_or_score_message(buffer,false);
        }
        free(buffer);
        return result;
    }
    else
    {
        error("Unrecognised type of message, expected SCORE. Message content : %s", buffer);
        free(buffer);
        return -1;
    }
}

int choose_a_card(int server_fd, card *chosen_card, hand *current_hand, bool is_automatic, int current_round, int deal_type, trick *current_trick)
{
    if(is_automatic)
    {
        if(current_trick->amount == 0)
        {
            chosen_card->rank = current_hand->cards[0].rank;
            chosen_card->suit = current_hand->cards[0].suit;
        }
        else
        {
            char current_trick_suit = current_trick->cards[0].suit;
            chosen_card->rank = 0;
            chosen_card->suit = '0';
            for(int i=0; i<cards_amount(current_hand); i++)
            {
                if(current_hand->cards[i].suit == current_trick_suit)
                {
                    chosen_card->rank = current_hand->cards[i].rank;
                    chosen_card->suit = current_hand->cards[i].suit;
                }
            }
            if(chosen_card->rank == 0 && chosen_card->suit == '0')
            {
                chosen_card->rank = current_hand->cards[0].rank;
                chosen_card->suit = current_hand->cards[0].suit;
            }
        }
        return 0;
    }
    else
    {
        struct pollfd poll_descriptors[2];

        poll_descriptors[0].fd = server_fd;
        poll_descriptors[0].events = POLLIN;
        poll_descriptors[0].revents = 0;

        poll_descriptors[1].fd = 0;
        poll_descriptors[1].events = POLLIN;
        poll_descriptors[1].revents = 0;

        do {
            for (int i = 0; i < 2; ++i) {
                poll_descriptors[i].revents = 0;
            }

            int poll_status = poll(poll_descriptors, 2, TIMEOUT);
            if (poll_status == -1 ) {
                if (errno == EINTR) {
                    error("interrupted system call");
                    return -1;
                }
                else {
                    error("poll");
                    return -1;
                }
            }
            else if (poll_status > 0) {
                //Read the additional TRICK
                if ((poll_descriptors[0].revents & POLLIN)!=0) {
                    
                    char *buffer = malloc(25*sizeof(char));
                    if(buffer == NULL)
                    {
                        syserr("Malloc error");
                        free(buffer);
                        return -1;
                    }

                    int readn_message_result = readn_message(server_fd, buffer, 25, is_automatic, server_address_and_port, client_address_and_port, current_hand);
                    if(readn_message_result!=0)
                    {
                        free(buffer);
                        return -1;
                    }

                    if(buffer[0] == 'T' && buffer[1] == 'R' && buffer[2] == 'I' && buffer[3] == 'C' && buffer[4] == 'K')
                    {
                        if(is_this_trick(buffer, current_trick, current_round))
                        {
                            if(print_out_trick_message(buffer, current_hand, current_round) == -1)
                            {
                                free(buffer);
                                return -1;
                            }
                            free(buffer);
                            continue;
                        }
                        else
                        {
                            error("Wrong TRICK received while thinking about the next move, expected the repeat of the last TRICK. Message content : %s", buffer);
                            free(buffer);
                            return -1;
                        }
                    }
                    else
                    {
                        error("A message other than TRICK received while thinking about the next move. The message: %s", buffer);
                        free(buffer);
                        return -1;
                    }

                }
                else if((poll_descriptors[1].revents & POLLIN)!=0) {
                    char *command = malloc(40*sizeof(char));
                    if(command == NULL)
                    {
                        syserr("Malloc error");
                        free(command);
                        return -1;
                    }
                    scanf("%s", command);
                    if(command[0] == 'c' && command[1] == 'a' && command[2] == 'r' && command[3] == 'd' && command[4] == 's')
                    {
                        free(command);
                        print_out_cards_on_hand(current_hand);
                        printf("\n\n");
                        return choose_a_card(server_fd, chosen_card, current_hand, is_automatic, current_round, deal_type, current_trick);
                    }
                    else if(command[0] == 't' && command[1] == 'r' && command[2] == 'i' && command[3] == 'c' && command[4] == 'k' && command[5] == 's')
                    {
                        free(command);
                        print_out_cards_played(current_hand);
                        return choose_a_card(server_fd, chosen_card, current_hand, is_automatic, current_round, deal_type, current_trick);
                    }
                    else if(command[0] == '!')
                    {
                        int buffer_place = 1;
                        if(load_card_from_this_buffer_place(command, &buffer_place, chosen_card) == -1)
                        {
                            printf("Wrong command entered, please try again.\n");
                            free(command);
                            return choose_a_card(server_fd, chosen_card, current_hand, is_automatic, current_round, deal_type, current_trick);
                        }
                        free(command);
                        return 0;
                    }
                    else
                    {
                        printf("Wrong command entered, please try again.\n");
                        free(command);
                        return choose_a_card(server_fd, chosen_card, current_hand, is_automatic, current_round, deal_type, current_trick);
                    }
                }
                
            }

        } while(true);

        return 0;
    }
}

bool is_this_trick(char *buffer, trick *compared_trick, int current_round)
{
    trick *buffer_trick = malloc(sizeof(trick));
    if(load_contents_of_TRICK(buffer, buffer_trick, current_round)!=0)
    {
        free(buffer_trick);
        return false;
    }
    if(buffer_trick->amount != compared_trick->amount)
    {
        free(buffer_trick);
        return false;
    }
    for(int i=0; i<buffer_trick->amount; i++)
    {
        if(buffer_trick->cards[i].rank != compared_trick->cards[i].rank)
        {
            free(buffer_trick);
            return false;
        }
        if(buffer_trick->cards[i].suit != compared_trick->cards[i].suit)
        {
            free(buffer_trick);
            return false;
        }
    }
    free(buffer_trick);
    return true;
}


int readn_message(int server_fd, char* result, size_t max_size, bool is_automatic, char *server_address_and_port, char *client_address_and_port, hand *current_hand)
{

    struct pollfd poll_descriptors[2];

    // The main socket has index 0.
    poll_descriptors[0].fd = server_fd;
    poll_descriptors[0].events = POLLIN;
    poll_descriptors[0].revents = 0;

    poll_descriptors[1].fd = 0;
    poll_descriptors[1].events = POLLIN;
    poll_descriptors[1].revents = 0;

    do {
        for (int i = 0; i < 2; ++i) {
            poll_descriptors[i].revents = 0;
        }

        int poll_status = poll(poll_descriptors, 2, TIMEOUT);
        if (poll_status == -1 ) {
            if (errno == EINTR) {
                error("interrupted system call");
            }
            else {
                syserr("poll");
            }
        }
        else if (poll_status > 0) {
            //Read the message
            if (poll_descriptors[0].revents & (POLLIN | POLLHUP)) {
                ssize_t read_length;
                ssize_t current_location = 0;
                bool end_of_message = false;
                while(!end_of_message)
                {
                    read_length = readn(server_fd, &result[current_location], 1);
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
                        //Connection closed
                        return 1;
                    }
                    
                    if(result[current_location] == '\n')
                    {
                        end_of_message = true;
                        result[current_location+1] = '\0';
                    }
                    if(current_location==(ssize_t)max_size-1)
                    {
                        error("Message bigger than expected");
                        return -1;
                    }
                    current_location++;
                }
                if(is_automatic)
                {
                    write_out_raport(result, server_address_and_port, client_address_and_port);
                }
                
                return 0;
            }   //reading interrupted by a stdin
            else if(poll_descriptors[1].revents & POLLIN) {
                char *command = malloc(40*sizeof(char));
                if(command == NULL)
                {
                    syserr("Malloc error");
                    free(command);
                }
                scanf("%s", command);
                if(command[0] == 'c' && command[1] == 'a' && command[2] == 'r' && command[3] == 'd' && command[4] == 's')
                {
                    free(command);
                    if(cards_amount(current_hand)==0)
                    {
                        printf("\n");
                    }
                    print_out_cards_on_hand(current_hand);
                    printf("\n\n");
                }
                else if(command[0] == 't' && command[1] == 'r' && command[2] == 'i' && command[3] == 'c' && command[4] == 'k' && command[5] == 's')
                {
                    free(command);
                    print_out_cards_played(current_hand);
                }
                else
                {
                    printf("Wrong command entered, please try again.\n");
                    free(command);
                }
            }
            
        }

    } while(true);

    return 1;
}