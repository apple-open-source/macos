/*
 * Copyright (c) 2007 Apple Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 2007 Apple Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <asl_core.h>
#include <string.h>
#include <membership.h>
#include <pthread.h>

/*
 * Message ID generation
 */
static uint64_t _asl_core_msg_next_id = 1;
static pthread_mutex_t msg_id_lock = PTHREAD_MUTEX_INITIALIZER;

#define mix(a, b, c) \
{ \
	a -= b; a -= c; a ^= (c>>13); \
	b -= c; b -= a; b ^= (a<< 8); \
	c -= a; c -= b; c ^= (b>>13); \
	a -= b; a -= c; a ^= (c>>12); \
	b -= c; b -= a; b ^= (a<<16); \
	c -= a; c -= b; c ^= (b>> 5); \
	a -= b; a -= c; a ^= (c>> 3); \
	b -= c; b -= a; b ^= (a<<10); \
	c -= a; c -= b; c ^= (b>>15); \
}

/*
 * Hash is used to improve string search.
 */
uint32_t
asl_core_string_hash(const char *s, uint32_t inlen)
{
	uint32_t a, b, c, l, len;

	if (s == NULL) return 0;

	l = inlen;
	if (l == 0)
	{
		if (s[0] == '\0') return 0;
		l = strlen(s);
	}

	len = l;
	a = b = 0x9e3779b9;
	c = 0;

	while (len >= 12)
	{
		a += (s[0] + ((uint32_t)s[1]<<8) + ((uint32_t)s[ 2]<<16) + ((uint32_t)s[ 3]<<24));
		b += (s[4] + ((uint32_t)s[5]<<8) + ((uint32_t)s[ 6]<<16) + ((uint32_t)s[ 7]<<24));
		c += (s[8] + ((uint32_t)s[9]<<8) + ((uint32_t)s[10]<<16) + ((uint32_t)s[11]<<24));

		mix(a, b, c);

		s += 12;
		len -= 12;
	}

	c += l;
	switch(len)
	{
		case 11: c += ((uint32_t)s[10]<<24);
		case 10: c += ((uint32_t)s[9]<<16);
		case 9 : c += ((uint32_t)s[8]<<8);

		case 8 : b += ((uint32_t)s[7]<<24);
		case 7 : b += ((uint32_t)s[6]<<16);
		case 6 : b += ((uint32_t)s[5]<<8);
		case 5 : b += s[4];

		case 4 : a += ((uint32_t)s[3]<<24);
		case 3 : a += ((uint32_t)s[2]<<16);
		case 2 : a += ((uint32_t)s[1]<<8);
		case 1 : a += s[0];
	}

	mix(a, b, c);

	if (c == 0) c = 1;
	return c;
}

const char *
asl_core_error(uint32_t code)
{
	switch (code)
	{
		case ASL_STATUS_OK: return "Operation Succeeded";
		case ASL_STATUS_INVALID_ARG: return "Invalid Argument";
		case ASL_STATUS_INVALID_STORE: return "Invalid Data Store";
		case ASL_STATUS_INVALID_STRING: return "Invalid String";
		case ASL_STATUS_INVALID_ID: return "Invalid ID Number";
		case ASL_STATUS_INVALID_MESSAGE: return "Invalid Message";
		case ASL_STATUS_NOT_FOUND: return "Not Found";
		case ASL_STATUS_READ_FAILED: return "Read Operation Failed";
		case ASL_STATUS_WRITE_FAILED: return "Write Operation Failed";
		case ASL_STATUS_NO_MEMORY: return "System Memory Allocation Failed";
		case ASL_STATUS_ACCESS_DENIED: return "Access Denied";
		case ASL_STATUS_READ_ONLY: return "Read Only Access";
		case ASL_STATUS_WRITE_ONLY: return "Write Only Access";
		case ASL_STATUS_MATCH_FAILED: return "Match Failed";
		case ASL_STATUS_NO_RECORDS: return "No More Records";
	}

	return "Operation Failed";
}

static uint32_t
asl_core_check_user_access(int32_t msgu, int32_t readu)
{
	/* -1 means anyone may read */
	if (msgu == -1) return ASL_STATUS_OK;

	/* Check for exact match */
	if (msgu == readu) return ASL_STATUS_OK;

	return ASL_STATUS_ACCESS_DENIED;
}

static uint32_t
asl_core_check_group_access(int32_t msgg, int32_t readu, int32_t readg)
{
	int check;
	uuid_t uu, gu;

	/* -1 means anyone may read */
	if (msgg == -1) return ASL_STATUS_OK;

	/* Check for exact match */
	if (msgg == readg) return ASL_STATUS_OK;

	/* Check if user (u) is in read group (msgg) */
	mbr_uid_to_uuid(readu, uu);
	mbr_gid_to_uuid(msgg, gu);

	check = 0;
	mbr_check_membership(uu, gu, &check);
	if (check != 0) return ASL_STATUS_OK;

	return ASL_STATUS_ACCESS_DENIED;
}

uint32_t
asl_core_check_access(int32_t msgu, int32_t msgg, int32_t readu, int32_t readg, uint16_t flags)
{
	uint16_t uset, gset;

	/* root (uid 0) may always read */
	if (readu == 0) return ASL_STATUS_OK;

	uset = flags & ASL_MSG_FLAG_READ_UID_SET;
	gset = flags & ASL_MSG_FLAG_READ_GID_SET;

	/* if no access controls are set, anyone may read */
	if ((uset | gset) == 0) return ASL_STATUS_OK;

	/* if only uid is set, then access is only by uid match */
	if ((uset != 0) && (gset == 0)) return asl_core_check_user_access(msgu, readu);

	/* if only gid is set, then access is only by gid match */
	if ((uset == 0) && (gset != 0)) return asl_core_check_group_access(msgg, readu, readg);

	/* both uid and gid are set - check user, then group */
	if ((asl_core_check_user_access(msgu, readu)) == ASL_STATUS_OK) return ASL_STATUS_OK;
	return asl_core_check_group_access(msgg, readu, readg);
}

uint64_t
asl_core_htonq(uint64_t n)
{
#ifdef __BIG_ENDIAN__
	return n;
#else
	u_int32_t t;
	union
	{
		u_int64_t q;
		u_int32_t l[2];
	} x;

	x.q = n;
	t = x.l[0];
	x.l[0] = htonl(x.l[1]);
	x.l[1] = htonl(t);

	return x.q;
#endif
}

uint64_t
asl_core_ntohq(uint64_t n)
{
#ifdef __BIG_ENDIAN__
	return n;
#else
	u_int32_t t;
	union
	{
		u_int64_t q;
		u_int32_t l[2];
	} x;

	x.q = n;
	t = x.l[0];
	x.l[0] = ntohl(x.l[1]);
	x.l[1] = ntohl(t);

	return x.q;
#endif
}

uint64_t
asl_core_new_msg_id(uint64_t start)
{
	uint64_t out;

	pthread_mutex_lock(&msg_id_lock);

	if (start != 0) _asl_core_msg_next_id = start;

	out = _asl_core_msg_next_id;
	_asl_core_msg_next_id++;

	pthread_mutex_unlock(&msg_id_lock);

	return out;
}
