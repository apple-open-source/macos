/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * NetInfo error status -> string conversion.
 * Copyright (C) 1989 by NeXT, Inc.
 */
#include <netinfo/ni.h>

static const struct {
	ni_status status;
	char *message;
} ni_errmsgs[] = {
	{ NI_OK,		"Operation succeeded" }, 
	{ NI_BADID,	"ID is invalid" }, 
	{ NI_STALE,	"Write attempted on stale version of object" }, 
	{ NI_NOSPACE,	"No space available for write operation" }, 
	{ NI_PERM,	"Permission denied" }, 
	{ NI_NODIR,	"No such directory" }, 
	{ NI_NOPROP,	"No such property" }, 
	{ NI_NONAME,	"No such name" }, 
	{ NI_NOTEMPTY,	"Cannot delete name object with children" }, 
	{ NI_UNRELATED,	"Object is not child of parent: cannot destroy" }, 
	{ NI_SERIAL,	"Serialization error" }, 
	{ NI_NETROOT,	"Hit network root domain" }, 
	{ NI_NORESPONSE,	"No response from remote parent" }, 
	{ NI_RDONLY,	"No writes allowed: all objects are read-only" }, 
	{ NI_SYSTEMERR,	"Remote system error" },
	{ NI_ALIVE,	"Can't regenerate: already in use" }, 
	{ NI_NOTMASTER,	"Operation makes no sense on clone" }, 
	{ NI_CANTFINDADDRESS, "Can't find address of server" }, 
	{ NI_DUPTAG,	"Duplicate domain tag: can't serve it" }, 
	{ NI_NOTAG,	"No such tag" }, 
	{ NI_AUTHERROR, "Authentication error" },
	{ NI_NOUSER,	"No such user" },
	{ NI_MASTERBUSY,	"Master server is busy" },
	{ NI_INVALIDDOMAIN,	"Invalid domain" },
	{ NI_BADOP,	"Invalid operation on master" },
	{ NI_FAILED,	"Communication failure" }
};

#define NI_ERRMSGSZ (sizeof(ni_errmsgs)/sizeof(ni_errmsgs[0]))

const char *
ni_error(
	 ni_status status
	 )
{
	int i;
	
	for (i = 0; i < NI_ERRMSGSZ; i++) {
		if (ni_errmsgs[i].status == status) {
			return (ni_errmsgs[i].message);
		}
	}
	return ("(unknown error)");
}
