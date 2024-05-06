#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>

#include "err.h"
#include "kierki-common.h"

void load_client_arguments(int argc, char *argv[], uint16_t *port, char const **host, bool *is_IPv4, bool *is_IPv6, bool *is_automatic, char *wanted_place, bool *port_declared);
int connect_to_server(struct sockaddr_in *server_address);
int send_IAM(int server_fd, char wanted_place);
int receive_BUSY_or_DEAL(int server_fd, hand *current_hand, bool is_automatic, int *deal_type);
int print_out_busy_message(char *buffer);
int load_starting_hand(char *buffer, hand* current_hand);
int receive_TAKEN_or_TRICK(int server_fd, hand *current_hand, trick *current_trick, int *current_points, int *current_round, bool is_automatic, int deal_type, bool *is_taken, char place_at_table);
int load_contents_of_TAKEN(char *buffer, hand *current_hand, int *current_points, int *current_round, bool is_automatic, int deal_type, char place_at_table);
int load_contents_of_TRICK(char *buffer, trick *current_trick, bool is_automatic);

int main(int argc, char *argv[]) {

    uint16_t port;
    char const *host;
    bool is_IPv4, is_IPv6, is_automatic, port_declared;
    char wanted_place;

    load_client_arguments(argc, argv, &port, &host, &is_IPv4, &is_IPv6, &is_automatic, &wanted_place, &port_declared);

    struct sockaddr_in server_address = get_server_address(host, port, is_IPv4, is_IPv6);

    int server_fd = connect_to_server(&server_address);

    if(send_IAM(server_fd, wanted_place)<0)
        return -1;
    
    int deal_type = 0;
    int current_points = 0;
    int current_round = 0;

    hand *current_hand = malloc(sizeof(hand));

    int busy_or_deal_result = receive_BUSY_or_DEAL(server_fd, current_hand, is_automatic, &deal_type);
    if(busy_or_deal_result<0)
    {
        return busy_or_deal_result;
    }

    trick *current_trick = malloc(sizeof(trick));

    bool is_taken = true;
    while(is_taken)
    {
        int taken_or_trick_result = receive_TAKEN_or_TRICK(server_fd, current_hand, current_trick, &current_points, &current_round, is_automatic, deal_type, &is_taken, wanted_place);
        if(taken_or_trick_result<0)
        {
            return taken_or_trick_result;
        }
    }
    respond_to_TRICK(server_fd, current_hand, is_automatic, &current_points, &current_round, deal_type);
    
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

int connect_to_server(struct sockaddr_in *server_address)
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

int send_IAM(int server_fd, char wanted_place)
{
    char *buffer=malloc(6*sizeof(char));
    if(buffer==NULL)
        syserr("Syserr in malloc");
    
    snprintf(buffer, 6, "IAM%c\r\n", wanted_place);
    int result = writen_data_packet(server_fd, buffer, 6);
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
    if(readn_message(server_fd, buffer, 40)==-1)
        return -1;
    
    if(buffer[0] == 'B' && buffer[1] == 'U' && buffer[2] == 'S' && buffer[3] == 'Y')
    {
        if(!is_automatic)
        {
            // TODO: check if the contents of the busy message are as they should be even if the client is not automatic
            if(print_out_busy_message(buffer) == -1)
                return -1;
        }
        return -2;
    }
    else if(buffer[0] == 'D' && buffer[1] == 'E' && buffer[2] == 'A' && buffer[3] == 'L')
    {
        if(buffer[4]>='1' && buffer[4]<='7') {
            *deal_type = buffer[4]-'0';
        }
        else {
            error("Wrong contents of DEAL message. Message content : %s", buffer);
            return -1;
        }

        if(!(buffer[5] == 'N' || buffer[5] == 'E' || buffer[5] == 'S' || buffer[5] == 'W')){
            error("Wrong contents of DEAL message. Message content : %s", buffer);
            return -1;
        }

        if(load_starting_hand(buffer, current_hand)==-1)
            return -1;
    }
    else
    {
        error("Unrecognised type of message, expected BUSY or DEAL. Message content : %s", buffer);
        return -1;
    }
    return 0;
}

int print_out_busy_message(char *buffer)
{
    char* busy_list= malloc(sizeof(char)*10);
    int busy_list_current_place = 0;
    int buffer_place = 4;

    bool has_north = false;
    bool has_west = false;
    bool has_south = false;
    bool has_east = false;

    while(buffer[buffer_place]!='\n' && buffer[buffer_place]!='\r')
    {
        char this_place = '0';
        if((buffer[buffer_place]=='N' && !has_north))
        {
            this_place = buffer[buffer_place];
            has_north = true;
        }
        else if(buffer[buffer_place]=='W' && !has_west)
        {
            this_place = buffer[buffer_place];
            has_west = true;
        }
        else if(buffer[buffer_place]=='S' && !has_south)
        {
            this_place = buffer[buffer_place];
            has_south = true;
        }
        else if(buffer[buffer_place]=='E' && !has_east)
        {
            this_place = buffer[buffer_place];
            has_east = true;
        }
        else
        {
            error("Wrong contents of the BUSY message. Message content : %s", buffer);
            return -1;
        }

        if(busy_list_current_place != 0)
        {
            busy_list[busy_list_current_place] = ',';
            busy_list[busy_list_current_place + 1] = ' ';
            busy_list[busy_list_current_place + 2] = this_place;
            busy_list_current_place = busy_list_current_place + 3;
        }
        else
        {
            busy_list[busy_list_current_place] = this_place;
            busy_list_current_place++;
        }
    }
    return 0;
}

int load_starting_hand(char *buffer, hand* current_hand)
{
    int currently_loading_card = 0;
    int buffer_place = 6;
    while(buffer[buffer_place]!='\n' && buffer[buffer_place]!='\r')
    {
        if(buffer[buffer_place]>='2' && buffer[buffer_place]<='9')
        {
            current_hand->cards[currently_loading_card].rank = buffer[buffer_place] - '0';
            buffer_place++;
        }
        else if(buffer[buffer_place] == '1' && buffer[buffer_place] == '0')
        {
            current_hand->cards[currently_loading_card].rank = 10;
            buffer_place = buffer_place + 2;
        }
        else if(buffer[buffer_place] == 'J')
        {
            current_hand->cards[currently_loading_card].rank = 11;
            buffer_place++;
        }
        else if(buffer[buffer_place] == 'Q')
        {
            current_hand->cards[currently_loading_card].rank = 12;
            buffer_place++;
        }
        else if(buffer[buffer_place] == 'K')
        {
            current_hand->cards[currently_loading_card].rank = 13;
            buffer_place++;
        }
        else if(buffer[buffer_place] == 'A')
        {
            current_hand->cards[currently_loading_card].rank = 14;
            buffer_place++;
        }
        else
        {
            error("Wrong contents of DEAL message. Message content : %s", buffer);
            return -1;
        }

        if(buffer[buffer_place] == 'C' || buffer[buffer_place] == 'D' || buffer[buffer_place] == 'H' || buffer[buffer_place] == 'S')
        {
            current_hand->cards[currently_loading_card].suit = buffer[buffer_place];
        }
        else 
        {
            error("Wrong contents of DEAL message. Message content : %s", buffer);
            return -1;
        }

        buffer_place++;
        currently_loading_card++;
    }
    return 0;
}

int receive_TAKEN_or_TRICK(int server_fd, hand *current_hand, trick *current_trick, int *current_points, int *current_round, bool is_automatic, int deal_type, bool *is_taken, char place_at_table)
{
    char *buffer = malloc(25*sizeof(char));
    if(readn_message(server_fd, buffer, 25)==-1)
        return -1;

    if(buffer[0] == 'T' && buffer[1] == 'A' && buffer[2] == 'K' && buffer[3] == 'E' && buffer[4] == 'N')
    {
        *is_taken = true;
        int result = load_contents_of_TAKEN(buffer, current_hand, current_points, current_round, is_automatic, deal_type, place_at_table);
        free(buffer);
        return result;
    }
    else if(buffer[0] == 'T' && buffer[1] == 'R' && buffer[2] == 'I' && buffer[3] == 'C' && buffer[4] == 'K')
    {
        *is_taken = false;
        int result = load_contents_of_TRICK(buffer, current_trick, is_automatic);
        free(buffer);
        return result;
    }
    else
    {
        error("Unrecognised type of message, expected TAKEN or TRICK. Message content : %s", buffer);
        return -1;
    }
}

int receive_TAKEN(int server_fd, hand *current_hand, int *current_points, int *current_round, bool is_automatic, int deal_type, char place_at_table)
{
    char *buffer = malloc(25*sizeof(char));
    if(readn_message(server_fd, buffer, 25)==-1)
        return -1;

    if(buffer[0] == 'T' && buffer[1] == 'A' && buffer[2] == 'K' && buffer[3] == 'E' && buffer[4] == 'N')
    {
        *is_taken = true;
        return load_contents_of_TAKEN(buffer, current_hand, current_points, current_round, is_automatic, deal_type, place_at_table);
    }
    else
    {
        error("Unrecognised type of message, expected TAKEN. Message content : %s", buffer);
        return -1;
    }
}

int receive_TRICK(int server_fd, hand *current_hand, trick *current_trick, int *current_points, int *current_round, bool is_automatic, int deal_type, bool *is_taken, char place_at_table)
{
    char *buffer = malloc(25*sizeof(char));
    if(readn_message(server_fd, buffer, 25)==-1)
        return -1;

    if(buffer[0] == 'T' && buffer[1] == 'R' && buffer[2] == 'I' && buffer[3] == 'C' && buffer[4] == 'K')
    {
        *is_taken = false;
        int result = load_contents_of_TRICK(buffer, current_trick, is_automatic);
        free(buffer);
        return result;
    }
    else
    {
        error("Unrecognised type of message, expected TRICK. Message content : %s", buffer);
        return -1;
    }
}

int load_contents_of_TAKEN(char *buffer, hand *current_hand, int *current_points, int *current_round, bool is_automatic, int deal_type, char place_at_table)
{

}

int load_contents_of_TRICK(char *buffer, trick *current_trick, bool is_automatic)
{

}