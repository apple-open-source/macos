/*
 * Copyright (c) 2011 - 2023  Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

#include <sys/smb_apple.h>
#include <sys/vnode.h>
#include <sys/mman.h>

#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_rq.h>
#include <smbfs/smbfs_lockf.h>
#include <smbfs/smbfs_node.h>
#include <smbfs/smbfs_subr.h>
#include <smbfs/smbfs_subr_2.h>
#include <smbfs/smbfs.h>
#include <smbclient/smbclient_internal.h>


extern int32_t gSMBSleeping;

lck_mtx_t global_dir_cache_lock;
struct global_dir_cache_entry *global_dir_cache_head;
uint64_t g_hardware_memory_size = 0;
uint32_t g_max_dirs_cached = k_2GB_max_dirs_cached;
uint32_t g_max_dir_entries_cached = k_2GB_max_dir_entries_cached;


#pragma mark - SMB Data Compression

/*
 * Part of this function from fbt_blacklist.c
 * Default list of file extensions that should be excluded from SMB data
 * compression because its expected they will not compress much compared
 * to the effort or processing them.
 *
 * This must be kept in asciibetical order for purposes of bsearch().
 */
const char * smb_compression_exclude_list[] =
{
    "7z",
    "aa3",
    "aac",
    "aes",
    "asf",
    "avchd",
    "avi",
    "bik",
    "bsf",
    "bz",
    "bz2",
    "bzip",
    "bzip2",
    "chm",
    "cpgz",
    "cr2",
    "divx",
    "dng",
    "docm",
    "docx",
    "dotm",
    "dotx",
    "emz",
    "epub",
    "f4v",
    "flv",
    "gif",
    "gpg",
    "graffle",
    "gz",
    "gzip",
    "hdmov",
    "heic",
    "heif",
    "hxs",
    "j2c",
    "jar",
    "jpeg",
    "jpg",
    "lzma",
    "m4a",
    "m4a",
    "m4v",
    "mint",
    "mkv",
    "mov",
    "mp2",
    "mp3",
    "mp4",
    "mpa",
    "mpe",
    "mpeg",
    "mpg",
    "mpq",
    "mshc",
    "msi",
    "mts",
    "nef",
    "odp",
    "ods",
    "odt",
    "opus",
    "otp",
    "ots",
    "ott",
    "pack",
    "pages",
    "png",
    "pptm",
    "pptx",
    "pspimage",
    "qt",
    "ra",
    "rar",
    "rpm",
    "sea",
    "sit",
    "tgz",
    "tif",
    "tiff",
    "vob",
    "war",
    "wav",
    "webarchive",
    "webm",
    "webp",
    "wma",
    "wmv",
    "wtv",
    "wv",
    "xlsb",
    "xlsm",
    "xlsx",
    "xps",
    "xz",
    "zip",
    "zstd"
};
#define SMB_EXCLUSION_LIST_COUNT (sizeof(smb_compression_exclude_list)/sizeof(smb_compression_exclude_list[0]))

static const void*
smb_bsearch(const void *key, const void *base0, size_t nmemb, size_t size, int (*compar)(const void *, const void *))
{
    /* Part of this function from fbt_blacklist.c */
    const char *base = base0;
    size_t lim = 0;
    int cmp = 0;
    const void *p = NULL;
    
    for (lim = nmemb; lim != 0; lim >>= 1) {
        p = base + (lim >> 1) * size;
            cmp = (*compar)(key, p);
            if (cmp == 0) {
                return p;
            }
            if (cmp > 0) {  /* key > p: move right */
                base = (const char *)p + size;
                lim--;
            }               /* else move left */
    }
    return NULL;
}

/*
 * Does this extension match any in the user supplied lists?
 */
int
smb_check_user_list(const char* extension, size_t extension_len,
                    char *list[], uint32_t list_cnt)
{
    /* Expected that the user lists will be small enough for a linear search */
    uint32_t i = 0;
    
    for (i = 0; i < list_cnt; i++) {
        if (extension_len != strlen(list[i])) {
            continue;
        }
        
        if (strncasecmp(extension, list[i], strlen(list[i])) == 0) {
            /* found a match */
            return(1);
        }
    }
    
    return(0);
}

static int
smb_cmp(const void *a, const void *b)
{
    /*
     * Part of this function from fbt_blacklist.c
     * Do the comparison as case insensitive so "zip" and "ZIP" are the same
     */
    const char *v = *(const char **)b;
    return strncasecmp((const char *)a, v, strlen(v));
}

/*
 * SMB data compression filename exclusion check (case insensitive)
 */
bool
smb_compression_excluded(const char* extension, size_t extension_len)
{
#pragma unused(extension_len)
    /* Part of this function from fbt_blacklist.c */
    const char *excluded = NULL;

    excluded = smb_bsearch(extension, smb_compression_exclude_list,
                           SMB_EXCLUSION_LIST_COUNT, sizeof(smb_compression_exclude_list[0]),
                           smb_cmp);
    return excluded;
}


int
smb_compression_allowed(struct mount *mp, vnode_t vp)
{
    /* Part of this function from is_package_name() */
    char *name = NULL;
    size_t name_len = 0;
    const char *ptr = NULL, *name_ext = NULL;
    struct smbnode *np = NULL;
    int compression_allowed = 1; /* assume it is compressible */
    int match_found = 0;
    struct smbmount *smp = NULL;
    struct smb_session *sessionp = NULL;

    /* Start with basic sanity checking */
    if ((vp == NULL) || (mp == NULL)) {
        SMBERROR("Null vp or mp? \n");
        return(0);
    }
    
    if (vnode_vtype(vp) != VREG) {
        /* Only regular files can be compressed */
        return(0);
    }

    np = VTOSMB(vp);
    if (np == NULL) {
        SMBERROR("Null np? \n");
        return(0);
    }

    if ((np->n_name == NULL) || (np->n_nmlen == 0)) {
        SMBERROR("n_name is null or n_nmlen is 0? \n");
        return(0);
    }

    smp = VFSTOSMBFS(mp);
    if (smp == NULL) {
        SMBERROR("Null smp? \n");
        return(0);
    }

    sessionp = SS_TO_SESSION(smp->sm_share);
    if (sessionp == NULL) {
        SMBERROR("Null sessionp? \n");
        return(0);
    }

    /* Is compression supported? */
    if (!SMBV_SMB3_OR_LATER(sessionp) ||
        (sessionp->server_compression_algorithms_map == 0)) {
        /* Compression not supported */
        return(0);
    }

    name = np->n_name;
    name_len = np->n_nmlen;
    
    /*
     * Find the filename extension if any.
     *
     * If the name is less than 3 bytes it can't be of the
     * form A.B and if it begins with a "." then it is also
     * does not have an extension. Assume its compressible.
     */
    if (name_len <= 3 || name[0] == '.') {
        return(compression_allowed);
    }

    name_ext = NULL;
    for (ptr = name; *ptr != '\0'; ptr++) {
        if (*ptr == '.') {
            name_ext = ptr;
        }
    }

    /* if there is no "." extension, it can't match. Assume its compressible */
    if ((name_ext == NULL) || (name_ext++ == NULL)) {
        return(compression_allowed);
    }

    /* advance over the "." */
    name_ext++;

    /* Check the default exclusion list */
    if (smb_compression_excluded(name_ext, strlen(name_ext))) {
        /* Compression not allowed, but does user list allow this file? */
        compression_allowed = 0;
        
        match_found = smb_check_user_list(name_ext, strlen(name_ext),
                                          smp->sm_args.compression_include,
                                          smp->sm_args.compression_include_cnt);
        if (match_found) {
            SMB_LOG_COMPRESS_LOCK(np, "Compression is allowed on <%s> from user list \n",
                                  np->n_name);
            compression_allowed = 1;
        }
        else {
            SMB_LOG_COMPRESS_LOCK(np, "Compression not allowed on <%s> from default list \n",
                                  np->n_name);
        }
    }
    else {
        /* Compression allowed, but does user list exclude this file? */
        compression_allowed = 1;

        match_found = smb_check_user_list(name_ext, strlen(name_ext),
                                          smp->sm_args.compression_exclude,
                                          smp->sm_args.compression_exclude_cnt);
        if (match_found) {
            SMB_LOG_COMPRESS_LOCK(np, "Compression not allowed on <%s> from user list \n",
                                  np->n_name);
            compression_allowed = 0;
        }
    }
    
    /* if we get here, no extension matched */
    return(compression_allowed);
}


#pragma mark - Dir Enumeration Caching

void
smb_dir_cache_add_entry(vnode_t dvp, void *in_cachep,
                        const char *name, size_t name_len,
                        struct smbfattr *fap,
                        uint32_t is_overflow, int is_locked)
{
    struct smb_enum_cache *cachep = in_cachep;
    struct smbnode *dnp = NULL;
    struct cached_dir_entry *entry = NULL;
    struct cached_dir_entry *current = NULL;
    uint32_t dir_cache_max_cnt = g_max_dir_entries_cached;
    struct timespec ts;
    const char *cache_namep;

    if (is_overflow) {
        cache_namep = "overflow";
    }
    else {
        cache_namep = "main";
    }
	
	if (fap == NULL) {
        SMBERROR("fap is null \n");
        return;
    }
    
    if (dvp == NULL) {
        SMBERROR("dvp is null \n");
        return;
    }
    dnp = VTOSMB(dvp);

    if (!vnode_isdir(dvp)) {
        SMBERROR("dvp is not a dir \n");
        return;
    }
    
    if (cachep->count >= dir_cache_max_cnt) {
        if (!(cachep->flags & kDirCachePartial)) {
            SMB_LOG_DIR_CACHE_LOCK(dnp, "Max entries allowed %lld >= %d for <%s> <%s>\n",
                                   cachep->count, dir_cache_max_cnt,
                                   dnp->n_name, cache_namep);
            cachep->flags |= kDirCachePartial;
 
            /* 
             * Do not need to reset cache time here as it was already done
             * when the last entry was added.
             */
        }
        return;
    }

    //SMB_LOG_DIR_CACHE2("Caching <%s> <%s>\n", name, cache_namep);

    /* Create a new cached_dir_entry and insert it into the list */
    SMB_MALLOC_TYPE(entry, struct cached_dir_entry, Z_WAITOK_ZERO);
    
	entry->name = vfs_addname(name, (uint32_t) name_len, 0, 0);
    entry->name_len = name_len;
    memcpy(&entry->fattr, fap, sizeof(entry->fattr));
    entry->next = NULL;
    
    /*
     * For non OS X servers, we are missing
     * 1) Resource Fork info for files
     * 2) Finder Info
     * 3) Max Access -> common user access
     *
     * If we are missing any of these, then go fetch them later.
     */
    if (!(entry->fattr.fa_valid_mask & FA_FINDERINFO_VALID) ||
        !(entry->fattr.fa_valid_mask & FA_MAX_ACCESS_VALID) ||
        ((entry->fattr.fa_vtype == VREG) &&
         !(entry->fattr.fa_valid_mask & FA_RSRC_FORK_VALID))) {
            SMB_LOG_DIR_CACHE2("Entry <%s> needs Meta Data \n", entry->name);
            entry->flags |= kCacheEntryNeedsMetaData;
        }
    
    if (!is_locked) {
        lck_mtx_lock(&dnp->d_enum_cache_list_lock);
    }
    
    if (cachep->list == NULL) {
        /* No other entries, so we are the first */
        if (is_overflow == 0) {
            cachep->offset = 0;
        }
        else {
            /* Save the starting offset for overflow cache */
            cachep->start_offset = cachep->offset;
        }
        cachep->list = entry;
        
        /* Set cache time */
        nanouptime(&ts);
        cachep->timer = ts.tv_sec;
        
        /* Remember current dir change cnt to detect local changes */
        smbnode_lease_lock(&dnp->n_lease, smb_dir_cache_add_entry);
        cachep->chg_cnt = dnp->d_changecnt;
        smbnode_lease_unlock(&dnp->n_lease);

		SMB_LOG_DIR_CACHE_LOCK(dnp, "Set chg cnt to %d for <%s> <%s> \n",
                               cachep->chg_cnt, dnp->n_name, cache_namep);
    }
    else {
        /* look for last entry in the list */
        current = cachep->list;
        while (current->next != NULL) {
            current = current->next;
        }
        
        /* put it at the end of the list */
        current->next = entry;
    }
    
    cachep->offset += 1;
    cachep->count += 1;
    
    /* Mark that the dir cache needs to get Meta Data and/or Finder Info */
    cachep->flags |= kDirCacheDirty;
    
    /* Was that the last entry that would fit? */
    if (cachep->count >= dir_cache_max_cnt) {
        SMB_LOG_DIR_CACHE_LOCK(dnp, "Max entries allowed %lld >= %d for <%s> <%s>\n",
                               cachep->count, dir_cache_max_cnt,
                               dnp->n_name, cache_namep);
        cachep->flags |= kDirCachePartial;

        /* Last entry that will fit, so reset cache time */
        nanouptime(&ts);
        cachep->timer = ts.tv_sec;
    }
    
    if (!is_locked) {
        lck_mtx_unlock(&dnp->d_enum_cache_list_lock);
    }
}

void
smb_dir_cache_check(vnode_t dvp, void *in_cachep, int is_locked,
                    vfs_context_t context)
{
    struct smb_enum_cache *cachep = in_cachep;
    struct smbnode *dnp = NULL;
    struct smbmount *smp = NULL;
    int32_t dir_cache_max = 60; /* keep in sync with getDefaultPreferences */
    int32_t dir_cache_min = 30; /* keep in sync with getDefaultPreferences */
    struct timespec	ts;
    time_t attrtimeo;
    int remove_cache = 0;
	struct smb_session *sessionp = NULL;
	static const char expired_str[] = "expired";
	static const char local_chg_str[] = "local change";
	const char *reason_str = expired_str;
	
    if (dvp == NULL) {
        SMBERROR("dvp is null \n");
        return;
    }
    dnp = VTOSMB(dvp);
    
    if (!vnode_isdir(dvp)) {
        SMBERROR("dvp is not a dir \n");
        return;
    }

	smp = VFSTOSMBFS(vnode_mount(dvp));
	if (smp == NULL) {
		SMBERROR("smp is null \n");
		return;
	}

	sessionp = SS_TO_SESSION(smp->sm_share);
	if (sessionp == NULL) {
		SMBERROR("sessionp is null \n");
		return;
	}
	
    SMB_LOG_KTRACE(SMB_DBG_DIR_CACHE_CHECK | DBG_FUNC_START, 0, 0, 0, 0, 0);

    if (!is_locked) {
        lck_mtx_lock(&dnp->d_enum_cache_list_lock);
    }

    /* Did the dir cache expire? */
    if (cachep->timer != 0) {
        if ((smp->sm_args.dir_cache_max != 0) &&
            (smp->sm_args.dir_cache_max <= 3600)){
            dir_cache_max = smp->sm_args.dir_cache_max;
        }
        
        if ((smp->sm_args.dir_cache_min != 0) &&
            (smp->sm_args.dir_cache_min >= 1)){
            dir_cache_min = smp->sm_args.dir_cache_min;
        }
        
        SMB_DIR_CACHE_TIME(ts, dnp, attrtimeo, dir_cache_max, dir_cache_min);
        
        if ((ts.tv_sec - cachep->timer) > attrtimeo) {
            if (dnp->d_main_cache.flags & kDirCacheComplete) {
                /*
                 * Make sure the enumeration has completed before removing it
                 * Dir cache has expired so remove it
                 */
                SMB_LOG_LEASING_LOCK(dnp, "Enum cache has expired <%ld> for <%s> \n",
                                       attrtimeo, dnp->n_name);
                remove_cache = 1;
                reason_str = expired_str;
            }
        }
    }
    else {
       nanouptime(&ts);
       cachep->timer = ts.tv_sec;
   }

    /* If dir cache has not expired, then was there a local change? */
    smbnode_lease_lock(&dnp->n_lease, smb_dir_cache_check);

    if ((remove_cache == 0) &&
        (cachep->chg_cnt != dnp->d_changecnt)) {
        smbnode_lease_unlock(&dnp->n_lease);

		SMB_LOG_DIR_CACHE_LOCK(dnp, "Local change for <%s> %d != %d\n",
                               dnp->n_name, cachep->chg_cnt, dnp->d_changecnt);
        remove_cache = 1;
		reason_str = local_chg_str;
    }
	else {
        smbnode_lease_unlock(&dnp->n_lease);
	}

    if (remove_cache == 1) {
        smbnode_lease_lock(&dnp->n_lease, smb_dir_cache_check);
        if ((SMBV_SMB3_OR_LATER(sessionp)) &&
            (sessionp->session_sopt.sv_active_capabilities & SMB2_GLOBAL_CAP_DIRECTORY_LEASING) &&
            !(smp->sm_args.altflags & SMBFS_MNT_DIR_LEASE_OFF) &&
            (dnp->n_lease.flags & SMB2_LEASE_GRANTED)) {
            /* Dir has an active lease, we need close it */
            smbnode_lease_unlock(&dnp->n_lease);
            smbfs_closedirlookup(dnp, 0, reason_str, context);
            smbfs_closedirlookup(dnp, 1, reason_str, context);
        }
        else {
            smbnode_lease_unlock(&dnp->n_lease);
        }
        
        /* Any locking was already done earlier in this function */
        smb_dir_cache_remove(dvp, cachep, "main", reason_str, 1, 0);
    }
    
    if (!is_locked) {
        lck_mtx_unlock(&dnp->d_enum_cache_list_lock);
    }

    SMB_LOG_KTRACE(SMB_DBG_DIR_CACHE_CHECK | DBG_FUNC_END, 0, 0, 0, 0, 0);
}

int32_t
smb_dir_cache_find_entry(vnode_t dvp, void *in_cachep,
                         char *name, size_t name_len,
                         struct smbfattr *fap, uint64_t req_attrs,
                         vfs_context_t context)
{
    struct smb_enum_cache *cachep = in_cachep;
    struct smbnode *dnp = NULL;
    struct cached_dir_entry *entry = NULL;
    int32_t error = 0;
    
    if (fap == NULL) {
        SMBERROR("fap is null \n");
        return (ENOENT);
    }
    
    if ((name == NULL) || (name_len == 0)) {
        SMBERROR("name is null or zero length \n");
        return (ENOENT);
    }
    
    if (dvp == NULL) {
        SMBERROR("dvp is null \n");
        return (ENOENT);
    }
    dnp = VTOSMB(dvp);
	   
    if (!vnode_isdir(dvp)) {
        SMBERROR("dvp is not a dir \n");
        return (EINVAL);
    }

    lck_mtx_lock(&dnp->d_enum_cache_list_lock);
    
    /* Setup the dir cache */
    smb_dir_cache_check(dvp, &dnp->d_main_cache, 1, context);

    /* Now search the list until we find a match */
    for (entry = cachep->list; entry; entry = entry->next) {
        if ((entry->name_len == name_len) &&
            (bcmp(entry->name, name, name_len) == 0)) {
            /* found a match, but did we manage to get the attrs they want? */
            if ((req_attrs != 0) &&
                !(req_attrs & entry->fattr.fa_valid_mask)) {
                SMB_LOG_DIR_CACHE("Asking for 0x%llx but only have 0x%llx for %s \n",
                                  req_attrs, entry->fattr.fa_valid_mask,
                                  entry->name);
                error = ENOENT;
            }
            
            if (error == 0) {
                *fap = entry->fattr;
            }
            
            lck_mtx_unlock(&dnp->d_enum_cache_list_lock);
            return(error);
        }
    }
    
    lck_mtx_unlock(&dnp->d_enum_cache_list_lock);
    return(ENOENT);	/* No match found */
}

int32_t
smb_dir_cache_get_attrs(struct smb_share *share, vnode_t dvp,
                        void *in_cachep, int is_locked,
                        vfs_context_t context)
{
    struct smb_enum_cache *cachep = in_cachep;
    int error = 0;
    struct timespec	start, stop;
    
    if (dvp == NULL) {
        SMBERROR("dvp is null \n");
        return (ENOENT);
    }
    
    if (!vnode_isdir(dvp)) {
        SMBERROR("dvp is not a dir \n");
        return (EINVAL);
    }

    if (!is_locked) {
        lck_mtx_lock(&VTOSMB(dvp)->d_enum_cache_list_lock);
    }
    
again:
	nanotime(&start);
    error = smb2fs_smb_cmpd_query_async(share, dvp, cachep,
                                        kDirCacheGetStreamInfo, context);
    nanotime(&stop);
    SMB_LOG_DIR_CACHE_LOCK(VTOSMB(dvp), "stream elapsed time %ld for <%s>\n",
                           stop.tv_sec - start.tv_sec, VTOSMB(dvp)->n_name);
    if (error) {
		if (error != ETIMEDOUT) {
			SMBERROR("smb2fs_smb_cmpd_query_async failed for stream info %d \n",
					error);
		}
    }
    else {
        nanotime(&start);
        error = smb2fs_smb_cmpd_query_async(share, dvp, cachep,
                                            kDirCacheGetFinderInfo,
                                            context);
        nanotime(&stop);
        SMB_LOG_DIR_CACHE_LOCK(VTOSMB(dvp), "FInfo elapsed time %ld for <%s>\n",
                               stop.tv_sec - start.tv_sec, VTOSMB(dvp)->n_name);
        if (error) {
			if (error != ETIMEDOUT) {
				SMBERROR("smb2fs_smb_cmpd_query_async failed for Finder Info %d \n",
						error);
			}
        }
    }
	
	if (error == ETIMEDOUT) {
        if (share->ss_going_away(share)) {
            /*
             * the request timed out because the share is going away
             * we should stop and let the unmount finish
             */
            SMBDEBUG("Share going away, not trying again");
        } else {
            /* Reconnect must have occurred, try again */
            goto again;
        }
	}
	
    if (!is_locked) {
        lck_mtx_unlock(&VTOSMB(dvp)->d_enum_cache_list_lock);
    }
    
    return(error);
}

void
smb_dir_cache_invalidate(vnode_t vp, uint32_t forceInvalidate)
{
#pragma unused(forceInvalidate)
    vnode_t par_vp = NULL;
    vnode_t data_par_vp = NULL;
    struct smbnode *np = VTOSMB(vp);
    
    par_vp = smbfs_smb_get_parent(np, kShareLock);
    if (par_vp != NULL) {
        if (!vnode_isnamedstream(vp)) {
            /* Not a named stream, so this is the parent dir */
			if (vnode_isdir(par_vp)) {
				/* Dont skip local change notify */
                VTOSMB(par_vp)->d_changecnt++;
			}
        }
        else {
            /* 
             * If its a named stream, then par_vp is the data fork vnode.
             * Have to now get the parent of the par_vp to get the real parent
             * dir.
             */
            data_par_vp = smbfs_smb_get_parent(VTOSMB(par_vp), kShareLock);
            if (data_par_vp != NULL) {
                /* Got the real parent dir */
				if (vnode_isdir(data_par_vp)) {
					/* Dont skip local change notify */
                    VTOSMB(data_par_vp)->d_changecnt++;
				}

				vnode_put(data_par_vp);
            }
            else {
                if (VTOSMB(par_vp)->n_parent_vid != 0) {
                    /* Parent got recycled already? Ok to ignore */
                    SMBWARNING_LOCK(VTOSMB(par_vp), "Missing parent for <%s> \n",
                                    VTOSMB(par_vp)->n_name);
                }
            }
        }
        
        vnode_put(par_vp);
    }
    else {
        if (np->n_parent_vid != 0) {
            /* Parent got recycled already? Ok to ignore */
            SMBWARNING_LOCK(np, "Missing parent for <%s> \n",
                            np->n_name);
        }
    }
}

void
smb_dir_cache_remove(vnode_t dvp, void *in_cachep,
					 const char *cache, const char *reason,
					 int is_locked, off_t offset)
{
    struct smb_enum_cache *cachep = in_cachep;
    struct smbnode *dnp = NULL;
    struct cached_dir_entry *entryp = NULL;
    struct cached_dir_entry *currp = NULL;
    off_t remove_count = 0;
    uint8_t partial_remove = 0;
    
    if (dvp == NULL) {
        SMBERROR("dvp is null \n");
        return;
    }
    dnp = VTOSMB(dvp);

    if (!vnode_isdir(dvp)) {
        SMBERROR("dvp is not a dir \n");
        return;
    }
    
    SMB_LOG_KTRACE(SMB_DBG_DIR_CACHE_REMOVE | DBG_FUNC_START, 0, 0, 0, 0, 0);

    if (!is_locked) {
        lck_mtx_lock(&dnp->d_enum_cache_list_lock);
    }
    
    entryp = cachep->list;
    if (entryp != NULL) {
        SMB_LOG_DIR_CACHE_LOCK(dnp, "Removing <%s> dir cache for <%s> due to <%s> \n",
                               cache, dnp->n_name, reason);
    }

    partial_remove = (offset > 0) && (offset > cachep->start_offset) && (offset <= cachep->offset);

    if (partial_remove) {
        remove_count = offset - cachep->start_offset;
        SMB_LOG_DIR_CACHE_LOCK(dnp, "Partial cache remove for <%s> cachep->start_offset:<%lld> offset:<%lld>\n",
                               dnp->n_name, cachep->start_offset, offset);
    }

    while (entryp != NULL) {
        /* wipe out the enum cache entries */
        currp = entryp;
        entryp = entryp->next;
		
		vfs_removename(currp->name);
        SMB_FREE_TYPE(struct cached_dir_entry, currp);

        remove_count--;
        if (partial_remove && (remove_count == 0)) {
            break;
        }
    }
    
    if (partial_remove) {
        cachep->start_offset = offset;
        cachep->count = cachep->offset - offset;
        cachep->list = entryp;
    } else {
        cachep->offset = 0;
        cachep->start_offset = 0;
        cachep->count = 0;
        cachep->list = NULL;
    }
    cachep->flags &= ~(kDirCacheComplete | kDirCachePartial);
    
	/* 
	 * Reset current dir change cnt so we dont keep trying to remove the dir
	 * cache because the change cnts dont match
	 */
    smbnode_lease_lock(&dnp->n_lease, smb_dir_cache_remove);
	cachep->chg_cnt = dnp->d_changecnt;
    smbnode_lease_unlock(&dnp->n_lease);

	if (!is_locked) {
        lck_mtx_unlock(&dnp->d_enum_cache_list_lock);
    }

    SMB_LOG_KTRACE(SMB_DBG_DIR_CACHE_REMOVE | DBG_FUNC_END, 0, 0, 0, 0, 0);
}

void
smb_dir_cache_remove_one(vnode_t dvp, void *in_cachep,
                         void *in_one_entryp, int is_locked)
{
    struct smb_enum_cache *cachep = in_cachep;
    struct smbnode *dnp = NULL;
    struct cached_dir_entry *one_entryp = in_one_entryp;
    struct cached_dir_entry *entryp = NULL;
    struct cached_dir_entry *prevp = NULL;
    
    if (dvp == NULL) {
        SMBERROR("dvp is null \n");
        return;
    }
    dnp = VTOSMB(dvp);
    
    if (!vnode_isdir(dvp)) {
        SMBERROR("dvp is not a dir \n");
        return;
    }

    if (!is_locked) {
        lck_mtx_lock(&dnp->d_enum_cache_list_lock);
    }
    
    entryp = cachep->list;
    
    /* Search for the matching entry and remove it */
    while (entryp != NULL) {
        if (entryp == one_entryp) {
            if (entryp == cachep->list) {
                cachep->list = entryp->next;
            }
            else {
                if (prevp != NULL) {
                    prevp->next = entryp->next;
                }
                else {
                    SMBERROR("prevp is NULL \n");
                }
            }
            
			vfs_removename(entryp->name);
            SMB_FREE_TYPE(struct cached_dir_entry, entryp);
            break;
        }
        
        prevp = entryp;
        entryp = entryp->next;
    }
    
    cachep->count -= 1;
    
    if (!is_locked) {
        lck_mtx_unlock(&dnp->d_enum_cache_list_lock);
    }
}

void
smb_global_dir_cache_add_entry(vnode_t dvp, int is_locked)
{
	struct smbnode *dnp = NULL;
	struct global_dir_cache_entry *entryp = NULL;
	struct global_dir_cache_entry *currentp = NULL;
	uint64_t current_dir_cnt = 0;
	struct timespec	ts;
	
	if (dvp == NULL) {
		SMBERROR("dvp is null \n");
		return;
	}
	dnp = VTOSMB(dvp);
	
	if (!vnode_isdir(dvp)) {
		SMBERROR("dvp is not a dir \n");
		return;
	}
	
	SMB_LOG_LEASING_LOCK(dnp, "Adding dir <%s> entry count <%lld> \n",
						   dnp->n_name, dnp->d_main_cache.count);
	
	/* Create a new cached_dir_entry and insert it into the list */
    SMB_MALLOC_TYPE(entryp, struct global_dir_cache_entry, Z_WAITOK_ZERO);
	
	entryp->name = vfs_addname(dnp->n_name, (uint32_t) dnp->n_nmlen, 0, 0);
	entryp->name_len = dnp->n_nmlen;
	entryp->dvp = dvp;
	entryp->dir_vid = vnode_vid(dvp);
	
	/* Set last accessed time */
	nanouptime(&entryp->last_access_time);
	
	entryp->cached_cnt = dnp->d_main_cache.count;
	entryp->next = NULL;

	if (!is_locked) {
		lck_mtx_lock(&global_dir_cache_lock);
	}
	
	if (global_dir_cache_head == NULL) {
		/* No other entries, so we are the first */
		global_dir_cache_head = entryp;
	}
	else {
		/* look for last entry in the list */
		currentp = global_dir_cache_head;
		
		/* also look for least recently accessed */
		ts = currentp->last_access_time;
		
		while (currentp->next != NULL) {
			/* Ignore dirs that have no entries cached */
			if (currentp->cached_cnt > 0) {
				/* Count up number of dirs */
				current_dir_cnt += 1;

				/*
				 * Is this entry the least recently accessed and does it have any
				 * entries that can be removed?
				 */
				if (timespeccmp(&ts, &currentp->last_access_time, >)) {
					ts = currentp->last_access_time;
				}
			}
			
			currentp = currentp->next;
		}
		
		/* put it at the end of the list */
		currentp->next = entryp;
	}
	
	if (!is_locked) {
		lck_mtx_unlock(&global_dir_cache_lock);
	}
}

/* This is called when the OS needs us to free memory */
void
smb_global_dir_cache_low_memory(int free_all, void *context_ptr)
{
#pragma unused(context_ptr)
	
	struct global_dir_cache_entry *currentp = NULL;
	vnode_t	dvp = NULL;
    int error;

    SMB_LOG_KTRACE(SMB_DBG_GLOBAL_DIR_LOW_MEMORY | DBG_FUNC_START,
                   free_all, 0, 0, 0, 0);

    if (free_all) {
		SMB_LOG_LEASING("Low memory call back with <%d> \n",
                        free_all);

		lck_mtx_lock(&global_dir_cache_lock);
		
		currentp = global_dir_cache_head;
		
		while (currentp != NULL) {
			/* Ignore dirs that have no entries cached */
			if (currentp->cached_cnt > 0) {
				/* Try to retrieve the vnode */
				dvp = currentp->dvp;
				if (vnode_getwithvid(currentp->dvp, currentp->dir_vid)) {
                    /*
                     * Must have gotten reclaimed, set its cached_cnt to 0 so
                     * we will ignore it until it gets removed.
                     */
                    currentp->cached_cnt = 0;
					dvp = NULL;
				}
				else {
					if (vnode_tag(dvp) != VT_CIFS) {
						SMBERROR("vnode_getwithvid found non SMB vnode???\n");
					}
                    else {
                        /*
                         * Try to get the lock and if fail, then it must be busy
                         * so skip this dir
                         */
                        error = smbnode_trylock(VTOSMB(dvp), SMBFS_EXCLUSIVE_LOCK);
                        if (error == 0) {
                            /* Got the lock */
                            SMB_LOG_LEASING("Removing dir cache entries for <%s> \n",
                                            currentp->name);

                            /*
                             * SMBFS_EXCLUSIVE_LOCK should be enough to
                             * keep others from accessing the dir while we close
                             * it.
                             */

                            /*
                             * 41800260/47501728 Be careful here. We can get
                             * called while machine is asleep so we cant send
                             * any request. Also can be called from mbuf
                             * allocator so another reason to not try sending
                             * any SMB requests. Just empty the dir cache and
                             * that should be enough.
                             */
                            smb_dir_cache_remove(dvp, &VTOSMB(dvp)->d_main_cache, "main", "low memory", 0, 0);
                            smb_dir_cache_remove(dvp, &VTOSMB(dvp)->d_overflow_cache, "overflow", "low memory", 0, 0);

                            /*
                             * Assume its cached count is now 0. Its actual count will
                             * get updated after its refilled.
                             */
                            currentp->cached_cnt = 0;

                            smbnode_unlock(VTOSMB(dvp));
                        }
                    }
				}
				
				if (dvp != NULL) {
					vnode_put(dvp);
				}
			}
			
			currentp = currentp->next;
		}
		
		lck_mtx_unlock(&global_dir_cache_lock);
	}

    SMB_LOG_KTRACE(SMB_DBG_GLOBAL_DIR_LOW_MEMORY | DBG_FUNC_END,
                   0, 0, 0, 0, 0);
}

void
smb_global_dir_cache_prune(void *oldest_ptr, int is_locked,
                           vfs_context_t context)
{
	struct global_dir_cache_entry *currentp = NULL;
	struct global_dir_cache_entry *oldestp = oldest_ptr;
	vnode_t	dvp = NULL;
	uint64_t current_dir_cnt = 0;
	struct timespec	ts;
	int32_t max_attempts = 50;	/* Safety to keep from looping forever */
    int error;

    SMB_LOG_KTRACE(SMB_DBG_GLOBAL_DIR_CACHE_PRUNE | DBG_FUNC_START, is_locked, 0, 0, 0, 0);

	if (!is_locked) {
		lck_mtx_lock(&global_dir_cache_lock);
	}

again:
	max_attempts -= 1;
	if (max_attempts <= 0) {
		SMB_LOG_DIR_CACHE2("Max attempts <%d> reached \n", max_attempts);
		goto exit;
	}
	
	/* if oldestp is non NULL, then free that entry first */
	if (oldestp != NULL) {
		if (oldestp->dvp == NULL) {
			SMBERROR("dvp is null? \n");
		}
		else {
			/* Try to retrieve the vnode */
			dvp = oldestp->dvp;
			if (vnode_getwithvid(oldestp->dvp, oldestp->dir_vid)) {
                /*
                 * Must have gotten reclaimed, set its cached_cnt to 0 so
                 * we will ignore it until it gets removed.
                 */
                oldestp->cached_cnt = 0;
				dvp = NULL;
			}
			else {
				if (vnode_tag(dvp) != VT_CIFS) {
					SMBERROR("vnode_getwithvid found non SMB vnode???\n");
				}
				else {
					/* 
					 * Try to get the lock and if fail, then it must be busy
					 * so skip this dir
					 */
                    error = smbnode_trylock(VTOSMB(dvp), SMBFS_EXCLUSIVE_LOCK);
                    if (error == 0) {
						/* Got the lock */
						SMB_LOG_LEASING("Removing dir cache entries for <%s> \n",
                                        oldestp->name);

                        /*
                         * SMBFS_EXCLUSIVE_LOCK should be enough to
                         * keep others from accessing the dir while we close
                         * it.
                         */
                        smbfs_closedirlookup(VTOSMB(dvp), 0, "smb_global_dir_cache_prune", context);

						smb_dir_cache_remove(dvp, &VTOSMB(dvp)->d_main_cache, "main", "prune", 1, 0);
						smb_dir_cache_remove(dvp, &VTOSMB(dvp)->d_overflow_cache, "overflow", "prune", 1, 0);
						
						/*
						 * Assume its cached count is now 0. Its actual count will
						 * get updated after its refilled.
						 */
						oldestp->cached_cnt = 0;
						
                        smbnode_unlock(VTOSMB(dvp));
					}
				}
			}
			
			if (dvp != NULL) {
				vnode_put(dvp);
			}
		}
	}
	
	/* Count number of current entries and find least recently used */
	currentp = global_dir_cache_head;
	oldestp = NULL;
	current_dir_cnt = 0;
	
	while (currentp != NULL) {
		/* Ignore dirs that have no entries cached */
		if (currentp->cached_cnt > 0) {
			/* Find first dir with entries and set that as the oldest */
			if (oldestp == NULL) {
				oldestp = currentp;
				ts = currentp->last_access_time;
			}

			/* Count up number of dirs */
			current_dir_cnt += 1;
			
			/*
			 * Is this entry the least recently accessed and does it have any
			 * entries that can be removed?
			 */
			if (timespeccmp(&ts, &currentp->last_access_time, >)) {
				ts = currentp->last_access_time;
				oldestp = currentp;
			}
		}

		currentp = currentp->next;
	}
	
    SMB_LOG_KTRACE(SMB_DBG_GLOBAL_DIR_CACHE_PRUNE | DBG_FUNC_NONE, 0xabc001, current_dir_cnt, 0, 0, 0);

    /* Do we have too many cached dir or entries? */
	if (current_dir_cnt > g_max_dirs_cached) {
		goto again;
	}
	
exit:
	if (!is_locked) {
		lck_mtx_unlock(&global_dir_cache_lock);
	}
    
    SMB_LOG_KTRACE(SMB_DBG_GLOBAL_DIR_CACHE_PRUNE | DBG_FUNC_END, 0, is_locked, 0, 0, 0);
}

void
smb_global_dir_cache_remove(int is_locked, int remove_all)
{
	struct global_dir_cache_entry *entryp = NULL;
	struct global_dir_cache_entry *currentp = NULL;
    struct global_dir_cache_entry *prevp = NULL;

	if (!is_locked) {
		lck_mtx_lock(&global_dir_cache_lock);
	}

    if (remove_all == 1) {
        SMB_LOG_LEASING("Removing all dirs \n");
        entryp = global_dir_cache_head;
        while (entryp != NULL) {
            /* wipe out the enum cache entries */
            currentp = entryp;
            entryp = entryp->next;

            vfs_removename(currentp->name);
            SMB_FREE_TYPE(struct global_dir_cache_entry, currentp);
        }

        global_dir_cache_head = NULL;
    }
    else {
        /* Check for reclaimed dir vnodes and remove them from list */
        entryp = global_dir_cache_head;
        while (entryp != NULL) {
            /* Check to see if this dir vnode has been reclaimed */
            if (vnode_getwithvid(entryp->dvp, entryp->dir_vid)) {
                currentp = entryp;
                entryp = entryp->next;
                /* Leave prevp unchanged since removing currentp */

                if (currentp == global_dir_cache_head) {
                    /* Its first in list */
                    global_dir_cache_head = currentp->next;
                }
                else {
                    /* Not first, so reset next pointers */
                    if (prevp != NULL) {
                        prevp->next = currentp->next;
                    }
                    else {
                        /* Should never happen */
                        SMBERROR("prevp is NULL \n");
                    }
                }

                SMB_LOG_LEASING("Removing dir <%s> \n",
                                currentp->name);

                vfs_removename(currentp->name);
                SMB_FREE_TYPE(struct global_dir_cache_entry, currentp);
                currentp = NULL;

                /* on to next entryp */
                continue;
            }
            else {
                vnode_put(entryp->dvp);
            }

            prevp = entryp;
            entryp = entryp->next;
        }
    }

    if (!is_locked) {
		lck_mtx_unlock(&global_dir_cache_lock);
	}
}

void
smb_global_dir_cache_remove_one(vnode_t dvp, int is_locked)
{
	struct smbnode *dnp = NULL;
	struct global_dir_cache_entry *entryp = NULL;
	struct global_dir_cache_entry *prevp = NULL;
	uint32_t dir_vid;
	
	if (dvp == NULL) {
		SMBERROR("dvp is null \n");
		return;
	}
	dnp = VTOSMB(dvp);
	
	if (!vnode_isdir(dvp)) {
		SMBERROR("dvp is not a dir \n");
		return;
	}
	
	/* We search by the vnode vid which should be unique */
	dir_vid = vnode_vid(dvp);
	
	if (!is_locked) {
		lck_mtx_lock(&global_dir_cache_lock);
	}
	
	/* Search for the matching entry and remove it */
	entryp = global_dir_cache_head;
	
	while (entryp != NULL) {
		if (dir_vid == entryp->dir_vid) {
			if (entryp == global_dir_cache_head) {
				/* Its first in list */
				global_dir_cache_head = entryp->next;
			}
			else {
				/* Not first, so reset next pointers */
				if (prevp != NULL) {
					prevp->next = entryp->next;
				}
				else {
					SMBERROR("prevp is NULL \n");
				}
			}
			
			SMB_LOG_LEASING_LOCK(dnp, "Removing dir <%s> \n", dnp->n_name);

			vfs_removename(entryp->name);
            SMB_FREE_TYPE(struct global_dir_cache_entry, entryp);
			break;
		}
		
		prevp = entryp;
		entryp = entryp->next;
	}
	
	if (!is_locked) {
		lck_mtx_unlock(&global_dir_cache_lock);
	}
}

int32_t
smb_global_dir_cache_update_entry(vnode_t dvp)
{
	struct smbnode *dnp = NULL;
	struct global_dir_cache_entry *entryp = NULL;
	uint32_t dir_vid;
	
	if (dvp == NULL) {
		SMBERROR("dvp is null \n");
		return (EINVAL);
	}
	dnp = VTOSMB(dvp);
	
	if (!vnode_isdir(dvp)) {
		SMBERROR("dvp is not a dir \n");
		return (EINVAL);
	}
	
	/* We search by the vnode vid which should be unique */
	dir_vid = vnode_vid(dvp);
	
	lck_mtx_lock(&global_dir_cache_lock);
	
	/* Now search the list until we find a match */
	for (entryp = global_dir_cache_head; entryp; entryp = entryp->next) {
		if (dir_vid == entryp->dir_vid) {
			/* found it, now update it */
			if (entryp->cached_cnt != (uint64_t) dnp->d_main_cache.count) {
				SMB_LOG_LEASING_LOCK(dnp, "Updating dir <%s> old entry count <%lld> new entry cnt <%lld> \n",
									   dnp->n_name, entryp->cached_cnt, dnp->d_main_cache.count);
			}
			
			if ((entryp->name_len == dnp->n_nmlen) &&
				(bcmp(entryp->name, dnp->n_name, entryp->name_len) == 0)) {
				/* Name did not change, so do not need to update name */
			}
			else {
				/* Name changed, so update it */
				vfs_removename(entryp->name);
				entryp->name = vfs_addname(dnp->n_name, (uint32_t) dnp->n_nmlen, 0, 0);
				entryp->name_len = dnp->n_nmlen;
			}
			
			/* Set last accessed time */
			nanouptime(&entryp->last_access_time);
			
			entryp->cached_cnt = dnp->d_main_cache.count;
			
			lck_mtx_unlock(&global_dir_cache_lock);
			return(0);
		}
	}
	
	lck_mtx_unlock(&global_dir_cache_lock);
	return(ENOENT);	/* No match found */
}

#pragma mark - buf_map_range helper functions

/*
 * Copied from Xsan (acfs_buf_map() functions) for vnop_strategy buf_map
 * handling to avoid ENOMEM errors due to contention over the upl
 */

#define NUM_ADDR_BUCKETS 32
lck_mtx_t smbfs_buf_map_locks[NUM_ADDR_BUCKETS];

void
smbfs_init_buf_map(void)
{
    for (int i = 0; i < NUM_ADDR_BUCKETS; i++ ) {
        lck_mtx_init(&smbfs_buf_map_locks[i], smbfs_mutex_group, smbfs_lock_attr);
    }
}

void
smbfs_teardown_buf_map(void)
{
    for (int i = 0; i < NUM_ADDR_BUCKETS; i++ ) {
        lck_mtx_destroy(&smbfs_buf_map_locks[i], smbfs_mutex_group);
    }
}

uint32_t
smbfs_hash_upl_addr(void *addr)
{
    intptr_t ip = (intptr_t)addr;
    return(crc32(0, (void *)&ip, sizeof(addr)) & (NUM_ADDR_BUCKETS - 1));
}

int
smbfs_buf_map(struct buf *bp, caddr_t *io_addr, vm_prot_t prot)
{
    void *upl = NULL;
    uint32_t bucket = 0;
    int error = 0;

    if (bp == NULL) {
        SMBERROR("bp is null? \n");
        return EINVAL;
    }
    
    SMB_LOG_KTRACE(SMB_DBG_BUF_MAP | DBG_FUNC_START, 0, 0, 0, 0, 0);
    
    /* Get the upl that this buf belongs to */
    upl = buf_upl(bp);

    SMB_LOG_IO("bp 0x%lx, upl 0x%lx, offset %lld, size %u, %s \n",
               smb_hideaddr(bp), smb_hideaddr(upl),
               ((off_t)buf_blkno(bp)) * PAGE_SIZE, buf_count(bp),
               (prot == PROT_WRITE) ? "PROT_WRITE" : "PROT-READ");

    /* Hash the upl */
    *io_addr = NULL;
    bucket = smbfs_hash_upl_addr(upl);

    /*
     * The buf_map_range_with_prot() doesn't support concurrent mappings of UPL.
     * Other worker threads executing smbfs_do_strategy() for the split IOs could be
     * trying to map the same UPL so serialization is needed here.
     * Ideally, the lock should be per UPL but lock per bucket is good enough
     * as we don't expect high collision rate of different UPLs hashing to the
     * same bucket.
     */
    lck_mtx_lock(&smbfs_buf_map_locks[bucket]);

    SMB_LOG_KTRACE(SMB_DBG_BUF_MAP | DBG_FUNC_NONE, 0xabc001, bucket, 0, 0, 0);

    error = buf_map_range_with_prot(bp, io_addr, prot);

    if (error) {
        /* Should not happen */
        SMBERROR("buf_map_range_with_prot failed %d, bp 0x%lx, upl 0x%lx, offset %lld, size %u, %s\n",
                 error, smb_hideaddr(bp), smb_hideaddr(upl),
                 ((off_t)buf_blkno(bp)) * PAGE_SIZE, buf_count(bp),
                 (prot == PROT_WRITE) ? "PROT_WRITE" : "PROT-READ");
        
        lck_mtx_unlock(&smbfs_buf_map_locks[bucket]);
    }

    SMB_LOG_KTRACE(SMB_DBG_BUF_MAP | DBG_FUNC_END, error, 0, 0, 0, 0);
    return error;
}

int
smbfs_buf_unmap(struct buf *bp)
{
    void *upl = NULL;
    uint32_t bucket = 0;
    int error = 0;

    if (bp == NULL) {
        SMBERROR("bp is null? \n");
        return EINVAL;
    }
    
    SMB_LOG_KTRACE(SMB_DBG_BUF_UNMAP | DBG_FUNC_START, 0, 0, 0, 0, 0);

    /* Get the upl that this buf belongs to */
    upl = buf_upl(bp);

    SMB_LOG_IO("bp 0x%lx, upl 0x%lx, offset %lld, size %u\n",
               smb_hideaddr(bp), smb_hideaddr(upl),
               ((off_t)buf_blkno(bp)) * PAGE_SIZE, buf_count(bp));

    error = buf_unmap_range(bp);
    if (error) {
        /* Should not happen */
        SMBERROR("buf_unmap_range failed %d, bp 0x%lx, upl 0x%lx, offset %lld, size %u\n",
                 error, smb_hideaddr(bp), smb_hideaddr(upl),
                 ((off_t)buf_blkno(bp)) * PAGE_SIZE, buf_count(bp));
    }
    
    /* Free lock this upl belong to */
    bucket = smbfs_hash_upl_addr(upl);

    SMB_LOG_KTRACE(SMB_DBG_BUF_UNMAP | DBG_FUNC_NONE, 0xabc001, bucket, 0, 0, 0);

    lck_mtx_unlock(&smbfs_buf_map_locks[bucket]);

    SMB_LOG_KTRACE(SMB_DBG_BUF_UNMAP | DBG_FUNC_END, error, 0, 0, 0, 0);
    return error;
}


#pragma mark - Misc Helper Functions

void
smb2fs_smb_file_id_check(struct smb_share *share, uint64_t ino,
                         char *network_name, uint32_t network_name_len)
{
    uint32_t no_file_ids = 0;
	
    /*
     * Check to see if server supports File IDs or not
     * Watch out because the ".." in every Query Dir response has File ID of 0
     * which is supposed to be illegal. Sigh.
     */
    if (SS_TO_SESSION(share)->session_misc_flags & SMBV_HAS_FILEIDS) {
        if (ino == 0) {
            no_file_ids = 1;
			
            if ((network_name != NULL) && (network_name_len > 0)) {
                if ((network_name_len == 2 &&
                     letohs(*(uint16_t * ) network_name) == 0x002e) ||
                    (network_name_len == 4 &&
                     letohl(*(uint32_t *) network_name) == 0x002e002e)) {
                        /*
                         * Its the ".." dir so allow the File ID of 0. "." and ".."
                         * dirs are ignored by smbfs_findnext so we can safely leave
                         * their fa_ino to be 0
                         */
                        no_file_ids = 0;
                    }
            }
        }
    }
    
    if (no_file_ids == 1) {
        SMBDEBUG("Server does not support File IDs \n");
        SS_TO_SESSION(share)->session_misc_flags &= ~SMBV_HAS_FILEIDS;
    }
}

uint64_t
smb2fs_smb_file_id_get(struct smbmount *smp, uint64_t ino, const char *name)
{
    uint64_t ret_ino;
    
    if (ino == smp->sm_root_ino) {
        /* If its the root File ID, then return SMBFS_ROOT_INO */
        ret_ino = SMBFS_ROOT_INO;
    }
    else {
        /*
         * If actual File ID is SMBFS_ROOT_INO, then return the root File ID
         * instead.
         */
        if (ino == SMBFS_ROOT_INO) {
            ret_ino = smp->sm_root_ino;
        }
        else {
            if (ino == 0) {
                /* This should never happen */
                SMBERROR("File ID of 0 in <%s>? \n",
                         ((name != NULL) ? name : "unknown name"));
                ret_ino = SMBFS_ROOT_INO;
            }
            else {
                ret_ino = ino;
            }
        }
    }
    
    return (ret_ino);
}

/*
 * This differs from smbfs_fullpath in
 * 1) no pad byte
 * 2) Unicode is always used
 * 3) no end null bytes
 */
int
smb2fs_fullpath(struct mbchain *mbp, struct smbnode *dnp, 
                const char *namep, size_t in_name_len, 
                const char *strm_namep, size_t in_strm_name_len,
                int name_flags, uint8_t sep_char)
{
	int error = 0; 
	const char *name = (namep ? namep : NULL);
	const char *strm_name = (strm_namep ? strm_namep : NULL);
	size_t name_len = in_name_len;
	size_t strm_name_len = in_strm_name_len;
    size_t len = 0;
    uint8_t stream_sep_char = ':';
    
	if (dnp != NULL) {
		struct smbmount *smp = dnp->n_mount;
		
		error = smb_fphelp(smp, mbp, dnp, TRUE, &len);
		if (error) {
			return error;
        }
	}
    
	if (name) {
        if (name_len > (SMB_MAXFNAMELEN * 2)) {
            SMBERROR("%s: Illegal name len %zu\n", __FUNCTION__, name_len);
            return EINVAL;
        }
        /* Add separator char only if we added a path from above */
        if (len > 0) {
            error = mb_put_uint16le(mbp, sep_char);
            if (error) {
                return error;
            }
        }
        
		error = smb_put_dmem(mbp, name, name_len, name_flags, TRUE, NULL);
		if (error) {
			return error;
        }
	}
    
    /* Add Stream Name */
	if (strm_name) {
        if (strm_name_len > (SMB_MAXFNAMELEN * 2)) {
            SMBERROR("%s: Illegal stream len %zu\n", __FUNCTION__ , strm_name_len);
            return EINVAL;
        }
        /* Add separator char */
        error = mb_put_uint16le(mbp, stream_sep_char);
        if (error) {
            return error;
        }
        
		error = smb_put_dmem(mbp, strm_name, strm_name_len, name_flags, TRUE, NULL);
		if (error) {
			return error;
        }
	}

	return error;
}

/* Caller will need to call vnode_put() when done with the parent vnode */
vnode_t
smbfs_smb_get_parent(struct smbnode *np, uint64_t flags)
{
    int error;
    vnode_t vp = NULL;
    
    if (flags & kShareLock) {
        lck_rw_lock_shared(&np->n_parent_rwlock);
    }
    
    if ((np->n_parent_vid != 0) &&
        (np->n_parent_vnode != NULL)) {
        /* Parent vid of 0 means no parent */
        vp = np->n_parent_vnode;
        error = vnode_getwithvid(vp, np->n_parent_vid);
        if (error != 0) {
            SMBWARNING_LOCK(np, "Failed (%d) to get parent by vid for <%s> \n",
                            error, np->n_name);
            vp = NULL;
        }
    }
    
    if (flags & kShareLock) {
        lck_rw_unlock_shared(&np->n_parent_rwlock);
    }
    
    return(vp);
}

