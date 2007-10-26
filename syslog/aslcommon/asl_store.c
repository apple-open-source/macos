#include <asl_store.h>
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

#define ARCHIVE_DELETE_VS_COPY_PERCENT 5

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

static void
_asl_put_16(uint16_t i, char *h)
{
	uint16_t x;

	x = htons(i);
	memcpy(h, &x, 2);
}

static uint32_t
_asl_get_32(char *h)
{
	uint32_t x;

	memcpy(&x, h, 4);
	return ntohl(x);
}

static void
_asl_put_32(uint32_t i, char *h)
{
	uint32_t x;

	x = htonl(i);
	memcpy(h, &x, 4);
}

static uint64_t
_asl_get_64(char *h)
{
	uint64_t x;

	memcpy(&x, h, 8);
	return _asl_ntohq(x);
}

static void
_asl_put_64(uint64_t i, char *h)
{
	uint64_t x;

	x = _asl_htonq(i);
	memcpy(h, &x, 8);
}

#define header_get_next(h)		_asl_get_32(h +  1)
#define header_get_id(h)		_asl_get_64(h +  5)
#define header_get_refcount(h)	_asl_get_32(h + 13)
#define header_get_hash(h)		_asl_get_32(h + 17)

#define header_put_next(i, h)		_asl_put_32(i, h +  1)
#define header_put_id(i, h)			_asl_put_64(i, h +  5)
#define header_put_refcount(i, h)	_asl_put_32(i, h + 13)
#define header_put_hash(i, h)		_asl_put_32(i, h + 17)

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
	slot_info_t *ai, *bi;

	if (a == NULL)
	{
		if (b == NULL) return 0;
		return -1;
	}

	if (b == NULL) return 1;

	ai = (slot_info_t *)a;
	bi = (slot_info_t *)b;

	if (ai->xid < bi->xid) return -1;

	if (ai->xid == bi->xid)
	{
		if (ai->slot < bi->slot) return -1;
		if (ai->slot == bi->slot) return 0;
		return 1;
	}

	return 1;
}

/* find a slot (with xid 0) in the slot list */
static uint32_t
slotlist_find_xid0_slot(asl_store_t *s, uint32_t slot)
{
	uint32_t top, bot, mid, range;

	if (s == NULL) return ASL_INDEX_NULL;
	if (s->slot_zero_count == 0) return ASL_INDEX_NULL;

	top = s->slot_zero_count - 1;
	bot = 0;
	mid = top / 2;

	range = top - bot;
	while (range > 1)
	{
		if (slot == s->slotlist[mid].slot) return mid;
		else if (slot < s->slotlist[mid].slot) top = mid;
		else bot = mid;

		range = top - bot;
		mid = bot + (range / 2);
	}

	if (slot == s->slotlist[top].slot) return top;
	if (slot == s->slotlist[bot].slot) return bot;

	return ASL_INDEX_NULL;
}

/* find an xid in the slot list */
static uint32_t
slotlist_find(asl_store_t *s, uint64_t xid, uint32_t slot, int32_t direction)
{
	uint32_t top, bot, mid, range;

	if (s == NULL) return ASL_INDEX_NULL;
	if (s->slotlist_count == 0) return ASL_INDEX_NULL;

	/* special case for xid 0: binary search for slot */
	if (xid == 0) return slotlist_find_xid0_slot(s, slot);

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
slotlist_insert(asl_store_t *s, uint8_t type, uint32_t slot, uint64_t xid, uint32_t hash)
{
	int i, j, k;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (s->slotlist_count == 0)
	{
		s->slotlist = (slot_info_t *)calloc(1, sizeof(slot_info_t));
		if (s->slotlist == NULL) return ASL_STATUS_NO_MEMORY;

		s->slotlist[0].type = type;
		s->slotlist[0].slot = slot;
		s->slotlist[0].xid  = xid;
		s->slotlist[0].hash = hash;
		s->slotlist_count = 1;
		if (xid == 0) s->slot_zero_count = 1;
		return ASL_STATUS_OK;
	}

	s->slotlist = (slot_info_t *)reallocf(s->slotlist, (s->slotlist_count + 1) * sizeof(slot_info_t));
	if (s->slotlist == NULL)
	{
		s->slotlist_count = 0;
		s->slot_zero_count = 0;
		return ASL_STATUS_NO_MEMORY;
	}

	/*
	 * slotlist is sorted in increasing order by xid
	 * there may be multiple xid 0 entries (empty slots) which are further sorted by slot
	 */
	if (xid == 0)
	{
		/* update empty count */
		s->slot_zero_count++;

		for (i = 0; (i < s->slotlist_count) && (s->slotlist[i].xid == 0); i++)
		{
			if (slot <= s->slotlist[i].slot) break;
		}
	}
	else
	{
		i = s->slotlist_count - 1;
		if (xid > s->slotlist[i].xid)
		{
			/* append XID at end of slotlist */
			i++;
		}
		else
		{
			/* usually we are adding records, so it's likely that the new xid will be large */
			for (i = s->slotlist_count; i > 0; i--)
			{
				if (xid > s->slotlist[i - 1].xid) break;
			}
		}
	}

	for (j = s->slotlist_count; j > i; j--)
	{
		k = j - 1;
		s->slotlist[j].type = s->slotlist[k].type;
		s->slotlist[j].slot = s->slotlist[k].slot;
		s->slotlist[j].xid  = s->slotlist[k].xid;
		s->slotlist[j].hash = s->slotlist[k].hash;
	}

	s->slotlist[i].type = type;
	s->slotlist[i].slot = slot;
	s->slotlist[i].xid  = xid;
	s->slotlist[i].hash = hash;
	s->slotlist_count++;

	return ASL_STATUS_OK;
}

static uint32_t
slotlist_delete(asl_store_t *s, uint32_t where)
{
	uint32_t i, j, n;

	if (s->slotlist_count == 0) return ASL_STATUS_OK;

	n = s->slotlist_count - 1;
	if (n == 0)
	{
		free(s->slotlist);
		s->slotlist = NULL;
		s->slotlist_count = 0;
		s->slot_zero_count = 0;
		return ASL_STATUS_OK;
	}

	if (s->slotlist[where].xid == 0) s->slot_zero_count--;

	for (i = where, j = i + 1; i < n; i++, j++)
	{
		s->slotlist[i].type = s->slotlist[j].type;
		s->slotlist[i].slot = s->slotlist[j].slot;
		s->slotlist[i].xid  = s->slotlist[j].xid;
		s->slotlist[i].hash = s->slotlist[j].hash;
	}

	s->slotlist_count = n;
	s->slotlist = (slot_info_t *)reallocf(s->slotlist, s->slotlist_count * sizeof(slot_info_t));

	if (s->slotlist == NULL)
	{
		s->slotlist_count = 0;
		s->slot_zero_count = 0;
		return ASL_STATUS_NO_MEMORY;
	}

	return ASL_STATUS_OK;
}

static uint32_t
slotlist_make_empty(asl_store_t *s, uint32_t where)
{
	uint32_t i, j, slot;

	if (s->slotlist_count == 0) return ASL_STATUS_OK;
	if (where > s->slotlist_count) return ASL_STATUS_OK;

	/*
	 * Special case for asl_store_archive.
	 * Since we expect to be doing lots of deletions during an archive call,
	 * this routine only marks the type as empty.
	 * asl_store_archive cleans up the slotlist when it is finished.
	 */
	if (s->flags & ASL_STORE_FLAG_DEFER_SORT)
	{
		s->slotlist[where].type = DB_TYPE_EMPTY;
		s->slotlist[where].hash = 0;

		s->empty_count++;

		return ASL_STATUS_OK;
	}

	slot = s->slotlist[where].slot;

	/* primary sort by xid */
	for (i = where, j = where - 1; (i > 0) && (s->slotlist[j].xid != 0); i--, j--)
	{
		s->slotlist[i].type = s->slotlist[j].type;
		s->slotlist[i].slot = s->slotlist[j].slot;
		s->slotlist[i].xid = s->slotlist[j].xid;
		s->slotlist[i].hash = s->slotlist[j].hash;
	}

	/* xid 0 entries sorted by slot */
	for (j = i - 1; (i > 0) && (s->slotlist[j].slot > slot); i--, j--)
	{
		s->slotlist[i].type = s->slotlist[j].type;
		s->slotlist[i].slot = s->slotlist[j].slot;
		s->slotlist[i].xid = s->slotlist[j].xid;
		s->slotlist[i].hash = s->slotlist[j].hash;
	}

	s->slotlist[i].type = DB_TYPE_EMPTY;
	s->slotlist[i].slot = slot;
	s->slotlist[i].xid = 0;
	s->slotlist[i].hash = 0;

	s->empty_count++;

	/* new xid=0 count */
	for (s->slot_zero_count = 0; (s->slot_zero_count < s->slotlist_count) && (s->slotlist[s->slot_zero_count].xid == 0); s->slot_zero_count++);

	return ASL_STATUS_OK;
}

static uint32_t
slotlist_init(asl_store_t *s)
{
	uint32_t i, si, status, hash, addslot;
	uint64_t xid, tick;
	uint8_t t;
	char tmp[DB_RECORD_LEN];

	s->empty_count = 0;

	/* Start at first slot after the header */
	status = fseek(s->db, DB_RECORD_LEN, SEEK_SET);
	if (status != 0) return ASL_STATUS_READ_FAILED;

	s->slotlist = (slot_info_t *)calloc(s->record_count, sizeof(slot_info_t));
	if (s->slotlist == NULL) return ASL_STATUS_NO_MEMORY;

	si = 0;

	for (i = 1; i < s->record_count; i++)
	{
		status = fread(tmp, DB_RECORD_LEN, 1, s->db);
		if (status != 1) return ASL_STATUS_READ_FAILED;

		t = tmp[0];
		addslot = 0;
		xid = 0;
		hash = 0;

		if (t == DB_TYPE_EMPTY)
		{
			addslot = 1;
			s->empty_count++;
		}

		if (t == DB_TYPE_STRING)
		{
			addslot = 1;
			s->string_count++;
			xid = header_get_id(tmp);
			hash = header_get_hash(tmp);
		}

		if (t == DB_TYPE_MESSAGE)
		{
			addslot = 1;
			s->message_count++;
			xid = header_get_id(tmp);
			tick = _asl_get_64(tmp + MSG_OFF_KEY_TIME);
			if (tick > s->max_time) s->max_time = tick;
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

	s->slotlist = (slot_info_t *)reallocf(s->slotlist, si * sizeof(slot_info_t));
	if (s->slotlist == NULL) return ASL_STATUS_NO_MEMORY;
	s->slotlist_count = si;

	/* slotlist is sorted by xid */
	qsort((void *)s->slotlist, s->slotlist_count, sizeof(slot_info_t), slot_comp);

	/* new xid=0 count */
	for (s->slot_zero_count = 0; (s->slot_zero_count < s->slotlist_count) && (s->slotlist[s->slot_zero_count].xid == 0); s->slot_zero_count++);

	return ASL_STATUS_OK;
}

uint32_t
asl_store_open(const char *path, uint32_t flags, asl_store_t **out)
{
	asl_store_t *s;
	struct stat sb;
	int status, i, j, fd;
	char cbuf[DB_RECORD_LEN];
	off_t fsize;
	uint64_t next;

	memset(&sb, 0, sizeof(struct stat));
	status = stat(path, &sb);

	fsize = 0;

	if (status < 0)
	{
		if (errno != ENOENT) return ASL_STATUS_FAILED;

		fd = open(path, O_RDWR | O_CREAT | O_EXCL, FILE_MODE);
		if (fd < 0) return ASL_STATUS_FAILED;

		memset(cbuf, 0, DB_RECORD_LEN);
		memcpy(cbuf, ASL_DB_COOKIE, ASL_DB_COOKIE_LEN);

		_asl_put_32(DB_VERSION, cbuf + DB_HEADER_VERS_OFFSET);

		/* record IDs start at 1 */
		_asl_put_64(1, cbuf + DB_HEADER_MAXID_OFFSET);

		status = write(fd, cbuf, DB_RECORD_LEN);
		close(fd);
		if (status != DB_RECORD_LEN) return ASL_STATUS_FAILED;

		fsize = DB_RECORD_LEN;
	}
	else
	{
		fsize = sb.st_size;
	}

	s = (asl_store_t *)calloc(1, sizeof(asl_store_t));
	if (s == NULL) return ASL_STATUS_NO_MEMORY;

	s->flags = flags;

	for (i = 0; i < RECORD_CACHE_SIZE; i++)
	{
		s->rcache[i] = malloc(DB_RECORD_LEN);
		if (s->rcache[i] == NULL)
		{
			for (j = 0; j < i; j++) free(s->rcache[j]);
			free(s);
			return ASL_STATUS_NO_MEMORY;
		}
	}

	s->db = NULL;
	if (flags & ASL_STORE_FLAG_READ_ONLY) s->db = fopen(path, "r");
	else s->db = fopen(path, "r+");
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

	next = 0;
	memcpy(&next, cbuf + DB_HEADER_MAXID_OFFSET, 8);
	s->next_id = _asl_ntohq(next);

	s->record_count = fsize / DB_RECORD_LEN;

	status = slotlist_init(s);

	for (i = 0; i < STRING_CACHE_SIZE; i++)
	{
		s->string_cache[i].index = ASL_INDEX_NULL;
		s->string_cache[i].refcount = 0;
		s->string_cache[i].str = NULL;
	}

	s->db_path = strdup(path);

	*out = s;
	return ASL_STATUS_OK;
}

uint32_t
asl_store_close(asl_store_t *s)
{
	uint32_t i;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;

	if (s->slotlist != NULL) free(s->slotlist);
	for (i = 0; i < RECORD_CACHE_SIZE; i++) free(s->rcache[i]);
	for (i = 0; i < STRING_CACHE_SIZE; i++)
	{
		if (s->string_cache[i].str != NULL) free(s->string_cache[i].str);
	}

	if (s->db_path != NULL) free(s->db_path);
	if (s->db != NULL) fclose(s->db);
	free(s);

	return ASL_STATUS_OK;
}

static char *
record_buffer_alloc(asl_store_t *s)
{
	uint32_t i;

	if (s == NULL) return calloc(1, DB_RECORD_LEN);

	for (i = 0; i < RECORD_CACHE_SIZE; i++)
	{
		if (s->rcache_state[i] == 0)
		{
			s->rcache_state[i] = 1;
			memset(s->rcache[i], 0, DB_RECORD_LEN);
			return s->rcache[i];
		}
	}

	return calloc(1, DB_RECORD_LEN);
}

static void
record_buffer_free(asl_store_t *s, char *p)
{
	uint32_t i;

	if (s == NULL) return free(p);

	for (i = 0; i < RECORD_CACHE_SIZE; i++)
	{
		if (s->rcache[i] == p)
		{
			s->rcache_state[i] = 0;
			return;
		}
	}

	free(p);
}

/*
 * Finds a free (DB_TYPE_EMPTY) record slot.
 * Returns the index of the free entry in the slotlist.
 * Returns ASL_INDEX_NULL if no free slots are available (next write should be at end of file).
 */
static uint32_t
get_free_slot(asl_store_t *s)
{
	uint32_t i;

	if (s == NULL) return ASL_INDEX_NULL;

	if (s->empty_count == 0) return ASL_INDEX_NULL;

	for (i = 0; i < s->slotlist_count; i++)
	{
		if (s->slotlist[i].type == DB_TYPE_EMPTY)
		{
			s->empty_count--;
			return i;
		}
	}

	/* impossible */
	s->empty_count = 0;
	return ASL_INDEX_NULL;
}

static void
record_list_free(asl_store_t *s, char **list)
{
	uint32_t i;

	if (list == NULL) return;

	for (i = 0; list[i] != NULL; i++) record_buffer_free(s, list[i]);
	free(list);
}

static uint64_t
new_id(asl_store_t *s)
{
	int status;
	uint64_t n, out;

	if (s == NULL) return ASL_REF_NULL;

	status = fseek(s->db, DB_HEADER_MAXID_OFFSET, SEEK_SET);
	if (status < 0) return ASL_REF_NULL;

	out = s->next_id;
	s->next_id++;

	n = _asl_htonq(s->next_id);
	status = fwrite(&n, 8, 1, s->db);
	if (status != 1) return ASL_REF_NULL;
	return out;
}

/*
 * Write each record in the list to a slot in the database.
 * Fills in "next" index.
 */
static uint32_t
save_record_list(asl_store_t *s, char **list, uint32_t *start)
{
	uint32_t i, n, status, si, slot, next, rcount, hash;
	uint8_t type;
	off_t offset;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;

	if (list == NULL) return ASL_STATUS_OK;

	for (n = 0; list[n] != NULL; n++);

	if (n == 0) return ASL_STATUS_OK;

	rcount = s->record_count;
	si = get_free_slot(s);

	/* update slotlist */
	type = list[0][0];

	if (type == DB_TYPE_STRING) s->string_count++;
	if (type == DB_TYPE_MESSAGE) s->message_count++;

	next = ASL_INDEX_NULL;
	if (si == ASL_INDEX_NULL)
	{
		next = s->record_count;
	}
	else if (si > s->slotlist_count)
	{
		return ASL_STATUS_FAILED;
	}
	else
	{
		next = s->slotlist[si].slot;
		slotlist_delete(s, si);
	}

	hash = 0;
	if (type == DB_TYPE_STRING) hash = header_get_hash(list[0]);

	status = slotlist_insert(s, type, next, header_get_id(list[0]), hash);
	if (status != ASL_STATUS_OK) return status;

	*start = next;

	for (i = 0; i < n; i++)
	{
		slot = next;

		next = 0;
		if ((i + 1) < n)
		{
			si = get_free_slot(s);
			if (si == ASL_INDEX_NULL)
			{
				next = s->record_count + 1;
			}
			else if (next > s->slotlist_count)
			{
				return ASL_STATUS_FAILED;
			}
			else
			{
				type = list[i + 1][0];
				next = s->slotlist[si].slot;
				slotlist_delete(s, si);
			}
		}

		offset = slot * DB_RECORD_LEN;
		status = fseek(s->db, offset, SEEK_SET);
		if (status < 0) return ASL_STATUS_WRITE_FAILED;

		header_put_next(next, list[i]);

		status = fwrite(list[i], DB_RECORD_LEN, 1, s->db);
		if (status != 1) return ASL_STATUS_WRITE_FAILED;

		if (si == ASL_INDEX_NULL) s->record_count++;
	}

	fflush(s->db);

	return ASL_STATUS_OK;
}

/*
 * Converts a string into a NULL-terminated list of records.
 * Sets sid to new string ID.
 */
static uint32_t
string_encode(asl_store_t *s, uint32_t hash, const char *str, uint64_t *sid, char ***list)
{
	char **outlist, *p;
	const char *t;
	uint32_t length, remaining, i, n, x;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (str == NULL) return ASL_STATUS_INVALID_STRING;

	*sid = new_id(s);
	if (*sid == ASL_REF_NULL) return ASL_STATUS_FAILED;

	length = strlen(str) + 1;
	remaining = length;

	x = DB_RECORD_LEN - DB_HLEN_STRING;
	if (remaining < x) x = remaining;
	remaining -= x;
	n = 1;

	x = DB_RECORD_LEN - DB_HLEN_STRCONT;
	n += (remaining + x - 1) / x;

	outlist = (char **)calloc(n + 1, sizeof(char *));
	if (outlist == NULL) return ASL_STATUS_NO_MEMORY;

	for (i = 0; i < n; i++)
	{
		outlist[i] = record_buffer_alloc(s);
		if (outlist[i] == NULL)
		{
			n = i;
			for (i = 0; i < n; i++) record_buffer_free(s, outlist[i]);
			free(outlist);
			return ASL_STATUS_NO_MEMORY;
		}
	}

	*list = outlist;

	outlist[0][0] = (char)DB_TYPE_STRING;
	p = outlist[0] + 5;

	/* sid */
	_asl_put_64(*sid, p);
	p += 8;

	/* refcount */
	_asl_put_32(1, p);
	p += 4;

	/* hash */
	_asl_put_32(hash, p);
	p += 4;

	/* string length (includes trailing nul) */
	_asl_put_32(length, p);
	p += 4;

	t = str;
	remaining = length;

	x = DB_RECORD_LEN - DB_HLEN_STRING;
	if (remaining < x) x = remaining;
	memcpy(p, t, x);

	t += x;
	remaining -= x;

	x = DB_RECORD_LEN - DB_HLEN_STRCONT;
	for (i = 1; i < n; i++)
	{
		outlist[i][0] = (char)DB_TYPE_STRCONT;
		p = outlist[i] + 5;

		if (remaining < x) x = remaining;
		memcpy(p, t, x);

		t += x;
		remaining -= x;
	}

	return ASL_STATUS_OK;
}

/*
 * Hash is used to improve string search.
 */
uint32_t
string_hash(const char *s, uint32_t inlen)
{
	uint32_t a, b, c, l, len;

	if (s == NULL) return 0;

	l = inlen;

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

	return c;
}

/*
 * Write refcount to database and update string cache.
 */
static void
string_set_refcount(asl_store_t *s, uint32_t index, const char *str, uint32_t refcount)
{
	uint32_t slot, i, min, status, v32;
	off_t offset;

	if (s == NULL) return;

	/* update the database */
	slot = s->slotlist[index].slot;

	offset = (slot * DB_RECORD_LEN) + 13;
	status = fseek(s->db, offset, SEEK_SET);

	if (status < 0) return;

	v32 = htonl(refcount);
	status = fwrite(&v32, 4, 1, s->db);

	min = 0;

	/* if the string is in the string cache, update the refcount there */
	for (i = 0; i < STRING_CACHE_SIZE; i++)
	{
		if (s->string_cache[i].index == index)
		{
			s->string_cache[i].refcount = refcount;
			return;
		}

		/* locate the minimum refcount while we're looping */
		if (s->string_cache[i].refcount < s->string_cache[min].refcount) min = i;
	}

	/* bail out if the refcount is too low */
	if (s->string_cache[min].refcount > refcount) return;

	/* replace the current minimum */
	if (s->string_cache[min].str != NULL) free(s->string_cache[min].str);

	s->string_cache[min].index = index;
	s->string_cache[min].refcount = refcount;
	s->string_cache[min].str = strdup(str);

	if (s->string_cache[min].str == NULL)
	{
		s->string_cache[min].index = ASL_INDEX_NULL;
		s->string_cache[min].refcount = 0;
		return;
	}
}

static uint32_t
string_fetch_slot(asl_store_t *s, uint32_t slot, char **out, uint32_t *refcount)
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

	*refcount = _asl_get_32(tmp + 13);

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
string_fetch_sid(asl_store_t *s, uint64_t sid, char **out)
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
	i = slotlist_find(s, sid, 0, 0);
	if (i == ASL_INDEX_NULL) return ASL_STATUS_NOT_FOUND;

	return string_fetch_slot(s, s->slotlist[i].slot, out, &ref);
}

static uint32_t
check_user_access(int32_t msgu, int32_t u)
{
	/* -1 means anyone may read */
	if (msgu == -1) return ASL_STATUS_OK;

	/* Check for exact match */
	if (msgu == u) return ASL_STATUS_OK;

	return ASL_STATUS_ACCESS_DENIED;
}

static uint32_t
check_group_access(int32_t msgg, int32_t u, int32_t g)
{
	int check;
	uuid_t uu, gu;

	/* -1 means anyone may read */
	if (msgg == -1) return ASL_STATUS_OK;

	/* Check for exact match */
	if (msgg == g) return ASL_STATUS_OK;

	/* Check if user (u) is in read group (msgg) */
	mbr_uid_to_uuid(u, uu);
	mbr_gid_to_uuid(msgg, gu);

	check = 0;
	mbr_check_membership(uu, gu, &check);
	if (check != 0) return ASL_STATUS_OK;

	return ASL_STATUS_ACCESS_DENIED;
}

static uint32_t
check_access(int32_t msgu, int32_t msgg, int32_t u, int32_t g, uint16_t flags)
{
	uint16_t uset, gset;

	/* root (uid 0) may always read */
	if (u == 0) return ASL_STATUS_OK;

	uset = flags & ASL_MSG_FLAG_READ_UID_SET;
	gset = flags & ASL_MSG_FLAG_READ_GID_SET;

	/* if no access controls are set, anyone may read */
	if ((uset | gset) == 0) return ASL_STATUS_OK;

	/* if only uid is set, then access is only by uid match */
	if ((uset != 0) && (gset == 0)) return check_user_access(msgu, u);

	/* if only gid is set, then access is only by gid match */
	if ((uset == 0) && (gset != 0)) return check_group_access(msgg, u, g);

	/* both uid and gid are set - check user, then group */
	if ((check_user_access(msgu, u)) == ASL_STATUS_OK) return ASL_STATUS_OK;
	return check_group_access(msgg, u, g);
}

static uint32_t
pmsg_fetch(asl_store_t *s, uint32_t slot, int32_t ruid, int32_t rgid, uint32_t action, pmsg_t **pmsg)
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

		status = check_access(msgu, msgg, ruid, rgid, flags);
		if (status != ASL_STATUS_OK) return status;

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
pmsg_match(asl_store_t *s, pmsg_t *q, pmsg_t *m)
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
pmsg_fetch_by_id(asl_store_t *s, uint64_t msgid, int32_t ruid, int32_t rgid, pmsg_t **pmsg, uint32_t *slot)
{
	uint32_t i, status;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (msgid == ASL_REF_NULL) return ASL_STATUS_INVALID_ARG;
	if (slot == NULL) return ASL_STATUS_INVALID_ARG;

	*slot = ASL_INDEX_NULL;

	i = slotlist_find(s, msgid, 0, 0);
	if (i == ASL_INDEX_NULL) return ASL_STATUS_INVALID_ID;

	*slot = s->slotlist[i].slot;

	/* read the message */
	*pmsg = NULL;
	status = pmsg_fetch(s, s->slotlist[i].slot, ruid, rgid, PMSG_FETCH_ALL, pmsg);
	if (status != ASL_STATUS_OK) return status;
	if (pmsg == NULL) return ASL_STATUS_FAILED;

	return status;
}

static uint32_t
msg_decode(asl_store_t *s, pmsg_t *pmsg, asl_msg_t **out)
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

		status = asl_set((aslmsg)msg, key, val);
		if (key != NULL) free(key);
		if (val != NULL) free(val);
		if (status != 0)
		{
			asl_free(msg);
			return ASL_STATUS_FAILED;
		}
	}

	*out = msg;
	return ASL_STATUS_OK;
}

/*
 * Finds string either in the string cache or in the database
 */
static uint32_t
store_string_find(asl_store_t *s, uint32_t hash, const char *str, uint32_t *index, uint32_t *refcount)
{
	uint32_t i, status, test;
	char *tmp;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (str == NULL) return ASL_STATUS_INVALID_ARG;
	if (index == NULL) return ASL_STATUS_INVALID_ARG;
	if (refcount == NULL) return ASL_STATUS_INVALID_ARG;
	if (s->slotlist == NULL) return ASL_STATUS_FAILED;

	/* check the cache */
	for (i = 0; i < STRING_CACHE_SIZE; i++)
	{
		if (s->string_cache[i].index == ASL_INDEX_NULL) continue;

		test = s->slotlist[s->string_cache[i].index].hash;
		if (test != hash) continue;

		if (s->string_cache[i].str == NULL)
		{
			/* can't happen, but clean up anyway */
			s->string_cache[i].index = ASL_INDEX_NULL;
			s->string_cache[i].refcount = 0;
			continue;
		}

		if (strcmp(s->string_cache[i].str, str)) continue;

		/* Bingo */
		*index = s->string_cache[i].index;
		*refcount = s->string_cache[i].refcount;
		return ASL_STATUS_OK;
	}

	/* check the database */
	for (i = 0; i < s->slotlist_count; i++)
	{
		if ((s->slotlist[i].type != DB_TYPE_STRING) || (s->slotlist[i].hash != hash)) continue;

		/* read the whole string */
		tmp = NULL;
		*refcount = 0;
		status = string_fetch_slot(s, s->slotlist[i].slot, &tmp, refcount);
		if (status != ASL_STATUS_OK) return status;
		if (tmp == NULL) return ASL_STATUS_FAILED;

		status = strcmp(tmp, str);
		free(tmp);
		if (status != 0) continue;

		/* Bingo! */
		*index = i;
		return ASL_STATUS_OK;
	}

	*refcount = 0;
	return ASL_STATUS_FAILED;
}

/*
 * Looks up a string ID number.
 * Creates the string if necessary.
 * Increments the string refcount.
 */
static uint64_t
string_retain(asl_store_t *s, const char *str, uint32_t create)
{
	uint32_t status, hash, index, slot, refcount, len;
	uint64_t nsid, sid;
	char **recordlist, *p;
	uint8_t inls;

	if (s == NULL) return ASL_REF_NULL;
	if (str == NULL) return ASL_REF_NULL;

	sid = ASL_REF_NULL;
	index = ASL_INDEX_NULL;
	slot = ASL_INDEX_NULL;
	refcount = 0;

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

	hash = string_hash(str, len);

	/* check the database */
	status = store_string_find(s, hash, str, &index, &refcount);
	if (status == ASL_STATUS_OK)
	{
		if (index == ASL_INDEX_NULL) return ASL_REF_NULL;
		if (create == 0) return s->slotlist[index].xid;

		refcount++;
		string_set_refcount(s, index, str, refcount);
		return s->slotlist[index].xid;
	}

	if (create == 0) return ASL_REF_NULL;

	/* create the string */
	recordlist = NULL;
	status = string_encode(s, hash, str, &sid, &recordlist);
	if (status != ASL_STATUS_OK) return ASL_REF_NULL;
	if (recordlist == NULL) return ASL_REF_NULL;

	status = save_record_list(s, recordlist, &slot);
	record_list_free(s, recordlist);
	if (status != ASL_STATUS_OK) return status;

	return sid;
}

static uint32_t
record_chain_free(asl_store_t *s, uint64_t xid, uint32_t slot, uint8_t type)
{
	uint32_t status, next, where;
	off_t offset;
	char zdb[DB_RECORD_LEN];

	if (s == NULL) return ASL_STATUS_INVALID_STORE;

	if ((type == DB_TYPE_STRING) && (s->string_count > 0)) s->string_count--;
	if ((type == DB_TYPE_MESSAGE) && (s->message_count > 0)) s->message_count--;

	memset(zdb, 0, DB_RECORD_LEN);

	/*
	 * Walk the chain and mark each slot as free.
	 *
	 * This is tricky:
	 * We need to know the index in the slot list for each slot used in the record.
	 * We are given the xid for the record, which we pass to slotlist_find
	 * to get the index of the first slot.  The slotlist entries for all subsequent
	 * slots will have xid 0, since they are of type DB_TYPE_KVLIST or DB_TYPE_STRCONT.
	 * Since slotlist has a secondary sort by slot, we can do a binary search within the
	 * xid 0 entries with slotlist_find to get the slotlist index of those slots.
	*/
	where = slotlist_find(s, xid, 0, 0);

	next = slot;
	while (next != 0)
	{
		/* update slotlist */
		slotlist_make_empty(s, where);

		offset = next * DB_RECORD_LEN;

		status = fseek(s->db, offset + 1, SEEK_SET);
		if (status < 0) return ASL_STATUS_WRITE_FAILED;

		status = fread(&next, 4, 1, s->db);
		if (status != 1) return ASL_STATUS_WRITE_FAILED;
		next = ntohl(next);

		status = fseek(s->db, offset, SEEK_SET);
		if (status < 0) return ASL_STATUS_WRITE_FAILED;

		status = fwrite(zdb, DB_RECORD_LEN, 1, s->db);
		if (status != 1) return ASL_STATUS_WRITE_FAILED;

		if (next != 0) where = slotlist_find_xid0_slot(s, next);
	}

	return ASL_STATUS_OK;
}

/*
 * Removes records.
 *
 * Decrements string refcount.
 * Removes string if refcount becomes zero.
 */
static uint32_t
id_release(asl_store_t *s, uint64_t xid, uint32_t slot, uint8_t type)
{
	uint32_t status, refcount, v32;
	uint64_t x;
	char head[17];
	off_t offset;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (xid == ASL_REF_NULL) return ASL_STATUS_INVALID_ID;
	if (slot == ASL_INDEX_NULL) return ASL_STATUS_INVALID_ARG;

	/* Note that record_chain_free() updates slotlist */

	offset = slot * DB_RECORD_LEN;
	status = fseek(s->db, offset, SEEK_SET);
	if (status < 0) return ASL_STATUS_WRITE_FAILED;

	status = fread(head, 17, 1, s->db);
	if (status != 1) return ASL_STATUS_WRITE_FAILED;

	if (head[0] != type) return ASL_STATUS_FAILED;

	x = header_get_id(head);

	if (x != xid) return ASL_STATUS_FAILED;

	/* small kludge: only strings are refcounted */
	refcount = 1;
	if (type == DB_TYPE_STRING) refcount = header_get_refcount(head);

	refcount--;
	if (refcount == 0)
	{
		return record_chain_free(s, xid, slot, type);
	}
	else
	{
		offset += 13;
		status = fseek(s->db, offset, SEEK_SET);
		if (status < 0) return ASL_STATUS_WRITE_FAILED;

		v32 = htonl(refcount);
		status = fwrite(&v32, 4, 1, s->db);
		if (status != 1) return ASL_STATUS_WRITE_FAILED;

		return ASL_STATUS_OK;
	}
}

static uint32_t
string_release(asl_store_t *s, uint64_t sid)
{
	uint32_t i;

	i = slotlist_find(s, sid, 0, 0);
	if (i == ASL_INDEX_NULL) return ASL_STATUS_INVALID_ID;

	return id_release(s, sid, s->slotlist[i].slot, DB_TYPE_STRING);
}

static uint32_t
message_release(asl_store_t *s, uint64_t xid)
{
	uint32_t i, slot, status;
	pmsg_t *pmsg;

	pmsg = NULL;
	slot = ASL_INDEX_NULL;

	/* read message and release strings */
	status = pmsg_fetch_by_id(s, xid, 0, 0, &pmsg, &slot);
	if (status != ASL_STATUS_OK) return status;
	if (pmsg == NULL) return ASL_STATUS_READ_FAILED;

	string_release(s, pmsg->host);
	string_release(s, pmsg->sender);
	string_release(s, pmsg->facility);
	string_release(s, pmsg->message);
	for (i = 0; i < pmsg->kvcount; i++) string_release(s, pmsg->kvlist[i]);
	free_pmsg(pmsg);

	return id_release(s, xid, slot, DB_TYPE_MESSAGE);
}

/*
 * Convert msg into a database record (or list of records if additional key/value pairs are present).
 * Returns a NULL-terminated list of records.
 *
 * Sets the message access control flags as follows:
 * If ruid is specified (anthing other than -1), then we use that as the ReadUID and set the uid access flag
 * Else if msg contains a ReadUID, we use that as the ReadUID and set the uid access flag
 * Else ReadUID is -1 and we don't set the uid access flag.
 * Same logic for ReadGID.
 */
static uint32_t
message_encode(asl_store_t *s, asl_msg_t *msg, int32_t ruid, int32_t rgid, uint64_t *msgid, char ***list)
{
	char **outlist, *std, *kvl, *p;
	uint32_t i, kvcount, kvn, len;
	uint32_t level;
	uint16_t flags;
	int32_t pid, uid, gid, muid, mgid;
	uint64_t time_val, host_sid, sender_sid, facility_sid, message_sid, k, v;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (msg == NULL) return ASL_STATUS_INVALID_MESSAGE;
	if (list == NULL) return ASL_STATUS_INVALID_ARG;

	len = 2;

	outlist = (char **)calloc(len, sizeof(char *));
	if (outlist == NULL) return ASL_STATUS_NO_MEMORY;

	std = record_buffer_alloc(s);
	if (std == NULL)
	{
		free(outlist);
		return ASL_STATUS_NO_MEMORY;
	}

	*msgid = new_id(s);
	if (*msgid == ASL_REF_NULL)
	{
		free(outlist);
		free(std);
		return ASL_STATUS_FAILED;
	}

	flags = 0;

	muid = -1;
	if (ruid != -1)
	{
		muid = ruid;
		flags |= ASL_MSG_FLAG_READ_UID_SET;
	}

	mgid = -1;
	if (rgid != -1)
	{
		mgid = rgid;
		flags |= ASL_MSG_FLAG_READ_GID_SET;
	}

	outlist[0] = std;

	flags = 0;
	level = ASL_INDEX_NULL;
	pid = -1;
	uid = -2;
	gid = -2;
	host_sid = ASL_REF_NULL;
	sender_sid = ASL_REF_NULL;
	facility_sid = ASL_REF_NULL;
	message_sid = ASL_REF_NULL;
	time_val = ASL_REF_NULL;

	kvcount = 0;
	kvn = 0;
	kvl = NULL;
	p = NULL;

	for (i = 0; i < msg->count; i++)
	{
		if (msg->key[i] == NULL) continue;

		else if (!strcmp(msg->key[i], ASL_KEY_TIME))
		{
			if (msg->val[i] != NULL) time_val = asl_parse_time(msg->val[i]);
		}
		else if (!strcmp(msg->key[i], ASL_KEY_HOST))
		{
			if (msg->val[i] != NULL) host_sid = string_retain(s, msg->val[i], 1);
		}
		else if (!strcmp(msg->key[i], ASL_KEY_SENDER))
		{
			if (msg->val[i] != NULL) sender_sid = string_retain(s, msg->val[i], 1);
		}
		else if (!strcmp(msg->key[i], ASL_KEY_PID))
		{
			if (msg->val[i] != NULL) pid = atoi(msg->val[i]);
		}
		else if (!strcmp(msg->key[i], ASL_KEY_UID))
		{
			if (msg->val[i] != NULL) uid = atoi(msg->val[i]);
		}
		else if (!strcmp(msg->key[i], ASL_KEY_GID))
		{
			if (msg->val[i] != NULL) gid = atoi(msg->val[i]);
		}
		else if (!strcmp(msg->key[i], ASL_KEY_LEVEL))
		{
			if (msg->val[i] != NULL) level = atoi(msg->val[i]);
		}
		else if (!strcmp(msg->key[i], ASL_KEY_MSG))
		{
			if (msg->val[i] != NULL) message_sid = string_retain(s, msg->val[i], 1);
		}
		else if (!strcmp(msg->key[i], ASL_KEY_FACILITY))
		{
			if (msg->val[i] != NULL) facility_sid = string_retain(s, msg->val[i], 1);
		}
		else if (!strcmp(msg->key[i], ASL_KEY_READ_UID))
		{
			if (((flags & ASL_MSG_FLAG_READ_UID_SET) == 0) && (msg->val[i] != NULL))
			{
				muid = atoi(msg->val[i]);
				flags |= ASL_MSG_FLAG_READ_UID_SET;
			}
		}
		else if (!strcmp(msg->key[i], ASL_KEY_READ_GID))
		{
			if (((flags & ASL_MSG_FLAG_READ_GID_SET) == 0) && (msg->val[i] != NULL))
			{
				mgid = atoi(msg->val[i]);
				flags |= ASL_MSG_FLAG_READ_GID_SET;
			}
		}
		else if (!strcmp(msg->key[i], ASL_KEY_MSG_ID))
		{
			/* Ignore */
			continue;
		}
		else
		{
			k = string_retain(s, msg->key[i], 1);
			if (k == ASL_REF_NULL) continue;

			v = ASL_REF_NULL;
			if (msg->val[i] != NULL) v = string_retain(s, msg->val[i], 1);

			if (kvl == NULL)
			{
				outlist = (char **)reallocf(outlist, (len + 1) * sizeof(char *));
				if (outlist == NULL)
				{
					free(std);
					return ASL_STATUS_NO_MEMORY;
				}

				kvl = record_buffer_alloc(s);
				if (kvl == NULL)
				{
					record_list_free(s, outlist);
					return ASL_STATUS_NO_MEMORY;
				}

				kvl[0] = DB_TYPE_KVLIST;
				p = kvl + 9;

				outlist[len - 1] = kvl;
				outlist[len] = NULL;
				len++;
			}

			kvcount++;

			_asl_put_64(k, p);
			kvn++;
			p += 8;

			_asl_put_64(v, p);
			kvn++;
			p += 8;

			if (kvn >= 8)
			{
				kvl = NULL;
				kvn = 0;
			}
		}
	}

	/* encode kvcount in first kvlist record */
	if (kvcount > 0)
	{
		kvl = outlist[1];
		_asl_put_32(kvcount, kvl + 5);
	}

	/* encode std */
	std[0] = DB_TYPE_MESSAGE;
	p = std + 5;

	_asl_put_64(*msgid, p);
	p += 8;

	_asl_put_32(muid, p);
	p += 4;

	_asl_put_32(mgid, p);
	p += 4;

	_asl_put_64(time_val, p);
	p += 8;

	_asl_put_64(host_sid, p);
	p += 8;

	_asl_put_64(sender_sid, p);
	p += 8;

	_asl_put_64(facility_sid, p);
	p += 8;

	_asl_put_32(level, p);
	p += 4;

	_asl_put_32(pid, p);
	p += 4;

	_asl_put_32(uid, p);
	p += 4;

	_asl_put_32(gid, p);
	p += 4;

	_asl_put_64(message_sid, p);
	p += 8;

	_asl_put_16(flags, p);
	p += 4;

	*list = outlist;

	return ASL_STATUS_OK;
}

uint32_t
asl_store_save(asl_store_t *s, asl_msg_t *msg, int32_t ruid, int32_t rgid, uint64_t *msgid)
{
	char **list;
	uint32_t status, slot;
	uint64_t tick;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (s->flags & ASL_STORE_FLAG_READ_ONLY) return ASL_STATUS_READ_ONLY;

	list = NULL;
	status = message_encode(s, msg, ruid, rgid, msgid, &list);
	if ((status != ASL_STATUS_OK) || (list == NULL)) return status;

	slot = 0;
	status = save_record_list(s, list, &slot);
	tick = _asl_get_64(list[0] + MSG_OFF_KEY_TIME);

	/* if time has gone backwards, unset the flag */
	if (tick < s->max_time) s->flags &= ~ASL_STORE_FLAG_TIME_SORTED;
	else s->max_time = tick;

	record_list_free(s, list);

	return status;
}

uint32_t
asl_store_remove(asl_store_t *s, uint64_t msgid)
{
	uint32_t status;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (s->flags & ASL_STORE_FLAG_READ_ONLY) return ASL_STATUS_READ_ONLY;

	status = message_release(s, msgid);
	return status;
}

uint32_t
asl_store_fetch(asl_store_t *s, uint64_t msgid, int32_t ruid, int32_t rgid, asl_msg_t **msg)
{
	uint32_t status, slot;
	pmsg_t *pmsg;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (msgid == ASL_REF_NULL) return ASL_STATUS_INVALID_ARG;

	pmsg = NULL;
	slot = ASL_INDEX_NULL;

	status = pmsg_fetch_by_id(s, msgid, ruid, rgid, &pmsg, &slot);
	if (status != ASL_STATUS_OK) return status;
	if (pmsg == NULL) return ASL_STATUS_FAILED;

	status = msg_decode(s, pmsg, msg);
	free_pmsg(pmsg);

	return status;
}

static uint32_t
query_to_pmsg(asl_store_t *s, asl_msg_t *q, pmsg_t **p)
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
				out->host = string_retain(s, q->val[i], 0);
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
				out->sender = string_retain(s, q->val[i], 0);
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
				out->message = string_retain(s, q->val[i], 0);
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
				out->facility = string_retain(s, q->val[i], 0);
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
			ksid = string_retain(s, q->key[i], 0);
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
				vsid = string_retain(s, q->val[i], 0);
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

uint32_t
msg_match(asl_store_t *s, uint32_t qtype, pmsg_t *qp, asl_msg_t *q, int32_t ruid, int32_t rgid, uint32_t slot, pmsg_t **iopm, asl_msg_t **iomsg, asl_msg_list_t **res, uint32_t *didmatch)
{
	uint32_t status, what;

	*didmatch = 0;

	if (qtype == Q_FAIL) return ASL_STATUS_OK;

	if (qtype == Q_NULL)
	{
		if (*iopm == NULL)
		{
			status = pmsg_fetch(s, slot, ruid, rgid, PMSG_FETCH_ALL, iopm);
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
			status = pmsg_fetch(s, slot, ruid, rgid, what, iopm);
			if (status != ASL_STATUS_OK) return status;
			if (*iopm == NULL) return ASL_STATUS_FAILED;
		}

		status = pmsg_match(s, qp, *iopm);
		if (status == 1)
		{
			if ((what == PMSG_FETCH_STD) && ((*iopm)->next != 0) && ((*iopm)->kvcount == 0))
			{
				status = pmsg_fetch(s, slot, ruid, rgid, PMSG_FETCH_KV, iopm);
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
				status = pmsg_fetch(s, slot, ruid, rgid, PMSG_FETCH_ALL, iopm);
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
next_search_slot(asl_store_t *s, uint32_t last_si, int32_t direction)
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
query_list_to_pmsg_list(asl_store_t *s, asl_msg_list_t *query, uint32_t *match, pmsg_t ***qp, uint32_t **qtype, uint32_t *count)
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
	if (qtype == NULL) return ASL_STATUS_INVALID_ARG;
	if (query->msg == NULL) return ASL_STATUS_INVALID_ARG;
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
 * Input to asl_store_match is a list of queries.
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
match_worker(asl_store_t *s, asl_msg_list_t *query, asl_msg_list_t **res, uint64_t *last_id, uint64_t **idlist, uint32_t *idcount, uint64_t start_id, int32_t count, int32_t direction, int32_t ruid, int32_t rgid)
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
	else si = slotlist_find(s, start_id, 0, direction);

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
			status = msg_match(s, Q_NULL, NULL, NULL, ruid, rgid, slot, &iopmsg, &iomsg, res, &didmatch);
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

			if (status == ASL_STATUS_ACCESS_DENIED)
			{
				si = next_search_slot(s, si, direction);
				continue;
			}
			else if (status != ASL_STATUS_OK)
			{
				match_worker_cleanup(qp, qtype, qcount, res);
				return status;
			}
		}
		else
		{
			for (i = 0; i < qcount; i++)
			{
				status = msg_match(s, qtype[i], qp[i], query->msg[i], ruid, rgid, slot, &iopmsg, &iomsg, res, &didmatch);
				if (status == ASL_STATUS_ACCESS_DENIED) break;
				else if (status != ASL_STATUS_OK)
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
asl_store_match(asl_store_t *s, asl_msg_list_t *query, asl_msg_list_t **res, uint64_t *last_id, uint64_t start_id, uint32_t count, int32_t direction, int32_t ruid, int32_t rgid)
{
	uint32_t idcount;

	idcount = 0;
	return match_worker(s, query, res, last_id, NULL, &idcount, start_id, count, direction, ruid, rgid);
}

uint32_t
asl_store_prune(asl_store_t *s, asl_msg_list_t *prune)
{
	uint64_t *idlist, max_id;
	uint32_t status, i, idcount;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (s->flags & ASL_STORE_FLAG_READ_ONLY) return ASL_STATUS_READ_ONLY;

	if (prune == NULL) return ASL_STATUS_OK;

	idlist = NULL;
	idcount = 0;
	max_id = 0;

	status = match_worker(s, prune, NULL, &max_id, &idlist, &idcount, 0, 0, 1, 0, 0);
	if (status != ASL_STATUS_OK)
	{
		if (idlist != NULL) free(idlist);
		return status;
	}

	for (i = 0; i < idcount; i++) message_release(s, idlist[i]);
	if (idlist != NULL) free(idlist);

	return ASL_STATUS_OK;
}

/*
 * Compact the database.
 * Removes NULL and EMPTY records by copying records to the front of the file.
 */
uint32_t
asl_store_compact(asl_store_t *s)
{
	char tmp[DB_RECORD_LEN];
	int status;
	uint8_t t;
	uint32_t i, j, nrecords, next, slcount, old_slcount, *record_map;
 	off_t offset;
	slot_info_t *old_slot_list;
	size_t vmrecord_map_len, vmslot_list_len;
	void *vmrecord_map, *vmslot_list;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (s->flags & ASL_STORE_FLAG_READ_ONLY) return ASL_STATUS_READ_ONLY;

	status = fseek(s->db, DB_RECORD_LEN, SEEK_SET);
	if (status < 0) return ASL_STATUS_READ_FAILED;

	/*
	 * record map is a mapping from pre-compaction record number to post-compaction record number.
	 * We allocate it in VM rather than on the malloc heap to keep from creating a lot of
	 * empty pages.
	 */
	nrecords = s->record_count;

	vmrecord_map_len = nrecords * sizeof(uint32_t);
	vmrecord_map = mmap(0, vmrecord_map_len, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

	record_map = (uint32_t *)vmrecord_map;
	if (record_map == NULL) return ASL_STATUS_NO_MEMORY;

	record_map[0] = 0;

	/* size of post-compaction slotlist */
	slcount = 0;

	/* first pass: create the record map (N.B. starting at 1 skips the header) */
	for (i = 1, j = 1; i < nrecords; i++)
	{
		record_map[i] = 0;

		status = fread(tmp, DB_RECORD_LEN, 1, s->db);
		if (status != 1)
		{
			munmap(vmrecord_map, vmrecord_map_len);
			return ASL_STATUS_READ_FAILED;
		}

		t = tmp[0];

		if (t == DB_TYPE_HEADER)
		{
			munmap(vmrecord_map, vmrecord_map_len);
			return ASL_STATUS_INVALID_STORE;
		}

		/*
		 * Only messages, kvlists, strings, and string continuations get copied.
		 * Empty, null, and unrecognized record types (i.e. corruption in the database)
		 * are skipped.  This compresses out gaps and deletes bad records.
		 */
		if ((t != DB_TYPE_MESSAGE) && (t != DB_TYPE_KVLIST) && (t != DB_TYPE_STRING) && (t != DB_TYPE_STRCONT)) continue;

		/* count to get size of new slotlist */
		if ((t == DB_TYPE_STRING) || (t == DB_TYPE_MESSAGE)) slcount++;

		record_map[i] = j++;
	}

	/* second pass: copy records and fix "next" indexes */
	for (i = 1; i < nrecords; i++)
	{
		offset = i * DB_RECORD_LEN;
		status = fseek(s->db, offset, SEEK_SET);
		if (status < 0)
		{
			munmap(vmrecord_map, vmrecord_map_len);
			return ASL_STATUS_READ_FAILED;
		}

		status = fread(tmp, DB_RECORD_LEN, 1, s->db);
		if (status != 1)
		{
			munmap(vmrecord_map, vmrecord_map_len);
			return ASL_STATUS_READ_FAILED;
		}

		t = tmp[0];

		/* only copy messages, kvlists, strings, and string continuations */
		if ((t != DB_TYPE_MESSAGE) && (t != DB_TYPE_KVLIST) && (t != DB_TYPE_STRING) && (t != DB_TYPE_STRCONT)) continue;

		next = _asl_get_32(tmp + 1);

		if (next > nrecords) next = 0;
		else next = record_map[next];

		_asl_put_32(next, tmp + 1);

		offset = record_map[i] * DB_RECORD_LEN;
		status = fseek(s->db, offset, SEEK_SET);
		if (status < 0)
		{
			munmap(vmrecord_map, vmrecord_map_len);
			return ASL_STATUS_READ_FAILED;
		}

		status = fwrite(tmp, DB_RECORD_LEN, 1, s->db);
		if (status != 1)
		{
			munmap(vmrecord_map, vmrecord_map_len);
			return ASL_STATUS_WRITE_FAILED;
		}
	}

	/* truncate file */
	s->record_count = j;
	offset = s->record_count * DB_RECORD_LEN;

	status = fseek(s->db, 0, SEEK_SET);
	if (status < 0)
	{
		munmap(vmrecord_map, vmrecord_map_len);
		return ASL_STATUS_READ_FAILED;
	}

	status = ftruncate(fileno(s->db), offset);
	if (status != 0)
	{
		munmap(vmrecord_map, vmrecord_map_len);
		return ASL_STATUS_WRITE_FAILED;
	}

	/*
	 * build new slotlist
	 *
	 * We start by allocating and copying the old slotlist into VM.
	 * Then we realloc the old slotlist to become the new slotlist.
	 * Then we build the new slotlist from the values in VM.
	 * Finally we deallocate the VM.
	 * This is done so that we don't create a large malloc heap.
	 */

	vmslot_list_len = s->slotlist_count * sizeof(slot_info_t);

	vmslot_list = mmap(0, vmslot_list_len, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	old_slot_list = (slot_info_t *)vmslot_list;
	if (old_slot_list == NULL)
	{
		munmap(vmrecord_map, vmrecord_map_len);
		return ASL_STATUS_NO_MEMORY;
	}

	old_slcount = s->slotlist_count;

	/* copy old values to VM */
	for (i = 0; i < s->slotlist_count; i++)
	{
		old_slot_list[i].type = s->slotlist[i].type;
		old_slot_list[i].slot = s->slotlist[i].slot;
		old_slot_list[i].xid = s->slotlist[i].xid;
		old_slot_list[i].hash = s->slotlist[i].hash;
	}

	s->slotlist = (slot_info_t *)reallocf(s->slotlist, slcount * sizeof(slot_info_t));
	if (s->slotlist == NULL)
	{
		munmap(vmrecord_map, vmrecord_map_len);
		munmap(vmslot_list, vmslot_list_len);
		return ASL_STATUS_NO_MEMORY;
	}

	s->slotlist_count = slcount;

	/* create the new compacted slotlist */
	for (i = 0, j = 0; i < old_slcount; i++)
	{
		t = old_slot_list[i].type;
		if ((t == DB_TYPE_STRING) || (t == DB_TYPE_MESSAGE))
		{
			s->slotlist[j].type = t;
			s->slotlist[j].slot = record_map[old_slot_list[i].slot];
			s->slotlist[j].xid = old_slot_list[i].xid;
			s->slotlist[j].hash = old_slot_list[i].hash;
			j++;
		}
	}

	munmap(vmslot_list, vmslot_list_len);

	s->empty_count = 0;

	/* fix string cache index (which indexes into slotlist) */
	for (i = 0; i < STRING_CACHE_SIZE; i++)
	{
		if (s->string_cache[i].index == ASL_INDEX_NULL) continue;
		s->string_cache[i].index = record_map[s->string_cache[i].index];
	}

	/* new xid=0 count */
	for (s->slot_zero_count = 0; (s->slot_zero_count < s->slotlist_count) && (s->slotlist[s->slot_zero_count].xid == 0); s->slot_zero_count++);

	munmap(vmrecord_map, vmrecord_map_len);

	return ASL_STATUS_OK;
}

static uint32_t
write_to_archive(asl_store_t *s, asl_store_t *a, uint64_t msgid)
{
	uint32_t status;
	uint64_t xid;
	aslmsg msg;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (a == NULL) return ASL_STATUS_OK;

	status = asl_store_fetch(s, msgid, 0, 0, &msg);
	if (status != ASL_STATUS_OK) return status;

	status = asl_store_save(a, msg, -1, -1, &xid);
	asl_free(msg);
	return status;
}

static uint64_t
oldest_id(asl_store_t *s)
{
	uint32_t si;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;

	si = next_search_slot(s, ASL_INDEX_NULL, 1);
	if (si == ASL_INDEX_NULL) return ASL_REF_NULL;

	return s->slotlist[si].xid;
}

/*
 * Archive/remove oldest messages to make the database <= max_size
 * This is slow - messages are removed one at a time.
 */
uint32_t
asl_store_truncate(asl_store_t *s, uint64_t max_size, const char *archive)
{
	uint32_t max_slots, curr_used;
	uint32_t status;
	uint64_t old;
	asl_store_t *a;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (s->flags & ASL_STORE_FLAG_READ_ONLY) return ASL_STATUS_READ_ONLY;

	if (max_size == 0) return ASL_STATUS_OK;

	a = NULL;

	max_slots = (max_size + DB_RECORD_LEN - 1) / DB_RECORD_LEN;
	curr_used = s->record_count - s->empty_count;

	if ((curr_used > max_slots) && (archive != NULL))
	{
		status = asl_store_open(archive, 0, &a);
		if (status != ASL_STATUS_OK) return status;
	}

	while (curr_used > max_slots)
	{
		old = oldest_id(s);
		if (old == ASL_REF_NULL) return ASL_STATUS_FAILED;

		if (archive != NULL)
		{
			status = write_to_archive(s, a, old);
			if (status != ASL_STATUS_OK) return status;
		}

		status = message_release(s, old);
		if (status != ASL_STATUS_OK) return status;

		curr_used = s->record_count - s->empty_count;
	}

	if (archive != NULL) asl_store_close(a);

	status = asl_store_compact(s);
	return status;
}

static uint32_t
archive_time_worker(asl_store_t *s, uint64_t cut_time, uint64_t **idlist, uint16_t **flags, uint32_t *idcount)
{
	uint32_t si, slot, status, check_sort;
	uint64_t xid, t, lastt;
	uint16_t rflags;
	char tmp[DB_RECORD_LEN];
	off_t offset;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (idlist == NULL) return ASL_STATUS_INVALID_ARG;
	if (flags == NULL) return ASL_STATUS_INVALID_ARG;
	if (idcount == NULL) return ASL_STATUS_INVALID_ARG;

	*idlist = NULL;
	*flags = NULL;
	*idcount = 0;
	si = ASL_INDEX_NULL;

	lastt = 0;
	check_sort = 1;

	/*
	 * loop through records
	 *
	 * Note that next_search_slot() traverses slotlist, which is sorted by id numder.
	 * If the ASL_STORE_FLAG_TIME_SORTED flag is not set, we must search the whole database.
	 * If the flag is set, timestamps will increase with xid, so we stop when we get
	 * a message with time > cut_time.
	 */
	forever
	{
		si = next_search_slot(s, si, 1);
		if (si == ASL_INDEX_NULL) break;

		slot = s->slotlist[si].slot;
		xid = s->slotlist[si].xid;

		offset = slot * DB_RECORD_LEN;
		status = fseek(s->db, offset, SEEK_SET);

		if (status < 0) return ASL_STATUS_READ_FAILED;

		status = fread(tmp, DB_RECORD_LEN, 1, s->db);
		if (status != 1) return ASL_STATUS_READ_FAILED;

		t = _asl_get_64(tmp + MSG_OFF_KEY_TIME);

		if (lastt > t) check_sort = 0;

		if (t > cut_time)
		{
			if (s->flags & ASL_STORE_FLAG_TIME_SORTED) return ASL_STATUS_OK;
			continue;
		}

		rflags = _asl_get_16(tmp + MSG_OFF_KEY_FLAGS);

		if (*idlist == NULL)
		{
			*idlist = (uint64_t *)calloc(1, sizeof(uint64_t));
			*flags = (uint16_t *)calloc(1, sizeof(uint16_t));
		}
		else
		{
			*idlist = (uint64_t *)reallocf(*idlist, (*idcount + 1) * sizeof(uint64_t));
			*flags = (uint16_t *)reallocf(*flags, (*idcount + 1) * sizeof(uint16_t));
		}

		if (*idlist == NULL)
		{
			if (*flags != NULL) free(*flags);
			*flags = NULL;
			return ASL_STATUS_NO_MEMORY;
		}

		if (*flags == NULL)
		{
			if (*idlist != NULL) free(*idlist);
			*idlist = NULL;
			return ASL_STATUS_NO_MEMORY;
		}

		(*idlist)[*idcount] = xid;
		(*flags)[*idcount] = rflags;
		(*idcount)++;
	}

	/* if timestamps increase with xid, set the flag to improve subsequent search performance */
	if (check_sort == 1) s->flags |= ASL_STORE_FLAG_TIME_SORTED;

	return ASL_STATUS_OK;
}

static uint32_t
archive_time_inverse(asl_store_t *s, uint64_t cut_time, uint64_t **idlist, uint32_t *idcount)
{
	uint32_t si, slot, status;
	uint64_t xid, t, lastt;
	char tmp[DB_RECORD_LEN];
	off_t offset;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (idlist == NULL) return ASL_STATUS_INVALID_ARG;
	if (idcount == NULL) return ASL_STATUS_INVALID_ARG;

	*idlist = NULL;
	*idcount = 0;
	si = ASL_INDEX_NULL;

	lastt = 0;

	/*
	 * loop through records
	 */
	forever
	{
		si = next_search_slot(s, si, 1);
		if (si == ASL_INDEX_NULL) break;

		slot = s->slotlist[si].slot;
		xid = s->slotlist[si].xid;

		offset = slot * DB_RECORD_LEN;
		status = fseek(s->db, offset, SEEK_SET);

		if (status < 0) return ASL_STATUS_READ_FAILED;

		status = fread(tmp, DB_RECORD_LEN, 1, s->db);
		if (status != 1) return ASL_STATUS_READ_FAILED;

		t = _asl_get_64(tmp + MSG_OFF_KEY_TIME);

		if (t <= cut_time) continue;

		if (*idlist == NULL)
		{
			*idlist = (uint64_t *)calloc(1, sizeof(uint64_t));
		}
		else
		{
			*idlist = (uint64_t *)reallocf(*idlist, (*idcount + 1) * sizeof(uint64_t));
		}

		if (*idlist == NULL) return ASL_STATUS_NO_MEMORY;


		(*idlist)[*idcount] = xid;
		(*idcount)++;
	}

	return ASL_STATUS_OK;
}

static uint32_t
archive_release(asl_store_t *s, uint64_t xid, uint64_t cut_time, uint64_t expire_ref)
{
	uint32_t i, slot, status;
	uint16_t rflags;
	pmsg_t *pmsg;
	uint64_t expire_time;
	char *str, tmp[DB_RECORD_LEN];
	off_t offset;

	pmsg = NULL;
	slot = ASL_INDEX_NULL;

	/* read message and release strings */
	status = pmsg_fetch_by_id(s, xid, 0, 0, &pmsg, &slot);
	if (status != ASL_STATUS_OK) return status;
	if (pmsg == NULL) return ASL_STATUS_READ_FAILED;

	if (expire_ref != ASL_REF_NULL)
	{
		for (i = 0; i < pmsg->kvcount; i += 2)
		{
			if (pmsg->kvlist[i] == expire_ref)
			{
				str = NULL;
				status = string_fetch_sid(s, pmsg->kvlist[i + 1], &str);
				if (status != ASL_STATUS_OK) return status;
				if (str != NULL)
				{
					expire_time = 0;
					/* relative time not allowed - that would be cheating! */
					if (str[0] != '+') expire_time = asl_parse_time(str);
					free(str);

					if (expire_time > cut_time)
					{
						/* expires in the future - mark as "do not archive" and don't release */
						free_pmsg(pmsg);

						offset = slot * DB_RECORD_LEN;
						status = fseek(s->db, offset, SEEK_SET);
						if (status < 0) return ASL_STATUS_READ_FAILED;

						status = fread(tmp, DB_RECORD_LEN, 1, s->db);
						if (status != 1) return ASL_STATUS_READ_FAILED;

						rflags = _asl_get_16(tmp + MSG_OFF_KEY_FLAGS);
						if ((rflags & ASL_MSG_FLAG_DO_NOT_ARCHIVE) == 0)
						{
							rflags |= ASL_MSG_FLAG_DO_NOT_ARCHIVE;
							_asl_put_16(rflags, tmp + MSG_OFF_KEY_FLAGS);

							status = fseek(s->db, offset, SEEK_SET);
							if (status < 0) return ASL_STATUS_WRITE_FAILED;

							status = fwrite(tmp, DB_RECORD_LEN, 1, s->db);
							if (status != 1) return ASL_STATUS_WRITE_FAILED;
						}

						return ASL_STATUS_OK;
					}
				}
			}
		}
	}

	string_release(s, pmsg->host);
	string_release(s, pmsg->sender);
	string_release(s, pmsg->facility);
	string_release(s, pmsg->message);
	for (i = 0; i < pmsg->kvcount; i++) string_release(s, pmsg->kvlist[i]);
	free_pmsg(pmsg);

	return id_release(s, xid, slot, DB_TYPE_MESSAGE);
}

static char *
asl_store_mk_tmp_path()
{
    char tmp[PATH_MAX], *path;

	if (confstr(_CS_DARWIN_USER_TEMP_DIR, tmp, sizeof(tmp)) <= 0) return NULL;

	path = NULL;
	asprintf(&path, "%sasl.%d.tmp", tmp, getpid());
	return path;
}

/*
 * Moves messages added at or before cut_time to an archive,
 * or delete them if archive_name is NULL.
 */
uint32_t
asl_store_archive(asl_store_t *s, uint64_t cut_time, const char *archive_name)
{
	asl_store_t *archive, *newstore;
	char *path, *newmapped;
	uint16_t *flags;
	uint32_t status, i, archive_count, save_count;
	uint64_t expire_ref, *archive_list, *save_list;
	size_t dbsize;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (s->flags & ASL_STORE_FLAG_READ_ONLY) return ASL_STATUS_READ_ONLY;

	if (cut_time == 0) return ASL_STATUS_OK;

	archive_count = 0;
	archive_list = NULL;
	save_count = 0;
	save_list = NULL;
	flags = NULL;

	s->flags = 0;

	status = archive_time_worker(s, cut_time, &archive_list, &flags, &archive_count);
	if (status != ASL_STATUS_OK) return status;

	if ((archive_list == NULL) || (archive_count == 0))
	{
		if (archive_list != NULL) free(archive_list);
		return ASL_STATUS_OK;
	}

	archive = NULL;
	if (archive_name != NULL)
	{
		status = asl_store_open(archive_name, 0, &archive);
		if (status != ASL_STATUS_OK) return status;
	}

	if (archive != NULL)
	{
		for (i = 0; i < archive_count; i++)
		{
			if (flags[i] & ASL_MSG_FLAG_DO_NOT_ARCHIVE) continue;

			status = write_to_archive(s, archive, archive_list[i]);
			if (status != ASL_STATUS_OK)
			{
				free(archive_list);
				asl_store_close(archive);
				return status;
			}
		}

		asl_store_close(archive);
	}

	/*
	 * Deleting large numbers of records is slow.
	 * If the number of records to be deleted is at least 1000, and
	 * is ARCHIVE_DELETE_VS_COPY_PERCENT or above, we copy the records
	 * that should remain to a temporary archive, then replace the
	 * database with the temporary one.
	 * Note that we need to know the name of the current DB file.
	 */
	path = NULL;
	if ((archive_count >= 1000) && (((archive_count * 100) / s->message_count) >= ARCHIVE_DELETE_VS_COPY_PERCENT) && (s->db_path != NULL)) path = asl_store_mk_tmp_path();
	if (path != NULL)
	{
		status = unlink(path);
		if ((status != 0) && (errno != ENOENT))
		{
			free(path);
			path = NULL;
		}
	}

	if (path != NULL)
	{
		if (archive_list != NULL) free(archive_list);
		archive_list = NULL;
		archive_count = 0;

		newstore = NULL;
		status = asl_store_open(path, 0, &newstore);
		free(path);
		path = NULL;
		if (status != ASL_STATUS_OK) return status;

		/* Set the next_id so that the archive will have ids bigger than the current archive. */
		newstore->next_id = s->next_id;

		/* get a list of records that we want to keep */
		status = archive_time_inverse(s, cut_time, &save_list, &save_count);
		if (status != ASL_STATUS_OK)
		{
			asl_store_close(newstore);
			return status;
		}

		if ((save_list == NULL) || (save_count == 0))
		{
			if (save_list != NULL) free(save_list);
			asl_store_close(newstore);
			return ASL_STATUS_OK;
		}

		/* save to the temp archive */
		for (i = 0; i < save_count; i++)
		{
			status = write_to_archive(s, newstore, save_list[i]);
			if (status != ASL_STATUS_OK)
			{
				if (save_list != NULL) free(save_list);
				asl_store_close(newstore);
				return status;
			}
		}

		free(save_list);
		save_list = NULL;

		/* try rename since it's fast, but may fail (e.g. files are on different filesystems) */
		fclose(s->db);
		status = rename(newstore->db_path, s->db_path);
		if (status == 0)
		{
			/* success */
			s->db = fopen(s->db_path, "r+");
			if (s->db == NULL)
			{
				/* Disaster! Can't open the database! */
				asl_store_close(newstore);
				return ASL_STATUS_FAILED;
			}
		}
		else
		{
			/* rename failed, copy the data */
			s->db = fopen(s->db_path, "r+");
			if (s->db == NULL)
			{
				/* Disaster! Can't open the database! */
				asl_store_close(newstore);
				return ASL_STATUS_FAILED;
			}

			dbsize = newstore->record_count * DB_RECORD_LEN;
			newmapped = mmap(0, dbsize, PROT_READ, MAP_PRIVATE, fileno(newstore->db), 0);
			if (newmapped == (void *)-1)
			{
				asl_store_close(newstore);
				return ASL_STATUS_FAILED;
			}

			fseek(s->db, 0, SEEK_SET);
			status = ftruncate(fileno(s->db), 0);
			if (status != ASL_STATUS_OK)
			{
				asl_store_close(newstore);
				return status;
			}

			status = fwrite(newmapped, dbsize, 1, s->db);
			munmap(newmapped, dbsize);
			if (status == 0)
			{
				asl_store_close(newstore);
				return ASL_STATUS_FAILED;
			}
		}

		/* swap data in the store handles */
		if (s->slotlist != NULL) free(s->slotlist);
		s->slotlist = newstore->slotlist;
		newstore->slotlist = NULL;

		for (i = 0; i < RECORD_CACHE_SIZE; i++)
		{
			free(s->rcache[i]);
			s->rcache[i] = newstore->rcache[i];
			newstore->rcache[i] = NULL;
			s->rcache_state[i] = newstore->rcache_state[i];
		}

		for (i = 0; i < STRING_CACHE_SIZE; i++)
		{
			s->string_cache[i].index = newstore->string_cache[i].index;
			s->string_cache[i].refcount = newstore->string_cache[i].refcount;
			if (s->string_cache[i].str != NULL) free(s->string_cache[i].str);
			s->string_cache[i].str = newstore->string_cache[i].str;
			newstore->string_cache[i].str = NULL;
		}

		s->flags = newstore->flags;
		s->record_count = newstore->record_count;
		s->message_count = newstore->message_count;
		s->string_count = newstore->string_count;
		s->empty_count = newstore->empty_count;
		s->next_id = newstore->next_id;
		s->max_time = newstore->max_time;
		s->slotlist_count = newstore->slotlist_count;
		s->slot_zero_count = newstore->slot_zero_count;

		fclose(newstore->db);
		unlink(newstore->db_path);
		free(newstore->db_path);
		free(newstore);

		return ASL_STATUS_OK;
	}

	expire_ref = string_retain(s, ASL_KEY_EXPIRE_TIME, 0);

	/*
	 * This flag turns off most of the code in slotlist_make_empty.
	 * We get much better performace while we delete records,
	 * but the slotlist has to be repaired and re-sorted afterwards.
	 */
	s->flags |= ASL_STORE_FLAG_DEFER_SORT;

	for (i = 0; i < archive_count; i++)
	{
		status = archive_release(s, archive_list[i], cut_time, expire_ref);
		if (status != ASL_STATUS_OK) return status;
	}

	s->flags &= ~ASL_STORE_FLAG_DEFER_SORT;

	free(archive_list);
	archive_list = NULL;
	archive_count = 0;

	free(flags);
	flags = NULL;

	/* zero xids for slots that became empty during archive release */
	for (i = 0; i < s->slotlist_count; i++)
	{
		if (s->slotlist[i].type == DB_TYPE_EMPTY) s->slotlist[i].xid = 0;
	}

	/* re-sort and determine the zero count */
	qsort((void *)s->slotlist, s->slotlist_count, sizeof(slot_info_t), slot_comp);

	/* new xid=0 count */
	for (s->slot_zero_count = 0; (s->slot_zero_count < s->slotlist_count) && (s->slotlist[s->slot_zero_count].xid == 0); s->slot_zero_count++);

	return ASL_STATUS_OK;
}

const char *
asl_store_error(uint32_t code)
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
	}

	return "Operation Failed";
}
