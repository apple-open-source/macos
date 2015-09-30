//
//  sockets.c
//  coretls_tools
//

#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>


#include "sockets.h"

/* Connect to a hostname:port */
int SocketConnect(const char *hostname, const char *service, bool udp)
{
    int err;
    int sock;
    struct addrinfo hints, *res, *res0;
    const char *cause = NULL;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = udp?PF_INET:PF_UNSPEC; // For udp, hint to ipv4
    hints.ai_socktype = udp?SOCK_DGRAM:SOCK_STREAM;
    err = getaddrinfo(hostname, service, &hints, &res0);
    if (err) {
        fprintf(stderr, "%s", gai_strerror(err));
        return err;
    }
    sock = -1;
    for (res = res0; res; res = res->ai_next) {
        sock = socket(res->ai_family, res->ai_socktype,
                   res->ai_protocol);
        if (sock < 0) {
            cause = "socket";
            continue;
        }

        if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
            cause = "connect";
            close(sock);
            sock = -1;
            continue;
        }

        break;  /* okay we got one */
    }
    if (sock < 0) {
        fprintf(stderr, "%s", cause);
    }
    freeaddrinfo(res0);

    return sock;
}

int SocketBind(int port, bool udp)
{
    struct sockaddr_in  sa;
    int					sock;
    int                 val  = 1;

    if ((sock=socket(AF_INET, udp?SOCK_DGRAM:SOCK_STREAM, 0))==-1) {
        perror("socket");
        return -errno;
    }

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&val, sizeof(val));


    memset((char *) &sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind (sock, (struct sockaddr *)&sa, sizeof(sa))==-1)
    {
        perror("bind");
        return -errno;
    }


    return sock;
}

