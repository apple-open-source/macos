/*
 * Copyright 1997 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.	 M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.	 It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: fetch.h,v 1.7 2003/04/29 21:02:54 lutherj Exp $
 */

#ifndef fetch_h
#define fetch_h 1

#include "time.h"


/* BUFFER_SIZE is the amount of data to copy per I/O operation when
 * downloading/uploading a file from/to the server. It should be 
 * 4K so that non-chunked I/O operations are page aligned to the
 * cache file, but shouldn't be larger since there are loops in
 * webdav_read and webdav_pagein that may be waiting for pages to be
 * written into the cache file. This small size shouldn't make much
 * difference when reading the cache file to PUT the data to the server
 * because the file system will notice sequential reads and read ahead
 * from disk.
 */
#define BUFFER_SIZE (4096)		/* 4K */

/* BODY_BUFFER_SIZE is the initial size of the buffer used to read an
 * HTTP entity body. The largest bodies are typically the XML data
 * returned by the PROPFIND method for a large collection (directory).
 * 64K is large enough to handle directories with 100-150 items.
 */
#define BODY_BUFFER_SIZE 0x10000	/* 64K */

#define UTF8_TO_ASCII_MAX_SCALE 3

/* IMPORTANT: The user-agent header MUST start with the
 * product token "WebDAVFS" because there are WebDAV servers
 * that special case this client.
 */
#define USER_AGENT_HEADER_PREFIX "User-Agent: WebDAVFS/"

struct fetch_state {
	const char *fs_status;
	char *fs_outputfile;	/* the name of the temp file; data is on the stack */
	uid_t fs_uid;
	int fs_fd;				/* open file descriptor to cache file */
	time_t fs_st_mtime;		/* time of last cache */
	int *fs_socketptr;
	int fs_verbose;			/* -q, -v option */
	int fs_mirror;			/* -m option */
	int fs_restart;			/* -r option */
	int fs_auto_retry;		/* -a option */
	int fs_use_connect;		/* fs_use_connect works around broken server TCPs 
								that cannot handle receiving our initial request
								as part of the SYN packet.  
								It's currently on (1) by default
							*/
	void *fs_proto;
	int (*fs_retrieve)(struct fetch_state *, int *);
	int (*fs_close)(struct fetch_state *);
};

int get(struct fetch_state *volatile fs, int *download_status);
int make_request(struct fetch_state *volatile fs,
	int( *function)(struct fetch_state *fs, void *arg), void *arg, int do_close);
char *percent_decode(const char *orig);
void percent_decode_in_place(char *uri);
char *utf8_encode(const unsigned char *orig);
char *to_base64(const unsigned char *buf, size_t len);
int from_base64(const char *base64str, unsigned char *outBuffer, size_t *lengthptr);
int reconstruct_url(const char *hostheader, const char *remotefile, char **url);
ssize_t socket_read_bytes(int fd, char *buf, size_t n);
ssize_t socket_read_line(int fd, char *buf, size_t n);
void zero_trailing_spaces(char *line);
char * SkipQuotedString(char *bytes);
char * SkipCodedURL(char *bytes);
char * SkipToken(char *bytes);
char * SkipLWS(char *bytes);

#endif /* ! fetch_h */
