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
int receive_TAKEN_or_TRICK_or_SCORE(int server_fd, hand *current_hand, trick *current_trick, int *current_points, int current_round, bool is_automatic, int deal_type, bool *is_taken, bool *is_score, char place_at_table);
int receive_TAKEN(int server_fd, hand *current_hand, int *current_points, int current_round, bool is_automatic, int deal_type, char place_at_table);
int receive_TRICK_or_SCORE(int server_fd, trick *current_trick, int current_round, bool is_automatic, int *current_points, bool *is_score);
int load_contents_of_TAKEN(char *buffer, hand *current_hand, int *current_points, int current_round, bool is_automatic, int deal_type, char place_at_table);
int load_round_number_TAKEN_or_TRICK(char *buffer, int current_round, int *buffer_place);
int load_card_list_TAKEN(char *buffer, hand *current_hand, int *buffer_place, card *cards_taken);
int load_client_taking_and_count_points_TAKEN(char *buffer, int current_round, int deal_type, int *current_points, char place_at_table, int *buffer_place, card *cards_taken);
int load_contents_of_TRICK(char *buffer, trick *current_trick, bool is_automatic, int current_round);
int load_card_from_this_buffer_place(char *buffer, int *buffer_place, card *loaded_card);
int respond_to_TRICK(int server_fd, hand *current_hand, bool is_automatic, int *current_points, int current_round, int deal_type, trick *current_trick);
int receive_TOTAL(int server_fd, bool is_automatic);
int receive_SCORE(int server_fd, bool is_automatic);
int load_contents_of_SCORE(char *buffer, bool is_automatic);
int load_contents_of_TOTAL(char *buffer, bool is_automatic);
void choose_a_card(card *chosen_card, hand *current_hand, bool is_automatic, int current_round, int deal_type, trick *current_trick);

char *server_address_and_port;
char *client_address_and_port;

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

    int deal_type, current_points, current_round;
    
    while(true)
    {
        new_hand_dealt:
        deal_type = 0;
        current_points = 0;
        current_round = 0;

        hand *current_hand = malloc(sizeof(hand));

        int busy_or_deal_result = receive_BUSY_or_DEAL(server_fd, current_hand, is_automatic, &deal_type);
        if(busy_or_deal_result<0)
        {
            free(current_hand);
            return busy_or_deal_result;
        }

        trick *current_trick = malloc(sizeof(trick));

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
                return taken_or_trick_or_score_result;
            }
            if(is_score)
            {
                free(current_hand);
                free(current_trick);
                int total_result = receive_TOTAL(server_fd, is_automatic);
                if(total_result<0)
                    return total_result;
                goto new_hand_dealt;
            }
        }
        respond_to_TRICK(server_fd, current_hand, is_automatic, &current_points, current_round, deal_type, current_trick);
        int taken_result = receive_TAKEN(server_fd, current_hand, &current_points, current_round, is_automatic, deal_type, wanted_place);
        if(taken_result<0)
        {
            free(current_hand);
            free(current_trick);
            return taken_result;
        }

        while(current_round<13)
        {
            current_round++;
            bool is_score = false;
            int trick_result = receive_TRICK_or_SCORE(server_fd, current_trick, current_round, is_automatic, &current_points, &is_score);
            if(trick_result<0)
            {
                free(current_hand);
                free(current_trick);
                return trick_result;
            }
            if(is_score)
            {
                free(current_hand);
                free(current_trick);
                int total_result = receive_TOTAL(server_fd, is_automatic);
                if(total_result<0)
                    return total_result;
                goto new_hand_dealt;
            }
            respond_to_TRICK(server_fd, current_hand, is_automatic, &current_points, current_round, deal_type, current_trick);
            int taken_result = receive_TAKEN(server_fd, current_hand, &current_points, current_round, is_automatic, deal_type, wanted_place);
            if(taken_result<0)
            {
                free(current_hand);
                free(current_trick);
                return taken_result;
            }
        }

        free(current_hand);
        free(current_trick);
        int score_result = receive_SCORE(server_fd, is_automatic);
        if(score_result<0)
            return score_result;

        int total_result = receive_TOTAL(server_fd, is_automatic);
        if(total_result<0)
            return total_result;
    }
    
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
    char *buffer=malloc(7*sizeof(char));
    if(buffer==NULL)
        syserr("Syserr in malloc");
    
    snprintf(buffer, 7, "IAM%c\r\n", wanted_place);
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
    if(readn_message(server_fd, buffer, 40, is_automatic, server_address_and_port, client_address_and_port)==-1)
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
        return -1;
    }

    if(readn_message(server_fd, buffer, 25, is_automatic, server_address_and_port, client_address_and_port)==-1)
    {
        free(buffer);
        return -1;
    }
        

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
        int result = load_contents_of_TRICK(buffer, current_trick, is_automatic, current_round);
        free(buffer);
        return result;
    }
    else if(buffer[0] == 'S' && buffer[1] == 'C' && buffer[2] == 'O' && buffer[3] == 'R' && buffer[4] == 'E')
    {
        *is_taken = false;
        *is_score = true;
        int result = load_contents_of_SCORE(buffer, is_automatic);
        free(buffer);
        return result;
    }
    else
    {
        error("Unrecognised type of message, expected TAKEN or TRICK. Message content : %s", buffer);
        free(buffer);
        return -1;
    }
}

int receive_TAKEN(int server_fd, hand *current_hand, int *current_points, int current_round, bool is_automatic, int deal_type, char place_at_table)
{
    char *buffer = malloc(25*sizeof(char));
    if(buffer == NULL)
    {
        syserr("Malloc error");
        return -1;
    }

    if(readn_message(server_fd, buffer, 25, is_automatic, server_address_and_port, client_address_and_port)==-1)
    {
        free(buffer);
        return -1;
    }

    if(buffer[0] == 'T' && buffer[1] == 'A' && buffer[2] == 'K' && buffer[3] == 'E' && buffer[4] == 'N')
    {
        int result = load_contents_of_TAKEN(buffer, current_hand, current_points, current_round, is_automatic, deal_type, place_at_table);
        free(buffer);
        return result;
    }
    else
    {
        error("Unrecognised type of message, expected TAKEN. Message content : %s", buffer);
        free(buffer);
        return -1;
    }
}

int receive_TRICK_or_SCORE(int server_fd, trick *current_trick, int current_round, bool is_automatic, int *current_points, bool *is_score)
{
    char *buffer = malloc(25*sizeof(char));
    if(buffer == NULL)
    {
        syserr("Malloc error");
        return -1;
    }
    
    if(readn_message(server_fd, buffer, 25, is_automatic, server_address_and_port, client_address_and_port)==-1)
    {
        free(buffer);
        return -1;
    }

    if(buffer[0] == 'T' && buffer[1] == 'R' && buffer[2] == 'I' && buffer[3] == 'C' && buffer[4] == 'K')
    {
        int result = load_contents_of_TRICK(buffer, current_trick, is_automatic, current_round);
        free(buffer);
        return result;
    }
    else if(buffer[0] == 'S' && buffer[1] == 'C' && buffer[2] == 'O' && buffer[3] == 'R' && buffer[4] == 'E')
    {
        *is_score = true;
        int result = load_contents_of_SCORE(buffer, is_automatic);
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

int load_contents_of_TAKEN(char *buffer, hand *current_hand, int *current_points, int current_round, bool is_automatic, int deal_type, char place_at_table)
{
    int buffer_place=5;
    int load_round_number_results = load_round_number_TAKEN_or_TRICK(buffer, current_round, &buffer_place);
    if(load_round_number_results < 0)
        return load_round_number_results;
    
    card *cards_taken = malloc(4*sizeof(card));
    int load_card_list_results = load_card_list_TAKEN(buffer, current_hand, &buffer_place, cards_taken);
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

int load_card_list_TAKEN(char *buffer, hand *current_hand, int *buffer_place, card *cards_taken)
{
    int current_card_amount = cards_amount(current_hand);
    for(int i=0; i<4; i++)
    {
        int load_card_results = load_card_from_this_buffer_place(buffer, buffer_place, &cards_taken[i]);
        if(load_card_results<0)
            return load_card_results;

        take_card_out_of_hand(current_hand, &cards_taken[i]);
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

int load_contents_of_TRICK(char *buffer, trick *current_trick, bool is_automatic, int current_round)
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
    else if(buffer[*buffer_place] == '1' && buffer[*buffer_place] == '0')
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

int respond_to_TRICK(int server_fd, hand *current_hand, bool is_automatic, int *current_points, int current_round, int deal_type, trick *current_trick)
{
    card *chosen_card = malloc(sizeof(card));
    choose_a_card(chosen_card, current_hand, is_automatic, current_round, deal_type, current_trick);

    take_card_out_of_hand(current_hand, chosen_card);

    int msg_size = 10;
    int rank_size = 1;
    if(current_round > 9)
        msg_size++;

    if(chosen_card->rank == 10)
    {
        msg_size++;
        rank_size++;
    }
        

    char *chosen_rank = malloc(rank_size * sizeof(char));
    if(chosen_card->rank >= 2 && chosen_card->rank <=9){
        chosen_rank[0] = '0' + chosen_card->rank;
    }
    if(chosen_card->rank == 10){
        chosen_rank[0] = '1';
        chosen_rank[1] = '0';
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

    char *message = malloc(msg_size * sizeof(char));

    snprintf(message, msg_size, "TRICK%d%s%c\r\n", current_round, chosen_rank, chosen_card->suit);
    int result = writen_data_packet(server_fd, message, msg_size);
    free(message);
    free(chosen_rank);
    free(chosen_card);
    return result;

}

int receive_TOTAL(int server_fd, bool is_automatic)
{
    char *buffer = malloc(25*sizeof(char));
    if(buffer == NULL)
    {
        syserr("Malloc error");
        return -1;
    }
    
    if(readn_message(server_fd, buffer, 25, is_automatic, server_address_and_port, client_address_and_port)==-1)
    {
        free(buffer);
        return -1;
    }

    if(buffer[0] == 'T' && buffer[1] == 'O' && buffer[2] == 'T' && buffer[3] == 'A' && buffer[4] == 'L')
    {
        int result = load_contents_of_TOTAL(buffer, is_automatic);
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

int receive_SCORE(int server_fd, bool is_automatic)
{
    char *buffer = malloc(25*sizeof(char));
    if(buffer == NULL)
    {
        syserr("Malloc error");
        return -1;
    }
    
    if(readn_message(server_fd, buffer, 25, is_automatic, server_address_and_port, client_address_and_port)==-1)
    {
        free(buffer);
        return -1;
    }

    if(buffer[0] == 'S' && buffer[1] == 'C' && buffer[2] == 'O' && buffer[3] == 'R' && buffer[4] == 'E')
    {
        int result = load_contents_of_SCORE(buffer, is_automatic);
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

int load_contents_of_TOTAL(char *buffer, bool is_automatic)
{
    if(!is_automatic)
    {
        printf("%s", buffer);
        printf("The total scores are:\n");
        int buffer_place = 5;
        while(buffer[buffer_place] != '\r' && buffer[buffer_place] != '\n')
        {
            printf("%c | ", buffer[buffer_place]);
            buffer_place++;
            while(buffer[buffer_place] >= '0' && buffer[buffer_place] <= '9')
            {
                printf("%c", buffer[buffer_place]);
            }
            printf("\n");
        }
        printf("\n");
    }
    return 0;
}

int load_contents_of_SCORE(char *buffer, bool is_automatic)
{
    if(!is_automatic)
    {
        printf("%s", buffer);
        printf("The scores are:\n");
        int buffer_place = 5;
        while(buffer[buffer_place] != '\r' && buffer[buffer_place] != '\n')
        {
            printf("%c | ", buffer[buffer_place]);
            buffer_place++;
            while(buffer[buffer_place] >= '0' && buffer[buffer_place] <= '9')
            {
                printf("%c", buffer[buffer_place]);
            }
            printf("\n");
        }
        printf("\n");
    }
    return 0;
}

void choose_a_card(card *chosen_card, hand *current_hand, bool is_automatic, int current_round, int deal_type, trick *current_trick)
{
    if(is_automatic)
    {
        chosen_card->rank = current_hand->cards[0].rank;
        chosen_card->suit = current_hand->cards[0].suit;
    }
}