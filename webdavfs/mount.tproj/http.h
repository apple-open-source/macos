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
 *	$Id: http.h,v 1.6 2003/09/08 18:31:15 lutherj Exp $
 */

#ifndef http_h
#define http_h	1

#include "fetch.h"

extern int http_parse(struct fetch_state *fs, const char *key, int use_proxy);
extern int get(struct fetch_state *volatile fs, int * download_status);
extern int make_request(struct fetch_state *volatile fs,
	int (*function)(struct fetch_state *fs, void *arg), void *arg,int do_close);
extern int http_lookup(struct fetch_state *fs, void *arg);

#ifdef NOT_NEEDED
extern int http_opendir(struct fetch_state *fs, void * arg);
#endif

extern int http_stat(struct fetch_state *fs, void * arg);
extern int http_statfs(struct fetch_state *fs, void * arg);
extern int http_mount(struct fetch_state *fs, void * arg);
extern int http_put(struct fetch_state *fs, void * arg);
extern int http_getlastmodified(struct fetch_state *fs, void *arg);
extern int http_delete(struct fetch_state *fs, void * arg);
extern int http_lock (struct fetch_state *fs, void *arg);
extern int http_unlock (struct fetch_state *fs, void *arg);
extern int http_refreshdir(struct fetch_state *fs, void * arg);
extern int http_delete_dir (struct fetch_state *fs, void * arg);
extern int http_mkcol(struct fetch_state *fs, void * arg);
extern int http_move(struct fetch_state *fs, void * arg);
extern int http_read(int *remote, int local, off_t total_length, int * download_status);
extern int http_read_chunked(int *remote, int local, off_t total_length, int * download_status,
	int *last_chunk);
extern int http_read_bytes (struct fetch_state *fs, void * arg);

struct http_state
{
	char *http_remote_request;
	char *http_decoded_file;
	char *http_host_header;
	int http_redirected;
	int connection_close;
};

#endif /* ! http_h */



