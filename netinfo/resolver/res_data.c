/*
 * Copyright (c) 1995-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$Id: res_data.c,v 1.3 2003/02/25 19:03:07 majka Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <ctype.h>
#include <netdb.h>
#include <resolv.h>
#include <res_update.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "res_private.h"

const char *__res_opcodes[] = {
	"QUERY",
	"IQUERY",
	"CQUERYM",
	"CQUERYU",	/* experimental */
	"NOTIFY",	/* experimental */
	"UPDATE",
	"6",
	"7",
	"8",
	"9",
	"10",
	"11",
	"12",
	"13",
	"ZONEINIT",
	"ZONEREF",
};

void
__h_errno_set(struct __res_state *res, int err)
{
	h_errno = res->res_h_errno = err;
}

void
res_client_close(res_state res)
{
	if (res == NULL) return;

	if (res->_u._ext.ext != NULL) free(res->_u._ext.ext);
	free(res);
}

/*
 * Open a resolver client, reading configuration from the
 * given file rather than the default (/etc/resolv.conf).
 */
res_state
res_client_open(char *path)
{
	extern int res_vinit_from_file(res_state, int, char *);
	res_state x;
	int status;

	x = (res_state)calloc(1, sizeof(struct __res_state));
	if (x == NULL) return NULL;

	x->retrans = RES_TIMEOUT;
	x->retry = 4;
	x->options = RES_DEFAULT;
	x->id = res_randomid();

	status = res_vinit_from_file(x, 1, path);
	if (status != 0)
	{
		res_client_close(x);
		return NULL;
	}

	return x;
}

int
res_init(void)
{
	extern int __res_vinit(res_state, int);

	if (!_res.retrans) _res.retrans = RES_TIMEOUT;
	if (!_res.retry) _res.retry = 4;
	if (!(_res.options & RES_INIT)) _res.options = RES_DEFAULT;
	if (!_res.id) _res.id = res_randomid();

	return (__res_vinit(&_res, 1));
}

int
res_query(const char *name, int class, int type, u_char *answer, int anslen)	
{
	if (((_res.options & RES_INIT) == 0) && (res_init() == -1))
	{
		RES_SET_H_ERRNO(&_res, NETDB_INTERNAL);
		return -1;
	}
	return (res_nquery(&_res, name, class, type, answer, anslen));
}

void
fp_nquery(const u_char *msg, int len, FILE *file)
{
	if (((_res.options & RES_INIT) == 0) && (res_init() == -1)) return;

	res_pquery(&_res, msg, len, file);
}

void
fp_query(const u_char *msg, FILE *file)
{
	fp_nquery(msg, NS_PACKETSZ, file);
}

void
p_query(const u_char *msg)
{
	fp_query(msg, stdout);
}

const char *
hostalias(const char *name)
{
	static char abuf[NS_MAXDNAME];

	return (res_hostalias(&_res, name, abuf, sizeof abuf));
}

void
res_close(void)
{
	res_nclose(&_res);
}

int
res_isourserver(const struct sockaddr_in *inp)
{
	return (res_ourserver_p(&_res, (const struct sockaddr *)inp));
}

int
res_nisourserver(const res_state res, const struct sockaddr_in *inp)
{
	return (res_ourserver_p(res, (const struct sockaddr *)inp));
}

int
res_mkquery(int op, const char *dname, int class, int type, const u_char *data, int datalen, const u_char *newrr_in, u_char *buf, int buflen)
{
	if (((_res.options & RES_INIT) == 0) && (res_init() == -1))
	{
		RES_SET_H_ERRNO(&_res, NETDB_INTERNAL);
		return -1;
	}

	return res_nmkquery(&_res, op, dname, class, type, data, datalen, newrr_in, buf, buflen);
}

int
res_querydomain(const char *name, const char *domain, int class, int type, u_char *answer, int anslen)
{
	if (((_res.options & RES_INIT) == 0) && (res_init() == -1))
	{
		RES_SET_H_ERRNO(&_res, NETDB_INTERNAL);
		return -1;
	}

	return res_nquerydomain(&_res, name, domain, class, type, answer, anslen);
}

int
res_search(const char *name, int class, int type, u_char *answer, int anslen)
{
	if (((_res.options & RES_INIT) == 0) && (res_init() == -1))
	{
		RES_SET_H_ERRNO(&_res, NETDB_INTERNAL);
		return -1;
	}

	return res_nsearch(&_res, name, class, type, answer, anslen);
}

int
res_send(const u_char *buf, int buflen, u_char *ans, int anssiz)
{
	if (((_res.options & RES_INIT) == 0) && (res_init() == -1))
	{
		/* errno should have been set by res_init() in this case. */
		return -1;
	}

	return res_nsend(&_res, buf, buflen, ans, anssiz);
}

int
res_sendsigned(const u_char *buf, int buflen, ns_tsig_key *key, u_char *ans, int anssiz)
{
	if (((_res.options & RES_INIT) == 0) && (res_init() == -1))
	{
		/* errno should have been set by res_init() in this case. */
		return -1;
	}

	return res_nsendsigned(&_res, buf, buflen, key, ans, anssiz);
}
