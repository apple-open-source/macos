/*
 * Copyright (c) 2002, 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

/*
 *  webdav_attrlist.c - WebDAV FS attribute list processing
 *
 *  Copyright (c) 2002, 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/attr.h>
#include <sys/kernel.h>

/*****************************************************************************/

__private_extern__ int 
webdav_getattrlist(struct vop_getattrlist_args *ap);

/*****************************************************************************/

enum
{
	WEBDAV_ATTR_CMN_NATIVE = 0,
	WEBDAV_ATTR_CMN_SUPPORTED = 0,
	WEBDAV_ATTR_VOL_NATIVE = ATTR_VOL_NAME |
		ATTR_VOL_CAPABILITIES |
		ATTR_VOL_ATTRIBUTES,
	WEBDAV_ATTR_VOL_SUPPORTED = WEBDAV_ATTR_VOL_NATIVE,
	WEBDAV_ATTR_DIR_NATIVE = 0,
	WEBDAV_ATTR_DIR_SUPPORTED = 0,
	WEBDAV_ATTR_FILE_NATIVE = 0,
	WEBDAV_ATTR_FILE_SUPPORTED = 0,
	WEBDAV_ATTR_FORK_NATIVE = 0,
	WEBDAV_ATTR_FORK_SUPPORTED = 0,
	
	WEBDAV_ATTR_CMN_SETTABLE	= 0,
	WEBDAV_ATTR_VOL_SETTABLE	= 0,
	WEBDAV_ATTR_DIR_SETTABLE	= 0,
	WEBDAV_ATTR_FILE_SETTABLE	= 0,
	WEBDAV_ATTR_FORK_SETTABLE	= 0
};

/*****************************************************************************/

/*
 * Copied from LibC string/rindex.c
 */
char *strrchr(p, ch)
        register const char *p;
        register int ch;
{
	register char *save;

	for (save = NULL;; ++p) {
		if (*p == ch)
			save = (char *)p;
		if (!*p)
			return(save);
	}
	/* NOTREACHED */
}

/*****************************************************************************/

/*
 * Pack a C-style string into an attribute buffer.  Returns the new varptr.
 */
static void *
packstr(char *s, void *attrptr, void *varptr)
{
	struct attrreference *ref = attrptr;
	u_long	length;

	length = strlen(s) + 1;	/* String, plus terminator */

	/*
	 * In the fixed-length part of buffer, store the offset and length of
	 * the variable-length data.
	 */
	ref->attr_dataoffset = (u_int8_t *)varptr - (u_int8_t *)attrptr;
	ref->attr_length = length;

	/* Copy the string to variable-length part of buffer */
	(void) strncpy((unsigned char *)varptr, s, length);

	/* Advance pointer past string, and round up to multiple of 4 bytes */        
	return (char *)varptr + ((length + 3) & ~3);
}

/*****************************************************************************/

/*
 * webdav_packvolattr
 *
 * Pack the volume-related attributes from a getattrlist call into result
 * buffers.  Fields are packed in order based on the bitmap masks.
 * Attributes with smaller masks are packed first.
 *
 * The buffer pointers are updated to point past the data that was returned.
 */
static int webdav_packvolattr(
    struct vnode *vp,		/* The volume's vnode */
    struct ucred *cred,
    struct attrlist *alist,	/* Desired attributes */
    void **attrptrptr,		/* Fixed-size attributes buffer */
    void **varptrptr)		/* Variable-size attributes buffer */
{
	#pragma unused(cred)
	attrgroup_t a;
	void *attrptr = *attrptrptr;
	void *varptr = *varptrptr;

	a = alist->volattr;
	if (a)
	{
		if (a & ATTR_VOL_NAME)
		{
			/* mount_webdav uses realpath() on f_mntonname so f_mntonname
			 * is NULL terminated and does not end with a slash.
			 */
			varptr = packstr(strrchr(vp->v_mount->mnt_stat.f_mntonname, '/') + 1,
				attrptr, varptr);
			++((struct attrreference *)attrptr);
		}

		if (a & ATTR_VOL_CAPABILITIES)
		{
			vol_capabilities_attr_t *vcapattrptr;

			vcapattrptr = (vol_capabilities_attr_t *) attrptr;

			/*
			 * Capabilities this volume format has.  Note that
			 * we do not set VOL_CAP_FMT_PERSISTENTOBJECTIDS.
			 * That's because we can't resolve an inode number
			 * into a directory entry (parent and name), which
			 * Carbon would need to support PBResolveFileIDRef.
			 */
			vcapattrptr->capabilities[VOL_CAPABILITIES_FORMAT] =
				VOL_CAP_FMT_FAST_STATFS; /* statfs is cached by webdavfs, so upper layers shouldn't cache */
			vcapattrptr->capabilities[VOL_CAPABILITIES_INTERFACES] =
				0; /* None of the optional interfaces are implemented. */
			vcapattrptr->capabilities[VOL_CAPABILITIES_RESERVED1] = 0;
			vcapattrptr->capabilities[VOL_CAPABILITIES_RESERVED2] = 0;

			/* Capabilities we know about: */
			vcapattrptr->valid[VOL_CAPABILITIES_FORMAT] =
				VOL_CAP_FMT_PERSISTENTOBJECTIDS |
				VOL_CAP_FMT_SYMBOLICLINKS |
				VOL_CAP_FMT_HARDLINKS |
				VOL_CAP_FMT_JOURNAL |
				VOL_CAP_FMT_JOURNAL_ACTIVE |
				VOL_CAP_FMT_NO_ROOT_TIMES |
				VOL_CAP_FMT_SPARSE_FILES |
				VOL_CAP_FMT_ZERO_RUNS |
				/* While WebDAV FS is case sensitive and case preserving,
				* not all WebDAV servers are case sensitive and case preserving.
				* That's because the volume used for storage on a WebDAV server
				* may not be case sensitive or case preserving. So, rather than
				* providing a wrong yes or no answer for VOL_CAP_FMT_CASE_SENSITIVE
				* and VOL_CAP_FMT_CASE_PRESERVING, we'll deny knowledge of those
				* volume attributes.
				*/
#if 0
				VOL_CAP_FMT_CASE_SENSITIVE |
				VOL_CAP_FMT_CASE_PRESERVING |
#endif
				VOL_CAP_FMT_FAST_STATFS;
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
				VOL_CAP_INT_FLOCK;
			vcapattrptr->valid[VOL_CAPABILITIES_RESERVED1] = 0;
			vcapattrptr->valid[VOL_CAPABILITIES_RESERVED2] = 0;

			++((vol_capabilities_attr_t *)attrptr);
		}

		if (a & ATTR_VOL_ATTRIBUTES)
		{
			vol_attributes_attr_t *volattrptr;

			volattrptr = (vol_attributes_attr_t *)attrptr;

			volattrptr->validattr.commonattr = WEBDAV_ATTR_CMN_SUPPORTED;
			volattrptr->validattr.volattr = WEBDAV_ATTR_VOL_SUPPORTED;
			volattrptr->validattr.dirattr = WEBDAV_ATTR_DIR_SUPPORTED;
			volattrptr->validattr.fileattr = WEBDAV_ATTR_FILE_SUPPORTED;
			volattrptr->validattr.forkattr = WEBDAV_ATTR_FORK_SUPPORTED;

			volattrptr->nativeattr.commonattr = WEBDAV_ATTR_CMN_NATIVE;
			volattrptr->nativeattr.volattr = WEBDAV_ATTR_VOL_NATIVE;
			volattrptr->nativeattr.dirattr = WEBDAV_ATTR_DIR_NATIVE;
			volattrptr->nativeattr.fileattr = WEBDAV_ATTR_FILE_NATIVE;
			volattrptr->nativeattr.forkattr = WEBDAV_ATTR_FORK_NATIVE;

			++((vol_attributes_attr_t *)attrptr);
		}
	}

	/* Update the buffer pointers to point past what we just returned */
	*attrptrptr = attrptr;
	*varptrptr = varptr;

	return 0;
}

/*****************************************************************************/

/*
 * Pack all attributes from a getattrlist or readdirattr call into
 * the result buffer.  For now, we only support volume attributes.
 */
static void
webdav_packattr(struct vnode *vp, struct ucred *cred, struct attrlist *alist,
	void **attrptr, void **varptr)
{
	if (alist->volattr != 0)
	{
		webdav_packvolattr(vp, cred, alist, attrptr, varptr);
	}
}

/*****************************************************************************/

/*
 * Calculate the fixed-size space required to hold a set of attributes.
 * For variable-length attributes, this will be the size of the
 * attribute reference (an offset and length).
 */
static size_t
webdav_attrsize(struct attrlist *attrlist)
{
	size_t size;
	attrgroup_t a = 0;

#if ((ATTR_CMN_NAME | ATTR_CMN_DEVID | ATTR_CMN_FSID | ATTR_CMN_OBJTYPE	|  \
      ATTR_CMN_OBJTAG | ATTR_CMN_OBJID | ATTR_CMN_OBJPERMANENTID |         \
      ATTR_CMN_PAROBJID | ATTR_CMN_SCRIPT | ATTR_CMN_CRTIME |              \
      ATTR_CMN_MODTIME | ATTR_CMN_CHGTIME | ATTR_CMN_ACCTIME |             \
      ATTR_CMN_BKUPTIME | ATTR_CMN_FNDRINFO | ATTR_CMN_OWNERID |           \
      ATTR_CMN_GRPID | ATTR_CMN_ACCESSMASK | ATTR_CMN_NAMEDATTRCOUNT |     \
      ATTR_CMN_NAMEDATTRLIST | ATTR_CMN_FLAGS | ATTR_CMN_USERACCESS)       \
      != ATTR_CMN_VALIDMASK)
#error	webdav_attrsize: Missing bits in common mask computation!
#endif

#if ((ATTR_VOL_FSTYPE | ATTR_VOL_SIGNATURE | ATTR_VOL_SIZE |                \
      ATTR_VOL_SPACEFREE | ATTR_VOL_SPACEAVAIL | ATTR_VOL_MINALLOCATION |   \
      ATTR_VOL_ALLOCATIONCLUMP | ATTR_VOL_IOBLOCKSIZE |                     \
      ATTR_VOL_OBJCOUNT | ATTR_VOL_FILECOUNT | ATTR_VOL_DIRCOUNT |          \
      ATTR_VOL_MAXOBJCOUNT | ATTR_VOL_MOUNTPOINT | ATTR_VOL_NAME |          \
      ATTR_VOL_MOUNTFLAGS | ATTR_VOL_INFO | ATTR_VOL_MOUNTEDDEVICE |        \
      ATTR_VOL_ENCODINGSUSED | ATTR_VOL_CAPABILITIES | ATTR_VOL_ATTRIBUTES) \
      != ATTR_VOL_VALIDMASK)
#error	webdav_attrsize: Missing bits in volume mask computation!
#endif

#if ((ATTR_DIR_LINKCOUNT | ATTR_DIR_ENTRYCOUNT | ATTR_DIR_MOUNTSTATUS)  \
      != ATTR_DIR_VALIDMASK)
#error	webdav_attrsize: Missing bits in directory mask computation!
#endif

#if ((ATTR_FILE_LINKCOUNT | ATTR_FILE_TOTALSIZE | ATTR_FILE_ALLOCSIZE |	\
      ATTR_FILE_IOBLOCKSIZE | ATTR_FILE_CLUMPSIZE | ATTR_FILE_DEVTYPE |	\
      ATTR_FILE_FILETYPE | ATTR_FILE_FORKCOUNT | ATTR_FILE_FORKLIST |	\
      ATTR_FILE_DATALENGTH | ATTR_FILE_DATAALLOCSIZE |			\
      ATTR_FILE_DATAEXTENTS | ATTR_FILE_RSRCLENGTH |			\
      ATTR_FILE_RSRCALLOCSIZE | ATTR_FILE_RSRCEXTENTS)			\
      != ATTR_FILE_VALIDMASK)
#error	webdav_attrsize: Missing bits in file mask computation!
#endif

#if ((ATTR_FORK_TOTALSIZE | ATTR_FORK_ALLOCSIZE) != ATTR_FORK_VALIDMASK)
#error	webdav_attrsize: Missing bits in fork mask computation!
#endif

	size = 0;

	if ((a = attrlist->volattr) != 0)
	{
		if (a & ATTR_VOL_NAME)
		{
			size += sizeof(struct attrreference);
		}
		if (a & ATTR_VOL_CAPABILITIES)
		{
			size += sizeof(vol_capabilities_attr_t);
		}
		if (a & ATTR_VOL_ATTRIBUTES)
		{
			size += sizeof(vol_attributes_attr_t);
		}
	};

	/*
	 * Ignore common, dir, file, and fork attributes since we
	 * don't support those yet.
	 */

	return size;
}

/*****************************************************************************/

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
__private_extern__ int 
webdav_getattrlist(struct vop_getattrlist_args *ap)
{
	struct vnode	*vp = ap->a_vp;
	struct attrlist	*alist = ap->a_alist;
	size_t		 fixedblocksize;
	size_t		 attrblocksize;
	size_t		 attrbufsize;
	void		*attrbufptr;
	void		*attrptr;
	void		*varptr;
	int		 error;

	/*
	 * Check the attrlist for valid inputs (i.e. be sure we understand what
	 * caller is asking).
	 *
	 * NOTE: we don't use ATTR_BIT_MAP_COUNT, because that could
	 * change in the header without this code changing.
	 */
	if ((alist->bitmapcount != 5) ||
		((alist->commonattr & ~ATTR_CMN_VALIDMASK) != 0) ||
		((alist->volattr & ~ATTR_VOL_VALIDMASK) != 0) ||
		((alist->dirattr & ~ATTR_DIR_VALIDMASK) != 0) ||
		((alist->fileattr & ~ATTR_FILE_VALIDMASK) != 0) ||
		((alist->forkattr & ~ATTR_FORK_VALIDMASK) != 0))
	{
		return EINVAL;
	}

	/*
	* Requesting volume information requires setting the
	* ATTR_VOL_INFO bit. Also, volume info requests are
	* mutually exclusive with all other info requests.
	*/
	if ((alist->volattr != 0) &&
	    (((alist->volattr & ATTR_VOL_INFO) == 0) ||
	     (alist->dirattr != 0) || (alist->fileattr != 0) ||
	     alist->forkattr != 0))
	{
		return EINVAL;
	}

	/*
	* Make sure caller isn't asking for an attibute we don't support.
	*/
	if ((alist->commonattr & ~WEBDAV_ATTR_CMN_SUPPORTED) != 0 ||
	    (alist->volattr & ~(WEBDAV_ATTR_VOL_SUPPORTED | ATTR_VOL_INFO)) != 0 ||
	    (alist->dirattr & ~WEBDAV_ATTR_DIR_SUPPORTED) != 0 ||
	    (alist->fileattr & ~WEBDAV_ATTR_FILE_SUPPORTED) != 0 ||
	    (alist->forkattr & ~WEBDAV_ATTR_FORK_SUPPORTED) != 0)
	{
		return EOPNOTSUPP;
	}

	/*
	 * Requesting volume information requires a vnode for the volume root.
	 */
	if (alist->volattr && (vp->v_flag & VROOT) == 0)
	{
		return EINVAL;
	}

	fixedblocksize = webdav_attrsize(alist);
	attrblocksize = fixedblocksize + (sizeof(u_long));
	if (alist->volattr & ATTR_VOL_NAME)
	{
		attrblocksize += ((MNAMELEN >> 2) << 2) + 4; /* MNAMELEN (padded) */
	}
	attrbufsize = MIN((size_t)ap->a_uio->uio_resid, attrblocksize);
	MALLOC(attrbufptr, void *, attrblocksize, M_TEMP, M_WAITOK);
	attrptr = attrbufptr;
	*((u_long *)attrptr) = 0;  /* Set buffer length in case of errors */
	++((u_long *)attrptr);     /* skip over length field */
	varptr = ((char *)attrptr) + fixedblocksize;

	webdav_packattr(vp, ap->a_cred, alist, &attrptr, &varptr);

	/* Don't return more data than was generated */
	attrbufsize = MIN(attrbufsize, (size_t) varptr - (size_t) attrbufptr);

	/* Return the actual buffer length */
	*((u_long *) attrbufptr) = attrbufsize;

	error = uiomove((caddr_t) attrbufptr, attrbufsize, ap->a_uio);

	FREE(attrbufptr, M_TEMP);
	return error;
}
