/*
 * Darwin check share access
 *
 * Copyright (c) 2007 Apple Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* This header has to be here due to preprocessor conflicts with Samba
 * headers.
 */
#include "includes.h"
#include <sys/attr.h>

/*
 * Given a path to a share check the users access. The
 * user needs to be able to read or write to the share point.
 *
 * Note: This routine doesn't handle the "force user" option.
 */
BOOL check_share_access(const char *path)
{
	struct attrlist attrlist;
	u_int32_t attrbuf[2];	/* Length field and access modes */

	ZERO_STRUCT(attrlist);
	ZERO_STRUCT(attrbuf);
	attrlist.bitmapcount = ATTR_BIT_MAP_COUNT;
	attrlist.commonattr = ATTR_CMN_USERACCESS;
	/* Call getattrlist to get the real volume name */
	if (getattrlist(path, &attrlist, attrbuf, sizeof(attrbuf), 0) != 0) {
		if (errno == EACCES) {
		    DEBUG(5, ("getattrlist for %s failed: %s disallowing access!\n", path, strerror(errno)));
		    return False;
		}
		DEBUG(10, ("getattrlist for %s failed: %s allowing access!\n", path, strerror(errno)));
		return True;
	}
	/* Check the length just to be safe */
	if (attrbuf[0] < sizeof(attrbuf)) {
	    DEBUG(10, ("getattrlist for %s returned a bad length (%d) allowing access!\n", path, attrbuf[0]));
	    return True;
	}
	/* Make sure they have read or write access */
	if ((attrbuf[1] & X_OK) && ((attrbuf[1] & R_OK) || (attrbuf[1] & W_OK)) ) {
		DEBUG(10, ("%s allowing access 0x%x\n", path, attrbuf[1]));
		return True;
	}
	DEBUG(5, ("%s disallowing access 0x%x\n", path, attrbuf[1]));
	return False;
}
