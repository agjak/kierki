#include <stdint.h>

uint16_t read_port(char const *string);
int read_timeout(char const *string);
struct sockaddr_in get_server_address(char const *host, uint16_t port, bool is_IPv4, bool is_IPv6);