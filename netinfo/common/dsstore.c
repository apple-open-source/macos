/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
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

#include <NetInfo/dsstatus.h>
#include "dsstore.h"
#include "nistore.h"
#include "dsindex.h"
#include "dsdata.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dir.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <utime.h>
#include <syslog.h>
#include <notify.h>

extern u_int32_t notify_set_state(int token, int state);
extern u_int32_t notify_get_state(int token, int *state);

#define DataStoreAccessMode 0700
#define ConfigFileName "Config"
#define IndexFileName "Index"
#define TempIndexFileName "TempIndex"
#define StoreFileBase "Store"
#define CreateFileName "Create"
#define TempFileName "Temp"
#define DeleteFileName "Delete"
#define CleanFileName "Clean"

#define NAME_INDEX_KEY "index_key"
#define NAME_NAME "name"

#define NETINFO_NOTIFY_PREFIX "com.apple.system.netinfo"
#define NETINFO_NOTIFY_SUFFIX "database_update"

#define ReservedFileNameLength 64

#define DataQuantum 32

static char zero[DataQuantum];

#define MIN_CACHE_SIZE 100
#define MAX_CACHE_SIZE 1000

#define forever for(;;)

#define Quantize(X) ((((X) + DataQuantum - 1) / DataQuantum) * DataQuantum)
static dsstatus store_save_data(dsstore *, dsrecord *, dsdata *);
static dsstatus dsstore_init(dsstore *s, u_int32_t dirty);
u_int32_t dsstore_max_id_internal(dsstore *s, u_int32_t lock);
dsstatus dsstore_save_internal(dsstore *s, dsrecord *r, u_int32_t lock);
dsstatus dsstore_remove_internal(dsstore *s, u_int32_t dsid, u_int32_t lock);
static dsrecord *dsstore_fetch_internal(dsstore *s, u_int32_t dsid, u_int32_t lock);

/*
 * Index file contains these entries
 *     dsid = record id
 *     vers = record version
 *     size = quantized size
 *     where = index into Store.<size> file for this record
 *
 * Update INDEX_ENTRY_SIZE if you change this structure.
 */

#define INDEX_ENTRY_SIZE 16

typedef struct
{
	u_int32_t dsid;
	u_int32_t vers;
	u_int32_t size;
	u_int32_t where;
} store_index_entry_t;

typedef struct
{
	u_int32_t size;
	u_int32_t info;
} store_file_info_t;

#define STORE_INFO_FILE_FULL 0x00000001

/*
 * Insert a new entry into a store's memory index.
 */
static void
dsstore_index_insert(dsstore *s, store_index_entry_t *x, int replace)
{
	unsigned int i, top, bot, mid, range;
	store_index_entry_t *e, *t, *b, *m;

	if (s == NULL) return;
	if (x == NULL) return;
	if (x->dsid == IndexNull) return;
	if (x->size == 0) return;

	e = (store_index_entry_t *)malloc(INDEX_ENTRY_SIZE);
	memmove(e, x, INDEX_ENTRY_SIZE);

	if (s->index_count == 0)
	{
		s->index = (void **)malloc(sizeof(void *));
		s->index[s->index_count] = e;
		s->index_count++;
		return;
	}

	top = s->index_count - 1;
	bot = 0;
	mid = top / 2;

	range = top - bot;
	while (range > 1)
	{
		m = s->index[mid];
		if (e->dsid == m->dsid)
		{
			if (replace)
			{
				s->index[mid] = e;
				free(m);
			}
			else
			{
				free(e);
			}
			return;
		}
		else if (e->dsid < m->dsid) top = mid;
		else bot = mid;

		range = top - bot;
		mid = bot + (range / 2);
	}

	t = s->index[top];
	if (e->dsid == t->dsid)
	{
		if (replace)
		{
			s->index[top] = e;
			free(t);
		}
		else
		{
			free(e);
		}
		return;
	}

	b = s->index[bot];
	if (e->dsid == b->dsid)
	{
		if (replace)
		{
			s->index[bot] = e;
			free(b);
		}
		else
		{
			free(e);
		}
		return;
	}

	if (e->dsid < b->dsid) mid = bot;
	else if (e->dsid > t->dsid) mid = top + 1;
	else mid = top;

	s->index_count++;
	s->index = (void **)realloc(s->index, sizeof(void *) * s->index_count);
	for (i = s->index_count - 1; i > mid; i--) s->index[i] = s->index[i - 1];
	s->index[mid] = e;
}

/*
 * Find an entry from a store's memory index.
 */
static u_int32_t
dsstore_index_lookup(dsstore *s, u_int32_t dsid)
{
	unsigned int top, bot, mid, range;
	store_index_entry_t *t, *b, *m;

	if (s == NULL) return IndexNull;
	if (s->index_count == 0) return IndexNull;

	top = s->index_count - 1;
	bot = 0;
	mid = top / 2;

	range = top - bot;
	while (range > 1)
	{
		m = s->index[mid];
		if (dsid == m->dsid) return mid;
		else if (dsid < m->dsid) top = mid;
		else bot = mid;

		range = top - bot;
		mid = bot + (range / 2);
	}

	t = s->index[top];
	if (dsid == t->dsid) return top;

	b = s->index[bot];
	if (dsid == b->dsid) return bot;
	return IndexNull;
}

/*
 * Delete an entry from a store's memory index.
 */
static void
dsstore_index_delete(dsstore *s, u_int32_t dsid)
{
	u_int32_t where, i;

	if (s == NULL) return;
	where = dsstore_index_lookup(s, dsid);
	if (where == IndexNull) return;

	free(s->index[where]);

	if (s->index_count == 1)
	{
		s->index_count = 0;
		free(s->index);
		s->index = NULL;
		return;
	}

	for (i = where + 1; i < s->index_count; i++)
		s->index[i - 1] = s->index[i];

	s->index_count--;
	s->index = (void **)realloc(s->index, s->index_count * sizeof(void *));	
}

#ifdef NOTDEF
/*
 * Find an entry with a specific version in the store's memory index.
 */
static u_int32_t
dsstore_version_lookup(dsstore *s, u_int32_t vers)
{
	u_int32_t i;
	store_index_entry_t *e;

	for (i = 0; i < s->index_count; i++)
	{
		e = s->index[i];
		if (e->vers == vers) return i;
	}

	return IndexNull;
}
#endif

/*
 * Lock the store.
 */
static dsstatus
dsstore_lock(dsstore *s, int block)
{
	int status, op;

	if (s == NULL) return DSStatusInvalidStore;

	op = LOCK_EX;
	if (block == 0) op |= LOCK_NB;

	status = flock(s->store_lock, op);
	if ((status < 0) && (errno == EWOULDBLOCK)) return DSStatusLocked;
	else if (status < 0) return DSStatusInvalidStore;

	return DSStatusOK;
}

/*
 * Unlock the store.
 */
static void
dsstore_unlock(dsstore *s)
{
	if (s == NULL) return;
	flock(s->store_lock, LOCK_UN);
}

/*
 * Stat a file in the store.
 */
static int
dsstore_stat(dsstore *s, char *name, struct stat *sb)
{
	char path[MAXPATHLEN + 1];

	if (s == NULL) return stat(name, sb);
	
	if ((strlen(s->dsname) + strlen(name) + 1) > MAXPATHLEN)
		return DSStatusInvalidStore;
	sprintf(path, "%s/%s", s->dsname, name);
	return stat(path, sb);
}

static dsstatus
dsstore_sync(dsstore *s)
{
	struct stat sb;
	u_int32_t i;
	u_int32_t status, check, vers;

	if (s == NULL) return DSStatusInvalidStore;

	check = 1;
	vers = 0;

	status = notify_check(s->notify_token, &check);
	if (status != NOTIFY_STATUS_OK) check = 1;

	status = notify_get_state(s->notify_token, &vers);
	if (status != NOTIFY_STATUS_OK) vers = 0;

	if ((check == 0) && (vers == s->max_vers)) return DSStatusOK;

	if (dsstore_stat(s, ConfigFileName, &sb) != 0) return DSStatusInvalidStore;
	if ((s->last_sec == sb.st_mtimespec.tv_sec) && (s->last_nsec == sb.st_mtimespec.tv_nsec)) return DSStatusOK;

	for (i = 0; i < s->index_count; i++) free(s->index[i]);
	if (s->index != NULL) free(s->index);

	for (i = 0; i < s->file_info_count; i++) free(s->file_info[i]);
	if (s->file_info != NULL) free(s->file_info);

	dscache_free(s->cache);
	s->cache_enabled = 0;

	if (s->sync_delegate != NULL) (s->sync_delegate)(s->sync_private);

	return dsstore_init(s, 1);
}

static dsstatus
dsstore_touch(dsstore *s)
{
	char path[MAXPATHLEN + 1];
	struct stat sb;

	if (s == NULL) return DSStatusInvalidStore;
	
	sprintf(path, "%s/%s", s->dsname, ConfigFileName);
	utime(path, NULL);

	if (stat(path, &sb) != 0) return DSStatusInvalidStore;

	s->last_sec = sb.st_mtimespec.tv_sec;
	s->last_nsec = sb.st_mtimespec.tv_nsec;

	return DSStatusOK;
}

/*
 * Find a record with a specific version.
 */
u_int32_t
dsstore_version_record(dsstore *s, u_int32_t vers)
{
	u_int32_t i;
	store_index_entry_t *e;

	if (s == NULL) return DSStatusInvalidStore;

	if (s->flags & DSSTORE_FLAGS_REMOTE_NETINFO)
		return nistore_version_record(s, vers);

	dsstore_lock(s, 1);
	dsstore_sync(s);
	
	for (i = 0; i < s->index_count; i++)
	{
		e = s->index[i];
		if (e->vers == vers)
		{
			dsstore_unlock(s);
			return e->dsid;
		}
	}

	dsstore_unlock(s);
	return IndexNull;
}

/*
 * Debugging utility to print the index.
 */
void
dsstore_print_index(dsstore *s, FILE *f)
{
	store_index_entry_t *e;
	int i;

	if (s == NULL) return;
	if (f == NULL) return;


	fprintf(f, "     #      ID   Vers   Size  Index\n");

	for (i = 0; i < s->index_count; i++)
	{
		e = (store_index_entry_t *)s->index[i];
		fprintf(f, "%6d: %6u %6u %6u %6u\n",
			i, e->dsid, e->vers, e->size, e->where);
	}
}

/*
 * Open a file in the store.
 */
static FILE *
dsstore_fopen(dsstore *s, char *name, char *mode)
{
	char path[MAXPATHLEN + 1];

	if (s == NULL) return fopen(name, mode);

	if ((strlen(s->dsname) + strlen(name) + 1) > MAXPATHLEN) return NULL;
	sprintf(path, "%s/%s", s->dsname, name);
	return fopen(path, mode);
}

/*
 * Unlink a file in the store.
 */
static int
dsstore_unlink(dsstore *s, char *name)
{
	char path[MAXPATHLEN + 1];

	if (s == NULL) return unlink(name);
	
	if ((strlen(s->dsname) + strlen(name) + 1) > MAXPATHLEN)
		return DSStatusWriteFailed;

	sprintf(path, "%s/%s", s->dsname, name);
	return unlink(path);
}

/*
 * Rename a file in the store.
 */
static int
dsstore_rename(dsstore *s, char *old, char *new)
{
	char pold[MAXPATHLEN + 1];
	char pnew[MAXPATHLEN + 1];

	if (s == NULL) return rename(old, new);
	
	if ((strlen(s->dsname) + strlen(old) + 1) > MAXPATHLEN)
		return DSStatusWriteFailed;

	if ((strlen(s->dsname) + strlen(new) + 1) > MAXPATHLEN)
		return DSStatusWriteFailed;

	sprintf(pold, "%s/%s", s->dsname, old);
	sprintf(pnew, "%s/%s", s->dsname, new);

	return rename(pold, pnew);
}

/*
 * Open a Store.<size> file.
 */
static FILE *
dsstore_store_fopen(dsstore *s, u_int32_t size, int flag)
{
	char path[MAXPATHLEN + 1];
	char str[40];
	struct stat sb;

	sprintf(str, "%u", size);
	if ((strlen(s->dsname) + strlen(StoreFileBase) + strlen(str) + 1) > MAXPATHLEN)
		return NULL;

	sprintf(path, "%s/%s.%u", s->dsname, StoreFileBase, size);

	if (flag == 0) return fopen(path, "r");

	if (0 != stat(path, &sb)) return fopen(path, "a+");
	return fopen(path, "r+");
}

static dsstatus
dsstore_write_index(dsstore *s)
{
	store_index_entry_t *e, x;
	int i, y;
	FILE *f;

	if (s == NULL) return DSStatusInvalidStore;

	dsstore_unlink(s, IndexFileName);
	dsstore_unlink(s, TempIndexFileName);

	f = dsstore_fopen(s, TempIndexFileName, "w");
	if (f == NULL) return DSStatusWriteFailed;

	y = IndexNull;

	for (i = 0; i < s->index_count; i++)
	{
		e = (store_index_entry_t *)s->index[i];
		
		/* More paraniod integrity checking */
		if ((i > 0) && (y >= e->dsid)) return DSStatusInvalidStore;
		if (e->size == 0) return DSStatusInvalidStore;
		y = e->dsid;

		x.dsid = htonl(e->dsid);
		x.vers = htonl(e->vers);
		x.size = htonl(e->size);
		x.where = htonl(e->where);

		if (fwrite(&x, INDEX_ENTRY_SIZE, 1, f) != 1) return DSStatusWriteFailed;
	}
	fclose(f);
	
	return dsstore_rename(s, TempIndexFileName, IndexFileName);
}

/*
 * Utilitys for checking flags.
 */
static int
dsstore_access_readwrite(dsstore *s)
{
	if ((s->flags & DSSTORE_FLAGS_ACCESS_MASK) == DSSTORE_FLAGS_ACCESS_READWRITE)
		return 1;
	return 0;
}

/*
 * Open a Data Store
 * Will creates the store if necessary.
 */
dsstatus
dsstore_new(dsstore **s, char *dirname, u_int32_t flags)
{
	int r;
	u_int32_t i, newstore;
	FILE *f;
	struct stat sb;
	char path[MAXPATHLEN + 1];
	
	newstore = 0;

	/*
	 * Make sure there's enough path space to create files in the store.
	 * ReservedFileNameLength is long enough to fit "Store." plus an
	 * integer string big enough for 128bit integer.  I.E. There is
	 * no possible chance of overflowing MAXPATHLEN, even though we
	 * explicitly check all path lengths everywhere they appear.
	 */
	if ((strlen(dirname) + ReservedFileNameLength + 1) > MAXPATHLEN)
		return DSStatusWriteFailed;

	if (0 != dsstore_stat(NULL, dirname, &sb))
	{
		newstore = 1;
		r = mkdir(dirname, DataStoreAccessMode);
		if (r != 0) return DSStatusWriteFailed;
	}

	sprintf(path, "%s/%s", dirname, IndexFileName);

	/*
	 * Don't create an empty Index if the store exists!
	 * Crash-recovery will re-create it.
	 */
	if ((newstore == 1) && (0 != dsstore_stat(NULL, path, &sb)))
	{
		f = fopen(path, "w");
		if (f == NULL) return DSStatusWriteFailed;
		fclose(f);
	}

	sprintf(path, "%s/%s", dirname, ConfigFileName);

	if (0 != dsstore_stat(NULL, path, &sb))
	{
		f = fopen(path, "w");
		if (f == NULL) return DSStatusWriteFailed;
		i = htonl(DSSTORE_VERSION);
		r = fwrite(&i, 4, 1, f);
		fclose(f);
		if (r != 1) return DSStatusWriteFailed;
	}

	return dsstore_open(s, dirname, flags);
}

static dsdata *
_dsstore_dsdata_fread(FILE *f, u_int32_t size, u_int32_t quant)
{
	dsdata *d;
	int n;
	u_int32_t len, type, x, q;

	if (f == NULL) return NULL;

	n = fread(&x, sizeof(u_int32_t), 1, f);
	if (n != 1) return NULL;
	type = ntohl(x);

	if (type == DataTypeNil) return NULL;

	if (type != DataTypeDSRecord)
	{
		syslog(LOG_ERR, "Data Store file Store.%u contains invalid record type %u", type);
		return NULL;
	}

	n = fread(&x, sizeof(u_int32_t), 1, f);
	if (n != 1) return NULL;
	len = ntohl(x);


	q = len + DSDATA_STORAGE_HEADER_SIZE;
	if (quant != 0) q = Quantize(q);

	if (q != size)
	{
		syslog(LOG_ERR, "Data Store file Store.%u contains invalid record size %u", q);
		return NULL;
	}

	d = dsdata_alloc(len);
	d->type = type;

	if (len > 0)
	{
		n = fread(d->data, d->length, 1, f);
		if (n != 1)
		{
			free(d);
			return NULL;
		}
	}

	d->retain = 1;
	
	return d;
}


static dsrecord *
_dsstore_dsrecord_fread(FILE *f, u_int32_t size, u_int32_t quant)
{
	dsdata *d;
	dsrecord *r;

	d = _dsstore_dsdata_fread(f, size, quant);
	r = dsdata_to_dsrecord(d);
	dsdata_release(d);
	return r;
}

/*
 * Reads a Store file and adds index entries.
 * Removes the store file if it is empty.
 */
static dsstatus
index_recovery_read(dsstore *s, u_int32_t size)
{
	FILE *f;
	u_int32_t  where;
	off_t offset;
	store_index_entry_t e;
	dsrecord *r;
	int count, is_empty;
	struct stat sb;
	char store[64];

	if (s == NULL) return DSStatusInvalidStore;
	if (size == 0) return DSStatusOK;

	sprintf(store, "%s.%d", StoreFileBase, size);
	if (dsstore_stat(s, store, &sb) != 0) return DSStatusReadFailed;

	e.size = size;
	count = sb.st_size / size;

	f = dsstore_fopen(s, store, "r");
	if (f == NULL) return DSStatusReadFailed;

	is_empty = 1;
	r = NULL;

	offset = 0;
	for (e.where = 0; e.where < count; e.where++)
	{
		if (fseek(f, offset, SEEK_SET) != 0)
		{
			syslog(LOG_ERR, "%s fseek %q status %s", store, offset, strerror(errno));
			break;
		}

		offset += size;

		r = _dsstore_dsrecord_fread(f, size, 1);
		if (r == NULL) continue;

		if (r->dsid != IndexNull) is_empty = 0;

		where = dsstore_index_lookup(s, r->dsid);
		if (where != IndexNull)
		{
			dsrecord_release(r);
			return DSStatusDuplicateRecord;
		}

		e.dsid = r->dsid;
		e.vers = r->vers;
		dsstore_index_insert(s, &e, 0);

		s->nichecksum += ((r->dsid + 1) * r->serial);

		if (e.vers > s->max_vers) s->max_vers = e.vers;

		dsrecord_release(r);
	}

	fclose(f);

	if (is_empty) return DSStatusNoData;
	return DSStatusOK;
}

/*
 * Recreates the Index file by reading all the store files.
 * Also calculates the nichecksum.
 */
static dsstatus
index_recovery(dsstore *s)
{
	DIR *dp;
	struct direct *d;
	int i, len;
	u_int32_t size;
	dsstatus status;
	int *unlink_list, unlink_count;
	char store[64], *p;

	if (s == NULL) return DSStatusInvalidStore;

	dp = opendir(s->dsname);
	if (dp == NULL) return DSStatusReadFailed;

	unlink_count = 0;
	unlink_list = NULL;

	len = strlen(StoreFileBase);

	while ((d = readdir(dp)))
	{
		if (!strncmp(d->d_name, StoreFileBase, len))
		{
			p = strrchr(d->d_name, '.');
			if (p == NULL) continue;
			size = atoi(p+1);
			if (size == 0) continue;

			status = index_recovery_read(s, size);
			if (status == DSStatusNoData)
			{
				if (unlink_count == 0)
				{
					unlink_list = (int *)malloc(sizeof(int));
				}
				else
				{
					unlink_list = (int *)realloc(unlink_list, (unlink_count + 1) * sizeof(int));
				}
				unlink_list[unlink_count] = size;
				unlink_count++;
			}
			else if (status != DSStatusOK)
			{
				closedir(dp);
				if (unlink_count > 0) free(unlink_list);
				return status;
			}
		}
	}

	closedir(dp);
	
	for (i = 0; i < unlink_count; i++)
	{
		sprintf(store, "%s.%d", StoreFileBase, unlink_list[i]);
		dsstore_unlink(s, store);
	}

	if (unlink_count > 0) free(unlink_list);

	return DSStatusOK;
}

/*
 * Writes to the data store are done in four steps:
 * 1: A copy of the new record is written to the Temp file.
 * 2: Temp is renamed as Create.
 * 3: The record is written into the Store file.
 * 4: The Create file is deleted.
 *
 * If a write failed while the record was being written to the Temp
 * file, or before the Temp file was renamed, we recover by removing
 * the Temp file. The data store reverts to the old version of the record.
 * 
 * If the Create file exists, we can be sure that its contents were correctly
 * written.  The write to the store may have failed, so the old or possibly
 * corrupted store record is deleted and the record is copied from the Create
 * file to the store.
 */
static dsstatus
create_recovery(dsstore *s, u_int32_t size)
{
	dsrecord *r;
	u_int32_t dsid, pdsid, i, len, parent_ok;
	dsdata *d;
	FILE *f;
	dsstatus status;

	/* Read record from Create file */
	f = dsstore_fopen(s, CreateFileName, "r");
	if (f == NULL)
	{
		/* Can't recover record */
		dsstore_unlink(s, CreateFileName);
		return DSStatusWriteFailed;
	}

	r = _dsstore_dsrecord_fread(f, size, 0);
	fclose(f);
	if (r == NULL)
	{
		/* Can't recover record */
		dsstore_unlink(s, CreateFileName);
		return DSStatusWriteFailed;
	}

	/* Delete record from store */
	status = dsstore_remove_internal(s, r->dsid, 0);
	if (status == DSStatusInvalidPath)
	{
	}
	else if (status != DSStatusOK)
	{
		/* This should never happen! */
		dsstore_unlink(s, CreateFileName);
		return DSStatusWriteFailed;
	}

	dsid = r->dsid;
	pdsid = r->super;
	
	/* Write data to store */	
	d = dsrecord_to_dsdata(r);
	status = store_save_data(s, r, d);
	dsdata_release(d);
	if (status != DSStatusOK) return DSStatusWriteFailed;
	
	r = dsstore_fetch_internal(s, pdsid, 0);
	if (r == NULL) return DSStatusWriteFailed;

	parent_ok = 0;
	len = r->sub_count;
	for (i = 0; i < len; i++) if (r->sub[i] == dsid) parent_ok = 1;

	/* Update parent */
	if (parent_ok == 0)
	{
		dsrecord_append_sub(r, dsid);
		status = dsstore_save_internal(s, r, 0);
		dsrecord_release(r);
		if (status != DSStatusOK) return DSStatusWriteFailed;
	}
	else
	{
		dsrecord_release(r);
	}

	dsstore_unlink(s, CreateFileName);

	return DSStatusOK;
}

/*
 * Deletions from the data store are done in two steps.  First the
 * record ID is written to the Delete file, then the record is
 * deleted from the Store file.  If the deletion failed before the ID
 * is written to the Delete file, the data store reverts to the old
 * version of the record.  If the Delete file was written and the
 * deletion from the store failed, the store record is deleted.
 */
static dsstatus
delete_recovery(dsstore *s)
{
	int ret;
	u_int32_t dsid;
	dsstatus status;
	FILE *f;

	f = dsstore_fopen(s, DeleteFileName, "r");
	if (f == NULL)
	{
		dsstore_unlink(s, DeleteFileName);
		return DSStatusWriteFailed;
	}

	ret = fread(&dsid, sizeof(u_int32_t), 1, f);
	fclose(f);
	if (ret != 1)
	{
		dsstore_unlink(s, DeleteFileName);
		return DSStatusWriteFailed;
	}

	status = dsstore_remove_internal(s, dsid, 0);
	if (status == DSStatusInvalidPath) status = DSStatusOK;

	dsstore_unlink(s, DeleteFileName);
	return status;
}

/*
 * Recursively check the tree starting at the root directory, 
 * checking "super" back pointers.
 * If the tree is repaired at any point, the check re-starts at the root.
 */
#define TreeCheckOK 0
#define TreeCheckUpdateFailed 1
#define TreeCheckOrphanedChild 2
#define TreeCheckKidnappedChild 3

static u_int32_t
tree_check_child(dsstore *s, dsrecord *parent, dsrecord *child, int write_allowed)
{
	dsrecord *r;
	dsstatus status, status1;

	if (child->super == parent->dsid) return TreeCheckOK;
	if (write_allowed == 0) return TreeCheckUpdateFailed;

	/*
	 * Child doesn't know this parent. The child's link to it's parent
	 * overrides the parent's link to a child.
	 */
	r = dsstore_fetch_internal(s, child->super, 0);
	if (r == NULL)
	{
		/*
		 * "Real" parent doesn't exist.  We link the child to the
		 * parent that claimed it.
		 */
		child->super = parent->dsid;
		status = dsstore_save_internal(s, child, 0);
		if (status == DSStatusOK) return TreeCheckOrphanedChild;
		return TreeCheckUpdateFailed;
	}

	/* Remove child from "false" parent. */
	dsrecord_remove_sub(parent, child->dsid);
	status = dsstore_save_internal(s, parent, 0);

	/* Make sure child is attached to real parent. */
	dsrecord_append_sub(r, child->dsid);
	status1 = dsstore_save_internal(s, r, 0);

	dsrecord_release(r);

	if ((status == DSStatusOK) && (status1 == DSStatusOK))
		return TreeCheckKidnappedChild;
	return TreeCheckUpdateFailed;
}

static u_int32_t
tree_check_record(dsstore *s, dsrecord *r, int write_allowed)
{
	u_int32_t i, tc;
	dsrecord *c;
	dsstatus status;

	for (i = 0; i < r->sub_count; i++)
	{
		c = dsstore_fetch_internal(s, r->sub[i], 0);
		if (c == NULL)
		{
			if (write_allowed == 0) return TreeCheckUpdateFailed;
			dsrecord_remove_sub(r, r->sub[i]);
			status = dsstore_save_internal(s, r, 0);
			if (status == DSStatusOK) 
			{
				/* Backup by one since r->sub[i] was removed. */
				i -= 1;
				continue;
			}
			return TreeCheckUpdateFailed;
		}

		tc = tree_check_child(s, r, c, write_allowed);
		/* If the child was an orphan, we can continue checking. */
		if (tc == TreeCheckOrphanedChild) tc = TreeCheckOK;
		if (tc != TreeCheckOK)
		{
			dsrecord_release(c);
			return tc;
		}
		
		tc = tree_check_record(s, c, write_allowed);
		dsrecord_release(c);
		/* Must fail back to the root dir, since the tree changed */
		if (tc != TreeCheckOK)
		return tc;
	}

	return TreeCheckOK;
}
			
static dsstatus
tree_check(dsstore *s, int write_allowed)
{
	dsrecord *r;
	u_int32_t tc;

	if (s->index_count == 0) return DSStatusOK;

	r = dsstore_fetch_internal(s, 0, 0);
	if (r == NULL) return DSStatusInvalidStore;

	tc = -1;
	while (tc != TreeCheckOK)
	{
		tc = tree_check_record(s, r, write_allowed);
		if (tc == TreeCheckUpdateFailed) return DSStatusInvalidStore;
	}

	dsrecord_release(r);

	return DSStatusOK;
}

static dsstatus
connection_check_record(dsstore *s, u_int32_t dsid, char *test)
{
	dsrecord *r, *p;
	u_int32_t c, n, i;
	dsstatus status;

	if (dsid == 0) return DSStatusOK;

	r = dsstore_fetch_internal(s, dsid, 0);
	if (r == NULL) return DSStatusInvalidRecordID;

	c = r->dsid;
	n = r->super;
	
	forever
	{
		if (n == dsid)
		{
			/* A cycle - break it and attach to root */
			n = 0;
			r->super = 0;
			status = dsstore_save_internal(s, r, 0);
		}

		p = dsstore_fetch_internal(s, n, 0);
		if (p == NULL)
		{
			/* Parent doesn't exist - attach to root */
			p = dsstore_fetch_internal(s, 0, 0);
		}

		if (dsrecord_has_sub(p, c) == 0)
		{
			dsrecord_append_sub(p, c);
			status = dsstore_save_internal(s, p, 0);
		}
		
		i = dsstore_index_lookup(s, n);
		if (i >= s->index_count) i = 0;
		c = n;
		n = p->super;

		dsrecord_release(r);
		r = p;

		if ((c == 0) || (test[i] != 0))
		{
			dsrecord_release(p);
			return DSStatusOK;
		}

		test[i] = 1;
	}

	return DSStatusOK;
}

static dsstatus
connection_check(dsstore *s)
{
	u_int32_t i;
	char *test;
	store_index_entry_t *e;
	dsstatus x, status;

	test = NULL;
	if (s->index_count > 0)
	{
		test = malloc(s->index_count);
		memset(test, 0, s->index_count);
	}

	x = DSStatusOK;
	for (i = 0; i < s->index_count; i++)
	{
		if (test[i] != 0) continue; 
		e = s->index[i];
		test[i] = 1;
		status = connection_check_record(s, e->dsid, test);
		if (status != DSStatusOK) x = status;
	}

	free(test);

	return x;
}

static dsstatus
dsstore_read_index(dsstore *s)
{
	struct stat sb;
	u_int32_t i, count, where, x;
	store_index_entry_t e;
	FILE *ifp;

	if (dsstore_stat(s, IndexFileName,  &sb) != 0)
	{
		/* This should never happen! */
		return DSStatusReadFailed;
	}

	count = sb.st_size / INDEX_ENTRY_SIZE;

	ifp = dsstore_fopen(s, IndexFileName, "r");
	if (ifp == NULL) return DSStatusReadFailed;

	x = IndexNull;

	for (i = 0; i < count; i++)
	{
		if (fread(&e, INDEX_ENTRY_SIZE, 1, ifp) != 1)
			return DSStatusReadFailed;

		e.dsid = ntohl(e.dsid);

		/* Index entries should be sorted */
		if ((i > 0) && (x >= e.dsid)) return DSStatusInvalidStore;
		x = e.dsid;

		where = dsstore_index_lookup(s, e.dsid);
		if (where != IndexNull) return DSStatusDuplicateRecord;

		e.vers = ntohl(e.vers);
		if (e.vers > s->max_vers) s->max_vers = e.vers;

		e.size = ntohl(e.size);
		/* Zero size is impossible */
		if (e.size == 0) return DSStatusInvalidStore;

		e.where = ntohl(e.where);

		dsstore_index_insert(s, &e, 0);
	}

	fclose(ifp);

	return DSStatusOK;
}

static void
dsstore_set_file_full(dsstore *s, u_int32_t size, u_int32_t is_full)
{
	u_int32_t i;
	store_file_info_t *finfo;

	for (i = 0; i < s->file_info_count; i++)
	{
		finfo = s->file_info[i];
		if (finfo->size == size)
		{
			if (is_full) finfo->info |= STORE_INFO_FILE_FULL;
			else finfo->info &= ~STORE_INFO_FILE_FULL;
			return;
		}
	}

	finfo = (store_file_info_t *)calloc(1, sizeof(store_file_info_t));
	finfo->size = size;
	if (is_full) finfo->info = STORE_INFO_FILE_FULL;

	if (s->file_info_count == 0)
	{
		s->file_info = malloc(sizeof(store_file_info_t **));
	}
	else
	{
		s->file_info = realloc(s->file_info, (s->file_info_count + 1) * sizeof(store_file_info_t **));
	}

	s->file_info[s->file_info_count] = finfo;
	s->file_info_count++;
}

static u_int32_t
dsstore_is_file_full(dsstore *s, u_int32_t size)
{
	u_int32_t i;
	store_file_info_t *finfo;

	for (i = 0; i < s->file_info_count; i++)
	{
		finfo = s->file_info[i];
		if (finfo->size == size)
		{
			if (finfo->info & STORE_INFO_FILE_FULL) return 1;
			return 0;
		}
	}

	return 0;
}

/*
 * Internal initializations.
 * - does create and delete disaster recovery
 * - reads the Clean file (contains nichecksum)
 * - reads the Index file
 * - re-creates the Index if necessary
 */
static dsstatus
dsstore_init(dsstore *s, u_int32_t dirty)
{
	dsstatus status;
	struct stat sb;
	u_int32_t i, where, size;
	int write_allowed;
	FILE *f;

	s->max_vers = 0;
	s->nichecksum = 0;

	s->index = NULL;
	s->index_count = 0;

	s->file_info = NULL;
	s->file_info_count = 0;

	write_allowed = dsstore_access_readwrite(s);

	if (dsstore_stat(s, ConfigFileName, &sb) != 0) return DSStatusInvalidStore;

	s->last_sec = sb.st_mtimespec.tv_sec;
	s->last_nsec = sb.st_mtimespec.tv_nsec;

	/* If Create or Delete file exists, we had a crash */
	if ((dirty == 0) && (dsstore_stat(s, CreateFileName, &sb) == 0)) dirty++;
	if ((dirty == 0) && (dsstore_stat(s, DeleteFileName, &sb) == 0)) dirty++;

	/* Can't start up read-only until someone has done crash-recovery */
	if ((dirty != 0) && (!write_allowed)) return DSStatusInvalidStore;

	/* If Clean file is missing or zero size, store is dirty */
	if (dirty == 0)
	{
		if (dsstore_stat(s, CleanFileName,  &sb) != 0) dirty++;
		else if (sb.st_size != sizeof(u_int32_t)) dirty++;
	}

	/* If Index file is missing or zero size, store is dirty */
	if (dirty == 0)
	{
		if (dsstore_stat(s, IndexFileName,  &sb) != 0) dirty++;
		else if (sb.st_size == 0) dirty++;
	}

	if (dirty == 0)
	{
		/* Store appears clean.  Read nichecksum from the Clean file */
		f = dsstore_fopen(s, CleanFileName, "r");
		i = fread(&(s->nichecksum), sizeof(u_int32_t), 1, f);
		fclose(f);
		if (i != 1) dirty++;
	}
	
	if (dirty == 0)
	{
		/* Store appears clean.  Read Index file */
		status = dsstore_read_index(s);
		if (status != DSStatusOK)
		{
			/* Index was corrupt.  Clean up from the failed read */
			for (i = 0; i < s->index_count; i++) free(s->index[i]);
			if (s->index != NULL) free(s->index);
			s->index = NULL;
			s->index_count = 0;
			dirty++;
		}
	}

	if (dirty != 0)
	{
		/* Store is dirty. Read the store to recover the Index */
		/* N.B. index_recovery() sets nichecksum */
		status = index_recovery(s);
		if (status != DSStatusOK) return status;
	}	

	if (write_allowed)
	{
		/* If Temp file exists, we remove it */
		dsstore_unlink(s, TempFileName);
		dsstore_unlink(s, TempIndexFileName);

		/* Delete Index file.  We rewrite it from memory on shutdown. */
		dsstore_unlink(s, IndexFileName);
	}

	/* Check for Create file */
	if (dsstore_stat(s, CreateFileName, &sb) == 0)
	{
		/* Complete create operation */
		status = create_recovery(s, sb.st_size);
		if (status != DSStatusOK) return status;
	}

	/* Check for Delete file */
	if (dsstore_stat(s, DeleteFileName, &sb) == 0)
	{
		/* Complete delete operation */
		status = delete_recovery(s);
		if (status != DSStatusOK) return status;
	}

	/* If the store was dirty, check the tree */
	if (dirty != 0)
	{
		status = tree_check(s, write_allowed);
		if (status != DSStatusOK) return status;

		status = connection_check(s);
		if (status != DSStatusOK) return status;
	}

	/* Check for root */
	if (s->index_count > 0)
	{
		where = dsstore_index_lookup(s, 0);
		if (where == IndexNull) return DSStatusNoRootRecord;
	}

	if ((s->flags & DSSTORE_FLAGS_CACHE_MASK) == DSSTORE_FLAGS_CACHE_ENABLED)
	{
		/*
		 * Set cache size to be 10% of the store size, but
		 * not less than the minimum (MIN_CACHE_SIZE), and
		 * not greater than the maximum (MAX_CACHE_SIZE).
		 */
		size = s->index_count / 10;
		if (size < MIN_CACHE_SIZE) size = MIN_CACHE_SIZE;
		if (size > MAX_CACHE_SIZE) size = MAX_CACHE_SIZE;

		s->cache_enabled = 1;
		s->cache = dscache_new(size);
	}

	return DSStatusOK;
}

/*
 * Open an existing Data Store
 */
dsstatus
dsstore_open(dsstore **s, char *dirname, u_int32_t flags)
{
	dsstatus status;
	char *p, *dot, *path;

	if (flags & DSSTORE_FLAGS_REMOTE_NETINFO)
		return nistore_open(s, dirname, flags);

	memset(zero, 0, DataQuantum);

	*s = (dsstore *)calloc(1, sizeof(dsstore));

	asprintf(&((*s)->dsname), "%s", dirname);

	(*s)->flags = flags;

	(*s)->sync_delegate = NULL;
	(*s)->sync_private = NULL;

	asprintf(&path, "%s/%s", dirname, ConfigFileName);
	(*s)->store_lock = open(path, O_RDONLY, 0);
	free(path);

	dsstore_lock(*s, 1);
	status = dsstore_init(*s, 0);

	if (flags & DSSTORE_FLAGS_NOTIFY_CHANGES)
	{
		p = strrchr(dirname, '/');
		if (p != NULL) p++;
		else p = dirname;

		dot = strrchr(p, '.');
		if ((dot != NULL)  && (!strncmp(dot, ".nidb", 5))) *dot = 0;

		asprintf(&((*s)->notification_name), "%s.%s.%s", NETINFO_NOTIFY_PREFIX, p, NETINFO_NOTIFY_SUFFIX);
		notify_register_check((*s)->notification_name, &((*s)->notify_token));
		notify_set_state((*s)->notify_token, (*s)->max_vers);
		if (dot != NULL) *dot = '.';
	}

	dsstore_unlock(*s);

	return status;
}

void
dsstore_set_notification_name(dsstore *s, const char *n)
{
	if (s == NULL) return;

	if (!(s->flags & DSSTORE_FLAGS_NOTIFY_CHANGES)) return;

	if (s->notification_name != NULL)
	{
		free(s->notification_name);
		s->notification_name = NULL;
		notify_cancel(s->notify_token);
	}

	if (n != NULL)
	{
		s->notification_name = strdup(n);
		notify_register_check(s->notification_name, &(s->notify_token));
		notify_set_state(s->notify_token, s->max_vers);
	}
}

void
dsstore_notify(dsstore *s)
{
	if (s == NULL) return;
	if (s->notification_name == NULL) return;
	if (!(s->flags & DSSTORE_FLAGS_NOTIFY_CHANGES)) return;

	notify_set_state(s->notify_token, s->max_vers);
	notify_post(s->notification_name);
}

dsstatus
dsstore_close(dsstore *s)
{
	u_int32_t i;
	struct stat sb;
	FILE *f;
	dsstatus status;

	if (s == NULL) return DSStatusInvalidStore;

	if (s->flags & DSSTORE_FLAGS_REMOTE_NETINFO)
		return nistore_close(s);

	dsstore_lock(s, 1);

	if (dsstore_stat(s, ConfigFileName, &sb) != 0)
	{
		dsstore_unlock(s);
		return DSStatusInvalidStore;
	}

	if ((s->last_sec != sb.st_mtimespec.tv_sec) || (s->last_nsec != sb.st_mtimespec.tv_nsec))
	{
		dsstore_unlock(s);
		return DSStatusInvalidStore;
	}

	/*
	 * Dump memory index to file.
	 */
	status = dsstore_write_index(s);

	if ((status == DSStatusOK) && dsstore_access_readwrite(s))
	{
		/* Write the Clean file */
		f = dsstore_fopen(s, CleanFileName, "w");
		i = fwrite(&(s->nichecksum), sizeof(u_int32_t), 1, f);
		fflush(f);
		fclose(f);
		if (i != 1) status = DSStatusWriteFailed;
	}

	for (i = 0; i < s->index_count; i++) free(s->index[i]);
	if (s->index != NULL) free(s->index);

	for (i = 0; i < s->file_info_count; i++) free(s->file_info[i]);
	if (s->file_info != NULL) free(s->file_info);

	dscache_free(s->cache);

	dsstore_set_notification_name(s, NULL);

	dsstore_touch(s);
	dsstore_unlock(s);
	close(s->store_lock);

	free(s->dsname);
	free(s);

	return status;
}

/*
 * Determines a record's version, quantized size, and index.
 */
static dsstatus
dsstore_index(dsstore *s, u_int32_t dsid, u_int32_t *vers, u_int32_t *size, u_int32_t *where)
{
	store_index_entry_t *e;
	u_int32_t i;

	i = dsstore_index_lookup(s, dsid);
	if (i == IndexNull) return DSStatusInvalidRecordID;

	e = (store_index_entry_t *)s->index[i];
	*vers = e->vers;
	*size = e->size;
	*where = e->where;

	return DSStatusOK;
}

/* 
 * Generate a record ID.
 * Returns smallest unused ID.  ID is added to the memory store,
 * but nothing is written to disk until dsstore_save() is called
 * with the a new record having this ID.
 */
static u_int32_t
dsstore_create_dsid(dsstore *s)
{
	u_int32_t i, n;
	store_index_entry_t *e, x;

	if (s == NULL) return IndexNull;

	if (!dsstore_access_readwrite(s)) return IndexNull;

	n = 0;
	for (i = 0; i < s->index_count; i++)
	{
		e = s->index[i];
		if (n != e->dsid) break;
		n++;
	}

	x.dsid = n;
	x.vers = IndexNull;
	x.size = IndexNull;
	x.where = IndexNull;
	dsstore_index_insert(s, &x, 0);

	return n;
}

u_int32_t
dsstore_max_id_internal(dsstore *s, u_int32_t lock)
{
	u_int32_t i, m;
	store_index_entry_t *e;

	if (s == NULL) return IndexNull;

	if (lock != 0)
	{
		dsstore_lock(s, 1);
		dsstore_sync(s);
	}

	m = 0;
	for (i = 0; i < s->index_count; i++)
	{
		e = s->index[i];
		if (e->dsid > m) m = e->dsid;
	}

	if (lock != 0) dsstore_unlock(s);
	return m;
}

u_int32_t
dsstore_max_id(dsstore *s)
{
	return dsstore_max_id_internal(s, 1);
}

u_int32_t
dsstore_version(dsstore *s)
{
	if (s == NULL) return IndexNull;

	if (s->flags & DSSTORE_FLAGS_REMOTE_NETINFO)
		return nistore_version(s);

	dsstore_lock(s, 1);
	dsstore_sync(s);
	dsstore_unlock(s);

	return s->max_vers;
}

/*
 * Add a new record to the store.
 * Do not use if the record is already in the store!
 */
static dsstatus
store_save_data(dsstore *s, dsrecord *r, dsdata *d)
{
	u_int32_t i, size, where, type, pad, setfull;
	store_index_entry_t e;
	off_t offset;
	FILE *f;
	dsstatus status;
	struct stat sb;
	char store[64];

	if (d == NULL) return DSStatusOK;
	if (r == NULL) return DSStatusOK;

	size = Quantize(dsdata_size(d));
	if (size == 0)
	{
		syslog(LOG_ERR, "store_save_data zero-length data for record %u", r->dsid);
		return DSStatusInvalidRecord;
	}

	/* Open store file */
	f = dsstore_store_fopen(s, size, 1);
	if (f == NULL)
	{
		syslog(LOG_ERR, "store_save_data %u Store.%u fopen status %s", r->dsid, size, strerror(errno));
		return DSStatusWriteFailed;
	}

	/* Find an empty slot (type == DataTypeNil) in the store file */
	where = IndexNull;
	offset = 0;
	i = 0;
	setfull = 1;

	if (dsstore_is_file_full(s, size) == 0)
	{
		while (1 == fread(&type, sizeof(u_int32_t), 1, f))
		{
			if (type == DataTypeNil)
			{
				where = i;
				break;
			}

			i++;
			offset = offset + size;
			if (fseek(f, offset, SEEK_SET) != 0)
			{
				syslog(LOG_ERR, "store_save_data %u Store.%u fseek %q status %s", r->dsid, size, offset, strerror(errno));
				fclose(f);
				return DSStatusWriteFailed;
			}
		}
	}
	else
	{
		/* No empty slots - advance to end of file */
		sprintf(store, "%s.%d", StoreFileBase, size);
		if (dsstore_stat(s, store, &sb) != 0)
		{
			syslog(LOG_ERR, "store_save_data %u Store.%u stat: %s", r->dsid, size, strerror(errno));
			fclose(f);
			return DSStatusWriteFailed;
		}
		offset = sb.st_size;
		i = offset / size;
		setfull = 0;
	}

	if (where == IndexNull)
	{
		if (setfull == 1) dsstore_set_file_full(s, size, 1);

		if (fseek(f, 0, SEEK_END) != 0)
		{
			syslog(LOG_ERR, "store_save_data %u Store.%u fseek SEEK_END status %s", r->dsid, size, strerror(errno));
			fclose(f);
			return DSStatusWriteFailed;
		}
		where = i;
	}
	else
	{
		offset = where * size;
		if (fseek(f, offset, SEEK_SET) != 0)
		{
			syslog(LOG_ERR, "store_save_data %u Store.%u fseek %q status %s", r->dsid, size, offset, strerror(errno));
			fclose(f);
			return DSStatusWriteFailed;
		}
	}

	/* Remove the Clean file when we dirty the store */
	dsstore_unlink(s, CleanFileName);
	dsstore_touch(s);

	/* write data to store file */
	status = dsdata_fwrite(d, f);
	if (status != DSStatusOK)
	{
		syslog(LOG_ERR, "store_save_data %u Store.%u dsdata_fwrite status %s", r->dsid, size, dsstatus_message(status));
		fclose(f);
		return status;
	}

	pad = size - dsdata_size(d);
	if (pad > 0)
	{
		if (fwrite(zero, pad, 1, f) != 1)
		{
			syslog(LOG_ERR, "store_save_data %u Store.%u dsdata_fwrite zero-padding status %s", r->dsid, size, dsstatus_message(status));
			fclose(f);
			return DSStatusWriteFailed;
		}
	}

	fclose(f);

	/* update memory index */
	e.dsid = r->dsid;
	e.vers = r->vers;
	e.size = size;
	e.where = where;
	dsstore_index_insert(s, &e, 1);

	/* Save in cache */
	if (s->cache_enabled == 1) dscache_save(s->cache, r);

	return DSStatusOK;
}

/*
 * 1: Write record to Temp file
 * 2: Rename Temp as Create
 * 3: Delete existing record from store
 * 4: Write new record to store
 * 5: Remove Create file
 */
dsstatus
dsstore_save_internal(dsstore *s, dsrecord *r, u_int32_t lock)
{
	FILE *f;
	dsdata *d;
	dsstatus status;
	dsrecord *curr;
	u_int32_t serial;

	if (s == NULL) return DSStatusInvalidStore;

	if (s->flags & DSSTORE_FLAGS_REMOTE_NETINFO)
		return nistore_save(s, r);

	if (!dsstore_access_readwrite(s)) return DSStatusWriteFailed;
	if (r == NULL) return DSStatusOK;

	if (lock != 0)
	{
		dsstore_lock(s, 1);
		dsstore_sync(s);
	}

	/*  dsid == IndexNull means a new record */
	if (r->dsid == IndexNull)
	{
		/* Set record ID if it is a new record */
		r->dsid = dsstore_create_dsid(s);
	}
	else
	{
		/* Check if the record is stale */
		curr = dsstore_fetch_internal(s, r->dsid, 0);
		if (curr == NULL)
		{
			syslog(LOG_ERR, "dsstore_save_internal fetch failed for %u", r->dsid);
			if (lock != 0) dsstore_unlock(s);
			return DSStatusInvalidStore;
		}

		serial = curr->serial;
		dsrecord_release(curr);
		if (r->serial != serial)
		{
			syslog(LOG_DEBUG, "dsstore_save_internal %u was stale (this is not an error)", r->dsid);
			if (lock != 0) dsstore_unlock(s);
			return DSStatusStaleRecord;
		}
	}

	s->save_count++;

	/* Update version numbers */
	s->max_vers++;
	r->vers = s->max_vers;
	r->serial++;

	/* Update nichecksum */
	s->nichecksum += ((r->dsid + 1) * r->serial);

	d = dsrecord_to_dsdata(r);
	if (d == NULL)
	{
		syslog(LOG_ERR, "dsstore_save_internal record conversion failed for %u", r->dsid);
		if (lock != 0) dsstore_unlock(s);
		return DSStatusInvalidRecord;
	}

	dsstore_touch(s);

	/* Write record to Temp file */
	f = dsstore_fopen(s, TempFileName, "w");
	if (f == NULL)
	{
		syslog(LOG_ERR, "dsstore_save_internal %u fopen %s failed: %s", r->dsid, TempFileName, strerror(errno));
		dsdata_release(d);
		if (lock != 0) dsstore_unlock(s);
		return DSStatusWriteFailed;
	}

	/* Remove the Clean file when we dirty the store */
	dsstore_unlink(s, CleanFileName);
	dsstore_touch(s);

	status = dsdata_fwrite(d, f);
	fclose(f);
	if (status != DSStatusOK)
	{
		syslog(LOG_ERR, "dsstore_save_internal %u fwrite %s failed: %s (%s)", r->dsid, TempFileName, strerror(errno), dsstatus_message(status));
		dsdata_release(d);
		dsstore_unlink(s, TempFileName);
		if (lock != 0) dsstore_unlock(s);
		return status;
	}

	/* Rename Temp file as Create file */
	status = dsstore_rename(s, TempFileName, CreateFileName);
	if (status != DSStatusOK)
	{
		syslog(LOG_ERR, "dsstore_save_internal %u rename %s %s failed: %s (%s)", r->dsid, TempFileName, CreateFileName, strerror(errno), dsstatus_message(status));
		dsdata_release(d);
		if (lock != 0) dsstore_unlock(s);
		return status;
	}

	/* Delete record from store */
	status = dsstore_remove_internal(s, r->dsid, 0);
	if (status == DSStatusInvalidPath)
	{
	}
	else if (status != DSStatusOK)
	{
		syslog(LOG_ERR, "dsstore_save_internal remove %u failed: %s", r->dsid, dsstatus_message(status));
		dsdata_release(d);
		if (lock != 0) dsstore_unlock(s);
		return status;
	}

	/* Write data to store */
	status = store_save_data(s, r, d);
	dsdata_release(d);
	if (status != DSStatusOK)
	{
		syslog(LOG_ERR, "dsstore_save_internal save %u failed: %s", r->dsid, dsstatus_message(status));
		if (lock != 0) dsstore_unlock(s);
		return status;
	}

	/* Remove Create file */
	dsstore_unlink(s, CreateFileName);

	if (lock != 0) dsstore_unlock(s);
	return DSStatusOK;
}

/*
 * 1: Delete existing record from store
 * 2: Write new record to store
 */
dsstatus
dsstore_save_fast(dsstore *s, dsrecord *r, u_int32_t lock)
{
	dsdata *d;
	dsstatus status;
	dsrecord *curr;
	u_int32_t serial;

	if (s == NULL) return DSStatusInvalidStore;

	if (s->flags & DSSTORE_FLAGS_REMOTE_NETINFO)
		return nistore_save(s, r);

	if (!dsstore_access_readwrite(s)) return DSStatusWriteFailed;
	if (r == NULL) return DSStatusOK;

	if (lock != 0)
	{
		dsstore_lock(s, 1);
		dsstore_sync(s);
	}

	/*  dsid == IndexNull means a new record */
	if (r->dsid == IndexNull)
	{
		/* Set record ID if it is a new record */
		r->dsid = dsstore_create_dsid(s);
	}
	else
	{
		/* Check if the record is stale */
		curr = dsstore_fetch_internal(s, r->dsid, 0);
		if (curr == NULL)
		{
			syslog(LOG_ERR, "dsstore_save_fast fetch failed for %u", r->dsid);
			if (lock != 0) dsstore_unlock(s);
			return DSStatusInvalidStore;
		}

		serial = curr->serial;
		dsrecord_release(curr);
		if (r->serial != serial)
		{
			syslog(LOG_DEBUG, "dsstore_save_fast %u was stale (this is not an error)", r->dsid);
			if (lock != 0) dsstore_unlock(s);
			return DSStatusStaleRecord;
		}
	}

	s->save_count++;

	/* Update version numbers */
	s->max_vers++;
	r->vers = s->max_vers;
	r->serial++;

	/* Update nichecksum */
	s->nichecksum += ((r->dsid + 1) * r->serial);

	d = dsrecord_to_dsdata(r);
	if (d == NULL)
	{
		syslog(LOG_ERR, "dsstore_save_fast record conversion failed for %u", r->dsid);
		if (lock != 0) dsstore_unlock(s);
		return DSStatusInvalidRecord;
	}

	/* Remove the Clean file when we dirty the store */
	dsstore_unlink(s, CleanFileName);
	dsstore_touch(s);

	/* Delete record from store */
	status = dsstore_remove_internal(s, r->dsid, 0);
	if (status == DSStatusInvalidPath)
	{
	}
	else if (status != DSStatusOK)
	{
		syslog(LOG_ERR, "dsstore_save_fast remove %u failed: %s", r->dsid, dsstatus_message(status));
		dsdata_release(d);
		if (lock != 0) dsstore_unlock(s);
		return status;
	}

	/* Write data to store */
	status = store_save_data(s, r, d);
	dsdata_release(d);
	if (status != DSStatusOK)
	{
		syslog(LOG_ERR, "dsstore_save_fast save %u failed: %s", r->dsid, dsstatus_message(status));
		if (lock != 0) dsstore_unlock(s);
		return status;
	}

	if (lock != 0) dsstore_unlock(s);

	return DSStatusOK;
}

dsstatus
dsstore_save(dsstore *s, dsrecord *r)
{
	return dsstore_save_internal(s, r, 1);
}

/*
 * Like dsstore_save() but does not write temporary files nor
 * does it update the record serial number.
 */
dsstatus
dsstore_save_copy(dsstore *s, dsrecord *r)
{
	dsdata *d;
	dsstatus status;

	if (s == NULL) return DSStatusInvalidStore;

	if (s->flags & DSSTORE_FLAGS_REMOTE_NETINFO)
		return nistore_save(s, r);

	if (!dsstore_access_readwrite(s)) return DSStatusWriteFailed;
	if (r == NULL) return DSStatusOK;

	dsstore_lock(s, 1);
	dsstore_sync(s);

	s->save_count++;

	/*  dsid == IndexNull means a new record */
	if (r->dsid == IndexNull)
	{
		/* Set record ID if it is a new record */
		r->dsid = dsstore_create_dsid(s);
	}

	/* Update version numbers */
	s->max_vers++;
	r->vers = s->max_vers;

	/* Update nichecksum */
	s->nichecksum += ((r->dsid + 1) * r->serial);

	d = dsrecord_to_dsdata(r);
	if (d == NULL)
	{
		syslog(LOG_ERR, "dsstore_save_copy record conversion failed for %u", r->dsid);
		dsstore_unlock(s);
		return DSStatusInvalidRecord;
	}

	/* Remove the Clean file when we dirty the store */
	dsstore_unlink(s, CleanFileName);
	dsstore_touch(s);
	
	/* Delete record from store */
	status = dsstore_remove_internal(s, r->dsid, 0);
	if (status == DSStatusInvalidPath)
	{
	}
	else if (status != DSStatusOK)
	{
		syslog(LOG_ERR, "dsstore_save_copy remove %u failed: %s", r->dsid, dsstatus_message(status));
		dsdata_release(d);
		dsstore_unlock(s);
		return status;
	}

	/* Write data to store */
	status = store_save_data(s, r, d);
	if (status != DSStatusOK)
	{
		syslog(LOG_ERR, "dsstore_save_copy save %u failed: %s", r->dsid, dsstatus_message(status));
	}

	dsdata_release(d);
	dsstore_unlock(s);

	return status;
}

dsstatus
dsstore_save_attribute(dsstore *s, dsrecord *r, dsattribute *a, u_int32_t asel)
{
	if (s == NULL) return DSStatusInvalidStore;

	if (s->flags & DSSTORE_FLAGS_REMOTE_NETINFO)
		return nistore_save_attribute(s, r, a, asel);

	return dsstore_save_internal(s, r, 1);

}

dsstatus
dsstore_authenticate(dsstore *s, dsdata *user, dsdata *password)
{
	if (s == NULL) return DSStatusInvalidStore;

	if (s->flags & DSSTORE_FLAGS_REMOTE_NETINFO)
		return nistore_authenticate(s, user, password);

	return DSStatusOK;
}

/*
 * 1: Write record id to Delete file
 * 2: delete record from store
 * 3: delete Delete file
 */
dsstatus
dsstore_remove_internal(dsstore *s, u_int32_t dsid, u_int32_t lock)
{
	u_int32_t i, size, where;
	off_t offset;
	int status;
	store_index_entry_t *e;
	char *z, fstore[64];
	FILE *f;
	struct stat sb;
	dsrecord *r;

	if (s == NULL) return DSStatusInvalidStore;

	if (s->flags & DSSTORE_FLAGS_REMOTE_NETINFO)
		return nistore_remove(s, dsid);

	if (!dsstore_access_readwrite(s)) return DSStatusWriteFailed;

	if (lock != 0)
	{
		dsstore_lock(s, 1);
		dsstore_sync(s);
	}

	i = dsstore_index_lookup(s, dsid);
	if (i == IndexNull)
	{
		syslog(LOG_ERR, "dsstore_remove_internal index lookup %u failed", dsid);
		if (lock != 0) dsstore_unlock(s);
		return DSStatusOK;
	}

	s->remove_count++;

	/* Delete from cache */
	if (s->cache_enabled == 1) dscache_remove(s->cache, dsid);

	e = s->index[i];
	size = e->size;
	where = e->where;

	dsstore_index_delete(s, dsid);

	if ((size == 0) || (size == IndexNull))
	{
		if (lock != 0) dsstore_unlock(s);
		return DSStatusOK;
	}

	f = dsstore_fopen(s, DeleteFileName, "w");
	if (f == NULL)
	{
		syslog(LOG_ERR, "dsstore_remove_internal %u fopen %s failed: %s", dsid, DeleteFileName, strerror(errno));
		if (lock != 0) dsstore_unlock(s);
		return DSStatusWriteFailed;
	}

	/* Remove the Clean file when we dirty the store */
	dsstore_unlink(s, CleanFileName);
	dsstore_touch(s);

	status = fwrite(&dsid, sizeof(u_int32_t), 1, f);
	fclose(f);
	if (status != 1)
	{
		syslog(LOG_ERR, "dsstore_remove_internal %u fwrite %s failed: %s", dsid, DeleteFileName, strerror(errno));
		dsstore_unlink(s, DeleteFileName);
		if (lock != 0) dsstore_unlock(s);
		return DSStatusWriteFailed;
	}

	sprintf(fstore, "%s.%d", StoreFileBase, size);
	if (0 != dsstore_stat(s, fstore, &sb))
	{
		/* Store.<size> doesn't exist.  This should never happen. */
		syslog(LOG_ERR, "dsstore_remove_internal %u stat %s failed: %s", dsid, fstore, strerror(errno));
		dsstore_unlink(s, DeleteFileName);
		if (lock != 0) dsstore_unlock(s);
		return DSStatusWriteFailed;
	}

	/* Open store file */
	f = dsstore_store_fopen(s, size, 1);
	if (f == NULL)
	{
		syslog(LOG_ERR, "dsstore_remove_internal %u fopen Store.%u failed: %s", dsid, size, strerror(errno));
		dsstore_unlink(s, DeleteFileName);
		if (lock != 0) dsstore_unlock(s);
		return DSStatusWriteFailed;
	}

	offset = where * size;
	status = fseek(f, offset, SEEK_SET);
	if (status != 0)
	{
		syslog(LOG_ERR, "dsstore_remove_internal %u Store.%u fseek %q failed: %s", dsid, size, offset, strerror(errno));
		fclose(f);
		dsstore_unlink(s, DeleteFileName);
		if (lock != 0) dsstore_unlock(s);
		return DSStatusWriteFailed;
	}

	/* Read record and update nichecksum */
	r = _dsstore_dsrecord_fread(f, size, 1);

	if (r == NULL)
	{
		/* Bad news - the database is corrupt */
		syslog(LOG_ERR, "dsstore_remove_internal %u Store.%u _dsstore_dsrecord_fread failed at offset %q: %s", dsid, size, offset, strerror(errno));
		return DSStatusWriteFailed;
	}
	
	if (r->dsid != dsid)
	{
		/* Bad news - the database is corrupt */
		syslog(LOG_ERR, "dsstore_remove_internal %u Store.%u _dsstore_dsrecord_fread returned incorrect ID %u at offset %q: %s", dsid, size, r->dsid, offset, strerror(errno));
		return DSStatusWriteFailed;
	}
	
	s->nichecksum -= ((dsid + 1) * r->serial);
	dsrecord_release(r);

	status = fseek(f, offset, SEEK_SET);
	if (status != 0)
	{
		syslog(LOG_ERR, "dsstore_remove_internal %u Store.%u fseek %q failed: %s", dsid, size, offset, strerror(errno));
	}

	z = malloc(size - DSDATA_STORAGE_HEADER_SIZE);
	memset(z, 0, size - DSDATA_STORAGE_HEADER_SIZE);
	i = 0;

	status = fwrite(&i, sizeof(u_int32_t), 1, f);
	if (status <= 0)
	{
		syslog(LOG_ERR, "dsstore_remove_internal %u Store.%u fwrite zero type offset %q failed: %u %s", dsid, size, offset, errno, strerror(errno));
	}

	if (status > 0) status = fwrite(&i, sizeof(u_int32_t), 1, f);
	if (status <= 0)
	{
		syslog(LOG_ERR, "dsstore_remove_internal %u Store.%u fwrite zero length offset %q failed: %u %s", dsid, size, offset, errno, strerror(errno));
	}

	if (status > 0) status = fwrite(z, size - DSDATA_STORAGE_HEADER_SIZE, 1, f);
	if (status <= 0)
	{
		syslog(LOG_ERR, "dsstore_remove_internal %u Store.%u fwrite zero data offset %q failed: %u %s", dsid, size, offset, errno, strerror(errno));
	}

	free(z);
	fclose(f);

	dsstore_unlink(s, DeleteFileName);

	dsstore_set_file_full(s, size, 0);

	if (lock != 0) dsstore_unlock(s);
	if (status <= 0) return DSStatusWriteFailed;
	return DSStatusOK;
}

dsstatus
dsstore_remove(dsstore *s, u_int32_t dsid)
{
	return dsstore_remove_internal(s, dsid, 1);
}

/* 
 * Fetch a record from the store
 */
static dsrecord *
dsstore_fetch_internal(dsstore *s, u_int32_t dsid, u_int32_t lock)
{
	u_int32_t vers;
	u_int32_t size, where;
	off_t offset;
	dsstatus status;
	dsrecord *r;
	FILE *f;

	if (s == NULL) return NULL;

	if (s->flags & DSSTORE_FLAGS_REMOTE_NETINFO)
		return nistore_fetch(s, dsid);

	if (lock != 0)
	{
		dsstore_lock(s, 1);
		dsstore_sync(s);
	}

	s->fetch_count++;

	/* Check the cache first */
	if (s->cache_enabled == 1)
	{
		r = dscache_fetch(s->cache, dsid);
		if (r != NULL)
		{
			if (lock != 0) dsstore_unlock(s);
			return r;
		}
	}
	
	status = dsstore_index(s, dsid, &vers, &size, &where);
	if (status != DSStatusOK)
	{
		if (lock != 0) dsstore_unlock(s);
		return NULL;
	}

	/* Open store file */
	f = dsstore_store_fopen(s, size, 0);
	if (f == NULL)
	{
		syslog(LOG_ERR, "dsstore_fetch_internal %u fopen Store.%u failed: %s", dsid, size, strerror(errno));
		if (lock != 0) dsstore_unlock(s);
		return NULL;
	}

	offset = where * size;
	if (fseek(f, offset, SEEK_SET) != 0)
	{
		syslog(LOG_ERR, "dsstore_fetch_internal %u Store.%u fseek %q failed: %s", dsid, size, offset, strerror(errno));
		fclose(f);
		if (lock != 0) dsstore_unlock(s);
		return NULL;
	}

	r = _dsstore_dsrecord_fread(f, size, 1);

	if (r == NULL)
	{
		/* Bad news - the database is corrupt */
		syslog(LOG_ERR, "dsstore_fetch_internal %u Store.%u _dsstore_dsrecord_fread failed at offset %q: %s", dsid, size, offset, strerror(errno));
		return NULL;
	}

	if (r->dsid != dsid)
	{
		/* Bad news - the database is corrupt */
		syslog(LOG_ERR, "dsstore_fetch_internal %u Store.%u _dsstore_dsrecord_fread returned incorrect ID %u at offset %q: %s", dsid, size, r->dsid, offset, strerror(errno));
		dsrecord_release(r);
		return NULL;
	}
	
	fclose(f);

	/* Save in cache */
	if (s->cache_enabled == 1) dscache_save(s->cache, r);
	if (lock != 0) dsstore_unlock(s);
	return r;
}

dsrecord *
dsstore_fetch(dsstore *s, u_int32_t dsid)
{
	return dsstore_fetch_internal(s, dsid, 1);
}

/* 
 * Get record's version, serial, and parent.
 */
dsstatus
dsstore_vital_statistics(dsstore *s, u_int32_t dsid, u_int32_t *vers, u_int32_t *serial, u_int32_t *super)
{
	u_int32_t v, size, where;
	off_t offset;
	dsstatus status;
	dsrecord *r;
	FILE *f;

	if (s == NULL) return DSStatusInvalidStore;

	if (vers != NULL) *vers = IndexNull;
	if (serial != NULL) *serial = IndexNull;
	if (super != NULL) *super = IndexNull;

	if (s->flags & DSSTORE_FLAGS_REMOTE_NETINFO)
		return nistore_vital_statistics(s, dsid, vers, serial, super);

	dsstore_lock(s, 1);
	dsstore_sync(s);
	
	/* Check the cache */
	if (s->cache_enabled == 1)
	{
		r = dscache_fetch(s->cache, dsid);
		if (r != NULL) 
		{
			if (vers != NULL) *vers = r->vers;
			if (serial != NULL) *serial = r->serial;
			if (super != NULL) *super = r->super;

			dsrecord_release(r);
			dsstore_unlock(s);
			return DSStatusOK;
		}
	}
	
	status = dsstore_index(s, dsid, &v, &size, &where);
	if (status != DSStatusOK)
	{
		dsstore_unlock(s);
		return status;
	}

	/* Open store file */
	f = dsstore_store_fopen(s, size, 0);
	if (f == NULL)
	{
		dsstore_unlock(s);
		return DSStatusReadFailed;
	}

	offset = where * size;
	if (fseek(f, offset, SEEK_SET) != 0)
	{
		fclose(f);
		dsstore_unlock(s);
		return DSStatusReadFailed;
	}

	status = dsrecord_fstats(f, &where, vers, serial, super);
	fclose(f);

	dsstore_unlock(s);
	return status;
}

/*
 * List child records values for a single key.
 */
dsstatus
dsstore_list(dsstore *s, u_int32_t dsid, dsdata *key, u_int32_t asel, dsrecord **list)
{
	dsrecord *r, *p;
	dsdata *d, *skey, *name, *rdn;
	dsattribute *a;
	u_int32_t i, j, k;

	if (s == NULL) return DSStatusInvalidStore;
	if (list == NULL) return DSStatusFailed;

	*list = NULL;

	if (s->flags & DSSTORE_FLAGS_REMOTE_NETINFO)
	{
		return nistore_list(s, dsid, key, asel, list);
	}

	dsstore_lock(s, 1);
	dsstore_sync(s);

	r = dsstore_fetch_internal(s, dsid, 0);
	if (r == NULL)
	{
		dsstore_unlock(s);
		return DSStatusInvalidPath;
	}

	if (r->sub_count == 0)
	{
		dsrecord_release(r);
		dsstore_unlock(s);
		return DSStatusOK;
	}

	rdn = cstring_to_dsdata("rdn");
	name = cstring_to_dsdata("name");

	*list = dsrecord_new();
	if (key != NULL)
	{
		d = cstring_to_dsdata("key");
		a = dsattribute_new(d);
		dsattribute_append(a, key);
		dsrecord_append_attribute(*list, a, SELECT_META_ATTRIBUTE);
		dsdata_release(d);
		dsattribute_release(a);
	}

	for (i = 0; i < r->sub_count; i++)
	{
		p = dsstore_fetch_internal(s, r->sub[i], 0);
		if (p == NULL)
		{
			dsdata_release(name);
			dsdata_release(rdn);
			dsrecord_release(r);
			dsstore_unlock(s);
			return DSStatusInvalidPath;
		}

		skey = key;
		if (skey == NULL)
		{
			/* Use record's rdn meta-attribute for a key */
			a = dsrecord_attribute(p, rdn, SELECT_META_ATTRIBUTE);
			if ((a != NULL) && (a->count > 0)) skey = a->value[0];
			else skey = name;
		}

		for (j = 0; j < p->count; j++)
		{
			if (dsdata_equal(skey, p->attribute[j]->key))
			{
				d = int32_to_dsdata(r->sub[i]);
				a = dsattribute_new(d);
				dsdata_release(d);
				dsrecord_append_attribute(*list, a, SELECT_ATTRIBUTE);

				a->count = p->attribute[j]->count;
				if (a->count > 0)
					a->value = (dsdata **)malloc(a->count * sizeof(dsdata *));

				for (k = 0; k < a->count; k++)
					a->value[k] = dsdata_retain(p->attribute[j]->value[k]);

				dsattribute_release(a);
			}
		}

		dsrecord_release(p);
	}

	dsrecord_release(r);
	dsstore_unlock(s);
	dsdata_release(rdn);
	dsdata_release(name);

	return DSStatusOK;
}

/*
 * Find a child record with the specified key (or meta-key) and value.
 * Input parameters: dsid is the ID of the parent record. key is the
 * attribute key or meta-key (as determined by asel), and val is the 
 * value to match.  The ID of the first record that matches is returned
 * in "match".
 */
dsstatus
dsstore_match(dsstore *s, u_int32_t dsid, dsdata *key, dsdata *val, u_int32_t asel, u_int32_t *match)
{
	dsrecord *r, *k;
	dsattribute *a, *a_index_key;
	u_int32_t i, j, index_this;
	dsdata *d_name, *d_index_key;
	dsindex_key_t *kx;
	dsindex_val_t *vx;

	*match = IndexNull;

	if (s == NULL) return DSStatusInvalidStore;

	if (s->flags & DSSTORE_FLAGS_REMOTE_NETINFO)
		return nistore_match(s, dsid, key, val, asel, match);

	dsstore_lock(s, 1);
	dsstore_sync(s);

	r = dsstore_fetch_internal(s, dsid, 0);
	if (r == NULL)
	{
		dsstore_unlock(s);
		return DSStatusInvalidPath;
	}

	if (r->sub_count == 0)
	{
		dsrecord_release(r);
		dsstore_unlock(s);
		return DSStatusOK;
	}

	/*
	 * If the record has an index for this key, use it to find a match.
	 */
	kx = dsindex_lookup_key(r->index, key);
	if (kx != NULL)
	{
		vx = dsindex_lookup_val(kx, val);
		if (vx != NULL)
		{
			if (vx->dsid_count > 0) *match = vx->dsid[0];
		}

		dsrecord_release(r);
		dsstore_unlock(s);
		return DSStatusOK;
	}

	/*
	 * Not indexed, so we have to read the child records to look for a match.
	 * While reading the child records we create an index for the "name" key,
	 * and any key listed as a value of the record's "index_key" meta-attribute.
	 */
	d_name = cstring_to_dsdata(NAME_NAME);
	d_index_key = cstring_to_dsdata(NAME_INDEX_KEY);
	a_index_key = dsrecord_attribute(r, d_index_key, SELECT_META_ATTRIBUTE);
	dsdata_release(d_index_key);

	if (r->index == NULL) r->index = dsindex_new();
	dsindex_insert_key(r->index, d_name);
	if (a_index_key != NULL)
	{
		for (i = 0; i < a_index_key->count; i++)
			dsindex_insert_key(r->index, a_index_key->value[i]);
	}

	for (i = 0; i < r->sub_count; i++)
	{
		k = dsstore_fetch_internal(s, r->sub[i], 0);
		if (k == NULL)
		{
			dsattribute_release(a_index_key);
			dsdata_release(d_name);
			dsrecord_release(r);
			dsstore_unlock(s);
			return DSStatusInvalidPath;
		}

		for (j = 0; j < k->count; j++)
		{
			index_this = 0;
			a = k->attribute[j];
			if (dsdata_equal(d_name, a->key)) index_this = 1;
			else if (dsattribute_index(a_index_key, a->key) != IndexNull) index_this = 1;

			if (index_this == 1)
				dsindex_insert_attribute(r->index, a, r->sub[i]);

			if (*match == IndexNull)
			{
				if (dsdata_equal(key, a->key))
				{
					if (dsattribute_index(a, val) != IndexNull)
						*match = r->sub[i];
				}
			}
		}

		dsrecord_release(k);
	}

	dsattribute_release(a_index_key);
	dsdata_release(d_name);
	if (s->cache_enabled == 1) dscache_save(s->cache, r);
	dsrecord_release(r);
	dsstore_unlock(s);

	return DSStatusOK;
}

u_int32_t
dsstore_record_version(dsstore *s, u_int32_t dsid)
{
	dsstatus status;
	u_int32_t vers, size, where;

	if (s == NULL) return IndexNull;

	if (s->flags & DSSTORE_FLAGS_REMOTE_NETINFO)
		return nistore_record_version(s, dsid);

	dsstore_lock(s, 1);
	dsstore_sync(s);
	
	status = dsstore_index(s, dsid, &vers, &size, &where);

	dsstore_unlock(s);

	if (status != DSStatusOK) return IndexNull;
	return vers;
}

u_int32_t
dsstore_record_serial(dsstore *s, u_int32_t dsid)
{
	dsstatus status;
	u_int32_t serial;

	if (s == NULL) return IndexNull;
	
	if (s->flags & DSSTORE_FLAGS_REMOTE_NETINFO)
		return nistore_record_serial(s, dsid);

	status = dsstore_vital_statistics(s, dsid, NULL, &serial, NULL);
	if (status != DSStatusOK) return IndexNull;
	return serial;
}

u_int32_t
dsstore_record_super(dsstore *s, u_int32_t dsid)
{
	dsstatus status;
	u_int32_t super;

	if (s == NULL) return IndexNull;

	if (s->flags & DSSTORE_FLAGS_REMOTE_NETINFO)
		return nistore_record_super(s, dsid);
	
	status = dsstore_vital_statistics(s, dsid, NULL, NULL, &super);
	if (status != DSStatusOK) return IndexNull;
	return super;
}

u_int32_t
dsstore_nichecksum(dsstore *s)
{
	if (s == NULL) return 0;

	dsstore_lock(s, 1);
	dsstore_sync(s);
	dsstore_unlock(s);

	return s->nichecksum;
}

void
dsstore_flush_cache(dsstore *s)
{
	if (s == NULL) return;
	if (s->cache_enabled == 1) dscache_flush(s->cache);
}

/* Resets all version numbers to zero. */
void
dsstore_reset(dsstore *s)
{
	store_index_entry_t *e;
	dsrecord *r;
	int i, ce;

	if (s == NULL) return;
	if (!dsstore_access_readwrite(s)) return;

	dsstore_lock(s, 1);
	dsstore_sync(s);

	ce = s->cache_enabled;
	s->cache_enabled = 0;
	if (ce == 1) dscache_flush(s->cache);

	for (i = 0; i < s->index_count; i++)
	{
		e = (store_index_entry_t *)s->index[i];
		e->vers = 0;
		r = dsstore_fetch_internal(s, e->dsid, 0);
		if (r == NULL) continue;
		s->max_vers = IndexNull;
		dsstore_save_internal(s, r, 0);
	}

	s->cache_enabled = ce;
	dsstore_unlock(s);
}

dsrecord *
dsstore_statistics(dsstore *s)
{
	dsrecord *r;
	dsattribute *a;
	dsdata *d;
	char str[64];

	if (s == NULL) return NULL;

	if (s->flags & DSSTORE_FLAGS_REMOTE_NETINFO)
		return nistore_statistics(s);

	dsstore_lock(s, 1);
	dsstore_sync(s);

	r = dsrecord_new();

	d = cstring_to_dsdata("checksum");
	a = dsattribute_new(d);
	dsdata_release(d);
	sprintf(str, "%u", s->nichecksum);
	d = cstring_to_dsdata(str);
	dsattribute_append(a, d);
	dsdata_release(d);
	dsrecord_append_attribute(r, a, 0);
	dsattribute_release(a);
	
	d = cstring_to_dsdata("version");
	a = dsattribute_new(d);
	dsdata_release(d);
	sprintf(str, "%u", s->max_vers);
	d = cstring_to_dsdata(str);
	dsattribute_append(a, d);
	dsdata_release(d);
	dsrecord_append_attribute(r, a, 0);
	dsattribute_release(a);
	
	d = cstring_to_dsdata("max_dsid");
	a = dsattribute_new(d);
	dsdata_release(d);
	sprintf(str, "%u", dsstore_max_id_internal(s, 0));
	d = cstring_to_dsdata(str);
	dsattribute_append(a, d);
	dsdata_release(d);
	dsrecord_append_attribute(r, a, 0);
	dsattribute_release(a);
	
	d = cstring_to_dsdata("fetch_count");
	a = dsattribute_new(d);
	dsdata_release(d);
	sprintf(str, "%u", s->fetch_count);
	d = cstring_to_dsdata(str);
	dsattribute_append(a, d);
	dsdata_release(d);
	dsrecord_append_attribute(r, a, 0);
	dsattribute_release(a);
	
	d = cstring_to_dsdata("save_count");
	a = dsattribute_new(d);
	dsdata_release(d);
	sprintf(str, "%u", s->save_count);
	d = cstring_to_dsdata(str);
	dsattribute_append(a, d);
	dsdata_release(d);
	dsrecord_append_attribute(r, a, 0);
	dsattribute_release(a);
	
	d = cstring_to_dsdata("remove_count");
	a = dsattribute_new(d);
	dsdata_release(d);
	sprintf(str, "%u", s->remove_count);
	d = cstring_to_dsdata(str);
	dsattribute_append(a, d);
	dsdata_release(d);
	dsrecord_append_attribute(r, a, 0);
	dsattribute_release(a);

	dsstore_unlock(s);

	return r;
}

void
dsstore_set_sync_delegate(dsstore *s, void (*delegate)(void *), void *private)
{
	if (s == NULL) return;
	s->sync_delegate = delegate;
	s->sync_private = private;
}
