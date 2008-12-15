/*
 * Copyright (c) 2007-2008 Apple Inc. All rights reserved.
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

#include <asl_core.h>
#include <asl_mini_memory.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <asl_private.h>

#define DEFAULT_MAX_RECORDS 256
#define MEM_STRING_HEADER_SIZE 8
#define CFLOG_LOCAL_TIME_KEY "CFLog Local Time"
#define CFLOG_THREAD_KEY "CFLog Thread"

#define forever for(;;)
extern time_t asl_parse_time(const char *str);
extern int asl_msg_cmp(asl_msg_t *a, asl_msg_t *b);

uint32_t
asl_mini_memory_statistics(asl_mini_memory_t *s, aslmsg *msg)
{
	aslmsg out;
	uint32_t i, n;
	uint64_t size;
	char str[256];

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (msg == NULL) return ASL_STATUS_INVALID_ARG;

	out = (aslmsg)calloc(1, sizeof(asl_msg_t));
	if (out == NULL) return ASL_STATUS_NO_MEMORY;

	size = sizeof(asl_mini_memory_t);
	size += ((s->record_count + 1) * sizeof(mini_mem_record_t));

	for (i = 0; i < s->string_count; i++)
	{
		size += MEM_STRING_HEADER_SIZE;
		if (((mini_mem_string_t *)s->string_cache[i])->str != NULL) size += (strlen(((mini_mem_string_t *)s->string_cache[i])->str) + 1);
	}

	snprintf(str, sizeof(str), "%llu", size);
	asl_set(out, "Size", str);

	n = 0;
	for (i = 0; i < s->record_count; i++) if (s->record[i]->mid != 0) n++;

	snprintf(str, sizeof(str), "%u", n);
	asl_set(out, "RecordCount", str);

	snprintf(str, sizeof(str), "%u", s->string_count);
	asl_set(out, "StringCount", str);

	*msg = out;
	return ASL_STATUS_OK;
}

uint32_t
asl_mini_memory_close(asl_mini_memory_t *s)
{
	uint32_t i;

	if (s == NULL) return ASL_STATUS_OK;

	if (s->record != NULL)
	{
		for (i = 0; i < s->record_count; i++)
		{
			if (s->record[i] != NULL) free(s->record[i]);
			s->record[i] = NULL;
		}

		free(s->record);
		s->record = NULL;
	}

	if (s->buffer_record != NULL) free(s->buffer_record);

	if (s->string_cache != NULL)
	{
		for (i = 0; i < s->string_count; i++)
		{
			if (s->string_cache[i] != NULL) free(s->string_cache[i]);
			s->string_cache[i] = NULL;
		}

		free(s->string_cache);
		s->string_cache = NULL;
	}

	free(s);

	return ASL_STATUS_OK;
}

uint32_t
asl_mini_memory_open(uint32_t max_records, asl_mini_memory_t **s)
{
	asl_mini_memory_t *out;
	uint32_t i;

	if (s == NULL) return ASL_STATUS_INVALID_ARG;

	if (max_records == 0) max_records = DEFAULT_MAX_RECORDS;

	out = calloc(1, sizeof(asl_mini_memory_t));
	if (out == NULL) return ASL_STATUS_NO_MEMORY;

	out->record_count = max_records;
	out->record = (mini_mem_record_t **)calloc(max_records, sizeof(mini_mem_record_t *));
	if (out->record == NULL)
	{
		free(out);
		return ASL_STATUS_NO_MEMORY;
	}

	for (i = 0; i < max_records; i++)
	{
		out->record[i] = (mini_mem_record_t *)calloc(1, sizeof(mini_mem_record_t));
		if (out->record[i] == NULL)
		{
			asl_mini_memory_close(out);
			return ASL_STATUS_NO_MEMORY;
		}
	}

	out->buffer_record = (mini_mem_record_t *)calloc(1, sizeof(mini_mem_record_t));
	if (out->buffer_record == NULL)
	{
		asl_mini_memory_close(out);
		return ASL_STATUS_NO_MEMORY;
	}

	out->next_id = 1;

	*s = out;
	return ASL_STATUS_OK;
}

static mini_mem_string_t *
mem_string_new(const char *str, uint32_t len, uint32_t hash)
{
	mini_mem_string_t *out;
	size_t ss;

	if (str == NULL) return NULL;

	ss = MEM_STRING_HEADER_SIZE + len + 1;
	out = (mini_mem_string_t *)calloc(1, ss);
	if (out == NULL) return NULL;

	out->hash = hash;
	out->refcount = 1;
	memcpy(out->str, str, len);

	return out;
}

/*
 * Find the first hash greater than or equal to a given hash in the string cache.
 * Return s->string_count if hash is greater that or equal to last hash in the string cache.
 * Caller must check if the hashes match or not.
 *
 * This routine is used both to find strings in the cache and to determine where to insert
 * new strings.  Note that the caller needs to do extra work after calling this routine.
 */
static uint32_t
asl_mini_memory_string_cache_search_hash(asl_mini_memory_t *s, uint32_t hash)
{
	uint32_t top, bot, mid, range;
	mini_mem_string_t *ms;

	if (s->string_count == 0) return 0;
	if (s->string_count == 1)
	{
		ms = (mini_mem_string_t *)s->string_cache[0];
		if (hash < ms->hash) return 0;
		return 1;
	}

	top = s->string_count - 1;
	bot = 0;
	mid = top / 2;

	range = top - bot;
	while (range > 1)
	{
		ms = (mini_mem_string_t *)s->string_cache[mid];

		if (hash == ms->hash)
		{
			while (mid > 0)
			{
				ms = (mini_mem_string_t *)s->string_cache[mid - 1];
				if (hash != ms->hash) break;
				mid--;
			}

			return mid;
		}
		else
		{
			ms = (mini_mem_string_t *)s->string_cache[mid];
			if (hash < ms->hash) top = mid;
			else bot = mid;
		}

		range = top - bot;
		mid = bot + (range / 2);
	}

	ms = (mini_mem_string_t *)s->string_cache[bot];
	if (hash <= ms->hash) return bot;

	ms = (mini_mem_string_t *)s->string_cache[top];
	if (hash <= ms->hash) return top;

	return s->string_count;
}

/*
 * Search the string cache.
 * If the string is in the cache, increment refcount and return it.
 * If the string is not in cache and create flag is on, create a new string.
 * Otherwise, return NULL.
 */
static mini_mem_string_t *
asl_mini_memory_string_retain(asl_mini_memory_t *s, const char *str, int create)
{
	uint32_t i, where, hash, len;

	if (s == NULL) return NULL;
	if (str == NULL) return NULL;
	len = strlen(str);

	/* check the cache */
	hash = asl_core_string_hash(str, len);
	where = asl_mini_memory_string_cache_search_hash(s, hash);

	/* asl_mini_memory_string_cache_search_hash just tells us where to look */
	if (where < s->string_count)
	{
		while (((mini_mem_string_t *)(s->string_cache[where]))->hash == hash)
		{
			if (!strcmp(str, ((mini_mem_string_t *)(s->string_cache[where]))->str))
			{
				((mini_mem_string_t *)(s->string_cache[where]))->refcount++;
				return s->string_cache[where];
			}

			where++;
		}
	}

	/* not found */
	if (create == 0) return NULL;

	/* create a new mini_mem_string_t and insert into the cache at index 'where' */
	if (s->string_count == 0)
	{
		s->string_cache = (void **)calloc(1, sizeof(void *));
	}
	else
	{
		s->string_cache = (void **)reallocf(s->string_cache, (s->string_count + 1) * sizeof(void *));
		for (i = s->string_count; i > where; i--) s->string_cache[i] = s->string_cache[i - 1];
	}

	if (s->string_cache == NULL)
	{
		s->string_count = 0;
		return NULL;
	}

	s->string_count++;
	s->string_cache[where] = mem_string_new(str, len, hash);

	return s->string_cache[where];
}

static uint32_t
asl_mini_memory_string_release(asl_mini_memory_t *s, mini_mem_string_t *m)
{
	uint32_t i, where;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (m == NULL) return ASL_STATUS_OK;

	if (m->refcount > 0) m->refcount--;
	if (m->refcount > 0) return ASL_STATUS_OK;

	where = asl_mini_memory_string_cache_search_hash(s, m->hash);
	if (((mini_mem_string_t *)(s->string_cache[where]))->hash != m->hash) return ASL_STATUS_OK;

	while (s->string_cache[where] != m)
	{
		if (((mini_mem_string_t *)(s->string_cache[where]))->hash != m->hash) return ASL_STATUS_OK;

		where++;
		if (where >= s->string_count) return ASL_STATUS_OK;
	}

	for (i = where + 1; i < s->string_count; i++) s->string_cache[i - 1] = s->string_cache[i];

	free(m);
	s->string_count--;

	if (s->string_count == 0)
	{
		free(s->string_cache);
		s->string_cache = NULL;
		return ASL_STATUS_OK;
	}

	s->string_cache = (void **)reallocf(s->string_cache, s->string_count * sizeof(void *));
	if (s->string_cache == NULL)
	{
		s->string_count = 0;
		return ASL_STATUS_NO_MEMORY;
	}

	return ASL_STATUS_OK;
}

/*
 * Release all a record's strings and reset it's values
 */
static void
asl_mini_memory_record_clear(asl_mini_memory_t *s, mini_mem_record_t *r)
{
	uint32_t i;

	if (s == NULL) return;
	if (r == NULL) return;

	asl_mini_memory_string_release(s, r->sender);
	asl_mini_memory_string_release(s, r->facility);
	asl_mini_memory_string_release(s, r->message);

	for (i = 0; i < r->kvcount; i++) asl_mini_memory_string_release(s, r->kvlist[i]);

	if (r->kvlist != NULL) free(r->kvlist);
	memset(r, 0, sizeof(mini_mem_record_t));
}

static void
asl_mini_memory_record_free(asl_mini_memory_t *s, mini_mem_record_t *r)
{
	asl_mini_memory_record_clear(s, r);
	free(r);
}

/*
 * Encode an aslmsg as a record structure.
 * Creates and caches strings.
 */
static uint32_t
asl_mini_memory_message_encode(asl_mini_memory_t *s, asl_msg_t *msg, mini_mem_record_t *r)
{
	uint32_t i;
	mini_mem_string_t *k, *v;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (msg == NULL) return ASL_STATUS_INVALID_MESSAGE;
	if (r == NULL) return ASL_STATUS_INVALID_ARG;

	memset(r, 0, sizeof(mini_mem_record_t));

	r->flags = 0;
	r->level = ASL_LEVEL_DEBUG;
	r->pid = -1;
	r->time = (uint64_t)-1;

	for (i = 0; i < msg->count; i++)
	{
		if (msg->key[i] == NULL) continue;

		else if (!strcmp(msg->key[i], ASL_KEY_TIME))
		{
			if (msg->val[i] != NULL) r->time = asl_parse_time(msg->val[i]);
		}
		else if (!strcmp(msg->key[i], ASL_KEY_SENDER))
		{
			if (msg->val[i] != NULL) r->sender = asl_mini_memory_string_retain(s, msg->val[i], 1);
		}
		else if (!strcmp(msg->key[i], ASL_KEY_PID))
		{
			if (msg->val[i] != NULL) r->pid = atoi(msg->val[i]);
		}
		else if (!strcmp(msg->key[i], ASL_KEY_LEVEL))
		{
			if (msg->val[i] != NULL) r->level = atoi(msg->val[i]);
		}
		else if (!strcmp(msg->key[i], ASL_KEY_MSG))
		{
			if (msg->val[i] != NULL) r->message = asl_mini_memory_string_retain(s, msg->val[i], 1);
		}
		else if (!strcmp(msg->key[i], ASL_KEY_FACILITY))
		{
			if (msg->val[i] != NULL) r->facility = asl_mini_memory_string_retain(s, msg->val[i], 1);
		}
		else if (!strcmp(msg->key[i], ASL_KEY_MSG_ID))
		{
			/* Ignore */
			continue;
		}
		else if (!strcmp(msg->key[i], ASL_KEY_TIME_NSEC))
		{
			/* Ignore */
			continue;
		}
		else if (!strcmp(msg->key[i], ASL_KEY_HOST))
		{
			/* Ignore */
			continue;
		}
		else if (!strcmp(msg->key[i], ASL_KEY_REF_PID))
		{
			/* Ignore */
			continue;
		}
		else if (!strcmp(msg->key[i], ASL_KEY_REF_PROC))
		{
			/* Ignore */
			continue;
		}
		else if (!strcmp(msg->key[i], ASL_KEY_SESSION))
		{
			/* Ignore */
			continue;
		}
		else if (!strcmp(msg->key[i], ASL_KEY_UID))
		{
			/* Ignore */
			continue;
		}
		else if (!strcmp(msg->key[i], ASL_KEY_GID))
		{
			/* Ignore */
			continue;
		}
		else if (!strcmp(msg->key[i], ASL_KEY_READ_UID))
		{
			/* Ignore */
			continue;
		}
		else if (!strcmp(msg->key[i], ASL_KEY_READ_GID))
		{
			/* Ignore */
			continue;
		}
		else if (!strcmp(msg->key[i], CFLOG_LOCAL_TIME_KEY))
		{
			/* Ignore */
			continue;
		}
		else if (!strcmp(msg->key[i], CFLOG_THREAD_KEY))
		{
			/* Ignore */
			continue;
		}
		else
		{
			k = asl_mini_memory_string_retain(s, msg->key[i], 1);
			if (k == NULL) continue;

			v = NULL;
			if (msg->val[i] != NULL) v = asl_mini_memory_string_retain(s, msg->val[i], 1);

			if (r->kvcount == 0)
			{
				r->kvlist = (mini_mem_string_t **)calloc(2, sizeof(mini_mem_string_t *));
			}
			else
			{
				r->kvlist = (mini_mem_string_t **)realloc(r->kvlist, (r->kvcount + 2) * sizeof(mini_mem_string_t *));
			}

			if (r->kvlist == NULL)
			{
				asl_mini_memory_record_clear(s, r);
				return ASL_STATUS_NO_MEMORY;
			}

			r->kvlist[r->kvcount++] = k;
			r->kvlist[r->kvcount++] = v;
		}
	}

	return ASL_STATUS_OK;
}

uint32_t
asl_mini_memory_save(asl_mini_memory_t *s, aslmsg msg, uint64_t *mid)
{
	uint32_t status;
	mini_mem_record_t *t;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (s->buffer_record == NULL) return ASL_STATUS_INVALID_STORE;

	/* asl_mini_memory_message_encode creates and caches strings */
	status = asl_mini_memory_message_encode(s, msg, s->buffer_record);
	if (status != ASL_STATUS_OK) return status;

	s->buffer_record->mid = s->next_id;
	s->next_id++;

	/* clear the first record */
	t = s->record[s->record_first];
	asl_mini_memory_record_clear(s, t);

	/* add the new record to the record list (swap in the buffer record) */
	s->record[s->record_first] = s->buffer_record;
	s->buffer_record = t;

	/* record list is a circular queue */
	s->record_first++;
	if (s->record_first >= s->record_count) s->record_first = 0;

	*mid = s->buffer_record->mid;

	return status;
}

/*
 * Decodes a record structure.
 */
static uint32_t
asl_mini_memory_message_decode(asl_mini_memory_t *s, mini_mem_record_t *r, asl_msg_t **out)
{
	uint32_t i, n;
	asl_msg_t *msg;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (r == NULL) return ASL_STATUS_INVALID_ARG;
	if (out == NULL) return ASL_STATUS_INVALID_ARG;

	*out = NULL;

	msg = (asl_msg_t *)calloc(1, sizeof(asl_msg_t));
	if (msg == NULL) return ASL_STATUS_NO_MEMORY;

	msg->type = ASL_TYPE_MSG;
	/* Level and Message ID are always set */
	msg->count = 2;

	if (r->time != (uint64_t)-1) msg->count++;
	if (r->sender != NULL) msg->count++;
	if (r->facility != NULL) msg->count++;
	if (r->pid != -1) msg->count++;
	if (r->message != NULL) msg->count++;

	msg->count += (r->kvcount / 2);

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

	/* Message ID */
	msg->key[n] = strdup(ASL_KEY_MSG_ID);
	if (msg->key[n] == NULL)
	{
		asl_free(msg);
		return ASL_STATUS_NO_MEMORY;
	}

	asprintf(&(msg->val[n]), "%lu", r->mid);
	if (msg->val[n] == NULL)
	{
		asl_free(msg);
		return ASL_STATUS_NO_MEMORY;
	}
	n++;

	/* Level */
	msg->key[n] = strdup(ASL_KEY_LEVEL);
	if (msg->key[n] == NULL)
	{
		asl_free(msg);
		return ASL_STATUS_NO_MEMORY;
	}

	asprintf(&(msg->val[n]), "%u", r->level);
	if (msg->val[n] == NULL)
	{
		asl_free(msg);
		return ASL_STATUS_NO_MEMORY;
	}
	n++;

	/* Time */
	if (r->time != (uint64_t)-1)
	{
		msg->key[n] = strdup(ASL_KEY_TIME);
		if (msg->key[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}

		asprintf(&(msg->val[n]), "%llu", r->time);
		if (msg->val[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}
		n++;
	}

	/* Sender */
	if (r->sender != NULL)
	{
		msg->key[n] = strdup(ASL_KEY_SENDER);
		if (msg->key[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}

		msg->val[n] = strdup(r->sender->str);
		n++;
	}

	/* Facility */
	if (r->facility != NULL)
	{
		msg->key[n] = strdup(ASL_KEY_FACILITY);
		if (msg->key[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}

		msg->val[n] = strdup(r->facility->str);
		n++;
	}

	/* PID */
	if (r->pid != -1)
	{
		msg->key[n] = strdup(ASL_KEY_PID);
		if (msg->key[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}

		asprintf(&(msg->val[n]), "%d", r->pid);
		if (msg->val[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}
		n++;
	}

	/* Message */
	if (r->message != NULL)
	{
		msg->key[n] = strdup(ASL_KEY_MSG);
		if (msg->key[n] == NULL)
		{
			asl_free(msg);
			return ASL_STATUS_NO_MEMORY;
		}

		msg->val[n] = strdup(r->message->str);
		n++;
	}

	/* Key - Value List */
	for (i = 0; i < r->kvcount; i++)
	{
		if ((r->kvlist[i] != NULL) && (r->kvlist[i]->str != NULL)) msg->key[n] = strdup(r->kvlist[i]->str);
		i++;
		if ((r->kvlist[i] != NULL) && (r->kvlist[i]->str != NULL)) msg->val[n] = strdup(r->kvlist[i]->str);
		n++;
	}

	*out = msg;
	return ASL_STATUS_OK;
}

uint32_t
asl_mini_memory_fetch(asl_mini_memory_t *s, uint64_t mid, aslmsg *msg)
{
	uint32_t i;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (msg == NULL) return ASL_STATUS_INVALID_ARG;

	for (i = 0; i < s->record_count; i++)
	{
		if (s->record[i]->mid == 0) break;
		if (s->record[i]->mid == mid) return asl_mini_memory_message_decode(s, s->record[i], msg);
	}

	return ASL_STATUS_INVALID_ID;
}

static mini_mem_record_t *
asl_mini_memory_query_to_record(asl_mini_memory_t *s, asl_msg_t *q, uint32_t *type)
{
	mini_mem_record_t *out;
	uint32_t i, j;
	mini_mem_string_t *key, *val;

	if (type == NULL) return NULL;

	if (s == NULL)
	{
		*type = ASL_QUERY_MATCH_ERROR;
		return NULL;
	}

	/* NULL query matches anything */
	*type = ASL_QUERY_MATCH_TRUE;
	if (q == NULL) return NULL;
	if (q->count == 0) return NULL;


	/* we can only do fast match on equality tests */
	*type = ASL_QUERY_MATCH_SLOW;
	if (q->op != NULL)
	{
		for (i = 0; i < q->count; i++) if (q->op[i] != ASL_QUERY_OP_EQUAL) return NULL;
	}

	out = (mini_mem_record_t *)calloc(1, sizeof(mini_mem_record_t));
	if (out == NULL)
	{
		*type = ASL_QUERY_MATCH_ERROR;
		return NULL;
	}

	for (i = 0; i < q->count; i++)
	{
		if (q->key[i] == NULL) continue;

		else if (!strcmp(q->key[i], ASL_KEY_MSG_ID))
		{
			if (q->val[i] == NULL) continue;

			if (*type & ASL_QUERY_MATCH_MSG_ID)
			{
				asl_mini_memory_record_free(s, out);
				*type = ASL_QUERY_MATCH_SLOW;
				return NULL;
			}

			*type |= ASL_QUERY_MATCH_MSG_ID;
			out->mid = atoll(q->val[i]);
		}
		else if (!strcmp(q->key[i], ASL_KEY_TIME))
		{
			if (q->val[i] == NULL) continue;

			if (*type & ASL_QUERY_MATCH_TIME)
			{
				asl_mini_memory_record_free(s, out);
				*type = ASL_QUERY_MATCH_SLOW;
				return NULL;
			}

			*type |= ASL_QUERY_MATCH_TIME;
			out->time = asl_parse_time(q->val[i]);
		}
		else if (!strcmp(q->key[i], ASL_KEY_LEVEL))
		{
			if (q->val[i] == NULL) continue;

			if (*type & ASL_QUERY_MATCH_LEVEL)
			{
				asl_mini_memory_record_free(s, out);
				*type = ASL_QUERY_MATCH_SLOW;
				return NULL;
			}

			*type |= ASL_QUERY_MATCH_LEVEL;
			out->level = atoi(q->val[i]);
		}
		else if (!strcmp(q->key[i], ASL_KEY_PID))
		{
			if (q->val[i] == NULL) continue;

			if (*type & ASL_QUERY_MATCH_PID)
			{
				asl_mini_memory_record_free(s, out);
				*type = ASL_QUERY_MATCH_SLOW;
				return NULL;
			}

			*type |= ASL_QUERY_MATCH_PID;
			out->pid = atoi(q->val[i]);
		}
		else if (!strcmp(q->key[i], ASL_KEY_SENDER))
		{
			if (q->val[i] == NULL) continue;

			if (*type & ASL_QUERY_MATCH_SENDER)
			{
				asl_mini_memory_record_free(s, out);
				*type = ASL_QUERY_MATCH_SLOW;
				return NULL;
			}

			*type |= ASL_QUERY_MATCH_SENDER;
			out->sender = asl_mini_memory_string_retain(s, q->val[i], 0);
			if (out->sender == NULL)
			{
				asl_mini_memory_record_free(s, out);
				*type = ASL_QUERY_MATCH_FALSE;
				return NULL;
			}
		}
		else if (!strcmp(q->key[i], ASL_KEY_FACILITY))
		{
			if (q->val[i] == NULL) continue;

			if (*type & ASL_QUERY_MATCH_FACILITY)
			{
				asl_mini_memory_record_free(s, out);
				*type = ASL_QUERY_MATCH_SLOW;
				return NULL;
			}

			*type |= ASL_QUERY_MATCH_FACILITY;
			out->facility = asl_mini_memory_string_retain(s, q->val[i], 0);
			if (out->facility == NULL)
			{
				asl_mini_memory_record_free(s, out);
				*type = ASL_QUERY_MATCH_FALSE;
				return NULL;
			}
		}
		else if (!strcmp(q->key[i], ASL_KEY_MSG))
		{
			if (q->val[i] == NULL) continue;

			if (*type & ASL_QUERY_MATCH_MESSAGE)
			{
				asl_mini_memory_record_free(s, out);
				*type = ASL_QUERY_MATCH_SLOW;
				return NULL;
			}

			*type |= ASL_QUERY_MATCH_MESSAGE;
			out->message = asl_mini_memory_string_retain(s, q->val[i], 0);
			if (out->message == NULL)
			{
				asl_mini_memory_record_free(s, out);
				*type = ASL_QUERY_MATCH_FALSE;
				return NULL;
			}
		}
		else
		{
			key = asl_mini_memory_string_retain(s, q->key[i], 0);
			if (key == NULL)
			{
				asl_mini_memory_record_free(s, out);
				*type = ASL_QUERY_MATCH_FALSE;
				return NULL;
			}

			for (j = 0; j < out->kvcount; j += 2)
			{
				if (out->kvlist[j] == key)
				{
					asl_mini_memory_record_free(s, out);
					*type = ASL_QUERY_MATCH_SLOW;
					return NULL;
				}
			}

			val = asl_mini_memory_string_retain(s, q->val[i], 0);

			if (out->kvcount == 0)
			{
				out->kvlist = (mini_mem_string_t **)calloc(2, sizeof(mini_mem_string_t *));
			}
			else
			{
				out->kvlist = (mini_mem_string_t **)realloc(out->kvlist, (out->kvcount + 2) * sizeof(mini_mem_string_t *));
			}

			if (out->kvlist == NULL)
			{
				asl_mini_memory_record_free(s, out);
				*type = ASL_QUERY_MATCH_ERROR;
				return NULL;
			}

			out->kvlist[out->kvcount++] = key;
			out->kvlist[out->kvcount++] = val;
		}
	}

	return out;
}

static uint32_t
asl_mini_memory_fast_match(asl_mini_memory_t *s, mini_mem_record_t *r, uint32_t qtype, mini_mem_record_t *q)
{
	uint32_t i, j;

	if (s == NULL) return 0;
	if (r == NULL) return 0;
	if (q == NULL) return 1;

	if ((qtype & ASL_QUERY_MATCH_MSG_ID) && (q->mid != r->mid)) return 0;
	if ((qtype & ASL_QUERY_MATCH_TIME) && (q->time != r->time)) return 0;
	if ((qtype & ASL_QUERY_MATCH_LEVEL) && (q->level != r->level)) return 0;
	if ((qtype & ASL_QUERY_MATCH_PID) && (q->pid != r->pid)) return 0;
	if ((qtype & ASL_QUERY_MATCH_SENDER) && (q->sender != r->sender)) return 0;
	if ((qtype & ASL_QUERY_MATCH_FACILITY) && (q->facility != r->facility)) return 0;
	if ((qtype & ASL_QUERY_MATCH_MESSAGE) && (q->message != r->message)) return 0;

	for (i = 0; i < q->kvcount; i += 2)
	{
		for (j = 0; j < r->kvcount; j += 2)
		{
			if (q->kvlist[i] == r->kvlist[j])
			{
				if (q->kvlist[i + 1] == r->kvlist[j + 1]) break;
				return 0;
			}
		}

		if (j >= r->kvcount) return 0;
	}

	return 1;
}

static uint32_t
asl_mini_memory_slow_match(asl_mini_memory_t *s, mini_mem_record_t *r, mini_mem_record_t *q, asl_msg_t *rawq)
{
	asl_msg_t *rawm;
	uint32_t status;

	rawm = NULL;
	status = asl_mini_memory_message_decode(s, r, &rawm);
	if (status != ASL_STATUS_OK) return 0;

	status = 0;
	if (asl_msg_cmp(rawq, rawm) != 0) status = 1;
	asl_free(rawm);
	return status;
}

uint32_t
asl_mini_memory_match(asl_mini_memory_t *s, aslresponse query, aslresponse *res, uint64_t *last_id, uint64_t start_id, uint32_t count, int32_t direction)
{
	uint32_t status, i, where, start, j, do_match, did_match, rescount, *qtype;
	mini_mem_record_t **qp;
	asl_msg_t *m;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (res == NULL) return ASL_STATUS_INVALID_ARG;

	do_match = 1;
	qp = NULL;
	qtype = NULL;
	rescount = 0;

	if ((query == NULL) || ((query != NULL) && (query->count == 0)))
	{
		do_match = 0;
	}
	else
	{
		qp = (mini_mem_record_t **)calloc(query->count, sizeof(mini_mem_record_t *));
		if (qp == NULL) return ASL_STATUS_NO_MEMORY;

		qtype = (uint32_t *)calloc(query->count, sizeof(uint32_t));
		if (qtype == NULL)
		{
			free(qp);
			return ASL_STATUS_NO_MEMORY;
		}

		do_match = 0;
		for (i = 0; i < query->count; i++)
		{
			qp[i] = asl_mini_memory_query_to_record(s, query->msg[i], &(qtype[i]));
			if (qtype[i] == ASL_QUERY_MATCH_ERROR)
			{
				for (j = 0; j < i; j++) asl_mini_memory_record_free(s, qp[j]);
				free(qp);
				free(qtype);
				return ASL_STATUS_FAILED;
			}

			if (qtype[i] != ASL_QUERY_MATCH_TRUE) do_match = 1;
		}
	}

	for (i = 0; i < s->record_count; i++)
	{
		if (direction >= 0)
		{
			where = (s->record_first + i) % s->record_count;
			if (s->record[where]->mid == 0) continue;
			if (s->record[where]->mid >= start_id) break;
		}
		else
		{
			where = ((s->record_count - (i + 1)) + s->record_first) % s->record_count;
			if (s->record[where]->mid == 0) continue;
			if (s->record[where]->mid <= start_id) break;
		}
	}

	if (i >= s->record_count) return ASL_STATUS_OK;

	start = where;

	/* 
	 * loop through records
	 */
	for (i = 0; i < s->record_count; i++)
	{
		if (s->record[where]->mid == 0)
		{
			if (direction >= 0)
			{
				where++;
				if (where >= s->record_count) where = 0;
			}
			else
			{
				if (where == 0) where = s->record_count - 1;
				else where--;
			}

			if (where == s->record_first) break;
			continue;
		}

		s->record[where]->flags &= ASL_MINI_MSG_FLAG_SEARCH_CLEAR;
		*last_id = s->record[where]->mid;
		did_match = 1;

		if (do_match != 0)
		{
			did_match = 0;

			for (j = 0; (j < query->count) && (did_match == 0); j++)
			{
				if (qtype[j] == ASL_QUERY_MATCH_TRUE)
				{
					did_match = 1;
				}
				else if (qtype[j] == ASL_QUERY_MATCH_SLOW)
				{
					did_match = asl_mini_memory_slow_match(s, s->record[where], qp[j], query->msg[j]);
				}
				else
				{
					did_match = asl_mini_memory_fast_match(s, s->record[where], qtype[j], qp[j]);
				}
			}
		}

		if (did_match == 1)
		{
			s->record[where]->flags |= ASL_MINI_MSG_FLAG_SEARCH_MATCH;
			rescount++;
			if ((count != 0) && (rescount >= count)) break;
		}

		if (direction >= 0)
		{
			where++;
			if (where >= s->record_count) where = 0;
		}
		else
		{
			if (where == 0) where = s->record_count - 1;
			else where--;
		}

		if (where == s->record_first) break;
	}

	if (query != NULL)
	{
		for (i = 0; i < query->count; i++) asl_mini_memory_record_free(s, qp[i]);
		free(qp);
		free(qtype);
	}

	*res = NULL;
	if (rescount == 0) return ASL_STATUS_OK;

	*res = (asl_msg_list_t *)calloc(1, sizeof(asl_msg_list_t));
	if (*res == NULL) return ASL_STATUS_NO_MEMORY;

	(*res)->count = rescount;

	(*res)->msg = (asl_msg_t **)calloc(rescount, sizeof(asl_msg_t *));
	if ((*res)->msg == NULL)
	{
		free(*res);
		*res = NULL;
		return ASL_STATUS_NO_MEMORY;
	}

	where = start;
	forever
	{
		if (s->record[where]->flags & ASL_MINI_MSG_FLAG_SEARCH_MATCH)
		{
			s->record[where]->flags &= ASL_MINI_MSG_FLAG_SEARCH_CLEAR;

			status = asl_mini_memory_message_decode(s, s->record[where], &m);
			if (status != ASL_STATUS_OK)
			{
				aslresponse_free(*res);
				*res = NULL;
				return status;
			}

			(*res)->msg[(*res)->curr++] = m;
			if ((*res)->curr == rescount) break;
		}

		if (direction >= 0)
		{
			where++;
			if (where >= s->record_count) where = 0;
		}
		else
		{
			if (where == 0) where = s->record_count - 1;
			else where--;
		}

		if (where == s->record_first) break;
	}

	(*res)->curr = 0;
	return ASL_STATUS_OK;
}
