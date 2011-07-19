/*
 * opie.c -- One-Time Password authentication.
 *
 * For license terms, see the file COPYING in this directory.
 */

#include  "config.h"
#include  <stdio.h>
#include  <string.h>
#include  <ctype.h>
#if defined(STDC_HEADERS)
#include  <stdlib.h>
#endif
#include  "fetchmail.h"
#include  "socket.h"

#include  "i18n.h"
#include "fm_md5.h"

#ifdef OPIE_ENABLE
#ifdef __cplusplus
extern "C" {
#endif
#include <opie.h>
#ifdef __cplusplus
}
#endif

int do_otp(int sock, const char *command, struct query *ctl)
{
    int i, rval;
    char buffer[128];
    char challenge[OPIE_CHALLENGE_MAX+1+8];
    char response[OPIE_RESPONSE_MAX+1];

    gen_send(sock, "%s X-OTP", command);

    if ((rval = gen_recv(sock, buffer, sizeof(buffer))))
	return rval;

	if (strncmp(buffer, "+", 1)) {
	report(stderr, GT_("server recv fatal\n"));
	return PS_AUTHFAIL;
	}

    to64frombits(buffer, ctl->remotename, strlen(ctl->remotename));
	suppress_tags = TRUE;
    gen_send(sock, buffer, sizeof(buffer));
	suppress_tags = FALSE;

    if ((rval = gen_recv(sock, buffer, sizeof(buffer))))
	return rval;

	memset(challenge, '\0', sizeof(challenge));
    if ((i = from64tobits(challenge, buffer+2, sizeof(challenge))) < 0) {
	report(stderr, GT_("Could not decode OTP challenge\n"));
	return PS_AUTHFAIL;
    };

	memset(response, '\0', sizeof(response));
    rval = opiegenerator(challenge, ctl->password, response);
    if ((rval == -2) && !run.poll_interval) {
	char secret[OPIE_SECRET_MAX+1];
	fprintf(stderr, GT_("Secret pass phrase: "));
	if (opiereadpass(secret, sizeof(secret), 0))
	    rval = opiegenerator(challenge, secret, response);
	memset(secret, 0, sizeof(secret));
    };

    if (rval)
	return(PS_AUTHFAIL);

    to64frombits(buffer, response, strlen(response));
    suppress_tags = TRUE;
    gen_send(sock, buffer, strlen(buffer));
    suppress_tags = FALSE;

    if ((rval = gen_recv(sock, buffer, sizeof(buffer))))
	return rval;

    return PS_SUCCESS;
}
#endif /* OPIE_ENABLE */

/* opie.c ends here */
