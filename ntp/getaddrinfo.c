#include <sys/socket.h>
#include <netdb.h>

#include <syslog.h>

static
int
my_getaddrinfo(const char* nodename,
	       const char *servname,
	       const struct addrinfo *hints,
	       struct addrinfo **res)
{
	int rc = getaddrinfo(nodename, servname, hints, res);
	if (rc == EAI_NONAME) {
		syslog(LOG_INFO, "getaddrinfo('%s','%s') returned EAI_NONAME, mapping to EAI_AGAIN", nodename, servname);
		return EAI_AGAIN;
	}
	return rc;
}
#define getaddrinfo my_getaddrinfo

