/** \file tls.c - collect common TLS functionality 
 * \author Matthias Andree
 * \date 2006
 */

#include "fetchmail.h"

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

/** return true if user allowed TLS */
int maybe_tls(struct query *ctl) {
#ifdef SSL_ENABLE
         /* opportunistic  or forced TLS */
    return (!ctl->sslproto || !strcasecmp(ctl->sslproto,"tls1"))
	&& !ctl->use_ssl;
#else
    (void)ctl;
    return 0;
#endif
}

/** return true if user requires TLS, note though that this code must
 * always use a logical AND with maybe_tls(). */
int must_tls(struct query *ctl) {
#ifdef SSL_ENABLE
    return maybe_tls(ctl)
	&& (ctl->sslfingerprint || ctl->sslcertck
		|| (ctl->sslproto && !strcasecmp(ctl->sslproto, "tls1")));
#else
    (void)ctl;
    return 0;
#endif
}
