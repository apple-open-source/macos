/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <db.h>
#include "recno.h"

/*
 * __REC_SEQ -- Recno sequential scan interface.
 *
 * Parameters:
 *	dbp:	pointer to access method
 *	key:	key for positioning and return value
 *	data:	data return value
 *	flags:	R_CURSOR, R_FIRST, R_LAST, R_NEXT, R_PREV.
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS or RET_SPECIAL if there's no next key.
 */
int
__rec_seq(dbp, key, data, flags)
	const DB *dbp;
	DBT *key, *data;
	u_int flags;
{
	BTREE *t;
	EPG *e;
	recno_t nrec;
	int status;

	t = dbp->internal;

	/* Toss any page pinned across calls. */
	if (t->bt_pinned != NULL) {
		mpool_put(t->bt_mp, t->bt_pinned, 0);
		t->bt_pinned = NULL;
	}

	switch(flags) {
	case R_CURSOR:
		if ((nrec = *(recno_t *)key->data) == 0)
			goto einval;
		break;
	case R_NEXT:
		if (ISSET(t, B_SEQINIT)) {
			nrec = t->bt_rcursor + 1;
			break;
		}
		/* FALLTHROUGH */
	case R_FIRST:
		nrec = 1;
		break;
	case R_PREV:
		if (ISSET(t, B_SEQINIT)) {
			if ((nrec = t->bt_rcursor - 1) == 0)
				return (RET_SPECIAL);
			break;
		}
		/* FALLTHROUGH */
	case R_LAST:
		if (!ISSET(t, R_EOF | R_INMEM) &&
		    t->bt_irec(t, MAX_REC_NUMBER) == RET_ERROR)
			return (RET_ERROR);
		nrec = t->bt_nrecs;
		break;
	default:
einval:		errno = EINVAL;
		return (RET_ERROR);
	}
	
	if (t->bt_nrecs == 0 || nrec > t->bt_nrecs) {
		if (!ISSET(t, R_EOF | R_INMEM) &&
		    (status = t->bt_irec(t, nrec)) != RET_SUCCESS)
			return (status);
		if (t->bt_nrecs == 0 || nrec > t->bt_nrecs)
			return (RET_SPECIAL);
	}

	if ((e = __rec_search(t, nrec - 1, SEARCH)) == NULL)
		return (RET_ERROR);

	SET(t, B_SEQINIT);
	t->bt_rcursor = nrec;

	status = __rec_ret(t, e, nrec, key, data);
	if (ISSET(t, B_DB_LOCK))
		mpool_put(t->bt_mp, e->page, 0);
	else
		t->bt_pinned = e->page;
	return (status);
}
