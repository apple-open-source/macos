/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 * Portions Copyright (c) 1998-2001 PADL Software Pty Ltd. All rights reserved.
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
#include <rpc/rpc.h>
#include <pwd.h>
#include <rpcsvc/yppasswd.h>

bool_t
xdr_passwd(xdrs, objp)
	XDR *xdrs;
	struct passwd *objp;
{
	if (!xdr_string(xdrs, &objp->pw_name, ~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->pw_passwd, ~0)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->pw_uid)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->pw_gid)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->pw_gecos, ~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->pw_dir, ~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->pw_shell, ~0)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_yppasswd(xdrs, objp)
	XDR *xdrs;
	struct yppasswd *objp;
{
	if (!xdr_string(xdrs, &objp->oldpass, ~0)) {
		return (FALSE);
	}
	if (!xdr_passwd(xdrs, &objp->newpw)) {
		return (FALSE);
	}
	return (TRUE);
}
