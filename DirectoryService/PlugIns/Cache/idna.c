/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unicode/uidna.h>
#include <unicode/ustring.h>
#include <dns.h>
#include <dns_util.h>
#include <nameser.h>

/* UTF8 */
/*
 * Copyright 2001 Unicode, Inc.
 * 
 * Disclaimer
 * 
 * This src code is provided as is by Unicode, Inc. No claims are
 * made as to fitness for any particular purpose. No warranties of any
 * kind are expressed or implied. The recipient agrees to determine
 * applicability of information provided. If this file has been
 * purchased on magnetic or optical media from Unicode, Inc., the
 * sole remedy for any claim will be exchange of defective media
 * within 90 days of receipt.
 * 
 * Limitations on Rights to Redistribute This Code
 * 
 * Unicode, Inc. hereby grants the right to freely use the information
 * supplied in this file in the creation of products supporting the
 * Unicode Standard, and to make copies of this file in any form
 * for internal or external distribution as long as this notice
 * remains attached.
 */

#include <arpa/nameser.h>
#include <resolv.h>
#include <unicode/uidna.h>
#include <dns.h>
#include <dns_util.h>

#ifdef HANDLE_DNS_LOOKUPS    

extern dns_reply_t *dns_lookup_soa_min(dns_handle_t d, const char *name, uint32_t class, uint32_t type, int *min);

#define ACE_PREFIX_LEN 4
static const char *ace_prefix = "xn--";

static const uint32_t kMaximumUCS2 = 0x0000FFFFUL;
static const uint32_t kMaximumUTF16 = 0x0010FFFFUL;

static const int32_t halfShift = 10;
static const uint32_t halfBase = 0x0010000UL;
static const uint32_t halfMask = 0x3FFUL;
static const uint32_t kSurrogateHighStart = 0xD800UL;
static const uint32_t kSurrogateLowStart = 0xDC00UL;

/*
 * Index into the table below with the first byte of a UTF-8 sequence to
 * get the number of trailing bytes that are supposed to follow it.
 */
static const int8_t trailingBytesForUTF8[256] =
{
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5
};

/*
 * Magic values subtracted from a buffer value during UTF8 conversion.
 * This table contains as many values as there might be trailing bytes
 * in a UTF-8 sequence.
 */
static const uint32_t offsetsFromUTF8[6] =
{
	0x00000000UL, 0x00003080UL, 0x000E2080UL, 0x03C82080UL, 0xFA082080UL, 0x82082080UL
};

int
isLegalUTF8(const uint8_t *src, int length)
{
	uint8_t a;
	const uint8_t *srcptr = src + length;

	switch (length)
	{
		default:
		{
			return 0;
			/* Everything else falls through when "true"... */
		}
		case 4:
		{
			if (((a = (*--srcptr)) < 0x80) || (a > 0xBF)) return 0;
		}
		case 3:
		{
			if (((a = (*--srcptr)) < 0x80) || (a > 0xBF)) return 0;
		}
		case 2:
		{
			if ((a = (*--srcptr)) > 0xBF) return 0;

			switch (*src)
			{
					/* no fall-through in this inner switch */
				case 0xE0:
				{
					if (a < 0xA0) return 0;
					break;
				}
				case 0xF0:
				{
					if (a < 0x90) return 0;
					break;
				}
				case 0xF4:
				{
					if (a > 0x8F) return 0;
					break;
				}
				default:
				{
					if (a < 0x80) return 0;
				}
			}
		}
		case 1:
		{
			if ((*src >= 0x80) && (*src < 0xC2)) return 0;
			if (*src > 0xF4) return 0;
		}
	}

	return 1;
}

static int
idna_needs_encoding(const char *name)
{
    while (*name != 0)
	{
        if (*name & 0x80) return 1;
        ++name;
    }

    return 0;
}

static char *
utf8_to_ace(const char *name)
{
    uint16_t *src, *srcp, *dst;
    uint32_t character;
    UErrorCode status;
    int32_t srclen, dstlen, bytesToRead;
	uint8_t byteval;
	char *out;

	if (name == NULL) return NULL;
	srclen = strlen(name) * 2;
	if (srclen == 0) return NULL;

	if (idna_needs_encoding(name) == 0) return strdup(name);

	srclen++;
	src = (uint16_t *)calloc(1, srclen);
	if (src == NULL) return NULL;

	dst = (uint16_t *)calloc(1, srclen * 4);
	if (dst == NULL)
	{
		free(src);
		return NULL;
	}

	srcp = src;
    while (*name != '\0')
	{
        bytesToRead = trailingBytesForUTF8[(uint8_t)*name] + 1;

        if (isLegalUTF8((const uint8_t *)name, bytesToRead) == 0)
		{
			free(src);
			free(dst);
			return NULL;
		}

        character = 0;

        /*
         * The cases all fall through.
         */
        switch (bytesToRead)
		{
			case 4:
				byteval = name[0];
				name++;
				character += byteval;
				character <<= 6;
			case 3:
				byteval = name[0];
				name++;
				character += byteval;
				character <<= 6;
			case 2:
				byteval = name[0];
				name++;
				character += byteval;
				character <<= 6;
			case 1:
				byteval = name[0];
				name++;
				character += byteval;
        }

        character -= offsetsFromUTF8[bytesToRead - 1];

        if (character <= kMaximumUCS2)
		{
            *(srcp++) = character;
        }
		else if (character <= kMaximumUTF16)
		{
            character -= halfBase;
            *(srcp++) = (character >> halfShift) + kSurrogateHighStart;
            *(srcp++) = (character & halfMask) + kSurrogateLowStart;
        }
		else
		{
			free(src);
			free(dst);
			return NULL;
		}
    }

	status = U_ZERO_ERROR;
	dstlen = uidna_IDNToASCII(src, srcp - src, dst, srclen, UIDNA_ALLOW_UNASSIGNED, NULL, &status);
	free(src);

    if (status == U_ZERO_ERROR)
	{
		out = calloc(1, dstlen * 4);
		u_austrncpy(out, dst, dstlen);
		free(dst);
		return out;
	}

	free(dst);
    return NULL;
}

static int
idna_needs_decoding(const char *name)
{
	char *p;

	if (name == NULL) return 0;

	p = (char *)name;
	while (p != NULL)
	{
		if (strncmp(p, ace_prefix, ACE_PREFIX_LEN) == 0) return 1;
		p = strchr(p, '.');
		if (p != NULL) p++;
	}

    return 0;
}

char *
ace_to_utf8(const char *name)
{
	uint16_t *src, *dst;
	uint32_t dstlen;
	int32_t srclen, outlen;
    UErrorCode status;
	char *out;

	if (name == NULL) return NULL;

	srclen = strlen(name);
	if (srclen == 0) return NULL;

	dstlen = srclen + 1;

	src = (uint16_t *)calloc(1, (srclen + 1) * sizeof(uint16_t));
	if (src == NULL) return NULL;

	dst = (uint16_t *)calloc(1, dstlen * sizeof(uint16_t));
	if (dst == NULL)
	{
		free(src);
		return NULL;
	}

	status = U_ZERO_ERROR;
	u_strFromUTF8((UChar *)src, srclen + 1, &srclen, name, srclen, &status);
	if (status != U_ZERO_ERROR)
	{
		free(src);
		free(dst);
		return NULL;
	}
	
	status = U_ZERO_ERROR;
	dstlen = uidna_IDNToUnicode((const UChar *)src, srclen, (UChar *)dst, dstlen, UIDNA_ALLOW_UNASSIGNED, NULL, &status);
	free(src);

	if ((status != U_ZERO_ERROR) || (dstlen == 0))
	{
		free(dst);
		return NULL;
	}

	outlen = dstlen * 3;
	out = calloc(1, dstlen * 3);
	if (out == NULL)
	{
		free(dst);
		return NULL;
	}

	status = U_ZERO_ERROR;
	u_strToUTF8(out, outlen, &outlen, (const UChar *)dst, dstlen, &status);
	if ((status != U_ZERO_ERROR) || (outlen == 0))
	{
		free(dst);
		return NULL;
	}

	out = reallocf(out, outlen + 1);
	return out;
}

static void
idna_dns_postprocess_name(char **p)
{
	char *uname;

	if (p == NULL) return;
	if (*p == NULL) return;
	if (idna_needs_decoding(*p) == 0) return;
	
	uname = ace_to_utf8(*p);
	if (uname == NULL) return;
	
	free(*p);
	*p = uname;
}

static void
idna_dns_postprocess_question(dns_question_t *q)
{
	idna_dns_postprocess_name(&(q->name));
}

static void
idna_dns_postprocess_resource_record(dns_resource_record_t *r)
{	
	if (r == NULL) return;
	idna_dns_postprocess_name(&(r->name));

	switch (r->dnstype)
	{
		case ns_t_md:
		case ns_t_mf:
		case ns_t_cname:
		case ns_t_mb:
		case ns_t_mg:
		case ns_t_mr:
		case ns_t_ptr:
		case ns_t_ns:
			idna_dns_postprocess_name(&(r->data.CNAME->name));
			return;
			
		case ns_t_soa:
			idna_dns_postprocess_name(&(r->data.SOA->mname));
			idna_dns_postprocess_name(&(r->data.SOA->rname));
			return;
			
		case ns_t_minfo:
			idna_dns_postprocess_name(&(r->data.MINFO->rmailbx));
			idna_dns_postprocess_name(&(r->data.MINFO->emailbx));
			return;
			
		case ns_t_mx:
			idna_dns_postprocess_name(&(r->data.MX->name));
			break;

		case ns_t_rp:
			idna_dns_postprocess_name(&(r->data.RP->mailbox));
			idna_dns_postprocess_name(&(r->data.RP->txtdname));
			return;
			
		case ns_t_afsdb:
			idna_dns_postprocess_name(&(r->data.AFSDB->hostname));
			return;

		case ns_t_rt:
			idna_dns_postprocess_name(&(r->data.RT->intermediate));
			return;
			
		default:
			return;
	}
}

static dns_reply_t *
idna_dns_postprocess_reply(dns_reply_t *r)
{
	dns_header_t *h;
	uint32_t i;

	if (r == NULL) return NULL;

	h = r->header;

	for (i = 0; i < h->qdcount; i++)
		idna_dns_postprocess_question(r->question[i]);

	for (i = 0; i < h->ancount; i++)
		idna_dns_postprocess_resource_record(r->answer[i]);

	for (i = 0; i < h->nscount; i++)
		idna_dns_postprocess_resource_record(r->authority[i]);

	for (i = 0; i < h->arcount; i++)
		idna_dns_postprocess_resource_record(r->additional[i]);
	
	return r;
}

/*
 * IDNA wrapper for dns_lookup()
 * name must be UTF-8
 */
dns_reply_t *
idna_dns_lookup(dns_handle_t dns, const char *name, uint32_t dnsclass, uint32_t dnstype, int *ttl)
{
	char *acename;
	dns_reply_t *r;

	acename = utf8_to_ace(name);
	if (acename == NULL) return NULL;

	r = dns_lookup_soa_min(dns, acename, dnsclass, dnstype, ttl);
	free(acename);

	return idna_dns_postprocess_reply(r);
}

#endif
