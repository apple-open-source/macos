/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 *	autod_parse.c
 *
 *	Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 *	Use is subject to license terms.
 */

/*
 * Portions Copyright 2007-2011 Apple Inc.
 */

#pragma ident	"@(#)autod_parse.c	1.31	05/06/08 SMI"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <pwd.h>
#include <mntopts.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fstab.h>
#include <locale.h>
#include <stdlib.h>
#include <unistd.h>
#include <rpc/rpc.h>
#include <rpcsvc/mount.h>
#include <fcntl.h>
#include <limits.h>

#include "automount.h"
#include "auto_mntopts.h"

/*
 * This structure is used to determine the hierarchical
 * relationship between directories
 */
struct _hiernode
{
	char dirname[MAXFILENAMELEN+1];
	struct _hiernode *subdir;
	struct _hiernode *leveldir;
	struct mapent *mapent;
};
typedef struct _hiernode hiernode;

void free_mapent(struct mapent *);

static int mapline_to_mapent(struct mapent **, struct mapline *, const char *,
				const char *, const char *, char *, uint_t);
static int hierarchical_sort(struct mapent *, hiernode **, const char *,
			const char *);
static int push_options(hiernode *, char *, const char *, int);
static int set_mapent_opts(struct mapent *, char *, const char *);
static int get_opts(const char *, char *, char *, size_t, bool_t *);
static int fstype_opts(struct mapent *, const char *, const char *,
			const char *);
static int modify_mapents(struct mapent **, const char *, const char *,
			const char *, hiernode *, const char *, uint_t, bool_t);
static int set_and_fake_mapent_mntlevel(hiernode *, const char *, const char *,
				const char *, struct mapent **, uint_t,
				const char *, bool_t);
static int mark_level1_root(hiernode *, char *);
static int mark_and_fake_level1_noroot(hiernode *, const char *, const char *,
				    const char *, struct mapent **, uint_t i,
				    const char *);
static int convert_mapent_to_automount(struct mapent *, const char *,
				const char *);
static int automount_opts(char **, const char *);
static int parse_fsinfo(const char *, struct mapent *);
static int is_nonnfs_url(char *, char *);
static int parse_nfs(const char *, struct mapent *, char *, char *, char **,
				char **);
static int parse_special(struct mapent *, char *, char *, char **, char **,
				int);
static int get_dir_from_path(char *, const char **, int);
static int alloc_hiernode(hiernode **, char *);
static void free_hiernode(hiernode *);
static void trace_mapents(char *, struct mapent *);
static void trace_hierarchy(hiernode *, int);
static struct mapent *do_mapent_hosts(const char *, const char *, uint_t,
			int *);
static void freeex_ent(struct exportnode *);
static void freeex(struct exportnode *);
static struct mapent *do_mapent_fstab(const char *, const char *, uint_t,
				int *, int *);
static struct mapent *do_mapent_static(const char *, uint_t, int *);
static void dump_mapent_err(struct mapent *, const char *, const char *);

#define	PARSE_OK	0
#define	PARSE_ERROR	-1
#define	MAX_FSLEN	32

/*
 * mapentry error type defininitions
 */
#define	MAPENT_NOERR	0
#define	MAPENT_UATFS	1

/*
 * Make a copy of a string, if it's not too long, and save a pointer to
 * it in the pointed-to location.  The maximum length includes the
 * null terminator (e.g., a value of 2 means the longest string can
 * have only 1 character).
 *
 * Return 0 on success, ENAMETOOLONG if the string is too long, and
 * ENOMEM if we can't allocate memory for the copy.
 */
static int
strldup(char **dstp, const char *str, size_t maxlen)
{
	size_t len;
	char *dst;

	len = strlen(str) + 1;
	if (len > maxlen)
		return (ENAMETOOLONG);
	dst = malloc(len);
	if (dst == NULL)
		return (ENOMEM);
	memcpy(dst, str, len);
	*dstp = dst;
	return (0);
}

/*
 * Make a copy of a string, with a "/" preceding the string.
 */
static char *
strprefix_slash(const char *str)
{
	size_t outstrsize;
	char *outstr;

	outstrsize = strlen(str) + 2;
	outstr = malloc(outstrsize);
	if (outstr == NULL)
		return (NULL);
	*outstr = '/';
	memcpy(outstr + 1, str, outstrsize - 1);
	return (outstr);
}

/*
 * parse_entry(const char *key, const char *mapname, const char *mapopts,
 *			const char *subdir, uint_t isdirect, int *node_type,
 *			bool_t isrestricted, bool_t mount_access, int *err)
 * Looks up the specified key in the specified map, and then parses the
 * result to build a mapentry list containing the information for the
 * mounts/lookups to be performed. Builds an intermediate mapentry list by
 * processing the map entry, hierarchically sorts (builds a tree of) the
 * list according to mountpoint. Then pushes options down the hierarchy,
 * and fills in the mount file system. Finally, modifies the intermediate
 * list depending on how far in the hierarchy the current request is (uses
 * subdir). Deals with special cases of -hosts, -fstab, and -static map
 * parsing.
 *
 * mapopts must be at most AUTOFS_MAXOPTSLEN bytes long (including the null
 * terminator).
 * 
 * Returns a pointer to the head of the mapentry list.
 */
struct mapent *
parse_entry(const char *key, const char *mapname, const char *mapopts,
			const char *subdir, uint_t isdirect, int *node_type,
			bool_t isrestricted, bool_t mount_access, int *err)
{
	struct dir_entry *dirp;
	char *stack[STACKSIZ];
	char **stkptr = stack;
	struct mapline ml;
	bool_t iswildcard;
	char defaultopts[AUTOFS_MAXOPTSLEN];

	struct mapent *mapents = NULL;
	struct mapent *me;
	hiernode *rootnode = NULL;

	/*
	 * For now, assume the map entry is not a self-link in -fstab,
	 * is not a wildcard match, and is not a trigger.
	 */
	if (node_type != NULL)
		*node_type = 0;

	/*
	 * Check whether this is a special or regular map and, based
	 * on that, do the appropriate lookup, select the appropriate
	 * parser, and build the mapentry list.
	 */
	if (strcmp(mapname, "-hosts") == 0) {
		/*
		 * the /net parser - uses do_mapent_hosts to build mapents.
		 * The mapopts are considered default for every entry, so we
		 * don't push options down hierarchies.
		 */
		mapents = do_mapent_hosts(mapopts, key, isdirect, err);
		if (mapents == NULL)		/* nothing to free */
			return (mapents);

		if (trace > 3)
			trace_mapents("do_mapent_hosts:(return)", mapents);

		*err = hierarchical_sort(mapents, &rootnode, key, mapname);
		if (*err != 0)
			goto parse_error_quiet;
	} else if (strcmp(mapname, "-fstab") == 0) {
		/*
		 * the /Network/Servers parser - uses do_mapent_fstab to
		 * build mapents.
		 *
		 * All entries in fstab have mount options ("ro" and
		 * "rw", if nothing else), so the default mapopts are
		 * ignored.
		 */
		mapents = do_mapent_fstab(mapopts, key, isdirect, node_type,
		    err);
		if (mapents == NULL)		/* nothing to free */
			return (mapents);

		if (trace > 3)
			trace_mapents("do_mapent_fstab:(return)", mapents);

		*err = hierarchical_sort(mapents, &rootnode, key, mapname);
		if (*err != 0)
			goto parse_error_quiet;
	} else if (strcmp(mapname, "-static") == 0) {
		/*
		 * the static fstab parser - looks up the fstab entry
		 * and builds mapents.
		 * (We don't want mapline_to_mapent to process it,
		 * as we don't want macro expansion to be done on
		 * it.)
		 *
		 * All entries in fstab have mount options ("ro" and
		 * "rw", if nothing else), so the default mapopts are
		 * ignored.
		 */
		mapents = do_mapent_static(key, isdirect, err);
		if (mapents == NULL)		/* nothing to free */
			return (mapents);

		if (trace > 3)
			trace_mapents("do_mapent_static:(return)", mapents);

		*err = hierarchical_sort(mapents, &rootnode, key, mapname);
		if (*err != 0)
			goto parse_error_quiet;
	} else {
		/*
		 * All other maps.
		 */

		/* Is there an entry for that map/key in the readdir cache? */
		dirp = rddir_entry_lookup(mapname, key);
		if (dirp == NULL || dirp->line == NULL) {
			/*
			 * Either there's no entry, or the entry has a
			 * name but no map line information.  We'll need
			 * to look up the entry in the map.
			 */

			/* initialize the stack of open files for this thread */
			stack_op(INIT, NULL, stack, &stkptr);

			/*
			 * Look up the entry in the map.
			 */
			switch (getmapent(key, mapname, &ml, stack, &stkptr,
				&iswildcard, isrestricted)) {

			case __NSW_SUCCESS:
				/*
				 * XXX - we failed to find this entry
				 * in the readdir cache, but we did find
				 * it in the map.  That means the readdir
				 * cache is out of date for this map,
				 * and we should flush the entry for
				 * this map.
				 */
				break;

			case __NSW_NOTFOUND:
				*err = ENOENT;	/* no such map entry */
				return ((struct mapent *)NULL);	/* we failed to find it */

			case __NSW_UNAVAIL:
				syslog(LOG_ERR, "parse_entry: getmapent for map %s, key %s failed",
				    mapname, key);
				*err = EIO;	/* error trying to look up entry */
				return ((struct mapent *)NULL);	/* we failed to find it */
			}
		} else {
			/*
			 * Yes, and it has map line information.
			 * Use it, rather than looking it up again
			 * in the map.
			 *
			 * It's not a wildcard entry, as it corresponds
			 * to an entry in the cache; wildcard entries
			 * are not cached, as they're not returned by
			 * readdir.
			 */
			CHECK_STRCPY(ml.linebuf, dirp->line, LINESZ);
			CHECK_STRCPY(ml.lineqbuf, dirp->lineq, LINESZ);
			iswildcard = FALSE;
		}

		/*
		 * Regular maps have no symlinks, but they *can* have
		 * wildcard entries.  If this is a wildcard entry,
		 * all references to it must trigger a mount, as that's
		 * the only way to check whether it exists or not.
		 */
		if (node_type != NULL) {
			if (iswildcard)
				*node_type |= NT_FORCEMOUNT;
		}

		if (trace > 1)
			trace_prt(1, "  mapline: %s\n", ml.linebuf);

		*err = mapline_to_mapent(&mapents, &ml, key, mapname,
		    mapopts, defaultopts, isdirect);
		if (*err != 0)
			goto parse_error;

		if (mapents == NULL) {
			*err = 0;
			return (mapents);
		}

		*err = hierarchical_sort(mapents, &rootnode, key, mapname);
		if (*err != 0)
			goto parse_error_quiet;

		*err = push_options(rootnode, defaultopts, mapopts,
		    MAPENT_NOERR);
		if (*err != 0)
			goto parse_error;

		if (trace > 3) {
			trace_prt(1, "\n\tpush_options (return)\n");
			trace_prt(0, "\tdefault options=%s\n", defaultopts);
			trace_hierarchy(rootnode, 0);
		};

		*err = parse_fsinfo(mapname, mapents);
		if (*err != 0)
			goto parse_error;
	}

	/*
	 * Modify the mapentry list. We *must* do this only after
	 * the mapentry list is completely built (since we need to
	 * have parse_fsinfo called first).
	 */
	*err = modify_mapents(&mapents, mapname, mapopts, subdir,
	    rootnode, key, isdirect, mount_access);
	if (*err != 0) {
		/*
		 * ENOENT means that something in subdir wasn't found
		 * in the map entry list; that's just a user error.
		 */
		if (*err == ENOENT)
			goto parse_error_quiet;
		else
			goto parse_error;
	}

	/*
	 * XXX: its dangerous to use rootnode after modify mapents as
	 * it may be pointing to mapents that have been freed
	 */
	if (rootnode != NULL)
		free_hiernode(rootnode);

	/*
	 * Is this a directory that has any non-faked, non-modified
	 * entries at level 0?
	 *
	 * If so, this is a trigger, as something should be mounted
	 * on it; otherwise it isn't, as nothing should be mounted
	 * directly on it (or it's a symlink).
	 */
	if (node_type != NULL && !(*node_type & NT_SYMLINK)) {
		for (me = mapents; me; me = me->map_next) {
			if (me->map_mntlevel == 0 &&
			    !me->map_faked && !me->map_modified) {
				*node_type |= NT_TRIGGER;
				break;
			}
		}
	}

	return (mapents);

parse_error:
	syslog(LOG_ERR, "parse_entry: mapentry parse error: map=%s key=%s",
	    mapname, key);
parse_error_quiet:
	free_mapent(mapents);
	if (rootnode != NULL)
		free_hiernode(rootnode);
	return ((struct mapent *)NULL);
}


/*
 * mapline_to_mapent(struct mapent **mapents, struct mapline *ml,
 *		const char *key, const char *mapname, const char *mapopts,
 *		char *defaultopts, uint_t isdirect)
 * Parses the mapline information in ml word by word to build an intermediate
 * mapentry list, which is passed back to the caller. The mapentries may have
 * holes (example no options), as they are completed only later. The logic is
 * awkward, but needed to provide the supported flexibility in the map entries.
 * (especially the first line). Note that the key is the full pathname of the
 * directory to be mounted in a direct map, and ml is the mapentry beyond key.
 *
 * mapopts must be at most MAXOPTSLEN bytes long (including the null
 * terminator).
 *
 * defaultopts must be in a buffer at least MAXOPTSLEN bytes in size; we
 * guarantee not to copy to it a string with more bytes than that (again,
 * including the null terminator).
 *
 * Returns 0 or an appropriate error value.
 */
static int
mapline_to_mapent(struct mapent **mapents, struct mapline *ml, const char *key,
		const char *mapname, const char *mapopts,
		char *defaultopts, uint_t isdirect)
{
	struct mapent *me = NULL;
	struct mapent *mp;
	char w[MAXPATHLEN];
	char wq[MAXPATHLEN];
	int implied;
	int err;

	char *lp = ml->linebuf;
	char *lq = ml->lineqbuf;

	/* do any macro expansions that are required to complete ml */
	switch (macro_expand(key, lp, lq, LINESZ)) {

	case MEXPAND_OK:
		break;

	case MEXPAND_LINE_TOO_LONG:
		syslog(LOG_ERR,
		"mapline_to_mapent: map %s: line too long (max %d chars)",
			mapname, LINESZ - 1);
		return (EIO);

	case MEXPAND_VARNAME_TOO_LONG:
		syslog(LOG_ERR,
		"mapline_to_mapent: map %s: variable name too long",
			mapname);
		return (EIO);
	}
	if (trace > 3 && (strcmp(ml->linebuf, lp) != 0))
		trace_prt(1,
			"  mapline_to_mapent: (expanded) mapline (%s,%s)\n",
			ml->linebuf, ml->lineqbuf);

	/* init the head of mapentry list to null */
	*mapents = NULL;

	/*
	 * Get the first word - its either a '-' if default options provided,
	 * a '/', if the mountroot is explicitly provided, or a mount filesystem
	 * if the mountroot is implicit. Note that if the first word begins with
	 * a '-' then the second must be read and it must be a mountpoint or a
	 * mount filesystem. Use mapopts if no default opts are provided.
	 */
	if (getword(w, wq, &lp, &lq, ' ', sizeof (w)) == -1)
		return (EIO);
	if (*w == '-') {
		if (strlcpy(defaultopts, w, MAXOPTSLEN) >= MAXOPTSLEN) {
			syslog(LOG_ERR, "default options are too long");
			return (EIO);
		}
		if (getword(w, wq, &lp, &lq, ' ', sizeof (w)) == -1)
			return (EIO);
	} else
		CHECK_STRCPY(defaultopts, mapopts, MAXOPTSLEN);

	implied = *w != '/'; /* implied is 1 only if '/' is implicit */
	while (*w == '/' || implied) {
		mp = me;
		if ((me = (struct mapent *)malloc(sizeof (*me))) == NULL)
			goto alloc_failed;
		(void) memset((char *)me, 0, sizeof (*me));
		if (*mapents == NULL)	/* special case of head */
			*mapents = me;
		else
			mp->map_next = me;

		/*
		 * direct maps get an empty string as root - to be filled
		 * by the entire path later. Indirect maps get /key as the
		 * map root.
		 */
		if (isdirect)
			me->map_root = strdup("");
		else
			me->map_root = strprefix_slash(key);
		if (me->map_root == NULL)
			goto alloc_failed;

		/* mntpnt is empty for the mount root */
		if (strcmp(w, "/") == 0 || implied)
			me->map_mntpnt = strdup("");
		else
			me->map_mntpnt = strdup(w);
		if (me->map_mntpnt == NULL)
			goto alloc_failed;

		/*
		 * If implied, the word must be a mount filesystem,
		 * and its already read in; also turn off implied - its
		 * not applicable except for the mount root. Else,
		 * read another (or two) words depending on if there's
		 * an option.
		 */
		if (implied)   /* must be a mount filesystem */
			implied = 0;
		else {
			if (getword(w, wq, &lp, &lq, ' ', sizeof (w)) == -1)
				return (EIO);
			if (w[0] == '-') {
				/* mount options */
				err = strldup(&me->map_mntopts, w, MAXOPTSLEN);
				if (err == ENAMETOOLONG) {
					syslog(LOG_ERR,
					    "mount options for are too long");
					return (EIO);
				}
				if (err == ENOMEM)
					goto alloc_failed;
				if (getword(w, wq, &lp, &lq, ' ',
				    sizeof (w)) == -1)
					return (EIO);
			}
		}

		/*
		 * must be a mount filesystem or a set of filesystems at
		 * this point.
		 */
		if (w[0] == '\0' || w[0] == '-') {
			syslog(LOG_ERR,
			"mapline_to_mapent: bad location=%s map=%s key=%s",
				w, mapname, key);
			return (EIO);
		}

		/*
		 * map_fsw and map_fswq hold information which will be
		 * used to determine filesystem information at a later
		 * point. This is required since we can only find out
		 * about the mount file system after the directories
		 * are hierarchically sorted and options have been pushed
		 * down the hierarchies.
		 */
		if (((me->map_fsw = strdup(w)) == NULL) ||
		    ((me->map_fswq = strdup(wq)) == NULL))
			goto alloc_failed;

		/*
		 * the next word, if any, is either another mount point or a
		 * mount filesystem if more than one server is listed.
		 */
		if (getword(w, wq, &lp, &lq, ' ', sizeof (w)) == -1)
			return (EIO);
		while (*w && *w != '/') {	/* more than 1 server listed */
			char *fsw, *fswq;
			if (asprintf(&fsw, "%s   %s", me->map_fsw, w) == -1)
				goto alloc_failed;
			free(me->map_fsw);
			me->map_fsw = fsw;
			if (asprintf(&fswq, "%s   %s", me->map_fswq, wq) == -1)
				goto alloc_failed;
			free(me->map_fswq);
			me->map_fswq = fswq;
			if (getword(w, wq, &lp, &lq, ' ', sizeof (w)) == -1)
				return (EIO);
		}

		/* initialize flags */
		me->map_mntlevel = -1;
		me->map_modified = FALSE;
		me->map_faked = FALSE;
		me->map_err = MAPENT_NOERR;

		me->map_next = NULL;
	}

	if (*mapents == NULL || w[0] != '\0') {	/* sanity check */
		if (verbose) {
			if (*mapents == NULL)
				syslog(LOG_ERR,
				"mapline_to_mapent: parsed with null mapents");
			else
				syslog(LOG_ERR,
				"mapline_to_mapent: parsed nononempty w=%s", w);
		}
		return (EIO);
	}

	if (trace > 3)
		trace_mapents("mapline_to_mapent:", *mapents);

	return (0);

alloc_failed:
	syslog(LOG_ERR, "mapline_to_mapent: Memory allocation failed");
	return (ENOMEM);
}

/*
 * hierarchical_sort(struct mapent *mapents, hiernode **rootnode, char *key
 *                   char *mapname)
 * sorts the mntpnts in each mapent to build a hierarchy of nodes, with
 * with the rootnode being the mount root. The hierarchy is setup as
 * levels, and subdirs below each level. Provides a link from node to
 * the relevant mapentry.
 * Returns 0 or appropriate error value; logs a message on error.
 */
static int
hierarchical_sort(struct mapent *mapents, hiernode **rootnode, const char *key,
	const char *mapname)
{
	hiernode *prevnode, *currnode, *newnode;
	const char *path;
	char dirname[MAXFILENAMELEN];

	int rc = 0;
	struct mapent *me = mapents;

	/* allocate the rootnode with a default path of "" */
	*rootnode = NULL;
	if ((rc = alloc_hiernode(rootnode, "")) != 0)
		return (rc);

	/*
	 * walk through mapents - for each mapent, locate the position
	 * within the hierarchy by walking across leveldirs, and
	 * subdirs of matched leveldirs. Starts one level below
	 * the root (assumes an implicit match with rootnode).
	 * XXX - this could probably be done more cleanly using recursion.
	 */
	while (me != NULL) {

		path = me->map_mntpnt;

		if ((rc = get_dir_from_path(dirname, &path,
		    sizeof (dirname))) != 0)
			return (rc);

		prevnode = *rootnode;
		currnode = (*rootnode)->subdir;

		while (dirname[0] != '\0') {
			if (currnode != NULL) {
				if (strcmp(currnode->dirname, dirname) == 0) {
					/*
					 * match found - mntpnt is a child of
					 * this node
					 */
					prevnode = currnode;
					currnode = currnode->subdir;
				} else {
					prevnode = currnode;
					currnode = currnode->leveldir;

					if (currnode == NULL) {
						/*
						 * No more leveldirs to match.
						 * Add a new one
						 */
						if ((rc = alloc_hiernode
							(&newnode, dirname))
							!= 0)
							return (rc);
						prevnode->leveldir = newnode;
						prevnode = newnode;
						currnode = newnode->subdir;
					} else {
						/* try this leveldir */
						continue;
					}
				}
			} else {
				/* no more subdirs to match. Add a new one */
				if ((rc = alloc_hiernode(&newnode,
				    dirname)) != 0)
					return (rc);
				prevnode->subdir = newnode;
				prevnode = newnode;
				currnode = newnode->subdir;
			}
			if ((rc = get_dir_from_path(dirname, &path,
			    sizeof (dirname))) != 0)
				return (rc);
		}

		if (prevnode->mapent != NULL) {
			/* duplicate mntpoint found */
			char *root;

			if (strcmp(mapname, "-fstab") == 0) {
				syslog(LOG_ERR,
				    "Duplicate mounts for %s:%s in fstab",
				    key, me->map_mntpnt);
			} else {
				root = me->map_root;
				while (*root == '/')
					root++;
				syslog(LOG_ERR,
				    "Duplicate submounts for %s%s in map %s, key %s",
				    root, me->map_mntpnt, mapname, key);
			}
			return (EIO);
		}

		/* provide a pointer from node to mapent */
		prevnode->mapent = me;
		me = me->map_next;
	}

	if (trace > 3) {
		trace_prt(1, "\n\thierarchical_sort:\n");
		trace_hierarchy(*rootnode, 0);	/* 0 is rootnode's level */
	}

	return (rc);
}

/*
 * push_options(hiernode *node, const char *defaultopts, const char *mapopts,
 *		int err)
 * Pushes the options down a hierarchical structure. Works recursively from the
 * root, which is passed in on the first call. Uses a replacement policy.
 * If a node points to a mapentry, and it has an option, then thats the option
 * for that mapentry. Else, the node's mapent inherits the option from the
 * default (which may be the global option for the entry or mapopts).
 * err is useful in flagging entries with errors in pushing options.
 * returns 0 or appropriate error value.
 *
 * defaultopts and mapopts must each be at most MAXOPTSLEN bytes long
 * (including the null terminator).
 */
static int
push_options(hiernode *node, char *defaultopts, const char *mapopts, int err)
{
	int rc = 0;
	struct mapent *me = NULL;

	/* ensure that all the dirs at a level are passed the default options */
	while (node != NULL) {
		me = node->mapent;
		if (me != NULL) {	/* not all nodes point to a mapentry */
			me->map_err = err;
			if ((rc = set_mapent_opts(me, defaultopts, mapopts)) != 0)
				return (rc);
		}

		/* push the options to subdirs */
		if (node->subdir != NULL) {
			if (node->mapent && node->mapent->map_fstype &&
			    strcmp(node->mapent->map_fstype, MNTTYPE_AUTOFS) == 0)
				err = MAPENT_UATFS;
			if ((rc = push_options(node->subdir, defaultopts,
				mapopts, err)) != 0)
				return (rc);
		}
		node = node->leveldir;
	}
	return (rc);
}

#define	BACKFSTYPE "backfstype" /* used in cachefs options */
#define	BACKFSTYPE_EQ "backfstype="
#define	FSTYPE "fstype"
#define	FSTYPE_EQ "fstype="
#define	NO_OPTS ""

/*
 * set_mapent_opts(struct mapent *me, char *defaultopts, const char *mapopts)
 * sets the mapentry's options, fstype and mounter fields by separating
 * out the fstype part from the opts. Use default options if me->map_mntopts
 * is NULL.  Note that defaultopts may be the same as mapopts.
 *
 * me->map_mntopts, defaultopts, and mapopts must each be at most
 * AUTOFS_MAXOPTSLEN bytes long (including the null terminator).
 *
 * Returns 0 or appropriate error value.
 */
static int
set_mapent_opts(struct mapent *me, char *defaultopts, const char *mapopts)
{
	char optsbuf[AUTOFS_MAXOPTSLEN];
	char *opts;
	char entryopts[AUTOFS_MAXOPTSLEN];
	char fstype[MAX_FSLEN], mounter[MAX_FSLEN];
	int rc = 0;
	bool_t fstype_opt = FALSE;

	/* set options to default options, if none exist for this entry */
	if (me->map_mntopts == NULL) {
		opts = defaultopts;
		if (defaultopts == NULL) { /* NULL opts for entry */
			/*
			 * File system type not explicitly specified, as
			 * no options were specified; it will be determined
			 * later.
			 */
			me->map_fstype = NULL;
			me->map_mounter = NULL;
			return (0);
		}
	} else {
		/*
		 * Make a copy of map_mntopts, as we'll free it below.
		 */
		CHECK_STRCPY(optsbuf, me->map_mntopts, AUTOFS_MAXOPTSLEN);
		opts = optsbuf;
	}

	if (*opts == '-')
		opts++;

	/* separate opts into fstype and (other) entrypopts */
	if (!get_opts(opts, entryopts, fstype, sizeof (fstype), &fstype_opt)) {
	    	syslog(LOG_ERR, "file system type is too long");
		return (EIO);
	}

	/* replace any existing opts */
	if (me->map_mntopts != NULL)
		free(me->map_mntopts);
	if ((me->map_mntopts = strdup(entryopts)) == NULL)
		return (ENOMEM);

	if (fstype_opt == TRUE) {
		/* mounter and fstype are the same size */
		strcpy(mounter,	fstype);

#ifdef MNTTYPE_CACHEFS
		/*
		 * The following ugly chunk of code crept in as a result of
		 * cachefs.  If it's a cachefs mount of an nfs filesystem, then
		 * it's important to parse the nfs special field.  Otherwise,
		 * just hand the special field to the fs-specific mount
		 */
		if (strcmp(fstype, MNTTYPE_CACHEFS) ==  0) {
			struct mnttab m;
			char *p;

			m.mnt_mntopts = entryopts;
			if ((p = hasmntopt(&m, BACKFSTYPE)) != NULL) {
				int len = strlen(MNTTYPE_NFS);

				p += strlen(BACKFSTYPE_EQ);

				if (strncmp(p, MNTTYPE_NFS, len) ==  0 &&
					(p[len] == '\0' || p[len] == ',')) {
					/*
					 * Cached nfs mount
					 */
					CHECK_STRCPY(fstype, MNTTYPE_NFS, sizeof (fstype));
					CHECK_STRCPY(mounter, MNTTYPE_CACHEFS, sizeof (mounter));
				}
			}
		}
#endif

		/*
		 * If the child options are exactly fstype = somefs, i.e.
		 * if no other options are specified, we need to do some
		 * more option pushing work.
		 */
		if (strcmp(me->map_mntopts, NO_OPTS) == 0) {
			free(me->map_mntopts);
			me->map_mntopts = NULL;
			if ((rc = fstype_opts(me, opts, defaultopts,
			    mapopts)) != 0)
				return (rc);
		}

		/*
		 * File system type explicitly specified; set it.
		 */
		if (((me->map_fstype = strdup(fstype)) == NULL) ||
			((me->map_mounter = strdup(mounter)) == NULL)) {
			if (me->map_fstype != NULL)
				free(me->map_fstype);
			syslog(LOG_ERR, "set_mapent_opts: No memory");
			return (ENOMEM);
		}
	} else {
		/*
		 * File system type not explicitly specified; it
		 * will be determined later.
		 */
		me->map_fstype = NULL;
		me->map_mounter = NULL;
	}

	return (rc);
}

/*
 * Check the option string for an "fstype"
 * option.  If found, return the fstype
 * and the option string with the fstype
 * option removed, e.g.
 *
 *  input:  "fstype=cachefs,ro,nosuid"
 *  opts:   "ro,nosuid"
 *  fstype: "cachefs"
 *
 * Also indicates if the fstype option was present
 * by setting a flag, if the pointer to the flag
 * is not NULL.
 *
 * input must be at most MAXOPTSLEN bytes long (including the null terminator).
 *
 * opts must point to a buffer at least MAXOPTSLEN bytes in size; we guarantee
 * not to copy to it a string with more bytes than that (again, including the
 * null terminator).
 *
 * 0 is returned if fstype would be more than fstype_size bytes long,
 * otherwise 1 is returned.
 */
static int
get_opts(input, opts, fstype, fstype_size, fstype_opt)
	const char *input;
	char *opts; 	/* output */
	char *fstype;   /* output */
	size_t fstype_size;
	bool_t *fstype_opt;
{
	char *p, *pb;
	char buf[MAXOPTSLEN];
	char *placeholder;

	*opts = '\0';
	CHECK_STRCPY(buf, input, sizeof (buf));
	pb = buf;
	while ((p = strtok_r(pb, ",", &placeholder)) != NULL) {
		pb = NULL;
		if (strncmp(p, FSTYPE_EQ, 7) == 0) {
			if (fstype_opt != NULL)
				*fstype_opt = TRUE;
			if (strlcpy(fstype, p + 7, fstype_size) >= fstype_size)
				return (0);
		} else {
			if (*opts)
				CHECK_STRCAT(opts, ",", MAXOPTSLEN);
			CHECK_STRCAT(opts, p, MAXOPTSLEN);
		}
	}
	return (1);
}

/*
 * fstype_opts(struct mapent *me, const char *opts, const char *defaultopts,
 *				const char *mapopts)
 * We need to push global options to the child entry if it is exactly
 * fstype=somefs.
 *
 * opts, defaultopts, and mapopts must each be at most AUTOFS_MAXOPTSLEN
 * bytes long (including the null terminator).
 */
static int
fstype_opts(struct mapent *me, const char *opts, const char *defaultopts,
				const char *mapopts)
{
	const char *optstopush;
	char pushopts[AUTOFS_MAXOPTSLEN];
	char pushentryopts[AUTOFS_MAXOPTSLEN];
	char pushfstype[MAX_FSLEN];
	int err;

	if (defaultopts && *defaultopts == '-')
		defaultopts++;

	/*
	 * the options to push are the global defaults for the entry,
	 * if they exist, or mapopts, if the global defaults for the
	 * entry does not exist.
	 */
	if (strcmp(defaultopts, opts) == 0) {
		if (*mapopts == '-')
			mapopts++;
		optstopush = mapopts;
	} else
		optstopush = defaultopts;
	if (!get_opts(optstopush, pushentryopts, pushfstype,
	    sizeof (pushfstype), NULL)) {
	    	syslog(LOG_ERR, "file system type is too long");
		return (EIO);
	}
	CHECK_STRCPY(pushopts, optstopush, sizeof (pushopts));

#ifdef MNTTYPE_CACHEFS
	if (strcmp(pushfstype, MNTTYPE_CACHEFS) == 0)
		err = strldup(&me->map_mntopts, pushopts, MAXOPTSLEN);
	else
#endif
		err = strldup(&me->map_mntopts, pushentryopts, MAXOPTSLEN);

	if (err == ENAMETOOLONG) {
		syslog(LOG_ERR, "mount options are too long");
		return (EIO);
	}
	if (err == ENOMEM) {
		syslog(LOG_ERR, "fstype_opts: No memory");
		return (ENOMEM);
	}

	return (0);
}

/*
 * modify_mapents(struct mapent **mapents, const char *mapname,
 *			const char *mapopts, const char *subdir,
 *			hiernode *rootnode, const char *key,
 *			uint_t isdirect, bool_t mount_access)
 * modifies the intermediate mapentry list into the final one, and passes
 * back a pointer to it. The final list may contain faked mapentries for
 * hiernodes that do not point to a mapentry, or converted mapentries, if
 * hiernodes that point to a mapentry need to be converted from nfs to autofs.
 * mounts. Entries that are not directly 1 level below the subdir are removed.
 * Returns 0, ENOENT, or EIO
 */
static int
modify_mapents(struct mapent **mapents, const char *mapname,
			const char *mapopts, const char *subdir,
			hiernode *rootnode, const char *key,
			uint_t isdirect, bool_t mount_access)
{
	struct mapent *mp = NULL;
	char w[MAXPATHLEN];

	struct mapent *me;
	int rc = 0;
	struct mapent *faked_mapents = NULL;

	/*
	 * correct the mapentry mntlevel from default -1 to level depending on
	 * position in hierarchy, and build any faked mapentries, if required
	 * at one level below the rootnode given by subdir.
	 */
	if ((rc = set_and_fake_mapent_mntlevel(rootnode, subdir, key, mapname,
		&faked_mapents, isdirect, mapopts, mount_access)) != 0)
		return (rc);

	/*
	 * attaches faked mapents to real mapents list. Assumes mapents
	 * is not NULL.
	 */
	me = *mapents;
	while (me->map_next != NULL)
		me = me->map_next;
	me->map_next = faked_mapents;

	/*
	 * get rid of nodes marked at level -1
	 */
	me = *mapents;
	while (me != NULL) {
		if ((me->map_mntlevel ==  -1) || (me->map_err) ||
			(mount_access == FALSE && me->map_mntlevel == 0)) {
			/*
			 * syslog any errors and free entry
			 */
			if (me->map_err)
				dump_mapent_err(me, key, mapname);

			if (me ==  (*mapents)) {
				/* special case when head has to be freed */
				*mapents = me->map_next;
				if ((*mapents) ==  NULL) {
					/* something wierd happened */
					if (verbose)
						syslog(LOG_ERR,
						"modify_mapents: level error");
					return (EIO);
				}

				/* separate out the node */
				me->map_next = NULL;
				free_mapent(me);
				me = *mapents;
			} else {
				mp->map_next = me->map_next;
				me->map_next = NULL;
				free_mapent(me);
				me = mp->map_next;
			}
			continue;
		}

		/*
		 * convert level 1 mapents that are not already autonodes
		 * to autonodes
		 */
		if (me->map_mntlevel == 1 && me->map_fstype != NULL &&
			(strcmp(me->map_fstype, MNTTYPE_AUTOFS) != 0) &&
			(me->map_faked != TRUE)) {
			if ((rc = convert_mapent_to_automount(me, mapname,
			    mapopts)) != 0)
				return (rc);
		}
		CHECK_STRCPY(w, (me->map_mntpnt+strlen(subdir)), sizeof (w));
		/* w is shorter than me->map_mntpnt, so this strcpy is safe */
		strcpy(me->map_mntpnt, w);
		mp = me;
		me = me->map_next;
	}

	if (trace > 3)
		trace_mapents("modify_mapents:", *mapents);

	return (0);
}

/*
 * set_and_fake_mapent_mntlevel(hiernode *rootnode, const char *subdir,
 *		const char *key, const char *mapname,
 *		struct mapent **faked_mapents, uint_t isdirect,
 *		const char *mapopts, bool_t mount_access)
 * sets the mapentry mount levels (depths) with respect to the subdir.
 * Assigns a value of 0 to the new root. Finds the level1 directories by
 * calling mark_*_level1_*(). Also cleans off extra /'s in level0 and
 * level1 map_mntpnts. Note that one level below the new root is an existing
 * mapentry if there's a mapentry (nfs mount) corresponding to the root,
 * and the direct subdir set for the root, if there's no mapentry corresponding
 * to the root (we install autodirs). Returns 0 or error value.
 */
static int
set_and_fake_mapent_mntlevel(hiernode *rootnode, const char *subdir,
		const char *key, const char *mapname,
		struct mapent **faked_mapents, uint_t isdirect,
		const char *mapopts, bool_t mount_access)
{
	char dirname[MAXFILENAMELEN];
	char traversed_path[MAXPATHLEN]; /* used in building fake mapentries */

	const char *subdir_child = subdir;
	hiernode *prevnode = rootnode;
	hiernode *currnode = rootnode->subdir;
	int rc = 0;
	traversed_path[0] = '\0';

	/*
	 * find and mark the root by tracing down subdir. Use traversed_path
	 * to keep track of how far we go, while guaranteeing that it
	 * contains no '/' at the end. Took some mucking to get that right.
	 */
	if ((rc = get_dir_from_path(dirname, &subdir_child, sizeof (dirname)))
				!= 0)
		return (rc);

	if (dirname[0] != '\0') {
		if (strlcat(traversed_path, "/", sizeof (traversed_path)) >=
		      sizeof (traversed_path) ||
		    strlcat(traversed_path, dirname, sizeof (traversed_path)) >=
		      sizeof (traversed_path)) {
			syslog(LOG_ERR, "set_and_fake_mapent_mntlevel: maximum path name length exceeded");
			return (EIO);
		}
	}

	prevnode = rootnode;
	currnode = rootnode->subdir;
	while (dirname[0] != '\0' && currnode != NULL) {
		if (strcmp(currnode->dirname, dirname) == 0) {

			/* subdir is a child of currnode */
			prevnode = currnode;
			currnode = currnode->subdir;

			if ((rc = get_dir_from_path(dirname, &subdir_child,
			    sizeof (dirname))) != 0)
				return (rc);
			if (dirname[0] != '\0') {
				if (strlcat(traversed_path, "/", sizeof (traversed_path)) >=
				      sizeof (traversed_path) ||
				    strlcat(traversed_path, dirname, sizeof (traversed_path)) >=
				      sizeof (traversed_path)) {
					syslog(LOG_ERR,
					    "set_and_fake_mapent_mntlevel: maximum path name length exceeded");
					return (EIO);
				}
			}
		} else {
			/* try next leveldir */
			prevnode = currnode;
			currnode = currnode->leveldir;
		}
	}

	if (dirname[0] != '\0') {
		/*
		 * We didn't find dirname in the map entry.
		 */
		if (verbose)
			syslog(LOG_ERR,
			"set_and_fake_mapent_mntlevel: subdir=%s error: %s not found in map=%s",
			    subdir, dirname, mapname);
		return (ENOENT);
	}

	/*
	 * see if level of root really points to a mapent and if
	 * have access to that filessystem - call appropriate
	 * routine to mark level 1 nodes, and build faked entries
	 */
	if (prevnode->mapent != NULL && mount_access == TRUE) {
		if (trace > 3)
			trace_prt(1, "  node mountpoint %s\t travpath=%s\n",
				prevnode->mapent->map_mntpnt, traversed_path);

		/*
		 * Copy traversed path map_mntpnt to get rid of any extra
		 * '/' the map entry may contain.
		 */
		if (strlen(prevnode->mapent->map_mntpnt) <
				strlen(traversed_path)) { /* sanity check */
			/*
			 * prevnode->mapent->map_mntpnt is too small to hold
			 * traversed_path.
			 */
			if (verbose)
				syslog(LOG_ERR,
				"set_and_fake_mapent_mntlevel: path=%s error",
				    traversed_path);
			return (EIO);
		}
		/*
		 * traversed_path is shorter than prevnode->mapent->map_mntpnt,
		 * so we can safely copy traversed_path to
		 * prevnode->mapent->map_mntpnt.
		 */
		if (strcmp(prevnode->mapent->map_mntpnt, traversed_path) != 0)
			strcpy(prevnode->mapent->map_mntpnt, traversed_path);

		prevnode->mapent->map_mntlevel = 0; /* root level is 0 */
		if (currnode != NULL) {
			if ((rc = mark_level1_root(currnode,
			    traversed_path)) != 0)
				return (rc);
		}
	} else if (currnode != NULL) {
		if (trace > 3)
			trace_prt(1, "  No rootnode, travpath=%s\n",
				traversed_path);
		if ((rc = mark_and_fake_level1_noroot(currnode,
		    traversed_path, key, mapname, faked_mapents, isdirect,
		    mapopts)) != 0)
			return (rc);
	}

	if (trace > 3) {
		trace_prt(1, "\n\tset_and_fake_mapent_mntlevel\n");
		trace_hierarchy(rootnode, 0);
	}

	return (rc);
}


/*
 * mark_level1_root(hiernode *node, char *traversed_path)
 * marks nodes upto one level below the rootnode given by subdir
 * recursively. Called if rootnode points to a mapent.
 * In this routine, a level 1 node is considered to be the 1st existing
 * mapentry below the root node, so there's no faking involved.
 * Returns 0 or error value
 */
static int
mark_level1_root(hiernode *node, char *traversed_path)
{
	/* ensure we touch all leveldirs */
	while (node) {
		/*
		 * mark node level as 1, if one exists - else walk down
		 * subdirs until we find one.
		 */
		if (node->mapent ==  NULL) {
			char w[MAXPATHLEN];

			if (node->subdir != NULL) {
				if (snprintf(w, sizeof (w), "%s/%s",
				    traversed_path, node->dirname) >=
				      (int)sizeof (w)) {
					syslog(LOG_ERR, "mark_level1_root: maximum path name length exceeded");
					return (EIO);
				}
				if (mark_level1_root(node->subdir, w) == EIO)
					return (EIO);
			} else {
				if (verbose) {
					syslog(LOG_ERR,
					"mark_level1_root: hierarchy error");
				}
				return (EIO);
			}
		} else {
			char w[MAXPATHLEN];

			if (snprintf(w, sizeof (w), "%s/%s", traversed_path,
			    node->dirname) >= (int)sizeof (w)) {
				syslog(LOG_ERR, "mark_level1_root: maximum path name length exceeded");
				return (EIO);
			}
			if (trace > 3)
				trace_prt(1, "  node mntpnt %s\t travpath %s\n",
				    node->mapent->map_mntpnt, w);

			/* replace mntpnt with travpath to clean extra '/' */
			if (strlen(node->mapent->map_mntpnt) < strlen(w)) {
				if (verbose) {
					/*
					 * node->mapent->map_mntpnt is too
					 * small to hold w.
					 */
					syslog(LOG_ERR,
					"mark_level1_root: path=%s error",
					    traversed_path);
				}
				return (EIO);
			}
			/*
			 * w is shorter than node->mapent->map_mntpnt,
			 * so we can safely copy w to node->mapent->map_mntpnt.
			 */
			if (strcmp(node->mapent->map_mntpnt, w) != 0)
				strcpy(node->mapent->map_mntpnt, w);
			node->mapent->map_mntlevel = 1;
		}
		node = node->leveldir;
	}
	return (0);
}

/*
 * mark_and_fake_level1_noroot(hiernode *node, char *traversed_path,
 * 			char *key,char *mapname, struct mapent **faked_mapents,
 *			uint_t isdirect, char *mapopts)
 * Called if the root of the hierarchy does not point to a mapent. marks nodes
 * upto one physical level below the rootnode given by subdir. checks if
 * there's a real mapentry. If not, it builds a faked one (autonode) at that
 * point. The faked autonode is direct, with the map being the same as the
 * original one from which the call originated. Options are same as that of
 * the map and assigned in automount_opts(). Returns 0 or error value.
 */
static int
mark_and_fake_level1_noroot(hiernode *node, const char *traversed_path,
			const char *key, const char *mapname,
			struct mapent **faked_mapents, uint_t isdirect,
			const char *mapopts)
{
	struct mapent *me;
	int rc = 0;
	char w[MAXPATHLEN];

	while (node != NULL) {
		me = NULL;
		if (node->mapent != NULL) {
			/*
			 * existing mapentry at level 1 - copy travpath to
			 * get rid of extra '/' in mntpnt
			 */
			if (snprintf(w, sizeof (w), "%s/%s", traversed_path,
			    node->dirname) >= (int)sizeof (w)) {
				syslog(LOG_ERR,
				"mark_fake_level1_noroot: maximum path name length exceeded");
				return (EIO);
			}
			if (trace > 3)
				trace_prt(1, "  node mntpnt=%s\t travpath=%s\n",
				    node->mapent->map_mntpnt, w);
			if (strlen(node->mapent->map_mntpnt) < strlen(w)) {
				/* sanity check */
				if (verbose) {
					/*
					 * node->mapent->map_mntpnt is too
					 * small to hold w.
					 */
					syslog(LOG_ERR,
					"mark_fake_level1_noroot:path=%s error",
					    traversed_path);
				}
				return (EIO);
			}
			/*
			 * w is shorter than node->mapent->map_mntpnt,
			 * so we can safely copy w to node->mapent->map_mntpnt.
			 */
			if (strcmp(node->mapent->map_mntpnt, w) != 0)
				strcpy(node->mapent->map_mntpnt, w);
			node->mapent->map_mntlevel = 1;
		} else {
			/*
			 * build the faked autonode
			 */
			if ((me = (struct mapent *)malloc(sizeof (*me)))
				== NULL)
				goto alloc_failed;
			(void) memset((char *)me, 0, sizeof (*me));

			if ((me->map_fs = (struct mapfs *)
				malloc(sizeof (struct mapfs))) == NULL)
				goto alloc_failed;
			(void) memset(me->map_fs, 0, sizeof (struct mapfs));

			if (isdirect)
				me->map_root = strdup("");
			else
				me->map_root = strprefix_slash(key);
			if (me->map_root == NULL)
				goto alloc_failed;

			if (asprintf(&me->map_mntpnt, "%s/%s", traversed_path,
				node->dirname) == -1)
				goto alloc_failed;
			me->map_fstype = strdup(MNTTYPE_AUTOFS);
			me->map_mounter = strdup(MNTTYPE_AUTOFS);

			/* set options */
			if ((rc = automount_opts(&me->map_mntopts, mapopts))
				!= 0) {
				free_mapent(me);
				return (rc);
			}
			me->map_fs->mfs_dir = strdup(mapname);
			me->map_mntlevel = 1;
			me->map_modified = FALSE;
			me->map_faked = TRUE;   /* mark as faked */
			if (me->map_root == NULL ||
			    me->map_mntpnt == NULL ||
			    me->map_fstype == NULL ||
			    me->map_mounter == NULL ||
			    me->map_mntopts == NULL ||
			    me->map_fs->mfs_dir == NULL)
				goto alloc_failed;

			if (*faked_mapents == NULL)
				*faked_mapents = me;
			else {			/* attach to the head */
				me->map_next = *faked_mapents;
				*faked_mapents = me;
			}
			node->mapent = me;
		}
		node = node->leveldir;
	}
	return (rc);

alloc_failed:
	syslog(LOG_ERR,	"mark_and_fake_level1_noroot: out of memory");
	if (me != NULL) {
		if (me->map_mounter != NULL)
			free(me->map_mounter);
		if (me->map_fstype != NULL)
			free(me->map_fstype);
		if (me->map_mntpnt != NULL)
			free(me->map_mntpnt);
		if (me->map_root != NULL)
			free(me->map_root);
		if (me->map_fs != NULL) {
			if (me->map_fs->mfs_dir != NULL)
				free(me->map_fs->mfs_dir);
			free(me->map_fs);
		}
		free(me);
	}
	free_mapent(*faked_mapents);
	return (ENOMEM);
}

/*
 * convert_mapent_to_automount(struct mapent *me, const char *mapname,
 *				const char *mapopts)
 * change the mapentry me to an automount - free fields first and NULL them
 * to avoid freeing again, while freeing the mapentry at a later stage.
 * Could have avoided freeing entries here as we don't really look at them.
 * Give the converted mapent entry the options that came with the map using
 * automount_opts(). Returns 0 or appropriate error value.
 */
static int
convert_mapent_to_automount(struct mapent *me, const char *mapname,
				const char *mapopts)
{
	struct mapfs *mfs = me->map_fs;		/* assumes it exists */
	int rc = 0;

	/* free relevant entries */
	if (mfs->mfs_host) {
		free(mfs->mfs_host);
		mfs->mfs_host = NULL;
	}
	while (me->map_fs->mfs_next != NULL) {
		mfs = me->map_fs->mfs_next;
		if (mfs->mfs_host)
			free(mfs->mfs_host);
		if (mfs->mfs_dir)
			free(mfs->mfs_dir);
		me->map_fs->mfs_next = mfs->mfs_next;	/* nulls eventually */
		free((void*)mfs);
	}

	/* replace relevant entries */
	if (me->map_fstype)
		free(me->map_fstype);
	if ((me->map_fstype = strdup(MNTTYPE_AUTOFS)) == NULL)
		goto alloc_failed;

	if (me->map_mounter)
		free(me->map_mounter);
	if ((me->map_mounter = strdup(me->map_fstype)) == NULL)
		goto alloc_failed;

	if (me->map_fs->mfs_dir)
		free(me->map_fs->mfs_dir);
	if ((me->map_fs->mfs_dir = strdup(mapname)) == NULL)
		goto alloc_failed;

	/* set options */
	if (me->map_mntopts) {
		free(me->map_mntopts);
		me->map_mntopts = NULL;
	}
	if ((rc = automount_opts(&me->map_mntopts, mapopts)) != 0)
		return (rc);

	/* mucked with this entry, set the map_modified field to TRUE */
	me->map_modified = TRUE;

	return (rc);

alloc_failed:
	syslog(LOG_ERR,
		"convert_mapent_to_automount: Memory allocation failed");
	return (ENOMEM);
}

/*
 * automount_opts(char **map_mntopts, const char *mapopts)
 * modifies automount opts - gets rid of all "indirect" and "direct" strings
 * if they exist, and then adds a direct string to force a direct automount.
 * Rest of the mapopts stay intact. Returns 0 or appropriate error.
 */
static int
automount_opts(char **map_mntopts, const char *mapopts)
{
	char *opts;
	char *opt;
	size_t len;
	char *placeholder;
	char buf[AUTOFS_MAXOPTSLEN];

	char *addopt = "direct";

	len = strlen(mapopts)+ strlen(addopt)+2;	/* +2 for ",", '\0' */
	if (len > AUTOFS_MAXOPTSLEN) {
		syslog(LOG_ERR,
		"option string %s too long (max=%d)", mapopts,
			AUTOFS_MAXOPTSLEN-8);
		return (EIO);
	}

	if (((*map_mntopts) = ((char *)malloc(len))) == NULL) {
		syslog(LOG_ERR,	"automount_opts: Memory allocation failed");
		return (ENOMEM);
	}
	memset(*map_mntopts, 0, len);

	/*
	 * strlen(mapopts) + strlen(addopt) + 2 <= AUTOFS_MAXOPTSLEN,
	 * so strlen(mapopts) < AUTOFS_MAXOPTSLEN, and this copy is safe.
	 */
	strcpy(buf, mapopts);
	opts = buf;
	while ((opt = strtok_r(opts, ",", &placeholder)) != NULL) {
		opts = NULL;

		/* remove trailing and leading spaces */
		while (isspace(*opt))
			opt++;
		len = strlen(opt)-1;
		while (isspace(opt[len]))
			opt[len--] = '\0';

		/*
		 * if direct or indirect found, get rid of it, else put it
		 * back
		 */
		if ((strcmp(opt, "indirect") == 0) ||
		    (strcmp(opt, "direct") == 0))
			continue;
		/*
		 * *map_mntopts is large enough to hold a copy of mapopts,
		 * so these strcat()s are safe.
		 */
		if (*map_mntopts[0] != '\0')
			strcat(*map_mntopts, ",");
		strcat(*map_mntopts, opt);
	}

	/*
	 * Add the direct string at the end.
	 * *map_mntopts is large enough to hold a copy of mapopts, plus
	 * a comma, plus addopt, so these strcat()s are safe.
	 */
	if (*map_mntopts[0] != '\0')
		strcat(*map_mntopts,	",");
	strcat(*map_mntopts, addopt);

	return (0);
}

/*
 * parse_fsinfo(char *mapname, struct mapent *mapents)
 * parses the filesystem information stored in me->map_fsw and me->map_fswq
 * and calls appropriate filesystem parser.
 * Returns 0 or an appropriate error value.
 */
static int
parse_fsinfo(const char *mapname, struct mapent *mapents)
{
	struct mapent *me = mapents;
	char *bufp;
	char *bufq;
	int err = 0;

	while (me != NULL) {
		bufp = "";
		bufq = "";
		if (me->map_fstype == NULL) {
			/*
			 * No file system type explicitly specified.
			 * If the thing to be mounted looks like
			 * a non-NFS URL, make the type "url", otherwise
			 * make it "nfs".
			 */
			if (is_nonnfs_url(me->map_fsw, me->map_fswq))
				me->map_fstype = strdup("url");
			else
				me->map_fstype = strdup("nfs");
		}
		if (strcmp(me->map_fstype, "nfs") == 0) {
			err = parse_nfs(mapname, me, me->map_fsw,
				me->map_fswq, &bufp, &bufq);
		} else {
			err = parse_special(me, me->map_fsw, me->map_fswq,
				&bufp, &bufq, MAXPATHLEN);
		}

		if (err != 0 || *me->map_fsw != '\0' ||
		    *me->map_fswq != '\0') {
			/* sanity check */
			if (verbose)
				syslog(LOG_ERR,
				"parse_fsinfo: mount location error %s",
				    me->map_fsw);
			return (EIO);
		}

		me = me->map_next;
	}

	if (trace > 3) {
		trace_mapents("parse_fsinfo:", mapents);
	}

	return (0);
}

/*
 * Check whether the first token we see looks like a non-NFS URL or not.
 */
static int
is_nonnfs_url(fsw, fswq)
	char *fsw, *fswq;
{
	char tok1[MAXPATHLEN + 1], tok1q[MAXPATHLEN + 1];
	char *tok1p, *tok1qp;
	char c;

	if (getword(tok1, tok1q, &fsw, &fswq, ' ', sizeof (tok1)) == -1)
		return (0);	/* error, doesn't look like anything */

	tok1p = tok1;
	tok1qp = tok1q;

	/*
	 * Scan until we find a character that doesn't belong in a URL
	 * scheme.  According to RFC 3986, "Scheme names consist of a
	 * sequence of characters beginning with a letter and followed
	 * by any combination of letters, digits, plus ("+"), period ("."),
	 * or hyphen ("-")."
	 *
	 * It's irrelevant whether the characters are quoted or not;
	 * quoting doesn't mean that the character is allowed to be part
	 * of a URL.
	 */
	while ((c = *tok1p) != '\0' && isascii(c) &&
	    (isalnum(c) || c == '+' || c == '.' || c == '-')) {
		tok1p++;
		tok1qp++;
	}

	/*
	 * The character in question had better be a colon (quoted or not),
	 * otherwise this doesn't look like a URL.
	 */
	if (*tok1p != ':')
		return (0);	/* not a URL */

	/*
	 * However, if it's a colon, that doesn't prove it's a URL; we
	 * could have server:/path.  Check whether the colon is followed
	 * by two slashes (quoted or not).
	 */
	if (*(tok1p + 1) != '/' || *(tok1p + 2) != '/')
		return (0);	/* not a URL */

	/*
	 * scheme://...
	 * Probably a URL; is the scheme "nfs"?
	 */
	if (tok1p - tok1 != 3) {
		/*
		 * It's not 3 characters long, so it's not "nfs", so
		 * we have something that looks like a non-NFS URL.
		 */
		return (1);
	}

	/*
	 * It's 3 characters long; is it "nfs" (case-insensitive)?
	 */
	if (strncasecmp(tok1, "nfs", 3) != 0)
		return (1);	/* no, so probably a non-NFS URL */
	return (0);	/* yes, so not a non-NFS URL */
}

/*
 * This function parses the map entry for a nfs type file system
 * The input is the string lp (and lq) which can be one of the
 * following forms:
 * a) host[(penalty)][,host[(penalty)]]... :/directory
 * b) host[(penalty)]:/directory[ host[(penalty)]:/directory]...
 * This routine constructs a mapfs link-list for each of
 * the hosts and the corresponding file system. The list
 * is then attatched to the mapent struct passed in.
 */
static int
parse_nfs(mapname, me, fsw, fswq, lp, lq)
	struct mapent *me;
	const char *mapname;
	char *fsw, *fswq, **lp, **lq;
{
	struct mapfs *mfs, **mfsp;
	char *wlp, *wlq;
	char *hl, hostlist[1024], *hlq, hostlistq[1024];
	char hostname_and_penalty[MXHOSTNAMELEN+5];
	char *hn, *hnq, hostname[MXHOSTNAMELEN+1];
	char dirname[MAXPATHLEN+1], subdir[MAXPATHLEN+1];
	char qbuff[MAXPATHLEN+1], qbuff1[MAXPATHLEN+1];
	char pbuff[10], pbuffq[10];
	int penalty;
	char w[MAXPATHLEN];
	char wq[MAXPATHLEN];
	int host_cnt;

	mfsp = &me->map_fs;
	*mfsp = NULL;

	/*
	 * there may be more than one entry in the map list. Get the
	 * first one. Use temps to handle the word information and
	 * copy back into fsw and fswq fields when done.
	 */
	*lp = fsw;
	*lq = fswq;
	if (getword(w, wq, lp, lq, ' ', MAXPATHLEN) == -1)
		return (EIO);
	while (*w && *w != '/') {
		bool_t maybe_url;

		maybe_url = TRUE;

		wlp = w; wlq = wq;
		if (getword(hostlist, hostlistq, &wlp, &wlq, ':',
			    sizeof (hostlist)) == -1)
			return (EIO);
		if (!*hostlist) {
			syslog(LOG_ERR,
			"parse_nfs: host list empty in map %s \"%s\"",
			mapname, w);
			goto bad_entry;
		}

		if (strcmp(hostlist, "nfs") != 0)
			maybe_url = FALSE;

		if (getword(dirname, qbuff, &wlp, &wlq, ':',
					sizeof (dirname)) == -1)
			return (EIO);
		if (*dirname == '\0') {
			syslog(LOG_ERR,
			"parse_nfs: directory name empty in map %s \"%s\"",
			mapname, w);
			goto bad_entry;
		}

		if (maybe_url == TRUE && strncmp(dirname, "//", 2) != 0)
			maybe_url = FALSE;

		/*
		 * See the next block comment ("Once upon a time ...") to
		 * understand this. It turns the deprecated concept
		 * of "subdir mounts" produced some useful code for handling
		 * the possibility of a ":port#" in the URL.
		 */
		if (maybe_url == FALSE)
			*subdir = '/';
		else
			*subdir = ':';

		*qbuff = ' ';

		/*
		 * Once upon time, before autofs, there was support for
		 * "subdir mounts". The idea was to "economize" the
		 * number of mounts, so if you had a number of entries
		 * all referring to a common subdirectory, e.g.
		 *
		 *	carol    seasons:/export/home11/carol
		 *	ted	 seasons:/export/home11/ted
		 *	alice	 seasons:/export/home11/alice
		 *
		 * then you could tell the automounter to mount a
		 * common mountpoint which was delimited by the second
		 * colon:
		 *
		 *	carol    seasons:/export/home11:carol
		 *	ted	 seasons:/export/home11:ted
		 *	alice	 seasons:/export/home11:alice
		 *
		 * The automounter would mount seasons:/export/home11
		 * then for any other map entry that referenced the same
		 * directory it would build a symbolic link that
		 * appended the remainder of the path after the second
		 * colon, i.e.  once the common subdir was mounted, then
		 * other directories could be accessed just by link
		 * building - no further mounts required.
		 *
		 * In theory the "mount saving" idea sounded good. In
		 * practice the saving didn't amount to much and the
		 * symbolic links confused people because the common
		 * mountpoint had to have a pseudonym.
		 *
		 * To remain backward compatible with the existing
		 * maps, we interpret a second colon as a slash.
		 */
		if (getword(subdir+1, qbuff+1, &wlp, &wlq, ':',
				sizeof (subdir)) == -1)
			return (EIO);

		if (*(subdir+1)) {
			if (strlcat(dirname, subdir, sizeof (dirname)) >=
			    sizeof (dirname)) {
				syslog(LOG_ERR, "directory+subdirectory is longer than MAXPATHLEN");
				return (EIO);
			}
		}

		hl = hostlist; hlq = hostlistq;

		host_cnt = 0;
		for (;;) {

			if (getword(hostname_and_penalty, qbuff, &hl, &hlq, ',',
				sizeof (hostname_and_penalty)) == -1)
				return (EIO);
			if (!*hostname_and_penalty)
				break;

			host_cnt++;
			if (host_cnt > 1)
				maybe_url = FALSE;

			hn = hostname_and_penalty;
			hnq = qbuff;
			if (getword(hostname, qbuff1, &hn, &hnq, '(',
				sizeof (hostname)) == -1)
				return (EIO);
			if (hostname[0] == '\0') {
				syslog(LOG_ERR,
				"parse_nfs: host name empty in host list in map %s \"%s\"",
				mapname, w);
				goto bad_entry;
			}

			if (strcmp(hostname, hostname_and_penalty) == 0) {
				penalty = 0;
			} else {
				maybe_url = FALSE;
				hn++; hnq++;
				if (getword(pbuff, pbuffq, &hn, &hnq, ')',
					sizeof (pbuff)) == -1)
					return (EIO);
				if (!*pbuff)
					penalty = 0;
				else
					penalty = atoi(pbuff);
			}
			mfs = (struct mapfs *)malloc(sizeof (*mfs));
			if (mfs == NULL) {
				syslog(LOG_ERR,
				"parse_nfs: Memory allocation failed");
				return (EIO);
			}
			(void) memset(mfs, 0, sizeof (*mfs));
			*mfsp = mfs;
			mfsp = &mfs->mfs_next;

			if (maybe_url == TRUE) {
				char *host;
				char *path;
				char *sport;

				host = dirname+2;
				path = strchr(host, '/');
				if (path == NULL) {
					syslog(LOG_ERR,
					"parse_nfs: illegal nfs url syntax: %s",
					host);

					return (EIO);
				}
				*path = '\0';
				sport =  strchr(host, ':');

				if (sport != NULL && sport < path) {
					*sport = '\0';
					mfs->mfs_port = atoi(sport+1);

					if (mfs->mfs_port > USHRT_MAX) {
						syslog(LOG_ERR,
							"parse_nfs: invalid "
							"port number (%d) in "
							"NFS URL",
							mfs->mfs_port);

						return (EIO);
					}

				}

				path++;
				if (*path == '\0')
					path = ".";

				mfs->mfs_flags |= MFS_URL;

				mfs->mfs_host = strdup(host);
				mfs->mfs_dir = strdup(path);
			} else {
				mfs->mfs_host = strdup(hostname);
				mfs->mfs_dir = strdup(dirname);
			}

			mfs->mfs_penalty = penalty;
			if (mfs->mfs_host == NULL || mfs->mfs_dir == NULL) {
				syslog(LOG_ERR,
				"parse_nfs: Memory allocation failed");
				return (EIO);
			}
		}
		/*
		 * We check host_cnt to make sure we haven't parsed an entry
		 * with no host information.
		 */
		if (host_cnt == 0) {
			syslog(LOG_ERR,
			"parse_nfs: invalid host specified - bad entry "
			"in map %s \"%s\"",
			mapname, w);
			return (EIO);
		}
		if (getword(w, wq, lp, lq, ' ', MAXPATHLEN) == -1)
			return (EIO);
	}

	/*
	 * w and wq are constructed from the input fsw and fswq,
	 * respectively, by extracting items, so they're guaranteed
	 * to be shorter than fsw and fswq, so these copies are safe.
	 */
	strcpy(fsw, w);
	strcpy(fswq, wq);

	return (0);

bad_entry:
	return (EIO);
}

static int
parse_special(me, w, wq, lp, lq, wsize)
	struct mapent *me;
	char *w, *wq, **lp, **lq;
	int wsize;
{
	char mountname[MAXPATHLEN + 1], qbuf[MAXPATHLEN + 1];
	char *wlp, *wlq;
	struct mapfs *mfs;

	wlp = w;
	wlq = wq;
	if (getword(mountname, qbuf, &wlp, &wlq, ' ', sizeof (mountname)) == -1)
		return (EIO);
	if (mountname[0] == '\0')
		return (EIO);

	mfs = (struct mapfs *)malloc(sizeof (struct mapfs));
	if (mfs == NULL) {
		syslog(LOG_ERR, "parse_special: Memory allocation failed");
		return (EIO);
	}
	(void) memset(mfs, 0, sizeof (*mfs));

	/*
	 * A device name that begins with a slash could
	 * be confused with a mountpoint path, hence use
	 * a colon to escape a device string that begins
	 * with a slash, e.g.
	 *
	 *	foo  -ro  /bar  foo:/bar
	 * and
	 *	foo  -ro  /dev/sr0
	 *
	 * would confuse the parser.  The second instance
	 * must use a colon:
	 *
	 *	foo  -ro  :/dev/sr0
	 */
	mfs->mfs_dir = strdup(&mountname[mountname[0] == ':']);
	if (mfs->mfs_dir == NULL) {
		syslog(LOG_ERR, "parse_special: Memory allocation failed");
		return (EIO);
	}
	me->map_fs = mfs;
	if (getword(w, wq, lp, lq, ' ', wsize) == -1)
		return (EIO);
	return (0);
}

/*
 * get_dir_from_path(char *dir, char **path, int dirsz)
 * gets the directory name dir from path for max string of length dirsz.
 * A modification of the getword routine. Assumes the delimiter is '/'
 * and that excess /'s are redundant.
 * Returns 0 or EIO
 */
static int
get_dir_from_path(char *dir, const char **path, int dirsz)
{
	char *tmp = dir;
	int count = dirsz;

	if (dirsz <= 0) {
		if (verbose)
			syslog(LOG_ERR,
			"get_dir_from_path: invalid directory size %d", dirsz);
		return (EIO);
	}

	/* get rid of leading /'s in path */
	while (**path == '/')
		(*path)++;

	/* now at a word or at the end of path */
	while ((**path) && ((**path) != '/')) {
		if (--count <= 0) {
			*tmp = '\0';
			syslog(LOG_ERR,
			"get_dir_from_path: max pathlength exceeded %d", dirsz);
			return (EIO);
		}
		*dir++ = *(*path)++;
	}

	*dir = '\0';

	/* get rid of trailing /'s in path */
	while (**path == '/')
		(*path)++;

	return (0);
}

/*
 * alloc_hiernode(hiernode **newnode, char *dirname)
 * allocates a new hiernode corresponding to a new directory entry
 * in the hierarchical structure, and passes a pointer to it back
 * to the calling program.
 * Returns 0 or appropriate error value.
 */
static int
alloc_hiernode(hiernode **newnode, char *dirname)
{
	if ((*newnode = (hiernode *)malloc(sizeof (hiernode))) == NULL) {
		syslog(LOG_ERR,	"alloc_hiernode: Memory allocation failed");
		return (ENOMEM);
	}

	memset(((char *)*newnode), 0, sizeof (hiernode));
	CHECK_STRCPY((*newnode)->dirname, dirname, sizeof ((*newnode)->dirname));
	return (0);
}

/*
 * free_hiernode(hiernode *node)
 * frees the allocated hiernode given the head of the structure
 * recursively calls itself until it frees entire structure.
 * Returns nothing.
 */
static void
free_hiernode(hiernode *node)
{
	hiernode *currnode = node;
	hiernode *prevnode = NULL;

	while (currnode != NULL) {
		if (currnode->subdir != NULL)
			free_hiernode(currnode->subdir);
		prevnode = currnode;
		currnode = currnode->leveldir;
		free((void*)prevnode);
	}
}

/*
 * free_mapent(struct mapent *)
 * free the mapentry and its fields
 */
void
free_mapent(me)
	struct mapent *me;
{
	struct mapfs *mfs;
	struct mapent *m;

	while (me) {
		while (me->map_fs) {
			mfs = me->map_fs;
			if (mfs->mfs_host)
				free(mfs->mfs_host);
			if (mfs->mfs_dir)
				free(mfs->mfs_dir);
			if (mfs->mfs_args)
				free(mfs->mfs_args);
			me->map_fs = mfs->mfs_next;
			free((char *)mfs);
		}

		if (me->map_root)
			free(me->map_root);
		if (me->map_mntpnt)
			free(me->map_mntpnt);
		if (me->map_mntopts)
			free(me->map_mntopts);
		if (me->map_fstype)
			free(me->map_fstype);
		if (me->map_mounter)
			free(me->map_mounter);
		if (me->map_fsw)
			free(me->map_fsw);
		if (me->map_fswq)
			free(me->map_fswq);

		m = me;
		me = me->map_next;
		free((char *)m);
	}
}

/*
 * trace_mapents(struct mapent *mapents)
 * traces through the mapentry structure and prints it element by element
 * returns nothing
 */
static void
trace_mapents(char *s, struct mapent *mapents)
{
	struct mapfs  *mfs;
	struct mapent *me;

	trace_prt(1, "\n\t%s\n", s);
	for (me = mapents; me; me = me->map_next) {
		trace_prt(1, "  (%s,%s)\t %s%s -%s\n",
			me->map_fstype ? me->map_fstype : "",
			me->map_mounter ? me->map_mounter : "",
			me->map_root  ? me->map_root : "",
			me->map_mntpnt ? me->map_mntpnt : "",
			me->map_mntopts ? me->map_mntopts : "");
		for (mfs = me->map_fs; mfs; mfs = mfs->mfs_next)
			trace_prt(0, "\t\t%s:%s\n",
				mfs->mfs_host ? mfs->mfs_host: "",
				mfs->mfs_dir ? mfs->mfs_dir : "");

		trace_prt(1, "\tme->map_fsw=%s\n",
			me->map_fsw ? me->map_fsw:"");
		trace_prt(1, "\tme->map_fswq=%s\n",
			me->map_fswq ? me->map_fswq:"");
		trace_prt(1, "\t mntlevel=%d\t%s\t%s err=%d\n",
			me->map_mntlevel,
			me->map_modified ? "modify=TRUE":"modify=FALSE",
			me->map_faked ? "faked=TRUE":"faked=FALSE",
			me->map_err);
	}
}

/*
 * trace_hierarchy(hiernode *node)
 * traces the allocated hiernode given the head of the structure
 * recursively calls itself until it traces entire structure.
 * the first call made at the root is made with a zero level.
 * nodelevel is simply used to print tab and make the tracing clean.
 * Returns nothing.
 */
static void
trace_hierarchy(hiernode *node, int nodelevel)
{
	hiernode *currnode = node;
	int i;

	while (currnode != NULL) {
		if (currnode->subdir != NULL) {
			for (i = 0; i < nodelevel; i++)
				trace_prt(0, "\t");
			trace_prt(0, "\t(%s, ",
				currnode->dirname ? currnode->dirname :"");
			if (currnode->mapent) {
				trace_prt(0, "%d, %s)\n",
					currnode->mapent->map_mntlevel,
					currnode->mapent->map_mntopts ?
					currnode->mapent->map_mntopts:"");
			}
			else
				trace_prt(0, " ,)\n");
			nodelevel++;
			trace_hierarchy(currnode->subdir, nodelevel);
		} else {
			for (i = 0; i < nodelevel; i++)
				trace_prt(0, "\t");
			trace_prt(0, "\t(%s, ",
				currnode->dirname ? currnode->dirname :"");
			if (currnode->mapent) {
				trace_prt(0, "%d, %s)\n",
					currnode->mapent->map_mntlevel,
					currnode->mapent->map_mntopts ?
					currnode->mapent->map_mntopts:"");
			}
			else
				trace_prt(0, ", )\n");
		}
		currnode = currnode->leveldir;
	}
}

static const struct mntopt mopts_vers[] = {
	MOPT_VERS,
	{ NULL,		0, 0, 0 }
};

static int
clnt_stat_to_errno(enum clnt_stat clnt_stat)
{
	switch (clnt_stat) {

	case RPC_UNKNOWNHOST:
		return (ENOENT);	/* no such host = no such directory */

	case RPC_TIMEDOUT:
		return (ETIMEDOUT);

	default:
		return (EIO);
	}
}

/*
 * mapopts must be at most MAXOPTSLEN bytes long (including the null
 * terminator).
 */
struct mapent *
do_mapent_hosts(mapopts, host, isdirect, err)
	const char *mapopts, *host;
	uint_t isdirect;
	int *err;
{
	CLIENT *cl;
	struct mapent *me, *ms = NULL, *mp;
	struct mapfs *mfs;
	struct exportnode *ex = NULL;
	struct exportnode *exlist = NULL, *texlist, **texp, *exnext;
	struct timeval timeout;
	enum clnt_stat clnt_stat;
	char entryopts[MAXOPTSLEN];
	char fstype[MAX_FSLEN], mounter[MAX_FSLEN];
	size_t exlen;
	int duplicate;
	mntoptparse_t mop;
	int flags;
	int altflags;
	long optval;
	rpcvers_t nfsvers;	/* version in map options, 0 if not there */
	rpcvers_t vers, versmin; /* used to negotiate nfs vers in pingnfs() */
	int retries, delay;

	if (trace > 1)
		trace_prt(1, "  do_mapent_hosts: host %s\n", host);

	/*
	 * Check whether the host has a name that begins with ".";
	 * if so, it's not a valid host name, and we return ENOENT.
	 * That way, we avoid doing host name lookups for various
	 * dot-file names, e.g. ".localized" and ".DS_Store".
	 */
	if (host[0] == '.') {
		*err = ENOENT;
		return ((struct mapent *)NULL);
	}

	/*
	 * XXX - this appears to assume that you can get *all* the
	 * file systems - or, at least, all the local file systems -
	 * from this machine simply by mounting / with the loopback
	 * file system.  We don't yet have a loopback file system,
	 * so we have to mount individual NFS exports.
	 */
#ifdef HAVE_LOFS
	/* check for special case: host is me */

	if (self_check(host)) {
		ms = (struct mapent *)malloc(sizeof (*ms));
		if (ms == NULL)
			goto alloc_failed;
		(void) memset((char *)ms, 0, sizeof (*ms));
		CHECK_STRCPY(fstype, MNTTYPE_NFS, sizeof (fstype));
		if (!get_opts(mapopts, entryopts, fstype, sizeof (fstype), NULL))
			goto fstype_too_long;
		ms->map_mntopts = strdup(entryopts);
		if (ms->map_mntopts == NULL)
			goto alloc_failed;
		ms->map_mounter = strdup(fstype);
		if (ms->map_mounter == NULL)
			goto alloc_failed;
		ms->map_fstype = strdup(MNTTYPE_NFS);
		if (ms->map_fstype == NULL)
			goto alloc_failed;

		if (isdirect)
			ms->map_root = strdup("");
		else
			ms->map_root = strprefix_slash(host);
		if (ms->map_root == NULL)
			goto alloc_failed;
		ms->map_mntpnt = strdup("");
		if (ms->map_mntpnt == NULL)
			goto alloc_failed;
		mfs = (struct mapfs *)malloc(sizeof (*mfs));
		if (mfs == NULL)
			goto alloc_failed;
		(void) memset((char *)mfs, 0, sizeof (*mfs));
		ms->map_fs = mfs;
		mfs->mfs_host = strdup(host);
		if (mfs->mfs_host == NULL)
			goto alloc_failed;
		mfs->mfs_dir  = strdup("/");
		if (mfs->mfs_dir == NULL)
			goto alloc_failed;

		/* initialize mntlevel and modify */
		ms->map_mntlevel = -1;
		ms->map_modified = FALSE;
		ms->map_faked = FALSE;

		if (trace > 1)
			trace_prt(1,
			"  do_mapent_hosts: self-host %s OK\n", host);

		*err = 0;
		return (ms);
	}
#endif

	/*
	 * Call pingnfs. Note that we can't have replicated hosts in /net.
	 * XXX - we would like to avoid duplicating the across the wire calls
	 * made here in nfsmount(). The pingnfs cache should help avoid it.
	 */
	flags = altflags = 0;
	getmnt_silent = 1;
	mop = getmntopts(mapopts, mopts_vers, &flags, &altflags);
	if (mop == NULL) {
		syslog(LOG_ERR, "Couldn't parse mount options \"%s\" for %s: %m",
		    mapopts, host);
		*err = EIO;
		return ((struct mapent *)NULL);
	}
	if (altflags & (NFS_MNT_VERS|NFS_MNT_NFSVERS)) {
		optval = get_nfs_vers(mop, altflags);
		if (optval == 0) {
			syslog(LOG_ERR, "Invalid NFS version number for %s", host);
			freemntopts(mop);
			*err = EIO;
			return ((struct mapent *)NULL);
		}
		nfsvers = (rpcvers_t)optval;
	} else
		nfsvers = 0;
	freemntopts(mop);
	if (set_versrange(nfsvers, &vers, &versmin) != 0) {
		syslog(LOG_ERR, "Incorrect NFS version specified for %s", host);
		*err = EIO;
		return ((struct mapent *)NULL);
	}
	clnt_stat = pingnfs(host, &vers, versmin, 0, NULL, NULL);
	if (clnt_stat != RPC_SUCCESS) {
		*err = clnt_stat_to_errno(clnt_stat);
		return ((struct mapent *)NULL);
	}

	retries = get_retry(mapopts);
	delay = INITDELAY;
retry:
	/* get export list of host */
	cl = clnt_create((char *)host, MOUNTPROG, MOUNTVERS, "tcp");
	if (cl == NULL) {
		cl = clnt_create((char *)host, MOUNTPROG, MOUNTVERS, "udp");
		if (cl == NULL) {
			syslog(LOG_ERR,
			"do_mapent_hosts: %s %s", host, clnt_spcreateerror(""));
			*err = clnt_stat_to_errno(rpc_createerr.cf_stat);
			return ((struct mapent *)NULL);
		}

	}
#ifdef MALLOC_DEBUG
	add_alloc("CLNT_HANDLE", cl, 0, __FILE__, __LINE__);
	add_alloc("AUTH_HANDLE", cl->cl_auth, 0,
		__FILE__, __LINE__);
#endif

	timeout.tv_usec = 0;
	timeout.tv_sec  = 25;
	if ((clnt_stat = clnt_call(cl, MOUNTPROC_EXPORT, (xdrproc_t)xdr_void, 0,
		(xdrproc_t)xdr_exports, (caddr_t)&ex, timeout)) != RPC_SUCCESS) {

		if (retries-- > 0) {
			clnt_destroy(cl);
			DO_DELAY(delay);
			goto retry;
		}

		syslog(LOG_ERR,
			"do_mapent_hosts: %s: export list: %s",
			host, clnt_sperrno(clnt_stat));
#ifdef MALLOC_DEBUG
		drop_alloc("CLNT_HANDLE", cl, __FILE__, __LINE__);
		drop_alloc("AUTH_HANDLE", cl->cl_auth,
			__FILE__, __LINE__);
#endif
		clnt_destroy(cl);
		*err = clnt_stat_to_errno(clnt_stat);
		return ((struct mapent *)NULL);
	}

#ifdef MALLOC_DEBUG
	drop_alloc("CLNT_HANDLE", cl, __FILE__, __LINE__);
	drop_alloc("AUTH_HANDLE", cl->cl_auth,
		__FILE__, __LINE__);
#endif
	clnt_destroy(cl);

	if (ex == NULL) {
		if (trace > 1)
			trace_prt(1, "  getmapent_hosts: null export list\n");
		*err = 0;
		return ((struct mapent *)NULL);
	}

	/* now sort by length of names - to get mount order right */
	exlist = ex;
	texlist = NULL;
#ifdef lint
	exnext = NULL;
#endif
	for (; ex; ex = exnext) {
		exnext = ex->ex_next;
		exlen = strlen(ex->ex_dir);
		duplicate = 0;
		for (texp = &texlist; *texp; texp = &((*texp)->ex_next)) {
			if (exlen < strlen((*texp)->ex_dir))
				break;
			duplicate = (strcmp(ex->ex_dir, (*texp)->ex_dir) == 0);
			if (duplicate) {
				/* disregard duplicate entry */
				freeex_ent(ex);
				break;
			}
		}
		if (!duplicate) {
			ex->ex_next = *texp;
			*texp = ex;
		}
	}
	exlist = texlist;

	/*
	 * The following ugly chunk of code crept in as
	 * a result of cachefs.  If it's a cachefs mount
	 * of an nfs filesystem, then have it handled as
	 * an nfs mount but have cachefs do the mount.
	 */
	CHECK_STRCPY(fstype, MNTTYPE_NFS, sizeof (fstype));
	if (!get_opts(mapopts, entryopts, fstype, sizeof (fstype), NULL))
		goto fstype_too_long;
	/* mounter and fstype are the same size */
	(void) strcpy(mounter, fstype);
#ifdef MNTTYPE_CACHEFS
	if (strcmp(fstype, MNTTYPE_CACHEFS) == 0) {
		struct mnttab m;
		char *p;

		m.mnt_mntopts = entryopts;
		if ((p = hasmntopt(&m, "backfstype")) != NULL) {
			int len = strlen(MNTTYPE_NFS);

			p += 11;
			if (strncmp(p, MNTTYPE_NFS, len) == 0 &&
			    (p[len] == '\0' || p[len] == ',')) {
				/*
				 * Cached nfs mount
				 */
				CHECK_STRCPY(fstype, MNTTYPE_NFS, sizeof (fstype));
				CHECK_STRCPY(mounter, MNTTYPE_CACHEFS, sizeof (mounter));
			}
		}
	}
#endif

	/* Now create a mapent from the export list */
	ms = NULL;
	me = NULL;

	for (ex = exlist; ex; ex = ex->ex_next) {
		mp = me;
		me = (struct mapent *)malloc(sizeof (*me));
		if (me == NULL)
			goto alloc_failed;
		(void) memset((char *)me, 0, sizeof (*me));

		if (ms == NULL)
			ms = me;
		else
			mp->map_next = me;

		if (isdirect)
			me->map_root = strdup("");
		else
			me->map_root = strprefix_slash(host);
		if (me->map_root == NULL)
			goto alloc_failed;

		if (strcmp(ex->ex_dir, "/") != 0) {
			if (*(ex->ex_dir) != '/')
				me->map_mntpnt = strprefix_slash(ex->ex_dir);
			else
				me->map_mntpnt = strdup(ex->ex_dir);
		} else
			me->map_mntpnt = strdup("");
		if (me->map_mntpnt == NULL)
			goto alloc_failed;

		me->map_fstype = strdup(fstype);
		if (me->map_fstype == NULL)
			goto alloc_failed;
		me->map_mounter = strdup(mounter);
		if (me->map_mounter == NULL)
			goto alloc_failed;
		me->map_mntopts = strdup(entryopts);
		if (me->map_mntopts == NULL)
			goto alloc_failed;

		mfs = (struct mapfs *)malloc(sizeof (*mfs));
		if (mfs == NULL)
			goto alloc_failed;
		(void) memset((char *)mfs, 0, sizeof (*mfs));
		me->map_fs = mfs;
		mfs->mfs_host = strdup(host);
		if (mfs->mfs_host == NULL)
			goto alloc_failed;
		mfs->mfs_dir = strdup(ex->ex_dir);
		if (mfs->mfs_dir == NULL)
			goto alloc_failed;

		/* initialize mntlevel and modify values */
		me->map_mntlevel = -1;
		me->map_modified = FALSE;
		me->map_faked = FALSE;
	}
	freeex(exlist);

	if (trace > 1)
		trace_prt(1, "  do_mapent_hosts: host %s OK\n", host);

	*err = 0;
	return (ms);

alloc_failed:
	syslog(LOG_ERR, "do_mapent_hosts: Memory allocation failed");
	free_mapent(ms);
	freeex(exlist);
	*err = ENOMEM;
	return ((struct mapent *)NULL);

fstype_too_long:
	syslog(LOG_ERR, "do_mapent_hosts: File system type is too long");
	free_mapent(ms);
	freeex(exlist);
	*err = EIO;
	return ((struct mapent *)NULL);
}


static void
freeex_ent(ex)
	struct exportnode *ex;
{
	struct groupnode *group, *tmpgroups;

	free(ex->ex_dir);
	group = ex->ex_groups;
	while (group) {
		free(group->gr_name);
		tmpgroups = group->gr_next;
		free((char *)group);
		group = tmpgroups;
	}
	free((char *)ex);
}

static void
freeex(ex)
	struct exportnode *ex;
{
	struct exportnode *tmpex;

	while (ex) {
		tmpex = ex->ex_next;
		freeex_ent(ex);
		ex = tmpex;
	}
}

struct create_mapent_args {
	uint_t		isdirect;
	const char	*host;
	struct mapent	*ms;
	struct mapent	*me;
};

static int
create_mapent(struct fstabnode *fst, void *arg)
{
	struct create_mapent_args *args = arg;
	struct mapent *me, *mp;
	struct mapfs *mfs;

	mp = args->me;
	me = (struct mapent *)malloc(sizeof (*me));
	if (me == NULL)
		goto alloc_failed;
	(void) memset((char *)me, 0, sizeof (*me));
	args->me = me;

	if (args->isdirect)
		me->map_root = strdup("");
	else
		me->map_root = strprefix_slash(args->host);
	if (me->map_root == NULL)
		goto alloc_failed;

	if (strcmp(fst->fst_dir, "/") == 0)
		me->map_mntpnt = strdup("");
	else {
		if (*(fst->fst_dir) != '/')
			me->map_mntpnt = strprefix_slash(fst->fst_dir);
		else
			me->map_mntpnt = strdup(fst->fst_dir);
	}
	if (me->map_mntpnt == NULL)
		goto alloc_failed;

	me->map_fstype = strdup(fst->fst_vfstype);
	if (me->map_fstype == NULL)
		goto alloc_failed;
	me->map_mounter = strdup(fst->fst_vfstype);
	if (me->map_mounter == NULL)
		goto alloc_failed;
	me->map_mntopts = strdup(fst->fst_mntops);
	if (me->map_mntopts == NULL)
		goto alloc_failed;

	mfs = (struct mapfs *)malloc(sizeof (*mfs));
	if (mfs == NULL)
		goto alloc_failed;
	(void) memset((char *)mfs, 0, sizeof (*mfs));
	me->map_fs = mfs;
	mfs->mfs_host = strdup(args->host);
	if (mfs->mfs_host == NULL)
		goto alloc_failed;
	if (strcmp(fst->fst_vfstype, "url") == 0) {
		/* Use the URL. */
		mfs->mfs_dir = strdup(fst->fst_url);
	} else
		mfs->mfs_dir = strdup(fst->fst_dir);
	if (mfs->mfs_dir == NULL)
		goto alloc_failed;

	/* initialize mntlevel and modify values */
	me->map_mntlevel = -1;
	me->map_modified = FALSE;
	me->map_faked = FALSE;

	if (args->ms == NULL)
		args->ms = me;
	else
		mp->map_next = me;

	return (0);

alloc_failed:
	free_mapent(me);
	return (ENOMEM);
}

/*
 * mapopts must be at most MAXOPTSLEN bytes long (including the null
 * terminator).
 */
struct mapent *
do_mapent_fstab(mapopts, host, isdirect, node_type, err)
	const char *mapopts, *host;
	uint_t isdirect;
	int *node_type;
	int *err;
{
	struct mapent *ms;
	struct mapfs *mfs;
	char entryopts[MAXOPTSLEN];
	char fstype[MAX_FSLEN];
	struct create_mapent_args args;

	if (trace > 1)
		trace_prt(1, "  do_mapent_fstab: host %s\n", host);

	/*
	 * Check for special case: host is me.
	 * We check based on the name, as was done for the selflink
	 * in the old automounter; we don't check based on the
	 * IP address, as that's expensive (it requires that we
	 * resolve the IP address of the server, which is really
	 * expensive the first time it's done, as it's likely not
	 * to be in the DNS resolver's cache).  The host_is_us()
	 * check handles multi-homed hosts (it checks against the
	 * names corresponding to *all* this host's IP addresses)
	 * and handles local names (it checks against this host's
	 * .local name, if it has one).
	 */

	if (host_is_us(host, strlen(host))) {
		ms = (struct mapent *)malloc(sizeof (*ms));
		if (ms == NULL)
			goto alloc_failed;
		(void) memset((char *)ms, 0, sizeof (*ms));
		CHECK_STRCPY(fstype, MNTTYPE_NFS, sizeof (fstype));
		if (!get_opts(mapopts, entryopts, fstype, sizeof (fstype), NULL))
			goto fstype_too_long;
		ms->map_mntopts = strdup(entryopts);
		if (ms->map_mntopts == NULL)
			goto alloc_failed;
		ms->map_mounter = strdup(fstype);
		if (ms->map_mounter == NULL)
			goto alloc_failed;
		ms->map_fstype = strdup(MNTTYPE_NFS);
		if (ms->map_fstype == NULL)
			goto alloc_failed;

		if (isdirect)
			ms->map_root = strdup("");
		else
			ms->map_root = strprefix_slash(host);
		if (ms->map_root == NULL)
			goto alloc_failed;
		ms->map_mntpnt = strdup("");
		if (ms->map_mntpnt == NULL)
			goto alloc_failed;
		mfs = (struct mapfs *)malloc(sizeof (*mfs));
		if (mfs == NULL)
			goto alloc_failed;
		(void) memset((char *)mfs, 0, sizeof (*mfs));
		ms->map_fs = mfs;
		mfs->mfs_host = strdup(host);
		if (mfs->mfs_host == NULL)
			goto alloc_failed;
		mfs->mfs_dir  = strdup("/");
		if (mfs->mfs_dir == NULL)
			goto alloc_failed;

		/* initialize mntlevel and modify */
		ms->map_mntlevel = -1;
		ms->map_modified = FALSE;
		ms->map_faked = FALSE;

		if (trace > 1)
			trace_prt(1,
			"  do_mapent_fstab: self-host %s OK\n", host);

		/*
		 * This is a symlink to /.
		 */
		if (node_type != NULL)
			*node_type = NT_SYMLINK;

		*err = 0;
		return (ms);
	}

	/*
	 * Create a mapent from the list of fstab map entries
	 * that refer to host.
	 */
	args.isdirect = isdirect;
	args.host = host;
	args.ms = NULL;
	args.me = NULL;
	*err = fstab_process_host(host, create_mapent, &args);
	if (*err != 0) {
		/*
		 * -1 means "there's no entry for that host".
		 */
		if (*err == -1)
			*err = ENOENT;
		else
			syslog(LOG_ERR, "do_mapent_fstab: Memory allocation failed");
		return ((struct mapent *)NULL);
	}

	if (trace > 1)
		trace_prt(1, "  do_mapent_fstab: host %s OK\n", host);

	return (args.ms);

alloc_failed:
	syslog(LOG_ERR, "do_mapent_fstab: Memory allocation failed");
	free_mapent(ms);
	*err = ENOMEM;
	return ((struct mapent *)NULL);

fstype_too_long:
	syslog(LOG_ERR, "do_mapent_hosts: File system type is too long");
	free_mapent(ms);
	*err = EIO;
	return ((struct mapent *)NULL);
}


struct mapent *
do_mapent_static(key, isdirect, err)
	const char *key;
	uint_t isdirect;
	int *err;
{
	struct staticmap *static_ent;
	struct mapent *me;
	struct mapfs *mfs;

	/*
	 * Look up the fstab entry with the specified key
	 * as its mount point.
	 */
	static_ent = get_staticmap_entry(key);
	if (static_ent == NULL) {
		*err = ENOENT;
		return (NULL);
	}

	if ((me = (struct mapent *)malloc(sizeof (*me))) == NULL)
		goto alloc_failed;
	memset((char *)me, 0, sizeof (*me));
	/*
	 * direct maps get an empty string as root - to be filled
	 * by the entire path later. Indirect maps get /key as the
	 * map root.
	 */
	if (isdirect)
		me->map_root = strdup("");
	else
		me->map_root = strprefix_slash(key);
	if (me->map_root == NULL)
		goto alloc_failed;

	me->map_mntpnt = strdup("");
	if (me->map_mntpnt == NULL)
		goto alloc_failed;
	me->map_fstype = strdup(static_ent->vfstype);
	if (me->map_fstype == NULL)
		goto alloc_failed;
	me->map_mounter = strdup(static_ent->vfstype);
	if (me->map_mounter == NULL)
		goto alloc_failed;
	me->map_mntopts = strdup(static_ent->mntops);
	if (me->map_mntopts == NULL)
		goto alloc_failed;
	me->map_fsw = strdup("");
	if (me->map_fsw == NULL)
		goto alloc_failed;
	me->map_fswq = strdup("");
	if (me->map_fswq == NULL)
		goto alloc_failed;

	/* initialize flags */
	me->map_mntlevel = -1;
	me->map_modified = FALSE;
	me->map_faked = FALSE;
	me->map_err = MAPENT_NOERR;

	/* Add the one and only mapfs entry. */
	mfs = (struct mapfs *)malloc(sizeof (*mfs));
	if (mfs == NULL)
		goto alloc_failed;
	(void) memset((char *)mfs, 0, sizeof (*mfs));
	me->map_fs = mfs;
	mfs->mfs_host = strdup(static_ent->host);
	if (mfs->mfs_host == NULL)
		goto alloc_failed;
	mfs->mfs_dir = strdup(static_ent->spec);
	if (mfs->mfs_dir == NULL)
		goto alloc_failed;

	me->map_next = NULL;

	release_staticmap_entry(static_ent);
	*err = 0;
	return (me);

alloc_failed:
	release_staticmap_entry(static_ent);
	syslog(LOG_ERR, "do_mapent_static: Memory allocation failed");
	free_mapent(me);
	*err = ENOMEM;
	return (NULL);
}

static const char uatfs_err[] = "submount under fstype=autofs not supported";
/*
 * dump_mapent_err(struct mapent *me, const char *key, const char *mapname)
 * syslog appropriate error in mapentries.
 */
static void dump_mapent_err(struct mapent *me, const char *key,
			const char *mapname)
{
	switch (me->map_err) {
	case MAPENT_NOERR:
		if (verbose)
			syslog(LOG_ERR,
			"map=%s key=%s mntpnt=%s: no error",
			mapname, key, me->map_mntpnt);
		break;
	case MAPENT_UATFS:
		syslog(LOG_ERR,
		"mountpoint %s in map %s key %s not mounted: %s",
		    me->map_mntpnt, mapname, key, uatfs_err);
		break;
	default:
		if (verbose)
			syslog(LOG_ERR,
			"map=%s key=%s mntpnt=%s: unknown mapentry error",
			mapname, key, me->map_mntpnt);
	}
}
