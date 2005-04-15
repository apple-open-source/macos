/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * msdosfs_attrlist.c - DOS FAT attribute list processing
 *
 * Copyright (c) 2002, Apple Computer, Inc.  All Rights Reserved.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <miscfs/specfs/specdev.h>
#include <sys/malloc.h>
#include <sys/attr.h>
#include <sys/utfconv.h>

#include "bpb.h"
#include "direntry.h"
#include "denode.h"
#include "msdosfsmount.h"
#include "fat.h"

enum {
    MSDOSFS_ATTR_CMN_NATIVE		= ATTR_CMN_DEVID | ATTR_CMN_FSID | ATTR_CMN_OBJTYPE | ATTR_CMN_OBJTAG | ATTR_CMN_OBJID |
                                ATTR_CMN_CRTIME | ATTR_CMN_MODTIME | ATTR_CMN_ACCTIME | ATTR_CMN_FLAGS,
    MSDOSFS_ATTR_CMN_SUPPORTED	= MSDOSFS_ATTR_CMN_NATIVE | ATTR_CMN_OWNERID | ATTR_CMN_GRPID | ATTR_CMN_ACCESSMASK |
                                ATTR_CMN_USERACCESS,
    MSDOSFS_ATTR_VOL_NATIVE		= ATTR_VOL_FSTYPE | ATTR_VOL_SIZE | ATTR_VOL_SPACEFREE | ATTR_VOL_SPACEAVAIL |
                                ATTR_VOL_MINALLOCATION | ATTR_VOL_ALLOCATIONCLUMP | ATTR_VOL_IOBLOCKSIZE |
                                ATTR_VOL_MAXOBJCOUNT | ATTR_VOL_MOUNTPOINT | ATTR_VOL_MOUNTFLAGS |
                                ATTR_VOL_MOUNTEDDEVICE | ATTR_VOL_CAPABILITIES | ATTR_VOL_NAME |
#ifdef ATTR_VOL_VCBFSID
                                ATTR_VOL_VCBFSID |
#endif
                                ATTR_VOL_SIGNATURE,
    MSDOSFS_ATTR_VOL_SUPPORTED	= MSDOSFS_ATTR_VOL_NATIVE,
    MSDOSFS_ATTR_DIR_NATIVE		= ATTR_DIR_MOUNTSTATUS,
    MSDOSFS_ATTR_DIR_SUPPORTED	= MSDOSFS_ATTR_DIR_NATIVE | ATTR_DIR_LINKCOUNT,
    MSDOSFS_ATTR_FILE_NATIVE	= ATTR_FILE_TOTALSIZE | ATTR_FILE_ALLOCSIZE | ATTR_FILE_IOBLOCKSIZE | ATTR_FILE_CLUMPSIZE |
                                ATTR_FILE_DATALENGTH | ATTR_FILE_DATAALLOCSIZE,
    MSDOSFS_ATTR_FILE_SUPPORTED	= MSDOSFS_ATTR_FILE_NATIVE | ATTR_FILE_LINKCOUNT,
    MSDOSFS_ATTR_FORK_NATIVE	= 0,
    MSDOSFS_ATTR_FORK_SUPPORTED	= MSDOSFS_ATTR_FORK_NATIVE,
    
    MSDOSFS_ATTR_CMN_SETTABLE	= 0,
    MSDOSFS_ATTR_VOL_SETTABLE	= ATTR_VOL_INFO | ATTR_VOL_NAME,
    MSDOSFS_ATTR_DIR_SETTABLE	= 0,
    MSDOSFS_ATTR_FILE_SETTABLE	= 0,
    MSDOSFS_ATTR_FORK_SETTABLE	= 0
};

/*
 * Pack a C-style string into an attribute buffer.  Returns the new varptr.
 */
static void *packstr(char *s, void *attrptr, void *varptr)
{
    struct attrreference *ref = attrptr;
    u_long length;
    
    length = strlen(s) + 1;		/* String, plus terminator */
    
    /* In fixed-length part of buffer, store offset and length of variable-length data */
    ref->attr_dataoffset = (u_int8_t *) varptr - (u_int8_t *) attrptr;
    ref->attr_length = length;
    
    /* Copy the string to variable-length part of buffer */
    (void) strncpy((unsigned char *)varptr, s, length);
    
    /* Advance pointer past string, and round up to multiple of 4 bytes */
    varptr += (length + 3) & ~3;
        
    return varptr;
}

/*
 * msdosfs_packvolattr
 *
 * Pack the volume-related attributes from a getattrlist call into result buffers.
 * Fields are packed in order based on the bitmap masks.  Attributes with smaller
 * masks are packed first.
 *
 * The buffer pointers are updated to point past the data that was returned.
 */
static void msdosfs_packvolattr(
    struct attrlist *alist,		/* Desired attributes */
    struct denode *dep,			/* DOS-specific vnode information */
    void **attrptrptr,			/* Buffer for fixed-size attributes */
    void **varptrptr,			/* Buffer for variable-length attributes */
    struct ucred *cred)			/* Credentials (used for ATTR_CMN_USERACCESS) */
{
    attrgroup_t a;
    void *attrptr = *attrptrptr;
    void *varptr = *varptrptr;
    struct msdosfsmount *pmp = dep->de_pmp;
    struct mount *mp = pmp->pm_mountp;
    
    a = alist->commonattr;
    if (a)
    {
#if 1
        /* We don't currently support volume names, but we could later */
        if (a & ATTR_CMN_NAME) {
            varptr = packstr(pmp->pm_label, attrptr, varptr);
            ++((struct attrreference *)attrptr);
        }
#endif
        if (a & ATTR_CMN_DEVID) *((dev_t *) attrptr)++ = pmp->pm_devvp->v_rdev;
        if (a & ATTR_CMN_FSID) *((fsid_t *)attrptr)++ = DETOV(dep)->v_mount->mnt_stat.f_fsid;
        if (a & ATTR_CMN_OBJTYPE) *((fsobj_type_t *)attrptr)++ = DETOV(dep)->v_type;
        if (a & ATTR_CMN_OBJTAG) *((fsobj_tag_t *)attrptr)++ = VT_MSDOSFS;
        if (a & ATTR_CMN_OBJID) {
			((fsobj_id_t *)attrptr)->fid_objno = defileid(dep);
			((fsobj_id_t *)attrptr)->fid_generation = 0;
			++((fsobj_id_t *)attrptr);
        }
        /* ATTR_CMN_OBJPERMANENTID and ATTR_CMN_PAROBJID not supported */
        if (a & ATTR_CMN_CRTIME) {
            dos2unixtime(dep->de_CDate, dep->de_CTime, dep->de_CHun, attrptr);
            ++((struct timespec *)attrptr);
        }
        if (a & ATTR_CMN_MODTIME) {
            dos2unixtime(dep->de_MDate, dep->de_MTime, 0, attrptr);
            ++((struct timespec *)attrptr);
        }
        /* ATTR_CMN_CHGTIME not supported */
        if (a & ATTR_CMN_ACCTIME) {
            dos2unixtime(dep->de_ADate, 0, 0, attrptr);
            ++((struct timespec *)attrptr);
        }
        /* ATTR_CMN_BKUPTIME and ATTR_CMN_FNDRINFO not supported */
        if (a & ATTR_CMN_OWNERID) *((uid_t *)attrptr)++ = get_pmuid(pmp, cred->cr_uid);
        if (a & ATTR_CMN_GRPID) *((gid_t *)attrptr)++ = pmp->pm_gid;
        if (a & ATTR_CMN_ACCESSMASK) *((u_long *)attrptr)++ = ALLPERMS & pmp->pm_mask;
        if (a & ATTR_CMN_FLAGS)
        {
            u_long flags;									/* chflags-style flags */
            
            flags = 0;										/* Assume no special flags set */
            if ((dep->de_Attributes & ATTR_ARCHIVE) == 0)	/* DOS: flag set means "needs to be archived" */
                flags |= SF_ARCHIVED;						/* BSD: flag set means "has been archived" */
            if (dep->de_Attributes & ATTR_READONLY)			/* DOS read-only becomes user immutable */
                flags |= UF_IMMUTABLE;
    
            *((u_long *)attrptr)++ = flags;
        }
        if (a & ATTR_CMN_USERACCESS)
        {
            if (cred->cr_uid == 0)
                *((u_long *)attrptr)++ = R_OK | W_OK | X_OK;		/* Root always has full access */
            else
                *((u_long *)attrptr)++ = pmp->pm_mask & S_IRWXO;	/* Everybody else has the same mask */
        }
    }
    
    a = alist->volattr;
    if (a)
    {
        if (a & ATTR_VOL_FSTYPE) *((u_long *)attrptr)++ = (u_long) mp->mnt_vfc->vfc_typenum;
        if (a & ATTR_VOL_SIGNATURE) *((u_long *)attrptr)++ = 0x4244;
        if (a & ATTR_VOL_SIZE) *((off_t *)attrptr)++ = (off_t) (pmp->pm_maxcluster-2) * (off_t) pmp->pm_bpcluster;
        if (a & ATTR_VOL_SPACEFREE) *((off_t *)attrptr)++ = (off_t) pmp->pm_freeclustercount * (off_t) pmp->pm_bpcluster;
        if (a & ATTR_VOL_SPACEAVAIL) *((off_t *)attrptr)++ = (off_t) pmp->pm_freeclustercount * (off_t) pmp->pm_bpcluster;
        if (a & ATTR_VOL_MINALLOCATION) *((off_t *)attrptr)++ = pmp->pm_bpcluster;
        if (a & ATTR_VOL_ALLOCATIONCLUMP) *((off_t *)attrptr)++ = pmp->pm_bpcluster;
        if (a & ATTR_VOL_IOBLOCKSIZE) *((u_long *)attrptr)++ =  pmp->pm_bpcluster;
        /*
         * ATTR_VOL_OBJCOUNT, ATTR_VOL_FILECOUNT, ATTR_VOL_DIRCOUNT are not supported.
         * If they were really needed, we'd have to iterate over the entire contents of
         * the volume.
         */
        if (a & ATTR_VOL_MAXOBJCOUNT) *((u_long *)attrptr)++ = 0xFFFFFFFF;
        if (a & ATTR_VOL_MOUNTPOINT) {
            varptr = packstr(mp->mnt_stat.f_mntonname, attrptr, varptr);
            ++((struct attrreference *)attrptr);
        }
        if (a & ATTR_VOL_NAME) {
            varptr = packstr(pmp->pm_label, attrptr, varptr);
            ++((struct attrreference *)attrptr);
        }
        if (a & ATTR_VOL_MOUNTFLAGS) *((u_long *)attrptr)++ = mp->mnt_flag;
        if (a & ATTR_VOL_MOUNTEDDEVICE) {
            varptr = packstr(mp->mnt_stat.f_mntfromname, attrptr, varptr);
            ++((struct attrreference *)attrptr);
        }
        /* ATTR_VOL_ENCODINGSUSED not supported */
        if (a & ATTR_VOL_CAPABILITIES)
        {
            vol_capabilities_attr_t *vcapattrptr;
            
            vcapattrptr = (vol_capabilities_attr_t *) attrptr;
            
            vcapattrptr->capabilities[VOL_CAPABILITIES_FORMAT] = 
            	VOL_CAP_FMT_NO_ROOT_TIMES |
            	VOL_CAP_FMT_CASE_PRESERVING |
            	VOL_CAP_FMT_FAST_STATFS ;
            vcapattrptr->capabilities[VOL_CAPABILITIES_INTERFACES] = 
            	VOL_CAP_INT_NFSEXPORT |
            	VOL_CAP_INT_VOL_RENAME |
            	VOL_CAP_INT_ADVLOCK |
            	VOL_CAP_INT_FLOCK ;
            vcapattrptr->capabilities[VOL_CAPABILITIES_RESERVED1] = 0;
            vcapattrptr->capabilities[VOL_CAPABILITIES_RESERVED2] = 0;
    
            vcapattrptr->valid[VOL_CAPABILITIES_FORMAT] =
                VOL_CAP_FMT_PERSISTENTOBJECTIDS |
                VOL_CAP_FMT_SYMBOLICLINKS |
                VOL_CAP_FMT_HARDLINKS |
                VOL_CAP_FMT_JOURNAL |
                VOL_CAP_FMT_JOURNAL_ACTIVE |
                VOL_CAP_FMT_NO_ROOT_TIMES |
                VOL_CAP_FMT_SPARSE_FILES |
                VOL_CAP_FMT_ZERO_RUNS |
                VOL_CAP_FMT_CASE_SENSITIVE |
                VOL_CAP_FMT_CASE_PRESERVING |
                VOL_CAP_FMT_FAST_STATFS ;
            vcapattrptr->valid[VOL_CAPABILITIES_INTERFACES] =
                VOL_CAP_INT_SEARCHFS |
                VOL_CAP_INT_ATTRLIST |
                VOL_CAP_INT_NFSEXPORT |
                VOL_CAP_INT_READDIRATTR |
                VOL_CAP_INT_EXCHANGEDATA |
                VOL_CAP_INT_COPYFILE |
                VOL_CAP_INT_ALLOCATE |
                VOL_CAP_INT_VOL_RENAME |
            	VOL_CAP_INT_ADVLOCK |
            	VOL_CAP_INT_FLOCK ;
            vcapattrptr->valid[VOL_CAPABILITIES_RESERVED1] = 0;
            vcapattrptr->valid[VOL_CAPABILITIES_RESERVED2] = 0;
    
            ++((vol_capabilities_attr_t *)attrptr);
        }
#ifdef ATTR_VOL_VCBFSID
        if (a & ATTR_VOL_VCBFSID) *((u_long *)attrptr)++ = 0x4953;
#endif
        if (a & ATTR_VOL_ATTRIBUTES) {
        	((vol_attributes_attr_t *)attrptr)->validattr.commonattr = MSDOSFS_ATTR_CMN_SUPPORTED;
        	((vol_attributes_attr_t *)attrptr)->validattr.volattr = MSDOSFS_ATTR_VOL_SUPPORTED;
        	((vol_attributes_attr_t *)attrptr)->validattr.dirattr = MSDOSFS_ATTR_DIR_SUPPORTED;
        	((vol_attributes_attr_t *)attrptr)->validattr.fileattr = MSDOSFS_ATTR_FILE_SUPPORTED;
        	((vol_attributes_attr_t *)attrptr)->validattr.forkattr = MSDOSFS_ATTR_FORK_SUPPORTED;

        	((vol_attributes_attr_t *)attrptr)->nativeattr.commonattr = MSDOSFS_ATTR_CMN_NATIVE;
        	((vol_attributes_attr_t *)attrptr)->nativeattr.volattr = MSDOSFS_ATTR_VOL_NATIVE;
        	((vol_attributes_attr_t *)attrptr)->nativeattr.dirattr = MSDOSFS_ATTR_DIR_NATIVE;
        	((vol_attributes_attr_t *)attrptr)->nativeattr.fileattr = MSDOSFS_ATTR_FILE_NATIVE;
        	((vol_attributes_attr_t *)attrptr)->nativeattr.forkattr = MSDOSFS_ATTR_FORK_NATIVE;

            ++((vol_attributes_attr_t *)attrptr);
        }
    }
    
    /* Update the buffer pointers to point past what we just returned */
    *attrptrptr = attrptr;
    *varptrptr = varptr;
}

static void msdosfs_packcommonattr(
    struct attrlist *alist,		/* Desired attributes */
    struct denode *dep,			/* DOS-specific vnode information */
    void **attrptrptr,			/* Buffer for fixed-size attributes */
    void **varptrptr,			/* Buffer for variable-length attributes */
    struct ucred *cred)			/* Credentials (used for ATTR_CMN_USERACCESS) */
{
    attrgroup_t a;
    void *attrptr = *attrptrptr;
    void *varptr = *varptrptr;
    struct msdosfsmount *pmp = dep->de_pmp;
    
    a = alist->commonattr;

#if 1
    /* We don't currently support file and directory names, but we could later */
    if (a & ATTR_CMN_NAME) {
        varptr = packstr(dep->de_Name, attrptr, varptr);	/*¥¥ Should really be long name */
        ++((struct attrreference *)attrptr);
    }
#endif
    if (a & ATTR_CMN_DEVID) *((dev_t *)attrptr)++ = pmp->pm_devvp->v_rdev;
    if (a & ATTR_CMN_FSID) *((fsid_t *)attrptr)++ = DETOV(dep)->v_mount->mnt_stat.f_fsid;
    if (a & ATTR_CMN_OBJTYPE) *((fsobj_type_t *)attrptr)++ = DETOV(dep)->v_type;
    if (a & ATTR_CMN_OBJTAG) *((fsobj_tag_t *)attrptr)++ = VT_MSDOSFS;
    if (a & ATTR_CMN_OBJID) {
        ((fsobj_id_t *)attrptr)->fid_objno = defileid(dep);
        ((fsobj_id_t *)attrptr)->fid_generation = 0;
        ++((fsobj_id_t *)attrptr);
    }
    /* ATTR_CMN_OBJPERMANENTID and ATTR_CMN_PAROBJID not supported */
    if (a & ATTR_CMN_CRTIME) {
        dos2unixtime(dep->de_CDate, dep->de_CTime, dep->de_CHun, attrptr);
        ++((struct timespec *)attrptr);
    }
    if (a & ATTR_CMN_MODTIME) {
        dos2unixtime(dep->de_MDate, dep->de_MTime, 0, attrptr);
        ++((struct timespec *)attrptr);
    }
    /* ATTR_CMN_CHGTIME not supported */
    if (a & ATTR_CMN_ACCTIME) {
        dos2unixtime(dep->de_ADate, 0, 0, attrptr);
        ++((struct timespec *)attrptr);
    }
    /* ATTR_CMN_BKUPTIME and ATTR_CMN_FNDRINFO not supported */
    if (a & ATTR_CMN_OWNERID) *((uid_t *)attrptr)++ = get_pmuid(pmp, cred->cr_uid);
    if (a & ATTR_CMN_GRPID) *((gid_t *)attrptr)++ = pmp->pm_gid;
    if (a & ATTR_CMN_ACCESSMASK) *((u_long *)attrptr)++ = ALLPERMS & pmp->pm_mask;
    if (a & ATTR_CMN_FLAGS)
    {
        u_long flags;									/* chflags-style flags */
        
        flags = 0;										/* Assume no special flags set */
        if ((dep->de_Attributes & ATTR_ARCHIVE) == 0)	/* DOS: flag set means "needs to be archived" */
            flags |= SF_ARCHIVED;						/* BSD: flag set means "has been archived" */
        if (dep->de_Attributes & ATTR_READONLY)			/* DOS read-only becomes user immutable */
        	flags |= UF_IMMUTABLE;

        *((u_long *)attrptr)++ = flags;
    }
    if (a & ATTR_CMN_USERACCESS)
    {
        if (cred->cr_uid == 0)
            *((u_long *)attrptr)++ = R_OK | W_OK | X_OK;		/* Root always has full access */
        else
            *((u_long *)attrptr)++ = pmp->pm_mask & S_IRWXO;	/* Everybody else has the same mask */
    }
    
    /* Update the buffer pointers to point past what we just returned */
    *attrptrptr = attrptr;
    *varptrptr = varptr;
}

static void msdosfs_packdirattr(struct attrlist *alist, struct denode *dep, void **attrptrptr)
{
    attrgroup_t a;
    void *attrptr = *attrptrptr;
    
    a = alist->dirattr;
    
    if (a & ATTR_DIR_LINKCOUNT) *((u_long *)attrptr)++ = 1;	/* A safe value to indicate we don't do hard links. */
    /* ATTR_DIR_ENTRYCOUNT not supported because it would require iterating over the directory contents. */
    if (a & ATTR_DIR_MOUNTSTATUS) {
        if (DETOV(dep)->v_mountedhere) {
            *((u_long *)attrptr)++ = DIR_MNTSTATUS_MNTPOINT;
        } else {
            *((u_long *)attrptr)++ = 0;
        }
    }
    
    *attrptrptr = attrptr;
}

static void msdosfs_packfileattr(struct attrlist *alist, struct denode *dep, void **attrptrptr)
{
    attrgroup_t a;
    void *attrptr = *attrptrptr;
    struct msdosfsmount *pmp = dep->de_pmp;
    off_t allocSize = 0;
    
    a = alist->fileattr;
    
    if (a & (ATTR_FILE_ALLOCSIZE | ATTR_FILE_DATAALLOCSIZE))
        allocSize = roundup((off_t) dep->de_FileSize, (off_t) pmp->pm_bpcluster);

    if (a & ATTR_FILE_LINKCOUNT) *((u_long *)attrptr)++ = 1; /* A safe value to indicate we don't do hard links. */
    if (a & ATTR_FILE_TOTALSIZE) *((off_t *)attrptr)++ = dep->de_FileSize;
    if (a & ATTR_FILE_ALLOCSIZE) *((off_t *)attrptr)++ = allocSize;
    if (a & ATTR_FILE_IOBLOCKSIZE) *((u_long *)attrptr)++ = pmp->pm_bpcluster;
    if (a & ATTR_FILE_CLUMPSIZE) *((u_long *)attrptr)++ = pmp->pm_bpcluster;
    /* ATTR_FILE_DEVTYPE, ATTR_FILE_FILETYPE, ATTR_FILE_FORKCOUNT, ATTR_FILE_FORKLIST not supported. */
    if (a & ATTR_FILE_DATALENGTH) *((off_t *)attrptr)++ = dep->de_FileSize;
    if (a & ATTR_FILE_DATAALLOCSIZE) *((off_t *)attrptr)++ = allocSize;
    /*
     * ATTR_FILE_DATAEXTENTS, ATTR_FILE_RSRCLENGTH, ATTR_FILE_RSRCALLOCSIZE, ATTR_FILE_RSRCEXTENTS
     * not supported.
     */

    *attrptrptr = attrptr;
}

static void msdosfs_packattr(struct attrlist *alist, struct denode *dep, void **attrptr, void **varptr, struct ucred *cred)
{
    if (alist->volattr != 0)
    {
        msdosfs_packvolattr(alist, dep, attrptr, varptr, cred);
    }
    else
    {
        msdosfs_packcommonattr(alist, dep, attrptr, varptr, cred);
        
        switch (DETOV(dep)->v_type)
        {
            case VDIR:
                msdosfs_packdirattr(alist, dep, attrptr);
                break;
            case VREG:
                msdosfs_packfileattr(alist, dep, attrptr);
                break;
            default:
                break;
        }
    }
}

static size_t msdosfs_attrsize(struct attrlist *attrlist)
{
	size_t size;
	attrgroup_t a=0;
	
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
		if (a & ATTR_CMN_CRTIME) size += sizeof(struct timespec);
		if (a & ATTR_CMN_MODTIME) size += sizeof(struct timespec);
		if (a & ATTR_CMN_CHGTIME) size += sizeof(struct timespec);
		if (a & ATTR_CMN_ACCTIME) size += sizeof(struct timespec);
		if (a & ATTR_CMN_BKUPTIME) size += sizeof(struct timespec);
		if (a & ATTR_CMN_FNDRINFO) size += 32 * sizeof(u_int8_t);
		if (a & ATTR_CMN_OWNERID) size += sizeof(uid_t);
		if (a & ATTR_CMN_GRPID) size += sizeof(gid_t);
		if (a & ATTR_CMN_ACCESSMASK) size += sizeof(u_long);
		if (a & ATTR_CMN_NAMEDATTRCOUNT) size += sizeof(u_long);
		if (a & ATTR_CMN_NAMEDATTRLIST) size += sizeof(struct attrreference);
		if (a & ATTR_CMN_FLAGS) size += sizeof(u_long);
		if (a & ATTR_CMN_USERACCESS) size += sizeof(u_long);
	};
	if ((a = attrlist->volattr) != 0) {
		if (a & ATTR_VOL_FSTYPE) size += sizeof(u_long);
		if (a & ATTR_VOL_SIGNATURE) size += sizeof(u_long);
		if (a & ATTR_VOL_SIZE) size += sizeof(off_t);
		if (a & ATTR_VOL_SPACEFREE) size += sizeof(off_t);
		if (a & ATTR_VOL_SPACEAVAIL) size += sizeof(off_t);
		if (a & ATTR_VOL_MINALLOCATION) size += sizeof(off_t);
		if (a & ATTR_VOL_ALLOCATIONCLUMP) size += sizeof(off_t);
		if (a & ATTR_VOL_IOBLOCKSIZE) size += sizeof(u_long);
		if (a & ATTR_VOL_OBJCOUNT) size += sizeof(u_long);
		if (a & ATTR_VOL_FILECOUNT) size += sizeof(u_long);
		if (a & ATTR_VOL_DIRCOUNT) size += sizeof(u_long);
		if (a & ATTR_VOL_MAXOBJCOUNT) size += sizeof(u_long);
		if (a & ATTR_VOL_MOUNTPOINT) size += sizeof(struct attrreference);
		if (a & ATTR_VOL_NAME) size += sizeof(struct attrreference);
		if (a & ATTR_VOL_MOUNTFLAGS) size += sizeof(u_long);
		if (a & ATTR_VOL_MOUNTEDDEVICE) size += sizeof(struct attrreference);
		if (a & ATTR_VOL_ENCODINGSUSED) size += sizeof(unsigned long long);
		if (a & ATTR_VOL_CAPABILITIES) size += sizeof(vol_capabilities_attr_t);
#ifdef ATTR_VOL_VCBFSID
		if (a & ATTR_VOL_VCBFSID) size += sizeof(u_long);
#endif
		if (a & ATTR_VOL_ATTRIBUTES) size += sizeof(vol_attributes_attr_t);
	};
	if ((a = attrlist->dirattr) != 0) {
		if (a & ATTR_DIR_LINKCOUNT) size += sizeof(u_long);
		if (a & ATTR_DIR_ENTRYCOUNT) size += sizeof(u_long);
		if (a & ATTR_DIR_MOUNTSTATUS) size += sizeof(u_long);
	};
	if ((a = attrlist->fileattr) != 0) {
		if (a & ATTR_FILE_LINKCOUNT) size += sizeof(u_long);
		if (a & ATTR_FILE_TOTALSIZE) size += sizeof(off_t);
		if (a & ATTR_FILE_ALLOCSIZE) size += sizeof(off_t);
		if (a & ATTR_FILE_IOBLOCKSIZE) size += sizeof(size_t);
		if (a & ATTR_FILE_CLUMPSIZE) size += sizeof(off_t);
		if (a & ATTR_FILE_DEVTYPE) size += sizeof(u_long);
		if (a & ATTR_FILE_FILETYPE) size += sizeof(u_long);
		if (a & ATTR_FILE_FORKCOUNT) size += sizeof(u_long);
		if (a & ATTR_FILE_FORKLIST) size += sizeof(struct attrreference);
		if (a & ATTR_FILE_DATALENGTH) size += sizeof(off_t);
		if (a & ATTR_FILE_DATAALLOCSIZE) size += sizeof(off_t);
		if (a & ATTR_FILE_DATAEXTENTS) size += sizeof(extentrecord);
		if (a & ATTR_FILE_RSRCLENGTH) size += sizeof(off_t);
		if (a & ATTR_FILE_RSRCALLOCSIZE) size += sizeof(off_t);
		if (a & ATTR_FILE_RSRCEXTENTS) size += sizeof(extentrecord);
	};
	if ((a = attrlist->forkattr) != 0) {
		if (a & ATTR_FORK_TOTALSIZE) size += sizeof(off_t);
		if (a & ATTR_FORK_ALLOCSIZE) size += sizeof(off_t);
	};

	return size;
}

/*
#
#% getattrlist	vp	= = =
#
 vop_getattrlist {
     IN struct vnode *vp;
     IN struct attrlist *alist;
     INOUT struct uio *uio;
     IN struct ucred *cred;
     IN struct proc *p;
 };

 */
__private_extern__
int msdosfs_getattrlist(struct vop_getattrlist_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct denode *dep = VTODE(vp);
	struct attrlist *alist = ap->a_alist;
	size_t fixedblocksize;
	size_t attrblocksize;
	size_t attrbufsize;
	void *attrbufptr;
	void *attrptr;
    void *varptr;
    int error;
    
    /*
     * Make sure caller isn't asking for an attibute we don't support.
     */
    if ((alist->bitmapcount != 5) ||
    	(alist->commonattr & ~MSDOSFS_ATTR_CMN_SUPPORTED) != 0 ||
        (alist->volattr & ~(MSDOSFS_ATTR_VOL_SUPPORTED | ATTR_VOL_INFO)) != 0 ||
        (alist->dirattr & ~MSDOSFS_ATTR_DIR_SUPPORTED) != 0 ||
        (alist->fileattr & ~MSDOSFS_ATTR_FILE_SUPPORTED) != 0 ||
        (alist->forkattr & ~MSDOSFS_ATTR_FORK_SUPPORTED) != 0)
    {
        return EOPNOTSUPP;
    }
    
	/*
	 * Requesting volume information requires setting the
	 * ATTR_VOL_INFO bit. Also, volume info requests are
	 * mutually exclusive with all other info requests.
	 */
	if ((alist->volattr != 0) &&
	    (((alist->volattr & ATTR_VOL_INFO) == 0) ||
	     (alist->dirattr != 0) || (alist->fileattr != 0) || alist->forkattr != 0))
    {
		return EINVAL;
	}

    /*
     * Requesting volume information requires a vnode for the volume root.
     */
    if (alist->volattr && (vp->v_flag & VROOT) == 0)
    {
        return EINVAL;
    }

	fixedblocksize = msdosfs_attrsize(alist);
	attrblocksize = fixedblocksize + (sizeof(u_long));  /* u_long for length field */
	if (alist->commonattr & ATTR_CMN_NAME)
		attrblocksize += 256;
	if (alist->volattr & ATTR_VOL_MOUNTPOINT)
		attrblocksize += PATH_MAX;
    if (alist->volattr & ATTR_VOL_MOUNTEDDEVICE)
		attrblocksize += PATH_MAX;
	if (alist->volattr & ATTR_VOL_NAME)
		attrblocksize += 256;
	attrbufsize = MIN(ap->a_uio->uio_resid, attrblocksize);
	MALLOC(attrbufptr, void *, attrblocksize, M_TEMP, M_WAITOK);
	attrptr = attrbufptr;
	*((u_long *)attrptr) = 0;  /* Set buffer length in case of errors */
	++((u_long *)attrptr);     /* skip over length field */
	varptr = ((char *)attrptr) + fixedblocksize;

    msdosfs_packattr(alist, dep, &attrptr, &varptr, ap->a_cred);

    /* Don't return more data than was generated */
    attrbufsize = MIN(attrbufsize, (size_t) varptr - (size_t) attrbufptr);
    
    /* Return the actual buffer length */
    *((u_long *) attrbufptr) = attrbufsize;
    
    error = uiomove((caddr_t) attrbufptr, attrbufsize, ap->a_uio);
    
    FREE(attrbufptr, M_TEMP);
    return error;
}


static int msdosfs_unpackvolattr(
    attrgroup_t attrs,
    struct denode *dep,
    void *attrbufptr)
{
    int i;
    int error = 0;
    attrreference_t *attrref;
	struct msdosfsmount *pmp = dep->de_pmp;
    u_int16_t volName[11];
    size_t unichars;
    u_char label[11];
    struct buf *bp = NULL;

    if (attrs & ATTR_VOL_NAME)
    {
        u_int16_t c;
        extern u_char l2u[256];
        u_char unicode2dos(u_int16_t uc);
        
        attrref = attrbufptr;

        if (attrref->attr_length > 63)
        	return EINVAL;
        error = utf8_decodestr((char*)attrref + attrref->attr_dataoffset, attrref->attr_length,
            volName, &unichars, sizeof(volName), 0, UTF_PRECOMPOSED);
        if (error)
            return error;
        unichars /= 2;	/* Bytes to characters */
		if (unichars > 11)
			return EINVAL;

        /*
         * Convert from Unicode to local encoding (like a short name).
         * We can't call unicode2dosfn here because it assumes a dot
         * between the first 8 and last 3 characters.
         *
         * The specification doesn't say what syntax limitations exist
         * for volume labels.  By experimentation, they appear to be
         * upper case only.  I am assuming they are like short names,
         * but no period is assumed/required after the 8th character.
         */
        
        /* Name is trailing space padded, so init to all spaces. */
        for (i=0; i<11; ++i)
            label[i] = ' ';

        for (i=0; i<unichars; ++i) {
            c = volName[i];
            if (c < 0x100)
                c = l2u[c];			/* Convert to upper case */
            if (c != ' ')			/* Allow space to pass unchanged */
                c = unicode2dos(c);	/* Convert to local encoding */
            if (c < 3)
                return EINVAL;		/* Illegal char in name */
            label[i] = c;
        }

        /* Copy the UTF-8 to pmp->pm_label */
        bcopy((char*)attrref + attrref->attr_dataoffset, pmp->pm_label, attrref->attr_length);
        pmp->pm_label[attrref->attr_length] = '\0';

        /* Update label in boot sector */
        error = meta_bread(pmp->pm_devvp, 0, pmp->pm_BlockSize, NOCRED, &bp);
        if (!error) {
            if (FAT32(pmp))
                bcopy(label, (char*)bp->b_data+71, 11);
            else
                bcopy(label, (char*)bp->b_data+43, 11);
            bwrite(bp);
            bp = NULL;
        }
        if (bp)
            brelse(bp);
        bp = NULL;

        /*
         * Update label in root directory, if any.  For now, don't
         * create one if it doesn't exist (in case devices like
         * cameras don't understand them).
         */
        if (pmp->pm_label_cluster != CLUST_EOFE) {
        	error = readep(pmp, pmp->pm_label_cluster, pmp->pm_label_offset, &bp, NULL);
            if (!error) {
                bcopy(label, bp->b_data + pmp->pm_label_offset, 11);
                bwrite(bp);
                bp = NULL;
            }
            if (bp)
                brelse(bp);
            bp=NULL;
        }
        
        /* Advance buffer pointer past attribute reference */
        attrbufptr = ++attrref;
    }
    
    return error;
}



static int msdosfs_unpackcommonattr(
    attrgroup_t attrs,
    struct denode *dep,
    void *attrbufptr)
{
    return EOPNOTSUPP;
}



static int msdosfs_unpackattr(
    struct attrlist *alist,
    struct denode *dep,
    void *attrbufptr)
{
    int error;
    
    if (alist->volattr != 0)
        error = msdosfs_unpackvolattr(alist->volattr, dep, attrbufptr);
    else
        error = msdosfs_unpackcommonattr(alist->commonattr, dep, attrbufptr);
    
    return error;
}



/*
#
#% setattrlist	vp	L L L
#
vop_setattrlist {
	IN struct vnode *vp;
	IN struct attrlist *alist;
	INOUT struct uio *uio;
	IN struct ucred *cred;
	IN struct proc *p;
};
*/
__private_extern__
int msdosfs_setattrlist(struct vop_setattrlist_args *ap)
{
    struct vnode *vp = ap->a_vp;
    struct denode *dep = VTODE(vp);
	struct attrlist *alist = ap->a_alist;
	size_t attrblocksize;
	void *attrbufptr;
    int error;
    
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return (EROFS);

    /* Check the attrlist for valid inputs (i.e. be sure we understand what caller is asking) */
	if ((alist->bitmapcount != ATTR_BIT_MAP_COUNT) ||
	    ((alist->commonattr & ~ATTR_CMN_SETMASK) != 0) ||
	    ((alist->volattr & ~ATTR_VOL_SETMASK) != 0) ||
	    ((alist->dirattr & ~ATTR_DIR_SETMASK) != 0) ||
	    ((alist->fileattr & ~ATTR_FILE_SETMASK) != 0) ||
        ((alist->forkattr & ~ATTR_FORK_SETMASK) != 0))
    {
		return EINVAL;
	}

	/*
	 * Setting volume information requires setting the
	 * ATTR_VOL_INFO bit. Also, volume info requests are
	 * mutually exclusive with all other info requests.
	 */
	if ((alist->volattr != 0) &&
	    (((alist->volattr & ATTR_VOL_INFO) == 0) ||
	     (alist->dirattr != 0) || (alist->fileattr != 0) || alist->forkattr != 0))
    {
		return EINVAL;
	}

    /*
     * Make sure caller isn't asking for an attibute we don't support.
     *¥¥ Right now, all we support is setting the volume name.
     */
    if ((alist->commonattr & ~MSDOSFS_ATTR_CMN_SETTABLE) != 0 ||
        (alist->volattr & ~MSDOSFS_ATTR_VOL_SETTABLE) != 0 ||
        (alist->dirattr & ~MSDOSFS_ATTR_DIR_SETTABLE) != 0 ||
        (alist->fileattr & ~MSDOSFS_ATTR_FILE_SETTABLE) != 0 ||
        (alist->forkattr & ~MSDOSFS_ATTR_FORK_SETTABLE) != 0)
    {
        return EOPNOTSUPP;
    }
    
    /*
     * Setting volume information requires a vnode for the volume root.
     */
    if (alist->volattr && (vp->v_flag & VROOT) == 0)
    {
        return EINVAL;
    }

    /*
     *¥¥ We should check that the user has access to change the
     *¥¥ passed attributes.
     */

    attrblocksize = ap->a_uio->uio_resid;
    if (attrblocksize < msdosfs_attrsize(alist))
        return EINVAL;
    /*¥¥ We should check that attrreferences don't point outside the buffer */
    
	MALLOC(attrbufptr, void *, attrblocksize, M_TEMP, M_WAITOK);

	error = uiomove((caddr_t)attrbufptr, attrblocksize, ap->a_uio);
	if (error)
        goto ErrorExit;

    error = msdosfs_unpackattr(alist, dep, attrbufptr);

ErrorExit:
    FREE(attrbufptr, M_TEMP);
    return error;
}
