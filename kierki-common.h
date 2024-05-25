#include <stdint.h>

uint16_t read_port(char const *string);
int read_timeout(char const *string);
struct sockaddr_in get_server_address(char const *host, uint16_t port, bool is_IPv4, bool is_IPv6);

ssize_t readn(int fd, void *vptr, size_t n);
ssize_t writen(int fd, const void *vptr, size_t n);
int writen_data_packet(int client_fd, void* to_write, size_t size);
int readn_data_packet(int client_fd, void* result, size_t size);
int readn_message(int client_fd, char* result, size_t max_size, bool is_automatic, char *server_address_and_port, char *client_address_and_port);

typedef struct {
    int rank;
    char suit;
} card;

typedef struct {
    card cards[13];
} hand;

typedef struct {
    card cards[3];
    int amount;
} trick;

void take_card_out_of_hand(hand *client_hand, card* card_to_take_out);
int cards_amount(hand *client_hand);
void print_out_card(card card_to_print);
void print_out_hand(hand *hand_to_print);

void write_out_raport(char *message, int message_length, char *sender_adress_and_port, char *receiver_address_and_port);