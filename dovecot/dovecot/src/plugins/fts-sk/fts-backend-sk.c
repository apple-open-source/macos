/* Copyright (c) 2006-2010 Dovecot authors, see the included COPYING file */
/*
 * Copyright (c) 2010-2011 Apple Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without  
 * modification, are permitted provided that the following conditions  
 * are met:
 * 
 * 1.  Redistributions of source code must retain the above copyright  
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above  
 * copyright notice, this list of conditions and the following  
 * disclaimer in the documentation and/or other materials provided  
 * with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its  
 * contributors may be used to endorse or promote products derived  
 * from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND  
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,  
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A  
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS  
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,  
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT  
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF  
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND  
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,  
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT  
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF  
 * SUCH DAMAGE.
 */

/* SearchKit indexing and searching of not only headers and text but also
   non-text attachments.

   The message uid and header/body indicator is in the name of each document so
   it is quick to map from SK search results to messages, and to filter by
   body-vs.-header.  But expunge is a challenge.  The names of the documents in
   the SK index are not deterministic both because of the random mkstemp()
   suffix and from possible changes to spill_dir over time.  Using
   SKIndexRenameDocument() and SKIndexMoveDocument() to unify the document
   namespace was too slow because both functions flush the index nearly every
   time they're called.  Iterating through all the documents was a non-starter.
   So instead we index one "shadow document" per message, the text of which is
   just "FTS<uid>SK", and which has properties attached that identify every
   regular document associated with that uid.  With this scheme, expunge can
   search for FTS<uid>SK to get the shadow document, and through that the
   regular documents.  All this just to make expunge quick, or at least no
   slower than it needs to be.  Since flushing the index can be very slow
   and the FTS API provides no way to make expunge asynchronous, defer
   expunges until the external updater runs (see fts_enqueue_update()) and
   executes a special command X-FTS-COMPACT.

   SearchKit does not expose the fd it uses, so locking uses a separate file.
   Conveniently, this allows all locking to happen at the border (the
   fts_backend_sk_* functions) which helps avoid races since SearchKit does not
   offer an API for "open if present otherwise create."

   This plugin can index small plain text documents internally, but as soon as
   it encounters large text or non-text it fires up an external indexer.  This
   separates any buggy or malicious content indexers (aka Spotlight importers,
   or text extractors) into a separate process with limited privileges.  It
   also allows us to time out if indexing some document takes too long.

   Indexing is pretty slow (not a surprise since it involves writing temp
   files, IPC with the indexer, and updating search indexes).  So whenever
   possible use asynchronous IPC with it:  send it a document to index and
   unwind all the way back out to the main command loop so some other client
   can use this process (when client_limit > 1).  Unfortunately implementing
   this is pretty ugly since it appears no other command suspends input from
   the client like this.  So we use a really bad "reacharound" to suspend and
   resume client input.

   SearchKit indexes are limited to 32 bits so use multiple indexes
   ("fragments") when necessary.  Only the 0th index contains the meta
   properties.  Locking covers all the indexes together. */

#include "lib.h"
#include "str.h"
#include "array.h"
#include "ioloop.h"
#include "istream.h"
#include "ostream.h"
#include "unichar.h"
#include "strescape.h"
#include "imap-client.h"
#include "imap-commands.h"
#include "mail-namespace.h"
#include "mail-storage-private.h"
#include "mail-search-build.h"
#include "nfs-workarounds.h"
#include "safe-mkstemp.h"
#include "mkdir-parents.h"
#include "file-lock.h"
#include "file-dotlock.h"
#include "write-full.h"
#include "seq-range-array.h"
#include "fd-set-nonblock.h"
#include "randgen.h"
#include "hex-binary.h"
#include "imap-search.h"
#include "fts-api.h"
#include "fts-storage.h"
#include "fts-sk-plugin.h"

#include <stdlib.h>
#include <sysexits.h>
#include <dlfcn.h>

/* config.h defines DEBUG to an empty comment which makes CoreServices barf */
#ifdef DEBUG
# undef DEBUG
# define DEBUG 1
#endif

#define dec2str Carbon_conflicting_dec2str
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
	/* <rdar://problem/7819581> */
#include <CoreFoundation/CFPreferences_Private.h>
#undef dec2str

#define __APPLE_API_PRIVATE
#include <sandbox.h>
#undef __APPLE_API_PRIVATE

#define	SK_FILE_NAME			"dovecot.skindex"
#define	SK_LOCK_FILE_NAME		"dovecot.sklock"

#define	SK_LOCK_TIMEOUT_SECS		20
#define	SK_DOTLOCK_STALE_TIMEOUT_SECS	(15 * 60)

#define	SK_DEFAULT_MIN_TERM_LENGTH	1
#define	SK_SEARCH_SECS			5
#define	SK_DEFAULT_SPILL_DIR		"/tmp"
#define	SK_COMPACT_AGE			(2 * 24 * 60 * 60)
#define	SK_COMPACT_EXPUNGES		100
#define	SK_COMPACT_BATCH_SIZE		8

/* Until SearchKit supports more than 32 bits limit the size of each index.
   The exact size isn't important.  What's important is that it, in
   conjunction with lazy flushing, will never allow an index to grow "too big"
   in SK's eyes.  */
#define	SK_FRAGMENT_THRESHOLD_SIZE	((off_t) 1024 * 1024 * 1024)

#define	SK_TEXT_BUFFER			(128 * 1024)

#define	SK_INDEX_NAME			"fts-sk"
#define	SK_SCHEME			"fts-sk"
#define	SK_META_URL			SK_SCHEME"://meta"
#define	SK_META_VERSION			1
#define	SK_VERSION_NAME			"version"
#define	SK_LAST_UID_NAME		"last_uid"
#define	SK_UIDV_NAME			"uidvalidity"
#define	SK_FRAGS_NAME			"fragments"
#define	SK_DOCUMENTS_NAME		"documents"
#define	SK_SHADOW_NAME			"shadow"
#define	SK_EXPUNGED_UIDS_NAME		"expunged_uids"
#define	SK_DEFERRED_SINCE_NAME		"deferred_since"

#define	SK_INDEXER			"skindexer"
#define	SK_INDEXER_EXECUTABLE		PKG_LIBEXECDIR"/"SK_INDEXER
#define	SK_INDEXER_TIMEOUT_SECS		(2 * 60)

#define	SK_TERM_CHARS			"@.-+#$%_&'"	/* arbitrary */

#define	CFRelease_and_null(x)		STMT_START { \
						CFRelease(x); \
						(x) = NULL; \
					} STMT_END

enum sk_where {
	SK_BODY,
	SK_HEADERS,
	SK_SHADOW,
	SK_WHERE_LAST
};
static const char *sk_wheres[SK_WHERE_LAST] = {
	"body",
	"hdr",
	"shadow"
};
static size_t sk_where_lengths[SK_WHERE_LAST];

struct sk_fts_index {
	char *path;
	SKIndexRef skiref;
	SKDocumentRef meta_doc;
};

struct sk_fts_backend {
	struct fts_backend backend;

	char *index_path;
	char *spill_dir;
	char *lock_path;
	enum file_lock_method lock_method;
	int lock_fd;
	struct dotlock_settings dotlock_set;
	struct file_lock *file_lock;
	struct dotlock *dotlock;
	int lock_type;

	uint32_t uidvalidity;
	mode_t create_mode;
	gid_t create_gid;
	CFMutableArrayRef pendingExpunges;

	unsigned int default_min_term_length;
	unsigned int lock_timeout_secs;
	unsigned int search_secs;
	int indexer_timeout_secs;
	long compact_age;
	int compact_expunges;

	ARRAY_DEFINE(indexes, struct sk_fts_index);

	unsigned int debug:1;
};

struct sk_meta_values {
	uint32_t last_uid;
	CFArrayRef expungedUids;
	time_t deferred_since;
};

struct sk_fts_backend_build_context {
	struct fts_backend_build_context ctx;

	unsigned int fragment;

	uint32_t next_uid, cur_uid, cur_seq;
	enum sk_where next_where, cur_where;
	buffer_t *cur_text;
	char *next_content_type, *cur_content_type;
	CFMutableArrayRef cur_shadow_ids;
	struct sk_meta_values *values;

	string_t *spill_path;
	int spill_fd;

	pid_t indexer_pid;
	struct ostream *to_indexer;
	struct istream *from_indexer;
	int from_indexer_fd;
	char *indexer_tag;
	struct ioloop *indexer_ioloop;
	struct io *indexer_io;
	struct timeout *indexer_timeout;
	const char *indexer_reply;

	unsigned int use_indexer_forever:1;
	unsigned int indexer_shook:1;
	unsigned int indexer_synchronous:1;
	unsigned int indexer_shadowing:1;
	unsigned int indexer_timed_out:1;
};

struct sk_fts_search_context {
	const struct sk_fts_index *skindex;
	SKSearchRef search;

	unsigned int done:1;
};

struct sk_fts_expunge_context {
	const struct sk_fts_index *skindex;
	SKSearchRef search;
	CFMutableArrayRef targets;

	unsigned int done:1;
};

static bool sk_plugin_initialized = FALSE;

/* the reacharound needs the symbols cmd_search_more and
   cmd_search_more_callback to determine whether to run asynchronously but
   they are not available in the LDA so use dlsym to look them up */
static bool (*sk_cmd_search_more)(struct client_command_context *);
static void (*sk_cmd_search_more_callback)(struct client_command_context *);

int sk_assert_build_cancel = 1;		/* see stats-plugin.c */

/* note that enabling mail_debug makes sandboxd use lots of cpu and
   slows down skindexer <rdar://problem/8200016> */
static const char sk_sandbox_profile[] = "\
(version 1)\n\
\n\
; set mail_debug to log to system log\n\
(deny default%s)\n\
(debug %s)\n\
\n\
(allow file-ioctl\n\
       (literal \"/dev/dtracehelper\"))\n\
\n\
(allow file-read*\n\
       (literal \"/\")\n\
       (regex #\"/(Library|Applications)($|/)\")	; explicitly not ^\n\
       (literal \"/private\")\n\
       (regex #\"^/(private/)?(tmp|var)($|/)\")\n\
       (literal \"/dev/autofs_nowait\")\n\
       (literal \"/dev/dtracehelper\")\n\
       (literal \"/dev/null\")\n\
       (regex #\"^/dev/u?random$\")\n\
       (subpath \"/System\")\n\
       (regex #\"^/usr/(lib|share)($|/)\")\n\
       (literal \""PKG_LIBEXECDIR"\")\n\
       (subpath \""SK_INDEXER_EXECUTABLE"\")	; subpath for resource fork\n\
       %s)	; spill dir\n\
\n\
(allow file-read-metadata\n\
       (regex #\"^/(private/)?etc(/localtime)?$)\")\n\
       (literal \"/usr\"))\n\
\n\
(allow file-write-data\n\
       (literal \"/dev/dtracehelper\")\n\
       (literal \"/dev/null\")\n\
       (regex #\"^/(private/)?(var/)?tmp($|/)\"))\n\
\n\
(allow ipc-posix-shm\n\
       (ipc-posix-name-regex #\"^/tmp/com\\.apple\\.csseed\\.\")\n\
       (ipc-posix-name \"apple.shm.notification_center\"))\n\
\n\
(allow mach-lookup\n\
       (global-name \"com.apple.CoreServices.coreservicesd\")\n\
       (global-name-regex #\"^com\\.apple\\.distributed_notifications\\.\")\n\
       (global-name-regex #\"^com\\.apple\\.system\\.DirectoryService\\.\")\n\
       (global-name \"com.apple.system.logger\")\n\
       (global-name \"com.apple.system.notification_center\"))\n\
\n\
(allow process-exec\n\
       (literal \""SK_INDEXER_EXECUTABLE"\"))\n\
\n\
(allow signal (target self))\n\
\n\
(allow sysctl-read)\n\
\n\
(allow file-read* file-write-data file-write-mode\n\
       %s)	; skindex path\n\
";

static void sk_plugin_init(void)
{
	unsigned int i;

	if (sk_plugin_initialized)
		return;

	/* <rdar://problem/7819581> */
	_CFPreferencesAlwaysUseVolatileUserDomains();

	for (i = 0; i < SK_WHERE_LAST; i++)
		sk_where_lengths[i] = strlen(sk_wheres[i]);

	/* this reacharound just gets uglier and uglier */
	sk_cmd_search_more = dlsym(RTLD_MAIN_ONLY, "cmd_search_more");
	sk_cmd_search_more_callback = dlsym(RTLD_MAIN_ONLY,
					    "cmd_search_more_callback");
	if (sk_cmd_search_more != NULL && sk_cmd_search_more_callback == NULL)
		i_fatal("fts_sk: found cmd_search_more but not cmd_search_more_callback");

	sk_plugin_initialized = TRUE;
}

static void sk_nonnull(const void *ptr, const char *tag)
{
	if (ptr == NULL)
		i_fatal_status(FATAL_OUTOFMEM,
			       "fts_sk: %s failed: Out of memory", tag);
}

static void sk_set_options(struct sk_fts_backend *backend, const char *str)
{
	const char *const *tmp;

	for (tmp = t_strsplit_spaces(str, " "); *tmp != NULL; tmp++) {
		if (strncasecmp(*tmp, "compact_age_days=", 17) == 0) {
			const char *p = *tmp + 17;
			long val = atoi(p) * 24 * 60 * 60;
			if (val <= 0)
				i_fatal("fts_sk: Invalid compact_age_days: %s",
					p);
			backend->compact_age = val;
		} else if (strncasecmp(*tmp, "compact_expunges=", 17) == 0) {
			const char *p = *tmp + 17;
			int val = atoi(p);
			if (val <= 0)
				i_fatal("fts_sk: Invalid compact_expunges: %s",
					p);
			backend->compact_expunges = val;
		} else if (strncasecmp(*tmp, "lock_timeout_secs=", 18) == 0) {
			const char *p = *tmp + 18;
			int val = atoi(p);
			if (val < 0)
				i_fatal("fts_sk: Invalid lock_timeout_secs: %s",
					p);
			backend->lock_timeout_secs = val;
		} else if (strncasecmp(*tmp, "min_term_length=", 16) == 0) {
			const char *p = *tmp + 16;
			int val = atoi(p);
			if (val <= 0)
				i_fatal("fts_sk: Invalid min_term_length: %s",
					p);
			backend->default_min_term_length = val;
		} else if (strncasecmp(*tmp, "indexer_timeout_secs=", 21) == 0) {
			const char *p = *tmp + 21;
			int val = atoi(p);
			if (val <= 0)
				i_fatal("fts_sk: Invalid indexer_timeout_secs: %s",
					p);
			backend->indexer_timeout_secs = val;
		} else if (strncasecmp(*tmp, "search_secs=", 12) == 0) {
			const char *p = *tmp + 12;
			int val = atoi(p);
			if (val < 0)
				i_fatal("fts_sk: Invalid search_secs: %s", p);
			backend->search_secs = val;
		} else if (strncasecmp(*tmp, "spill_dir=", 10) == 0) {
			const char *p = *tmp + 10;
			if (strcasecmp(p, "index") == 0) {
				i_free(backend->spill_dir);
				backend->spill_dir =
					i_strdup(backend->index_path);
			} else if (*p == '/') {
				i_free(backend->spill_dir);
				backend->spill_dir = i_strdup(p);
			} else
				i_fatal("fts_sk: Invalid spill_dir: %s "
					"(need \"index\" or an absolute path)",
					p);
		} else
			i_fatal("fts_sk: Invalid setting: %s", *tmp);
	}
}

static CFDictionaryRef sk_index_properties(struct sk_fts_backend *backend)
{
	CFMutableDictionaryRef properties;
	CFNumberRef minTermLength;
	int max_terms;
	CFNumberRef maxTerms;

	properties =
		CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
	sk_nonnull(properties, "CFDictionaryCreateMutable(index properties)");

	minTermLength = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type,
				       &backend->default_min_term_length);
	sk_nonnull(minTermLength, "CFNumberCreate(min_term_length)");
	CFDictionaryAddValue(properties, kSKMinTermLength, minTermLength);
	CFRelease_and_null(minTermLength);

	max_terms = 0;
	maxTerms = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type,
				  &max_terms);
	sk_nonnull(maxTerms, "CFNumberCreate(max_terms)");
	CFDictionaryAddValue(properties, kSKMaximumTerms, maxTerms);
	CFRelease_and_null(maxTerms);

	CFDictionaryAddValue(properties, kSKProximityIndexing, kCFBooleanTrue);
	CFDictionaryAddValue(properties, kSKTermChars, CFSTR(SK_TERM_CHARS));

	return properties;
}

static CFDictionaryRef sk_meta_properties(struct sk_fts_backend *backend,
					  const struct sk_meta_values *values)
{
	CFMutableDictionaryRef metaprops;
	unsigned int version, fragments;
	CFNumberRef versionNum, lastUid, uidValidity, fragmentCount,
		deferredSince;

	metaprops =
		CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
	sk_nonnull(metaprops, "CFDictionaryCreateMutable(metaprops)");

	version = SK_META_VERSION;
	versionNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type,
				    &version);
	sk_nonnull(versionNum, "CFNumberCreate(version)");
	CFDictionaryAddValue(metaprops, CFSTR(SK_VERSION_NAME), versionNum);
	CFRelease_and_null(versionNum);

	uidValidity = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type,
				     &backend->uidvalidity);
	sk_nonnull(uidValidity, "CFNumberCreate(uidvalidity)");
	CFDictionaryAddValue(metaprops, CFSTR(SK_UIDV_NAME), uidValidity);
	CFRelease_and_null(uidValidity);

	fragments = array_count(&backend->indexes);
	fragmentCount = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type,
				       &fragments);
	sk_nonnull(fragmentCount, "CFNumberCreate(fragments)");
	CFDictionaryAddValue(metaprops, CFSTR(SK_FRAGS_NAME), fragmentCount);
	CFRelease_and_null(fragmentCount);

	lastUid = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type,
				 &values->last_uid);
	sk_nonnull(lastUid, "CFNumberCreate(last_uid)");
	CFDictionaryAddValue(metaprops, CFSTR(SK_LAST_UID_NAME), lastUid);
	CFRelease_and_null(lastUid);

	if (values->expungedUids != NULL)
		CFDictionaryAddValue(metaprops, CFSTR(SK_EXPUNGED_UIDS_NAME),
				     values->expungedUids);
	else {
		/* SearchKit seems to keep removed metaprops dict entries,
		   so save an empty array */
		CFArrayRef empty = CFArrayCreate(kCFAllocatorDefault, NULL, 0,
						 &kCFTypeArrayCallBacks);
		CFDictionaryAddValue(metaprops, CFSTR(SK_EXPUNGED_UIDS_NAME),
				     empty);
		CFRelease_and_null(empty);
	}

	i_assert(sizeof values->deferred_since == sizeof (long));
	deferredSince = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType,
				       &values->deferred_since);
	sk_nonnull(deferredSince, "CFNumberCreate(deferred_since)");
	CFDictionaryAddValue(metaprops, CFSTR(SK_DEFERRED_SINCE_NAME),
			     deferredSince);
	CFRelease_and_null(deferredSince);

	return metaprops;
}

static CFDictionaryRef sk_get_metaprops(struct sk_fts_backend *backend)
{
	const struct sk_fts_index *skindex = array_idx(&backend->indexes, 0);

	CFDictionaryRef metaprops =
		SKIndexCopyDocumentProperties(skindex->skiref,
					      skindex->meta_doc);
	if (metaprops == NULL)
		i_error("fts_sk: meta properties missing from %s",
			skindex->path);
	return metaprops;
}

static struct sk_meta_values *sk_meta_values_init(void)
{
	struct sk_meta_values *values;

	values = i_new(struct sk_meta_values, 1);
	values->last_uid = (uint32_t) -1;
	values->expungedUids = NULL;
	values->deferred_since = 0;

	return values;
}

static void sk_meta_values_deinit(struct sk_meta_values **_values)
{
	struct sk_meta_values *values = *_values;

	*_values = NULL;
	if (values->expungedUids != NULL)
		CFRelease_and_null(values->expungedUids);
	i_free(values);
}

static int sk_set_meta_values(struct sk_fts_backend *backend,
			      const struct sk_meta_values *values)
{
	const struct sk_fts_index *skindex = array_idx(&backend->indexes, 0);

	if (values->last_uid == 0) {
		/* need to do this only once */
		if (!SKIndexAddDocumentWithText(skindex->skiref,
						skindex->meta_doc,
						NULL, TRUE)) {
			i_error("fts_sk: initializing meta props in %s failed",
				skindex->path);
			return -1;
		}
	}

	/* meta_doc must be indexed before setting its properties */
	CFDictionaryRef metaprops = sk_meta_properties(backend, values);
	SKIndexSetDocumentProperties(skindex->skiref, skindex->meta_doc,
				     metaprops);
	CFRelease_and_null(metaprops);
	return 1;
}

static int sk_get_meta_values(struct sk_fts_backend *backend,
			      struct sk_meta_values **values_r)
{
	CFDictionaryRef metaprops;
	struct sk_meta_values *values;
	CFNumberRef lastUid, deferredSince;
	int ret = 0;

	metaprops = sk_get_metaprops(backend);
	if (metaprops == NULL)
		return -1;

	values = sk_meta_values_init();

	lastUid = (CFNumberRef) CFDictionaryGetValue(metaprops,
				CFSTR(SK_LAST_UID_NAME));
	if (lastUid == NULL ||
	    !CFNumberGetValue(lastUid, kCFNumberSInt32Type, &values->last_uid))
		ret = -1;

	values->expungedUids = (CFArrayRef) CFDictionaryGetValue(metaprops,
					    CFSTR(SK_EXPUNGED_UIDS_NAME));
	if (values->expungedUids != NULL)
		CFRetain(values->expungedUids);

	deferredSince = (CFNumberRef) CFDictionaryGetValue(metaprops,
				      CFSTR(SK_DEFERRED_SINCE_NAME));
	i_assert(sizeof values->deferred_since == sizeof (long));
	if (deferredSince == NULL ||
	    !CFNumberGetValue(deferredSince, kCFNumberLongType,
			      &values->deferred_since))
		ret = -1;

	CFRelease_and_null(metaprops);

	if (ret < 0)
		sk_meta_values_deinit(&values);
	else
		*values_r = values;
	return ret;
}

static int sk_delete(struct sk_fts_backend *backend)
{
	struct sk_fts_index *skindex;
	unsigned int count, i;
	int ret = 0;

	skindex = array_get_modifiable(&backend->indexes, &count);
	for (i = 0; i < count; i++) {
		if (unlink(skindex[i].path) == 0) {
			i_info("fts_sk: deleted %s", skindex[i].path);
			if (ret == 0)
				ret = 1;
		} else if (errno != ENOENT) {
			i_error("fts_sk: unlink(%s) failed: %m",
				skindex[i].path);
			ret = -1;
		}
		i_assert(skindex[i].skiref == NULL);
		if (i > 0) {
			i_free(skindex[i].path);
			i_assert(skindex[i].meta_doc == NULL);
		}
	}
	array_delete(&backend->indexes, 1, count - 1);

	return ret;
}

static void sk_close(struct sk_fts_backend *backend)
{
	struct sk_fts_index *skindex;

	array_foreach_modifiable(&backend->indexes, skindex) {
		if (skindex->skiref != NULL) {
			SKIndexClose(skindex->skiref);
			skindex->skiref = NULL;
		}
	}
}

static int sk_flush(struct sk_fts_backend *backend)
{
	const struct sk_fts_index *skindex;
	int ret = 0;

	array_foreach(&backend->indexes, skindex) {
		if (!SKIndexFlush(skindex->skiref)) {
			i_error("fts_sk: SKIndexFlush(%s) failed",
				skindex->path);
			ret = -1;
		}
	}

	return ret;
}

static int sk_open(struct sk_fts_backend *backend, bool writable,
		   bool meta_only)
{
	struct sk_fts_index *skindex;
	size_t path_len;
	CFURLRef url;
	CFDictionaryRef metaprops;
	int version = -1;
	uint32_t uidvalidity = (uint32_t) -1;
	int error_ret = -1;
	unsigned int fragments = 0, i;
	string_t *fragment_path;

	skindex = array_idx_modifiable(&backend->indexes, 0);
	if (skindex->skiref != NULL)
		return 1;

	path_len = strlen(skindex->path);
	url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
		(const UInt8 *) skindex->path, path_len, FALSE);
	sk_nonnull(url, "CFURLCreateFromFileSystemRepresentation(path)");

	/* open the first index */
	skindex->skiref = SKIndexOpenWithURL(url, CFSTR(SK_INDEX_NAME),
					     writable);
	CFRelease_and_null(url);
	if (skindex->skiref == NULL)
		return -1;

	metaprops = sk_get_metaprops(backend);
	if (metaprops != NULL) {
		CFNumberRef versionNum, uidValidity, fragmentCount;

		versionNum = (CFNumberRef) CFDictionaryGetValue(metaprops,
			CFSTR(SK_VERSION_NAME));
		if (versionNum == NULL ||
		    !CFNumberGetValue(versionNum, kCFNumberSInt32Type,
				      &version))
			version = -1;

		uidValidity = (CFNumberRef) CFDictionaryGetValue(metaprops,
			CFSTR(SK_UIDV_NAME));
		if (uidValidity == NULL ||
		    !CFNumberGetValue(uidValidity, kCFNumberSInt32Type,
				      &uidvalidity))
			uidvalidity = (uint32_t) -1;

		fragmentCount = (CFNumberRef) CFDictionaryGetValue(metaprops,
			CFSTR(SK_FRAGS_NAME));
		if (fragmentCount == NULL ||
		    !CFNumberGetValue(fragmentCount, kCFNumberSInt32Type,
				      &fragments))
			fragments = 0;

		CFRelease_and_null(metaprops);
	}

	/* prepare index fragment paths in case we need to call sk_delete() */
	fragment_path = t_str_new(path_len + 5);
	str_append(fragment_path, skindex->path);
	for (i = 1; fragments == 0 || i < fragments; i++) {
		str_truncate(fragment_path, path_len);
		str_printfa(fragment_path, "-%u", i);

		if (fragments == 0) {
			struct stat stbuf;

			if (stat(str_c(fragment_path), &stbuf) < 0) {
				if (errno != ENOENT)
					i_error("fts_sk: stat(%s): %m",
						str_c(fragment_path));
				break;
			}
		}

		if (i < array_count(&backend->indexes))
			i_assert((array_idx(&backend->indexes, i))->path != 
				 NULL);
		else {
			skindex = array_append_space(&backend->indexes);
			skindex->path = i_strdup(str_c(fragment_path));
		}
	}

	/* the array may have been reallocated */
	skindex = array_get_modifiable(&backend->indexes, &fragments);

	/* sanity check */
	if (version != SK_META_VERSION || uidvalidity != backend->uidvalidity) {
		if (version > SK_META_VERSION) {
			i_warning("fts_sk: %s is version %d which is "
				  "newer than version %d; ignoring",
				  skindex->path, version, SK_META_VERSION);
			error_ret = 0;
			sk_close(backend);
		} else {
			/* FIXME when appropriate: upgrade old index to new
			   version */
			sk_close(backend);
			sk_delete(backend);
		}
	}
	/* the array may have been reallocated */
	skindex = array_get_modifiable(&backend->indexes, &fragments);
	i_assert(fragments > 0);
	if (skindex->skiref == NULL)
		return error_ret;

	if (meta_only)
		return 1;

	/* open the other fragments */
	for (i = 1; i < fragments; i++) {
		url = CFURLCreateFromFileSystemRepresentation(
			kCFAllocatorDefault, (const UInt8 *) skindex[i].path,
			strlen(skindex[i].path), FALSE);
		sk_nonnull(url, "CFURLCreateFromFileSystemRepresentation(fragment_path)");
		skindex[i].skiref = SKIndexOpenWithURL(url,
						       CFSTR(SK_INDEX_NAME),
						       writable);
		CFRelease_and_null(url);

		if (skindex[i].skiref == NULL) {
			i_error("fts_sk: failed to open index fragment %s",
				skindex[i].path);
			break;
		}
	}
	if (i < fragments) {
		sk_close(backend);
		return -1;
	}

	return 1;
}

static int sk_create(struct sk_fts_backend *backend, unsigned int fragno)
{
	struct sk_fts_index *skindex;
	CFURLRef url;

	skindex = array_idx_modifiable(&backend->indexes, fragno);
	i_assert(skindex->skiref == NULL);

	url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
		(const UInt8 *) skindex->path, strlen(skindex->path), FALSE);
	sk_nonnull(url, "CFURLCreateFromFileSystemRepresentation(path)");

	/* Don't adjust umask since we can't set the mode til after. */
	CFDictionaryRef properties = sk_index_properties(backend);
	skindex->skiref = SKIndexCreateWithURL(url,
					       CFSTR(SK_INDEX_NAME),
					       kSKIndexInverted,
					       properties);
	CFRelease_and_null(properties);
	CFRelease_and_null(url);
	if (skindex->skiref != NULL) {
		struct sk_meta_values *values;

		if (chmod(skindex->path, backend->create_mode) < 0)
			i_warning("fts_sk: chmod(%s, 0%o) failed: %m",
			       skindex->path, backend->create_mode);
		if (backend->create_gid != (gid_t) -1 &&
		    chown(skindex->path, (uid_t) -1,
			  backend->create_gid) < 0)
			i_warning("fts_sk: chown(%s, -1, %d) failed: %m",
			       skindex->path, backend->create_gid);

		values = sk_meta_values_init();
		values->last_uid = 0;
		if ((fragno == 0 && sk_set_meta_values(backend, values) < 0) ||
		    sk_flush(backend) < 0) {
			sk_close(backend);
			sk_delete(backend);
		}
		sk_meta_values_deinit(&values);
	}

	return skindex->skiref == NULL ? -1 : 1;
}

static int sk_open_or_create(struct sk_fts_backend *backend, bool meta_only)
{
	int ret;

	ret = sk_open(backend, TRUE, meta_only);
	if (ret < 0)		/* maybe it doesn't exist; try creating it */
		ret = sk_create(backend, 0);
	if (ret <= 0) {
		/* self-heal when index is damaged */
		if (sk_delete(backend) > 0)
			ret = sk_create(backend, 0);
	}
	if (ret <= 0)
		i_error("fts_sk: Could not open or create %s",
			(array_idx(&backend->indexes, 0))->path);

	return ret;
}

static void sk_doc_mkname(string_t *name, uint32_t uid, enum sk_where where,
			  uint32_t seq)
{
	/* putting the uid and the where in the url is crucial */
	str_printfa(name, "uid=%u&where=%s&seq=%u", uid, sk_wheres[where], seq);
}

static SKDocumentRef sk_create_document_immediate(uint32_t uid,
						  enum sk_where where,
						  uint32_t seq)
{
	string_t *url;
	CFURLRef docurl;
	SKDocumentRef doc;

	url = t_str_new(64);
	str_append(url, "/"SK_DOCUMENTS_NAME"/");
	sk_doc_mkname(url, uid, where, seq);

	docurl = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
		str_data(url), str_len(url), FALSE);
	if (docurl == NULL) {
		i_error("fts_sk: CFURLCreateWithString(%s) failed", str_c(url));
		return NULL;
	}

	doc = SKDocumentCreateWithURL(docurl);
	if (doc == NULL)
		i_error("fts_sk: SKDocumentCreateWithURL(%s) failed",
			str_c(url));

	CFRelease_and_null(docurl);
	return doc;
}

static const char *sk_clean_term(const char *term, bool *quote)
{
	string_t *clean;

	if (strcmp(term, "NOT") == 0 || strcmp(term, "AND") == 0 ||
	    strcmp(term, "OR") == 0) {
		/* quote around SearchKit operators */
		*quote = TRUE;
		return term;
	}

	clean = t_str_new(64);
	while (*term) {
		char c = *term;
		unsigned int len = uni_utf8_char_bytes(c);
		if (len == 1) {
			if ((unsigned char) c <= '\"' ||
			    strchr("&|*()", c) != NULL) {
				/* escape SearchKit operators */
				str_append_c(clean, '\\');
			}
		} else if (len == 0)
			len = 1;
		str_append_n(clean, term, len);
		term += len;
	}

	return str_c(clean);
}

static bool sk_build_query(CFMutableStringRef query, const char *term)
{
	bool definite = TRUE;
	const char **words;
	unsigned int count, i;

	words = t_strsplit_spaces(term, " \t");
	for (count = 0; words[count] != NULL; ++count)
		;

	if (count > 1)
		CFStringAppend(query, CFSTR("("));

	for (i = 0; i < count; i++) {
		bool quote = FALSE;
		const char *word = sk_clean_term(words[i], &quote);

		if (i > 0)
			CFStringAppend(query, CFSTR(" "));

		if (quote) {
			CFStringAppend(query, CFSTR("\""));
			CFStringAppendCString(query, word, kCFStringEncodingUTF8);
			CFStringAppend(query, CFSTR("\""));

			/* SearchKit can't do subphrase matches */
			definite = FALSE;
		} else {
			/* IMAP requires substring match always */
			CFStringAppend(query, CFSTR("*"));
			CFStringAppendCString(query, word, kCFStringEncodingUTF8);
			CFStringAppend(query, CFSTR("*"));
		}
	}

	if (count > 1)
		CFStringAppend(query, CFSTR(")"));

	return definite;
}

static int sk_score_cmp(const struct fts_score_map *s1,
			const struct fts_score_map *s2)
{
	return s1->uid < s2->uid ? -1 :
		(s1->uid > s2->uid ? 1 : 0);
}

static void sk_search_do(struct sk_fts_backend *backend,
			 struct sk_fts_search_context *sctx,
			 enum fts_lookup_flags flags,
			 const ARRAY_TYPE(seq_range) *expunged,
			 ARRAY_TYPE(seq_range) *uids,
			 ARRAY_TYPE(fts_score_map) *score_map)
{
	enum { SK_MAXDOCS = 50 };
	SKDocumentID results[SK_MAXDOCS];
	float scores[SK_MAXDOCS];
	CFStringRef names[SK_MAXDOCS];
	CFIndex found = 0, i;

	memset(results, 0, sizeof results);
	if (score_map != NULL)
		memset(scores, 0, sizeof scores);
	memset(names, 0, sizeof names);
	sctx->done = !SKSearchFindMatches(sctx->search, SK_MAXDOCS, results,
					  score_map != NULL ? scores : NULL,
					  backend->search_secs, &found);
	SKIndexCopyInfoForDocumentIDs(sctx->skindex->skiref, found, results,
				      names, NULL);
	for (i = 0; i < found; i++) {
		char name[128], where[128];
		uint32_t uid = 0;

		if (names[i] == NULL)
			continue;

		if (!CFStringGetCString(names[i], name, sizeof name,
					kCFStringEncodingUTF8)) {
			CFRelease_and_null(names[i]);
			continue;
		}
		CFRelease_and_null(names[i]);

		where[0] = '\0';
		if (sscanf(name, "uid=%u&where=%s", &uid, where) != 2 ||
		    uid == 0)
			continue;

		if ((flags & FTS_LOOKUP_FLAG_HEADER) == 0 &&
		    strncmp(where, sk_wheres[SK_HEADERS],
			    sk_where_lengths[SK_HEADERS]) == 0)
			continue;	/* body only */
		if ((flags & FTS_LOOKUP_FLAG_BODY) == 0 &&
		    strncmp(where, sk_wheres[SK_BODY],
			    sk_where_lengths[SK_BODY]) == 0)
			continue;	/* header only */
		if (strncmp(where, sk_wheres[SK_SHADOW],
			    sk_where_lengths[SK_SHADOW]) == 0)
			continue;	/* shadows are internal only */

		if (seq_range_exists(expunged, uid))
			continue;

		seq_range_array_add(uids, found, uid);

		if (score_map != NULL) {
			struct fts_score_map *score =
				array_append_space(score_map);
			score->uid = uid;
			score->score = scores[i];
		}
	}
}

static int sk_search(struct sk_fts_backend *backend, CFStringRef query,
		     enum fts_lookup_flags flags,
		     ARRAY_TYPE(seq_range) *uids,
		     ARRAY_TYPE(fts_score_map) *score_map)
{
	struct sk_meta_values *values = NULL;
	ARRAY_TYPE(seq_range) expunged;
	unsigned int count, i;
	const struct sk_fts_index *skindex;
	ARRAY_DEFINE(searches, struct sk_fts_search_context);
	struct sk_fts_search_context *sctx;
	bool more;

	/* searching may find uids that have been expunge-deferred;
	   skip those results */
	if (sk_get_meta_values(backend, &values) < 0)
		return -1;
	count = values->expungedUids != NULL ?
		CFArrayGetCount(values->expungedUids) : 0;
	t_array_init(&expunged, count);
	for (i = 0; i < count; i++) {
		CFNumberRef num = (CFNumberRef)
			CFArrayGetValueAtIndex(values->expungedUids, i);
		uint32_t uid;

		if (num == NULL ||
		    !CFNumberGetValue(num, kCFNumberSInt32Type, &uid))
			uid = (uint32_t) -1;
		if (uid != (uint32_t) -1)
			seq_range_array_add(&expunged, count, uid);
	}
	sk_meta_values_deinit(&values);

	skindex = array_get(&backend->indexes, &count);
	t_array_init(&searches, count);
	for (i = 0; i < count; i++) {
		SKSearchRef search;

		/* SK requires flush before search */
		if (!SKIndexFlush(skindex[i].skiref)) {
			i_error("fts_sk: SKIndexFlush(%s) failed",
				skindex[i].path);
			return -1;
		}

		search = SKSearchCreate(skindex[i].skiref, query,
					score_map != NULL ?
					kSKSearchOptionDefault :
					kSKSearchOptionNoRelevanceScores);
		if (search == NULL) {
			char qbuf[1024];
			if (!CFStringGetCString(query, qbuf, sizeof qbuf,
						kCFStringEncodingUTF8))
				strcpy(qbuf, "(unknown)");
			i_error("fts_sk: SKSearchCreate(%s, %s) failed",
				skindex[i].path, qbuf);
			break;
		}

		sctx = array_append_space(&searches);
		sctx->skindex = &skindex[i];
		sctx->search = search;
	}
	if (i < count) {
		array_foreach_modifiable(&searches, sctx)
			CFRelease_and_null(sctx->search);
		return -1;
	}

	/* search across all the indexes in parallel */
	sctx = array_get_modifiable(&searches, &count);
	do {
		more = FALSE;
		for (i = 0; i < count; i++) {
			if (sctx[i].done)
				continue;
			sk_search_do(backend, &sctx[i], flags, &expunged,
				     uids, score_map);
			if (sctx[i].done)
				CFRelease_and_null(sctx[i].search);
			else
				more = TRUE;
		}
	} while (more);

	/* fts_mail_get_special() expects the scores to be sorted by uid */
	if (score_map != NULL)
		array_sort(score_map, sk_score_cmp);

	return 1;
}

static int sk_spill_init(struct sk_fts_backend_build_context *ctx)
{
	struct sk_fts_backend *backend =
		(struct sk_fts_backend *) ctx->ctx.backend;

	if (ctx->spill_path == NULL) {
		ctx->spill_path = str_new(default_pool, 128);
		str_printfa(ctx->spill_path, "%s/", backend->spill_dir);
		sk_doc_mkname(ctx->spill_path, ctx->cur_uid, ctx->cur_where,
			      ctx->cur_seq);
		str_append(ctx->spill_path, "&rand=");
	}

	ctx->spill_fd = safe_mkstemp(ctx->spill_path, 0600, (uid_t) -1,
				     backend->create_gid);
	if (ctx->spill_fd < 0) {
		i_error("fts_sk: safe_mkstemp(%s) failed: %m",
			str_c(ctx->spill_path));
		return -1;
	}

	return 1;
}

static int sk_spill(struct sk_fts_backend_build_context *ctx)
{
	const void *cur_data;
	size_t cur_size;

	cur_data = buffer_get_data(ctx->cur_text, &cur_size);
	if (cur_size == 0)
		return 1;

	if (ctx->spill_fd < 0) {
		if (sk_spill_init(ctx) < 0)
			return -1;
	}

	if (write_full(ctx->spill_fd, cur_data, cur_size) < 0) {
		i_error("fts_sk: write_full(%s, %"PRIuSIZE_T") failed: %m",
			str_c(ctx->spill_path), cur_size);
		return -1;
	}

	buffer_set_used_size(ctx->cur_text, 0);
	return 1;
}

static void sk_indexer_end_clean(struct sk_fts_backend_build_context *ctx)
{
	int status;

	io_loop_stop(ctx->indexer_ioloop);

	if (waitpid(ctx->indexer_pid, &status, 0) < 0)
		i_error("fts_sk: waitpid(%d) failed: %m", ctx->indexer_pid);
	else if (WIFEXITED(status)) {
		if (WEXITSTATUS(status) != 0)
			i_error("fts_sk: indexer process %d exited with code %d",
				ctx->indexer_pid, WEXITSTATUS(status));
	} else if (WIFSIGNALED(status))
		i_error("fts_sk: indexer process %d terminated abnormally, "
			"signal %d", ctx->indexer_pid, WTERMSIG(status));
	else if (WIFSTOPPED(status))
		i_error("fts_sk: indexer process %d stopped, signal %d",
			ctx->indexer_pid, WSTOPSIG(status));
	else
		i_error("fts_sk: indexer process %d terminated abnormally, "
			"exit status %d", ctx->indexer_pid, status);
}

static void sk_indexer_end_dirty(struct sk_fts_backend_build_context *ctx)
{
	if (ctx->indexer_ioloop != NULL)
		io_loop_stop(ctx->indexer_ioloop);

	if (kill(ctx->indexer_pid, SIGKILL) < 0) {
		if (errno == ESRCH)
			return;
		i_error("fts_sk: kill(%d, %d) failed: %m", ctx->indexer_pid,
			SIGKILL);
	}

	i_warning("fts_sk: killed unresponsive indexer process %d",
		  ctx->indexer_pid);
	(void) waitpid(ctx->indexer_pid, NULL, 0);
}

static void sk_indexer_end(struct sk_fts_backend_build_context *ctx)
{
	struct sk_fts_backend *backend =
		(struct sk_fts_backend *) ctx->ctx.backend;
	struct io *io;
	struct timeout *timeout;

	if (ctx->indexer_pid < 0)
		return;

	ctx->indexer_synchronous = TRUE;
	if (ctx->indexer_io != NULL)
		io_remove(&ctx->indexer_io);
	if (ctx->indexer_timeout != NULL)
		timeout_remove(&ctx->indexer_timeout);
	if (ctx->indexer_ioloop != NULL)
		io_loop_destroy(&ctx->indexer_ioloop);
	o_stream_close(ctx->to_indexer);

	if (ctx->indexer_timed_out) {
		ctx->indexer_ioloop = NULL;
		sk_indexer_end_dirty(ctx);
	} else {
		ctx->indexer_ioloop = io_loop_create();
		io = io_add(ctx->from_indexer_fd, IO_READ, sk_indexer_end_clean,
			    ctx);
		timeout = timeout_add(backend->indexer_timeout_secs * 1000,
				      sk_indexer_end_dirty, ctx);
		io_loop_run(ctx->indexer_ioloop);
		timeout_remove(&timeout);
		io_remove(&io);
		io_loop_destroy(&ctx->indexer_ioloop);
	}

	ctx->indexer_pid = -1;
	o_stream_destroy(&ctx->to_indexer);
	i_stream_destroy(&ctx->from_indexer);
	ctx->from_indexer_fd = -1;

	if (ctx->indexer_tag != NULL)
		i_free_and_null(ctx->indexer_tag);

	ctx->use_indexer_forever = FALSE;
	ctx->indexer_shook = FALSE;
	ctx->indexer_synchronous = FALSE;
	ctx->indexer_shadowing = FALSE;
}

static void sk_indexer_drop_privileges(void)
{
	int euid, ruid;
	const char *error;

	euid = geteuid();
	ruid = getuid();
	i_assert(euid != 0);

	if (seteuid(0) == 0) {
		if (setuid(euid) < 0)
			i_fatal("fts_sk: setuid(%d) failed: %m "
				"(euid=%d ruid=%d)",
				euid, geteuid(), getuid());
	} else
		(void) setuid(euid);

	if (seteuid(0) == 0)
		error = "seteuid(0) succeeded";
	else if (setuid(0) == 0)
		error = "setuid(0) succeeded";
	else if (geteuid() == 0)
		error = "euid=0";
	else if (getuid() == 0)
		error = "ruid=0";
	else
		error = NULL;
	if (error != NULL)
		i_fatal("fts_sk: failed to drop root privileges: %s "
			"(euid=%d ruid=%d)", error, geteuid(), getuid());
}

static bool sk_indexer_sandbox(struct sk_fts_backend_build_context *ctx)
{
	struct sk_fts_backend *backend =
		(struct sk_fts_backend *) ctx->ctx.backend;
	unsigned int conversions;
	const char *profile, *spill_dir_clean, *index_path, *index_path_clean,
		*spill_allow, *index_allow;
	char *spill_dir_real, *index_path_real, *errorbuf;

	/* make sure the profile has exactly the expected number of %'s */
	conversions = 0;
	for (profile = sk_sandbox_profile; *profile; profile++)
		if (*profile == '%')
			++conversions;
	i_assert(conversions == 4);

	/* ok, safe to printf */
	spill_dir_clean = str_escape(backend->spill_dir);
	spill_dir_real = realpath(backend->spill_dir, NULL);
	if (spill_dir_real != NULL) {
		const char *spill_dir_real_clean = str_escape(spill_dir_real);
		free(spill_dir_real);
		spill_allow = t_strdup_printf("(subpath \"%s\")\n"
					      "(subpath \"%s\")",
					      spill_dir_clean,
					      spill_dir_real_clean);
	} else
		spill_allow = t_strdup_printf("(subpath \"%s\")",
					      spill_dir_clean);

	index_path = (array_idx(&backend->indexes, ctx->fragment))->path;
	index_path_clean = str_escape(index_path);
	index_path_real = realpath(index_path, NULL);
	if (index_path_real != NULL) {
		const char *index_path_real_clean = str_escape(index_path_real);
		free(index_path_real);
		index_allow = t_strdup_printf("(literal \"%s\")\n"
					      "(literal \"%s\")",
					      index_path_clean,
					      index_path_real_clean);
	} else
		index_allow = t_strdup_printf("(literal \"%s\")",
					      index_path_clean);
	profile = t_strdup_printf(sk_sandbox_profile,
				  backend->debug ? "" : " (with no-log)",
				  backend->debug ? "deny" : "none",
				  spill_allow,
				  index_allow);

	errorbuf = NULL;
	if (sandbox_init(profile, 0, &errorbuf) != 0) {
		i_error("fts_sk: sandbox_init failed: %s", errorbuf);
		sandbox_free_error(errorbuf);
		return FALSE;
	}

	return TRUE;
}

static void sk_indexer_child_exec(struct sk_fts_backend_build_context *ctx)
{
	struct sk_fts_backend *backend =
		(struct sk_fts_backend *) ctx->ctx.backend;
	char *args[5];
	int fd;

	i_set_fatal_handler(NULL);
	i_set_error_handler(NULL);
	i_set_info_handler(NULL);

	sk_indexer_drop_privileges();

	/* if the sandbox can't be established, run the indexer anyway */
	(void) sk_indexer_sandbox(ctx);

	for (fd = getdtablesize(); --fd >= 3; )
		(void) close(fd);

	args[0] = SK_INDEXER;
	args[1] = backend->spill_dir;
	args[2] = (array_idx(&backend->indexes, ctx->fragment))->path;
	args[3] = SK_INDEX_NAME;
	args[4] = NULL;
	execv(SK_INDEXER_EXECUTABLE, args);
	i_fatal_status(FATAL_EXEC, "fts_sk: execv(%s) failed: %m",
		       SK_INDEXER_EXECUTABLE);
}

static int sk_indexer_sync(struct sk_fts_backend_build_context *ctx);

static int sk_indexer_start(struct sk_fts_backend_build_context *ctx)
{
	pid_t pid;
	int parent_to_child[2], child_to_parent[2];

	if (ctx->indexer_pid > 0)
		return 1;

	if (pipe(parent_to_child) < 0) {
		i_error("fts_sk: pipe() failed: %m");
		return -1;
	}
	if (pipe(child_to_parent) < 0) {
		i_error("fts_sk: pipe() failed: %m");
		(void) close(parent_to_child[0]);
		(void) close(parent_to_child[1]);
		return -1;
	}

	pid = fork();
	if (pid < 0) {
		i_error("fts_sk: fork() failed: %m");
		(void) close(parent_to_child[0]);
		(void) close(parent_to_child[1]);
		(void) close(child_to_parent[0]);
		(void) close(child_to_parent[1]);
		return -1;
	}

	if (pid == 0) {
		(void) close(parent_to_child[1]);
		(void) close(child_to_parent[0]);

		if (dup2(parent_to_child[0], 0) < 0)
			i_fatal("fts_sk: dup2(stdin) failed: %m");
		if (dup2(child_to_parent[1], 1) < 0)
			i_fatal("fts_sk: dup2(stdout) failed: %m");
		if (dup2(child_to_parent[1], 2) < 0)
			i_fatal("fts_sk: dup2(stderr) failed: %m");

		sk_indexer_child_exec(ctx);
	}

	(void) close(parent_to_child[0]);
	fd_set_nonblock(parent_to_child[1], TRUE);
	fd_set_nonblock(child_to_parent[0], TRUE);
	(void) close(child_to_parent[1]);

	ctx->use_indexer_forever = TRUE;
	ctx->indexer_pid = pid;
	ctx->to_indexer = o_stream_create_fd(parent_to_child[1], 8192, TRUE);
	ctx->from_indexer = i_stream_create_fd(child_to_parent[0], 8192, TRUE);
	ctx->from_indexer_fd = child_to_parent[0];

	if (o_stream_send_str(ctx->to_indexer,
			      "version\t"PACKAGE_VERSION"\n") <= 0) {
		i_error("fts_sk: sending handshake to indexer process %d failed",
			pid);
		sk_indexer_end(ctx);
		return -1;
	}

	/* now if possible, go away and do some other work until the indexer
	   signals readiness by sending us its version string */
	return sk_indexer_sync(ctx);
}

static int sk_indexer_handle_reply(struct sk_fts_backend_build_context *ctx)
{
	size_t taglen;

	/* arrgh! <rdar://problem/7819581> */
	if (strstr(ctx->indexer_reply, "CFPreferences") != NULL)
		return 0;
	if (strstr(ctx->indexer_reply, "bootstrap_look_up") != NULL)
		return 0;

#ifdef DEBUG
	if (strstr(ctx->indexer_reply, "DEBUG") != NULL) {
		i_debug("%s", ctx->indexer_reply);
		return 0;
	}
#endif

	if (strncmp(ctx->indexer_reply, "version\t", 8) == 0) {
		bool already_shook = ctx->indexer_shook;
		ctx->indexer_shook = TRUE;
		if (strcmp(ctx->indexer_reply + 8, PACKAGE_VERSION) == 0) {
			/* the first version lets us continue from
			   sk_indexer_start().  others are irrelevant */
			return !already_shook;
		}
		i_error("fts_sk: indexer version mismatch: "
			"process %d ("SK_INDEXER_EXECUTABLE") is v%s, "
			"imap is v"PACKAGE_VERSION,
			ctx->indexer_pid, ctx->indexer_reply + 8);
		return -1;
	} else if (!ctx->indexer_shook) {
		i_error("fts_sk: indexer process %d failed to identify itself"
			" (greeted with \"%s\")",
			ctx->indexer_pid, ctx->indexer_reply);
		return -1;
	}

	/* if it's a tagged response, keep it */
	taglen = strlen(ctx->indexer_tag);
	if (strncmp(ctx->indexer_reply, ctx->indexer_tag, taglen) == 0 &&
	    ctx->indexer_reply[taglen] == '\t')
		return 1;

	/* otherwise log it and keep reading */
	i_info("fts_sk: indexer process %d: %s", ctx->indexer_pid,
	       ctx->indexer_reply);
	return 0;
}

static bool sk_build_cancel(struct client_command_context *cmd)
{
	i_assert(cmd->cancel);

	i_assert(cmd->func == sk_build_cancel);
	cmd->func = sk_cmd_search_more;

	/* cleanly cancel the search */
	(void) sk_cmd_search_more(cmd);

	/* sk_build_cancelling needed ignore=TRUE */
	cmd->client->ignore = FALSE;

	return TRUE;
}

static bool sk_build_cancelling(struct sk_fts_backend_build_context *ctx)
{
	struct mail_search_context *mail_ctx;
	struct imap_search_context *imap_ctx;
	struct client_command_context *cmd;

	if (ctx->ctx.fts_ctx == NULL)
		return FALSE;

	mail_ctx = fts_build_get_mail_context(ctx->ctx.fts_ctx);
	if (mail_ctx == NULL)
		return FALSE;

	imap_ctx = mail_ctx->imap_ctx;
	if (imap_ctx == NULL)
		return FALSE;

	cmd = imap_ctx->cmd;
	if (cmd == NULL)
		return FALSE;

	return cmd->client->ignore;
}

static void sk_indexer_resume_build(struct sk_fts_backend_build_context *ctx)
{
	struct mail_search_context *mail_ctx;
	struct imap_search_context *imap_ctx;
	struct client_command_context *cmd;

	/* nobody said reacharounds were pretty.
	   the assertions are for figuring out where it all went south */
	i_assert(ctx->ctx.fts_ctx != NULL);
	mail_ctx = fts_build_get_mail_context(ctx->ctx.fts_ctx);
	i_assert(mail_ctx != NULL);
	imap_ctx = mail_ctx->imap_ctx;
	i_assert(imap_ctx != NULL);
	i_assert(imap_ctx->to == NULL);
	cmd = imap_ctx->cmd;
	i_assert(cmd != NULL);
	imap_ctx->to = timeout_add(0, sk_cmd_search_more_callback, cmd);
	i_assert(!sk_assert_build_cancel || cmd->func == sk_build_cancel);
	cmd->func = sk_cmd_search_more;

	/* client_add_missing_io() restores cmd->client->io for us */
	cmd->client->ignore = FALSE;

	if (ctx->indexer_io != NULL)
		io_remove(&ctx->indexer_io);
	if (ctx->indexer_timeout != NULL)
		timeout_remove(&ctx->indexer_timeout);
}

static void sk_indexer_input(struct sk_fts_backend_build_context *ctx)
{
	int ret;

	while ((ret = i_stream_read(ctx->from_indexer)) > 0) {
		while ((ctx->indexer_reply =
			i_stream_next_line(ctx->from_indexer)) != NULL) {
			if (sk_indexer_handle_reply(ctx) != 0)
				break;
		}

		if (ctx->indexer_reply != NULL)
			break;
	}

	if (ret < 0 || ctx->indexer_reply != NULL) {
		if (ctx->indexer_reply == NULL)
			ctx->indexer_reply = "[EOF]";
		if (ctx->indexer_synchronous)
			io_loop_stop(ctx->indexer_ioloop);
		else
			sk_indexer_resume_build(ctx);
	}
}

static void sk_indexer_timeout(struct sk_fts_backend_build_context *ctx)
{
	ctx->indexer_timed_out = TRUE;
	if (ctx->indexer_synchronous)
		io_loop_stop(ctx->indexer_ioloop);
	else
		sk_indexer_resume_build(ctx);
}

static int sk_indexer_sync(struct sk_fts_backend_build_context *ctx)
{
	struct sk_fts_backend *backend =
		(struct sk_fts_backend *) ctx->ctx.backend;
	struct mail_search_context *mail_ctx = NULL;
	struct imap_search_context *imap_ctx = NULL;
	struct client_command_context *cmd = NULL;

	/* nobody said reacharounds were pretty.
	   conditions should be right to use async I/O, but just in case they
	   aren't, fall back to synchronous. */
	if (ctx->ctx.fts_ctx != NULL)
		mail_ctx = fts_build_get_mail_context(ctx->ctx.fts_ctx);
	if (mail_ctx != NULL)
		imap_ctx = mail_ctx->imap_ctx;
	if (imap_ctx != NULL)
		cmd = imap_ctx->cmd;
	if (cmd == NULL || sk_cmd_search_more == NULL ||
	    cmd->func != sk_cmd_search_more)
		ctx->indexer_synchronous = TRUE;

	if (ctx->indexer_synchronous) {
		struct io *io;
		struct timeout *timeout;

		/* wait right here for the reply from the indexer */

		ctx->indexer_ioloop = io_loop_create();
		io = io_add(ctx->from_indexer_fd, IO_READ,
			    sk_indexer_input, ctx);
		timeout =
		       timeout_add(backend->indexer_timeout_secs * 1000,
				   sk_indexer_timeout, ctx);
		io_loop_run(ctx->indexer_ioloop);
		timeout_remove(&timeout);
		io_remove(&io);
		io_loop_destroy(&ctx->indexer_ioloop);

		return 1;
	} else {
		/* trade the client I/O for I/O from the indexer,
		   and request a continuation */

		if (cmd->client->io != NULL)
			io_remove(&cmd->client->io);
		cmd->client->ignore = TRUE;
		if (imap_ctx->to != NULL)
			timeout_remove(&imap_ctx->to);

		ctx->indexer_io = io_add(ctx->from_indexer_fd, IO_READ,
					 sk_indexer_input, ctx);
		ctx->indexer_timeout =
		       timeout_add(backend->indexer_timeout_secs * 1000,
				   sk_indexer_timeout, ctx);

		/* tell imap_search_start not to set its immediate
		   timeout, and cancel search gracefully if needed */
		cmd->func = sk_build_cancel;
		i_assert(cmd->context != NULL);

		return 0;
	}
}

static void sk_save_id(struct sk_fts_backend_build_context *ctx,
		       SKDocumentID id)
{
	CFNumberRef num;

	num = CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &id);
	sk_nonnull(num, "CFNumberCreate(id)");
	CFArrayAppendValue(ctx->cur_shadow_ids, num);
	CFRelease_and_null(num);
}

static void sk_indexer_save_id(struct sk_fts_backend_build_context *ctx,
			       const char *idstr)
{
	char *endstr;
	SKDocumentID id;

	id = strtol(idstr, &endstr, 10);
	if (*idstr == '\0' || *endstr != '\0')
		i_error("fts_sk: bad document ID from indexer process %d: %s",
			ctx->indexer_pid, idstr);
	else
		sk_save_id(ctx, id);
}

typedef void sk_indexer_reply_callback_t(struct sk_fts_backend_build_context *,
					 const char *reply);

static int sk_indexer_do(struct sk_fts_backend_build_context *ctx,
			 const char *cmd,
			 sk_indexer_reply_callback_t reply_callback)
{
	struct sk_fts_backend *backend =
		(struct sk_fts_backend *) ctx->ctx.backend;
	int ret;

	/* caution:  if we return 0, we are requesting a continuation */

	if (ctx->indexer_tag == NULL) {	/* new command, not continuation */
		unsigned char randbuf[8];
		const char *line;

		ret = sk_indexer_start(ctx);
		if (ret <= 0)
			return ret;

		random_fill_weak(randbuf, sizeof randbuf);
		ctx->indexer_tag = i_strdup(binary_to_hex(randbuf,
							  sizeof randbuf));
		line = t_strdup_printf("%s\t%s\n", ctx->indexer_tag, cmd);
		if (o_stream_send_str(ctx->to_indexer, line) <= 0) {
			i_error("fts_sk: sending command to "
				"indexer process %d failed", ctx->indexer_pid);
			sk_indexer_end(ctx);
			return -1;
		}

		ret = sk_indexer_sync(ctx);
		if (ret == 0)
			return 0;
	}

	ret = -1;
	if (ctx->indexer_reply != NULL) {
		size_t randlen = strlen(ctx->indexer_tag);
		if (strncmp(ctx->indexer_reply, ctx->indexer_tag,
			    randlen) == 0 && 
		    ctx->indexer_reply[randlen] == '\t')
			ret = 1;
		if (ret == 1 &&
		    strncmp(&ctx->indexer_reply[randlen + 1], "OK\t", 3) == 0) {
			if (reply_callback != NULL)
				reply_callback(ctx,
					&ctx->indexer_reply[randlen + 4]);
		} else if (backend->debug)
			i_error("fts_sk: indexer process %d "
				"failed command \"%s\": %s",
				ctx->indexer_pid, cmd, ctx->indexer_reply);
	} else
		i_error("fts_sk: timed out waiting for indexer process %d "
			"to execute command \"%s\"", ctx->indexer_pid, cmd);

	ctx->indexer_reply = NULL;
	i_free_and_null(ctx->indexer_tag);
	if (ctx->indexer_io != NULL)
		io_remove(&ctx->indexer_io);
	if (ctx->indexer_timeout != NULL)
		timeout_remove(&ctx->indexer_timeout);
	if (ret < 0)
		sk_indexer_end(ctx);

	return ret;
}

static int sk_index_path_indexer(struct sk_fts_backend_build_context *ctx,
				 const char *path)
{
	const char *cmd;

	cmd = t_strdup_printf("index\t%s\t%s", path, ctx->cur_content_type);
	return sk_indexer_do(ctx, cmd, sk_indexer_save_id);
}

static int sk_shadow_indexer(struct sk_fts_backend_build_context *ctx,
			     const char *path)
{
	string_t *cmd;
	CFIndex count, i;
	bool first = TRUE;
	int ret;

	cmd = t_str_new(128);
	str_printfa(cmd, "shadow\t%s\tids=", path);

	count = CFArrayGetCount(ctx->cur_shadow_ids);
	for (i = 0; i < count; i++) {
		CFNumberRef num;
		SKDocumentID id;

		num = CFArrayGetValueAtIndex(ctx->cur_shadow_ids, i);
		if (CFNumberGetValue(num, kCFNumberCFIndexType, &id)) {
			if (!first)
				str_append_c(cmd, ',');
			str_printfa(cmd, "%ld", id);
			first = FALSE;
		}
	}

	ret = sk_indexer_do(ctx, str_c(cmd), NULL);
	if (ret == 0)
		return 0;

	CFArrayRemoveAllValues(ctx->cur_shadow_ids);
	return ret;
}

static int sk_spill_flush(struct sk_fts_backend_build_context *ctx)
{
	struct sk_fts_backend *backend =
		(struct sk_fts_backend *) ctx->ctx.backend;
	int ret = 1;

	/* caution:  if we return 0, we are requesting a continuation */

	if (ctx->indexer_tag == NULL) {
		ret = sk_spill(ctx);
		if (ret < 0 || ctx->spill_path == NULL)
			return ret;

		sk_close(backend);
	}

	if (!ctx->indexer_shadowing) {
		ret = sk_index_path_indexer(ctx, str_c(ctx->spill_path));
		if (ret == 0)
			return 0;

		if (ret > 0)
			ctx->values->last_uid = ctx->cur_uid;
	}

	if (ret > 0 && ctx->cur_where == SK_SHADOW) {
		ret = sk_shadow_indexer(ctx, str_c(ctx->spill_path));
		if (ret == 0) {
			ctx->indexer_shadowing = TRUE;
			return 0;
		}
		ctx->indexer_shadowing = FALSE;
	}

	if (close(ctx->spill_fd) < 0)
		i_error("fts_sk: close(%s) failed: %m", str_c(ctx->spill_path));
	ctx->spill_fd = -1;

	if (unlink(str_c(ctx->spill_path)) < 0)
		i_error("fts_sk: unlink(%s) failed: %m",
			str_c(ctx->spill_path));
	str_free(&ctx->spill_path);

	return ret;
}

static int sk_build_flush(struct sk_fts_backend_build_context *ctx)
{
	struct sk_fts_backend *backend =
		(struct sk_fts_backend *) ctx->ctx.backend;
	const void *cur_data;
	size_t cur_size;
	SKDocumentRef doc;
	CFStringRef text;
	const struct sk_fts_index *skindex;
	int ret;

	/* caution:  if we return 0, we are requesting a continuation */

	/* it's probably safe to index small plain text internally, but
	   once we use the external indexer it's faster to always use it */
	if (ctx->spill_fd >= 0 || ctx->use_indexer_forever ||
	    (strncasecmp(ctx->cur_content_type, "text/", 5) != 0 &&
	     strcasecmp(ctx->cur_content_type, "text") != 0))
		return sk_spill_flush(ctx);

	cur_data = buffer_get_data(ctx->cur_text, &cur_size);
	if (cur_size == 0)
		return 1;

	if (sk_open_or_create(backend, FALSE) <= 0)
		ret = -1;

	doc = sk_create_document_immediate(ctx->cur_uid, ctx->cur_where,
					   ctx->cur_seq);
	if (doc == NULL)
		return -1;

	text = CFStringCreateWithBytesNoCopy(kCFAllocatorDefault,
					     cur_data, cur_size,
					     kCFStringEncodingUTF8,
					     FALSE, kCFAllocatorNull);
	if (text == NULL) {
		/* maybe non-text chars in cur_text.  perhaps the
		   external indexer can make something of it */
		CFRelease_and_null(doc);
		sk_close(backend);
		return sk_spill_flush(ctx);
	}

	skindex = array_idx(&backend->indexes, ctx->fragment);
	if (SKIndexAddDocumentWithText(skindex->skiref, doc, text, TRUE)) {
		ctx->values->last_uid = ctx->cur_uid;
		buffer_set_used_size(ctx->cur_text, 0);
		ret = 1;

		sk_save_id(ctx, SKIndexGetDocumentID(skindex->skiref, doc));
	} else {
		i_error("fts_sk: SKIndexAddDocumentWithText(path=%s, size=%lu) failed",
			skindex->path, cur_size);
		ret = -1;
	}
	CFRelease_and_null(text);

	if (ret > 0 && ctx->cur_where == SK_SHADOW) {
		CFMutableDictionaryRef properties;

		properties =
			CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
				&kCFTypeDictionaryKeyCallBacks,
				&kCFTypeDictionaryValueCallBacks);
		sk_nonnull(properties, "CFDictionaryCreateMutable(shadow properties)");

		CFDictionaryAddValue(properties, CFSTR(SK_SHADOW_NAME),
				     ctx->cur_shadow_ids);
		CFRelease_and_null(ctx->cur_shadow_ids);
		ctx->cur_shadow_ids = CFArrayCreateMutable(kCFAllocatorDefault,
						0, &kCFTypeArrayCallBacks);
		sk_nonnull(ctx->cur_shadow_ids,
			   "CFArrayCreateMutable(shadow_ids)");

		SKIndexSetDocumentProperties(skindex->skiref, doc, properties);
		CFRelease_and_null(properties);
	}

	CFRelease_and_null(doc);

	return ret;
}

static int sk_assign_fragment(struct sk_fts_backend_build_context *ctx)
{
	struct sk_fts_backend *backend =
		(struct sk_fts_backend *) ctx->ctx.backend;
	struct sk_fts_index *skindex;
	unsigned int count, i;
	struct stat stbuf;
	struct sk_meta_values *values = NULL;

	/* continue to use same fragment? */
	skindex = array_get_modifiable(&backend->indexes, &count);
	if (ctx->fragment != (unsigned int) -1) {
		if (ctx->fragment >= count)
			i_panic("fts_sk: ctx->fragment=%u >= count=%u",
				ctx->fragment, count);

		if (stat(skindex[ctx->fragment].path, &stbuf) < 0) {
			i_error("fts_sk: stat(%s): %m",
				skindex[ctx->fragment].path);
			return -1;
		}
		if (stbuf.st_size < SK_FRAGMENT_THRESHOLD_SIZE)
			return 1;
	}

	/* use the first fragment smaller than the threshold */
	for (i = 0; i < count; i++) {
		if (i == ctx->fragment)
			continue;

		if (stat(skindex[i].path, &stbuf) < 0) {
			i_error("fts_sk: stat(%s): %m", skindex[i].path);
			return -1;
		}
		if (stbuf.st_size < SK_FRAGMENT_THRESHOLD_SIZE) {
			ctx->fragment = i;
			return 1;
		}
	}

	/* create a new fragment */
	i_assert(buffer_get_used_size(ctx->cur_text) == 0);
	sk_indexer_end(ctx);
	if (sk_open(backend, TRUE, FALSE) <= 0)
		return -1;

	/* will need meta values later */
	if (sk_get_meta_values(backend, &values) < 0) {
		sk_close(backend);
		return -1;
	}
	skindex = array_append_space(&backend->indexes);
	skindex->path = i_strdup_printf("%s-%u",
					(array_idx(&backend->indexes, 0))->path,
					count);
	if (sk_create(backend, count) < 0) {
		i_error("fts_sk: could not create new index fragment %s",
			skindex->path);
		sk_meta_values_deinit(&values);
		sk_close(backend);
		return -1;
	}
	ctx->fragment = count;

	/* register the new fragment in the meta properties */
	if (sk_set_meta_values(backend, values) < 0) {
		sk_meta_values_deinit(&values);
		sk_close(backend);
		(void) unlink(skindex->path);
		return -1;
	}
	sk_meta_values_deinit(&values);

	return 1;
}

static int sk_build_shadow(struct sk_fts_backend_build_context *ctx);

static int sk_build_common(struct sk_fts_backend_build_context *ctx,
			   uint32_t uid, const unsigned char *data, size_t size,
			   enum sk_where where, const char *content_type)
{
	int ret = 1;

	i_assert(uid >= ctx->values->last_uid);

	/* caution:  if we return 0, we are requesting a continuation */

	if (uid != ctx->cur_uid || where != ctx->cur_where ||
	    strcasecmp(content_type, ctx->cur_content_type) != 0) {
		if (uid != ctx->cur_uid && ctx->cur_where != SK_SHADOW) {
			ret = sk_build_shadow(ctx);
			if (ret <= 0)
				return ret;
		}

		ret = sk_build_flush(ctx);
		if (ret <= 0)
			return ret;

		/* we come through here multiple times for the body of a
		   a single uid, but SK needs unique document names */
		if (uid == ctx->cur_uid)
			++ctx->cur_seq;
		else {
			ctx->cur_seq = 0;

			if (sk_assign_fragment(ctx) < 0)
				return -1;
		}

		ctx->cur_uid = uid;
		ctx->cur_where = where;
		i_free(ctx->cur_content_type);
		ctx->cur_content_type = i_strdup(content_type);
	}

	if (buffer_get_used_size(ctx->cur_text) + size >
	    buffer_get_size(ctx->cur_text)) {
		ret = sk_spill(ctx);
		if (ret < 0)
			return ret;
	}

	buffer_append(ctx->cur_text, data, size);

	i_assert(ret != 0);
	return ret;
}

static int sk_build_shadow(struct sk_fts_backend_build_context *ctx)
{
	string_t *data;

	if (ctx->cur_uid == 0)
		return 1;

	data = t_str_new(32);
	str_printfa(data, "FTS%uSK", ctx->cur_uid);
	return sk_build_common(ctx, ctx->cur_uid, str_data(data), str_len(data),
			       SK_SHADOW, "text/plain");
}

static void sk_shadow_expunge(const struct sk_fts_index *skindex,
			      SKDocumentRef shadow, CFMutableArrayRef targets)
{
	CFDictionaryRef properties;
	CFArrayRef ids;
	CFIndex count, i;

	properties = SKIndexCopyDocumentProperties(skindex->skiref, shadow);
	if (properties == NULL)
		return;

	ids = (CFArrayRef) CFDictionaryGetValue(properties,
						CFSTR(SK_SHADOW_NAME));
	if (ids == NULL) {
		/* may not be shadow properties; ignore */
		CFRelease_and_null(properties);
		return;
	}

	count = CFArrayGetCount(ids);
	for (i = 0; i < count; i++) {
		CFNumberRef num;
		SKDocumentID id;
		SKDocumentRef doc;

		num = CFArrayGetValueAtIndex(ids, i);
		if (!CFNumberGetValue(num, kCFNumberCFIndexType, &id))
			continue;

		doc = SKIndexCopyDocumentForDocumentID(skindex->skiref, id);
		if (doc == NULL)
			continue;

		CFArrayAppendValue(targets, doc);
		CFRelease_and_null(doc);
	}
	CFRelease_and_null(properties);
}

static void sk_expunge_do(struct sk_fts_backend *backend,
			  struct sk_fts_expunge_context *ectx)
{
	enum { SK_MAXDOCS = 5 };
	SKDocumentID results[SK_MAXDOCS];
	CFIndex found = 0, i;

	memset(results, 0, sizeof results);
	ectx->done = !SKSearchFindMatches(ectx->search, SK_MAXDOCS, results,
					  NULL, backend->search_secs, &found);
	for (i = 0; i < found; i++) {
		SKDocumentRef doc;
		CFIndex count;

		doc = SKIndexCopyDocumentForDocumentID(ectx->skindex->skiref,
						       results[i]);
		if (doc == NULL)
			continue;

		/* have (probably) a shadow doc; remove the documents
		   that its properties identify */
		sk_shadow_expunge(ectx->skindex, doc, ectx->targets);

		/* also remove the shadow document itself */
		count = CFArrayGetCount(ectx->targets);
		if (count > 0 && results[i] !=
		    SKIndexGetDocumentID(ectx->skindex->skiref,
			CFArrayGetValueAtIndex(ectx->targets, count - 1)))
			CFArrayAppendValue(ectx->targets, doc);

		CFRelease_and_null(doc);
	}
}

static int sk_expunge_defer(struct sk_fts_backend *backend, uint32_t uid)
{
	CFNumberRef num;

	if (backend->pendingExpunges == NULL) {
		backend->pendingExpunges =
			CFArrayCreateMutable(kCFAllocatorDefault, 0,
					     &kCFTypeArrayCallBacks);
		sk_nonnull(backend->pendingExpunges,
			   "CFArrayCreateMutable(pendingExpunges)");
	}

	num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &uid);
	sk_nonnull(num, "CFNumberCreate(uid)");
	CFArrayAppendValue(backend->pendingExpunges, num);
	CFRelease_and_null(num);

	return 0;
}

static int sk_expunge_commit(struct sk_fts_backend *backend)
{
	struct sk_meta_values *values = NULL;
	int ret = 0;

	if (backend->pendingExpunges == NULL ||
	    CFArrayGetCount(backend->pendingExpunges) == 0)
		return 0;

	if (sk_get_meta_values(backend, &values) < 0)
		return -1;

	if (values->expungedUids != NULL) {
		CFIndex count = CFArrayGetCount(values->expungedUids);
		if (count > 0) {
			CFArrayAppendArray(backend->pendingExpunges,
					   values->expungedUids,
					   CFRangeMake(0, count));
		} else {
			/* this marks the beginning of this set of deferred
			   expunges */
			values->deferred_since = ioloop_time;
		}
		CFRelease_and_null(values->expungedUids);
	} else {
		/* this marks the beginning of this set of deferred
		   expunges */
		values->deferred_since = ioloop_time;
	}
	values->expungedUids = backend->pendingExpunges;
	backend->pendingExpunges = NULL;

	if (sk_set_meta_values(backend, values) < 0)
		ret = -1;
	sk_meta_values_deinit(&values);

	return ret;
}

static void sk_expunge_cancel(struct sk_fts_backend *backend)
{
	if (backend->pendingExpunges != NULL)
		CFRelease_and_null(backend->pendingExpunges);
}

static int sk_compact_batch(struct sk_fts_backend *backend, uint32_t *uids,
			    unsigned int uids_count)
{
	const struct sk_fts_index *skindex;
	unsigned int count, i;
	CFMutableStringRef query;
	ARRAY_DEFINE(expunges, struct sk_fts_expunge_context);
	struct sk_fts_expunge_context *ectx;
	bool more;
	int ret = 0;

	/* first find the shadow documents for the uids */
	query = CFStringCreateMutable(kCFAllocatorDefault, 0);
	sk_nonnull(query, "CFStringCreateMutable(query)");
	for (i = 0; i < uids_count; i++) {
		if (uids[i] == (uint32_t) -1)
			continue;
		CFStringAppendFormat(query, NULL, CFSTR("FTS%uSK "), uids[i]);
	}
	if (CFStringGetLength(query) == 0) {
		CFRelease_and_null(query);
		return 0;
	}
	skindex = array_get(&backend->indexes, &count);
	t_array_init(&expunges, count);
	for (i = 0; i < count; i++) {
		SKSearchRef search;

		search = SKSearchCreate(skindex[i].skiref, query,
					kSKSearchOptionNoRelevanceScores |
					kSKSearchOptionSpaceMeansOR);
		if (search == NULL) {
			char qbuf[1024];
			if (!CFStringGetCString(query, qbuf, sizeof qbuf,
						kCFStringEncodingUTF8))
				strcpy(qbuf, "(unknown)");
			i_error("fts_sk: SKSearchCreate(%s, %s) failed",
				skindex[i].path, qbuf);
			break;
		}

		ectx = array_append_space(&expunges);
		ectx->skindex = &skindex[i];
		ectx->search = search;
		ectx->targets = CFArrayCreateMutable(kCFAllocatorDefault, 0,
						     &kCFTypeArrayCallBacks);
		sk_nonnull(ectx->targets, "CFArrayCreateMutable(targets)");
	}
	CFRelease_and_null(query);
	if (i < count) {
		array_foreach_modifiable(&expunges, ectx) {
			CFRelease_and_null(ectx->search);
			CFRelease_and_null(ectx->targets);
		}
		return -1;
	}

	/* search across all the indexes in parallel */
	ectx = array_get_modifiable(&expunges, &count);
	do {
		more = FALSE;
		for (i = 0; i < count; i++) {
			if (ectx[i].done)
				continue;
			sk_expunge_do(backend, &ectx[i]);
			if (ectx[i].done)
				CFRelease_and_null(ectx[i].search);
			else
				more = TRUE;
		}
	} while (more);

	/* now that the search is done, remove all the targeted documents */
	for (i = 0; i < count; i++) {
		CFMutableArrayRef targets = ectx[i].targets;
		CFIndex tcount = CFArrayGetCount(targets);
		CFIndex t;

		for (t = 0; t < tcount; t++) {
			SKDocumentRef doc = (SKDocumentRef)
				CFArrayGetValueAtIndex(targets, t);
			(void) SKIndexRemoveDocument(skindex[i].skiref, doc);
		}

		CFRelease_and_null(ectx[i].targets);
	}

	return ret;
}

static int sk_compact(struct sk_fts_backend *backend)
{
	struct sk_meta_values *values = NULL;
	CFIndex num_expunged, num_compacted;
	time_t deferred_age;
	const struct sk_fts_index *skindex;
	int ret = 0;

	if (sk_get_meta_values(backend, &values) < 0)
		return -1;
	if (values->expungedUids == NULL) {
		sk_meta_values_deinit(&values);
		return 0;
	}

	/* compact only if there are enough expunges to make it worthwhile,
	   or if the expunges are getting long in the tooth */
	num_expunged = CFArrayGetCount(values->expungedUids);
	deferred_age = ioloop_time - values->deferred_since;
	if (num_expunged == 0 ||
	    (num_expunged < backend->compact_expunges &&
	     deferred_age < backend->compact_age)) {
		sk_meta_values_deinit(&values);
		return 0;
	}

	array_foreach(&backend->indexes, skindex) {
		/* SK requires flush before search. Technically this
		   should flush before each search, but flushing is slow
		   and we may be expunging a large number of messages. */
		if (!SKIndexFlush(skindex->skiref)) {
			i_error("fts_sk: SKIndexFlush(%s) failed",
				skindex->path);
			return -1;
		}
	}

	num_compacted = 0;
	do {
		CFIndex to_compact, u;
		uint32_t uids[SK_COMPACT_BATCH_SIZE];

		to_compact = num_expunged - num_compacted;
		if (to_compact > SK_COMPACT_BATCH_SIZE)
			to_compact = SK_COMPACT_BATCH_SIZE;
		for (u = 0; u < to_compact; u++) {
			CFNumberRef num = (CFNumberRef)
				CFArrayGetValueAtIndex(values->expungedUids,
						       num_compacted + u);
			if (num == NULL ||
			    !CFNumberGetValue(num, kCFNumberSInt32Type,
					      &uids[u]))
				uids[u] = (uint32_t) -1;
		}

		if (sk_compact_batch(backend, uids, to_compact) < 0)
			ret = -1;

		num_compacted += to_compact;
	} while (num_compacted < num_expunged);

	if (ret >= 0) {
		CFRelease_and_null(values->expungedUids);
		ret = sk_set_meta_values(backend, values);
	}
	sk_meta_values_deinit(&values);

	array_foreach(&backend->indexes, skindex) {
		if (!SKIndexCompact(skindex->skiref)) {
			i_error("fts_sk: SKIndexCompact(%s) failed",
				skindex->path);
			ret = -1;
		}
	}

	return ret;
}

static bool sk_is_locked(struct sk_fts_backend *backend, int lock_type)
{
	i_assert(lock_type == F_RDLCK || lock_type == F_WRLCK);

	if (backend->lock_type == F_UNLCK) {
		i_assert(backend->file_lock == NULL &&
			 backend->dotlock == NULL);
		return FALSE;
	}
	i_assert(backend->file_lock != NULL || backend->dotlock != NULL);

	if (lock_type == F_WRLCK && backend->lock_type != F_WRLCK)
		return FALSE;

	return TRUE;
}

static int sk_lock(struct sk_fts_backend *backend, int lock_type)
{
	int ret;
	const char *lock_path;

	i_assert(!sk_is_locked(backend, F_RDLCK));

	if (backend->lock_method != FILE_LOCK_METHOD_DOTLOCK) {
		lock_path = backend->lock_path;
		if (backend->lock_fd == -1) {
			backend->lock_fd = open(lock_path, O_RDWR | O_CREAT,
						0600);
			if (backend->lock_fd == -1) {
				i_error("fts_sk: creat(%s) failed: %m",
					lock_path);
				return -1;
			}
		}

		ret = file_wait_lock_multiclient(backend->lock_fd, lock_path,
						 lock_type,
						 backend->lock_method,
						 backend->lock_timeout_secs,
						 &backend->file_lock,
						 POINTER_CAST_TO(backend, uintmax_t));
	} else {
		lock_path = (array_idx(&backend->indexes, 0))->path;
		ret = file_dotlock_create_multiclient(&backend->dotlock_set,
						      lock_path, 0,
						      &backend->dotlock,
						      POINTER_CAST_TO(backend, uintmax_t));

	}
	if (ret == 0) {
		i_warning("fts_sk: %s: Locking timed out", lock_path);
		return 0;
	}
	if (ret < 0)
		return -1;

	backend->lock_type = lock_type;
	return 1;
}

static void sk_unlock(struct sk_fts_backend *backend)
{
	if (backend->file_lock != NULL)
		file_unlock_multiclient(&backend->file_lock,
					POINTER_CAST_TO(backend, uintmax_t));

	if (backend->dotlock != NULL)
		(void) file_dotlock_delete_multiclient(&backend->dotlock,
						       POINTER_CAST_TO(backend, uintmax_t));

	backend->lock_type = F_UNLCK;
}

static int sk_lock_and_open(struct sk_fts_backend *backend, int lock_type,
			    bool meta_only)
{
	int ret = -1;

	i_assert(lock_type == F_RDLCK || lock_type == F_WRLCK);
	if (lock_type == F_RDLCK) {
		if (sk_lock(backend, F_RDLCK) <= 0)
			return -1;

		ret = sk_open(backend, FALSE, meta_only);
		if (ret <= 0)
			sk_unlock(backend);
		if (ret == 0)
			return -1;
	}
	if (ret < 0) {
		if (sk_lock(backend, F_WRLCK) <= 0)
			return -1;

		ret = sk_open_or_create(backend, meta_only);
	}
	if (ret <= 0) {
		sk_unlock(backend);
		return -1;
	}

	return 1;
}


static struct fts_backend *fts_backend_sk_init(struct mailbox *box)
{
	struct mail_storage *storage;
	const char *path, *env;
	struct mailbox_status status;
	struct sk_fts_backend *backend;
	struct sk_fts_index *skindex;
	CFURLRef meta_url;

	sk_plugin_init();

	storage = mailbox_get_storage(box);
	path = mailbox_list_get_path(box->list, box->name,
				     MAILBOX_LIST_PATH_TYPE_INDEX);
	if (*path == '\0') {
		/* SearchKit supports in-memory indexes; maybe later */
		if (storage->set->mail_debug)
			i_debug("fts_sk: Disabled with in-memory indexes");
		return NULL;
	}

	backend = i_new(struct sk_fts_backend, 1);
	backend->backend = fts_backend_sk;

	backend->index_path = i_strdup(path);
	backend->spill_dir = i_strdup(SK_DEFAULT_SPILL_DIR);
	backend->lock_path = i_strconcat(path, "/"SK_LOCK_FILE_NAME, NULL);
	backend->lock_method = storage->set->parsed_lock_method;
	if (backend->lock_method == FILE_LOCK_METHOD_FCNTL) {
		/* POSIX requires that *all* of a process's fcntl locks on a
		   file are removed when the process closes *any* descriptor
		   for that file.  This breaks multiclient locking, so instead
		   use flock locks.  See fcntl(2). */
		backend->lock_method = FILE_LOCK_METHOD_FLOCK;
	}
	backend->lock_fd = -1;
	backend->dotlock_set.stale_timeout = SK_DOTLOCK_STALE_TIMEOUT_SECS;
	backend->dotlock_set.use_excl_lock = storage->set->dotlock_use_excl;
	backend->dotlock_set.nfs_flush = storage->set->mail_nfs_index;
	backend->lock_type = F_UNLCK;

	mailbox_get_status(box, STATUS_UIDVALIDITY, &status);
	backend->uidvalidity = status.uidvalidity;
	backend->create_mode = box->file_create_mode;
	backend->create_gid = box->file_create_gid;

	backend->default_min_term_length = SK_DEFAULT_MIN_TERM_LENGTH;
	backend->lock_timeout_secs = SK_LOCK_TIMEOUT_SECS;
	backend->search_secs = SK_SEARCH_SECS;
	backend->indexer_timeout_secs = SK_INDEXER_TIMEOUT_SECS;
	backend->compact_age = SK_COMPACT_AGE;
	backend->compact_expunges = SK_COMPACT_EXPUNGES;
	backend->debug = storage->set->mail_debug;

	i_array_init(&backend->indexes, 1);
	skindex = array_append_space(&backend->indexes);
	skindex->path = i_strconcat(path, "/"SK_FILE_NAME, NULL);
	meta_url = CFURLCreateWithString(kCFAllocatorDefault,
					 CFSTR(SK_META_URL), NULL);
	sk_nonnull(meta_url, "CFURLCreateWithString(meta_url)");
	skindex->meta_doc = SKDocumentCreateWithURL(meta_url);
	sk_nonnull(skindex->meta_doc, "SKDocumentCreateWithURL(meta_url)");
	CFRelease_and_null(meta_url);

	env = mail_user_plugin_getenv(box->storage->user, "fts_sk");
	if (env != NULL)
		sk_set_options(backend, env);

	backend->dotlock_set.timeout = backend->lock_timeout_secs;

	return &backend->backend;
}

static void fts_backend_sk_deinit(struct fts_backend *_backend)
{
	struct sk_fts_backend *backend = (struct sk_fts_backend *) _backend;
	struct sk_fts_index *skindex;

	/* just in case */
	sk_close(backend);
	sk_unlock(backend);

	array_foreach_modifiable(&backend->indexes, skindex) {
		i_free(skindex->path);
		if (skindex->meta_doc != NULL)
			CFRelease_and_null(skindex->meta_doc);
	}
	array_free(&backend->indexes);

	if (backend->pendingExpunges != NULL)
		CFRelease_and_null(backend->pendingExpunges);
	if (backend->lock_fd != -1) {
		if (close(backend->lock_fd) < 0)
			i_error("fts_sk: close(%s) failed: %m",
				backend->lock_path);
		backend->lock_fd = -1;
	}
	i_free(backend->lock_path);
	i_free(backend->spill_dir);
	i_free(backend->index_path);
	i_free(backend);
}

static int fts_backend_sk_get_last_uid(struct fts_backend *_backend,
				       uint32_t *last_uid_r)
{
	struct sk_fts_backend *backend = (struct sk_fts_backend *) _backend;
	struct sk_meta_values *values = NULL;
	int ret = 0;

	if (sk_lock_and_open(backend, F_RDLCK, TRUE) < 0)
		return -1;

	if (sk_get_meta_values(backend, &values) < 0)
		ret = -1;
	else {
		*last_uid_r = values->last_uid;
		sk_meta_values_deinit(&values);
	}

	sk_close(backend);
	if (ret < 0)
		sk_delete(backend);
	sk_unlock(backend);

	return ret;
}

static int fts_backend_sk_build_init(struct fts_backend *_backend,
				     uint32_t *last_uid_r,
				     struct fts_backend_build_context **ctx_r)
{
	struct sk_fts_backend *backend = (struct sk_fts_backend *) _backend;
	struct sk_fts_backend_build_context *ctx;
	struct sk_meta_values *values = NULL;

	if (sk_lock_and_open(backend, F_WRLCK, FALSE) < 0)
		return -1;

	if (sk_get_meta_values(backend, &values) < 0) {
		sk_close(backend);
		sk_delete(backend);
		sk_unlock(backend);
		return -1;
	}
	*last_uid_r = values->last_uid;

	ctx = i_new(struct sk_fts_backend_build_context, 1);
	ctx->ctx.backend = _backend;
	ctx->fragment = (unsigned int) -1;
	ctx->values = values;
	ctx->cur_text = buffer_create_dynamic(default_pool, SK_TEXT_BUFFER);
	ctx->cur_content_type = i_strdup("text/plain");
	ctx->cur_shadow_ids = CFArrayCreateMutable(kCFAllocatorDefault, 0,
						   &kCFTypeArrayCallBacks);
	sk_nonnull(ctx->cur_shadow_ids, "CFArrayCreateMutable(shadow_ids)");

	ctx->spill_fd = -1;
	ctx->indexer_pid = -1;
	ctx->from_indexer_fd = -1;

	*ctx_r = &ctx->ctx;
	return 0;
}

static void fts_backend_sk_build_hdr(struct fts_backend_build_context *_ctx,
				     uint32_t uid)
{
	struct sk_fts_backend_build_context *ctx =
		(struct sk_fts_backend_build_context *) _ctx;

	ctx->next_uid = uid;
	ctx->next_where = SK_HEADERS;
	if (ctx->next_content_type != NULL)
		i_free(ctx->next_content_type);
	ctx->next_content_type = i_strdup("text/plain");
}

static bool
fts_backend_sk_build_body_begin(struct fts_backend_build_context *_ctx,
				uint32_t uid, const char *content_type,
				const char *content_disposition ATTR_UNUSED)
{
	struct sk_fts_backend_build_context *ctx =
		(struct sk_fts_backend_build_context *) _ctx;

	if (strncasecmp(content_type, "multipart/", 10) == 0)
		return FALSE;	/* ignore multipart preamble and epilogue */

	ctx->next_uid = uid;
	ctx->next_where = SK_BODY;
	if (ctx->next_content_type != NULL)
		i_free(ctx->next_content_type);
	ctx->next_content_type = i_strdup(content_type);

	return TRUE;	/* we take all other types */
}

static int fts_backend_sk_build_more(struct fts_backend_build_context *_ctx,
				     const unsigned char *data, size_t size)
{
	struct sk_fts_backend_build_context *ctx =
		(struct sk_fts_backend_build_context *) _ctx;
	struct sk_fts_backend *backend =
		(struct sk_fts_backend *) _ctx->backend;

	i_assert(sk_is_locked(backend, F_WRLCK));
	return sk_build_common(ctx, ctx->next_uid, data, size,
			       ctx->next_where, ctx->next_content_type);
}

static int fts_backend_sk_build_deinit(struct fts_backend_build_context *_ctx)
{
	struct sk_fts_backend_build_context *ctx =
		(struct sk_fts_backend_build_context *)_ctx;
	struct sk_fts_backend *backend =
		(struct sk_fts_backend *) _ctx->backend;
	int ret = 0;

	i_assert(sk_is_locked(backend, F_WRLCK));

	/* this deinit routine cannot be continued */
	ctx->indexer_synchronous = TRUE;

	if (!sk_build_cancelling(ctx)) {
		if (sk_build_shadow(ctx) < 0)
			ret = -1;
		if (sk_build_flush(ctx) < 0)
			ret = -1;
	}
	sk_indexer_end(ctx);
	if (sk_open(backend, TRUE, FALSE) <= 0)	/* do not create if gone */
		ret = -1;
	else {
		/* don't do anything here that alters the actual index,
		   so SKIndexFlush() will be fast */
		if (sk_set_meta_values(backend, ctx->values) < 0)
			ret = -1;
		if (sk_flush(backend) < 0) {
			sk_close(backend);
			sk_delete(backend);
			ret = -1;
		}
	}
	sk_meta_values_deinit(&ctx->values);

	sk_close(backend);
	sk_unlock(backend);

	if (ctx->next_content_type != NULL)
		i_free(ctx->next_content_type);
	i_free(ctx->cur_content_type);
	buffer_free(&ctx->cur_text);
	CFRelease_and_null(ctx->cur_shadow_ids);
	if (ctx->spill_fd >= 0) {
		if (close(ctx->spill_fd) < 0)
			i_error("fts_sk: close(%s) failed: %m",
				str_c(ctx->spill_path));
		if (unlink(str_c(ctx->spill_path)) < 0 && errno != ENOENT)
			i_error("fts_sk: unlink(%s) failed: %m",
				str_c(ctx->spill_path));
		ctx->spill_fd = -1;
	}
	if (ctx->spill_path != NULL)
		str_free(&ctx->spill_path);

	i_free(ctx);
	return ret;
}

static void fts_backend_sk_expunge(struct fts_backend *_backend,
				   struct mail *mail)
{
	struct sk_fts_backend *backend = (struct sk_fts_backend *)_backend;

	if (!sk_is_locked(backend, F_WRLCK)) {
		if (sk_lock_and_open(backend, F_WRLCK, FALSE) < 0)
			return;
	}

	if (sk_expunge_defer(backend, mail->uid) < 0) {
		sk_close(backend);
		sk_delete(backend);
		sk_unlock(backend);
	}
}

static void fts_backend_sk_expunge_finish(struct fts_backend *_backend,
					  struct mailbox *box ATTR_UNUSED,
					  bool committed)
{
	struct sk_fts_backend *backend = (struct sk_fts_backend *)_backend;

	if (!sk_is_locked(backend, F_WRLCK)) {
		/* failed to get lock to expunge */
		i_assert((array_idx(&backend->indexes, 0))->skiref == NULL);
		return;
	}

	if (committed) {
		if (sk_expunge_commit(backend) < 0 || sk_flush(backend) < 0) {
			sk_close(backend);
			sk_delete(backend);
		} else
			sk_close(backend);
	} else {
		sk_expunge_cancel(backend);
		sk_close(backend);
	}

	sk_unlock(backend);
}

static void fts_backend_sk_compact(struct fts_backend *_backend)
{
	struct sk_fts_backend *backend = (struct sk_fts_backend *)_backend;
	int ret;

	if (sk_lock_and_open(backend, F_WRLCK, FALSE) < 0)
		return;
	ret = sk_compact(backend);
	sk_close(backend);
	if (ret < 0)
		sk_delete(backend);
	sk_unlock(backend);
}

static int fts_backend_sk_lock(struct fts_backend *_backend)
{
	struct sk_fts_backend *backend = (struct sk_fts_backend *)_backend;

	return sk_lock_and_open(backend, F_RDLCK, FALSE);
}

static void fts_backend_sk_unlock(struct fts_backend *_backend)
{
	struct sk_fts_backend *backend = (struct sk_fts_backend *)_backend;

	i_assert(sk_is_locked(backend, F_RDLCK));
	sk_close(backend);
	sk_unlock(backend);
}

static int fts_backend_sk_lookup_one(struct fts_backend *_backend,
				     const char *key,
				     enum fts_lookup_flags flags,
				     ARRAY_TYPE(seq_range) *definite_uids,
				     ARRAY_TYPE(seq_range) *maybe_uids)
{
	struct sk_fts_backend *backend = (struct sk_fts_backend *) _backend;
	CFMutableStringRef query;
	ARRAY_TYPE(seq_range) *uids = definite_uids;
	int ret;

	i_assert((flags & FTS_LOOKUP_FLAG_INVERT) == 0);
	i_assert(sk_is_locked(backend, F_RDLCK));

	query = CFStringCreateMutable(kCFAllocatorDefault, 0);
	sk_nonnull(query, "CFStringCreateMutable(query)");

	if (!sk_build_query(query, key))
		uids = maybe_uids;

	array_clear(definite_uids);
	array_clear(maybe_uids);

	ret = sk_search(backend, query, flags, uids, NULL);
	CFRelease_and_null(query);
	return ret;
}

static int fts_backend_sk_lookup_all(struct fts_backend_lookup_context *ctx,
				     ARRAY_TYPE(seq_range) *definite_uids,
				     ARRAY_TYPE(seq_range) *maybe_uids,
				     ARRAY_TYPE(fts_score_map) *score_map)
{
	struct sk_fts_backend *backend =
		(struct sk_fts_backend *) ctx->backend;
	unsigned int i, count;
	const struct fts_backend_lookup_field *fields;
	enum fts_lookup_flags flags;
	CFMutableStringRef query;
	ARRAY_TYPE(seq_range) *uids = definite_uids;
	int ret;

	i_assert(sk_is_locked(backend, F_RDLCK));

	fields = array_get(&ctx->fields, &count);

	/* SearchKit does not support naked "NOT foo".  It needs a
	   non-inverted search term from which to exclude the NOT term(s)). */
	for (i = 0; i < count; i++)
		if ((fields[i].flags & FTS_LOOKUP_FLAG_INVERT) == 0)
			break;
	if (i >= count)
		return -1;

	flags = fields[0].flags & (FTS_LOOKUP_FLAG_HEADER |
				   FTS_LOOKUP_FLAG_BODY);
	i_assert(flags != 0);

	query = CFStringCreateMutable(kCFAllocatorDefault, 0);
	sk_nonnull(query, "CFStringCreateMutable(query)");

	for (i = 0; i < count; i++) {
		if (i > 0) {
			/* SearchKit can't search by document name, which is
			   where our hint about header vs. body lives (see
			   <rdar://problem/7793170>).  Make FTS fall back to
			   lookup_one. */
			if ((fields[i].flags & (FTS_LOOKUP_FLAG_HEADER |
						FTS_LOOKUP_FLAG_BODY)) !=
			    flags) {
				CFRelease_and_null(query);
				return -1;
			}

			CFStringAppend(query, CFSTR(" "));
		}

		if (fields[i].flags & FTS_LOOKUP_FLAG_INVERT)
			CFStringAppend(query, CFSTR("NOT "));

		if (!sk_build_query(query, fields[i].key)) {
			/* SearchKit can't do subphrase matches */
			uids = maybe_uids;
		}
	}

	array_clear(definite_uids);
	array_clear(maybe_uids);

	ret = sk_search(backend, query, flags, uids, score_map);
	CFRelease_and_null(query);
	return ret;
}

struct fts_backend fts_backend_sk = {
	.name = "sk",
	.flags = FTS_BACKEND_FLAG_SUBSTRING_LOOKUPS |
		 FTS_BACKEND_FLAG_BINARY_MIME_PARTS,

	{
		fts_backend_sk_init,
		fts_backend_sk_deinit,
		fts_backend_sk_get_last_uid,
		NULL,
		fts_backend_sk_build_init,
		fts_backend_sk_build_hdr,
		fts_backend_sk_build_body_begin,
		NULL,
		fts_backend_sk_build_more,
		fts_backend_sk_build_deinit,
		fts_backend_sk_expunge,
		fts_backend_sk_expunge_finish,
		fts_backend_sk_compact,
		fts_backend_sk_lock,
		fts_backend_sk_unlock,
		fts_backend_sk_lookup_one,
		NULL,
		fts_backend_sk_lookup_all
	}
};
