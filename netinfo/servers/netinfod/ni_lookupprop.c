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

/*
 * ni_lookupprop() implementation
 * Copyright (C) 1989 by NeXT, Inc.
 *
 * This function is a copy of the one in the NetInfo client library. 
 * It is duplicated here since we do not wish to pull in any code from
 * the client library.
 */
#include "ni_server.h"
#include <stdio.h>
#include <string.h>
#include <NetInfo/mm.h>

/*
 * We can do this without an addition to the protocol
 */
ni_status
ni_lookupprop(
	      void *ni,
	      ni_id *id,
	      ni_name_const pname,
	      ni_namelist *nl
	      )
{
	ni_status status;
	ni_namelist list;
	ni_index which;
	
	NI_INIT(&list);
	status = ni_listprops(ni, id, &list);
	if (status != NI_OK) {
		return (status);
	}
	which = ni_namelist_match(list, pname);
	ni_namelist_free(&list);
	if (which == NI_INDEX_NULL) {
		return (NI_NOPROP);
	}
	return (ni_readprop(ni, id, which, nl));
}

