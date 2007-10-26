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
/*	$OpenBSD: yppush_err.c,v 1.2 1996/05/30 09:53:20 deraadt Exp $ */

/*
 * Copyright (c) 1996 Mats O Jansson <moj@stacken.kth.se>
 * All rights reserved.
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
 *	This product includes software developed by Mats O Jansson
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__unused static char rcsid[] = "$OpenBSD: yppush_err.c,v 1.2 1996/05/30 09:53:20 deraadt Exp $";
#endif /* not lint */

#include <rpc/rpc.h>
#include <rpcsvc/yp.h>

char *
yppush_err_string(enum yppush_status y) {
	switch(y) {
	case YPPUSH_SUCC:
			return "Success";
	case YPPUSH_AGE:
			return "Master's version not newer";
	case YPPUSH_NOMAP:
			return "Can't find server for map";
	case YPPUSH_NODOM:
			return "Domain not supported";
	case YPPUSH_RSRC:
			return "Local resource alloc failure";
	case YPPUSH_RPC:
			return "RPC failure talking to server";
	case YPPUSH_MADDR:
			return "Can't get master address";
	case YPPUSH_YPERR:
			return "YP server/map db error";
	case YPPUSH_BADARGS:
			return "Request arguments bad";
	case YPPUSH_DBM:
			return "Local dbm operation failed";
	case YPPUSH_FILE:
			return "Local file I/O operation failed";
	case YPPUSH_SKEW:
			return "Map version skew during transfer";
	case YPPUSH_CLEAR:
			return "Can't send \"Clear\" req to local ypserv";
	case YPPUSH_FORCE:
			return "No local order number in map  use -f flag.";
	case YPPUSH_XFRERR:
			return "ypxfr error";
	case YPPUSH_REFUSED:
			return "Transfer request refused by ypserv";
        }
	/* NOTREACHED */
	return "Unknown error";
};
