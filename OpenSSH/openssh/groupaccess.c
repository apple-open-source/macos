/* $OpenBSD: groupaccess.c,v 1.13 2008/07/04 03:44:59 djm Exp $ */
/*
 * Copyright (c) 2001 Kevin Steves.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/param.h>

#include <grp.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>

#ifdef __APPLE_MEMBERSHIP__
#include <membership.h>
#endif

#include "xmalloc.h"
#include "groupaccess.h"
#include "match.h"
#include "log.h"

#ifdef __APPLE_MEMBERSHIP__
// SPI for 5235093
int32_t getgrouplist_2(const char *, gid_t, gid_t **);
int32_t getgroupcount(const char *, gid_t);
#endif

static int ngroups;
static char **groups_byname;
#ifdef __APPLE_MEMBERSHIP__
uuid_t u_uuid;
#endif

/*
 * Initialize group access list for user with primary (base) and
 * supplementary groups.  Return the number of groups in the list.
 */
int
ga_init(struct passwd *pw)
{
	gid_t *groups_bygid = NULL;
	int i, j;
	struct group *gr;

#ifdef __APPLE_MEMBERSHIP__
	if (0 != mbr_uid_to_uuid(pw->pw_uid, u_uuid))
		return 0;
#endif

	if (ngroups > 0)
		ga_free();

#ifndef __APPLE_MEMBERSHIP__
	ngroups = NGROUPS_MAX;
#if defined(HAVE_SYSCONF) && defined(_SC_NGROUPS_MAX)
	ngroups = MAX(NGROUPS_MAX, sysconf(_SC_NGROUPS_MAX));
#endif	
	groups_bygid = xcalloc(ngroups, sizeof(*groups_bygid));
#else
	if (-1 == (ngroups = getgrouplist_2(pw->pw_name, pw->pw_gid,
	    &groups_bygid))) {
		logit("getgrouplist_2 failed");
		return;
	}
#endif
	groups_byname = xcalloc(ngroups, sizeof(*groups_byname));
#ifndef __APPLE_MEMBERSHIP__
	if (getgrouplist(pw->pw_name, pw->pw_gid, groups_bygid, &ngroups) == -1) {
	    logit("getgrouplist: groups list too small");
		xfree(groups_bygid);
		return;
	}
#endif
	for (i = 0, j = 0; i < ngroups; i++)
		if ((gr = getgrgid(groups_bygid[i])) != NULL)
			groups_byname[j++] = xstrdup(gr->gr_name);
	xfree(groups_bygid);
	return (ngroups = j);
}

/*
 * Return 1 if one of user's groups is contained in groups.
 * Return 0 otherwise.  Use match_pattern() for string comparison.
 * Use mbr_check_membership() for membership checking on Mac OS X.
 */
int
ga_match(char * const *groups, int n)
{
#ifdef __APPLE_MEMBERSHIP__
	int i, ismember = 0;
	uuid_t g_uuid;
	struct group *grp;

	for (i = 0; i < n; i++) {
		if ((grp = getgrnam(groups[i])) == NULL ||
		   (mbr_gid_to_uuid(grp->gr_gid, g_uuid) != 0) ||
		   (mbr_check_membership(u_uuid, g_uuid, &ismember) != 0))
			return 0;
		if (ismember)
			return 1;
	}
#else
	int i, j;

	for (i = 0; i < ngroups; i++)
		for (j = 0; j < n; j++)
			if (match_pattern(groups_byname[i], groups[j]))
				return 1;
#endif
	return 0;
}

/*
 * Return 1 if one of user's groups matches group_pattern list.
 * Return 0 on negated or no match.
 */
int
ga_match_pattern_list(const char *group_pattern)
{
	int i, found = 0;
	size_t len = strlen(group_pattern);

	for (i = 0; i < ngroups; i++) {
		switch (match_pattern_list(groups_byname[i],
		    group_pattern, len, 0)) {
		case -1:
			return 0;	/* Negated match wins */
		case 0:
			continue;
		case 1:
			found = 1;
		}
	}
	return found;
}

/*
 * Free memory allocated for group access list.
 */
void
ga_free(void)
{
	int i;

	if (ngroups > 0) {
		for (i = 0; i < ngroups; i++)
			xfree(groups_byname[i]);
		ngroups = 0;
		xfree(groups_byname);
	}
}
