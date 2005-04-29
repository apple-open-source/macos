/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

	Change History (most recent first):

$Log: dnssd_ipc.c,v $
Revision 1.10  2004/09/22 20:05:38  majka
Integrated
3725573 - Need Error Codes for handling Lighthouse setup failure on NAT
3805822 - Socket-based APIs aren't endian-safe
3806739 - DNSServiceSetDefaultDomainForUser header comments incorrect

Revision 1.9.4.1  2004/09/20 21:54:33  ksekar
<rdar://problem/3805822> Socket-based APIs aren't endian-safe

Revision 1.9  2004/09/16 23:45:24  majka
Integrated 3775315 and 3765280.

Revision 1.8.4.1  2004/09/02 19:43:41  ksekar
<rdar://problem/3775315>: Sync dns-sd client files between Libinfo and
mDNSResponder projects

Revision 1.11  2004/06/18 04:56:09  rpantos
casting goodness

Revision 1.10  2004/06/12 01:08:14  cheshire
Changes for Windows compatibility

Revision 1.9  2004/05/18 23:51:27  cheshire
Tidy up all checkin comments to use consistent "<rdar://problem/xxxxxxx>" format for bug numbers

Revision 1.8  2003/11/05 22:44:57  ksekar
<rdar://problem/3335230>: No bounds checking when reading data from client
Reviewed by: Stuart Cheshire

Revision 1.7  2003/08/12 19:56:25  cheshire
Update to APSL 2.0

 */

#include "dnssd_ipc.h"

void put_long(const uint32_t l, char **ptr)
	{
	(*ptr)[0] = (char)((l >> 24) &  0xFF);
	(*ptr)[1] = (char)((l >> 16) &  0xFF);
	(*ptr)[2] = (char)((l >>  8) &  0xFF);
	(*ptr)[3] = (char)((l      ) &  0xFF);
	*ptr += sizeof(uint32_t);
	}

uint32_t get_long(char **ptr)
	{
	uint8_t *p = (uint8_t*) *ptr;
	*ptr += sizeof(uint32_t);
	return((uint32_t) ((uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]));
	}

void put_short(uint16_t s, char **ptr)
	{
	(*ptr)[0] = (char)((s >>  8) &  0xFF);
	(*ptr)[1] = (char)((s      ) &  0xFF);
	*ptr += sizeof(uint16_t);
	}

uint16_t get_short(char **ptr)
	{
	uint8_t *p = (uint8_t*) *ptr;
	*ptr += sizeof(uint16_t);
	return((uint16_t) ((uint16_t)p[0] << 8 | p[1]));
	}

int put_string(const char *str, char **ptr)
	{
	if (!str) str = "";
	strcpy(*ptr, str);
	*ptr += strlen(str) + 1;
	return 0;
	}

int get_string(char **ptr, char *buffer, int buflen)
	{
	int overrun = (int)strlen(*ptr) <  buflen ? 0 : -1;
	strncpy(buffer, *ptr,  buflen - 1);
	buffer[buflen - 1] = '\0';
	*ptr += strlen(buffer) + 1;
	return overrun;
	}

void put_rdata(const int rdlen, const char *rdata, char **ptr)
	{
	memcpy(*ptr, rdata, rdlen);
	*ptr += rdlen;
	}

char *get_rdata(char **ptr, int rdlen)
	{
	char *rd = *ptr;
	*ptr += rdlen;
	return rd;
	}

void ConvertHeaderBytes(ipc_msg_hdr *hdr)
	{
	hdr->version   = htonl(hdr->version);
	hdr->datalen   = htonl(hdr->datalen);
	hdr->flags     = htonl(hdr->flags);
	hdr->op        = htonl(hdr->op );
	hdr->reg_index = htonl(hdr->reg_index);
	}
