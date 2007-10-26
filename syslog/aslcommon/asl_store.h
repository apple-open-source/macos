#ifndef __ASL_STORE_H__
#define __ASL_STORE_H__

/*
 * ASL Database
 *
 * Log messages are stored in 80 byte records of the form:
 *
 * | 1    | 4    | 8  | 4    | 4    | 8    | 8    | 8      | 8        | 4     | 4   | 4   | 4   | 8       | 2     | 1    | (80 bytes)
 * | Type | Next | ID | RUID | RGID | Time | Host | Sender | Facility | LEVEL | PID | UID | GID | Message | Flags | Zero |
 *
 * If there are no additional key/value pairs in the message, Next will be zero.  If there are additional 
 * key/value pairs in the database, Next is a record number for a record with the format:
 *
 * | 1    | 4    | 4      | 8    | 8    | 8    | 8    | 8    | 8    | 8    | 8    | 7    | (80 bytes)
 * | Type | Next | Count  | Key1 | Val1 | Key2 | Val2 | Key3 | Val3 | Key4 | Val4 | Zero | 
 *
 * Additional records will be chained using the Next field, with the count field left zero.
 *
 * Strings stored in records of the form:
 *
 * | 1    | 4    | 8  | 4        | 4    | 4      | 55     | (80 bytes)
 * | Type | Next | ID | Refcount | Hash | Length | String |
 * 
 * If the string is longer than 55 bytes, Next is a record number for a record with the format: 
 *
 * | 1    | 4    | 75     | (80 bytes)
 * | Type | Next | String |
 * 
 * The first record (header) in the database has the format:
 *
 * | 12     | 4    | 8      | 56   | (80 bytes)
 * | Cookie | Vers | Max ID | Zero |
 * 
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <asl.h>

#define ASL_STORE_FLAG_READ_ONLY   0x00000001
#define ASL_STORE_FLAG_DEFER_SORT  0x00000002
#define ASL_STORE_FLAG_TIME_SORTED 0x00000004

#define ASL_MSG_FLAG_DO_NOT_ARCHIVE 0x0001
#define ASL_MSG_FLAG_READ_UID_SET   0x0002
#define ASL_MSG_FLAG_READ_GID_SET   0x0004

#define DB_VERSION 1
#define DB_RECORD_LEN 80

#define DB_HEADER_COOKIE_OFFSET 0
#define DB_HEADER_VERS_OFFSET 12
#define DB_HEADER_MAXID_OFFSET 16

#define DB_TYPE_NULL    255
#define DB_TYPE_EMPTY   0
#define DB_TYPE_HEADER  1
#define DB_TYPE_MESSAGE 2
#define DB_TYPE_KVLIST  3
#define DB_TYPE_STRING  4
#define DB_TYPE_STRCONT 5

#define ASL_REF_NULL 0xffffffffffffffffLL

#define ASL_STATUS_OK 0
#define ASL_STATUS_INVALID_ARG 1
#define ASL_STATUS_INVALID_STORE 2
#define ASL_STATUS_INVALID_STRING 3
#define ASL_STATUS_INVALID_ID 4
#define ASL_STATUS_INVALID_MESSAGE 5
#define ASL_STATUS_NOT_FOUND 6
#define ASL_STATUS_READ_FAILED 7
#define ASL_STATUS_WRITE_FAILED 8
#define ASL_STATUS_NO_MEMORY 9
#define ASL_STATUS_ACCESS_DENIED 10
#define ASL_STATUS_READ_ONLY 11
#define ASL_STATUS_FAILED 9999

#define ASL_KEY_FACILITY "Facility"
#define ASL_KEY_READ_UID "ReadUID"
#define ASL_KEY_READ_GID "ReadGID"
#define ASL_KEY_EXPIRE_TIME "ASLExpireTime"
#define ASL_KEY_MSG_ID "ASLMessageID"

#define STRING_CACHE_SIZE 100
#define RECORD_CACHE_SIZE 8

typedef struct
{
	uint8_t type;
	uint32_t slot;
	uint64_t xid;
	uint32_t hash;
} slot_info_t;

typedef struct
{
	uint32_t index;
	uint32_t refcount;
	char *str;
} string_cache_entry_t;

typedef struct
{
	uint32_t flags;
	uint32_t record_count;
	uint32_t message_count;
	uint32_t string_count;
	uint32_t empty_count;
	uint64_t next_id;
	uint64_t max_time;
	slot_info_t *slotlist;
	uint32_t slotlist_count;
	uint32_t slot_zero_count;
	string_cache_entry_t string_cache[STRING_CACHE_SIZE];
	char *rcache[RECORD_CACHE_SIZE];
	uint8_t rcache_state[RECORD_CACHE_SIZE];
	char *db_path;
	FILE *db;
} asl_store_t;

uint32_t asl_store_open(const char *path, uint32_t flags, asl_store_t **s);
uint32_t asl_store_close(asl_store_t *s);

uint32_t asl_store_save(asl_store_t *s, aslmsg msg, int32_t ruid, int32_t rgid, uint64_t *msgid);
uint32_t asl_store_fetch(asl_store_t *s, uint64_t msgid, int32_t ruid, int32_t rgid, aslmsg *msg);
uint32_t asl_store_remove(asl_store_t *s, uint64_t msgid);

uint32_t asl_store_match(asl_store_t *s, aslresponse query, aslresponse *res, uint64_t *last_id, uint64_t start_id, uint32_t count, int32_t direction, int32_t ruid, int32_t rgid);

uint32_t asl_store_prune(asl_store_t *s, aslresponse prune);
uint32_t asl_store_archive(asl_store_t *s, uint64_t cut_time, const char *archive);
uint32_t asl_store_truncate(asl_store_t *s, uint64_t max_size, const char *archive);
uint32_t asl_store_compact(asl_store_t *s);

const char *asl_store_error(uint32_t code);

#endif /*__ASL_STORE_H__*/
