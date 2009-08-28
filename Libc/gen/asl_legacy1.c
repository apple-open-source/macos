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
#include <asl_legacy1.h>
#include <asl_private.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <string.h>
#include <membership.h>
#include <mach/mach.h>
#include <sys/syslimits.h>
#include <sys/types.h>
#include <time.h>
#include <sys/mman.h>

#define forever for(;;)

#define FILE_MODE 0600

#define DB_RECORD_LEN 80

#define DB_HEADER_COOKIE_OFFSET 0
#define DB_HEADER_VERS_OFFSET 12

#define DB_TYPE_EMPTY   0
#define DB_TYPE_HEADER  1
#define DB_TYPE_MESSAGE 2
#define DB_TYPE_KVLIST  3
#define DB_TYPE_STRING  4
#define DB_TYPE_STRCONT 5

/*
 * Magic Cookie for database files.
 * MAXIMUM 12 CHARS! (DB_HEADER_VERS_OFFSET)
 */
#define ASL_DB_COOKIE "ASL DB"
#define ASL_DB_COOKIE_LEN 6

#define ASL_INDEX_NULL 0xffffffff

#define DB_HLEN_EMPTY    0
#define DB_HLEN_HEADER  13
#define DB_HLEN_MESSAGE 13
#define DB_HLEN_KVLIST   9
#define DB_HLEN_STRING  25
#define DB_HLEN_STRCONT  5

#define MSG_OFF_KEY_TYPE 0
#define MSG_OFF_KEY_NEXT 1
#define MSG_OFF_KEY_ID 5
#define MSG_OFF_KEY_RUID 13
#define MSG_OFF_KEY_RGID 17
#define MSG_OFF_KEY_TIME 21
#define MSG_OFF_KEY_HOST 29
#define MSG_OFF_KEY_SENDER 37
#define MSG_OFF_KEY_FACILITY 45
#define MSG_OFF_KEY_LEVEL 53
#define MSG_OFF_KEY_PID 57
#define MSG_OFF_KEY_UID 61
#define MSG_OFF_KEY_GID 65
#define MSG_OFF_KEY_MSG 69
#define MSG_OFF_KEY_FLAGS 77

extern time_t asl_parse_time(const char *str);
extern int asl_msg_cmp(asl_msg_t *a, asl_msg_t *b);

#define asl_msg_list_t asl_search_result_t

#define PMSG_SEL_TIME		0x0001
#define PMSG_SEL_HOST		0x0002
#define PMSG_SEL_SENDER		0x0004
#define PMSG_SEL_FACILITY	0x0008
#define PMSG_SEL_MESSAGE	0x0010
#define PMSG_SEL_LEVEL		0x0020
#define PMSG_SEL_PID		0x0040
#define PMSG_SEL_UID		0x0080
#define PMSG_SEL_GID		0x0100
#define PMSG_SEL_RUID		0x0200
#define PMSG_SEL_RGID		0x0400

#define PMSG_FETCH_ALL 0
#define PMSG_FETCH_STD 1
#define PMSG_FETCH_KV  2

#define Q_NULL 100001
#define Q_FAST 100002
#define Q_SLOW 100003
#define Q_FAIL 100004

typedef struct
{
	uint16_t kselect;
	uint16_t vselect;
	uint64_t msgid;
	uint64_t time;
	uint64_t host;
	uint64_t sender;
	uint64_t facility;
	uint64_t message;
	uint32_t level;
	uint32_t pid;
	int32_t uid;
	int32_t gid;
	int32_t ruid;
	int32_t rgid;
	uint32_t next;
	uint32_t kvcount;
	uint64_t *kvlist;
} pmsg_t;

static uint64_t
_asl_htonq(uint64_t n)
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

static uint64_t
_asl_ntohq(uint64_t n)
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

static uint16_t
_asl_get_16(char *h)
{
	uint16_t x;

	memcpy(&x, h, 2);
	return ntohs(x);
}

static uint32_t
_asl_get_32(char *h)
{
	uint32_t x;

	memcpy(&x, h, 4);
	return ntohl(x);
}

static uint64_t
_asl_get_64(char *h)
{
	uint64_t x;

	memcpy(&x, h, 8);
	return _asl_ntohq(x);
}

#define header_get_next(h)		_asl_get_32(h +  1)
#define header_get_id(h)		_asl_get_64(h +  5)
#define header_get_hash(h)		_asl_get_32(h + 17)

/*
 * callback for sorting slotlist
 * primary sort is by xid
 * secondary sort is by slot, which happens when xid is 0
 * this allows us to quickly find xids (using binary search on the xid key)
 * it's also used to find slots quickly from record_chain_free()
 */
static int
slot_comp(const void *a, const void *b)
{
	asl_legacy1_slot_info_t *ai, *bi;

	if (a == NULL)
	{
		if (b == NULL) return 0;
		return -1;
	}

	if (b == NULL) return 1;

	ai = (asl_legacy1_slot_info_t *)a;
	bi = (asl_legacy1_slot_info_t *)b;

	if (ai->xid < bi->xid) return -1;

	if (ai->xid == bi->xid)
	{
		if (ai->slot < bi->slot) return -1;
		if (ai->slot == bi->slot) return 0;
		return 1;
	}

	return 1;
}

/* find an xid in the slot list */
static uint32_t
slotlist_find(asl_legacy1_t *s, uint64_t xid, int32_t direction)
{
	uint32_t top, bot, mid, range;

	if (s == NULL) return ASL_INDEX_NULL;
	if (s->slotlist_count == 0) return ASL_INDEX_NULL;
	if (xid == 0) return ASL_INDEX_NULL;

	top = s->slotlist_count - 1;
	bot = 0;
	mid = top / 2;

	range = top - bot;
	while (range > 1)
	{
		if (xid == s->slotlist[mid].xid) return mid;
		else if (xid < s->slotlist[mid].xid) top = mid;
		else bot = mid;

		range = top - bot;
		mid = bot + (range / 2);
	}

	if (xid == s->slotlist[top].xid) return top;
	if (xid == s->slotlist[bot].xid) return bot;

	if (direction == 0) return ASL_INDEX_NULL;
	if (direction < 0) return bot;
	return top;
}

static uint32_t
slotlist_init(asl_legacy1_t *s, uint32_t count)
{
	uint32_t i, si, status, hash, addslot;
	uint64_t xid;
	uint8_t t;
	char tmp[DB_RECORD_LEN];

	/* Start at first slot after the header */
	status = fseek(s->db, DB_RECORD_LEN, SEEK_SET);
	if (status != 0) return ASL_STATUS_READ_FAILED;

	s->slotlist = (asl_legacy1_slot_info_t *)calloc(count, sizeof(asl_legacy1_slot_info_t));
	if (s->slotlist == NULL) return ASL_STATUS_NO_MEMORY;

	si = 0;

	for (i = 1; i < count; i++)
	{
		status = fread(tmp, DB_RECORD_LEN, 1, s->db);
		if (status != 1) return ASL_STATUS_READ_FAILED;

		t = tmp[0];
		addslot = 0;
		xid = 0;
		hash = 0;

		if (t == DB_TYPE_EMPTY) addslot = 1;

		if (t == DB_TYPE_STRING)
		{
			addslot = 1;
			xid = header_get_id(tmp);
			hash = header_get_hash(tmp);
		}

		if (t == DB_TYPE_MESSAGE)
		{
			addslot = 1;
			xid = header_get_id(tmp);
		}

		if (addslot == 1)
		{
			s->slotlist[si].type = t;
			s->slotlist[si].slot = i;
			s->slotlist[si].xid = xid;
			s->slotlist[si].hash = hash;
			si++;
		}
	}

	s->slotlist = (asl_legacy1_slot_info_t *)reallocf(s->slotlist, si * sizeof(asl_legacy1_slot_info_t));
	if (s->slotlist == NULL) return ASL_STATUS_NO_MEMORY;
	s->slotlist_count = si;

	/* slotlist is sorted by xid */
	qsort((void *)s->slotlist, s->slotlist_count, sizeof(asl_legacy1_slot_info_t), slot_comp);

	return ASL_STATUS_OK;
}

uint32_t
asl_legacy1_open(const char *path, asl_legacy1_t **out)
{
	asl_legacy1_t *s;
	struct stat sb;
	int status;
	char cbuf[DB_RECORD_LEN];
	off_t fsize;
	uint32_t count;

	memset(&sb, 0, sizeof(struct stat));
	status = stat(path, &sb);
	if (status < 0) return ASL_STATUS_FAILED;

	fsize = sb.st_size;

	s = (asl_legacy1_t *)calloc(1, sizeof(asl_legacy1_t));
	if (s == NULL) return ASL_STATUS_NO_MEMORY;

	s->db = fopen(path, "r");
	if (s->db == NULL)
	{
		free(s);
		return ASL_STATUS_INVALID_STORE;
	}

	memset(cbuf, 0, DB_RECORD_LEN);
	status = fread(cbuf, DB_RECORD_LEN, 1, s->db);
	if (status != 1)
	{
		fclose(s->db);
		free(s);
		return ASL_STATUS_READ_FAILED;
	}

	/* Check the database Magic Cookie */
	if (strncmp(cbuf, ASL_DB_COOKIE, ASL_DB_COOKIE_LEN))
	{
		fclose(s->db);
		free(s);
		return ASL_STATUS_INVALID_STORE;
	}

	count = fsize / DB_RECORD_LEN;

	status = slotlist_init(s, count);

	*out = s;
	return ASL_STATUS_OK;
}

uint32_t
asl_legacy1_close(asl_legacy1_t *s)
{
	if (s == NULL) return ASL_STATUS_INVALID_STORE;

	if (s->slotlist != NULL) free(s->slotlist);
	if (s->db != NULL) fclose(s->db);
	free(s);

	return ASL_STATUS_OK;
}

static uint32_t
string_fetch_slot(asl_legacy1_t *s, uint32_t slot, char **out)
{
	off_t offset;
	uint8_t type;
	uint32_t status, next, len, x, remaining;
	char *outstr, *p, tmp[DB_RECORD_LEN];

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (out == NULL) return ASL_STATUS_INVALID_ARG;

	*out = NULL;
	offset = slot * DB_RECORD_LEN;
	status = fseek(s->db, offset, SEEK_SET);

	if (status < 0) return ASL_STATUS_READ_FAILED;

	status = fread(tmp, DB_RECORD_LEN, 1, s->db);
	if (status != 1) return ASL_STATUS_READ_FAILED;

	type = tmp[0];
	if (type != DB_TYPE_STRING) return ASL_STATUS_INVALID_STRING;

	len = _asl_get_32(tmp + 21);
	if (len == 0) return ASL_STATUS_OK;

	next = header_get_next(tmp);

	outstr = calloc(1, len);
	if (outstr == NULL) return ASL_STATUS_NO_MEMORY;

	p = outstr;
	remaining = len;

	x = DB_RECORD_LEN - DB_HLEN_STRING;
	if (x > remaining) x = remaining;

	memcpy(p, tmp + DB_HLEN_STRING, x);
	p += x;
	remaining -= x;

	while ((next != 0) && (remaining > 0))
	{
		offset = next * DB_RECORD_LEN;
		status = fseek(s->db, offset, SEEK_SET);

		if (status < 0)
		{
			free(outstr);
			return ASL_STATUS_READ_FAILED;
		}

		status = fread(tmp, DB_RECORD_LEN, 1, s->db);
		if (status != 1)
		{
			free(outstr);
			return ASL_STATUS_READ_FAILED;
		}

		next = header_get_next(tmp);

		x = DB_RECORD_LEN - DB_HLEN_STRCONT;
		if (x > remaining) x = remaining;

		memcpy(p, tmp + DB_HLEN_STRCONT, x);
		p += x;
		remaining -= x;
	}

	if ((next != 0) || (remaining != 0))
	{
		free(outstr);
		return ASL_STATUS_READ_FAILED;
	}

	*out = outstr;
	return ASL_STATUS_OK;
}

static uint32_t
string_fetch_sid(asl_legacy1_t *s, uint64_t sid, char **out)
{
	uint32_t i, len, ref;
	uint64_t nsid;
	uint8_t inls;
	char *p;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (out == NULL) return ASL_STATUS_INVALID_ARG;

	*out = NULL;
	if (sid == ASL_REF_NULL) return ASL_STATUS_OK;

	ref = 0;

	inls = 0;
	nsid = _asl_htonq(sid);
	memcpy(&inls, &nsid, 1);
	if (inls & 0x80)
	{
		/* inline string */
		inls &= 0x0f;
		len = inls;
		*out = calloc(1, len);
		if (*out == NULL) return ASL_STATUS_NO_MEMORY;
		p = 1 + (char *)&nsid;
		memcpy(*out, p, len);
		return ASL_STATUS_OK;
	}

	/* Find the string in the database */
	i = slotlist_find(s, sid, 0);
	if (i == ASL_INDEX_NULL) return ASL_STATUS_NOT_FOUND;

	return string_fetch_slot(s, s->slotlist[i].slot, out);
}

static uint32_t
pmsg_fetch(asl_legacy1_t *s, uint32_t slot, uint32_t action, pmsg_t **pmsg)
{
	off_t offset;
	uint32_t status, i, n, v32, next;
	int32_t msgu, msgg;
	uint64_t msgid;
	uint16_t flags;
	pmsg_t *out;
	char *p, tmp[DB_RECORD_LEN];

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (pmsg == NULL) return ASL_STATUS_INVALID_ARG;

	out = NULL;

	if ((action == PMSG_FETCH_ALL) || (action == PMSG_FETCH_STD))
	{
		*pmsg = NULL;

		offset = slot * DB_RECORD_LEN;
		status = fseek(s->db, offset, SEEK_SET);

		if (status < 0) return ASL_STATUS_READ_FAILED;

		status = fread(tmp, DB_RECORD_LEN, 1, s->db);
		if (status != 1) return ASL_STATUS_READ_FAILED;

		msgid = _asl_get_64(tmp + MSG_OFF_KEY_ID);
		msgu = _asl_get_32(tmp + MSG_OFF_KEY_RUID);
		msgg = _asl_get_32(tmp + MSG_OFF_KEY_RGID);
		flags = _asl_get_16(tmp + MSG_OFF_KEY_FLAGS);

		out = (pmsg_t *)calloc(1, sizeof(pmsg_t));
		if (out == NULL) return ASL_STATUS_NO_MEMORY;


		p = tmp + 21;

		/* ID */
		out->msgid = msgid;

		/* ReadUID */
		out->ruid = msgu;

		/* ReadGID */
		out->rgid = msgg;

		/* Time */
		out->time = _asl_get_64(p);
		p += 8;

		/* Host */
		out->host = _asl_get_64(p);
		p += 8;

		/* Sender */
		out->sender = _asl_get_64(p);
		p += 8;

		/* Facility */
		out->facility = _asl_get_64(p);
		p += 8;

		/* Level */
		out->level = _asl_get_32(p);
		p += 4;

		/* PID */
		out->pid = _asl_get_32(p);
		p += 4;

		/* UID */
		out->uid = _asl_get_32(p);
		p += 4;

		/* GID */
		out->gid = _asl_get_32(p);
		p += 4;

		/* Message */
		out->message = _asl_get_64(p);
		p += 8;

		next = header_get_next(tmp);
		out->next = next;

		if (action == PMSG_FETCH_STD)
		{
			/* caller only wants "standard" keys */
			*pmsg = out;
			return ASL_STATUS_OK;
		}

		*pmsg = out;
	}
	else
	{
		out = *pmsg;
	}

	n = 0;
	next = out->next;

	while (next != 0)
	{
		offset = next * DB_RECORD_LEN;
		status = fseek(s->db, offset, SEEK_SET);
		if (status < 0)
		{
			*pmsg = NULL;
			free(out);
			return ASL_STATUS_READ_FAILED;
		}

		status = fread(tmp, DB_RECORD_LEN, 1, s->db);
		if (status != 1)
		{
			*pmsg = NULL;
			free(out);
			return ASL_STATUS_READ_FAILED;
		}

		if (out->kvcount == 0)
		{
			v32 = _asl_get_32(tmp + 5);
			out->kvcount = v32 * 2;
			out->kvlist = (uint64_t *)calloc(out->kvcount, sizeof(uint64_t));
			if (out->kvlist == NULL)
			{
				*pmsg = NULL;
				free(out);
				return ASL_STATUS_NO_MEMORY;
			}
		}

		p = tmp + 9;

		for (i = 0; (i < 4) && (n < out->kvcount); i++)
		{
			out->kvlist[n++] = _asl_get_64(p);
			p += 8;

			out->kvlist[n++] = _asl_get_64(p);
			p += 8;
		}

		next = header_get_next(tmp);
	}

	return ASL_STATUS_OK;
}

static uint32_t
pmsg_match(asl_legacy1_t *s, pmsg_t *q, pmsg_t *m)
{
	uint32_t i, j;

	if (s == NULL) return 0;
	if (q == NULL) return 1;
	if (m == NULL) return 0;

	if (q->kselect & PMSG_SEL_TIME)
	{
		if (q->time == ASL_REF_NULL) return 0;
		if ((q->vselect & PMSG_SEL_TIME) && (q->time != m->time)) return 0;
	}

	if (q->kselect & PMSG_SEL_HOST)
	{
		if (q->host == ASL_REF_NULL) return 0;
		if ((q->vselect & PMSG_SEL_HOST) && (q->host != m->host)) return 0;
	}

	if (q->kselect & PMSG_SEL_SENDER)
	{
		if (q->sender == ASL_REF_NULL) return 0;
		if ((q->vselect & PMSG_SEL_SENDER) && (q->sender != m->sender)) return 0;
	}

	if (q->kselect & PMSG_SEL_FACILITY)
	{
		if (q->facility == ASL_REF_NULL) return 0;
		if ((q->vselect & PMSG_SEL_FACILITY) && (q->facility != m->facility)) return 0;
	}

	if (q->kselect & PMSG_SEL_MESSAGE)
	{
		if (q->message == ASL_REF_NULL) return 0;
		if ((q->vselect & PMSG_SEL_MESSAGE) && (q->message != m->message)) return 0;
	}

	if (q->kselect & PMSG_SEL_LEVEL)
	{
		if (q->level == ASL_INDEX_NULL) return 0;
		if ((q->vselect & PMSG_SEL_LEVEL) && (q->level != m->level)) return 0;
	}

	if (q->kselect & PMSG_SEL_PID)
	{
		if (q->pid == -1) return 0;
		if ((q->vselect & PMSG_SEL_PID) && (q->pid != m->pid)) return 0;
	}

	if (q->kselect & PMSG_SEL_UID)
	{
		if (q->uid == -2) return 0;
		if ((q->vselect & PMSG_SEL_UID) && (q->uid != m->uid)) return 0;
	}

	if (q->kselect & PMSG_SEL_GID)
	{
		if (q->gid == -2) return 0;
		if ((q->vselect & PMSG_SEL_GID) && (q->gid != m->gid)) return 0;
	}

	if (q->kselect & PMSG_SEL_RUID)
	{
		if (q->ruid == -1) return 0;
		if ((q->vselect & PMSG_SEL_RUID) && (q->ruid != m->ruid)) return 0;
	}

	if (q->kselect & PMSG_SEL_RGID)
	{
		if (q->rgid == -1) return 0;
		if ((q->vselect & PMSG_SEL_RGID) && (q->rgid != m->rgid)) return 0;
	}

	for (i = 0; i < q->kvcount; i += 2)
	{
		for (j = 0; j < m->kvcount; j += 2)
		{
			if (q->kvlist[i] == m->kvlist[j])
			{
				if (q->kvlist[i + 1] == m->kvlist[j + 1]) break;
				return 0;
			}
		}

		if (j >= m->kvcount) return 0;
	}

	return 1;
}

static void
free_pmsg(pmsg_t *p)
{
	if (p == NULL) return;
	if (p->kvlist != NULL) free(p->kvlist);
	free(p);
}

static uint32_t
pmsg_fetch_by_id(asl_legacy1_t *s, uint64_t msgid, pmsg_t **pmsg, uint32_t *slot)
{
	uint32_t i, status;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (msgid == ASL_REF_NULL) return ASL_STATUS_INVALID_ARG;
	if (slot == NULL) return ASL_STATUS_INVALID_ARG;

	*slot = ASL_INDEX_NULL;

	i = slotlist_find(s, msgid, 0);
	if (i == ASL_INDEX_NULL) return ASL_STATUS_INVALID_ID;

	*slot = s->slotlist[i].slot;

	/* read the message */
	*pmsg = NULL;
	status = pmsg_fetch(s, s->slotlist[i].slot, PMSG_FETCH_ALL, pmsg);
	if (status != ASL_STATUS_OK) return status;
	if (pmsg == NULL) return ASL_STATUS_FAILED;

	return status;
}

static uint32_t
msg_decode(asl_legacy1_t *s, pmsg_t *pmsg, asl_msg_t **out)
{
	uint32_t status, i, n;
	char *key, *val;
	asl_msg_t *msg;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (out == NULL) return ASL_STATUS_INVALID_ARG;
	if (pmsg == NULL) return ASL_STATUS_INVALID_ARG;

	*out = NULL;

	msg = (asl_msg_t *)calloc(1, sizeof(asl_msg_t));
	if (msg == NULL) return ASL_STATUS_NO_MEMORY;

	msg->type = ASL_TYPE_MSG;
	msg->count = 0;
	if (pmsg->time != ASL_REF_NULL) msg->count++;
	if (pmsg->host != ASL_REF_NULL) msg->count++;
	if (pmsg->sender != ASL_REF_NULL) msg->count++;
	if (pmsg->facility != ASL_REF_NULL) msg->count++;
	if (pmsg->message != ASL_REF_NULL) msg->count++;
	if (pmsg->level != ASL_INDEX_NULL) msg->count++;
	if (pmsg->pid != -1) msg->count++;
	if (pmsg->uid != -2) msg->count++;
	if (pmsg->gid != -2) msg->count++;
	if (pmsg->ruid != -1) msg->count++;
	if (pmsg->rgid != -1) msg->count++;

	msg->count += pmsg->kvcount / 2;

	if (msg->count == 0)
	{
		free(msg);
		return ASL_STATUS_INVALID_MESSAGE;
	}

	/* Message ID */
	msg->count += 1;

	msg->key = (char **)calloc(msg->count, sizeof(char *));
	if (msg->key == NULL)
	{
		free(msg);
		return ASL_STATUS_NO_MEMORY;
	}

	msg->val = (char **)calloc(msg->count, sizeof(char *));
	if (msg->val == NULL)
	{
		free(msg->key);
		free(msg);
		return ASL_STATUS_NO_MEMORY;
	}

	n = 0;

	/* Time */
	if (pmsg->time != ASL_REF_NULL)
	{
		msg->key[n] = strdup(ASL_KEY_TIME);
		if (msg->key[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}

		asprintf(&(msg->val[n]), "%llu", pmsg->time);
		if (msg->val[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}
		n++;
	}

	/* Host */
	if (pmsg->host != ASL_REF_NULL)
	{
		msg->key[n] = strdup(ASL_KEY_HOST);
		if (msg->key[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}

		status = string_fetch_sid(s, pmsg->host, &(msg->val[n]));
		n++;
	}

	/* Sender */
	if (pmsg->sender != ASL_REF_NULL)
	{
		msg->key[n] = strdup(ASL_KEY_SENDER);
		if (msg->key[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}

		status = string_fetch_sid(s, pmsg->sender, &(msg->val[n]));
		n++;
	}

	/* Facility */
	if (pmsg->facility != ASL_REF_NULL)
	{
		msg->key[n] = strdup(ASL_KEY_FACILITY);
		if (msg->key[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}

		status = string_fetch_sid(s, pmsg->facility, &(msg->val[n]));
		n++;
	}

	/* Level */
	if (pmsg->level != ASL_INDEX_NULL)
	{
		msg->key[n] = strdup(ASL_KEY_LEVEL);
		if (msg->key[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}

		asprintf(&(msg->val[n]), "%u", pmsg->level);
		if (msg->val[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}
		n++;
	}

	/* PID */
	if (pmsg->pid != -1)
	{
		msg->key[n] = strdup(ASL_KEY_PID);
		if (msg->key[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}

		asprintf(&(msg->val[n]), "%d", pmsg->pid);
		if (msg->val[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}
		n++;
	}

	/* UID */
	if (pmsg->uid != -2)
	{
		msg->key[n] = strdup(ASL_KEY_UID);
		if (msg->key[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}

		asprintf(&(msg->val[n]), "%d", pmsg->uid);
		if (msg->val[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}
		n++;
	}

	/* GID */
	if (pmsg->gid != -2)
	{
		msg->key[n] = strdup(ASL_KEY_GID);
		if (msg->key[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}

		asprintf(&(msg->val[n]), "%d", pmsg->gid);
		if (msg->val[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}
		n++;
	}

	/* Message */
	if (pmsg->message != ASL_REF_NULL)
	{
		msg->key[n] = strdup(ASL_KEY_MSG);
		if (msg->key[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}

		status = string_fetch_sid(s, pmsg->message, &(msg->val[n]));
		n++;
	}

	/* ReadUID */
	if (pmsg->ruid != -1)
	{
		msg->key[n] = strdup(ASL_KEY_READ_UID);
		if (msg->key[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}

		asprintf(&(msg->val[n]), "%d", pmsg->ruid);
		if (msg->val[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}
		n++;
	}

	/* ReadGID */
	if (pmsg->rgid != -1)
	{
		msg->key[n] = strdup(ASL_KEY_READ_GID);
		if (msg->key[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}

		asprintf(&(msg->val[n]), "%d", pmsg->rgid);
		if (msg->val[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}
		n++;
	}

	/* Message ID */
	msg->key[n] = strdup(ASL_KEY_MSG_ID);
	if (msg->key[n] == NULL)
	{
		asl_free(msg);
		return ASL_STATUS_NO_MEMORY;
	}

	asprintf(&(msg->val[n]), "%llu", pmsg->msgid);
	if (msg->val[n] == NULL)
	{
		asl_free(msg);
		return ASL_STATUS_NO_MEMORY;
	}
	n++;

	/* Key - Value List */
	for (i = 0; i < pmsg->kvcount; i++)
	{
		key = NULL;
		status = string_fetch_sid(s, pmsg->kvlist[i++], &key);
		if (status != ASL_STATUS_OK)
		{
			if (key != NULL) free(key);
			continue;
		}

		val = NULL;
		status = string_fetch_sid(s, pmsg->kvlist[i], &val);
		if (status != ASL_STATUS_OK)
		{
			if (key != NULL) free(key);
			if (val != NULL) free(val);
			continue;
		}

		msg->key[n] = key;
		msg->val[n] = val;
		n++;
	}

	*out = msg;
	return ASL_STATUS_OK;
}

/*
 * Finds string either in the string cache or in the database
 */
static uint32_t
store_string_find(asl_legacy1_t *s, uint32_t hash, const char *str, uint32_t *index)
{
	uint32_t i, status;
	char *tmp;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (str == NULL) return ASL_STATUS_INVALID_ARG;
	if (index == NULL) return ASL_STATUS_INVALID_ARG;
	if (s->slotlist == NULL) return ASL_STATUS_FAILED;

	/* check the database */
	for (i = 0; i < s->slotlist_count; i++)
	{
		if ((s->slotlist[i].type != DB_TYPE_STRING) || (s->slotlist[i].hash != hash)) continue;

		/* read the whole string */
		tmp = NULL;
		status = string_fetch_slot(s, s->slotlist[i].slot, &tmp);
		if (status != ASL_STATUS_OK) return status;
		if (tmp == NULL) return ASL_STATUS_FAILED;

		status = strcmp(tmp, str);
		free(tmp);
		if (status != 0) continue;

		/* Bingo! */
		*index = i;
		return ASL_STATUS_OK;
	}

	return ASL_STATUS_FAILED;
}

/*
 * Looks up a string ID number.
 */
static uint64_t
string_lookup(asl_legacy1_t *s, const char *str)
{
	uint32_t status, hash, index, slot, len;
	uint64_t nsid, sid;
	char *p;
	uint8_t inls;

	if (s == NULL) return ASL_REF_NULL;
	if (str == NULL) return ASL_REF_NULL;

	sid = ASL_REF_NULL;
	index = ASL_INDEX_NULL;
	slot = ASL_INDEX_NULL;

	len = strlen(str);
	if (len < 8)
	{
		/* inline string */
		inls = len;
		inls |= 0x80;

		nsid = 0;
		p = (char *)&nsid;
		memcpy(p, &inls, 1);
		memcpy(p + 1, str, len);
		sid = _asl_ntohq(nsid);
		return sid;
	}

	hash = asl_core_string_hash(str, len);

	/* check the database */
	status = store_string_find(s, hash, str, &index);
	if (status == ASL_STATUS_OK)
	{
		if (index == ASL_INDEX_NULL) return ASL_REF_NULL;
		return s->slotlist[index].xid;
	}

	return ASL_REF_NULL;
}

uint32_t
asl_legacy1_fetch(asl_legacy1_t *s, uint64_t msgid, asl_msg_t **msg)
{
	uint32_t status, slot;
	pmsg_t *pmsg;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (msgid == ASL_REF_NULL) return ASL_STATUS_INVALID_ARG;

	pmsg = NULL;
	slot = ASL_INDEX_NULL;

	status = pmsg_fetch_by_id(s, msgid, &pmsg, &slot);
	if (status != ASL_STATUS_OK) return status;
	if (pmsg == NULL) return ASL_STATUS_FAILED;

	status = msg_decode(s, pmsg, msg);
	free_pmsg(pmsg);

	return status;
}

static uint32_t
query_to_pmsg(asl_legacy1_t *s, asl_msg_t *q, pmsg_t **p)
{
	pmsg_t *out;
	uint32_t i, j;
	uint64_t ksid, vsid;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (p == NULL) return ASL_STATUS_INVALID_ARG;

	if (q == NULL) return Q_NULL;
	if (q->count == 0) return Q_NULL;

	*p = NULL;

	if (q->op != NULL)
	{
		for (i = 0; i < q->count; i++) if (q->op[i] != ASL_QUERY_OP_EQUAL) return Q_SLOW;
	}

	out = (pmsg_t *)calloc(1, sizeof(pmsg_t));
	if (out == NULL) return ASL_STATUS_NO_MEMORY;

	for (i = 0; i < q->count; i++)
	{
		if (q->key[i] == NULL) continue;

		else if (!strcmp(q->key[i], ASL_KEY_TIME))
		{
			if (out->kselect & PMSG_SEL_TIME)
			{
				free_pmsg(out);
				return Q_SLOW;
			}

			out->kselect |= PMSG_SEL_TIME;
			if (q->val[i] != NULL)
			{
				out->vselect |= PMSG_SEL_TIME;
				out->time = asl_parse_time(q->val[i]);
			}
		}
		else if (!strcmp(q->key[i], ASL_KEY_HOST))
		{
			if (out->kselect & PMSG_SEL_HOST)
			{
				free_pmsg(out);
				return Q_SLOW;
			}

			out->kselect |= PMSG_SEL_HOST;
			if (q->val[i] != NULL)
			{
				out->vselect |= PMSG_SEL_HOST;
				out->host = string_lookup(s, q->val[i]);
				if (out->host == ASL_REF_NULL)
				{
					free_pmsg(out);
					return Q_FAIL;
				}
			}
		}
		else if (!strcmp(q->key[i], ASL_KEY_SENDER))
		{
			if (out->kselect & PMSG_SEL_SENDER)
			{
				free_pmsg(out);
				return Q_SLOW;
			}

			out->kselect |= PMSG_SEL_SENDER;
			if (q->val[i] != NULL)
			{
				out->vselect |= PMSG_SEL_SENDER;
				out->sender = string_lookup(s, q->val[i]);
				if (out->sender == ASL_REF_NULL)
				{
					free_pmsg(out);
					return Q_FAIL;
				}
			}
		}
		else if (!strcmp(q->key[i], ASL_KEY_PID))
		{
			if (out->kselect & PMSG_SEL_PID)
			{
				free_pmsg(out);
				return Q_SLOW;
			}

			out->kselect |= PMSG_SEL_PID;
			if (q->val[i] != NULL)
			{
				out->vselect |= PMSG_SEL_PID;
				out->pid = atoi(q->val[i]);
			}
		}
		else if (!strcmp(q->key[i], ASL_KEY_UID))
		{
			if (out->kselect & PMSG_SEL_UID)
			{
				free_pmsg(out);
				return Q_SLOW;
			}

			out->kselect |= PMSG_SEL_UID;
			if (q->val[i] != NULL)
			{
				out->vselect |= PMSG_SEL_UID;
				out->uid = atoi(q->val[i]);
			}
		}
		else if (!strcmp(q->key[i], ASL_KEY_GID))
		{
			if (out->kselect & PMSG_SEL_GID)
			{
				free_pmsg(out);
				return Q_SLOW;
			}

			out->kselect |= PMSG_SEL_GID;
			if (q->val[i] != NULL)
			{
				out->vselect |= PMSG_SEL_GID;
				out->gid = atoi(q->val[i]);
			}
		}
		else if (!strcmp(q->key[i], ASL_KEY_LEVEL))
		{
			if (out->kselect & PMSG_SEL_LEVEL)
			{
				free_pmsg(out);
				return Q_SLOW;
			}

			out->kselect |= PMSG_SEL_LEVEL;
			if (q->val[i] != NULL)
			{
				out->vselect |= PMSG_SEL_LEVEL;
				out->level = atoi(q->val[i]);
			}
		}
		else if (!strcmp(q->key[i], ASL_KEY_MSG))
		{
			if (out->kselect & PMSG_SEL_MESSAGE)
			{
				free_pmsg(out);
				return Q_SLOW;
			}

			out->kselect |= PMSG_SEL_MESSAGE;
			if (q->val[i] != NULL)
			{
				out->vselect |= PMSG_SEL_MESSAGE;
				out->message = string_lookup(s, q->val[i]);
				if (out->message == ASL_REF_NULL)
				{
					free_pmsg(out);
					return Q_FAIL;
				}
			}
		}
		else if (!strcmp(q->key[i], ASL_KEY_FACILITY))
		{
			if (out->kselect & PMSG_SEL_FACILITY)
			{
				free_pmsg(out);
				return Q_SLOW;
			}

			out->kselect |= PMSG_SEL_FACILITY;
			if (q->val[i] != NULL)
			{
				out->vselect |= PMSG_SEL_FACILITY;
				out->facility = string_lookup(s, q->val[i]);
				if (out->facility == ASL_REF_NULL)
				{
					free_pmsg(out);
					return Q_FAIL;
				}
			}
		}
		else if (!strcmp(q->key[i], ASL_KEY_READ_UID))
		{
			if (out->kselect & PMSG_SEL_RUID)
			{
				free_pmsg(out);
				return Q_SLOW;
			}

			out->kselect |= PMSG_SEL_RUID;
			if (q->val[i] != NULL)
			{
				out->vselect |= PMSG_SEL_RUID;
				out->ruid = atoi(q->val[i]);
			}
		}
		else if (!strcmp(q->key[i], ASL_KEY_READ_GID))
		{
			if (out->kselect & PMSG_SEL_RGID)
			{
				free_pmsg(out);
				return Q_SLOW;
			}

			out->kselect |= PMSG_SEL_RGID;
			if (q->val[i] != NULL)
			{
				out->vselect |= PMSG_SEL_RGID;
				out->rgid = atoi(q->val[i]);
			}
		}
		else
		{
			ksid = string_lookup(s, q->key[i]);
			if (ksid == ASL_REF_NULL)
			{
				free_pmsg(out);
				return Q_FAIL;
			}

			for (j = 0; j < out->kvcount; j += 2)
			{
				if (out->kvlist[j] == ksid)
				{
					free_pmsg(out);
					return Q_SLOW;
				}
			}

			vsid = ASL_REF_NULL;
			if (q->val[i] != NULL)
			{
				vsid = string_lookup(s, q->val[i]);
				if (ksid == ASL_REF_NULL)
				{
					free_pmsg(out);
					return Q_FAIL;
				}
			}

			if (out->kvcount == 0)
			{
				out->kvlist = (uint64_t *)calloc(2, sizeof(uint64_t));
			}
			else
			{
				out->kvlist = (uint64_t *)reallocf(out->kvlist, (out->kvcount + 2) * sizeof(uint64_t));
			}

			if (out->kvlist == NULL)
			{
				free_pmsg(out);
				return ASL_STATUS_NO_MEMORY;
			}

			out->kvlist[out->kvcount++] = ksid;
			out->kvlist[out->kvcount++] = vsid;
		}
	}

	*p = out;
	return Q_FAST;
}

static uint32_t
msg_match(asl_legacy1_t *s, uint32_t qtype, pmsg_t *qp, asl_msg_t *q, uint32_t slot, pmsg_t **iopm, asl_msg_t **iomsg, asl_msg_list_t **res, uint32_t *didmatch)
{
	uint32_t status, what;

	*didmatch = 0;

	if (qtype == Q_FAIL) return ASL_STATUS_OK;

	if (qtype == Q_NULL)
	{
		if (*iopm == NULL)
		{
			status = pmsg_fetch(s, slot, PMSG_FETCH_ALL, iopm);
			if (status != ASL_STATUS_OK) return status;
			if (*iopm == NULL) return ASL_STATUS_FAILED;
		}
	}
	else if (qtype == Q_FAST)
	{
		if (qp == NULL) return ASL_STATUS_INVALID_ARG;

		what = PMSG_FETCH_STD;
		if (qp->kvcount > 0) what = PMSG_FETCH_ALL;

		if (*iopm == NULL)
		{
			status = pmsg_fetch(s, slot, what, iopm);
			if (status != ASL_STATUS_OK) return status;
			if (*iopm == NULL) return ASL_STATUS_FAILED;
		}

		status = pmsg_match(s, qp, *iopm);
		if (status == 1)
		{
			if ((what == PMSG_FETCH_STD) && ((*iopm)->next != 0) && ((*iopm)->kvcount == 0))
			{
				status = pmsg_fetch(s, slot, PMSG_FETCH_KV, iopm);
				if (status != ASL_STATUS_OK) return status;
				if (*iopm == NULL) return ASL_STATUS_FAILED;
			}
		}
		else return ASL_STATUS_OK;
	}
	else if (qtype == Q_SLOW)
	{
		if (*iomsg == NULL)
		{
			if (*iopm == NULL)
			{
				status = pmsg_fetch(s, slot, PMSG_FETCH_ALL, iopm);
				if (status != ASL_STATUS_OK) return status;
				if (*iopm == NULL) return ASL_STATUS_FAILED;
			}

			status = msg_decode(s, *iopm, iomsg);
			if (status == ASL_STATUS_INVALID_MESSAGE) return ASL_STATUS_OK;
			if (status != ASL_STATUS_OK) return status;
			if (*iomsg == NULL) return ASL_STATUS_FAILED;
		}

		status = 0;
		if (asl_msg_cmp(q, *iomsg) != 0) status = 1;
		if (status == 0) return ASL_STATUS_OK;
	}

	*didmatch = 1;

	if (res == NULL) return ASL_STATUS_OK;

	if (*iomsg == NULL)
	{
		status = msg_decode(s, *iopm, iomsg);
		if (status == ASL_STATUS_INVALID_MESSAGE)
		{
			*didmatch = 0;
			return ASL_STATUS_OK;
		}

		if (status != ASL_STATUS_OK) return status;
	}

	if ((*res)->count == 0) (*res)->msg = (asl_msg_t **)calloc(1, sizeof(asl_msg_t *));
	else (*res)->msg = (asl_msg_t **)reallocf((*res)->msg, (1 + (*res)->count) * sizeof(asl_msg_t *));
	if ((*res)->msg == NULL) return ASL_STATUS_NO_MEMORY;

	(*res)->msg[(*res)->count++] = *iomsg;

	return ASL_STATUS_OK;
}

static uint32_t
next_search_slot(asl_legacy1_t *s, uint32_t last_si, int32_t direction)
{
	uint32_t i;

	if (direction >= 0)
	{
		for (i = last_si + 1; i < s->slotlist_count; i++)
		{
			if (s->slotlist[i].type == DB_TYPE_MESSAGE) return i;
		}

		return ASL_INDEX_NULL;
	}

	if (last_si == 0) return ASL_INDEX_NULL;
	if (last_si > s->slotlist_count) return ASL_INDEX_NULL;

	for (i = last_si - 1; i > 0; i--)
	{
		if (s->slotlist[i].type == DB_TYPE_MESSAGE) return i;
	}

	if (s->slotlist[0].type == DB_TYPE_MESSAGE) return 0;

	return ASL_INDEX_NULL;
}

static uint32_t
query_list_to_pmsg_list(asl_legacy1_t *s, asl_msg_list_t *query, uint32_t *match, pmsg_t ***qp, uint32_t **qtype, uint32_t *count)
{
	pmsg_t **outp, *pm;
	uint32_t i, j, *outt;
	*match = 0;
	*qp = NULL;
	*qtype = 0;
	*count = 0;

	if (query == NULL) return ASL_STATUS_OK;
	if (match == NULL) return ASL_STATUS_INVALID_ARG;
	if (qp == NULL) return ASL_STATUS_INVALID_ARG;
	if (qtype == NULL) return ASL_STATUS_OK;
	if (query->msg == NULL) return ASL_STATUS_OK;
	if (query->count == 0) return ASL_STATUS_OK;

	outp = (pmsg_t **)calloc(query->count, sizeof(pmsg_t *));
	if (outp == NULL) return ASL_STATUS_NO_MEMORY;

	outt = (uint32_t *)calloc(query->count, sizeof(uint32_t));
	if (outt == NULL)
	{
		free(outp);
		return ASL_STATUS_NO_MEMORY;
	}

	*match = 1;

	for (i = 0; i < query->count; i++)
	{
		pm = NULL;
		outt[i] = query_to_pmsg(s, query->msg[i], &pm);
		if (outt[i] <= ASL_STATUS_FAILED)
		{
			if (pm != NULL) free_pmsg(pm);
			for (j = 0; j < i; j++) free_pmsg(outp[j]);
			free(outp);
			free(outt);
			return ASL_STATUS_NO_MEMORY;
		}

		outp[i] = pm;
	}

	*count = query->count;
	*qp = outp;
	*qtype = outt;
	return ASL_STATUS_OK;
}

static void
match_worker_cleanup(pmsg_t **ql, uint32_t *qt, uint32_t n, asl_msg_list_t **res)
{
	uint32_t i;

	if (ql != NULL)
	{
		for (i = 0; i < n; i++) free_pmsg(ql[i]);
		free(ql);
	}

	if (qt != NULL) free(qt);

	if (res != NULL)
	{
		for (i = 0; i < (*res)->count; i++) asl_free((*res)->msg[i]);
		free(*res);
	}
}

/*
 * Input to asl_legacy1_match is a list of queries.
 * A record in the store matches if it matches any query (i.e. query list is "OR"ed)
 *
 * If counting up (direction is positive) find first record with ID > start_id.
 * Else if counting down (direction is negative) find first record with ID < start_id.
 *
 * Set match flag on.
 * If any query is NULL, set match flog off (skips matching below).
 * Else if all queries only check "standard" keys, set std flag to on.
 *
 * If a query only tests equality, convert it to a pmsg_t.  The conversion routine
 * checks for string values that are NOT in the database.  If a string is not found,
 * the conversion fails and the query is markes as "never matches". Otherwise,
 * the query is marked "fast".
 *
 * If all queries are marked as "never matches", return NULL.
 *
 * match loop:
 *  fetch record (with std flag)
 *  if match flag is off, decode record and add it to result.
 *  else for each query:
 *    if query is NULL (shouldn't happen) decode record and add it to result.  Return to match loop.
 *    else if query never matches, ignore it.
 *    else if query is fast, use pmsg_match.  If it succeeds, decode record and add it to result.  Return to match loop.
 *    else decode record and use asl_cmp.  If it succeeds, add record to result.  Return to match loop.
 *
 * return results.
 */
static uint32_t
match_worker(asl_legacy1_t *s, asl_msg_list_t *query, asl_msg_list_t **res, uint64_t *last_id, uint64_t **idlist, uint32_t *idcount, uint64_t start_id, int32_t count, int32_t direction)
{
	uint32_t mx, si, slot, i, qcount, match, didmatch, status, *qtype;
	uint64_t xid;
	pmsg_t **qp, *iopmsg;
	asl_msg_t *iomsg;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if ((res == NULL) && (idlist == NULL)) return ASL_STATUS_INVALID_ARG;
	if (last_id == NULL) return ASL_STATUS_INVALID_ARG;
	if (idcount == NULL) return ASL_STATUS_INVALID_ARG;

	if (res != NULL) *res = NULL;
	if (idlist != NULL) *idlist = NULL;

	mx = 0;

	if (direction < 0) direction = -1;
	else direction = 1;

	si = ASL_INDEX_NULL;
	if ((direction == -1) && (start_id == ASL_REF_NULL)) si = s->slotlist_count;
	else si = slotlist_find(s, start_id, direction);

	si = next_search_slot(s, si, direction);
	if (si == ASL_INDEX_NULL) return ASL_STATUS_OK;
	if (si >= s->slotlist_count) return ASL_STATUS_FAILED;

	slot = s->slotlist[si].slot;

	status = query_list_to_pmsg_list(s, query, &match, &qp, &qtype, &qcount);
	if (status != ASL_STATUS_OK) return status;

	/*
	 * initialize result list if we've been asked to return messages
	 */
	if (res != NULL)
	{
		*res = (asl_msg_list_t *)calloc(1, sizeof(asl_msg_list_t));
		if (*res == NULL)
		{
			match_worker_cleanup(qp, qtype, qcount, NULL);
			return ASL_STATUS_NO_MEMORY;
		}
	}

	/*
	 * loop through records
	 */
	*idcount = 0;
	while ((count == 0) || (*idcount < count))
	{
		if (si == ASL_INDEX_NULL) break;
		if (si >= s->slotlist_count) break;

		slot = s->slotlist[si].slot;
		xid = s->slotlist[si].xid;

		*last_id = xid;

		iopmsg = NULL;
		iomsg = NULL;

		didmatch = 0;
		if (match == 0)
		{
			status = msg_match(s, Q_NULL, NULL, NULL, slot, &iopmsg, &iomsg, res, &didmatch);
			free_pmsg(iopmsg);
			if (didmatch == 0)
			{
				asl_free(iomsg);
				iomsg = NULL;
			}
			else
			{
				if (idlist != NULL)
				{
					if (*idlist == NULL) *idlist = (uint64_t *)calloc(1, sizeof(uint64_t));
					else *idlist = (uint64_t *)reallocf(*idlist, (*idcount + 1) * sizeof(uint64_t));
					if (*idlist == NULL) status = ASL_STATUS_NO_MEMORY;
					else (*idlist)[*idcount] = xid;
				}

				(*idcount)++;
			}

			if (status != ASL_STATUS_OK)
			{
				match_worker_cleanup(qp, qtype, qcount, res);
				return status;
			}
		}
		else
		{
			for (i = 0; i < qcount; i++)
			{
				status = msg_match(s, qtype[i], qp[i], query->msg[i], slot, &iopmsg, &iomsg, res, &didmatch);
				if (status != ASL_STATUS_OK)
				{
					free_pmsg(iopmsg);
					asl_free(iomsg);
					match_worker_cleanup(qp, qtype, qcount, res);
					return status;
				}

				if (didmatch == 1)
				{
					if (idlist != NULL)
					{
						if (*idlist == NULL) *idlist = (uint64_t *)calloc(1, sizeof(uint64_t));
						else *idlist = (uint64_t *)reallocf(*idlist, (*idcount + 1) * sizeof(uint64_t));
						if (*idlist == NULL)
						{
							match_worker_cleanup(qp, qtype, qcount, res);
							return ASL_STATUS_NO_MEMORY;
						}

						(*idlist)[*idcount] = xid;
					}

					(*idcount)++;
					break;
				}
			}

			free_pmsg(iopmsg);
			if ((didmatch == 0) || (res == NULL)) asl_free(iomsg);
		}

		si = next_search_slot(s, si, direction);
	}

	match_worker_cleanup(qp, qtype, qcount, NULL);
	return status;
}

uint32_t
asl_legacy1_match(asl_legacy1_t *s, asl_msg_list_t *query, asl_msg_list_t **res, uint64_t *last_id, uint64_t start_id, uint32_t count, int32_t direction)
{
	uint32_t idcount;

	idcount = 0;
	return match_worker(s, query, res, last_id, NULL, &idcount, start_id, count, direction);
}
