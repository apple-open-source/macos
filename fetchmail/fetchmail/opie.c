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
#include "md5.h"

#if OPIE_ENABLE
#include <opie.h>

int do_otp(int sock, char *command, struct query *ctl)
{
    int i, rval;
    int result;
    char buffer[128];
    char challenge[OPIE_CHALLENGE_MAX+1];
    char response[OPIE_RESPONSE_MAX+1];

    gen_send(sock, "%s X-OTP");

    if (rval = gen_recv(sock, buffer, sizeof(buffer)))
	return rval;

    if ((i = from64tobits(challenge, buffer)) < 0) {
	report(stderr, _("Could not decode initial BASE64 challenge\n"));
	return PS_AUTHFAIL;
    };

    to64frombits(buffer, ctl->remotename, strlen(ctl->remotename));
    if (rval = gen_transact(sock, buffer, sizeof(buffer)))
	return rval;

    if ((i = from64tobits(challenge, buffer)) < 0) {
	report(stderr, _("Could not decode OTP challenge\n"));
	return PS_AUTHFAIL;
    };

    rval = opiegenerator(challenge, !strcmp(ctl->password, "opie") ? "" : ctl->password, response);
    if ((rval == -2) && !run.poll_interval) {
	char secret[OPIE_SECRET_MAX+1];
	fprintf(stderr, _("Secret pass phrase: "));
	if (opiereadpass(secret, sizeof(secret), 0))
	    rval = opiegenerator(challenge, secret, response);
	memset(secret, 0, sizeof(secret));
    };

    if (rval)
	return(PS_AUTHFAIL);

    to64frombits(buffer, response, strlen(response));
    suppress_tags = TRUE;
    result = gen_transact(sock, buffer, strlen(buffer));
    suppress_tags = FALSE;

    if (result)
	return PS_SUCCESS;
    else
	return PS_AUTHFAIL;
};
#endif /* OPIE_ENABLE */

/* opie.c ends here */
