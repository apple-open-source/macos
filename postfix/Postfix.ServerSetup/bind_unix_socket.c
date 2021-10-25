#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
// Utility that does the equivalent of python bind for unix domain socket
int main(int argc, char **argv) {
    char * s_path = argv[1];
    if (!s_path) return(1);
    struct sockaddr_un s;
    int fd;
    s.sun_family = AF_UNIX;
    strncpy(s.sun_path, (char *)s_path, sizeof(s.sun_path));
    fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    bind(fd, (struct sockaddr *) &s, sizeof(struct sockaddr_un));
    close(fd);
    return 0;
}

