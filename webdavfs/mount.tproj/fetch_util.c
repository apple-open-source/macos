/*-
 * Copyright (c) 1996
 *		Jean-Marc Zucconi
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 *	OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* $Id: fetch_util.c,v 1.4 2003/02/06 20:22:31 lutherj Exp $ */

#include <sys/types.h>
#include <sys/syslog.h>

#include <stdio.h>
#include "fetch.h"
#include "webdavd.h"

/*****************************************************************************/

int get(struct fetch_state *volatile fs, int * download_status);

/*****************************************************************************/

/* XXX This function could probably be moved to http.c or removed completely */
int get(struct fetch_state *volatile fs, int *download_status)
{
	volatile int error;
	
	error = fs->fs_retrieve(fs, download_status);
	if ( !error )
	{
		fs->fs_close(fs);
	}
	
	return error;
}

/*****************************************************************************/

/* XXX This function could probably be moved to http.c or removed completely */
int make_request(struct fetch_state *volatile fs,
	int( *function)(struct fetch_state *fs, void *arg), void *arg, int do_close)
{
	volatile int error;
	
	error = function(fs, arg);
	
	if (do_close == WEBDAV_FS_CLOSE)
	{
		fs->fs_close(fs);
	}
	
	return error;
}

/*****************************************************************************/

