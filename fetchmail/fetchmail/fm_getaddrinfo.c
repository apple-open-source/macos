#include "config.h"
#include "fetchmail.h"
#include "i18n.h"

#include <signal.h>
#include <errno.h>
#include <string.h>

/** This is a getaddrinfo() replacement that blocks SIGALRM,
 * to avoid issues with non-reentrant getaddrinfo() implementations
 * after SIGALRM timeouts, for instance on MacOS X or NetBSD. */
int fm_getaddrinfo(const char *node, const char *serv, const struct addrinfo *hints, struct addrinfo **res)
{
    int rc;

#ifndef GETADDRINFO_ASYNCSAFE
    sigset_t ss, os;

    sigemptyset(&ss);
    sigaddset(&ss, SIGALRM);

    if (sigprocmask(SIG_BLOCK, &ss, &os))
	report(stderr, GT_("Cannot modify signal mask: %s"), strerror(errno));
#endif

    rc = getaddrinfo(node, serv, hints, res);

#ifndef GETADDRINFO_ASYNCSAFE
    if (sigprocmask(SIG_SETMASK, &os, NULL))
	report(stderr, GT_("Cannot modify signal mask: %s"), strerror(errno));
#endif

    return rc;
}

/** this is a debugging freeaddrinfo() wrapper. */
void fm_freeaddrinfo(struct addrinfo *ai)
{
    freeaddrinfo(ai);
}
