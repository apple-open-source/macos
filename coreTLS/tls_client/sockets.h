//
//  sockets.h
//  coretls_tools
//

#ifndef __SOCKETS_H__
#define __SOCKETS_H__

#include <stdbool.h>

int SocketConnect(const char *hostname, const char *service, bool udp);
int SocketBind(int port, bool udp);

#endif /* __SOCKETS_H__ */
