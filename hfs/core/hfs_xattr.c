/*
 * Copyright (c) 2004-2020 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 * 
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/ubc.h>
#include <sys/utfconv.h>
#include <sys/vnode.h>
#include <sys/xattr.h>
#include <sys/fcntl.h>
#include <sys/fsctl.h>
#include <sys/kauth.h>

#include "hfs.h"
#include "hfs_cnode.h"
#include "hfs_mount.h"
#include "hfs_format.h"
#include "hfs_endian.h"
#include "hfs_btreeio.h"
#include "hfs_fsctl.h"
#include "hfs_cprotect.h"

#include "BTreesInternal.h"

#define HFS_XATTR_VERBOSE  0

#define  ATTRIBUTE_FILE_NODE_SIZE   8192


/* State information for the listattr_callback callback function. */
struct listattr_callback_state {
	u_int32_t   fileID;
	int         result;
	uio_t       uio;
	size_t      size;
#if HFS_COMPRESSION
	int         showcompressed;
	vfs_context_t ctx;
	vnode_t     vp;
#endif /* HFS_COMPRESSION */
};


/* HFS Internal Names */
#define XATTR_XATTREXTENTS_NAME	      "system.xattrextents"

static u_int32_t emptyfinfo[8] = {0};

static int hfs_zero_hidden_fields (struct cnode *cp, u_int8_t *finderinfo); 

const char hfs_attrdatafilename[] = "Attribute Data";

static int  listattr_callback(const HFSPlusAttrKey *key, const HFSPlusAttrData *data,
                       struct listattr_callback_state *state);

static int  remove_attribute_records(struct hfsmount *hfsmp, BTreeIterator * iterator);

static int  getnodecount(struct hfsmount *hfsmp, size_t nodesize);

static size_t  getmaxinlineattrsize(struct vnode * attrvp);

static int  read_attr_data(struct hfsmount *hfsmp, uio_t uio, size_t datasize, HFSPlusExtentDescriptor *extents);

static int  write_attr_data(struct hfsmount *hfsmp, uio_t uio, size_t datasize, HFSPlusExtentDescriptor *extents);

static int  alloc_attr_blks(struct hfsmount *hfsmp, size_t attrsize, size_t extentbufsize, HFSPlusExtentDescriptor *extents, int *blocks);

static void  free_attr_blks(struct hfsmount *hfsmp, int blkcnt, HFSPlusExtentDescriptor *extents);

static int  has_overflow_extents(HFSPlusForkData *forkdata);

static int  count_extent_blocks(int maxblks, HFSPlusExtentRecord extents);

#if NEW_XATTR

/*
 * Iterator over set bits within an xattr free fext bitmap.
 *
 * See xbm_make_iter() for details on creation.
 */
typedef struct {
	const uint64_t *bitmap;
	size_t i;
	size_t len;
} xbm_iter_t;

// xattr IO subsystem assertion
#define xattr_assert(cond) if(!(cond)) do { \
	panic("xattr_assert() failed: %s",#cond); \
} while(0)

#define xattr_infomsg(...) // errmsg(__VA_ARGS__)

/*
 * Purely for documentation purposes, simulate C++ const_cast<>() in the C
 * language.
 * This makes it quite obvious that the `const' part of a cast is being removed
 * to workaround an api that accepts a non-const pointer but otherwise has no
 * reason to modify its value.
 * E.g.
 *
 * ancient_sum(a, count)
 * int *a; int count;
 * {
 *   int s;
 *   s = 0;
 *   while (count--) {
 *     s = s + a[i];
 *   }
 *   return s;
 * }
 *
 * int modern_sum(const int *a, int count) {
 *   return ancient_sum(const_cast(int *, a), count);
 * }
 */
#define const_cast(type, expr) ((type)(expr))

static uint64_t fext2cluster(const HFSPlusExtentDescriptor *fext, uint32_t fs_bsize);
static bool cluster_off_in_fext(uint64_t off, const HFSPlusExtentDescriptor *fext,
		uint32_t fs_bsize);

static bool fext2cluster_check(const HFSPlusExtentDescriptor *fext, uint32_t fs_bsize,
		uint64_t *out);
static uint32_t xattr_cluster_scale(uint32_t fs_bsize);

// xattr fext thread local storage routines
static const HFSPlusExtentDescriptor **_Nullable xattr_fext_alloc(
		xattr_io_info_t *info) __attribute__((warn_unused_result));
static void xattr_fext_free(xattr_io_info_t *info,
		const HFSPlusExtentDescriptor **xfext);
static void xattr_fext_set(xattr_io_info_t *info,
		const HFSPlusExtentDescriptor **xfext, const HFSPlusExtentDescriptor *fext);
static void xattr_fext_clear(xattr_io_info_t *info,
		const HFSPlusExtentDescriptor **xfext);

static size_t xattr_fext_index(const xattr_io_info_t *info,
		const HFSPlusExtentDescriptor **xfext);

// xattr fext free bitmap routines, namespace `xbm_'
static bool xbm_find_free(const xattr_io_info_t *info, size_t *out);
static void xbm_set_used(xattr_io_info_t *info, size_t i);
static void xbm_clear_used(xattr_io_info_t *info, size_t i);

static bool xbm_valid_index(const xattr_io_info_t *info, int64_t i);
static size_t xbm_size(const xattr_io_info_t *info);

// bitmap iterator functions
static xbm_iter_t xbm_make_iter(const uint64_t *bitmap, size_t len);
static bool xbm_iter_next(xbm_iter_t *ier);
static size_t xbm_iter_peek(const xbm_iter_t *ier);

// bitmap_ wrappers under namespace bm_
static bool bm_find(const uint64_t *bm,
		size_t from, size_t len, bool value, size_t *out);
static bool bm_valid_index(int64_t i, size_t len);
#endif

#if NAMEDSTREAMS
/*
 * Obtain the vnode for a stream.
 */
int
hfs_vnop_getnamedstream(struct vnop_getnamedstream_args* ap)
{
	vnode_t vp = ap->a_vp;
	vnode_t *svpp = ap->a_svpp;
	struct cnode *cp;
	int error = 0;

	*svpp = NULL;

	/*
	 * We only support the "com.apple.ResourceFork" stream.
	 */
	if (strcmp(ap->a_name, XATTR_RESOURCEFORK_NAME) != 0) {
		return (ENOATTR);
	}
	cp = VTOC(vp);
	if ( !S_ISREG(cp->c_mode) ) {
		return (EPERM);
	}
#if HFS_COMPRESSION
	int hide_rsrc = hfs_hides_rsrc(ap->a_context, VTOC(vp), 1);
#endif /* HFS_COMPRESSION */
	if ((error = hfs_lock(VTOC(vp), HFS_EXCLUSIVE_LOCK, HFS_LOCK_DEFAULT))) {
		return (error);
	}
	if ((!hfs_has_rsrc(cp)
#if HFS_COMPRESSION
	     || hide_rsrc
#endif /* HFS_COMPRESSION */
	     ) && (ap->a_operation != NS_OPEN)) {
		hfs_unlock(cp);
		return (ENOATTR);
	}
	error = hfs_vgetrsrc(VTOHFS(vp), vp, svpp);
	hfs_unlock(cp);

	return (error);
}

/*
 * Create a stream.
 */
int
hfs_vnop_makenamedstream(struct vnop_makenamedstream_args* ap)
{
	vnode_t vp = ap->a_vp;
	vnode_t *svpp = ap->a_svpp;
	struct cnode *cp;
	int error = 0;

	*svpp = NULL;

	/*
	 * We only support the "com.apple.ResourceFork" stream.
	 */
	if (strcmp(ap->a_name, XATTR_RESOURCEFORK_NAME) != 0) {
		return (ENOATTR);
	}
	cp = VTOC(vp);
	if ( !S_ISREG(cp->c_mode) ) {
		return (EPERM);
	}
#if HFS_COMPRESSION
	if (hfs_hides_rsrc(ap->a_context, VTOC(vp), 1)) {
		if (VNODE_IS_RSRC(vp)) {
			return EINVAL;
		} else {
			error = decmpfs_decompress_file(vp, VTOCMP(vp), -1, 1, 0);
			if (error != 0)
				return error;
		}
	}
#endif /* HFS_COMPRESSION */
	if ((error = hfs_lock(cp, HFS_EXCLUSIVE_LOCK, HFS_LOCK_DEFAULT))) {
		return (error);
	}
	error = hfs_vgetrsrc(VTOHFS(vp), vp, svpp);
	hfs_unlock(cp);

	return (error);
}

/*
 * Remove a stream.
 */
int
hfs_vnop_removenamedstream(struct vnop_removenamedstream_args* ap)
{
	vnode_t svp = ap->a_svp;
	cnode_t *scp = VTOC(svp);
	int error = 0;

	/*
	 * We only support the "com.apple.ResourceFork" stream.
	 */
	if (strcmp(ap->a_name, XATTR_RESOURCEFORK_NAME) != 0) {
		return (ENOATTR);
	}
#if HFS_COMPRESSION
	if (hfs_hides_rsrc(ap->a_context, scp, 1)) {
		/* do nothing */
		return 0;
	}
#endif /* HFS_COMPRESSION */

	hfs_lock_truncate(scp, HFS_EXCLUSIVE_LOCK, HFS_LOCK_DEFAULT);
	if (VTOF(svp)->ff_size) {
		// hfs_truncate will deal with the cnode lock
		error = hfs_truncate(svp, 0, IO_NDELAY, 0, ap->a_context);
	}
	hfs_unlock_truncate(scp, HFS_LOCK_DEFAULT);

	return error;
}
#endif


/* Zero out the date added field for the specified cnode */
static int hfs_zero_hidden_fields (struct cnode *cp, u_int8_t *finderinfo) 
{
	u_int8_t *finfo = finderinfo;
    
	/* Advance finfo by 16 bytes to the 2nd half of the finderinfo */
	finfo = finfo + 16;
	
	if (S_ISREG(cp->c_attr.ca_mode) || S_ISLNK(cp->c_attr.ca_mode)) {
		struct FndrExtendedFileInfo *extinfo = (struct FndrExtendedFileInfo *)finfo;
		extinfo->document_id = 0;
		extinfo->date_added = 0;
		extinfo->write_gen_counter = 0;
	} else if (S_ISDIR(cp->c_attr.ca_mode)) {
		struct FndrExtendedDirInfo *extinfo = (struct FndrExtendedDirInfo *)finfo;
		extinfo->document_id = 0;
		extinfo->date_added = 0;
		extinfo->write_gen_counter = 0;
	} else {
		/* Return an error */
		return -1;
	}
	return 0;
    
}

/*
 * Retrieve the data of an extended attribute.
 */
int
hfs_vnop_getxattr(struct vnop_getxattr_args *ap)
/*
	struct vnop_getxattr_args {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		char * a_name;
		uio_t a_uio;
		size_t *a_size;
		int a_options;
		vfs_context_t a_context;
	};
*/
{
	struct vnode *vp = ap->a_vp;
	struct cnode *cp;
	struct hfsmount *hfsmp;
	uio_t uio = ap->a_uio;
	size_t bufsize;
	int result;

	cp = VTOC(vp);
	if (vp == cp->c_vp) {
#if HFS_COMPRESSION
		int decmpfs_hide = hfs_hides_xattr(ap->a_context, VTOC(vp), ap->a_name, 1); /* 1 == don't take the cnode lock */
		if (decmpfs_hide && !(ap->a_options & XATTR_SHOWCOMPRESSION))
				return ENOATTR;
#endif /* HFS_COMPRESSION */
		
		/* Get the Finder Info. */
		if (strcmp(ap->a_name, XATTR_FINDERINFO_NAME) == 0) {
			u_int8_t finderinfo[32];
			bufsize = 32;

			if ((result = hfs_lock(cp, HFS_SHARED_LOCK, HFS_LOCK_DEFAULT))) {
				return (result);
			}
			/* Make a copy since we may not export all of it. */
			bcopy(cp->c_finderinfo, finderinfo, sizeof(finderinfo));
			hfs_unlock(cp);
			
			/* Zero out the date added field in the local copy */
			hfs_zero_hidden_fields (cp, finderinfo);

			/* Don't expose a symlink's private type/creator. */
			if (vnode_islnk(vp)) {
				struct FndrFileInfo *fip;

				fip = (struct FndrFileInfo *)&finderinfo;
				fip->fdType = 0;
				fip->fdCreator = 0;
			}
			/* If Finder Info is empty then it doesn't exist. */
			if (bcmp(finderinfo, emptyfinfo, sizeof(emptyfinfo)) == 0) {
				return (ENOATTR);
			}
			if (uio == NULL) {
				*ap->a_size = bufsize;
				return (0);
			}
			if ((user_size_t)uio_resid(uio) < bufsize)
				return (ERANGE);

			result = uiomove((caddr_t)&finderinfo , bufsize, uio);

			return (result);
		}
		/* Read the Resource Fork. */
		if (strcmp(ap->a_name, XATTR_RESOURCEFORK_NAME) == 0) {
			struct vnode *rvp = NULL;
			int openunlinked = 0;
			int namelen = 0;

			if ( !S_ISREG(cp->c_mode) ) {
				return (EPERM);
			}
			if ((result = hfs_lock(cp, HFS_EXCLUSIVE_LOCK, HFS_LOCK_DEFAULT))) {
				return (result);
			}
			namelen = cp->c_desc.cd_namelen;

			if (!hfs_has_rsrc(cp)) {
				hfs_unlock(cp);
				return (ENOATTR);
			}
			hfsmp = VTOHFS(vp);
			if ((cp->c_flag & C_DELETED) && (namelen == 0)) {
				openunlinked = 1;
			}
			
			result = hfs_vgetrsrc(hfsmp, vp, &rvp);
			hfs_unlock(cp);
			if (result) {
				return (result);
			}
			if (uio == NULL) {
				*ap->a_size = (size_t)VTOF(rvp)->ff_size;
			} else {
#if HFS_COMPRESSION
				user_ssize_t uio_size = 0;
				if (decmpfs_hide)
					uio_size = uio_resid(uio);
#endif /* HFS_COMPRESSION */
				result = VNOP_READ(rvp, uio, 0, ap->a_context);
#if HFS_COMPRESSION
				if (decmpfs_hide &&
				    (result == 0) &&
				    (uio_resid(uio) == uio_size)) {
					/*
					 * We intentionally make the above call to VNOP_READ so that
					 * it can return an authorization/permission/etc. Error
					 * based on ap->a_context and thus deny this operation;
					 * in that case, result != 0 and we won't proceed.
					 * 
					 * However, if result == 0, it will have returned no data
					 * because hfs_vnop_read hid the resource fork
					 * (hence uio_resid(uio) == uio_size, i.e. the uio is untouched)
					 * 
					 * In that case, we try again with the decmpfs_ctx context
					 * to get the actual data
					 */
					result = VNOP_READ(rvp, uio, 0, decmpfs_ctx);
				}
#endif /* HFS_COMPRESSION */
			}
			/* force the rsrc fork vnode to recycle right away */
			if (openunlinked) {
				int vref;
				vref = vnode_ref (rvp);
				if (vref == 0) {
					vnode_rele (rvp);
				}
				vnode_recycle(rvp);
			}
			vnode_put(rvp);
			return (result);
		}
	}
	hfsmp = VTOHFS(vp);
#if CONFIG_HFS_STD
	/*
	 * Standard HFS only supports native FinderInfo and Resource Forks.
	 */
	if (hfsmp->hfs_flags & HFS_STANDARD) {
		return (EPERM);
	}
#endif

	if ((result = hfs_lock(cp, HFS_SHARED_LOCK, HFS_LOCK_DEFAULT))) {
		return (result);
	}
	
	/* Check for non-rsrc, non-finderinfo EAs */
	result = hfs_getxattr_internal (cp, ap, VTOHFS(cp->c_vp), 0);

	hfs_unlock(cp);
	
	return MacToVFSError(result);
}

// Has same limitations as hfs_getxattr_internal below
int hfs_xattr_read(vnode_t vp, const char *name, void *data, size_t *size)
{
	uio_t uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);

	uio_addiov(uio, CAST_USER_ADDR_T(data), *size);

	struct vnop_getxattr_args args = {
		.a_uio = uio,
		.a_name = name,
		.a_size = size
	};

	int ret = hfs_getxattr_internal(VTOC(vp), &args, VTOHFS(vp), 0);

	uio_free(uio);

	return ret;
}

/*
 * getxattr_internal
 *
 * We break out this internal function which searches the attributes B-Tree and the 
 * overflow extents file to find non-resource, non-finderinfo EAs.  There may be cases 
 * where we need to get EAs in contexts where we are already holding the cnode lock, 
 * and to re-enter hfs_vnop_getxattr would cause us to double-lock the cnode.  Instead, 
 * we can just directly call this function.
 *
 * We pass the hfsmp argument directly here because we may not necessarily have a cnode to
 * operate on.  Under normal conditions, we have a file or directory to query, but if we
 * are operating on the root directory (id 1), then we may not have a cnode.  In this case, if hte
 * 'cp' argument is NULL, then we need to use the 'fileid' argument as the entry to manipulate
 *
 * NOTE: This function assumes the cnode lock for 'cp' is held exclusive or shared. 
 */ 
int hfs_getxattr_internal (struct cnode *cp, struct vnop_getxattr_args *ap, 
		struct hfsmount *hfsmp, u_int32_t fileid) 
{
	
	struct filefork *btfile;
	struct BTreeIterator * iterator = NULL;
	size_t bufsize = 0;
	HFSPlusAttrRecord *recp = NULL;
	size_t recp_size = 0;
	FSBufferDescriptor btdata;
	int lockflags = 0;
	int result = 0;
	u_int16_t datasize = 0;
	uio_t uio = ap->a_uio;
	u_int32_t target_id = 0;

	if (cp) {
		target_id = cp->c_fileid;
	} else {
		target_id = fileid;
	}


	/* Bail if we don't have an EA B-Tree. */
	if ((hfsmp->hfs_attribute_vp == NULL) ||
	   ((cp) &&  (cp->c_attr.ca_recflags & kHFSHasAttributesMask) == 0)) {
		result = ENOATTR;
		goto exit;
	}
	
	/* Initialize the B-Tree iterator for searching for the proper EA */
	btfile = VTOF(hfsmp->hfs_attribute_vp);
	
	iterator = hfs_mallocz(sizeof(*iterator));

	/* Allocate memory for reading in the attribute record.  This buffer is 
	 * big enough to read in all types of attribute records.  It is not big 
	 * enough to read inline attribute data which is read in later.
	 */
	recp = hfs_malloc(recp_size = sizeof(HFSPlusAttrRecord));
	btdata.bufferAddress = recp;
	btdata.itemSize = sizeof(HFSPlusAttrRecord);
	btdata.itemCount = 1;

	result = hfs_buildattrkey(target_id, ap->a_name, (HFSPlusAttrKey *)&iterator->key);
	if (result) {
		goto exit;
	}

	/* Lookup the attribute in the Attribute B-Tree */
	lockflags = hfs_systemfile_lock(hfsmp, SFL_ATTRIBUTE, HFS_SHARED_LOCK);
	result = BTSearchRecord(btfile, iterator, &btdata, &datasize, NULL);
	hfs_systemfile_unlock(hfsmp, lockflags);
	
	if (result) {
		if (result == btNotFound) {
			result = ENOATTR;
		}
		goto exit;
	}
	
	/* 
	 * Operate differently if we have inline EAs that can fit in the attribute B-Tree or if
	 * we have extent based EAs.
	 */
	switch (recp->recordType) {

		/* Attribute fits in the Attribute B-Tree */
		case kHFSPlusAttrInlineData: {
			/*
			 * Sanity check record size. It's not required to have any
			 * user data, so the minimum size is 2 bytes less that the
			 * size of HFSPlusAttrData (since HFSPlusAttrData struct
			 * has 2 bytes set aside for attribute data).
			 */
			if (datasize < (sizeof(HFSPlusAttrData) - 2)) {
				printf("hfs_getxattr: vol=%s %d,%s invalid record size %d (expecting %lu)\n", 
					   hfsmp->vcbVN, target_id, ap->a_name, datasize, sizeof(HFSPlusAttrData));
				result = ENOATTR;
				break;
			}
			*ap->a_size = recp->attrData.attrSize;
			if (uio && recp->attrData.attrSize != 0) {
				if (*ap->a_size > (user_size_t)uio_resid(uio)) {
					/* User provided buffer is not large enough for the xattr data */
					result = ERANGE;
				} else {
					/* Previous BTreeSearchRecord() read in only the attribute record, 
					 * and not the attribute data.  Now allocate enough memory for 
					 * both attribute record and data, and read the attribute record again. 
					 */
					bufsize = sizeof(HFSPlusAttrData) - 2 + recp->attrData.attrSize;
					hfs_free(recp, recp_size);
					recp = hfs_malloc(recp_size = bufsize);

					btdata.bufferAddress = recp;
					btdata.itemSize = bufsize;
					btdata.itemCount = 1;

					bzero(iterator, sizeof(*iterator));
					result = hfs_buildattrkey(target_id, ap->a_name, (HFSPlusAttrKey *)&iterator->key);
					if (result) {
						goto exit;
					}

					/* Lookup the attribute record and inline data */
					lockflags = hfs_systemfile_lock(hfsmp, SFL_ATTRIBUTE, HFS_SHARED_LOCK);
					result = BTSearchRecord(btfile, iterator, &btdata, &datasize, NULL);
					hfs_systemfile_unlock(hfsmp, lockflags);
					if (result) {
						if (result == btNotFound) {
							result = ENOATTR;
						}
						goto exit;
					}

					/* Copy-out the attribute data to the user buffer */
					*ap->a_size = recp->attrData.attrSize;
					result = uiomove((caddr_t) &recp->attrData.attrData , recp->attrData.attrSize, uio);
				}
			}
			break;
		}

		/* Extent-Based EAs */
		case kHFSPlusAttrForkData: {
			if (datasize < sizeof(HFSPlusAttrForkData)) {
				printf("hfs_getxattr: vol=%s %d,%s invalid record size %d (expecting %lu)\n", 
					   hfsmp->vcbVN, target_id, ap->a_name, datasize, sizeof(HFSPlusAttrForkData));
				result = ENOATTR;
				break;
			}
			*ap->a_size = recp->forkData.theFork.logicalSize;
			if (uio == NULL) {
				break;
			}
			if (*ap->a_size > (user_size_t)uio_resid(uio)) {
				result = ERANGE;
				break;
			}
			/* Process overflow extents if necessary. */
			if (has_overflow_extents(&recp->forkData.theFork)) {
				HFSPlusExtentDescriptor *extentbuf;
				HFSPlusExtentDescriptor *extentptr;
				size_t extentbufsize;
				u_int32_t totalblocks;
				u_int32_t blkcnt;
				u_int32_t attrlen;
				
				totalblocks = recp->forkData.theFork.totalBlocks;
				/* Ignore bogus block counts. */
				if (totalblocks > howmany(HFS_XATTR_MAXSIZE, hfsmp->blockSize)) {
					result = ERANGE;
					break;
				}
				attrlen = recp->forkData.theFork.logicalSize;
				
				/* Get a buffer to hold the worst case amount of extents. */
				extentbufsize = totalblocks * sizeof(HFSPlusExtentDescriptor);
				extentbufsize = roundup(extentbufsize, sizeof(HFSPlusExtentRecord));
				extentbuf = hfs_mallocz(extentbufsize);
				extentptr = extentbuf;
				
				/* Grab the first 8 extents. */
				bcopy(&recp->forkData.theFork.extents[0], extentptr, sizeof(HFSPlusExtentRecord));
				extentptr += kHFSPlusExtentDensity;
				blkcnt = count_extent_blocks(totalblocks, recp->forkData.theFork.extents);
				
				/* Now lookup the overflow extents. */
				lockflags = hfs_systemfile_lock(hfsmp, SFL_ATTRIBUTE, HFS_SHARED_LOCK);
				while (blkcnt < totalblocks) {
					((HFSPlusAttrKey *)&iterator->key)->startBlock = blkcnt;
					result = BTSearchRecord(btfile, iterator, &btdata, &datasize, NULL);
					if (result ||
						(recp->recordType != kHFSPlusAttrExtents) ||
						(datasize < sizeof(HFSPlusAttrExtents))) {
						printf("hfs_getxattr: %s missing extents, only %d blks of %d found\n",
							   ap->a_name, blkcnt, totalblocks);
						result = ENOATTR;
						break;   /* break from while */
					}
					/* Grab the next 8 extents. */
					bcopy(&recp->overflowExtents.extents[0], extentptr, sizeof(HFSPlusExtentRecord));
					extentptr += kHFSPlusExtentDensity;
					blkcnt += count_extent_blocks(totalblocks, recp->overflowExtents.extents);
				}
				
				/* Release Attr B-Tree lock */
				hfs_systemfile_unlock(hfsmp, lockflags);
				
				if (blkcnt < totalblocks) {
					result = ENOATTR;
				} else {
					result = read_attr_data(hfsmp, uio, attrlen, extentbuf);
				}
				hfs_free(extentbuf, extentbufsize);
				
			} else { /* No overflow extents. */
				result = read_attr_data(hfsmp, uio, recp->forkData.theFork.logicalSize, recp->forkData.theFork.extents);
			}
			break;
		}
			
		default:
			/* We only support Extent or inline EAs.  Default to ENOATTR for anything else */
			result = ENOATTR;
			break;		
	}
	
exit:	
	hfs_free(iterator, sizeof(*iterator));
	hfs_free(recp, recp_size);
	
	return result;
	
}


/*
 * Set the data of an extended attribute.
 */
int
hfs_vnop_setxattr(struct vnop_setxattr_args *ap)
/*
	struct vnop_setxattr_args {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		char * a_name;
		uio_t a_uio;
		int a_options;
		vfs_context_t a_context;
	};
*/
{
	struct vnode *vp = ap->a_vp;
	struct cnode *cp = NULL;
	struct hfsmount *hfsmp;
	uio_t uio = ap->a_uio;
	size_t attrsize;
	void * user_data_ptr = NULL;
	int result;
	time_t orig_ctime=VTOC(vp)->c_ctime;

	if (ap->a_name == NULL || ap->a_name[0] == '\0') {
		return (EINVAL);  /* invalid name */
	}
	hfsmp = VTOHFS(vp);
	if (VNODE_IS_RSRC(vp)) {
		return (EPERM);
	}

#if HFS_COMPRESSION
	if (hfs_hides_xattr(ap->a_context, VTOC(vp), ap->a_name, 1) ) { /* 1 == don't take the cnode lock */
		result = decmpfs_decompress_file(vp, VTOCMP(vp), -1, 1, 0);
		if (result != 0)
			return result;
	}
#endif /* HFS_COMPRESSION */

	nspace_snapshot_event(vp, orig_ctime, NAMESPACE_HANDLER_METADATA_WRITE_OP, NSPACE_REARM_NO_ARG);

	/* Set the Finder Info. */
	if (strcmp(ap->a_name, XATTR_FINDERINFO_NAME) == 0) {
		union {
			uint8_t data[32];
			char cdata[32];
			struct FndrFileInfo info;
		} fi;
		void * finderinfo_start;
		u_int8_t *finfo = NULL;
		u_int16_t fdFlags;
		u_int32_t dateadded = 0;
		u_int32_t write_gen_counter = 0;
		u_int32_t document_id = 0;

		attrsize = sizeof(VTOC(vp)->c_finderinfo);

		if ((user_size_t)uio_resid(uio) != attrsize) {
			return (ERANGE);
		}
		/* Grab the new Finder Info data. */
		if ((result = uiomove(fi.cdata, attrsize, uio))) {
			return (result);
		}

		if ((result = hfs_lock(VTOC(vp), HFS_EXCLUSIVE_LOCK, HFS_LOCK_DEFAULT))) {
			return (result);
		}
		cp = VTOC(vp);

		/* Symlink's don't have an external type/creator. */
		if (vnode_islnk(vp)) {
			/* Skip over type/creator fields. */
			finderinfo_start = &cp->c_finderinfo[8];
			attrsize -= 8;
		} else {
			finderinfo_start = &cp->c_finderinfo[0];
			/*
			 * Don't allow the external setting of
			 * file type to kHardLinkFileType.
			 */
			if (fi.info.fdType == SWAP_BE32(kHardLinkFileType)) {
				hfs_unlock(cp);
				return (EPERM);
			} 
		}

		/* Grab the current date added from the cnode */
		dateadded = hfs_get_dateadded (cp);
		if (S_ISREG(cp->c_attr.ca_mode) || S_ISLNK(cp->c_attr.ca_mode)) {
			struct FndrExtendedFileInfo *extinfo = (struct FndrExtendedFileInfo *)((u_int8_t*)cp->c_finderinfo + 16);
			/* 
			 * Grab generation counter directly from the cnode 
			 * instead of calling hfs_get_gencount(), because 
			 * for zero generation count values hfs_get_gencount() 
			 * lies and bumps it up to one.
			 */
			write_gen_counter = extinfo->write_gen_counter;
			document_id = extinfo->document_id;
		} else if (S_ISDIR(cp->c_attr.ca_mode)) {
			struct FndrExtendedDirInfo *extinfo = (struct FndrExtendedDirInfo *)((u_int8_t*)cp->c_finderinfo + 16);
			write_gen_counter = extinfo->write_gen_counter;
			document_id = extinfo->document_id;
		}

		/* 
		 * Zero out the finder info's reserved fields like date added, 
		 * generation counter, and document id to ignore user's attempts 
		 * to set it 
		 */ 
		hfs_zero_hidden_fields(cp, fi.data);

		if (bcmp(finderinfo_start, emptyfinfo, attrsize)) {
			/* attr exists and "create" was specified. */
			if (ap->a_options & XATTR_CREATE) {
				hfs_unlock(cp);
				return (EEXIST);
			}
		} else { /* empty */
			/* attr doesn't exists and "replace" was specified. */
			if (ap->a_options & XATTR_REPLACE) {
				hfs_unlock(cp);
				return (ENOATTR);
			}
		}

		/* 
		 * Now restore the date added and other reserved fields to the finderinfo to 
		 * be written out.  Advance to the 2nd half of the finderinfo to write them 
		 * out into the buffer.
		 *
		 * Make sure to endian swap the date added back into big endian.  When we used
		 * hfs_get_dateadded above to retrieve it, it swapped into local endianness
		 * for us.  But now that we're writing it out, put it back into big endian.
		 */
		finfo = &fi.data[16];
		if (S_ISREG(cp->c_attr.ca_mode) || S_ISLNK(cp->c_attr.ca_mode)) {
			struct FndrExtendedFileInfo *extinfo = (struct FndrExtendedFileInfo *)finfo;
			extinfo->date_added = OSSwapHostToBigInt32(dateadded);
			extinfo->write_gen_counter = write_gen_counter;
			extinfo->document_id = document_id;
		} else if (S_ISDIR(cp->c_attr.ca_mode)) {
			struct FndrExtendedDirInfo *extinfo = (struct FndrExtendedDirInfo *)finfo;
			extinfo->date_added = OSSwapHostToBigInt32(dateadded);
			extinfo->write_gen_counter = write_gen_counter;
			extinfo->document_id = document_id;
		}

		/* Set the cnode's Finder Info. */
		if (attrsize == sizeof(cp->c_finderinfo)) {
			bcopy(&fi.data[0], finderinfo_start, attrsize);
		} else {
			bcopy(&fi.data[8], finderinfo_start, attrsize);
		}
	
		/* Updating finderInfo updates change time and modified time */
		cp->c_touch_chgtime = TRUE;
		cp->c_flag |= C_MODIFIED;

		/*
		 * Mirror the invisible bit to the UF_HIDDEN flag.
		 *
		 * The fdFlags for files and frFlags for folders are both 8 bytes
		 * into the userInfo (the first 16 bytes of the Finder Info).  They
		 * are both 16-bit fields.
		 */
		fdFlags = *((u_int16_t *) &cp->c_finderinfo[8]);
		if (fdFlags & OSSwapHostToBigConstInt16(kFinderInvisibleMask)) {
			cp->c_bsdflags |= UF_HIDDEN;
		} else {
			cp->c_bsdflags &= ~UF_HIDDEN;
		}

		result = hfs_update(vp, 0);

		hfs_unlock(cp);
		return (result);
	}
	/* Write the Resource Fork. */
	if (strcmp(ap->a_name, XATTR_RESOURCEFORK_NAME) == 0) {
		struct vnode *rvp = NULL;
		int namelen = 0;
		int openunlinked = 0;

		if (!vnode_isreg(vp)) {
			return (EPERM);
		}
		if ((result = hfs_lock(VTOC(vp), HFS_EXCLUSIVE_LOCK, HFS_LOCK_DEFAULT))) {
			return (result);
		}
		cp = VTOC(vp);
		namelen = cp->c_desc.cd_namelen;

		if (hfs_has_rsrc(cp)) {
			/* attr exists and "create" was specified. */
			if (ap->a_options & XATTR_CREATE) {
				hfs_unlock(cp);
				return (EEXIST);
			}
		} else {
			/* attr doesn't exists and "replace" was specified. */
			if (ap->a_options & XATTR_REPLACE) {
				hfs_unlock(cp);
				return (ENOATTR);
			}
		}
		
		/*
		 * Note that we could be called on to grab the rsrc fork vnode
		 * for a file that has become open-unlinked.
		 */
		if ((cp->c_flag & C_DELETED) && (namelen == 0)) {
			openunlinked = 1;
		}

		result = hfs_vgetrsrc(hfsmp, vp, &rvp);
		hfs_unlock(cp);
		if (result) {
			return (result);
		}
		/* VNOP_WRITE marks cnode as needing a modtime update */
		result = VNOP_WRITE(rvp, uio, 0, ap->a_context);
		
		/* if open unlinked, force it inactive */
		if (openunlinked) {
			int vref;
			vref = vnode_ref (rvp);
			if (vref == 0) {
				vnode_rele(rvp);
			}
			vnode_recycle (rvp);	
		} else {
			/* cnode is not open-unlinked, so re-lock cnode to sync */
			if ((result = hfs_lock(cp, HFS_EXCLUSIVE_LOCK, HFS_LOCK_DEFAULT))) {
				vnode_recycle (rvp);
				vnode_put(rvp);
				return result;
			}
			
			/* hfs fsync rsrc fork to force to disk and update modtime */
			result = hfs_fsync (rvp, MNT_NOWAIT, 0, vfs_context_proc (ap->a_context));
			hfs_unlock (cp);
		}

		vnode_put(rvp);
		return (result);
	}
#if CONFIG_HFS_STD
	/*
	 * Standard HFS only supports native FinderInfo and Resource Forks.
	 */
	if (hfsmp->hfs_flags & HFS_STANDARD) {
		return (EPERM);
	}
#endif
	attrsize = uio_resid(uio);

	/* Enforce an upper limit. */
	if (attrsize > HFS_XATTR_MAXSIZE) {
		result = E2BIG;
		goto exit;
	}

	/*
	 * Attempt to copy the users attr data before taking any locks,
	 * only if it will be an inline attribute.  For larger attributes, 
	 * the data will be directly read from the uio.
	 */
	if (attrsize > 0 &&
	    hfsmp->hfs_max_inline_attrsize != 0 &&
	    attrsize < hfsmp->hfs_max_inline_attrsize) {
		user_data_ptr = hfs_malloc(attrsize);

		result = uiomove((caddr_t)user_data_ptr, attrsize, uio);
		if (result) {
			goto exit;
		}
	}

	result = hfs_lock(VTOC(vp), HFS_EXCLUSIVE_LOCK, HFS_LOCK_DEFAULT);
	if (result) {
		goto exit;
	}
	cp = VTOC(vp);
	
	/* 
	 * If we're trying to set a non-finderinfo, non-resourcefork EA, then
	 * call the breakout function.
	 */
	result = hfs_setxattr_internal (cp, user_data_ptr, attrsize, ap, VTOHFS(vp), 0);

 exit:
	if (cp) {
		hfs_unlock(cp);
	}
	if (user_data_ptr) {
		hfs_free(user_data_ptr, attrsize);
	}

	return (result == btNotFound ? ENOATTR : MacToVFSError(result));
}

// Has same limitations as hfs_setxattr_internal below
int hfs_xattr_write(vnode_t vp, const char *name, const void *data, size_t size)
{
	struct vnop_setxattr_args args = {
		.a_vp 	= vp,
		.a_name = name,
	};

	return hfs_setxattr_internal(VTOC(vp), data, size, &args, VTOHFS(vp), 0);
}

/*
 * hfs_setxattr_internal
 * 
 * Internal function to set non-rsrc, non-finderinfo EAs to either the attribute B-Tree or
 * extent-based EAs.
 *
 * See comments from hfs_getxattr_internal on why we need to pass 'hfsmp' and fileid here.
 * The gist is that we could end up writing to the root folder which may not have a cnode.
 *
 * Assumptions: 
 *		1. cnode 'cp' is locked EXCLUSIVE before calling this function.
 *		2. data_ptr contains data to be written.  If gathering data from userland, this must be
 *			done before calling this function.  
 *		3. If data originates entirely in-kernel, use a null UIO, and ensure the size is less than 
 *			hfsmp->hfs_max_inline_attrsize bytes long. 
 */ 
int hfs_setxattr_internal (struct cnode *cp, const void *data_ptr, size_t attrsize,
						   struct vnop_setxattr_args *ap, struct hfsmount *hfsmp, 
						   u_int32_t fileid) 
{
	uio_t uio = ap->a_uio;
	struct vnode *vp = ap->a_vp;
	int started_transaction = 0;
	struct BTreeIterator * iterator = NULL;
	struct filefork *btfile = NULL;
	FSBufferDescriptor btdata;
	HFSPlusAttrRecord attrdata;  /* 90 bytes */
	HFSPlusAttrRecord *recp = NULL;
	size_t recp_size = 0;
	HFSPlusExtentDescriptor *extentptr = NULL;
	size_t extentbufsize = 0;
	int result = 0;
	int lockflags = 0;
	int exists = 0;
	int allocatedblks = 0;
	u_int32_t target_id;

	if (cp) {
		target_id = cp->c_fileid;
	} else {
		target_id = fileid;
	}
	
	/* Start a transaction for our changes. */
	if (hfs_start_transaction(hfsmp) != 0) {
	    result = EINVAL;
	    goto exit;
	}
	started_transaction = 1;
	
	/*
	 * Once we started the transaction, nobody can compete
	 * with us, so make sure this file is still there.
	 */
	if ((cp) && (cp->c_flag & C_NOEXISTS)) {
		result = ENOENT;
		goto exit;
	}
	
	/*
	 * If there isn't an attributes b-tree then create one.
	 */
	if (hfsmp->hfs_attribute_vp == NULL) {
		result = hfs_create_attr_btree(hfsmp, ATTRIBUTE_FILE_NODE_SIZE,
		                               getnodecount(hfsmp, ATTRIBUTE_FILE_NODE_SIZE));
		if (result) {
			goto exit;
		}
	}
	if (hfsmp->hfs_max_inline_attrsize == 0) {
		hfsmp->hfs_max_inline_attrsize = getmaxinlineattrsize(hfsmp->hfs_attribute_vp);
	}

	lockflags = hfs_systemfile_lock(hfsmp, SFL_ATTRIBUTE, HFS_EXCLUSIVE_LOCK);

	/* Build the b-tree key. */
	iterator = hfs_mallocz(sizeof(*iterator));
	result = hfs_buildattrkey(target_id, ap->a_name, (HFSPlusAttrKey *)&iterator->key);
	if (result) {
		goto exit;
	}
	
	/* Preflight for replace/create semantics. */
	btfile = VTOF(hfsmp->hfs_attribute_vp);
	btdata.bufferAddress = &attrdata;
	btdata.itemSize = sizeof(attrdata);
	btdata.itemCount = 1;
	exists = BTSearchRecord(btfile, iterator, &btdata, NULL, NULL) == 0;
	
	/* Replace requires that the attribute already exists. */
	if ((ap->a_options & XATTR_REPLACE) && !exists) {
		result = ENOATTR;
		goto exit;	
	}
	/* Create requires that the attribute doesn't exist. */
	if ((ap->a_options & XATTR_CREATE) && exists) {
		result = EEXIST;
		goto exit;	
	}
	
	/* If it won't fit inline then use extent-based attributes. */
	if (attrsize > hfsmp->hfs_max_inline_attrsize) {
		int blkcnt;
		int extentblks;
		u_int32_t *keystartblk;
		int i;
		
		if (uio == NULL) {
			/*
			 * setxattrs originating from in-kernel are not supported if they are bigger
			 * than the inline max size. Just return ENOATTR and force them to do it with a
			 * smaller EA.
			 */
			result = EPERM;
			goto exit;
		}
		
		/* Get some blocks. */
		blkcnt = howmany(attrsize, hfsmp->blockSize);
		extentbufsize = blkcnt * sizeof(HFSPlusExtentDescriptor);
		extentbufsize = roundup(extentbufsize, sizeof(HFSPlusExtentRecord));
		extentptr = hfs_mallocz(extentbufsize);
		result = alloc_attr_blks(hfsmp, attrsize, extentbufsize, extentptr, &allocatedblks);
		if (result) {
			allocatedblks = 0;
			goto exit;  /* no more space */
		}
		/* Copy data into the blocks. */
		result = write_attr_data(hfsmp, uio, attrsize, extentptr);
		if (result) {
			if (vp) {
				const char *name = vnode_getname(vp);
				printf("hfs_setxattr: write_attr_data vol=%s err (%d) %s:%s\n",
						hfsmp->vcbVN, result,  name ? name : "", ap->a_name);
				if (name)
					vnode_putname(name);
			}
			goto exit;
		}

		/* Now remove any previous attribute. */
		if (exists) {
			result = remove_attribute_records(hfsmp, iterator);
			if (result) {
				if (vp) {
					const char *name = vnode_getname(vp);
					printf("hfs_setxattr: remove_attribute_records vol=%s err (%d) %s:%s\n",
							hfsmp->vcbVN, result, name ? name : "", ap->a_name);
					if (name)
						vnode_putname(name);
				}
				goto exit;
			}
		}
		/* Create attribute fork data record. */
		recp = hfs_malloc(recp_size = sizeof(HFSPlusAttrRecord));

		btdata.bufferAddress = recp;
		btdata.itemCount = 1;
		btdata.itemSize = sizeof(HFSPlusAttrForkData);
		
		recp->recordType = kHFSPlusAttrForkData;
		recp->forkData.reserved = 0;
		recp->forkData.theFork.logicalSize = attrsize;
		recp->forkData.theFork.clumpSize = 0;
		recp->forkData.theFork.totalBlocks = blkcnt;
		bcopy(extentptr, recp->forkData.theFork.extents, sizeof(HFSPlusExtentRecord));
		
		(void) hfs_buildattrkey(target_id, ap->a_name, (HFSPlusAttrKey *)&iterator->key);
		
		result = BTInsertRecord(btfile, iterator, &btdata, btdata.itemSize);
		if (result) {
			printf ("hfs_setxattr: BTInsertRecord(): vol=%s %d,%s err=%d\n", 
					hfsmp->vcbVN, target_id, ap->a_name, result);
			goto exit; 
		}
		extentblks = count_extent_blocks(blkcnt, recp->forkData.theFork.extents);
		blkcnt -= extentblks;
		keystartblk = &((HFSPlusAttrKey *)&iterator->key)->startBlock;
		i = 0;
		
		/* Create overflow extents as needed. */
		while (blkcnt > 0) {
			/* Initialize the key and record. */
			*keystartblk += (u_int32_t)extentblks;
			btdata.itemSize = sizeof(HFSPlusAttrExtents);
			recp->recordType = kHFSPlusAttrExtents;
			recp->overflowExtents.reserved = 0;
			
			/* Copy the next set of extents. */
			i += kHFSPlusExtentDensity;
			bcopy(&extentptr[i], recp->overflowExtents.extents, sizeof(HFSPlusExtentRecord));
			
			result = BTInsertRecord(btfile, iterator, &btdata, btdata.itemSize);
			if (result) {
				printf ("hfs_setxattr: BTInsertRecord() overflow: vol=%s %d,%s err=%d\n", 
						hfsmp->vcbVN, target_id, ap->a_name, result);
				goto exit;
			}
			extentblks = count_extent_blocks(blkcnt, recp->overflowExtents.extents);
			blkcnt -= extentblks;
		}
	} else { /* Inline data */ 
		if (exists) {
			result = remove_attribute_records(hfsmp, iterator);
			if (result) {
				goto exit;
			}
		}
		
		/* Calculate size of record rounded up to multiple of 2 bytes. */
		btdata.itemSize = sizeof(HFSPlusAttrData) - 2 + attrsize + ((attrsize & 1) ? 1 : 0);
		recp = hfs_malloc(recp_size = btdata.itemSize);

		recp->recordType = kHFSPlusAttrInlineData;
		recp->attrData.reserved[0] = 0;
		recp->attrData.reserved[1] = 0;
		recp->attrData.attrSize = attrsize;
		
		/* Copy in the attribute data (if any). */
		if (attrsize > 0) {
			if (data_ptr) {
				bcopy(data_ptr, &recp->attrData.attrData, attrsize);
			} else {
				/* 
				 * A null UIO meant it originated in-kernel.  If they didn't supply data_ptr 
				 * then deny the copy operation.
				 */
				if (uio == NULL) {
					result = EPERM;
					goto exit;
				}
				result = uiomove((caddr_t)&recp->attrData.attrData, attrsize, uio);
			}
			
			if (result) {
				goto exit;
			}
		}
		
		(void) hfs_buildattrkey(target_id, ap->a_name, (HFSPlusAttrKey *)&iterator->key);
		
		btdata.bufferAddress = recp;
		btdata.itemCount = 1;
		result = BTInsertRecord(btfile, iterator, &btdata, btdata.itemSize);
	}
	
exit:
	if (btfile && started_transaction) {
		(void) BTFlushPath(btfile);
	}
	hfs_systemfile_unlock(hfsmp, lockflags);
	if (result == 0) {
		if (vp) {
			cp = VTOC(vp);
			/* Setting an attribute only updates change time and not 
			 * modified time of the file.
			 */
			cp->c_touch_chgtime = TRUE;
			cp->c_flag |= C_MODIFIED;
			cp->c_attr.ca_recflags |= kHFSHasAttributesMask;
			if ((strcmp(ap->a_name, KAUTH_FILESEC_XATTR) == 0)) {
				cp->c_attr.ca_recflags |= kHFSHasSecurityMask;
			}
			(void) hfs_update(vp, 0);
		}
	}
	if (started_transaction) {
		if (result && allocatedblks) {
			free_attr_blks(hfsmp, allocatedblks, extentptr);
		}
		hfs_end_transaction(hfsmp);
	}
	
	hfs_free(recp, recp_size);
	hfs_free(extentptr, extentbufsize);
	hfs_free(iterator, sizeof(*iterator));
	
	return result;	
}




/*
 * Remove an extended attribute.
 */
int
hfs_vnop_removexattr(struct vnop_removexattr_args *ap)
/*
	struct vnop_removexattr_args {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		char * a_name;
		int a_options;
		vfs_context_t a_context;
	};
*/
{
	struct vnode *vp = ap->a_vp;
	struct cnode *cp = VTOC(vp);
	struct hfsmount *hfsmp;
	struct BTreeIterator * iterator = NULL;
	int lockflags;
	int result;
	time_t orig_ctime=VTOC(vp)->c_ctime;

	if (ap->a_name == NULL || ap->a_name[0] == '\0') {
		return (EINVAL);  /* invalid name */
	}
	hfsmp = VTOHFS(vp);
	if (VNODE_IS_RSRC(vp)) {
		return (EPERM);
	}

#if HFS_COMPRESSION
	if (hfs_hides_xattr(ap->a_context, VTOC(vp), ap->a_name, 1) && !(ap->a_options & XATTR_SHOWCOMPRESSION)) {
		return ENOATTR;
	}
#endif /* HFS_COMPRESSION */

	nspace_snapshot_event(vp, orig_ctime, NAMESPACE_HANDLER_METADATA_DELETE_OP, NSPACE_REARM_NO_ARG);
	
	/* If Resource Fork is non-empty then truncate it. */
	if (strcmp(ap->a_name, XATTR_RESOURCEFORK_NAME) == 0) {
		struct vnode *rvp = NULL;

		if ( !vnode_isreg(vp) ) {
			return (EPERM);
		}
		if ((result = hfs_lock(cp, HFS_EXCLUSIVE_LOCK, HFS_LOCK_DEFAULT))) {
			return (result);
		}
		if (!hfs_has_rsrc(cp)) {
			hfs_unlock(cp);
			return (ENOATTR);
		}
		result = hfs_vgetrsrc(hfsmp, vp, &rvp);
		hfs_unlock(cp);
		if (result) {
			return (result);
		}

		hfs_lock_truncate(VTOC(rvp), HFS_EXCLUSIVE_LOCK, HFS_LOCK_DEFAULT);

		// Tell UBC now before we take the cnode lock and start the transaction
		hfs_ubc_setsize(rvp, 0, false);

		if ((result = hfs_lock(VTOC(rvp), HFS_EXCLUSIVE_LOCK, HFS_LOCK_DEFAULT))) {
			hfs_unlock_truncate(cp, HFS_LOCK_DEFAULT);
			vnode_put(rvp);
			return (result);
		}

		/* Start a transaction for encapsulating changes in 
		 * hfs_truncate() and hfs_update()
		 */
		if ((result = hfs_start_transaction(hfsmp))) {
			hfs_unlock_truncate(cp, HFS_LOCK_DEFAULT);
			hfs_unlock(cp);
			vnode_put(rvp);
			return (result);
		}

		result = hfs_truncate(rvp, (off_t)0, IO_NDELAY, 0, ap->a_context);
		if (result == 0) {
			cp->c_touch_chgtime = TRUE;
			cp->c_flag |= C_MODIFIED;
			result = hfs_update(vp, 0);
		}

		hfs_end_transaction(hfsmp);
		hfs_unlock_truncate(VTOC(rvp), HFS_LOCK_DEFAULT);
		hfs_unlock(VTOC(rvp));

		vnode_put(rvp);
		return (result);
	}
	/* Clear out the Finder Info. */
	if (strcmp(ap->a_name, XATTR_FINDERINFO_NAME) == 0) {
		void * finderinfo_start;
		int finderinfo_size;
		u_int8_t finderinfo[32];
		u_int32_t date_added = 0, write_gen_counter = 0, document_id = 0;
		u_int8_t *finfo = NULL;
        
		if ((result = hfs_lock(cp, HFS_EXCLUSIVE_LOCK, HFS_LOCK_DEFAULT))) {
			return (result);
		}
		
		/* Use the local copy to store our temporary changes. */
		bcopy(cp->c_finderinfo, finderinfo, sizeof(finderinfo));
		
		
		/* Zero out the date added field in the local copy */
		hfs_zero_hidden_fields (cp, finderinfo);
		
		/* Don't expose a symlink's private type/creator. */
		if (vnode_islnk(vp)) {
			struct FndrFileInfo *fip;
			
			fip = (struct FndrFileInfo *)&finderinfo;
			fip->fdType = 0;
			fip->fdCreator = 0;
		}
		
		/* Do the byte compare against the local copy */
		if (bcmp(finderinfo, emptyfinfo, sizeof(emptyfinfo)) == 0) {
            hfs_unlock(cp);
			return (ENOATTR);
		}
		
		/* 
		 * If there was other content, zero out everything except 
		 * type/creator and date added.  First, save the date added.
		 */
		finfo = cp->c_finderinfo;
		finfo = finfo + 16;
		if (S_ISREG(cp->c_attr.ca_mode) || S_ISLNK(cp->c_attr.ca_mode)) {
			struct FndrExtendedFileInfo *extinfo = (struct FndrExtendedFileInfo *)finfo;
			date_added = extinfo->date_added;
			write_gen_counter = extinfo->write_gen_counter;
			document_id = extinfo->document_id;
		} else if (S_ISDIR(cp->c_attr.ca_mode)) {
			struct FndrExtendedDirInfo *extinfo = (struct FndrExtendedDirInfo *)finfo;
			date_added = extinfo->date_added;
			write_gen_counter = extinfo->write_gen_counter;
			document_id = extinfo->document_id;
		}
		
		if (vnode_islnk(vp)) {
			/* Ignore type/creator */
			finderinfo_start = &cp->c_finderinfo[8];
			finderinfo_size = sizeof(cp->c_finderinfo) - 8;
		} else {
			finderinfo_start = &cp->c_finderinfo[0];
			finderinfo_size = sizeof(cp->c_finderinfo);
		}
		bzero(finderinfo_start, finderinfo_size);
		
		
		/* Now restore the date added */
		if (S_ISREG(cp->c_attr.ca_mode) || S_ISLNK(cp->c_attr.ca_mode)) {
			struct FndrExtendedFileInfo *extinfo = (struct FndrExtendedFileInfo *)finfo;
			extinfo->date_added = date_added;
			extinfo->write_gen_counter = write_gen_counter;
			extinfo->document_id = document_id;
		} else if (S_ISDIR(cp->c_attr.ca_mode)) {
			struct FndrExtendedDirInfo *extinfo = (struct FndrExtendedDirInfo *)finfo;
			extinfo->date_added = date_added;
			extinfo->write_gen_counter = write_gen_counter;
			extinfo->document_id = document_id;
		}
        
		/* Updating finderInfo updates change time and modified time */
		cp->c_touch_chgtime = TRUE;
		cp->c_flag |= C_MODIFIED;
		hfs_update(vp, 0);
        
		hfs_unlock(cp);
        
		return (0);
	}
#if CONFIG_HFS_STD
	/*
	 * Standard HFS only supports native FinderInfo and Resource Forks.
	 */
	if (hfsmp->hfs_flags & HFS_STANDARD) {
		return (EPERM);
	}
#endif
	if (hfsmp->hfs_attribute_vp == NULL) {
		return (ENOATTR);
	}

	iterator = hfs_mallocz(sizeof(*iterator));

	if ((result = hfs_lock(cp, HFS_EXCLUSIVE_LOCK, HFS_LOCK_DEFAULT))) {
		goto exit_nolock;
	}

	result = hfs_buildattrkey(cp->c_fileid, ap->a_name, (HFSPlusAttrKey *)&iterator->key);
	if (result) {
		goto exit;	
	}

	if (hfs_start_transaction(hfsmp) != 0) {
	    result = EINVAL;
	    goto exit;
	}
	lockflags = hfs_systemfile_lock(hfsmp, SFL_ATTRIBUTE | SFL_BITMAP, HFS_EXCLUSIVE_LOCK);
	
	result = remove_attribute_records(hfsmp, iterator);

	hfs_systemfile_unlock(hfsmp, lockflags);

	if (result == 0) {
		cp->c_touch_chgtime = TRUE;

		lockflags = hfs_systemfile_lock(hfsmp, SFL_ATTRIBUTE, HFS_SHARED_LOCK);

		/* If no more attributes exist, clear attribute bit */
		result = file_attribute_exist(hfsmp, cp->c_fileid);
		if (result == 0) {
			cp->c_attr.ca_recflags &= ~kHFSHasAttributesMask;
			cp->c_flag |= C_MODIFIED;
		}
		if (result == EEXIST) {
			result = 0;
		}

		hfs_systemfile_unlock(hfsmp, lockflags);

		/* If ACL was removed, clear security bit */
		if (strcmp(ap->a_name, KAUTH_FILESEC_XATTR) == 0) {
			cp->c_attr.ca_recflags &= ~kHFSHasSecurityMask;
			cp->c_flag |= C_MODIFIED;
		}
		(void) hfs_update(vp, 0);
	}

	hfs_end_transaction(hfsmp);
exit:
	hfs_unlock(cp);
exit_nolock:
	hfs_free(iterator, sizeof(*iterator));
	return MacToVFSError(result);
}

/*
 * Removes a non rsrc-fork, non-finderinfo EA from the specified file ID.
 * Note that this results in a bit of code duplication for the xattr removal
 * path.  This is done because it's a bit messy to deal with things without the 
 * cnode.  This function is used by exchangedata to port XATTRS to alternate
 * fileIDs while everything is locked, and the cnodes are in a transitional state.  
 *
 * Assumes that the cnode backing the fileid specified is LOCKED.
 */ 

int 
hfs_removexattr_by_id (struct hfsmount *hfsmp, uint32_t fileid, const char *xattr_name ) {
	struct BTreeIterator iter; // allocated on the stack to avoid heap allocation mid-txn
	int ret = 0;
	int started_txn = 0;
	int lockflags;

	memset (&iter, 0, sizeof(iter));

	//position the B-Tree iter key before grabbing locks and starting a txn 	
	ret = hfs_buildattrkey (fileid, xattr_name, (HFSPlusAttrKey*)&iter.key);
	if (ret) {
		goto xattr_out;
	}	

	//note: this is likely a nested transaction since there is a global transaction cover
	if (hfs_start_transaction (hfsmp) != 0) {
		ret = EINVAL;		
		goto xattr_out;		
	}
	started_txn = 1;


    lockflags = hfs_systemfile_lock(hfsmp, SFL_ATTRIBUTE | SFL_BITMAP, HFS_EXCLUSIVE_LOCK);

	//actually remove the EA from the tree
    ret = remove_attribute_records(hfsmp, &iter);

    hfs_systemfile_unlock(hfsmp, lockflags);
	
	/*
   	 * NOTE: Responsibility of the caller to remove the "has XATTRs" bit in the catalog record
	 * if this was the last EA.  
	 */	 


xattr_out:
	if (started_txn) {
		hfs_end_transaction(hfsmp);
	}

	return MacToVFSError(ret);	
		
}


/* Check if any attribute record exist for given fileID.  This function 
 * is called by hfs_vnop_removexattr to determine if it should clear the 
 * attribute bit in the catalog record or not.
 * 
 * Note - you must acquire a shared lock on the attribute btree before
 *        calling this function.
 * 
 * Output: 
 * 	EEXIST	- If attribute record was found
 *	0	- Attribute was not found
 *	(other)	- Other error (such as EIO) 
 */
int
file_attribute_exist(struct hfsmount *hfsmp, uint32_t fileID)
{
	HFSPlusAttrKey *key;
	struct BTreeIterator * iterator = NULL;
	struct filefork *btfile;
	int result = 0;

	// if there's no attribute b-tree we sure as heck
	// can't have any attributes!
	if (hfsmp->hfs_attribute_vp == NULL) {
	    return false;
	}

	iterator = hfs_mallocz(sizeof(*iterator));

	key = (HFSPlusAttrKey *)&iterator->key;

	result = hfs_buildattrkey(fileID, NULL, key);
	if (result) {
		goto out;
	}

	btfile = VTOF(hfsmp->hfs_attribute_vp);
	result = BTSearchRecord(btfile, iterator, NULL, NULL, NULL);
	if (result && (result != btNotFound)) {
		goto out;
	}

	result = BTIterateRecord(btfile, kBTreeNextRecord, iterator, NULL, NULL);
	/* If no next record was found or fileID for next record did not match,
	 * no more attributes exist for this fileID
	 */
	if ((result && (result == btNotFound)) || (key->fileID != fileID)) {
		result = 0;	
	} else {
		result = EEXIST;
	}

out:
	hfs_free(iterator, sizeof(*iterator));
	return result;
}


/*
 * Remove all the records for a given attribute.
 *
 * - Used by hfs_vnop_removexattr, hfs_vnop_setxattr and hfs_removeallattr.
 * - A transaction must have been started.
 * - The Attribute b-tree file must be locked exclusive.
 * - The Allocation Bitmap file must be locked exclusive.
 * - The iterator key must be initialized.
 */
int
remove_attribute_records(struct hfsmount *hfsmp, BTreeIterator * iterator)
{
	struct filefork *btfile;
	FSBufferDescriptor btdata;
	HFSPlusAttrRecord attrdata;  /* 90 bytes */
	u_int16_t datasize;
	int result;

	btfile = VTOF(hfsmp->hfs_attribute_vp);

	btdata.bufferAddress = &attrdata;
	btdata.itemSize = sizeof(attrdata);
	btdata.itemCount = 1;
	result = BTSearchRecord(btfile, iterator, &btdata, &datasize, NULL);
	if (result) {
		goto exit; /* no records. */
	}
	/*
	 * Free the blocks from extent based attributes.
	 *
	 * Note that the block references (btree records) are removed
	 * before releasing the blocks in the allocation bitmap.
	 */
	if (attrdata.recordType == kHFSPlusAttrForkData) {
		int totalblks;
		int extentblks;
		u_int32_t *keystartblk;

		if (datasize < sizeof(HFSPlusAttrForkData)) {
			printf("hfs: remove_attribute_records: bad record size %d (expecting %lu)\n", datasize, sizeof(HFSPlusAttrForkData));
		}
		totalblks = attrdata.forkData.theFork.totalBlocks;

		/* Process the first 8 extents. */
		extentblks = count_extent_blocks(totalblks, attrdata.forkData.theFork.extents);
		if (extentblks > totalblks)
			panic("hfs: remove_attribute_records: corruption...");
		if (BTDeleteRecord(btfile, iterator) == 0) {
			free_attr_blks(hfsmp, extentblks, attrdata.forkData.theFork.extents);
		}
		totalblks -= extentblks;
		keystartblk = &((HFSPlusAttrKey *)&iterator->key)->startBlock;

		/* Process any overflow extents. */
		while (totalblks) {
			*keystartblk += (u_int32_t)extentblks;

			result = BTSearchRecord(btfile, iterator, &btdata, &datasize, NULL);
			if (result ||
			    (attrdata.recordType != kHFSPlusAttrExtents) ||
			    (datasize < sizeof(HFSPlusAttrExtents))) {
				printf("hfs: remove_attribute_records: BTSearchRecord: vol=%s, err=%d (%d), totalblks %d\n",
					hfsmp->vcbVN, MacToVFSError(result), attrdata.recordType != kHFSPlusAttrExtents, totalblks);
				result = ENOATTR;
				break;   /* break from while */
			}
			/* Process the next 8 extents. */
			extentblks = count_extent_blocks(totalblks, attrdata.overflowExtents.extents);
			if (extentblks > totalblks)
				panic("hfs: remove_attribute_records: corruption...");
			if (BTDeleteRecord(btfile, iterator) == 0) {
				free_attr_blks(hfsmp, extentblks, attrdata.overflowExtents.extents);
			}
			totalblks -= extentblks;
		}
	} else {
		result = BTDeleteRecord(btfile, iterator);
	}
	(void) BTFlushPath(btfile);
exit:
	return (result == btNotFound ? ENOATTR :  MacToVFSError(result));
}


/*
 * Retrieve the list of extended attribute names.
 */
int
hfs_vnop_listxattr(struct vnop_listxattr_args *ap)
/*
	struct vnop_listxattr_args {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		uio_t a_uio;
		size_t *a_size;
		int a_options;
		vfs_context_t a_context;
*/
{
	struct vnode *vp = ap->a_vp;
	struct cnode *cp = VTOC(vp);
	struct hfsmount *hfsmp;
	uio_t uio = ap->a_uio;
	struct BTreeIterator * iterator = NULL;
	struct filefork *btfile;
	struct listattr_callback_state state;
	user_addr_t user_start = 0;
	user_size_t user_len = 0;
	int lockflags;
	int result;
    u_int8_t finderinfo[32];


	if (VNODE_IS_RSRC(vp)) {
		return (EPERM);
	}
	
#if HFS_COMPRESSION
	int compressed = hfs_file_is_compressed(cp, 1); /* 1 == don't take the cnode lock */
#endif /* HFS_COMPRESSION */
	
	hfsmp = VTOHFS(vp);
	*ap->a_size = 0;
	
	/* 
	 * Take the truncate lock; this serializes us against the ioctl
	 * to truncate data & reset the decmpfs state
	 * in the compressed file handler. 
	 */
	hfs_lock_truncate(cp, HFS_SHARED_LOCK, HFS_LOCK_DEFAULT);

	/* Now the regular cnode lock (shared) */
	if ((result = hfs_lock(cp, HFS_SHARED_LOCK, HFS_LOCK_DEFAULT))) {
		hfs_unlock_truncate(cp, HFS_LOCK_DEFAULT);
		return (result);
	}

	/* 
	 * Make a copy of the cnode's finderinfo to a local so we can
	 * zero out the date added field.  Also zero out the private type/creator
	 * for symlinks.
	 */
	bcopy(cp->c_finderinfo, finderinfo, sizeof(finderinfo));
	hfs_zero_hidden_fields (cp, finderinfo);
	
	/* Don't expose a symlink's private type/creator. */
	if (vnode_islnk(vp)) {
		struct FndrFileInfo *fip;
		
		fip = (struct FndrFileInfo *)&finderinfo;
		fip->fdType = 0;
		fip->fdCreator = 0;
	}	

	
	/* If Finder Info is non-empty then export it's name. */
	if (bcmp(finderinfo, emptyfinfo, sizeof(emptyfinfo)) != 0) {
		if (uio == NULL) {
			*ap->a_size += sizeof(XATTR_FINDERINFO_NAME);
		} else if ((user_size_t)uio_resid(uio) < sizeof(XATTR_FINDERINFO_NAME)) {
			result = ERANGE;
			goto exit;
		} else {
			result = uiomove(XATTR_FINDERINFO_NAME,
			                  sizeof(XATTR_FINDERINFO_NAME), uio);
			if (result)
				goto exit;
		}
	}
	/* If Resource Fork is non-empty then export it's name. */
	if (S_ISREG(cp->c_mode) && hfs_has_rsrc(cp)) {
#if HFS_COMPRESSION
		if ((ap->a_options & XATTR_SHOWCOMPRESSION) ||
		    !compressed ||
		    !decmpfs_hides_rsrc(ap->a_context, VTOCMP(vp))
		    )
#endif /* HFS_COMPRESSION */
		{
			if (uio == NULL) {
				*ap->a_size += sizeof(XATTR_RESOURCEFORK_NAME);
			} else if ((user_size_t)uio_resid(uio) < sizeof(XATTR_RESOURCEFORK_NAME)) {
				result = ERANGE;
				goto exit;
			} else {
				result = uiomove(XATTR_RESOURCEFORK_NAME,
								 sizeof(XATTR_RESOURCEFORK_NAME), uio);
				if (result)
					goto exit;
			}
		}
	}
#if CONFIG_HFS_STD
	/*
	 * Standard HFS only supports native FinderInfo and Resource Forks.
	 * Return at this point.
	 */
	if (hfsmp->hfs_flags & HFS_STANDARD) {
		result = 0;
		goto exit;
	}
#endif
	/* Bail if we don't have any extended attributes. */
	if ((hfsmp->hfs_attribute_vp == NULL) ||
	    (cp->c_attr.ca_recflags & kHFSHasAttributesMask) == 0) {
		result = 0;
		goto exit;
	}
	btfile = VTOF(hfsmp->hfs_attribute_vp);

	iterator = hfs_mallocz(sizeof(*iterator));

	result = hfs_buildattrkey(cp->c_fileid, NULL, (HFSPlusAttrKey *)&iterator->key);
	if (result)
		goto exit;	

	/*
	 * Lock the user's buffer here so that we won't fault on
	 * it in uiomove while holding the attributes b-tree lock.
	 */
	if (uio && uio_isuserspace(uio)) {
		user_start = uio_curriovbase(uio);
		user_len = uio_curriovlen(uio);

		if ((result = vslock(user_start, user_len)) != 0) {
			user_start = 0;
			goto exit;
		}
	}
	lockflags = hfs_systemfile_lock(hfsmp, SFL_ATTRIBUTE, HFS_SHARED_LOCK);

	result = BTSearchRecord(btfile, iterator, NULL, NULL, NULL);
	if (result && result != btNotFound) {
		hfs_systemfile_unlock(hfsmp, lockflags);
		goto exit;
	}

	state.fileID = cp->c_fileid;
	state.result = 0;
	state.uio = uio;
	state.size = 0;
#if HFS_COMPRESSION
	state.showcompressed = !compressed || ap->a_options & XATTR_SHOWCOMPRESSION;
	state.ctx = ap->a_context;
	state.vp = vp;
#endif /* HFS_COMPRESSION */

	/*
	 * Process entries starting just after iterator->key.
	 */
	result = BTIterateRecords(btfile, kBTreeNextRecord, iterator,
	                          (IterateCallBackProcPtr)listattr_callback, &state);
	hfs_systemfile_unlock(hfsmp, lockflags);
	if (uio == NULL) {
		*ap->a_size += state.size;
	}

	if (state.result || result == btNotFound)
		result = state.result;

exit:
	if (user_start) {
		vsunlock(user_start, user_len, TRUE);
	}
	hfs_free(iterator, sizeof(*iterator));
	hfs_unlock(cp);
	hfs_unlock_truncate(cp, HFS_LOCK_DEFAULT);
	
	return MacToVFSError(result);
}


/*
 * Callback - called for each attribute record
 */
static int
listattr_callback(const HFSPlusAttrKey *key, __unused const HFSPlusAttrData *data, struct listattr_callback_state *state)
{
	char attrname[XATTR_MAXNAMELEN + 1];
	ssize_t bytecount;
	int result;

	if (state->fileID != key->fileID) {
		state->result = 0;
		return (0);	/* stop */
	}
	/*
	 * Skip over non-primary keys
	 */
	if (key->startBlock != 0) {
		return (1);	/* continue */
	}

	/* Convert the attribute name into UTF-8. */
	result = utf8_encodestr(key->attrName, key->attrNameLen * sizeof(UniChar),
				(u_int8_t *)attrname, (size_t *)&bytecount, sizeof(attrname), '/', 0);
	if (result) {
		state->result = result;
		return (0);	/* stop */
	}
	bytecount++; /* account for null termination char */

	if (xattr_protected(attrname))
		return (1);     /* continue */

#if HFS_COMPRESSION
	if (!state->showcompressed && decmpfs_hides_xattr(state->ctx, VTOCMP(state->vp), attrname) )
		return 1; /* continue */
#endif /* HFS_COMPRESSION */
	
	if (state->uio == NULL) {
		state->size += bytecount;
	} else {
		if (bytecount > uio_resid(state->uio)) {
			state->result = ERANGE;
			return (0);	/* stop */
		}
		result = uiomove((caddr_t) attrname, bytecount, state->uio);
		if (result) {
			state->result = result;
			return (0);	/* stop */
		}
	}
	return (1); /* continue */
}

/*
 * Remove all the attributes from a cnode.
 *
 * This function creates/ends its own transaction so that each
 * attribute is deleted in its own transaction (to avoid having
 * a transaction grow too large).
 *
 * This function takes the necessary locks on the attribute
 * b-tree file and the allocation (bitmap) file.
 *
 * NOTE: Upon sucecss, this function will return with an open
 * transaction.  The reason we do it this way is because when we
 * delete the last attribute, we must make sure the flag in the
 * catalog record that indicates there are no more records is cleared.
 * The caller is responsible for doing this and *must* do it before
 * ending the transaction.
 */
int
hfs_removeallattr(struct hfsmount *hfsmp, u_int32_t fileid, 
				  bool *open_transaction)
{
	BTreeIterator *iterator = NULL;
	HFSPlusAttrKey *key;
	struct filefork *btfile;
	int result, lockflags = 0;

	*open_transaction = false;

	if (hfsmp->hfs_attribute_vp == NULL)
		return 0;

	btfile = VTOF(hfsmp->hfs_attribute_vp);

	iterator = hfs_mallocz(sizeof(BTreeIterator));

	key = (HFSPlusAttrKey *)&iterator->key;

	/* Loop until there are no more attributes for this file id */
	do {
		if (!*open_transaction)
			lockflags = hfs_systemfile_lock(hfsmp, SFL_ATTRIBUTE, HFS_SHARED_LOCK);

		(void) hfs_buildattrkey(fileid, NULL, key);
		result = BTIterateRecord(btfile, kBTreeNextRecord, iterator, NULL, NULL);
		if (result || key->fileID != fileid)
			goto exit;

		hfs_systemfile_unlock(hfsmp, lockflags);
		lockflags = 0;

		if (*open_transaction) {
			hfs_end_transaction(hfsmp);
			*open_transaction = false;
		}

		if (hfs_start_transaction(hfsmp) != 0) {
			result = EINVAL;
			goto exit;
		}

		*open_transaction = true;

		lockflags = hfs_systemfile_lock(hfsmp, SFL_ATTRIBUTE | SFL_BITMAP, HFS_EXCLUSIVE_LOCK);

		result = remove_attribute_records(hfsmp, iterator);

#if HFS_XATTR_VERBOSE
		if (result) {
			printf("hfs_removeallattr: unexpected err %d\n", result);
		}
#endif
	} while (!result);

exit:
	hfs_free(iterator, sizeof(*iterator));

	if (lockflags)
		hfs_systemfile_unlock(hfsmp, lockflags);

	result = result == btNotFound ? 0 : MacToVFSError(result);

	if (result && *open_transaction) {
		hfs_end_transaction(hfsmp);
		*open_transaction = false;
	}

	return result;
}

void
hfs_xattr_init(struct hfsmount * hfsmp)
{
#if CONFIG_HFS_STD
	if (ISSET(hfsmp->hfs_flags, HFS_STANDARD))
		return;
#endif

	/*
	 * If there isn't an attributes b-tree then create one.
	 */
	if ((hfsmp->hfs_attribute_vp == NULL) &&
	    !(hfsmp->hfs_flags & HFS_READ_ONLY)) {
		(void) hfs_create_attr_btree(hfsmp, ATTRIBUTE_FILE_NODE_SIZE,
		                             getnodecount(hfsmp, ATTRIBUTE_FILE_NODE_SIZE));
	}
	if (hfsmp->hfs_attribute_vp)
		hfsmp->hfs_max_inline_attrsize = getmaxinlineattrsize(hfsmp->hfs_attribute_vp);
}

/*
 * Enable/Disable volume attributes stored as EA for root file system.
 * Supported attributes are - 
 *	1. Extent-based Extended Attributes 
 */
int
hfs_set_volxattr(struct hfsmount *hfsmp, unsigned int xattrtype, int state)
{
	struct BTreeIterator * iterator = NULL;
	struct filefork *btfile;
	int lockflags;
	int result;

#if CONFIG_HFS_STD
	if (hfsmp->hfs_flags & HFS_STANDARD) {
		return (ENOTSUP);
	}
#endif
	if (xattrtype != HFSIOC_SET_XATTREXTENTS_STATE) {
		return EINVAL;
	}

	/*
	 * If there isn't an attributes b-tree then create one.
	 */
	if (hfsmp->hfs_attribute_vp == NULL) {
		result = hfs_create_attr_btree(hfsmp, ATTRIBUTE_FILE_NODE_SIZE,
		                               getnodecount(hfsmp, ATTRIBUTE_FILE_NODE_SIZE));
		if (result) {
			return (result);
		}
	}

	iterator = hfs_mallocz(sizeof(*iterator));

	/*
	 * Build a b-tree key.
	 * We use the root's parent id (1) to hold this volume attribute.
	 */
	(void) hfs_buildattrkey(kHFSRootParentID, XATTR_XATTREXTENTS_NAME,
			      (HFSPlusAttrKey *)&iterator->key);

	/* Start a transaction for our changes. */
	if (hfs_start_transaction(hfsmp) != 0) {
		result = EINVAL;
		goto exit;
	}
	btfile = VTOF(hfsmp->hfs_attribute_vp);

	lockflags = hfs_systemfile_lock(hfsmp, SFL_ATTRIBUTE, HFS_EXCLUSIVE_LOCK);

	if (state == 0) {
		/* Remove the attribute. */
		result = BTDeleteRecord(btfile, iterator);
		if (result == btNotFound)
			result = 0;
	} else {
		FSBufferDescriptor btdata;
		HFSPlusAttrData attrdata;
		u_int16_t datasize;

		datasize = sizeof(attrdata);
		btdata.bufferAddress = &attrdata;
		btdata.itemSize = datasize;
		btdata.itemCount = 1;
		attrdata.recordType = kHFSPlusAttrInlineData;
		attrdata.reserved[0] = 0;
		attrdata.reserved[1] = 0;
		attrdata.attrSize    = 2;
		attrdata.attrData[0] = 0;
		attrdata.attrData[1] = 0;

		/* Insert the attribute. */
		result = BTInsertRecord(btfile, iterator, &btdata, datasize);
		if (result == btExists)
			result = 0;
	}
	(void) BTFlushPath(btfile);

	hfs_systemfile_unlock(hfsmp, lockflags);

	/* Finish the transaction of our changes. */
	hfs_end_transaction(hfsmp);

	/* Update the state in the mount point */
	hfs_lock_mount (hfsmp);
	if (state == 0) {
		hfsmp->hfs_flags &= ~HFS_XATTR_EXTENTS; 
	} else {
		hfsmp->hfs_flags |= HFS_XATTR_EXTENTS; 
	}
	hfs_unlock_mount (hfsmp);

exit:
	hfs_free(iterator, sizeof(*iterator));
	return MacToVFSError(result);
}


/*
 * hfs_attrkeycompare - compare two attribute b-tree keys.
 *
 * The name portion of the key is compared using a 16-bit binary comparison. 
 * This is called from the b-tree code.
 */
int
hfs_attrkeycompare(HFSPlusAttrKey *searchKey, HFSPlusAttrKey *trialKey)
{
	u_int32_t searchFileID, trialFileID;
	int result;

	searchFileID = searchKey->fileID;
	trialFileID = trialKey->fileID;
	result = 0;
	
	if (searchFileID > trialFileID) {
		++result;
	} else if (searchFileID < trialFileID) {
		--result;
	} else {
		u_int16_t * str1 = &searchKey->attrName[0];
		u_int16_t * str2 = &trialKey->attrName[0];
		int length1 = searchKey->attrNameLen;
		int length2 = trialKey->attrNameLen;
		u_int16_t c1, c2;
		int length;
	
		if (length1 < length2) {
			length = length1;
			--result;
		} else if (length1 > length2) {
			length = length2;
			++result;
		} else {
			length = length1;
		}
	
		while (length--) {
			c1 = *(str1++);
			c2 = *(str2++);
	
			if (c1 > c2) {
				result = 1;
				break;
			}
			if (c1 < c2) {
				result = -1;
				break;
			}
		}
		if (result)
			return (result);
		/*
		 * Names are equal; compare startBlock
		 */
		if (searchKey->startBlock == trialKey->startBlock) {
			return (0);
		} else {
			return (searchKey->startBlock < trialKey->startBlock ? -1 : 1);
		}
	}

	return result;
}


/*
 * hfs_buildattrkey - build an Attribute b-tree key
 */
int
hfs_buildattrkey(u_int32_t fileID, const char *attrname, HFSPlusAttrKey *key)
{
	int result = 0;
	size_t unicodeBytes = 0;

	if (attrname != NULL) {
		/*
		 * Convert filename from UTF-8 into Unicode
		 */	
		result = utf8_decodestr((const u_int8_t *)attrname, strlen(attrname), key->attrName,
					&unicodeBytes, sizeof(key->attrName), 0, 0);
		if (result) {
			if (result != ENAMETOOLONG)
				result = EINVAL;  /* name has invalid characters */
			return (result);
		}
		key->attrNameLen = unicodeBytes / sizeof(UniChar);
		key->keyLength = kHFSPlusAttrKeyMinimumLength + unicodeBytes;
	} else {
		key->attrNameLen = 0;
		key->keyLength = kHFSPlusAttrKeyMinimumLength;
	}
	key->pad = 0;
	key->fileID = fileID;
	key->startBlock = 0;

	return (0);
 }

/*
 * getnodecount - calculate starting node count for attributes b-tree.
 */
static int
getnodecount(struct hfsmount *hfsmp, size_t nodesize)
{
	u_int64_t freebytes;
	u_int64_t calcbytes;

	/*
	 * 10.4: Scale base on current catalog file size (20 %) up to 20 MB.
	 * 10.5: Attempt to be as big as the catalog clump size.
	 *
	 * Use no more than 10 % of the remaining free space.
	 */
	freebytes = (u_int64_t)hfs_freeblks(hfsmp, 0) * (u_int64_t)hfsmp->blockSize;

	calcbytes = MIN(hfsmp->hfs_catalog_cp->c_datafork->ff_size / 5, 20 * 1024 * 1024);

	calcbytes = MAX(calcbytes, hfsmp->hfs_catalog_cp->c_datafork->ff_clumpsize);
	
	calcbytes = MIN(calcbytes, freebytes / 10);

	return (MAX(2, (int)(calcbytes / nodesize)));
}


/*
 * getmaxinlineattrsize - calculate maximum inline attribute size.
 *
 * This yields 3,802 bytes for an 8K node size.
 */
static size_t
getmaxinlineattrsize(struct vnode * attrvp)
{
	struct BTreeInfoRec btinfo;
	size_t nodesize = ATTRIBUTE_FILE_NODE_SIZE;
	size_t maxsize;

	if (attrvp != NULL) {
		(void) hfs_lock(VTOC(attrvp), HFS_SHARED_LOCK, HFS_LOCK_DEFAULT);
		if (BTGetInformation(VTOF(attrvp), 0, &btinfo) == 0)
			nodesize = btinfo.nodeSize;
		hfs_unlock(VTOC(attrvp));
	}
	maxsize = nodesize;
	maxsize -= sizeof(BTNodeDescriptor);     /* minus node descriptor */
	maxsize -= 3 * sizeof(u_int16_t);        /* minus 3 index slots */
	maxsize /= 2;                            /* 2 key/rec pairs minumum */
	maxsize -= sizeof(HFSPlusAttrKey);       /* minus maximum key size */
	maxsize -= sizeof(HFSPlusAttrData) - 2;  /* minus data header */
	maxsize &= 0xFFFFFFFE;                   /* multiple of 2 bytes */
	
	return (maxsize);
}

/*
 * Initialize vnode for attribute data I/O.  
 * 
 * On success, 
 * 	- returns zero
 * 	- the attrdata vnode is initialized as hfsmp->hfs_attrdata_vp
 * 	- an iocount is taken on the attrdata vnode which exists 
 * 	  for the entire duration of the mount.  It is only dropped 
 * 	  during unmount
 * 	- the attrdata cnode is not locked
 *
 * On failure, 
 * 	- returns non-zero value
 * 	- the caller does not have to worry about any locks or references
 */
int init_attrdata_vnode(struct hfsmount *hfsmp)
{
	vnode_t vp;
	int result = 0;
	struct cat_desc cat_desc;
	struct cat_attr cat_attr;
	struct cat_fork cat_fork;
	int newvnode_flags = 0;

	bzero(&cat_desc, sizeof(cat_desc));
	cat_desc.cd_parentcnid = kHFSRootParentID;
	cat_desc.cd_nameptr = (const u_int8_t *)hfs_attrdatafilename;
	cat_desc.cd_namelen = strlen(hfs_attrdatafilename);
	cat_desc.cd_cnid = kHFSAttributeDataFileID;
	/* Tag vnode as system file, note that we can still use cluster I/O */
	cat_desc.cd_flags |= CD_ISMETA; 

	bzero(&cat_attr, sizeof(cat_attr));
	cat_attr.ca_linkcount = 1;
	cat_attr.ca_mode = S_IFREG;
	cat_attr.ca_fileid = cat_desc.cd_cnid;
	cat_attr.ca_blocks = hfsmp->totalBlocks;

	/*
	 * The attribute data file is a virtual file that spans the
	 * entire file system space.
	 *
	 * Each extent-based attribute occupies a unique portion of
	 * in this virtual file.  The cluster I/O is done using actual
	 * allocation block offsets so no additional mapping is needed
	 * for the VNOP_BLOCKMAP call.
	 *
	 * This approach allows the attribute data to be cached without
	 * incurring the high cost of using a separate vnode per attribute.
	 *
	 * Since we need to acquire the attribute b-tree file lock anyways,
	 * the virtual file doesn't introduce any additional serialization.
	 */
	bzero(&cat_fork, sizeof(cat_fork));
	cat_fork.cf_size = (u_int64_t)hfsmp->totalBlocks * (u_int64_t)hfsmp->blockSize;
	cat_fork.cf_blocks = hfsmp->totalBlocks;
	cat_fork.cf_extents[0].startBlock = 0;
	cat_fork.cf_extents[0].blockCount = cat_fork.cf_blocks;

	result = hfs_getnewvnode(hfsmp, NULL, NULL, &cat_desc, 0, &cat_attr, 
				 &cat_fork, &vp, &newvnode_flags);
	if (result == 0) {
		hfsmp->hfs_attrdata_vp = vp;
#if NEW_XATTR
		vnode_setnoreadahead(hfsmp->hfs_attrdata_vp);
#endif
		hfs_unlock(VTOC(vp));
	}
	return (result);
}

/* The following code (inside NEW_XATTR) was ported from apfs. */
#if NEW_XATTR
/*
 * This is the same as fext2cluster_check(), but overflow is asserted against.
 *
 * This is useful for places which hold the invariant that `fext' is already
 * representable when translated but additionally want to assert that that is
 * the case.
 */
static uint64_t
fext2cluster(const HFSPlusExtentDescriptor *fext, uint32_t fs_bsize)
{
	uint64_t off;
	const bool ok = fext2cluster_check(fext, fs_bsize, &off);
	xattr_assert(ok);
	return off;
}

/*
 * Translate `fext' to a cluster layer virtual offset, set via `out', that is
 * suitable for IO to the single xattr vnode.
 *
 * For any particular file extent, this will map to a unique logical offset
 * that may be used to key into the ubc. The returned value has the property
 * such that file extents on adjacent physical blocks will always be mapped to
 * different page sized multiples. Internally, this just multiplies by the
 * larger of the pagesize and the blocksize (see xattr_cluster_scale() for the
 * derivation). For further details on the importance of this in the overall
 * scheme of xattr IO, see uio_set_fext().
 *
 * This return trues if `fext' is representable in cluster layer virtual
 * offset. It may return false for corrupted file extents:
 * - extents that likely extend beyond the size of the underlying drive
 *
 * The second point should not happen under normal circumstances even for large
 * drives. Large drives (by the logic in hfs_newfs()) are automatically
 * formatted to use large block sizes: drives sized >= 16TB use 16kiB fs
 * blocksize, at which point the virtual offset computed is equal to the device
 * offset (even on a 16kiB pagesize system).
 * So as long as a drive does not exceed 2^63 bytes in capacity (which is the
 * precision of `off_t'), the internal multiplication should not overflow.
 */
static bool
fext2cluster_check(const HFSPlusExtentDescriptor *fext, uint32_t fs_bsize,
		uint64_t *out)
{
	const uint64_t pbn = fext->startBlock;
	
	// xattrs has invalid start block
	if (!pbn) {
		return false;
	}

	// scale pbn
	uint64_t off;
	if (__builtin_mul_overflow(pbn, xattr_cluster_scale(fs_bsize), &off)) {
		return false;
	}

	*out = off;

	// the whole fext should be in range
	if (__builtin_add_overflow(off, fext->blockCount * fs_bsize, &off)) {
		return false;
	}

	// don't exceed signed 64bit precision
	if (off > INT64_MAX) {
		return false;
	}

	return true;
}

/*
 * Return the scale factor for translating xattr physical block numbers into
 * a logical offset into the single xattr vnode's ubc.
 *
 * Translated blocks have two key requirements:
 * - They must not overlap.
 *   Otherwise two different blocks' contents will collide.
 * - They must not fall within the same page.
 *   Otherwise reading a whole page may pull in multiple xattr's blocks, but
 *   only decrypt with one of the xattr's keys.
 *
 * A table of all possible configurations:
 *   pagesize, fs blocksize, scale
 *   4k, 4k, 4k
 *   4k, 8k, 8k
 *   ...
 *   4k, 64k, 64k
 *   16k, 4k, 16k
 *   16k, 8k, 16k
 *   16k, 16k, 16k
 *   16k, 32k, 32k
 *   16k, 64k, 64k
 *
 * This may be expressed as
 *   scale = max(pagesize, fs blocksize)
 */
static uint32_t
xattr_cluster_scale(uint32_t fs_bsize)
{
	return MAX(PAGE_SIZE, fs_bsize);
}

/*
 * See off_in_range().
 */
static bool
off_in_range2(uint64_t off, uint64_t start, uint64_t len)
{
	return (start <= off) && (off < (start + len));
}

/*
 * Return true if `off' returned from fext2cluster() was produced from
 * `fext'.
 */
static bool
cluster_off_in_fext(uint64_t off, const HFSPlusExtentDescriptor *fext,
		uint32_t fs_bsize)
{
	return off_in_range2(off, fext2cluster(fext, fs_bsize), fext->blockCount * fs_bsize);
}

/*
 * Allocate space to save a file extent reference for xattr IO.
 *
 * This provides a mechanism to communicate file extents from top level, stream
 * based xattr IO functions down into lower level IO functions
 * (_blockmap() and _strategy()).
 *
 * This doesn't really return a file extent, it returns a reference into
 * `info->xattr_fexts' which may be pointed to a file extent reference.
 * Alternatively this could just return an integral index index, but then we'd
 * need some way to signal failure.
 *
 * Note: the returned reference cannot be assigned directly; it must be set
 * via xattr_fext_set() to correctly synchronize with a racing call to
 * xattr_fext_find().
 *
 * This call will not block; it will return NULL if no free spaces are
 * available. On success, follow with a call to xattr_fext_free().
 *
 * In terms of the implementation, this is a basic zone allocator for thread
 * local storage in disguise. it supports only one file extent to be
 * set by up to 64 threads.
 * For further details, see the documentation for each field above the
 * definition of `xattr_io_info_t'.
 */
static const HFSPlusExtentDescriptor **
xattr_fext_alloc(xattr_io_info_t *info)
{
	const HFSPlusExtentDescriptor **ret;
	size_t i;

	// search for the first free bit
	lck_spin_lock(&info->lock);
	if (!xbm_find_free(info, &i)) {
		// no free fexts
		ret = NULL;
		goto fail;
	}

	// mark that position as allocated
	xbm_set_used(info, i);
	ret = &info->xattr_fexts[i];
	xattr_assert(!*ret);

fail:
	lck_spin_unlock(&info->lock);
	return ret;
}

/*
 * Free the memory associated with an xattr fext referenced return from
 * xattr_fext_alloc().
 * This simply amounts to clearing the bit within `info->free_bm' that
 * corresponds to `xfext'. While not strictly neccessary, we also clear out the
 * xattr fext itself to hold the invariant that a clear bit within the free
 * bitmap has a corresponding NULL fext reference.
 */
static void
xattr_fext_free(xattr_io_info_t *info, const HFSPlusExtentDescriptor **xfext)
{
	lck_spin_lock(&info->lock);
	const size_t i = xattr_fext_index(info, xfext);
	xbm_clear_used(info, i);
	info->xattr_fexts[i] = NULL;
	lck_spin_unlock(&info->lock);
}

/*
 * Given an allocated xattr fext from xattr_fext_alloc() assign it to reference
 * `fext'. A copy of this fext may be returned by a subsequent call to
 * xattr_fext_find().
 *
 * This may be called multiple times for the same value of `xfext'.
 * `fext' will be borrowed until a subsequent call to xattr_fext_set() for a
 * different file extent or xattr_fext_free() for `xfext'. It must have
 * lifetime that spans at least as long from when it's first set to when it's
 * cleared either by xattr_fext_free() or xattr_fext_clear().
 *
 * Note: `fext' may be introspected by other threads via xattr_fext_find()
 * (and in terms of getxattr(), two threads may use each other's file extents
 * if they race to read the same xattr).
 */
static void
xattr_fext_set(xattr_io_info_t *info, const HFSPlusExtentDescriptor **xfext,
		const HFSPlusExtentDescriptor *fext)
{
	xattr_assert(xbm_valid_index(info, xattr_fext_index(info, xfext)));
	lck_spin_lock(&info->lock);
	*xfext = fext;
	lck_spin_unlock(&info->lock);
}

/*
 * Given a cluster layer virtual offset, attempt to look up a file extent set
 * via a previous call to xattr_fext_set().
 *
 * If such a fext is found, its value is copied to `out' and true is returned.
 * Note: `out' should reference wired memory: it will be stored to while a spin
 * lock is held; accesses must not fault.
 *
 * off_out will contain the "unvirtualized" offset
 */
bool
hfs_xattr_fext_find(xattr_io_info_t *info, uint32_t fs_bsize, uint64_t off,
				HFSPlusExtentDescriptor *out, uint64_t *off_out)
{
	bool found = false;
	lck_spin_lock(&info->lock);

	// search through all in-use fexts
	xbm_iter_t iter = xbm_make_iter(info->free_bm, xbm_size(info));
	while(xbm_iter_next(&iter)) {
		const HFSPlusExtentDescriptor *fext = info->xattr_fexts[xbm_iter_peek(&iter)];
		if (!fext || !cluster_off_in_fext(off, fext, fs_bsize)) {
			continue;
		}
		// `off' intersects; return `fext'
		*out = *fext;
		found = true;
		break;
	}

	lck_spin_unlock(&info->lock);

	if (found) {
		*off_out = ((uint64_t)out->startBlock * fs_bsize) + off - fext2cluster(out, fs_bsize);
	}

	return found;
}

/*
 * Given an allocated xattr fext, clear its reference to any `fext' passed to a
 * previous call to xattr_fext_set().
 *
 * This will end the lifetime of such a fext and prevent xattr_fext_find() from
 * taking a reference to it. From here, its backing memory can be deallocated.
 *
 * Unlike xattr_fext_free(), `xfext' will remain allocated and it may passed to
 * xattr_fext_set() again.
 */
static void
xattr_fext_clear(xattr_io_info_t *info, const HFSPlusExtentDescriptor **xfext)
{
	xattr_assert(xbm_valid_index(info, xattr_fext_index(info, xfext)));
	lck_spin_lock(&info->lock);
	*xfext = NULL;
	lck_spin_unlock(&info->lock);
}

/*
 * For an xattr file extent, `xfext', returned from a previous call to
 * xattr_fext_alloc(), return its index within `info->xattr_fexts'.
 */
static size_t
xattr_fext_index(const xattr_io_info_t *info, const HFSPlusExtentDescriptor **xfext)
{
	xattr_assert((info->xattr_fexts <= xfext) &&
			(xfext < &info->xattr_fexts[xbm_size(info)]));
	return ((uintptr_t)xfext - (uintptr_t)info->xattr_fexts) / sizeof(*xfext);
}

static void
bitmap_set_range(uint64_t *bm, int64_t index, int64_t count)
{
	int dstshift0, dstshift1;
	uint64_t dstmask0, dstmask1;
	int64_t bmi;

	dstshift0 = index % 64;
	dstshift1 = 64 - (index % 64);
	dstmask0 = ~0ULL << (index % 64);
	dstmask1 = (dstshift1 == 64) ? 0ULL : (~0ULL >> (64 - (index % 64)));

	bmi = index / 64;
	while (count >= 64) {
		bm[bmi] = (bm[bmi] & ~dstmask0) | ((~0ULL << dstshift0) & dstmask0);
		if (dstmask1)
			bm[bmi + 1] = (bm[bmi + 1] & ~dstmask1) | ((~0ULL >> dstshift1) & dstmask1);
		bmi++;
		count -= 64;
	}
	if (count) {
		// adjust dstmask to cover just the bits remaining
		dstmask0 = ((1ULL << count) - 1) << (index % 64);
		dstmask1 = (dstshift1 == 64) ? 0ULL : (((1ULL << count) - 1) >> (64 - (index % 64)));
		bm[bmi] = (bm[bmi] & ~dstmask0) | ((~0ULL << dstshift0) & dstmask0);
		if ((count > (64 - dstshift0)) && dstmask1)
			bm[bmi + 1] = (bm[bmi + 1] & ~dstmask1) | ((~0ULL >> dstshift1) & dstmask1);
	}
}

static void
bitmap_clear_range(uint64_t *bm, int64_t index, int64_t count)
{
	int dstshift0, dstshift1;
	uint64_t dstmask0, dstmask1;
	int64_t bmi;

	dstshift0 = index % 64;
	dstshift1 = 64 - (index % 64);
	dstmask0 = ~0ULL << (index % 64);
	dstmask1 = (dstshift1 == 64) ? 0ULL : (~0ULL >> (64 - (index % 64)));

	bmi = index / 64;
	while (count >= 64) {
		bm[bmi] = (bm[bmi] & ~dstmask0) | ((0ULL << dstshift0) & dstmask0);
		if (dstmask1)
			bm[bmi + 1] = (bm[bmi + 1] & ~dstmask1) | ((0ULL >> dstshift1) & dstmask1);
		bmi++;
		count -= 64;
	}
	if (count) {
		// adjust dstmask to cover just the bits remaining
		dstmask0 = ((1ULL << count) - 1) << (index % 64);
		dstmask1 = (dstshift1 == 64) ? 0ULL : (((1ULL << count) - 1) >> (64 - (index % 64)));
		bm[bmi] = (bm[bmi] & ~dstmask0) | ((0ULL << dstshift0) & dstmask0);
		if ((count > (64 - dstshift0)) && dstmask1)
			bm[bmi + 1] = (bm[bmi + 1] & ~dstmask1) | ((0ULL >> dstshift1) & dstmask1);
	}
}

static int
ctzll(uint64_t val)
{
	return (val == 0) ? 64 : __builtin_ctzll(val);
}

// search forwards through range and return index of first "bit" (0 or 1) encountered
static int
bitmap_range_find_first(int bit, const uint64_t *bm, int64_t index, int64_t count, int64_t *clear_index)
{
	int64_t bmi, check_count;
	uint64_t val;
	int pos;

	bmi = index / 64;
	while (count > 0) {
		check_count = 64 - (index % 64);
		if (check_count > count)
			check_count = count;
		val = bm[bmi] >> (index % 64);
		if (!bit)
			val = ~val;
		pos = ctzll(val);
		if (pos < check_count) {
			*clear_index = index + pos;
			return 1;
		}
		index += check_count;
		count -= check_count;
		bmi++;
	}
	return 0;
}

/*
 * Search for the first free (clear) bit within `info's free xattr fext bitmap.
 * Return false if no bits are clear.
 * If some bit is found, its index is set to `*out' and true is returned.
 */
static bool
xbm_find_free(const xattr_io_info_t *info, size_t *out)
{
	return bm_find(info->free_bm, 0, xbm_size(info), false, out);
}

/*
 * Set the bit at index `i' within `info's underlying free xattr fext bitmap.
 *
 * info->lock must be held.
 * It only makes sense to operate on one bit at a time, so this wraps
 * bitmap_set_range().
 */
static void
xbm_set_used(xattr_io_info_t *info, size_t i)
{
	xattr_assert(xbm_valid_index(info, i));
	bitmap_set_range(info->free_bm, i, 1);
}

/*
 * Clear the bit at index `i' within `info's underlying free xattr fext bitmap.
 * This is the opposite of xbm_set_used().
 */
static void
xbm_clear_used(xattr_io_info_t *info, size_t i)
{
	xattr_assert(xbm_valid_index(info, i));
	bitmap_clear_range(info->free_bm, i, 1);
}

/*
 * Return whether the given bitmap index is a valid index into `info's free
 * bitmap.
 */
static bool
xbm_valid_index(const xattr_io_info_t *info, int64_t i)
{
	return bm_valid_index(i, xbm_size(info));
}

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(A) (sizeof(A) / sizeof(A[0]))
#endif

/*
 * Return the total number of *bits* in `info's free xattr fext bitmap.
 */
static size_t
xbm_size(const xattr_io_info_t *info)
{
	// one bit per xattr fext
	return ARRAY_SIZE(info->xattr_fexts);
}

/*
 * Factory for an iterator over all the set (true value) bits in `bitmap'.
 * `len' is the length in *bits* of `bitmap'.
 *
 * The iterator is created in an uninitialized state, a call to xbm_iter_next()
 * is required to find the first set bit (this is different than
 * make_fext_iterator()). As a consequence of this, an iterator may iterate
 * zero times if no bits within `bitmap' are set. After each successful call to
 * xbm_iter_next(), xbm_iter_peek() will return the zero-based index of each
 * set bit.
 *
 * The intended use is along the lines of:
 * uint64_t *bm = ...; // a bitmap 123 bits (two uint64_ts) long
 * xbm_iterator_t iter = xbm_make_iter(bm, 123);
 * while(xbm_iter_next(&iter)) {
 *   size_t i = xbm_iter_peek(&iter);
 *   printf("index %lu is set\n", i);
 * }
 *
 * In terms of the iterator internals, we hold the invariant that a valid
 * iterator always has `i' in [0, len). A valid iterator is one for which
 * xbm_iter_peek() will return an index in range of `bitmap'. To bootstrap the
 * first iteration, `i' is set to SIZE_MAX; further details are in
 * xbm_iter_next().
 */
static xbm_iter_t
xbm_make_iter(const uint64_t *bitmap, size_t len)
{
	xattr_assert(len);
	return (xbm_iter_t) {
		.bitmap = bitmap,
		.i = SIZE_MAX, // This will overflow-to-zero on the first call to xbm_iter_next()
		.len = len
	};
}

/*
 * Advance `iter' to internally reference the next set bit within the bitmap.
 * If there are no more set bits, this returns false.
 *
 * On success, the index of the next set bit can be retrieved by
 * xbm_iter_peek().
 *
 * Internally, this searches for the first bit set at bit index >= iter->i+1.
 * For a new iterator, the value of `i' is initialized to SIZE_MAX so `i'+1
 * will unsigned integer overflow (which is well defined) to zero.
 */
static bool
xbm_iter_next(xbm_iter_t *iter)
{
	size_t i;
	// find the next set bit > `i'
	const bool found = bm_find(iter->bitmap, iter->i + 1, iter->len, true, &i);

	// if no bit is found, invalidate the iterator by setting i=len
	iter->i = found ? i : iter->len;
	return found;
}

/*
 * Return the index a the set bit after a successful call to xbm_iter_next().
 */
static size_t
xbm_iter_peek(const xbm_iter_t *iter)
{
	xattr_assert(iter->i < iter->len);
	return iter->i;
}

/*
 * Search for the first bit with `value' within bitmap `bm' >= bit index `from'
 * for at most `len' *bits*. Whether such a bit exists is returned and if
 * that's the case, its bit index is written via `out'.
 *
 * This is just a fancy wrapper around bitmap_range_find_first().
 */
static bool
bm_find(const uint64_t *bm, size_t from, size_t len, bool value, size_t *out)
{
	xattr_assert(bm_valid_index(from, len));

	// search for `value' in [from, len)
	int64_t i;
	if (!bitmap_range_find_first(value,
				const_cast(uint64_t *, bm), from, len, &i)) {
		return false;
	}

	// some bit found; check the returned index is valid
	xattr_assert(bm_valid_index(i, len));
	*out = (size_t)i;
	return true;
}

/*
 * Return true if `i' is a valid index into a bit of `len' bits.
 *
 * The underlying bitmap_ routines operate on `int64_t' indices. This is
 * mainly to safely convert to `size_t'.
 */
static bool
bm_valid_index(int64_t i, size_t len)
{
	return (i >= 0) && ((uint64_t)i < len);
}

/*
 * Virtualize `uio' offset to target xattr `fext' before a call to
 * cluster_xattr().
 *
 * The computation of the IO offset is somewhat subtle. The reason for this
 * fact largely has to do with how exactly the single xattr vnode
 * (hfsmp->hfs_attrdata_vp) caches data for multiple xattrs. First,
 * some discussion on the motivation for the single xattr vnode design. At the
 * top level, xattr apis are quite different from normal file data apis. Some
 * key properties are:
 * - xattr IO apis do no support editing or random reads
 * - xattrs may not be mmapped
 * - any one file may have an arbitrary number of xattrs
 * To contrast with a normal file, each file has a corresponding vnode which in
 * turn has its own private ubc. The only way in which xattrs are actually like
 * files is that they the have the same size limits and in
 * terms of their implementation, they use the same on-disk structures.
 * The result of this is that it is too high overhead to have one vnode per
 * xattr, but to instead to maintain a disk block-type cache for xattr data.
 * This cache is implemented as the ubc of a virtual file known as the single
 * xattr vnode. Reads and writes are serviced by the cluster layer. The cluster
 * layer operates in units of the vm pagesize. On a system for which
 * pagesize > fs blocksize, then the naïve approach of using an identity
 * mapping between ubc logical offset and device offset poses a challenge.
 * Consider the following scenario:
 * - 16k vm pagesize
 * - 4k fs blocksize
 *
 * On disk, we have two xattrs that reside on adjacent blocks:
 *       xattr A        xattr B
 *          [aaaa|aaaa|bbbb|bbbb]
 *      pbn 4    5    6    7    8
 *
 * Suppose we want to just read xattr A -- pbn 4 and 5 -- the cluster layer can
 * issue just an 8k IO, but it will store it in memory as a whole page, so we
 * would result in
 *
 *       xattr A        xattr B
 * in memory:
 *          [aaaa|aaaa|0000|0000]
 *
 * on disk:
 *          [aaaa|aaaa|bbbb|bbbb]
 *      pbn 4    5    6    7    8
 *
 * A subsequent read for pbn 6 or 7, as a part of xattr B, will find the page
 * already cached in the ubc and erroneously return zeros.
 * Instead, we could have the cluster layer issue the full 16k IO, but then we
 * run into encryption issues on per-file (really per-xattr) key volumes
 *
 *       xattr A        xattr B
 * in memory:
 *          [aaaa|aaaa|O!#W|JF%R]
 *
 * on disk:
 *          [asdf|ghjk|ZXCV|BNM,]  encrypted
 *          [aaaa|aaaa|bbbb|bbbb]  unencrypted
 *      pbn 4    5    6    7    8
 *
 * In this case, the issue is that we have the crypto state available to read
 * xattr A, but we do not have the crypto state for xattr B, so we would
 * incorrectly decrypt pbn 6, 7 in the same IO.
 *
 * The solution to this is to use a scaled mapping between ubc logical offset
 * and device offset. In this case, we use
 *   logical offset = physical block number * pagesize
 * and result looks like
 *
 *       xattr A        xattr B
 * in memory:
 *          [aaaa|aaaa|0000|0000] ... [bbbb|bbbb|0000|0000]
 *          64k                       96k
 *
 * on disk:
 *          [asdf|ghjk|ZXCV|BNM,]  encrypted
 *          [aaaa|aaaa|bbbb|bbbb]  unencrypted
 *      pbn 4    5    6    7    8
 *
 * In memory, xattr A occupies the virtual range of [64k, 96k), but its
 * contents are representable in the first 8k out of [64k, 80k). Note that the
 * mapping here is not per block but rather per xattr file extent. The contents
 * tracked by an individual file extent are logically contiguous in memory. In
 * the example above, xattr A has one file extent spanning [0, 8k). Suppose it
 * instead had two file extents -- [0, 4k) at pbn 4 and [4k, 8k) at pbn 5 --
 * the above diagram would instead look like
 * in memory:
 *          [aaaa|0000|0000|0000][aaaa|0000|0000|0000]
 *          64k                  80k
 * on disk:
 *          [aaaa|aaaa]            unencrypted
 *      pbn 4    5
 *
 * This scaled mapping approach guarantees that xattrs are always on different
 * pages from other xattrs, but it comes at an increased memory cost for
 * non-page multiple sized xattrs.
 * --
 *
 * If `fext' is not representable as a virtual offset (e.g. its phys_block_num
 * is corrupt), this function returns false.
 */
static bool
uio_set_fext(uio_t uio, const HFSPlusExtentDescriptor *fext, uint32_t fs_bsize)
{
	uint64_t off;
	if (!fext2cluster_check(fext, fs_bsize, &off)) {
		// `fext' is out of range
		return false;
	}

	uio_setoffset(uio, off);
	return true;
}
#endif  // NEW_XATTR
/*
 * Read an extent based attribute.
 */
static int
read_attr_data(struct hfsmount *hfsmp, uio_t uio, size_t datasize, HFSPlusExtentDescriptor *extents)
{
	vnode_t evp = hfsmp->hfs_attrdata_vp;
	off_t filesize;
	int bufsize;
	int64_t iosize;
	int attrsize;
	int blksize;
	int i;
	int result = 0;

	hfs_lock_truncate(VTOC(evp), HFS_SHARED_LOCK, HFS_LOCK_DEFAULT);

	bufsize = (int)uio_resid(uio);
	attrsize = (int)datasize;
	blksize = (int)hfsmp->blockSize;
	filesize = VTOF(evp)->ff_size;

#if NEW_XATTR
	// allocate an xattr fext for tls through the cluster layer
	const HFSPlusExtentDescriptor **xattr_fext;
	if (!(xattr_fext = xattr_fext_alloc(&hfsmp->hfs_xattr_io))) {
		result = ENOMEM;
		goto exit;
	}
#endif
	/*
	 * Read the attribute data one extent at a time.
	 * For the typical case there is only one extent.
	 */
	for (i = 0; (attrsize > 0) && (bufsize > 0) && (extents[i].startBlock != 0); ++i) {
		iosize = extents[i].blockCount * blksize;
		iosize = MIN(iosize, attrsize);
		iosize = MIN(iosize, bufsize);
		uio_setresid(uio, iosize);
#if NEW_XATTR
		// virtualize the IO offset to target this fext
		if (!uio_set_fext(uio, &extents[i], blksize)) {
			// `fext' is corrupted
			result = EILSEQ;
			break;
		}

		// stage the next xattr fext for IO
		xattr_fext_set(&hfsmp->hfs_xattr_io, xattr_fext, &extents[i]);

		// Set filesize to end of data read to prevent cluster read-ahead
		filesize = uio_offset(uio) + iosize;
#else
		uio_setoffset(uio, (u_int64_t)extents[i].startBlock * (u_int64_t)blksize);
#endif
		result = cluster_read(evp, uio, filesize, IO_SYNC | IO_UNIT);

#if NEW_XATTR
		// post IO, unstage this xattr fext
		xattr_fext_clear(&hfsmp->hfs_xattr_io, xattr_fext);
#endif

#if HFS_XATTR_VERBOSE
		printf("hfs: read_attr_data: cr iosize %lld [%d, %d] (%d)\n",
			iosize, extents[i].startBlock, extents[i].blockCount, result);
#endif
		if (result)
			break;
		attrsize -= iosize;
		bufsize -= iosize;
	}
	uio_setresid(uio, bufsize);
	uio_setoffset(uio, datasize);

#if NEW_XATTR
	xattr_fext_free(&hfsmp->hfs_xattr_io, xattr_fext);

exit:
#endif
	hfs_unlock_truncate(VTOC(evp), HFS_LOCK_DEFAULT);
	return (result);
}

/*
 * Write an extent based attribute.
 */
static int
write_attr_data(struct hfsmount *hfsmp, uio_t uio, size_t datasize, HFSPlusExtentDescriptor *extents)
{
	vnode_t evp = hfsmp->hfs_attrdata_vp;
	off_t filesize;
	int bufsize;
	int attrsize;
	int64_t iosize;
	int blksize;
	int i;
	int result = 0;

	hfs_lock_truncate(VTOC(evp), HFS_SHARED_LOCK, HFS_LOCK_DEFAULT);

	bufsize = uio_resid(uio);
	attrsize = (int) datasize;
	blksize = (int) hfsmp->blockSize;
	filesize = VTOF(evp)->ff_size;

#if NEW_XATTR
	// allocate an xattr fext for tls through the cluster layer
	const HFSPlusExtentDescriptor **xattr_fext;
	if (!(xattr_fext = xattr_fext_alloc(&hfsmp->hfs_xattr_io))) {
		result = ENOMEM;
		goto exit;
	}
#endif

	/*
	 * Write the attribute data one extent at a time.
	 */
	for (i = 0; (attrsize > 0) && (bufsize > 0) && (extents[i].startBlock != 0); ++i) {
		iosize = extents[i].blockCount * blksize;
		iosize = MIN(iosize, attrsize);
		iosize = MIN(iosize, bufsize);
		uio_setresid(uio, iosize);
#if NEW_XATTR
		// virtualize the IO offset to target this fext
		if (!uio_set_fext(uio, &extents[i], blksize)) {
			// `fext' is corrupted
			result = EILSEQ;
			break;
		}

		// stage the next xattr fext for IO
		xattr_fext_set(&hfsmp->hfs_xattr_io, xattr_fext, &extents[i]);

		filesize = uio_offset(uio) + iosize;
#else
		uio_setoffset(uio, (u_int64_t)extents[i].startBlock * (u_int64_t)blksize);
#endif
		result = cluster_write(evp, uio, filesize, filesize, filesize,
		                       (off_t) 0, IO_SYNC | IO_UNIT);

#if NEW_XATTR
		// post IO, unstage this xattr fext
		xattr_fext_clear(&hfsmp->hfs_xattr_io, xattr_fext);
#endif

#if HFS_XATTR_VERBOSE
		printf("hfs: write_attr_data: cw iosize %lld [%d, %d] (%d)\n",
			iosize, extents[i].startBlock, extents[i].blockCount, result);
#endif
		if (result)
			break;
		attrsize -= iosize;
		bufsize -= iosize;
	}
	uio_setresid(uio, bufsize);
	uio_setoffset(uio, datasize);

#if NEW_XATTR
	xattr_fext_free(&hfsmp->hfs_xattr_io, xattr_fext);

exit:
#endif
	hfs_unlock_truncate(VTOC(evp), HFS_LOCK_DEFAULT);
	return (result);
}

/*
 * Allocate blocks for an extent based attribute.
 */
static int
alloc_attr_blks(struct hfsmount *hfsmp, size_t attrsize, size_t extentbufsize, HFSPlusExtentDescriptor *extents, int *blocks)
{
	int blkcnt;
	int startblk;
	int lockflags;
	int i;
	int maxextents;
	int result = 0;

	startblk = hfsmp->hfs_metazone_end;
	blkcnt = howmany(attrsize, hfsmp->blockSize);
	if (blkcnt > (int)hfs_freeblks(hfsmp, 0)) {
		return (ENOSPC);
	}
	*blocks = blkcnt;
	maxextents = extentbufsize / sizeof(HFSPlusExtentDescriptor);

	lockflags = hfs_systemfile_lock(hfsmp, SFL_BITMAP, HFS_EXCLUSIVE_LOCK);

	for (i = 0; (blkcnt > 0) && (i < maxextents); i++) {
		/* Try allocating and see if we find something decent */
		result = BlockAllocate(hfsmp, startblk, blkcnt, blkcnt, 0,
				       &extents[i].startBlock, &extents[i].blockCount);
		/* 
		 * If we couldn't find anything, then re-try the allocation but allow
		 * journal flushes.
		 */
		if (result == dskFulErr) {
			result = BlockAllocate(hfsmp, startblk, blkcnt, blkcnt, HFS_ALLOC_FLUSHTXN,
					&extents[i].startBlock, &extents[i].blockCount);
		}

		
#if HFS_XATTR_VERBOSE
		printf("hfs: alloc_attr_blks: BA blkcnt %d [%d, %d] (%d)\n",
			blkcnt, extents[i].startBlock, extents[i].blockCount, result);
#endif
		if (result) {
			extents[i].startBlock = 0;
			extents[i].blockCount = 0;
			break;
		}
		blkcnt -= extents[i].blockCount;
		startblk = extents[i].startBlock + extents[i].blockCount;
	}
	/*
	 * If it didn't fit in the extents buffer then bail.
	 */
	if (blkcnt) {
		result = ENOSPC;

#if HFS_XATTR_VERBOSE
		printf("hfs: alloc_attr_blks: unexpected failure, %d blocks unallocated\n", blkcnt);
#endif
		for (; i >= 0; i--) {
			if ((blkcnt = extents[i].blockCount) != 0) {
				(void) BlockDeallocate(hfsmp, extents[i].startBlock, blkcnt, 0);
				extents[i].startBlock = 0;
				extents[i].blockCount = 0;
		    }
		}
	}

	hfs_systemfile_unlock(hfsmp, lockflags);
	return MacToVFSError(result);
}

/*
 * Release blocks from an extent based attribute.
 */
static void
free_attr_blks(struct hfsmount *hfsmp, int blkcnt, HFSPlusExtentDescriptor *extents)
{
	vnode_t evp = hfsmp->hfs_attrdata_vp;
	int remblks = blkcnt;
	int lockflags;
	int i;

	lockflags = hfs_systemfile_lock(hfsmp, SFL_BITMAP, HFS_EXCLUSIVE_LOCK);

	for (i = 0; (remblks > 0) && (extents[i].blockCount != 0); i++) {
		if (extents[i].blockCount > (u_int32_t)blkcnt) {
#if HFS_XATTR_VERBOSE
			printf("hfs: free_attr_blks: skipping bad extent [%d, %d]\n",
				extents[i].startBlock, extents[i].blockCount);
#endif
			extents[i].blockCount = 0;
			continue;
		}
		if (extents[i].startBlock == 0) {
			break;
		}
		(void)BlockDeallocate(hfsmp, extents[i].startBlock, extents[i].blockCount, 0);
		remblks -= extents[i].blockCount;
		extents[i].startBlock = 0;
		extents[i].blockCount = 0;

#if HFS_XATTR_VERBOSE
		printf("hfs: free_attr_blks: BlockDeallocate [%d, %d]\n",
		       extents[i].startBlock, extents[i].blockCount);
#endif
		/* Discard any resident pages for this block range. */
		if (evp) {
			off_t  start, end;

			start = (u_int64_t)extents[i].startBlock * (u_int64_t)hfsmp->blockSize;
			end = start + (u_int64_t)extents[i].blockCount * (u_int64_t)hfsmp->blockSize;
			(void) ubc_msync(hfsmp->hfs_attrdata_vp, start, end, &start, UBC_INVALIDATE);
		}
	}

	hfs_systemfile_unlock(hfsmp, lockflags);
}

static int
has_overflow_extents(HFSPlusForkData *forkdata)
{
	u_int32_t blocks;

	if (forkdata->extents[7].blockCount == 0)
		return (0);

	blocks = forkdata->extents[0].blockCount +
		 forkdata->extents[1].blockCount +
		 forkdata->extents[2].blockCount +
		 forkdata->extents[3].blockCount +
		 forkdata->extents[4].blockCount +
		 forkdata->extents[5].blockCount +
		 forkdata->extents[6].blockCount +
		 forkdata->extents[7].blockCount;	

	return (forkdata->totalBlocks > blocks);
}

static int
count_extent_blocks(int maxblks, HFSPlusExtentRecord extents)
{
	int blocks;
	int i;

	for (i = 0, blocks = 0; i < kHFSPlusExtentDensity; ++i) {
		/* Ignore obvious bogus extents. */
		if (extents[i].blockCount > (u_int32_t)maxblks)
			continue;
		if (extents[i].startBlock == 0 || extents[i].blockCount == 0)
			break;
		blocks += extents[i].blockCount;
	}
	return (blocks);
}

