/*
 * Copyright (c) 2012-2014 Apple Inc. All rights reserved.
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

#include <sys/mount.h>
#include <sys/vm.h>
#include <sys/kauth.h>
#include <sys/smb_apple.h>
#include <sys/msfscc.h>

#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <smbfs/smbfs.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_rq_2.h>
#include <netsmb/smb_conn.h>
#include <smbfs/smbfs_subr.h>
#include <smbfs/smbfs_subr_2.h>
#include <smbfs/smbfs_attrlist.h>

#define SMBFS_AVERAGE_NAME_SIZE   22
#define AVERAGE_SMBDIRENTRY_SIZE  (8 + SMBFS_AVERAGE_NAME_SIZE + 4)

/* Packing routines: */
static int attrblksize(struct attrlist *attrlist);

static char* mountpointname(struct mount *mp);

static void packattrblk(struct attrblock *abp,
                        struct smbmount *smp,
                        struct vnode *vp,
                        struct smbfs_fctx *ctx,
                        struct vfs_context *context);

static void packcommonattr(struct attrblock *abp,
                           struct smbmount *smp,
                           struct smbfs_fctx *ctx,
                           struct vfs_context *context);

static void packdirattr(struct attrblock *abp,
                        struct vnode *vp,
                        struct smbfs_fctx *ctx);

static void packfileattr(struct attrblock *abp,
                         struct smbmount *smp,
                         struct smbfs_fctx *ctx,
                         struct vfs_context *context);

static void packnameattr(struct attrblock *abp, struct smbfs_fctx *ctx,
                         const u_int8_t *name, size_t namelen);


/*==================== Attribute list support routines ====================*/

/*
 * Calculate the total size of an attribute block.
 */
static int
attrblksize(struct attrlist *attrlist)
{
	int size;
	attrgroup_t a;
	int sizeof_timespec;
	boolean_t is_64_bit = proc_is64bit(current_proc());
	
    if (is_64_bit) 
        sizeof_timespec = sizeof(struct user64_timespec);
    else
        sizeof_timespec = sizeof(struct user32_timespec);
    
	DBG_ASSERT((attrlist->commonattr & ~ATTR_CMN_VALIDMASK) == 0);
    
	DBG_ASSERT((attrlist->volattr & ~ATTR_VOL_VALIDMASK) == 0);
    
	DBG_ASSERT((attrlist->dirattr & ~ATTR_DIR_VALIDMASK) == 0);
    
	DBG_ASSERT((attrlist->fileattr & ~ATTR_FILE_VALIDMASK) == 0);
    
	DBG_ASSERT((attrlist->forkattr & ~ATTR_FORK_VALIDMASK) == 0);
    
	size = 0;
	
	if ((a = attrlist->commonattr) != 0) {
		if (a & ATTR_CMN_NAME) size += sizeof(struct attrreference);
		if (a & ATTR_CMN_DEVID) size += sizeof(dev_t);
		if (a & ATTR_CMN_FSID) size += sizeof(fsid_t);
		if (a & ATTR_CMN_OBJTYPE) size += sizeof(fsobj_type_t);
		if (a & ATTR_CMN_OBJTAG) size += sizeof(fsobj_tag_t);
		if (a & ATTR_CMN_OBJID) size += sizeof(fsobj_id_t);
		if (a & ATTR_CMN_OBJPERMANENTID) size += sizeof(fsobj_id_t);
		if (a & ATTR_CMN_PAROBJID) size += sizeof(fsobj_id_t);
		if (a & ATTR_CMN_SCRIPT) size += sizeof(text_encoding_t);
		if (a & ATTR_CMN_CRTIME) size += sizeof_timespec;
		if (a & ATTR_CMN_MODTIME) size += sizeof_timespec;
		if (a & ATTR_CMN_CHGTIME) size += sizeof_timespec;
		if (a & ATTR_CMN_ACCTIME) size += sizeof_timespec;
		if (a & ATTR_CMN_BKUPTIME) size += sizeof_timespec;
		if (a & ATTR_CMN_FNDRINFO) size += 32 * sizeof(u_int8_t);
		if (a & ATTR_CMN_OWNERID) size += sizeof(uid_t);
		if (a & ATTR_CMN_GRPID) size += sizeof(gid_t);
		if (a & ATTR_CMN_ACCESSMASK) size += sizeof(u_int32_t);
		if (a & ATTR_CMN_FLAGS) size += sizeof(u_int32_t);
		if (a & ATTR_CMN_USERACCESS) size += sizeof(u_int32_t);
		if (a & ATTR_CMN_FILEID) size += sizeof(u_int64_t);
		if (a & ATTR_CMN_PARENTID) size += sizeof(u_int64_t);
	}
	if ((a = attrlist->dirattr) != 0) {
		if (a & ATTR_DIR_LINKCOUNT) size += sizeof(u_int32_t);
		if (a & ATTR_DIR_ENTRYCOUNT) size += sizeof(u_int32_t);
		if (a & ATTR_DIR_MOUNTSTATUS) size += sizeof(u_int32_t);
	}
	if ((a = attrlist->fileattr) != 0) {
		if (a & ATTR_FILE_LINKCOUNT) size += sizeof(u_int32_t);
		if (a & ATTR_FILE_TOTALSIZE) size += sizeof(off_t);
		if (a & ATTR_FILE_ALLOCSIZE) size += sizeof(off_t);
		if (a & ATTR_FILE_IOBLOCKSIZE) size += sizeof(u_int32_t);
		if (a & ATTR_FILE_CLUMPSIZE) size += sizeof(u_int32_t);
		if (a & ATTR_FILE_DEVTYPE) size += sizeof(u_int32_t);
		if (a & ATTR_FILE_DATALENGTH) size += sizeof(off_t);
		if (a & ATTR_FILE_DATAALLOCSIZE) size += sizeof(off_t);
		if (a & ATTR_FILE_RSRCLENGTH) size += sizeof(off_t);
		if (a & ATTR_FILE_RSRCALLOCSIZE) size += sizeof(off_t);
	}
    
	return (size);
}

static char*
mountpointname(struct mount *mp)
{
	size_t namelength = strlen(vfs_statfs(mp)->f_mntonname);
	int foundchars = 0;
	char *c;
	
	if (namelength == 0)
		return (NULL);
	
	/*
	 * Look backwards through the name string, looking for
	 * the first slash encountered (which must precede the
	 * last part of the pathname).
	 */
	for (c = vfs_statfs(mp)->f_mntonname + namelength - 1;
	     namelength > 0; --c, --namelength) {
		if (*c != '/') {
			foundchars = 1;
		} else if (foundchars) {
			return (c + 1);
		}
	}
	
	return (vfs_statfs(mp)->f_mntonname);
}

/*
 * Pack cnode attributes into an attribute block.
 */
static void
packattrblk(struct attrblock *abp,
            struct smbmount *smp,
            struct vnode *vp,
            struct smbfs_fctx *ctx,
            struct vfs_context *context)
{
	struct attrlist *attrlistp = abp->ab_attrlist;
	uint32_t is_dir;
    int error;
    uint32_t stream_flags = 0;
	attrgroup_t file_attr = attrlistp->fileattr;
	attrgroup_t common_attr = attrlistp->commonattr;
	struct smbnode *np = NULL;
    uint32_t need_rsrc_fork = 0;
    size_t afp_size = 0;
    uint8_t	afp_info[60] = {0};
    uint8_t	zero_finfo[32] = {0};
	uio_t afp_uio = NULL;
    SMBFID fid = 0;
	struct timespec ts;
    uint32_t rsrc_fork_from_cache = 0;
    uint32_t finder_info_from_cache = 0;
    uint32_t max_access_from_cache = 0;

    SMB_LOG_KTRACE(SMB_DBG_PACK_ATTR_BLK | DBG_FUNC_START,
                   attrlistp->commonattr,
                   attrlistp->fileattr,
                   attrlistp->dirattr, 0, 0);

    /* Do we need Resource Fork info? */
    is_dir = (ctx->f_attr.fa_attr & SMB_EFA_DIRECTORY) ? 1 : 0;
    if ((!is_dir) &&
        ((ATTR_FILE_TOTALSIZE & file_attr) ||
         (ATTR_FILE_ALLOCSIZE & file_attr) ||
         (ATTR_FILE_RSRCLENGTH & file_attr) ||
         (ATTR_FILE_RSRCALLOCSIZE & file_attr))) {
            need_rsrc_fork = 1;
    }

    /* 
     * We may already have all the meta data we need from Mac <-> Mac (not yet
     * implemented) or dont need Resource Fork, Finder Info, or Max Access data.
     * If we do have all the needed meta data, then just go update vnode caches
     * if we have a vnode and then pack the return attribute block.
     */
    if (((need_rsrc_fork) && (ctx->f_attr.fa_valid_mask & FA_RSRC_FORK_VALID)) &&
        ((ATTR_CMN_FNDRINFO & common_attr) && (ctx->f_attr.fa_valid_mask & FA_FINDERINFO_VALID)) &&
        ((ATTR_CMN_USERACCESS & common_attr) && (ctx->f_attr.fa_valid_mask & FA_MAX_ACCESS_VALID))) {
        SMB_LOG_KTRACE(SMB_DBG_PACK_ATTR_BLK | DBG_FUNC_NONE,
                       0xabc001, 0, 0, 0, 0);
        goto update_caches;
    }
    
    /*
     * Figure out what attributes we are missing and then go get them from
     * either the vnode or from the server.
     */

    /*
     * Do we have a vnode that already has the attributes we need?
     */
    if (vp != NULL) {
        np = VTOSMB(vp);
        
        /* Check cached Resource Fork info */
        if ((need_rsrc_fork) &&
            !(ctx->f_attr.fa_valid_mask & FA_RSRC_FORK_VALID)) {

            lck_mtx_lock(&np->rfrkMetaLock);
            
            if (np->rfrk_cache_timer != 0) {
                /* Resource fork data is valid in vnode so use it */
                rsrc_fork_from_cache = 1;
                ctx->f_attr.fa_valid_mask |= FA_RSRC_FORK_VALID;
                ctx->f_attr.fa_rsrc_size = np->rfrk_size;
                ctx->f_attr.fa_rsrc_alloc = np->rfrk_alloc_size;
                
                SMB_LOG_KTRACE(SMB_DBG_PACK_ATTR_BLK | DBG_FUNC_NONE,
                               0xabc002, 0, 0, 0, 0);
          }
            
            lck_mtx_unlock(&np->rfrkMetaLock);
        }
        
        /* Check cached Finder Info */
        if ((ATTR_CMN_FNDRINFO & common_attr) &&
            !(ctx->f_attr.fa_valid_mask & FA_FINDERINFO_VALID)) {

            if (np->finfo_cache_timer != 0) {
                /* Finder Info data is valid in vnode so use it */
                finder_info_from_cache = 1;
                ctx->f_attr.fa_valid_mask |= FA_FINDERINFO_VALID;
                bcopy(&np->finfo, ctx->f_attr.fa_finder_info, sizeof(u_int8_t) * 32);
                
                SMB_LOG_KTRACE(SMB_DBG_PACK_ATTR_BLK | DBG_FUNC_NONE,
                               0xabc003, 0, 0, 0, 0);
            }
        }

        /* Check cached Max Info */
        if ((ATTR_CMN_USERACCESS & common_attr) &&
            !(ctx->f_attr.fa_valid_mask & FA_MAX_ACCESS_VALID)) {
            if (timespeccmp(&np->maxAccessRightChTime, &np->n_chtime, ==)) {

                /* Max Access data is valid in vnode so use it */
                max_access_from_cache = 1;
                ctx->f_attr.fa_valid_mask |= FA_MAX_ACCESS_VALID;
                ctx->f_attr.fa_max_access = np->maxAccessRights;
                
                SMB_LOG_KTRACE(SMB_DBG_PACK_ATTR_BLK | DBG_FUNC_NONE,
                               0xabc004, 0, 0, 0, 0);
            }
        }
    }
    
    /* Are we still missing attribute information? */
    if (((need_rsrc_fork) && !(ctx->f_attr.fa_valid_mask & FA_RSRC_FORK_VALID)) ||
        ((ATTR_CMN_FNDRINFO & common_attr) && !(ctx->f_attr.fa_valid_mask & FA_FINDERINFO_VALID)) ||
        ((ATTR_CMN_USERACCESS & common_attr) && !(ctx->f_attr.fa_valid_mask & FA_MAX_ACCESS_VALID))) {
        /*
        * This will get us
        * 1) Resource Fork sizes
        * 2) Whether Finder Info exists or not on the item
        * 3) For SMB 2/3, gets the max access on the item
        *
        * If we have to ask the server for the resource fork info or the 
        * user access, do it now as this will also tell us if there is any
        * Finder Info on the file or not. For SMB 2/3, it will also get us the
        * max access which is used for ATTR_CMN_USERACCESS.
        *
        * Best case (SMB 2/3) - Just this one call because no Finder Info found
        * Worst case (SMB 2/3) - This call and another call to read Finder Info
        *
        * Best case (SMB 1) - This call and 2 calls (Create/Read + Close) to
        *                       read Finder Info which will get the max access 
        *                       for SMB 1
        * Worst case (SMB 1) - This call and 2 calls (Create + Close) to get
        *                        max access because there is no Finder Info
        */
        error = smbfs_smb_qstreaminfo(ctx->f_share, ctx->f_dnp, (is_dir) ? VDIR : VREG,
                                      ctx->f_LocalName, ctx->f_LocalNameLen,
                                      SFM_RESOURCEFORK_NAME,
                                      NULL, NULL,
                                      &ctx->f_attr.fa_rsrc_size, &ctx->f_attr.fa_rsrc_alloc,
                                      &stream_flags, &ctx->f_attr.fa_max_access,
                                      context);

        SMB_LOG_KTRACE(SMB_DBG_PACK_ATTR_BLK | DBG_FUNC_NONE,
                       0xabc005, error, 0, 0, 0);

        if ((!error) || (error == ENOATTR)) {
             /* smbfs_smb_qstreaminfo worked */
             
             if (stream_flags & SMB_NO_SUBSTREAMS) {
                 /* No named streams at all on item */
                 ctx->f_attr.fa_valid_mask |= FA_FSTATUS_VALID;
                 ctx->f_attr.fa_fstatus = kNO_SUBSTREAMS;
             }
             else {
                 /* At least one named stream on item */
                 ctx->f_attr.fa_valid_mask |= FA_FSTATUS_VALID;
                 ctx->f_attr.fa_fstatus = 0;
             }
             
             /* Did we need Resource Fork info? */
             if ((need_rsrc_fork) &&
                 !(ctx->f_attr.fa_valid_mask & FA_RSRC_FORK_VALID)) {
                 /* 
                  * Successfully got Resource Fork Info. Its either already in 
                  * f_attr or no Resource Fork was found 
                  */
                 ctx->f_attr.fa_valid_mask |= FA_RSRC_FORK_VALID;

                 if (stream_flags & SMB_NO_RESOURCE_FORK) {
                     /* No Resource Fork, so set resource fork lengths to zero */
                     ctx->f_attr.fa_rsrc_size = 0;
                     ctx->f_attr.fa_rsrc_alloc = 0;
                 }
                 else {
                     /* SMBDEBUG("%s rsrc fork from qstreaminfo\n", ctx->f_LocalName); */
                 }
             }
             
             /* Did we need Finder Info? */
             if ((ATTR_CMN_FNDRINFO & common_attr) &&
                 !(ctx->f_attr.fa_valid_mask & FA_FINDERINFO_VALID)) {
                 /* 
                  * Now we know if there is Finder Info or not on the item 
                  */
                 if (stream_flags & SMB_NO_FINDER_INFO) {
                     /* No Finder Info, so set Finder Info to all zeros */
                     ctx->f_attr.fa_valid_mask |= FA_FINDERINFO_VALID;
                     bcopy(&zero_finfo, ctx->f_attr.fa_finder_info,
                           sizeof(u_int8_t) * 32);
                 }
             }
             
             /* Did we need Max Access? */
             if ((ATTR_CMN_USERACCESS & common_attr) &&
                 !(ctx->f_attr.fa_valid_mask & FA_MAX_ACCESS_VALID)) {
                 if (SSTOVC(ctx->f_share)->vc_flags & SMBV_SMB2) {
                     /*
                      * Only SMB 2/3 can get max access from 
                      * smbfs_smb_qstreaminfo call 
                      */
                     ctx->f_attr.fa_valid_mask |= FA_MAX_ACCESS_VALID;
                 }
             }
         }
         else {
             /* Got some sort of error. This shouldn't happen */
             SMBDEBUG("smbfs_smb_qstreaminfo failed %d for %s \n",
                      error, ctx->f_LocalName);
         }
    }

    /* 
     * Do we still need to get the Finder Info?  At this point we know
     * (1) Either no vnode or the cached finder info is not present
     * (2) smbfs_smb_qstreaminfo told us that there is Finder Info on the item
     *
     * <11615553> For SMB 1, we can NOT get both the Finder Info and max
     * access at the same time. Windows based servers will often give 
     * "Unspecified error" when you do a CreateAndX with extended response (ie 
     * max access) combined with Read of a named stream.
     */
    if ((ATTR_CMN_FNDRINFO & common_attr) &&
        !(ctx->f_attr.fa_valid_mask & FA_FINDERINFO_VALID)) {
        
        do {
            afp_uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
            if (afp_uio == NULL) {
                SMBERROR("uio_create failed for %s \n", ctx->f_LocalName);
                break;
            }
            
            error = uio_addiov(afp_uio, CAST_USER_ADDR_T(afp_info),
                               sizeof(afp_info));
            if (error) {
                SMBERROR("uio_addiov failed for %s \n", ctx->f_LocalName);
                break;
            }
            
            uio_setoffset(afp_uio, 0);
            
            /* Open/Read/Close the Finder Info */
            error = smbfs_smb_cmpd_create_read_close(ctx->f_share, ctx->f_dnp,
                                                     ctx->f_LocalName, ctx->f_LocalNameLen,
                                                     SFM_FINDERINFO_NAME, strlen(SFM_FINDERINFO_NAME),
                                                     afp_uio, &afp_size,
                                                     NULL,
                                                     context);
            
            SMB_LOG_KTRACE(SMB_DBG_PACK_ATTR_BLK | DBG_FUNC_NONE,
                           0xabc006, error, 0, 0, 0);
            
            if (!error) {
                /* Successfully got Finder Info */
                ctx->f_attr.fa_valid_mask |= FA_FINDERINFO_VALID;
                
                /* Verify returned size */
                if (afp_size != AFP_INFO_SIZE) {
                    /* Could be a 0 size returned meaning no Finder Info */
                    if (afp_size != 0) {
                        /* SMBDEBUG("%s Finder Info size mismatch %ld != %d \n",
                                 ctx->f_LocalName, afp_size, AFP_INFO_SIZE); */
                    }
                    bcopy(&zero_finfo, ctx->f_attr.fa_finder_info,
                          sizeof(u_int8_t) * 32);
                }
                else {
                    /* Correct size, so just copy it in */
                    bcopy(&afp_info[AFP_INFO_FINDER_OFFSET],
                          ctx->f_attr.fa_finder_info,
                          sizeof(u_int8_t) * 32);
                    /* SMBDEBUG("Finder Info 0x%x 0x%x 0x%x 0x%x for %s \n",
                             afp_info[AFP_INFO_FINDER_OFFSET],
                             afp_info[AFP_INFO_FINDER_OFFSET + 1],
                             afp_info[AFP_INFO_FINDER_OFFSET + 2],
                             afp_info[AFP_INFO_FINDER_OFFSET + 3],
                             ctx->f_LocalName); */
                }
            }
            else {
                if (error != ENOENT) {
                    SMBDEBUG("smbfs_smb_cmpd_create_read_close failed %d for %s \n",
                             error, ctx->f_LocalName);
                }
            }
            
            if (afp_uio) {
                uio_free(afp_uio);
            }
        } while (0);
    }
    
    /* Do we still need Max Access and its SMB 1? */
    if ((ATTR_CMN_USERACCESS & common_attr) &&
        !(ctx->f_attr.fa_valid_mask & FA_MAX_ACCESS_VALID) &&
        !(SSTOVC(ctx->f_share)->vc_flags & SMBV_SMB2))
    {
        error = smb1fs_smb_open_maxaccess(ctx->f_share, ctx->f_dnp,
                                          ctx->f_LocalName, ctx->f_LocalNameLen,
                                          &fid, &ctx->f_attr.fa_max_access,
                                          context);
        if (!error) {
            ctx->f_attr.fa_valid_mask |= FA_MAX_ACCESS_VALID;
        }
        
        if (fid) {
            (void)smbfs_smb_close(ctx->f_share, fid, context);
        }
    }
    
update_caches:
    /* Update vnodes caches if the data did not come from the vnode caches */
    if (vp) {
        np = VTOSMB(vp);

        if (ctx->f_attr.fa_valid_mask & FA_FSTATUS_VALID) {
            /* Update whether there is a named streams or not */
            np->n_fstatus = ctx->f_attr.fa_fstatus;
        }

        /* Do we have updated resource fork info? */
        if ((rsrc_fork_from_cache == 0) &&
            (need_rsrc_fork) &&
            (ctx->f_attr.fa_valid_mask & FA_RSRC_FORK_VALID)) {
            
            lck_mtx_lock(&np->rfrkMetaLock);
            np->rfrk_size = ctx->f_attr.fa_rsrc_size;
            np->rfrk_alloc_size = ctx->f_attr.fa_rsrc_alloc;
            nanouptime(&ts);
            np->rfrk_cache_timer = ts.tv_sec;
            lck_mtx_unlock(&np->rfrkMetaLock);
        }

        /* Do we have updated Finder Info? */
       if ((finder_info_from_cache == 0) &&
            (ATTR_CMN_FNDRINFO & common_attr) &&
            (ctx->f_attr.fa_valid_mask & FA_FINDERINFO_VALID)) {
           
           bcopy(ctx->f_attr.fa_finder_info, &np->finfo,
                 sizeof(u_int8_t) * 32);
           nanouptime(&ts);
           np->finfo_cache_timer = ts.tv_sec;
        }

        /* Do we have updated Max Access? */
        if ((max_access_from_cache == 0) &&
            (ATTR_CMN_USERACCESS & common_attr) &&
            (ctx->f_attr.fa_valid_mask & FA_MAX_ACCESS_VALID)) {
            
            np->maxAccessRights = ctx->f_attr.fa_max_access;
            np->maxAccessRightChTime = ctx->f_attr.fa_chtime;
        }
        
        /* We can get the unix mode from the vnode */
        if (np->n_flag & NHAS_POSIXMODES) {
            ctx->f_attr.fa_permissions = np->n_mode;
            ctx->f_attr.fa_valid_mask |= FA_UNIX_MODES_VALID;
        }
        
        /* We can get the uid/gid from the vnode */
        ctx->f_attr.fa_uid = np->n_uid;
        ctx->f_attr.fa_gid = np->n_gid;
    }
    
    /*
     * Error Handling. At this point, we should have valid information and if
     * we do not, then some earlier error must have occurred, so fill in with
     * default values.
     */
    if ((need_rsrc_fork) && !(ctx->f_attr.fa_valid_mask & FA_RSRC_FORK_VALID)) {
        /* Assume resource fork lengths of zero */
        ctx->f_attr.fa_valid_mask |= FA_RSRC_FORK_VALID;
        ctx->f_attr.fa_rsrc_size = 0;
        ctx->f_attr.fa_rsrc_alloc = 0;
    }
    
    if ((ATTR_CMN_FNDRINFO & common_attr) &&
        !(ctx->f_attr.fa_valid_mask & FA_FINDERINFO_VALID)) {
        /* Assume zero Finder Info */
        ctx->f_attr.fa_valid_mask |= FA_FINDERINFO_VALID;
        bcopy(&zero_finfo, ctx->f_attr.fa_finder_info, sizeof(u_int8_t) * 32);
    }
    
    if ((ATTR_CMN_USERACCESS & common_attr) &&
        !(ctx->f_attr.fa_valid_mask & FA_MAX_ACCESS_VALID)) {
        /* Assume full access */
        ctx->f_attr.fa_valid_mask |= FA_MAX_ACCESS_VALID;
        ctx->f_attr.fa_max_access = SA_RIGHT_FILE_ALL_ACCESS | STD_RIGHT_ALL_ACCESS;
    }
    
    /*
     * At this point we should have all the meta data we need to fill out the
     * return attribute block. 
     */
	if (attrlistp->commonattr) {
		packcommonattr(abp, smp, ctx, context);
    }

	if (attrlistp->dirattr && is_dir) {
		packdirattr(abp, vp, ctx);
    }

	if (attrlistp->fileattr && !is_dir) {
		packfileattr(abp, smp, ctx, context);
    }
    
    SMB_LOG_KTRACE(SMB_DBG_PACK_ATTR_BLK | DBG_FUNC_END, 0, 0, 0, 0, 0);
}

static void
packcommonattr(struct attrblock *abp,
               struct smbmount *smp,
               struct smbfs_fctx *ctx,
               struct vfs_context *context)
{
	attrgroup_t attr = abp->ab_attrlist->commonattr;
	struct mount *mp = smp->sm_mp;
	void *attrbufptr = *abp->ab_attrbufpp;
	void *varbufptr = *abp->ab_varbufpp;
	boolean_t is_64_bit = proc_is64bit(vfs_context_proc(context));
	uid_t cuid = 1;
	int isroot = 0;
	struct timespec temp_time;
    
    /* 
     * If there was a vnode np, then ctx->f_attr.fa_uid/fa_gid was set to
     * np->n_uid/n_gid which could have been updated by a Get ACL
     */
    uid_t uid = (uid_t) ctx->f_attr.fa_uid;
    gid_t gid = (gid_t) ctx->f_attr.fa_gid;
    mode_t mode = 0;
    uint32_t flags = 0;
    uint32_t cmn_user_rights = 0;
    uint32_t ino;
    
    /* Calculate uid, gid and mode from Query Dir results */
    if (ctx->f_attr.fa_attr & SMB_EFA_DIRECTORY) {
        flags |= SMBFS_GET_UGM_IS_DIR;
    }
    
    if (!(ctx->f_attr.fa_valid_mask & FA_UNIX_MODES_VALID)) {
        /* Must not have had a vnode to get unix mode from */
        flags |= SMBFS_GET_UGM_REMOVE_POSIX_MODES;
    }

    /* Get the uid, gid and mode */
    smb_get_uid_gid_mode(ctx->f_share, smp,
                         &ctx->f_attr, flags,
                         &uid, &gid, &mode);

    /* This is used later in this function... */
    if (attr & (ATTR_CMN_OWNERID | ATTR_CMN_GRPID)) {
		cuid = kauth_cred_getuid(vfs_context_ucred(context));
		isroot = cuid == 0;
	}
	
	if (ATTR_CMN_NAME & attr) {
        packnameattr(abp, ctx, (const u_int8_t *) ctx->f_LocalName,
                     ctx->f_LocalNameLen);
        
		attrbufptr = *abp->ab_attrbufpp;
		varbufptr = *abp->ab_varbufpp;
	}
    
	if (ATTR_CMN_DEVID & attr) {
        /* Copy AFP Client behavior */
		*((dev_t *)attrbufptr) = vfs_statfs(mp)->f_fsid.val[0];
		attrbufptr = ((dev_t *)attrbufptr) + 1;
	}
    
	if (ATTR_CMN_FSID & attr) {
        /* Copy AFP Client behavior */
        *((fsid_t *)attrbufptr) = vfs_statfs(mp)->f_fsid;
		attrbufptr = ((fsid_t *)attrbufptr) + 1;
	}
    
	if (ATTR_CMN_OBJTYPE & attr) {
        /*
         * Because of the Steve/Conrad Symlinks we can never be completely
         * sure that we have the correct vnode type if its a file. Since we 
         * don't support Steve/Conrad Symlinks with Darwin we can always count 
         * on the vtype being correct. For directories we always know the 
         * correct information.
         */
        *((fsobj_type_t *)attrbufptr) = ctx->f_attr.fa_vtype;
        
		attrbufptr = ((fsobj_type_t *)attrbufptr) + 1;
	}
    
	if (ATTR_CMN_OBJTAG & attr) {
		*((fsobj_tag_t *)attrbufptr) = VT_CIFS;
		attrbufptr = ((fsobj_tag_t *)attrbufptr) + 1;
	}
    
	/*
	 * Exporting file IDs from HFS Plus:
	 *
	 * For "normal" files the c_fileid is the same value as the
	 * c_cnid.  But for hard link files, they are different - the
	 * c_cnid belongs to the active directory entry (ie the link)
	 * and the c_fileid is for the actual inode (ie the data file).
	 *
	 * The stat call (getattr) will always return the c_fileid
	 * and Carbon APIs, which are hardlink-ignorant, will always
	 * receive the c_cnid (from getattrlist).
	 */
	if (ATTR_CMN_OBJID & attr) {
        /*
         * VOL_CAP_FMT_64BIT_OBJECT_IDS is set so this value is undefined
         */
        ino = (uint32_t) smb2fs_smb_file_id_get(smp, ctx->f_attr.fa_ino,
                                                ctx->f_LocalName);

        ((fsobj_id_t *)attrbufptr)->fid_objno = ino;
		((fsobj_id_t *)attrbufptr)->fid_generation = 0;
		attrbufptr = ((fsobj_id_t *)attrbufptr) + 1;
	}
    
	if (ATTR_CMN_OBJPERMANENTID & attr) {
        /*
         * VOL_CAP_FMT_64BIT_OBJECT_IDS is set so this value is undefined
         */
        ino = (uint32_t) smb2fs_smb_file_id_get(smp, ctx->f_attr.fa_ino,
                                                ctx->f_LocalName);

        ((fsobj_id_t *)attrbufptr)->fid_objno = ino;
		((fsobj_id_t *)attrbufptr)->fid_generation = 0;
		attrbufptr = ((fsobj_id_t *)attrbufptr) + 1;
	}
    
	if (ATTR_CMN_PAROBJID & attr) {
        /*
         * VOL_CAP_FMT_64BIT_OBJECT_IDS is set so this value is undefined
         */
        lck_rw_lock_shared(&ctx->f_dnp->n_name_rwlock);
        ino = (uint32_t) smb2fs_smb_file_id_get(smp, ctx->f_dnp->n_ino,
                                                ctx->f_dnp->n_name);
        lck_rw_unlock_shared(&ctx->f_dnp->n_name_rwlock);

        ((fsobj_id_t *)attrbufptr)->fid_objno = ino;
		((fsobj_id_t *)attrbufptr)->fid_generation = 0;
		attrbufptr = ((fsobj_id_t *)attrbufptr) + 1;
	}
    
	if (ATTR_CMN_SCRIPT & attr) {
        /* %%% TO DO: What should this be??? */
		*((text_encoding_t *)attrbufptr) = 0;  /* kTextEncodingMacRoman */
		attrbufptr = ((text_encoding_t *)attrbufptr) + 1;
	}
    
	if (ATTR_CMN_CRTIME & attr) {
        temp_time = ctx->f_attr.fa_crtime;
        
	    if (is_64_bit) {
            ((struct user64_timespec *)attrbufptr)->tv_sec = temp_time.tv_sec;
            ((struct user64_timespec *)attrbufptr)->tv_nsec = temp_time.tv_nsec;
			attrbufptr = ((struct user64_timespec *)attrbufptr) + 1;
	    }
	    else {
            /* Seems like getattr just slams in uint64_t into uint32_t fields */
            ((struct user32_timespec *)attrbufptr)->tv_sec = (uint32_t) temp_time.tv_sec;
            ((struct user32_timespec *)attrbufptr)->tv_nsec = (uint32_t) temp_time.tv_nsec;
			attrbufptr = ((struct user32_timespec *)attrbufptr) + 1;
	    }
	}
    
	if (ATTR_CMN_MODTIME & attr) {
        temp_time = ctx->f_attr.fa_mtime;

	    if (is_64_bit) {
             ((struct user64_timespec *)attrbufptr)->tv_sec = temp_time.tv_sec;
             ((struct user64_timespec *)attrbufptr)->tv_nsec = temp_time.tv_nsec;
			 attrbufptr = ((struct user64_timespec *)attrbufptr) + 1;
	    }
	    else {
            /* Seems like getattr just slams in uint64_t into uint32_t fields */
            ((struct user32_timespec *)attrbufptr)->tv_sec = (uint32_t) temp_time.tv_sec;
            ((struct user32_timespec *)attrbufptr)->tv_nsec = (uint32_t) temp_time.tv_nsec;
			attrbufptr = ((struct user32_timespec *)attrbufptr) + 1;
	    }
	}
    
	if (ATTR_CMN_CHGTIME & attr) {
        temp_time = ctx->f_attr.fa_chtime;

	    if (is_64_bit) {
            ((struct user64_timespec *)attrbufptr)->tv_sec = temp_time.tv_sec;
            ((struct user64_timespec *)attrbufptr)->tv_nsec = temp_time.tv_nsec;
			attrbufptr = ((struct user64_timespec *)attrbufptr) + 1;
	    }
	    else {
            /* Seems like getattr just slams in uint64_t into uint32_t fields */
            ((struct user32_timespec *)attrbufptr)->tv_sec = (uint32_t) temp_time.tv_sec;
            ((struct user32_timespec *)attrbufptr)->tv_nsec = (uint32_t) temp_time.tv_nsec;
			attrbufptr = ((struct user32_timespec *)attrbufptr) + 1;
	    }
	}
    
	if (ATTR_CMN_ACCTIME & attr) {
        temp_time = ctx->f_attr.fa_atime;

	    if (is_64_bit) {
            ((struct user64_timespec *)attrbufptr)->tv_sec = temp_time.tv_sec;
            ((struct user64_timespec *)attrbufptr)->tv_nsec = temp_time.tv_nsec;
			attrbufptr = ((struct user64_timespec *)attrbufptr) + 1;
	    }
	    else {
            /* Seems like getattr just slams in uint64_t into uint32_t fields */
            ((struct user32_timespec *)attrbufptr)->tv_sec = (uint32_t) temp_time.tv_sec;
            ((struct user32_timespec *)attrbufptr)->tv_nsec = (uint32_t) temp_time.tv_nsec;
			attrbufptr = ((struct user32_timespec *)attrbufptr) + 1;
	    }
	}

	if (ATTR_CMN_BKUPTIME & attr) {
        /* Backup time not supported so return 0 */
	    if (is_64_bit) {
            ((struct user64_timespec *)attrbufptr)->tv_sec = 0;
            ((struct user64_timespec *)attrbufptr)->tv_nsec = 0;
			attrbufptr = ((struct user64_timespec *)attrbufptr) + 1;
	    }
	    else {
            ((struct user32_timespec *)attrbufptr)->tv_sec = 0;
            ((struct user32_timespec *)attrbufptr)->tv_nsec = 0;
			attrbufptr = ((struct user32_timespec *)attrbufptr) + 1;
	    }
	}
    
	if (ATTR_CMN_FNDRINFO & attr) {
        DBG_ASSERT(ctx->f_attr.fa_valid_mask & FA_FINDERINFO_VALID);
        bcopy(ctx->f_attr.fa_finder_info, attrbufptr, sizeof(u_int8_t) * 32);
		attrbufptr = (char *)attrbufptr + sizeof(u_int8_t) * 32;
	}

	if (ATTR_CMN_OWNERID & attr) {
        uid_t nuid;
        
        if (SMBV_HAS_GUEST_ACCESS(SSTOVC(ctx->f_share))) {
            nuid = UNKNOWNUID;
		} 
        else {
			/* 
			 * For servers that support the UNIX extensions we know the uid/gid. 
			 * For server that don't support ACLs then the node uid/gid will be 
			 * set to the mounted user's uid/gid. For all other servers we need 
			 * to get the ACL and translate the SID to a uid or gid. The uid/gid 
			 * really is for display purpose only and means nothing to us. We will 
			 * set the nodes ids if we get a request for the ACL, but otherwise
			 * we leave them unset for performance reasons.
			 */
			if (ctx->f_attr.fa_uid == KAUTH_UID_NONE) {
                nuid = smp->sm_args.uid;
            }
			else {
                nuid = uid;
            }
		}

		if (!isroot) {
			if (((unsigned int)vfs_flags(smp->sm_mp)) & MNT_UNKNOWNPERMISSIONS)
				nuid = cuid;
			else if (nuid == UNKNOWNUID)
				nuid = cuid;
		}

		*((uid_t *)attrbufptr) = nuid;
		attrbufptr = ((uid_t *)attrbufptr) + 1;
	}
    
	if (ATTR_CMN_GRPID & attr) {
		gid_t ngid;

        if (SMBV_HAS_GUEST_ACCESS(SSTOVC(ctx->f_share))) {
            ngid = UNKNOWNGID;
		} 
        else {
			/* 
			 * For servers that support the UNIX extensions we know the uid/gid. 
			 * For server that don't support ACLs then the node uid/gid will be 
			 * set to the mounted user's uid/gid. For all other servers we need 
			 * to get the ACL and translate the SID to a uid or gid. The uid/gid 
			 * really is for display purpose only and means nothing to us. We will 
			 * set the nodes ids if we get a request for the ACL, but otherwise
			 * we leave them unset for performance reasons.
			 */
			if (ctx->f_attr.fa_gid == KAUTH_GID_NONE) {
                ngid = smp->sm_args.gid;
            }
			else {
                ngid = gid;
            }
		}

		if (!isroot) {
			gid_t cgid = kauth_cred_getgid(vfs_context_ucred(context));
			if (((unsigned int)vfs_flags(smp->sm_mp)) & MNT_UNKNOWNPERMISSIONS)
				ngid = cgid;
			else if (ngid == UNKNOWNUID)
				ngid = cgid;
		}

		*((gid_t *)attrbufptr) = ngid;
		attrbufptr = ((gid_t *)attrbufptr) + 1;
	}
    
	if (ATTR_CMN_ACCESSMASK & attr) {
        if (ctx->f_attr.fa_vtype == VDIR) {
            *((u_int32_t *)attrbufptr) = (S_IFDIR | mode);
        }
        
        if (ctx->f_attr.fa_vtype == VREG) {
            *((u_int32_t *)attrbufptr) = (S_IFREG | mode);
        }
        
        if (ctx->f_attr.fa_vtype == VLNK) {
            *((u_int32_t *)attrbufptr) = (S_IFLNK | mode);
        }
        
		attrbufptr = ((u_int32_t *)attrbufptr) + 1;
	}
    
	if (ATTR_CMN_FLAGS & attr) {
        uint32_t va_flags = 0;
        
        if (ctx->f_attr.fa_attr & SMB_EFA_HIDDEN) {
            /* 
             * Dont have to special case whether root vnode is hidden or not.
             * root volume doesn't show up in a readdirattr, I think? 
             */
            va_flags |= UF_HIDDEN;
        }
        
        /*
         * Remember that SMB_EFA_ARCHIVE means the items needs to be
         * archived and SF_ARCHIVED means the item has been archive.
         *
         * NOTE: Windows does not set ATTR_ARCHIVE bit for directories.
         */
        if ((ctx->f_attr.fa_vtype != VDIR) &&
            !(ctx->f_attr.fa_attr & SMB_EFA_ARCHIVE)) {
            va_flags |= SF_ARCHIVED;
        }
        
		if (node_isimmutable(ctx->f_share, NULL, &ctx->f_attr)) {
            va_flags |= UF_IMMUTABLE;
        }
        
		*((u_int32_t *)attrbufptr) = va_flags;
		attrbufptr = ((u_int32_t *)attrbufptr) + 1;
	}
    
	if (ATTR_CMN_USERACCESS & attr) {
        /* 
         * The effective permissions for the current user which we derive
         * from the max access
         */
        DBG_ASSERT(ctx->f_attr.fa_valid_mask & FA_MAX_ACCESS_VALID);

        if (ctx->f_attr.fa_max_access & SMB2_FILE_READ_DATA) {
            cmn_user_rights |= R_OK;
        }
        
        if (ctx->f_attr.fa_max_access & SMB2_FILE_WRITE_DATA) {
            cmn_user_rights |= W_OK;
        }

        if (ctx->f_attr.fa_max_access & SMB2_FILE_EXECUTE) {
            cmn_user_rights |= X_OK;
        }
         
		*((u_int32_t *)attrbufptr) = cmn_user_rights;
		attrbufptr = ((u_int32_t *)attrbufptr) + 1;
	}
    
	if (ATTR_CMN_FILEID & attr) {
        *((u_int64_t *)attrbufptr) = smb2fs_smb_file_id_get(smp,
                                                            ctx->f_attr.fa_ino,
                                                            ctx->f_LocalName);
		attrbufptr = ((u_int64_t *)attrbufptr) + 1;
	}
    
	if (ATTR_CMN_PARENTID & attr) {
        lck_rw_lock_shared(&ctx->f_dnp->n_name_rwlock);
        *((u_int64_t *)attrbufptr) = smb2fs_smb_file_id_get(smp,
                                                            ctx->f_dnp->n_ino,
                                                            ctx->f_dnp->n_name);
        lck_rw_unlock_shared(&ctx->f_dnp->n_name_rwlock);
		attrbufptr = ((u_int64_t *)attrbufptr) + 1;
	}
	
	*abp->ab_attrbufpp = attrbufptr;
	*abp->ab_varbufpp = varbufptr;
}

static void
packdirattr(struct attrblock *abp,
            struct vnode *vp,
            struct smbfs_fctx *ctx)
{
	attrgroup_t attr = abp->ab_attrlist->dirattr;
	void *attrbufptr = *abp->ab_attrbufpp;
	u_int32_t entries;
    uint32_t mnt_status;

	/*
	 * The DIR_LINKCOUNT is the count of real directory hard links.
	 * (i.e. its not the sum of the implied "." and ".." references
	 *  typically used in stat's st_nlink field)
	 */
	if (ATTR_DIR_LINKCOUNT & attr) {
        /* There ARE no hard links, at least not yet... */
		*((u_int32_t *)attrbufptr) = 1;
		attrbufptr = ((u_int32_t *)attrbufptr) + 1;
	}
    
	if (ATTR_DIR_ENTRYCOUNT & attr) {
        /* Apparently 0 is a fine answer to return for a filesystem */
		entries = 0;

		*((u_int32_t *)attrbufptr) = entries;
		attrbufptr = ((u_int32_t *)attrbufptr) + 1;
	}
    
	if (ATTR_DIR_MOUNTSTATUS & attr) {
        mnt_status = 0;
        
        /* if its a DFS reparse point, then its a mnt trigger */
        if ((ctx->f_attr.fa_attr & SMB_EFA_REPARSE_POINT) &&
            (ctx->f_attr.fa_reparse_tag == IO_REPARSE_TAG_DFS)) {
            mnt_status |= DIR_MNTSTATUS_TRIGGER;
        }

        /* If we have a vnode, check to see if its already mounted on */
        if ((vp != NULL) && vnode_mountedhere(vp)) {
            mnt_status |= DIR_MNTSTATUS_MNTPOINT;
        }
        
        *((u_int32_t *)attrbufptr) = mnt_status;
		attrbufptr = ((u_int32_t *)attrbufptr) + 1;
	}
    
	*abp->ab_attrbufpp = attrbufptr;
}

static void
packfileattr(struct attrblock *abp,
             struct smbmount *smp,
             struct smbfs_fctx *ctx,
             struct vfs_context *context)
{
#pragma unused(context)
	attrgroup_t attr = abp->ab_attrlist->fileattr;
	void *attrbufptr = *abp->ab_attrbufpp;
	void *varbufptr = *abp->ab_varbufpp;
    uint64_t data_fork_size, data_fork_alloc;

    /* Use values from ctx */
    data_fork_size = ctx->f_attr.fa_size;
    data_fork_alloc = ctx->f_attr.fa_data_alloc;
    
    if (ATTR_FILE_LINKCOUNT & attr) {
        /* There ARE no hard links, at least not yet... */
		*((u_int32_t *)attrbufptr) = 1;
		attrbufptr = ((u_int32_t *)attrbufptr) + 1;
	}
    
	if (ATTR_FILE_TOTALSIZE & attr) {
        /* Return same value as smbfs_vnop_getattr */
        DBG_ASSERT(ctx->f_attr.fa_valid_mask & FA_RSRC_FORK_VALID);

		*((off_t *)attrbufptr) = data_fork_size + ctx->f_attr.fa_rsrc_size;
		attrbufptr = ((off_t *)attrbufptr) + 1;
	}
    
	if (ATTR_FILE_ALLOCSIZE & attr) {
        DBG_ASSERT(ctx->f_attr.fa_valid_mask & FA_RSRC_FORK_VALID);

        /* Should already be rounded up */
		*((off_t *)attrbufptr) = data_fork_alloc + ctx->f_attr.fa_rsrc_alloc;
		attrbufptr = ((off_t *)attrbufptr) + 1;
	}
    
	if (ATTR_FILE_IOBLOCKSIZE & attr) {
		*((u_int32_t *)attrbufptr) = smp->sm_statfsbuf.f_bsize;
		attrbufptr = ((u_int32_t *)attrbufptr) + 1;
	}
    
	if (ATTR_FILE_CLUMPSIZE & attr) {
        /* This attribute is obsolete so return 0 */
		*((u_int32_t *)attrbufptr) = 0;
		attrbufptr = ((u_int32_t *)attrbufptr) + 1;
	}
    
	if (ATTR_FILE_DEVTYPE & attr) {
        /* Copy AFP Client behavior */
        *((u_int32_t *)attrbufptr) = 0;
		attrbufptr = ((u_int32_t *)attrbufptr) + 1;
	}
    
	if (ATTR_FILE_DATALENGTH & attr) {
		*((off_t *)attrbufptr) = data_fork_size;
		attrbufptr = ((off_t *)attrbufptr) + 1;
	}
    
    if (ATTR_FILE_DATAALLOCSIZE & attr) {
        *((off_t *)attrbufptr) = data_fork_alloc;
        attrbufptr = ((off_t *)attrbufptr) + 1;
    }
    
    if (ATTR_FILE_RSRCLENGTH & attr) {
        DBG_ASSERT(ctx->f_attr.fa_valid_mask & FA_RSRC_FORK_VALID);

        *((off_t *)attrbufptr) = ctx->f_attr.fa_rsrc_size;
        attrbufptr = ((off_t *)attrbufptr) + 1;
    }
    
    if (ATTR_FILE_RSRCALLOCSIZE & attr) {
        DBG_ASSERT(ctx->f_attr.fa_valid_mask & FA_RSRC_FORK_VALID);

        *((off_t *)attrbufptr) = ctx->f_attr.fa_rsrc_alloc;
        attrbufptr = ((off_t *)attrbufptr) + 1;
    }

	*abp->ab_attrbufpp = attrbufptr;
	*abp->ab_varbufpp = varbufptr;
}

static void
packnameattr(struct attrblock *abp, struct smbfs_fctx *ctx,
             const u_int8_t *name, size_t namelen)
{
	void *varbufptr;
	struct attrreference * attr_refptr;
	char *mpname;
	size_t mpnamelen;
	u_int32_t attrlength;
	u_int8_t empty = 0;
	struct smbmount *smp;
	
	/* A cnode's name may be incorrect for the root of a mounted
	 * filesystem (it can be mounted on a different directory name
	 * than the name of the volume, such as "blah-1").  So for the
	 * root directory, it's best to return the last element of the
	 location where the volume's mounted:
	 */

	smp = ctx->f_share->ss_mount;
    if ((ctx->f_attr.fa_ino == smp->sm_root_ino) &&
	    (mpname = mountpointname(vnode_mount(ctx->f_dnp->n_vnode)))) {
		mpnamelen = strlen(mpname);
		
		/* Trim off any trailing slashes: */
		while ((mpnamelen > 0) && (mpname[mpnamelen-1] == '/'))
			--mpnamelen;
        
		/* If there's anything left, use it instead of the volume's name */
		if (mpnamelen > 0) {
			name = (u_int8_t *)mpname;
			namelen = mpnamelen;
		}
	}
	if (name == NULL) {
		name = &empty;
		namelen = 0;
	}
    
	varbufptr = *abp->ab_varbufpp;
	attr_refptr = (struct attrreference *)(*abp->ab_attrbufpp);
    
	attrlength = (uint32_t) (namelen + 1);
	attr_refptr->attr_dataoffset = (uint32_t) ((char *)varbufptr - (char *)attr_refptr);
	attr_refptr->attr_length = attrlength;
	(void) strncpy((char *)varbufptr, (const char *) name, attrlength);
	/*
	 * Advance beyond the space just allocated and
	 * round up to the next 4-byte boundary:
	 */
	varbufptr = ((char *)varbufptr) + attrlength + ((4 - (attrlength & 3)) & 3);
	++attr_refptr;
    
	*abp->ab_attrbufpp = attr_refptr;
	*abp->ab_varbufpp = varbufptr;
}

/*
 * readdirattr operation will return attributes for the items in the
 * directory specified. 
 *
 * It does not do . and .. entries. The problem is if you are at the root of the
 * smbfs directory and go to .. you could be crossing a mountpoint into a
 * different (ufs) file system. The attributes that apply for it may not 
 * apply for the file system you are doing the readdirattr on. To make life 
 * simpler, this call will only return entries in its directory, hfs like.
 */
int
smbfs_vnop_readdirattr(struct vnop_readdirattr_args *ap)
/* struct vnop_readdirattr_args {
                              struct vnode *a_vp;
                              struct attrlist *a_alist;
                              struct uio *a_uio;
                              u_long a_maxcount;
                              u_long a_options;
                              u_long *a_newstate;
                              int *a_eofflag;
                              u_long *a_actualcount;
                              vfs_context_t a_context;
                              } *ap; */
{
	struct vnode *vp = NULL;
	struct vnode *dvp = ap->a_vp;
	struct attrlist *alist = ap->a_alist;
	uio_t uio = ap->a_uio;
	int maxcount = ap->a_maxcount;
	u_int32_t fixedblocksize;
	u_int32_t maxattrblocksize;
	u_int32_t currattrbufsize;
	void *attrbufptr = NULL;
	void *attrptr;
	void *varptr;
	struct attrblock attrblk;
    vfs_context_t context = ap->a_context;
	struct smbnode *dnp = NULL;
	struct smbfs_fctx *ctx;
	off_t offset;
	int error = 0;
    struct smb_share *share = NULL;
    struct smbmount *smp = NULL;

	*(ap->a_actualcount) = 0;
	*(ap->a_eofflag) = 0;
    
	/* Check for invalid options and buffer space. */
	if (((ap->a_options & ~(FSOPT_NOINMEMUPDATE | FSOPT_NOFOLLOW)) != 0) ||
	    (uio_resid(uio) <= 0) || (uio_iovcnt(uio) > 1) || (maxcount <= 0)) {
        SMBDEBUG("Invalid options or buf size\n");
		return (EINVAL);
	}
    
	/*
	 * Reject requests for unsupported attributes.
	 */
	if ((alist->bitmapcount != ATTR_BIT_MAP_COUNT) ||
	    (alist->commonattr & ~SMBFS_ATTR_CMN_VALID) ||
	    (alist->volattr  != 0) ||
	    (alist->dirattr & ~SMBFS_ATTR_DIR_VALID) ||
	    (alist->fileattr & ~SMBFS_ATTR_FILE_VALID) ||
	    (alist->forkattr != 0)) {
        SMBDEBUG("Unsupported attributes\n");
		return (EINVAL);
	}
    
    /*
     * Lock parent dir that we are enumerating
     */
	if ((error = smbnode_lock(VTOSMB(dvp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);

    VTOSMB(dvp)->n_lastvop = smbfs_vnop_readdirattr;
    
	dnp = VTOSMB(dvp);
	smp = VTOSMBFS(dvp);

	SMB_LOG_KTRACE(SMB_DBG_READ_DIR_ATTR | DBG_FUNC_START, dnp->d_fid, 0, 0, 0, 0);

	/*
     * Do we need to start or restart the directory listing 
     *
     * The uio_offset is actually just used to store whatever we want.  In HFS, 
     * they store an index and a dir tag. For SMB, we will store just the offset 
     */
	offset = uio_offset(uio);
	
    /* Get Share reference */
	share = smb_get_share_with_reference(VTOSMBFS(dvp));

    /* Non FAT Filesystem and named streams are required */
    if ((share->ss_fstype == SMB_FS_FAT) ||
        !(share->ss_attributes & FILE_NAMED_STREAMS)) {
        smb_share_rele(share, context);
        SMBDEBUG("FAT or no named streams so smbfs_vnop_readdirattr not supported\n");
        error = ENOTSUP;
        goto done;
    }

    /*
     * Do we need to start or restart the directory listing
     *
     * The uio_offset is actually just used to store whatever we want.  In HFS,
     * they store an index and a dir tag. For SMB, we will store just the offset
     */
    if (!dnp->d_fctx || (dnp->d_fctx->f_share != share) || (offset == 0) ||
		(offset != dnp->d_offset)) {
		smbfs_closedirlookup(dnp, context);
		error = smbfs_smb_findopen(share, dnp, "*", 1, &dnp->d_fctx, TRUE, 
                                   context);
	}
    
	/* 
	 * The directory fctx keeps a reference on the share so we can release our 
	 * reference on the share now.
	 */ 
	smb_share_rele(share, context);
	
	if (error) {
		SMBERROR_LOCK(dnp, "Can't open search for %s, error = %d", dnp->n_name, error);
		goto done;
	}
	ctx = dnp->d_fctx;

	/* 
     * Get a buffer to hold packed attributes. 
     */
	fixedblocksize = (sizeof(u_int32_t) + attrblksize(alist)); /* 4 bytes for length */
	maxattrblocksize = fixedblocksize;
	if (alist->commonattr & ATTR_CMN_NAME) 
		maxattrblocksize += (share->ss_maxfilenamelen * 3) + 1;
    
	MALLOC(attrbufptr, void *, maxattrblocksize, M_TEMP, M_WAITOK);
	if (attrbufptr == NULL) {
		error = ENOMEM;
		goto done;
	}
	attrptr = attrbufptr;
	varptr = (char *)attrbufptr + fixedblocksize;  /* Point to variable-length storage */

    /* 
	 * They are continuing from some point ahead of us in the buffer. Skip all
	 * entries until we reach their point in the buffer.
	 */
	while (dnp->d_offset < offset) {
		error = smbfs_findnext(ctx, context);
		if (error) {
			smbfs_closedirlookup(dnp, context);
			goto done;
		}
		dnp->d_offset++;
	}

	/* Loop until we end the search or we don't have enough room for the max element */
    while (uio_resid(uio)) {
        /* Get one entry out of the buffer and fill in ctx with its info */
		error = smbfs_findnext(ctx, context);
		if (error) {
			break;
        }

        /*
         * <14430881> If file IDs are supported by this server, skip any
         * child that has the same id as the current parent that we are
         * enumerating. Seems like snapshot dirs have the same id as the parent 
         * and that will cause us to deadlock when we find the vnode with same 
         * id and then try to lock it again (deadlock on parent id).
         */
        if (SSTOVC(share)->vc_misc_flags & SMBV_HAS_FILEIDS) {
            if (ctx->f_attr.fa_ino == dnp->n_ino) {
                SMBDEBUG("Skipping <%s> as it has same ID as parent\n",
                         ctx->f_LocalName);
                continue;
            }
        }
        
        /* 
         * Check to see if vnode already exists. If so, then can pull the data
         * from the inode instead of having to poll the server for missing info
         *
         * Go ahead and create the vnode if its not already there. Hopefully
         * this will improve Finder browsing performance
         */
        error = smbfs_nget(share, vnode_mount(dvp),
                           dvp, ctx->f_LocalName, ctx->f_LocalNameLen,
                           &ctx->f_attr, &vp,
                           MAKEENTRY, SMBFS_NGET_CREATE_VNODE,
                           ap->a_context);
        
        SMB_LOG_KTRACE(SMB_DBG_READ_DIR_ATTR | DBG_FUNC_NONE,
                        0xabc001, error, 0, 0, 0);

        /* dont care if we got an error or not, just whether vp == NULL or not */
        if ((error == 0) && (vp != NULL)) {
            /*
             * Enumerates alway return the correct case of the name.
             * Update the name and parent if needed.
             */
            smbfs_update_name_par(ctx->f_share, dvp, vp,
                                  &ctx->f_attr.fa_reqtime,
                                  ctx->f_LocalName, ctx->f_LocalNameLen);
        }
        
		*((u_int32_t *)attrptr) = 0;
		attrptr = ((u_int32_t *)attrptr) + 1;
		attrblk.ab_attrlist = alist;
		attrblk.ab_attrbufpp = &attrptr;
		attrblk.ab_varbufpp = &varptr;
		attrblk.ab_flags = 0;
		attrblk.ab_blocksize = maxattrblocksize;
		attrblk.ab_context = ap->a_context;
        
		/* 
         * Pack catalog entries into attribute buffer. 
         */
		packattrblk(&attrblk, smp, vp, ctx, context);
		currattrbufsize = (uint32_t) ((char *)varptr - (char *)attrbufptr);
        
        /* Done with the vp */
        if (vp != NULL) {
            smbnode_unlock(VTOSMB(vp));
            vnode_put(vp);
        }
        
        /* 
         * Make sure there's enough buffer space remaining. 
         */
		// LP64todo - fix this!
		if (uio_resid(uio) < 0 || (currattrbufsize > (u_int32_t)uio_resid(uio))) {
			break;
		} 
        else {
			*((u_int32_t *)attrbufptr) = currattrbufsize;
			error = uiomove((caddr_t)attrbufptr, currattrbufsize, uio);
			if (error) {
				break;
			}
            
			attrptr = attrbufptr;
			/* Point to variable-length storage */
			varptr = (char *)attrbufptr + fixedblocksize; 
            
			*ap->a_actualcount += 1;
            dnp->d_offset++;
            
			/* Termination checks */
			if ((--maxcount <= 0) ||
			    // LP64todo - fix this!
			    uio_resid(uio) < 0 ||
			    ((u_int32_t)uio_resid(uio) < (fixedblocksize + SMBFS_AVERAGE_NAME_SIZE))) {
				break;
			}
		}
    } /* while loop */
    
 	if (error == ENOENT) {
		*(ap->a_eofflag) = TRUE;
		error = 0;
	}
    
	if (error) {
		goto done;
	}
    
done:
	/* Last offset into uio_offset. */
	uio_setoffset(uio, dnp->d_offset);
    
    *ap->a_newstate = dnp->d_changecnt;
    
	if (attrbufptr) {
		FREE(attrbufptr, M_TEMP);
    }
    
	smbnode_unlock(VTOSMB(dvp));
    
    SMB_LOG_KTRACE(SMB_DBG_READ_DIR_ATTR | DBG_FUNC_END,
                   error, *ap->a_actualcount, 0, 0, 0);
	return (error);
}

