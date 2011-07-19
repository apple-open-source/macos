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

/* Helper process to index mail message parts safely.  See the fts-sk plugin.
   Essentially just a sandbox around these two SearchKit calls:
	SKLoadDefaultExtractorPlugIns()
	SKIndexAddDocument()
   TO DO:  should run chrooted */

#include "lib.h"
#include "istream.h"
#include "ostream.h"
#include "ioloop.h"
#include "failures.h"
#include "fd-set-nonblock.h"
#include "fd-close-on-exec.h"

#include <unistd.h>

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

#define	INACTIVITY_TIMEOUT_MSECS	(3 * 60 * 1000)

#define	CFRelease_and_null(x)		STMT_START { \
						CFRelease(x); \
						(x) = NULL; \
					} STMT_END

static struct ioloop *ioloop;

struct indexer_settings {
	const char *directory;
	const char *index_path;
	const char *index_name;
};

struct indexer {
	const struct indexer_settings *set;
	struct istream *input;
	struct ostream *output;
	struct io *io_in;
	struct timeout *timeout;

	SKIndexRef skiref;

	unsigned int shook:1;
};

static void indexer_open(struct indexer *indexer)
{
	const char *index_path;
	CFURLRef index_url;
	CFStringRef index_name;

	if (indexer->skiref != NULL)
		return;

	index_path = indexer->set->index_path;
	if (*index_path != '/')
		index_path = t_strdup_printf("%s/%s", indexer->set->directory,
					     indexer->set->index_path);
	index_url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
		(const UInt8 *) index_path, strlen(index_path), FALSE);
	if (index_url == NULL)
		i_fatal("CFURLCreateFromFileSystemRepresentation(%s) failed",
			index_path);

	index_name = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault,
						     indexer->set->index_name,
						     kCFStringEncodingUTF8,
						     kCFAllocatorNull);
	if (index_name == NULL)
		i_fatal("CFStringCreateWithCStringNoCopy(%s) failed",
			indexer->set->index_name);

	indexer->skiref = SKIndexOpenWithURL(index_url, index_name, TRUE);
	CFRelease_and_null(index_name);
	CFRelease_and_null(index_url);
	if (indexer->skiref == NULL)
		i_fatal("SKIndexOpenWithURL(%s, %s) failed",
			index_path, indexer->set->index_name);
}

static void indexer_close(struct indexer *indexer)
{
	if (indexer->skiref != NULL) {
		SKIndexClose(indexer->skiref);
		indexer->skiref = NULL;
	}
}

static const char *indexer_doc_path(struct indexer *indexer, const char *file)
{
	size_t dir_len;

	dir_len = strlen(indexer->set->directory);
	if (strncmp(file, indexer->set->directory, dir_len) == 0 &&
	    file[dir_len] == '/')
		file += dir_len + 1;
	if (strchr(file, '/') != NULL)
		i_fatal("invalid file name input: %s", file);

	return t_strdup_printf("%s/%s", indexer->set->directory, file);
}

static SKDocumentRef create_doc(const char *doc_path)
{
	CFURLRef doc_url;
	SKDocumentRef doc;

	doc_url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
		(const UInt8 *) doc_path, strlen(doc_path), FALSE);
	if (doc_url == NULL)
		i_fatal("CFURLCreateFromFileSystemRepresentation(%s) failed",
			doc_path);

	doc = SKDocumentCreateWithURL(doc_url);
	CFRelease_and_null(doc_url);
	if (doc == NULL)
		i_fatal("SKDocumentCreateWithURL(%s) failed", doc_path);

	return doc;
}

static const char *indexer_index(struct indexer *indexer, const char *tag,
				 const char *file, const char *type)
{
	const char *doc_path, *reply;
	SKDocumentRef doc;
	CFStringRef type_str;

	doc_path = indexer_doc_path(indexer, file);
	doc = create_doc(doc_path);

	type_str = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault,
						   type, kCFStringEncodingUTF8,
						   kCFAllocatorNull);
	if (type_str == NULL) {
		CFRelease_and_null(doc);
		return t_strdup_printf(
			"CFStringCreateWithCStringNoCopy(%s) failed",
			type);
	}

	indexer_open(indexer);

	if (SKIndexAddDocument(indexer->skiref, doc, type_str, TRUE)) {
		SKDocumentID id = SKIndexGetDocumentID(indexer->skiref, doc);
		reply = t_strdup_printf("%s\tOK\t%ld", tag, id);
	} else {
		i_error("SKIndexAddDocument(%s) failed", doc_path);
		reply = t_strdup_printf("%s\tFAIL", tag);
	}

	CFRelease_and_null(type_str);
	CFRelease_and_null(doc);

	return reply;
}

static const char *indexer_shadow(struct indexer *indexer, const char *tag,
				  const char *file, const char * const *ids)
{
	CFMutableDictionaryRef properties;
	SKDocumentID id;
	CFNumberRef num;
	CFMutableArrayRef shadow_ids;
	const char *doc_path;
	SKDocumentRef doc;

	properties = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
					       &kCFTypeDictionaryKeyCallBacks,
					       &kCFTypeDictionaryValueCallBacks);
	if (properties == NULL)
		i_fatal("CFDictionaryCreateMutable(properties) failed");

	shadow_ids = CFArrayCreateMutable(kCFAllocatorDefault, 0,
					  &kCFTypeArrayCallBacks);
	if (shadow_ids == NULL)
		i_fatal("CFArrayCreateMutable(shadow_ids) failed");

	while (*ids != NULL) {
		id = strtol(*ids, NULL, 10);
		num = CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType,
				     &id);
		if (num == NULL)
			i_fatal("CFNumberCreate(id) failed");

		CFArrayAppendValue(shadow_ids, num);
		CFRelease_and_null(num);

		ids++;
	}

	CFDictionaryAddValue(properties, CFSTR("shadow"), shadow_ids);
	CFRelease_and_null(shadow_ids);

	doc_path = indexer_doc_path(indexer, file);
	doc = create_doc(doc_path);
	indexer_open(indexer);

	SKIndexSetDocumentProperties(indexer->skiref, doc, properties);

	CFRelease_and_null(doc);
	CFRelease_and_null(properties);

	return t_strdup_printf("%s\tOK\tdone", tag);
}

static const char *indexer_handle_version(struct indexer *indexer,
					  const char * const *args)
{
	/* version <version> */
	if (indexer->shook)
		i_fatal("duplicate handshake");
	indexer->shook = TRUE;
	if (strcmp(args[1], PACKAGE_VERSION) == 0)
		return "version\t"PACKAGE_VERSION;
	i_fatal("indexer version mismatch: "
		"indexer is v"PACKAGE_VERSION", parent is %s",
		args[1]);
}

static const char *indexer_handle_index(struct indexer *indexer,
					const char * const *args)
{
	const char *tag, *file, *type, *reply;

	/* <tag> index <file> <type> */
	if (args[2] == NULL || args[3] == NULL)
		i_fatal("bad arguments to index command");

	tag = args[0];
	file = args[2];
	type = args[3];

	sandbox_note(t_strdup_printf("indexing \"%s\" type \"%s\"",
				     file, type));
	reply = indexer_index(indexer, tag, file, type);
	sandbox_note("");

	return reply;
}

static const char *indexer_handle_shadow(struct indexer *indexer,
					 const char * const *args)
{
	const char *tag, *file, *reply;
	const char * const *ids;

	/* <tag> shadow <file> ids=<num>,<num>,... */
	if (args[2] == NULL || args[3] == NULL)
		i_fatal("bad arguments to shadow command");

	tag = args[0];
	file = args[2];
	if (strncmp(args[3], "ids=", 4) != 0)
		i_fatal("bad ids argument to shadow command: %s", args[3]);
	ids = t_strsplit(&args[3][4], ",");

	sandbox_note(t_strdup_printf("shadowing \"%s\"", file));
	reply = indexer_shadow(indexer, tag, file, ids);
	sandbox_note("");

	return reply;
}

static const char *indexer_handle_line(struct indexer *indexer,
				       const char *line)
{
	const char * const *list;

	list = t_strsplit(line, "\t");
	if (list[0] == NULL || list[1] == NULL)
		i_fatal("bad indexer command line: %s", line);

	if (strcmp(list[0], "version") == 0)
		return indexer_handle_version(indexer, list);
	if (!indexer->shook)
		i_fatal("parent failed to identify itself");

	/* <tag> <command> [args...] */
	if (strcmp(list[1], "index") == 0)
		return indexer_handle_index(indexer, list);
	if (strcmp(list[1], "shadow") == 0)
		return indexer_handle_shadow(indexer, list);

	i_fatal("bad indexer command line: %s", line);
}

static void indexer_input(struct indexer *indexer)
{
	int ret;
	struct const_iovec iov[2];

	iov[1].iov_base = "\n";
	iov[1].iov_len = 1;
	while ((ret = i_stream_read(indexer->input)) > 0) {
		const char *line;

		while ((line = i_stream_next_line(indexer->input)) != NULL) {
			iov[0].iov_base = indexer_handle_line(indexer, line);
			iov[0].iov_len = strlen(iov[0].iov_base);
			if (o_stream_sendv(indexer->output, iov, 2) !=
			    (ssize_t) (iov[0].iov_len + iov[1].iov_len)) {
				io_loop_stop(ioloop);
				return;
			}
		}
	}
	if (ret == 0)
		timeout_reset(indexer->timeout);
	else if (ret == -2)
		i_fatal("input buffer overflow");
	else
		io_loop_stop(ioloop);
}

static void indexer_timeout(struct indexer *indexer ATTR_UNUSED)
{
	io_loop_stop(ioloop);
}

static struct indexer *indexer_create(int in_fd, int out_fd,
				      const struct indexer_settings *set)
{
	struct indexer *indexer;

	indexer = i_new(struct indexer, 1);
	indexer->set = set;
	fd_set_nonblock(in_fd, TRUE);
	indexer->input = i_stream_create_fd(in_fd, 8192, TRUE);
	indexer->output = o_stream_create_fd(out_fd, 8192, TRUE);
	indexer->io_in = io_add(in_fd, IO_READ, indexer_input, indexer);
	indexer->timeout = timeout_add(INACTIVITY_TIMEOUT_MSECS,
				       indexer_timeout, indexer);

	return indexer;
}

static void indexer_destroy(struct indexer **_indexer)
{
	struct indexer *indexer = *_indexer;

	*_indexer = NULL;

	indexer_close(indexer);
	timeout_remove(&indexer->timeout);
	if (indexer->io_in != NULL)
		io_remove(&indexer->io_in);
	o_stream_destroy(&indexer->output);
	i_stream_destroy(&indexer->input);
	i_free(indexer);
}

static void indexer_announce(struct indexer *indexer)
{
	o_stream_cork(indexer->output);
	if (o_stream_send_str(indexer->output,
			      "version\t"PACKAGE_VERSION"\n") <= 0)
		i_fatal("output error: %m");
	o_stream_uncork(indexer->output);
}

static void indexer_init(int argc, char **argv, struct indexer_settings *set)
{
	if (argc != 4)
		i_fatal("Usage: %s directory index-file index-name", argv[0]);

	memset(set, 0, sizeof *set);
	set->directory = argv[1];
	set->index_path = argv[2];
	set->index_name = argv[3];

	if (*set->directory != '/')
		i_fatal("directory must be absolute path: %s", set->directory);

	if (seteuid(0) == 0 || setuid(0) == 0 ||
	    getuid() == 0 || geteuid() == 0)
		i_fatal("Running as user root isn't permitted");

	/* <rdar://problem/7819581> */
	_CFPreferencesAlwaysUseVolatileUserDomains();

	sandbox_note("loading metadata importers");
	SKLoadDefaultExtractorPlugIns();
	sandbox_note("initializing");

	if (chdir(set->directory) < 0)
		i_fatal("chdir(%s) failed: %m", set->directory);
}

int main(int argc, char **argv)
{
	struct indexer_settings settings;
	struct indexer *indexer;

	sandbox_note("initializing");

#ifdef DEBUG
	i_set_failure_prefix("DEBUG: ");
/* XXX HERE APPLE - <rdar://problem/7968043> */
	fd_debug_verify_leaks(4, getdtablesize());
#endif

	lib_init();
	i_set_failure_prefix(t_strconcat(argv[0], ": ", NULL));
	indexer_init(argc, argv, &settings);

	ioloop = io_loop_create();
	indexer = indexer_create(0, 1, &settings);

	/* signal that all the indexers are loaded and we're rearin' to go */
	indexer_announce(indexer);

	io_loop_run(ioloop);

	sandbox_note("deinitializing");

	indexer_destroy(&indexer);
	io_loop_destroy(&ioloop);

	lib_deinit();
	return 0;
}
