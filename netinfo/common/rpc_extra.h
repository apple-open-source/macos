#include <sys/types.h>
#include <sys/socket.h>
#include <rpc/rpc.h>

SVCXPRT *svctcp_bind(int sock, struct sockaddr_in s, u_int sendsize, u_int recvsize);
SVCXPRT *svcudp_bind(int sock, struct sockaddr_in s);

