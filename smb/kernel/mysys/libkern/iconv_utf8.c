/*
 * Copyright (c) 2001 Apple Computer
 * All rights reserved.
 *
 */
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/smb_apple.h>
#include <sys/utfconv.h>
#include <sys/iconv.h>

#include "iconv_converter_if.h"

/*
 * UTF-8 converter
 */

#ifdef MODULE_DEPEND
MODULE_DEPEND(iconv_utf8, libiconv, 1, 1, 1);
#endif

/*
 * UTF-8 converter instance
 */
struct iconv_utf8 {
	KOBJ_FIELDS;
	int d_type;
	int d_flags;
	struct iconv_cspair *d_csp;
};

enum {
	UTF8_ENCODE = 1,
	UTF8_DECODE = 2
};

static int
iconv_utf8_open(struct iconv_converter_class *dcp,
	struct iconv_cspair *csp, struct iconv_cspair *cspf, void **dpp)
{
	#pragma unused(cspf)
	struct iconv_utf8 *dp;

	dp = (struct iconv_utf8 *)kobj_create((struct kobj_class*)dcp, M_ICONV, M_WAITOK);
	if (strcmp(csp->cp_to, "utf-8") == 0) {
		dp->d_type = UTF8_ENCODE;
		dp->d_flags = UTF_DECOMPOSED | UTF_NO_NULL_TERM;
	} else if (strcmp(csp->cp_from, "utf-8") == 0) {
		dp->d_type = UTF8_DECODE;
		dp->d_flags = UTF_PRECOMPOSED;
	}
	/* Little endian Unicode over the wire */
	if (BYTE_ORDER != LITTLE_ENDIAN)
		dp->d_flags |= UTF_REVERSE_ENDIAN;
	dp->d_csp = csp;
	csp->cp_refcount++;
	*dpp = (void*)dp;
	return 0;
}

static int
iconv_utf8_close(void *data)
{
	struct iconv_utf8 *dp = data;

	dp->d_csp->cp_refcount--;
	kobj_delete((struct kobj*)data, M_ICONV);
	return 0;
}

static int
iconv_utf8_conv(void *d2p, const char **inbuf,
	size_t *inbytesleft, char **outbuf, size_t *outbytesleft)
{
	struct iconv_utf8 *dp = (struct iconv_utf8*)d2p;
	size_t inlen;
	size_t outlen;
	int error;

	if (inbuf == NULL || *inbuf == NULL || outbuf == NULL || *outbuf == NULL)
		return 0;

	inlen = *inbytesleft;
	outlen = 0;
	
	if (dp->d_type == UTF8_ENCODE)
		error = utf8_encodestr((u_int16_t *)*inbuf, inlen, *outbuf, &outlen, *outbytesleft, 0, dp->d_flags);
	else if (dp->d_type == UTF8_DECODE)
		error = utf8_decodestr(*inbuf, inlen, (u_int16_t *)*outbuf, &outlen, *outbytesleft, 0, dp->d_flags);
	else
		return (-1);

	*inbuf += inlen;
	*outbuf += outlen;
	*inbytesleft -= inlen;
	*outbytesleft -= outlen;
	return 0;
}

static const char *
iconv_utf8_name(struct iconv_converter_class *dcp)
{
	#pragma unused(dcp)
	return "utf8";
}

static kobj_method_t iconv_utf8_methods[] = {
	KOBJMETHOD(iconv_converter_open,	iconv_utf8_open),
	KOBJMETHOD(iconv_converter_close,	iconv_utf8_close),
	KOBJMETHOD(iconv_converter_conv,	iconv_utf8_conv),
#if 0
	KOBJMETHOD(iconv_converter_init,	iconv_utf8_init),
	KOBJMETHOD(iconv_converter_done,	iconv_utf8_done),
#endif
	KOBJMETHOD(iconv_converter_name,	iconv_utf8_name),
	{0, 0}
};

KICONV_CONVERTER(utf8, sizeof(struct iconv_utf8));
