#include <stdio.h>
#include <stddef.h>        // for offsetof
#include <sys/socket.h>
#include <netinet/in.h>    // sockaddr_in, sockaddr_in6
#include <string.h>        // just in case

int main() {
    printf("=== struct layout & sizes ===\n\n");

    printf("struct sockaddr:\n");
    printf("  size: %zu\n", sizeof(struct sockaddr));
    printf("  sa_family offset: %zu\n", offsetof(struct sockaddr, sa_family));
    printf("  sa_data offset:   %zu\n", offsetof(struct sockaddr, sa_data));
    puts("");

    printf("struct sockaddr_in (IPv4):\n");
    printf("  size: %zu\n", sizeof(struct sockaddr_in));
    printf("  sin_family offset: %zu\n", offsetof(struct sockaddr_in, sin_family));
    printf("  sin_port offset:   %zu\n", offsetof(struct sockaddr_in, sin_port));
    printf("  sin_addr offset:   %zu\n", offsetof(struct sockaddr_in, sin_addr));
    puts("");

    printf("struct sockaddr_in6 (IPv6):\n");
    printf("  size: %zu\n", sizeof(struct sockaddr_in6));
    printf("  sin6_family offset:   %zu\n", offsetof(struct sockaddr_in6, sin6_family));
    printf("  sin6_port offset:     %zu\n", offsetof(struct sockaddr_in6, sin6_port));
    printf("  sin6_addr offset:     %zu\n", offsetof(struct sockaddr_in6, sin6_addr));
    puts("");

    printf("struct sockaddr_storage:\n");
    printf("  size: %zu\n", sizeof(struct sockaddr_storage));
    // The first field is usually the family; implementation may name it differently,
    // but it's guaranteed to be at the same offset as sa_family in sockaddr for compatibility.
    printf("  assumed family offset (should match sockaddr): 0\n");
    puts("");

    printf("Note: casting sockaddr_storage* to sockaddr* works because the initial bytes\n"
           "contain the address family at the same position. The storage is oversized to\n"
           "safely hold IPv4, IPv6, etc.\n");
    return 0;
}
