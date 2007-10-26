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
/*	$OpenBSD: yppush_xdr.c,v 1.2 1996/05/30 09:53:22 deraadt Exp $ */

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
#ifndef LINT
__unused static char rcsid[] = "$OpenBSD: yppush_xdr.c,v 1.2 1996/05/30 09:53:22 deraadt Exp $";
#endif

#include <rpc/rpc.h>
#include <rpcsvc/yp.h>

__private_extern__
bool_t
xdr_domainname(xdrs, objp)
	XDR *xdrs;
	domainname *objp;
{
	if (!xdr_string(xdrs, objp, YPMAXDOMAIN)) {
		return (FALSE);
	}
	return (TRUE);
}

__private_extern__
bool_t
xdr_mapname(xdrs, objp)
	XDR *xdrs;
	mapname *objp;
{
	if (!xdr_string(xdrs, objp, YPMAXMAP)) {
		return (FALSE);
	}
	return (TRUE);
}

__private_extern__
bool_t
xdr_peername(xdrs, objp)
	XDR *xdrs;
	peername *objp;
{
	if (!xdr_string(xdrs, objp, YPMAXPEER)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypmap_parms(xdrs, objp)
	XDR *xdrs;
	ypmap_parms *objp;
{
	if (!xdr_domainname(xdrs, &objp->domain)) {
		return (FALSE);
	}
	if (!xdr_mapname(xdrs, &objp->map)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->ordernum)) {
		return (FALSE);
	}
	if (!xdr_peername(xdrs, &objp->peer)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypreq_xfr(xdrs, objp)
	XDR *xdrs;
	ypreq_xfr *objp;
{
	if (!xdr_ypmap_parms(xdrs, &objp->map_parms)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->transid)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->prog)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->port)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_yppush_status(xdrs, objp)
	XDR *xdrs;
	yppush_status *objp;
{
	 if (!xdr_enum(xdrs, (enum_t *)objp)) {
		 return (FALSE);
	 }
	return (TRUE);
}

bool_t
xdr_yppushresp_xfr(xdrs, objp)
	XDR *xdrs;
	yppushresp_xfr *objp;
{

	 if (!xdr_u_int(xdrs, &objp->transid)) {
		 return (FALSE);
	 }
	 if (!xdr_yppush_status(xdrs, &objp->status)) {
		 return (FALSE);
	 }
	return (TRUE);
}



