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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Portions Copyright 2007 Apple Inc.
 *
 * $Id$
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include <pthread.h>
#include <fstab.h>
#include <errno.h>
#include <assert.h>
#include <mntopts.h>
#include "autofs.h"
#include "automount.h"
#include "auto_mntopts.h"

#define MIN_CACHE_TIME	30	// min cache time for fstab cache (sec)

/*
 * Structure for a server host in fstab.
 */
struct fstabhost {
	char	*name;			/* name of the host */
	struct fstabnode *fstab_ents;	/* fstab entries for that host */
	struct fstabhost *next;		/* next entry in hash table bucket */
};

#define HASHTABLESIZE	251

/*
 * Hash table of hosts with at least one "net" entry.
 */
static struct fstabhost *fstab_hashtable[HASHTABLESIZE];

/*
 * Hash table of static map entries.
 */
static struct staticmap *static_hashtable[HASHTABLESIZE];

/*
 * Read/write lock on the cache.
 */
static pthread_rwlock_t fstab_cache_lock = PTHREAD_RWLOCK_INITIALIZER;

static time_t min_cachetime;	// Min time that the cache is valid
static time_t max_cachetime;	// Max time that the cache is valid

static int
process_fstab_mntopts(struct fstab *fs, char **mntops_outp, char **url)
{
	char *mntops_out;
	size_t optlen;
	char *p;

	/*
	 * Remove "net", "bg", and "fg" from the mount options;
	 * "net" is irrelevant, and the automounter can't mount
	 * stuff in the background - it's not supposed to return
	 * until the mount has succeeded or failed - so "bg"
	 * should be ignored and "fg" is irrelevant.
	 *
	 * If "url==" appears, extract the URL and return it through
	 * "*url", and remove it from the mount options.
	 *
	 * If "fs->fs_type" isn't a null string, it's either "rw" or "ro";
	 * add it to the end of the list of options.
	 */
	optlen = strlen(fs->fs_mntops) + strlen(fs->fs_type) + 1 + 1;
	if (optlen > MAXOPTSLEN)
		return (ENAMETOOLONG);
	mntops_out = (char *)malloc(optlen);
	if (mntops_out == NULL)
		return (ENOMEM);
	strcpy(mntops_out, "");

	/*
	 * Copy over mount options, except for "net", "bg", "fg", or
	 * "url=="; extract the URL from "url==" and return it through
	 * "*url".
	 */
	*url = NULL;	/* haven't seen it yet */
	while ((p = strsep(&fs->fs_mntops, ",")) != NULL) {
		if (strcmp(p, "net") == 0 || strcmp(p, "bg") == 0 ||
		    strcmp(p, "bg") == 0)
			continue;

		if (strncmp(p, "url==", 5) == 0) {
			/*
			 * Extract the URL.
			 */
			if (*url != NULL)
				free(*url);
			*url = strdup(p + 5);
			if (*url == NULL) {
				free(mntops_out);
				return (ENOMEM);
			}
			continue;	/* don't add it to the mount options */
		}

		if (mntops_out[0] != '\0') {
			/*
			 * We already have mount options; add
			 * a comma before this one.
			 */
			CHECK_STRCAT(mntops_out, ",", optlen);
		}
		CHECK_STRCAT(mntops_out, p, optlen);
	}
	if (fs->fs_type[0] != '\0') {
		/*
		 * We have a type, which is either "rw", "ro", or nothing.
		 * If it's "rw" or "ro", add that to the end of the
		 * options list, just as the old automounter did.
		 */
		if (mntops_out[0] != '\0') {
			/*
			 * We already have mount options; add
			 * a comma before this one.
			 */
			CHECK_STRCAT(mntops_out, ",", optlen);
		}
		CHECK_STRCAT(mntops_out, fs->fs_type, optlen);
	}
	*mntops_outp = mntops_out;
	return (0);
}

static void
freefst_ent(fst)
	struct fstabnode *fst;
{
	if (fst->fst_dir != NULL)
		free(fst->fst_dir);
	if (fst->fst_vfstype != NULL)
		free(fst->fst_vfstype);
	if (fst->fst_mntops != NULL)
		free(fst->fst_mntops);
	if (fst->fst_url != NULL)
		free(fst->fst_url);
	free((char *)fst);
}

static void
freefst_list(fst)
	struct fstabnode *fst;
{
	struct fstabnode *tmpfst;

	while (fst) {
		tmpfst = fst->fst_next;
		freefst_ent(fst);
		fst = tmpfst;
	}
}

static struct fstabhost *
find_host_entry(const char *host, struct fstabhost ***bucketp)
{
	size_t hash;
	const unsigned char *hashp;
	unsigned char c;
	struct fstabhost **bucket;
	struct fstabhost *hostent;

	/*
	 * Cheesy hash function - just add all the characters in the
	 * host name together, after lower-casing all upper-case
	 * letters, and take it mod HASHTABLESIZE.
	 */
	hash = 0;
	for (hashp = (const unsigned char *)host; (c = *hashp) != '\0'; hashp++)
		hash += tolower(c);
	bucket = &fstab_hashtable[hash % HASHTABLESIZE];
	if (bucketp != NULL)
		*bucketp = bucket;
	for (hostent = *bucket; hostent != NULL; hostent = hostent->next) {
		if (strcasecmp(hostent->name, host) == 0)
			return (hostent);
	}
	return NULL;
}

static struct staticmap *
find_staticmap_entry(const char *dir, struct staticmap ***bucketp)
{
	size_t hash;
	const unsigned char *hashp;
	unsigned char c;
	struct staticmap **bucket;
	struct staticmap *staticent;

	/*
	 * Cheesy hash function - just add all the characters in the
	 * directory name together, and take it mod HASHTABLESIZE.
	 */
	hash = 0;
	for (hashp = (const unsigned char *)dir; (c = *hashp) != '\0'; hashp++)
		hash += c;
	bucket = &static_hashtable[hash % HASHTABLESIZE];
	if (bucketp != NULL)
		*bucketp = bucket;
	for (staticent = *bucket; staticent != NULL; staticent = staticent->next) {
		if (strcmp(staticent->dir, dir) == 0)
			return (staticent);
	}
	return NULL;
}

/*
 * This assumes that a write lock on the fstab cache lock is held.
 */
static void
clean_hashtables(void)
{
	int i;
	struct fstabhost *host_ent, *next_host_ent;
	struct staticmap *static_ent, *next_static_ent;

	for (i = 0; i < HASHTABLESIZE; i++) {
		for (host_ent = fstab_hashtable[i]; host_ent != NULL;
		    host_ent = next_host_ent) {
			next_host_ent = host_ent->next;
			free(host_ent->name);
			freefst_list(host_ent->fstab_ents);
			free(host_ent);
		}
		fstab_hashtable[i] = NULL;
	}

	for (i = 0; i < HASHTABLESIZE; i++) {
		for (static_ent = static_hashtable[i]; static_ent != NULL;
		    static_ent = next_static_ent) {
			next_static_ent = static_ent->next;
			free(static_ent->dir);
			free(static_ent->vfstype);
			free(static_ent->mntops);
			free(static_ent->host);
			free(static_ent->spec);
			free(static_ent);
		}
		static_hashtable[i] = NULL;
	}
}

static void
sort_fstab_entries(void)
{
	int i;
	struct fstabhost *host_ent;
	struct fstabnode *fst = NULL;
	struct fstabnode *tfstlist, **tfstp, *fstnext;
	size_t fstlen;
	int duplicate;

	for (i = 0; i < HASHTABLESIZE; i++) {
		for (host_ent = fstab_hashtable[i]; host_ent != NULL;
		    host_ent = host_ent->next) {
			tfstlist = NULL;
			for (fst = host_ent->fstab_ents; fst; fst = fstnext) {
				fstnext = fst->fst_next;
				fstlen = strlen(fst->fst_dir);
				duplicate = 0;
				for (tfstp = &tfstlist; *tfstp;
				    tfstp = &((*tfstp)->fst_next)) {
					if (fstlen < strlen((*tfstp)->fst_dir))
						break;
					duplicate = (strcmp(fst->fst_dir, (*tfstp)->fst_dir) == 0);
					if (duplicate) {
						/* disregard duplicate entry */
						freefst_ent(fst);
						break;
					}
				}
				if (!duplicate) {
					fst->fst_next = *tfstp;
					*tfstp = fst;
				}
			}
			host_ent->fstab_ents = tfstlist;
		}
	}
}

static const struct mntopt mopts_net[] = {
	MOPT_NET,
	{ NULL,		0, 0, 0 }
};

/*
 * Flush out all information we got the last time we read all the
 * fstab entries, and then read them and reconstruct that information.
 *
 * This assumes that a read lock is held on fstab_cache_lock.
 * It will take a write lock on it; this means that all read
 * locks will have been released before it gets the write lock.
 * If it returns 0, the write lock is still held; if it returns an
 * error, the write lock has been released.  (One reason it can
 * return an error is that, for whatever reason, the attempt to
 * get the write lock failed....)
 */
static int
readfstab(void)
{
	int err;
	struct fstab *fs;
	char *p;
	mntoptparse_t mop;
	int flags;
	int altflags;
	char *mntops, *url;

	if (trace  > 1)
		trace_prt(1, "readfstab called\n");

	/*
	 * Re-read fstab and rebuild the table.
	 * We assume we were called with a reader lock; release
	 * it and grab a writer lock, to ensure that nobody else
	 * will be looking at it while we're modifying it.
	 */
	pthread_rwlock_unlock(&fstab_cache_lock);
	err = pthread_rwlock_wrlock(&fstab_cache_lock);
	if (err != 0) {
		pr_msg("Error attempting to get write lock on fstab cache: %m");
		return (err);	/* use the cached data */
	}

	/*
	 * If the cache was populated recently, i.e. less than
	 * MIN_CACHE_TIME seconds ago, then just ignore this
	 * request to purge/repopulate the cache.  This avoids
	 * spurious cache purging by because a process is
	 * repeatedly looking up a name that's not cached.
	 */
	if (time(NULL) < min_cachetime)
		return (0);

	/*
	 * Clean out the old entries, in case this is being called
	 * because we failed to find an entry in a non-empty
	 * cache.
	 */
	clean_hashtables();

	/*
	 * OK, scan the fstab and build our data structure.
	 */
	setfsent();

	while ((fs = getfsent()) != NULL) {
		char *vfstype;

		/*
		 * Is this an entry with an empty type, an "rw", a "ro"
		 * entry, or none of those?
		 */
		if (fs->fs_type[0] != '\0' &&
		    strcmp(fs->fs_type, FSTAB_RW) != 0 &&
		    strcmp(fs->fs_type, FSTAB_RO) != 0) {
			/* None of those - ignore it. */
			continue;
		}

		/*
		 * Is "net" one of the mount options?
		 */
		flags = altflags = 0;
		getmnt_silent = 1;
		mop = getmntopts(fs->fs_mntops, mopts_net, &flags, &altflags);
		if (mop == NULL) {
			pr_msg("Couldn't parse mount options \"%s\" for %s: %m",
			    fs->fs_mntops, fs->fs_spec);
			continue;	/* give up on this */
		}
		freemntopts(mop);

		/*
		 * Extract the host name from fs_spec.
		 */
		p = strchr(fs->fs_spec, ':');
		if (p == NULL) {
			pr_msg("Mount for %s has no host name", fs->fs_spec);
			continue;	/* no host name - ignore this */
		}
		if (p == fs->fs_spec) {
			pr_msg("Mount for %s has an empty host name", fs->fs_spec);
			continue;	/* empty host name - ignore this */
		}
		*p++ = '\0';		/* split into host name and the rest */

		/*
		 * Massage the mount options.
		 */
		err = process_fstab_mntopts(fs, &mntops, &url);
		if (err == ENAMETOOLONG) {
			pr_msg("Mount options for %s are too long",
			    fs->fs_spec);
			continue;	/* give up on this */
		}
		if (err == ENOMEM)
			goto outofmem;

		/*
		 * If the VFS type is empty, we treat it as "nfs", for
		 * backwards compatibility with the old automounter.
		 */
		vfstype = fs->fs_vfstype;
		if (vfstype[0] == '\0')
			vfstype = "nfs";
		if (strcmp(vfstype, "url") == 0) {
			/* We must have a URL. */
			if (url == NULL) {
				pr_msg("Mount for %s has type url but no URL",
				    fs->fs_spec);
				free(mntops);
				continue;
			}
		} else {
			/* We must not have a URL. */
			if (url != NULL) {
				pr_msg("Mount for %s has type %s but has a URL",
				    fs->fs_spec, vfstype);
				free(mntops);
				free(url);
				continue;
			}
		}

		if (altflags & FSTAB_MNT_NET) {
			struct fstabnode *fst;
			struct fstabhost *host_entry;
			struct fstabhost **fstab_bucket;

			/*
			 * Entry has "net" - it's for -fstab.
			 *
			 * Allocate an entry for this fstab record.
			 */
			fst = calloc(1, sizeof (struct fstabnode));
			if (fst == NULL) {
				free(url);
				free(mntops);
				goto outofmem;
			}

			fst->fst_dir = strdup(p);
			if (fst->fst_dir == NULL) {
				free(fst);
				free(url);
				free(mntops);
				goto outofmem;
			}
			fst->fst_vfstype = strdup(vfstype);
			if (fst->fst_vfstype == NULL) {
				free(fst->fst_dir);
				free(fst);
				free(url);
				free(mntops);
				goto outofmem;
			}
			fst->fst_mntops = mntops;	/* this is mallocated */
			fst->fst_url = url;		/* as is this */

			/*
			 * Now add an entry for the host if we haven't already
			 * done so.
			 */
			host_entry = find_host_entry(fs->fs_spec, &fstab_bucket);
			if (host_entry == NULL) {
				/*
				 * We found no entry for the host; allocate
				 * one and add it to the hash table bucket.
				 */
				host_entry = malloc(sizeof (struct fstabhost));
				if (host_entry == NULL) {
					free(fst->fst_vfstype);
					free(fst->fst_dir);
					free(fst);
					free(url);
					free(mntops);
					goto outofmem;
				}
				host_entry->name = strdup(fs->fs_spec);
				if (host_entry->name == NULL) {
					free(host_entry);
					free(fst->fst_vfstype);
					free(fst->fst_dir);
					free(fst);
					free(url);
					free(mntops);
					goto outofmem;
				}
				host_entry->fstab_ents = fst;
				host_entry->next = *fstab_bucket;
				*fstab_bucket = host_entry;
			} else {
				/*
				 * We found an entry; add the fstab
				 * entry to its list of fstab entries.
				 */
				fst->fst_next = host_entry->fstab_ents;
				host_entry->fstab_ents = fst;
			}
		} else {
			/*
			 * Entry doesn't have "net" - it's for -static.
			 *
			 * Do we already have an entry for this
			 * directory?
			 */
			struct staticmap *static_ent;
			struct staticmap **static_bucket;

			if (strlen(fs->fs_file) == 0) {
				pr_msg("Mount for %s has an empty mount point path",
				    fs->fs_spec);
				continue;	/* empty mount point path - ignore this */
			}

			static_ent = find_staticmap_entry(fs->fs_file,
			    &static_bucket);
			if (static_ent == NULL) {
				/* No - make one. */
				static_ent = malloc(sizeof (struct staticmap));
				if (static_ent == NULL) {
					free(url);
					free(mntops);
					goto outofmem;
				}
				static_ent->dir = strdup(fs->fs_file);
				if (static_ent->dir == NULL) {
					free(static_ent);
					free(url);
					free(mntops);
					goto outofmem;
				}
				static_ent->vfstype = strdup(vfstype);
				if (static_ent->vfstype == NULL) {
					free(static_ent->dir);
					free(static_ent);
					free(url);
					free(mntops);
					goto outofmem;
				}
				static_ent->mntops = mntops;
				static_ent->host = strdup(fs->fs_spec);
				if (static_ent->host == NULL) {
					free(static_ent->vfstype);
					free(static_ent->dir);
					free(static_ent);
					free(url);
					free(mntops);
					goto outofmem;
				}
				if (url != NULL)
					static_ent->spec = url;
				else {
					static_ent->spec = strdup(p);
					if (static_ent->spec == NULL) {
						free(static_ent->host);
						free(static_ent->vfstype);
						free(static_ent->dir);
						free(static_ent);
						free(url);
						free(mntops);
						goto outofmem;
					}
				}

				/* Add it to the hash bucket. */
				static_ent->next = *static_bucket;
				*static_bucket = static_ent;
			} else {
				/* Yes - leave it. */
				free(mntops);
				free(url);
			}
		}
	}
	endfsent();

	/*
	 * Now, for each host entry, sort the list of fstab entries
	 * by the length of the name; automountd expects that, so it
	 * can get the mount order right.
	 */
	sort_fstab_entries();

	/*
	 * Update the min and max cache times
	 */
	min_cachetime = time(NULL) + MIN_CACHE_TIME;
	max_cachetime = time(NULL) + RDDIR_CACHE_TIME * 2;

	return (0);

outofmem:
	endfsent();
	clean_hashtables();
	pthread_rwlock_unlock(&fstab_cache_lock);
	pr_msg("Memory allocation failed while reading fstab");
	return (ENOMEM);
}

/*
 * Look up a particular host in the fstab map hash table and, if we find it,
 * run the callback routine on each entry in its fstab entry list.
 */
int
fstab_process_host(const char *host, int (*callback)(struct fstabnode *, void *),
    void *callback_arg)
{
	int err;
	struct fstabhost *hostent;
	struct fstabnode *fst;

	/*
	 * Get a read lock, so the cache doesn't get modified out
	 * from under us.
	 */
	err = pthread_rwlock_rdlock(&fstab_cache_lock);
	if (err != 0)
		return (err);

	hostent = find_host_entry(host, NULL);
	if (hostent == NULL) {
		/*
		 * We've seen no entries for that host,
		 * either because the cache has been purged
		 * or we didn't find any the last time we
		 * scanned the fstab entries.
		 *
		 * Try re-reading the fstab entries.
		 */
		err = readfstab();
		if (err != 0) {
			/*
			 * That failed; give up.
			 */
			return (err);
		}

		/*
		 * Now see if we can find the host.
		 */
		hostent = find_host_entry(host, NULL);
		if (hostent == NULL) {
			/*
			 * No - give up.
			 */
			pthread_rwlock_unlock(&fstab_cache_lock);
			return (-1);
		}
	}

	/*
	 * Run the callback on every entry in the fstab node list.
	 * If the callback returns a non-zero value, stop and
	 * return that value as an error.
	 */
	err = 0;
	for (fst = hostent->fstab_ents; fst != NULL; fst = fst->fst_next) {
		err = (*callback)(fst, callback_arg);
		if (err != 0)
			break;
	}

	/* We're done processing the list; release the lock. */
	pthread_rwlock_unlock(&fstab_cache_lock);

	return (err);
}

/*
 * This assumes that a read or write lock on the fstab cache lock is held.
 */
static int
scan_fstab(struct dir_entry **list)
{
	struct dir_entry *last = NULL;
	int i;
	struct fstabhost *host_ent;
	int err;
	char thishost[MAXHOSTNAMELEN];
	char *p;

	for (i = 0; i < HASHTABLESIZE; i++) {
		for (host_ent = fstab_hashtable[i]; host_ent != NULL;
		    host_ent = host_ent->next) {
		    	/*
			 * Add an entry for it if we haven't already
			 * done so.
			 */
			err = add_dir_entry(host_ent->name, list, &last);
			if (err)
				return (err);
			assert(last != NULL);
		}
	}

	/*
	 * If we're a server, add an entry for this host's FQDN and
	 * for the first component of its name; it will show up as
	 * a symbolic link to "/".
	 *
	 * We do this, just as the old automounter did, so that, on
	 * a server, you can refer to network home directories on the
	 * machine with a /Network/Server/... path even if you haven't
	 * yet made a mount record for the host, or if you're on an
	 * Active Directory network and mount records are synthesized
	 * when the user is looked up.  (See 5479706.)
	 */
	if (we_are_a_server()) {
		/* (presumed) FQDN */
		gethostname(thishost, MAXHOSTNAMELEN);
		err = add_dir_entry(thishost, list, &last);
		if (err)
			return (err);
		assert(last != NULL);

		/* First component. */
		p = strchr(thishost, '.');
		if (p != NULL) {
			*p = '\0';
			err = add_dir_entry(thishost, list, &last);
			if (err)
				return (err);
			assert(last != NULL);
		}
	}
	return (0);
}

/*
 * Enumerate all the entries in the -fstab map.
 * This is used by a readdir on the -fstab map; those are likely to
 * be followed by stats on one or more of the entries in that map, so
 * we populate the fstab map cache and return values from that.
 */
int
getfstabkeys(struct dir_entry **list, int *error, int *cache_time)
{
	int err;

	/*
	 * Get a read lock, so the cache doesn't get modified out
	 * from under us.
	 */
	err = pthread_rwlock_rdlock(&fstab_cache_lock);
	if (err != 0) {
		*error = err;
		return (__NSW_UNAVAIL);
	}

	/*
	 * Return the time until the current cache data times out.
	 */
	*cache_time = max_cachetime - time(NULL);
	if (*cache_time < 0)
		*cache_time = 0;
	*error = 0;
	if (trace  > 1)
		trace_prt(1, "getfstabkeys called\n");

	/*
	 * Get what we have cached.
	 */
	*error = scan_fstab(list);
	if (*error != 0)
		goto done;

	if (*list == NULL) {
		/*
		 * We've seen no entries, either because the cache has
		 * been purged or we didn't find any the last time we
		 * scanned the fstab entries.
		 *
		 * Try re-reading the fstab entries.
		 */
		err = readfstab();
		if (err != 0) {
			/*
			 * That failed; give up.
			 */
			*error = err;
			return (__NSW_UNAVAIL);
		}

		/*
		 * Get what we now have cached.
		 */
		*error = scan_fstab(list);
		if (*error != 0)
			goto done;
	}
done:	if (*list != NULL) {
		/*
		 * list of entries found
		 */
		*error = 0;
	}

	/* We're done processing the list; release the lock. */
	pthread_rwlock_unlock(&fstab_cache_lock);

	return (__NSW_SUCCESS);
}

/*
 * Check whether we have any entries in the -fstab map.
 * This is used by automount to decide whether to mount that map
 * or not.
 */
int
havefstabkeys(void)
{
	int err;
	int ret;
	int i;

	/*
	 * Are we a server?  If so, we always have -fstab entries,
	 * as there's always the selflink.
	 */
	if (we_are_a_server())
		return (1);

	/*
	 * Get a read lock, so the cache doesn't get modified out
	 * from under us.
	 */
	err = pthread_rwlock_rdlock(&fstab_cache_lock);
	if (err != 0)
		return (0);

	if (trace  > 1)
		trace_prt(1, "havefstabkeys called\n");

	/*
	 * Check what we have cached.
	 */
	for (i = 0; i < HASHTABLESIZE; i++) {
		if (fstab_hashtable[i] != NULL) {
			/*
			 * We have at least one entry.
			 */
			ret = 1;
			goto done;
		}
	}

	/*
	 * We've seen no entries, either because the cache has
	 * been purged or we didn't find any the last time we
	 * scanned the fstab entries.
	 *
	 * Try re-reading the fstab entries.
	 */
	err = readfstab();
	if (err != 0) {
		/*
		 * That failed; give up.
		 */
		ret = 0;
		goto done;
	}

	/*
	 * Check what we now have cached.
	 */
	for (i = 0; i < HASHTABLESIZE; i++) {
		if (fstab_hashtable[i] != NULL) {
			/*
			 * We have at least one entry.
			 */
			ret = 1;
			goto done;
		}
	}

	/*
	 * Nothing found.
	 */
	ret = 0;

done:

	/* We're done processing the list; release the lock. */
	pthread_rwlock_unlock(&fstab_cache_lock);

	return (ret);
}

/*
 * Load the -static direct map.
 */
int
loaddirect_static(local_map, opts, stack, stkptr)
	char *local_map, *opts;
	char **stack, ***stkptr;
{
	int done = 0;
	int i;
	struct staticmap *static_ent;

	/*
	 * Get a read lock, so the cache doesn't get modified out
	 * from under us.
	 */
	if (pthread_rwlock_rdlock(&fstab_cache_lock) != 0)
		return (__NSW_UNAVAIL);

	/*
	 * Get what we have cached.
	 */
	for (i = 0; i < HASHTABLESIZE; i++) {
		for (static_ent = static_hashtable[i]; static_ent != NULL;
		    static_ent = static_ent->next) {
			dirinit(static_ent->dir, local_map, opts, 1, stack, stkptr);
			done++;
		}
	}

	if (!done) {
		/*
		 * We've seen no entries, either because the cache has
		 * been purged or we didn't find any the last time we
		 * scanned the fstab entries.
		 *
		 * Try re-reading the fstab entries.
		 */
		if (readfstab() != 0) {
			/*
			 * That failed; give up.
			 */
			return (__NSW_UNAVAIL);
		}

		/*
		 * Get what we now have cached.
		 */
		for (i = 0; i < HASHTABLESIZE; i++) {
			for (static_ent = static_hashtable[i];
			    static_ent != NULL;
			    static_ent = static_ent->next) {
				dirinit(static_ent->dir, local_map, opts, 1, stack, stkptr);
				done++;
			}
		}
	}

	pthread_rwlock_unlock(&fstab_cache_lock);
	return (done ? __NSW_SUCCESS : __NSW_NOTFOUND);
}

/*
 * Find the -static map entry corresponding to a given mount point.
 */
struct staticmap *
get_staticmap_entry(const char *dir)
{
	struct staticmap *static_ent;

	/*
	 * Get a read lock, so the cache doesn't get modified out
	 * from under us.
	 */
	if (pthread_rwlock_rdlock(&fstab_cache_lock) != 0)
		return (NULL);

	/*
	 * Get what we have cached.
	 */
	static_ent = find_staticmap_entry(dir, NULL);

	if (static_ent == NULL) {
		/*
		 * We've found no entry, either because the cache has
		 * been purged or we didn't find any the last time we
		 * scanned the fstab entries.
		 *
		 * Try re-reading the fstab entries.
		 */
		if (readfstab() != 0) {
			/*
			 * That failed; give up.
			 */
			return (NULL);
		}

		/*
		 * Check what we now have cached.
		 */
		static_ent = find_staticmap_entry(dir, NULL);
	}

	pthread_rwlock_unlock(&fstab_cache_lock);
	return (static_ent);
}

/*
 * Purge the fstab cache; if scheduled is true, do so only if it's
 * stale, otherwise do it unconditionally.
 */
void
clean_fstab_cache(int scheduled)
{
	/*
	 * If this is a scheduled cache cleanup, and the cache is still
	 * valid, don't purge it.
	 */
	if (scheduled && max_cachetime > time(NULL))
		return;

	/*
	 * Lock the cache against all operations.
	 */
	if (pthread_rwlock_wrlock(&fstab_cache_lock) != 0)
		return;

	/*
	 * Clean out the old entries.
	 */
	clean_hashtables();

	/*
	 * Reset the minimum cache time, so that we'll reread the fstab
	 * entries; we have to do so, as we discarded the results of
	 * the last read.
	 */
	min_cachetime = 0;

	pthread_rwlock_unlock(&fstab_cache_lock);
}
