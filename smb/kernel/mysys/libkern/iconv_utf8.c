/*
 * Copyright (c) 2001 - 2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/smb_apple.h>
#include <sys/utfconv.h>
#include <sys/smb_iconv.h>

#include "iconv_converter_if.h"

/*
 * UTF-8 converter
 */

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

	dp = (struct iconv_utf8 *)kobj_create((struct kobj_class*)dcp, M_ICONV);
	if (strncmp(csp->cp_to, "utf-8", ICONV_CSNMAXLEN) == 0) {
		dp->d_type = UTF8_ENCODE;
		dp->d_flags = UTF_DECOMPOSED | UTF_NO_NULL_TERM;
	} else if (strncmp(csp->cp_from, "utf-8", ICONV_CSNMAXLEN) == 0) {
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
iconv_utf8_conv(void *d2p, const char **inbuf, size_t *inbytesleft, char **outbuf, 
		size_t *outbytesleft, int flags)
{
	struct iconv_utf8 *dp = (struct iconv_utf8*)d2p;
	size_t inlen;
	size_t outlen;
	int error;

	if (inbuf == NULL || *inbuf == NULL || outbuf == NULL || *outbuf == NULL)
		return 0;

	flags |= dp->d_flags;	/* Include the default flags that are not passed in */
	inlen = *inbytesleft;
	outlen = 0;
	
	if (dp->d_type == UTF8_ENCODE)
		error = utf8_encodestr((u_int16_t *)*inbuf, inlen, (u_int8_t *)*outbuf, &outlen, *outbytesleft, 0, flags);
	else if (dp->d_type == UTF8_DECODE)
		error = utf8_decodestr((u_int8_t *)*inbuf, inlen, (u_int16_t *)*outbuf, &outlen, *outbytesleft, 0, flags);
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
