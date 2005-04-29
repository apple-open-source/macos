#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "pmap_wakeup.h"

void pmap_wakeup(void)
{
	struct sockaddr_un sun;
	int fd;
	char b;

	memset(&sun, 0, sizeof(sun));

	sun.sun_family = AF_UNIX;
	strcpy(sun.sun_path, "/var/run/portmap.socket");

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1)
		return;

	if (connect(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		close(fd);
		return;
	}

	read(fd, &b, sizeof(b));
	close(fd);
}
