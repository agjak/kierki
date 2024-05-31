#include <stdint.h>

uint16_t read_port(char const *string);
int read_timeout(char const *string);
void get_server_address(char const *host, uint16_t port, bool *is_IPv4, bool *is_IPv6, struct sockaddr_in *server_address_ipv4, struct sockaddr_in6 *server_address_ipv6);
void install_signal_handler(int signal, void (*handler)(int), int flags);

ssize_t readn(int fd, void *vptr, size_t n);
ssize_t writen(int fd, const void *vptr, size_t n);
int writen_data_packet(int client_fd, void* to_write, size_t size);

typedef struct {
    int rank;
    char suit;
} card;

typedef struct {
    card cards[13];
    card played_cards[13][4];
} hand;

typedef struct {
    card cards[4];
    int amount;
} trick;

void take_card_out_of_hand(hand *client_hand, card* card_to_take_out);
int cards_amount(hand *client_hand);
void print_out_card(card card_to_print);
void print_out_cards_on_hand(hand *hand_to_print);
void print_out_cards_played(hand *hand_to_print);


void write_out_raport(char *message, char *sender_adress_and_port, char *receiver_address_and_port);