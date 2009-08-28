/*
 * Darwin Show all volumes for admin users
 *
 * Copyright (c) 2007-2008 Apple Inc. All rights reserved.
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
#include "smb_macros.h"
#include <sys/attr.h>

#define APPLE_SLASH_VOLUMES "/Volumes"

/*
 * We have a path to a mounted volume, use getattrlist to get its real name. If
 * we fail then just use the last component of the path name.
 */
static void get_volume_name(const char *path, char *volname)
{
	struct attrlist attrlist;
	char attrbuf[sizeof(struct attrreference) + sizeof(u_int32_t) + NAME_MAX + 1];
	struct attrreference * data = (struct attrreference *)attrbuf;
	u_int32_t *nmlen;
	char *name = NULL;
	int len, maxlen;

	ZERO_STRUCT(attrlist);
	ZERO_STRUCT(attrbuf);
	attrlist.bitmapcount = ATTR_BIT_MAP_COUNT;
	attrlist.commonattr = ATTR_CMN_NAME;
	/* Call getattrlist to get the real volume name */
	if (getattrlist(path, &attrlist, attrbuf, sizeof(attrbuf), 0) != 0) {
		DEBUG(5, ("getattrlist for %s failed: %s\n", path, strerror(errno)));
		return;
	}
	/* Make sure we didn't get something bad */
	maxlen = data->attr_dataoffset - (sizeof(struct attrreference) +  sizeof(u_int32_t));
	nmlen = (u_int32_t *)(attrbuf+sizeof(struct attrreference));
	/* Should never happen, but just to be safe */
	if (*nmlen > maxlen) {
		DEBUG(5, ("name length to large for buffer nmlen=%u maxlen=%u\n",
			    (unsigned)nmlen, (unsigned)maxlen));
		return;
	}
	len = *nmlen++;
	name = (char *)nmlen;
	strlcpy(volname, name, NAME_MAX + 1);
	return;
}

static int add_admin_volume(const char * name, const char * path)
{
	int snum;

	snum = lp_servicenumber(name);
	if (VALID_SNUM(snum)) {
	    return snum;
	}

	snum = lp_add_default_service(name);
	if (!VALID_SNUM(snum)) {
		return -1;
	}

	lp_do_parameter(snum, "path", path);
	lp_do_parameter(snum, "valid users", "+BUILTIN\\Administrators");
	lp_do_parameter(snum, "read only", "no");
	return snum;
}

/*
 * Get all the mounted volumes and search for local volumes that are either the root mount
 * point or have been mounted under /Volumes. Call getattrlist to get the real volume name
 * to display in the share list.
 */
int apple_clone_local_volumes(const char * svcname)
{
	struct statfs *sb, *stat_p = NULL;
	int n = getfsstat(NULL, 0, MNT_NOWAIT);
	char volname[NAME_MAX + 1];
	int ii;

	if (n <= 0)	/* nothing to do just return */
		goto out;

	stat_p = (struct statfs *)SMB_MALLOC(n * sizeof(*stat_p));
	if (stat_p == NULL)  /* nothing to do just return */
		goto out;

	/* Never wait gettng the list of mounted volumes */
	if (getfsstat(stat_p, n * sizeof(*stat_p), MNT_NOWAIT) <= 0)
		goto out;

	sb = stat_p;
	for (ii = 0; ii < n; ii++, sb++) {
		/* Must be local mount and either the root volume or mount under /Volumes */
		if (!(sb->f_flags & MNT_LOCAL)) {
			continue;
		}

		if (sb->f_flags & MNT_DONTBROWSE)  {
		    continue;
		}

		if (strncmp(sb->f_mntonname, "/", 2) == 0) {
		    continue;
		}

		if (strncmp(sb->f_mntonname, APPLE_SLASH_VOLUMES,
			    strlen(APPLE_SLASH_VOLUMES)) == 0) {

			int snum;

			get_volume_name(sb->f_mntonname, volname);

			/* If we were asked to add a service for a specific
			 * admin volume, check whether we have the right one.
			 */
			if (svcname && !strequal(svcname, volname)) {
			    continue;
			}

			snum = add_admin_volume(volname, sb->f_mntonname);
			if (VALID_SNUM(snum)) {
				DEBUG(5,("Sharing %s with path = %s\n",
					    volname, sb->f_mntonname));
			} else {
				DEBUG(5,("Failed to share %s with path = %s\n",
					    volname, sb->f_mntonname));
			}

			if (svcname) {
				return snum;
			}
		}
	}

out:
	/* Clean up */
	SAFE_FREE(stat_p);

	return GLOBAL_SECTION_SNUM;
}
