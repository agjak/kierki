#include <stdint.h>

uint16_t read_port(char const *string);
int read_timeout(char const *string);
struct sockaddr_in get_server_address(char const *host, uint16_t port, bool is_IPv4, bool is_IPv6);

ssize_t readn(int fd, void *vptr, size_t n);
ssize_t writen(int fd, const void *vptr, size_t n);
int writen_data_packet(int client_fd, void* to_write, size_t size);
int readn_data_packet(int client_fd, void* result, size_t size);
int readn_message(int client_fd, void* result, size_t max_size);

typedef struct {
    int rank;
    char suit;
} card;

typedef struct {
    card cards[13];
} hand;