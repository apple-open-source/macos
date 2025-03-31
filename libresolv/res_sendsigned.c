/*	$NetBSD: res_sendsigned.c,v 1.1 2012/11/15 18:48:49 christos Exp $	*/
#include <sys/cdefs.h>
__RCSID("$NetBSD: res_sendsigned.c,v 1.1 2012/11/15 18:48:49 christos Exp $");

#include "port_before.h"
#ifndef __APPLE__
#include "fd_setsize.h"
#endif

#include <sys/types.h>
#include <sys/param.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#ifdef __APPLE__
#include <arpa/nameser_compat.h>
#endif
#include <arpa/inet.h>

#ifndef __APPLE__
#include <isc/dst.h>
#else
#include "dst.h"
#endif

#include <errno.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "port_after.h"

#include "res_debug.h"


/*% res_nsendsigned */
int
res_nsendsigned(res_state statp, const u_char *msg, int msglen,
		ns_tsig_key *key, u_char *answer, int anslen)
{
	res_state nstatp;
	DST_KEY *dstkey;
	int usingTCP = 0;
	u_char *newmsg;
	int newmsglen, bufsize, siglen;
	u_char sig[64];
	HEADER *hp;
	time_t tsig_time;
	int ret;
	int len;

	dst_init();

	nstatp = (res_state) malloc(sizeof(*statp));
	if (nstatp == NULL) {
		errno = ENOMEM;
		return (-1);
	}
	memcpy(nstatp, statp, sizeof(*statp));

	bufsize = msglen + 1024;
	newmsg = (u_char *) malloc(bufsize);
	if (newmsg == NULL) {
		free(nstatp);
		errno = ENOMEM;
		return (-1);
	}
	memcpy(newmsg, msg, msglen);
	newmsglen = msglen;

	if (ns_samename(key->alg, NS_TSIG_ALG_HMAC_MD5) != 1)
		dstkey = NULL;
	else
		dstkey = dst_buffer_to_key(key->name, KEY_HMAC_MD5,
					   NS_KEY_TYPE_AUTH_ONLY,
					   NS_KEY_PROT_ANY,
					   key->data, key->len);
	if (dstkey == NULL) {
		errno = EINVAL;
		free(nstatp);
		free(newmsg);
		return (-1);
	}

	nstatp->nscount = 1;
	siglen = sizeof(sig);
	ret = ns_sign(newmsg, &newmsglen, bufsize, NOERROR, dstkey, NULL, 0,
		      sig, &siglen, 0);
	if (ret < 0) {
		free (nstatp);
		free (newmsg);
		dst_free_key(dstkey);
		if (ret == NS_TSIG_ERROR_NO_SPACE)
			errno  = EMSGSIZE;
		else if (ret == -1)
			errno  = EINVAL;
		return (ret);
	}

	if (newmsglen > PACKETSZ || nstatp->options & RES_USEVC)
		usingTCP = 1;
	if (usingTCP == 0)
		nstatp->options |= RES_IGNTC;
	else
		nstatp->options |= RES_USEVC;
	/*
	 * Stop res_send printing the answer.
	 */
	nstatp->options &= ~RES_DEBUG;
	nstatp->pfcode &= ~RES_PRF_REPLY;

retry:

	len = res_nsend(nstatp, newmsg, newmsglen, answer, anslen);
	if (len < 0) {
		free (nstatp);
		free (newmsg);
		dst_free_key(dstkey);
		return (len);
	}

	ret = ns_verify(answer, &len, dstkey, sig, siglen,
	    NULL, NULL, &tsig_time, (nstatp->options & RES_KEEPTSIG) != 0);
	if (ret != 0) {
		DprintC((statp->options & RES_DEBUG) ||
			((statp->pfcode & RES_PRF_REPLY) &&
			 (statp->pfcode & RES_PRF_HEAD1)),
			";; got answer:");

		DprintQ((statp->options & RES_DEBUG) ||
			(statp->pfcode & RES_PRF_REPLY),
			"",
			answer, (anslen > len) ? len : anslen);

		if (ret > 0) {
			DprintC(statp->pfcode & RES_PRF_REPLY,
			       ";; server rejected TSIG (%s)",
				p_rcode(ret));
		} else {
			DprintC(statp->pfcode & RES_PRF_REPLY,
			       ";; TSIG invalid (%s)",
				p_rcode(-ret));
		}

		free (nstatp);
		free (newmsg);
		dst_free_key(dstkey);
		if (ret == -1)
			errno = EINVAL;
		else
			errno = ENOTTY;
		return (-1);
	}

	hp = (HEADER *)(void *)answer;
	if (hp->tc && !usingTCP && (statp->options & RES_IGNTC) == 0U) {
		nstatp->options &= ~RES_IGNTC;
		usingTCP = 1;
		goto retry;
	}
	DprintC((statp->options & RES_DEBUG) ||
		((statp->pfcode & RES_PRF_REPLY) &&
		 (statp->pfcode & RES_PRF_HEAD1)),
		";; got answer:\n");

	DprintQ((statp->options & RES_DEBUG) ||
		(statp->pfcode & RES_PRF_REPLY),
		"",
		answer, (anslen > len) ? len : anslen);

	DprintC(statp->pfcode & RES_PRF_REPLY,
		";; TSIG ok\n");

	free (nstatp);
	free (newmsg);
	dst_free_key(dstkey);
	return (len);
}

/*! \file */
