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

#ifndef __asl_mini_memory_H__
#define __asl_mini_memory_H__
#include <stdint.h>
#include <asl.h>

#define ASL_MINI_MSG_FLAG_SEARCH_MATCH 0x80
#define ASL_MINI_MSG_FLAG_SEARCH_CLEAR 0x7f

typedef struct
{
	uint32_t hash;
	uint32_t refcount;
	char str[];
} mini_mem_string_t;

typedef struct
{
	uint32_t mid;
	uint64_t time;
	uint8_t level;
	uint8_t flags;
	uint32_t pid;
	uint32_t uid;
	uint32_t gid;
	uint32_t kvcount;
	mini_mem_string_t *sender;
	mini_mem_string_t *facility;
	mini_mem_string_t *message;
	mini_mem_string_t **kvlist;
} mini_mem_record_t;

typedef struct
{
	uint32_t next_id;
	uint32_t string_count;
	void **string_cache;
	uint32_t record_count;
	uint32_t record_first;
	mini_mem_record_t **record;
	mini_mem_record_t *buffer_record;
} asl_mini_memory_t;

uint32_t asl_mini_memory_open(uint32_t max_records, asl_mini_memory_t **s);
uint32_t asl_mini_memory_close(asl_mini_memory_t *s);
uint32_t asl_mini_memory_statistics(asl_mini_memory_t *s, aslmsg *msg);

uint32_t asl_mini_memory_save(asl_mini_memory_t *s, aslmsg msg, uint64_t *mid);
uint32_t asl_mini_memory_fetch(asl_mini_memory_t *s, uint64_t mid, aslmsg *msg);

uint32_t asl_mini_memory_match(asl_mini_memory_t *s, aslresponse query, aslresponse *res, uint64_t *last_id, uint64_t start_id, uint32_t count, int32_t direction);

#endif __asl_mini_memory_H__
