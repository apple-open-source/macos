/*
 * ntfs_inode.h - Defines for inode structures for the NTFS kernel driver.
 *
 * Copyright (c) 2006-2011 Anton Altaparmakov.  All Rights Reserved.
 * Portions Copyright (c) 2006-2011 Apple Inc.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution. 
 * 3. Neither the name of Apple Inc. ("Apple") nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ALTERNATIVELY, provided that this notice and licensing terms are retained in
 * full, this file may be redistributed and/or modified under the terms of the
 * GNU General Public License (GPL) Version 2, in which case the provisions of
 * that version of the GPL will apply to you instead of the license terms
 * above.  You can obtain a copy of the GPL Version 2 at
 * http://developer.apple.com/opensource/licenses/gpl-2.txt.
 */

#ifndef _OSX_NTFS_INODE_H
#define _OSX_NTFS_INODE_H

#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/kernel_types.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ucred.h>
#include <sys/vnode.h>
#include <sys/xattr.h>

#include <libkern/OSTypes.h>

#include <kern/debug.h>
#include <kern/locks.h>

/* Forward declarations. */
typedef struct _ntfs_inode ntfs_inode;
typedef struct _ntfs_attr ntfs_attr;
struct _ntfs_dirhint;

/* Structures associated with ntfs inode caching. */
typedef LIST_HEAD(, _ntfs_inode) ntfs_inode_list_head;
typedef LIST_ENTRY(_ntfs_inode) ntfs_inode_list_entry;

#include "ntfs_layout.h"
#include "ntfs_runlist.h"
#include "ntfs_sfm.h"
#include "ntfs_types.h"
#include "ntfs_vnops.h"
#include "ntfs_volume.h"

/* The NTFS in-memory inode structure. */
struct _ntfs_inode {
	ntfs_inode_list_entry hash; /* Hash bucket list this inode is in. */
	ntfs_volume *vol;	/* Pointer to the ntfs volume of this inode. */
	vnode_t vn;		/* Vnode attached to the ntfs inode or NULL if
				   this is an extent ntfs inode. */
	SInt32 nr_refs;		/* This is the number of usecount references on
				   the vnode of this inode that are held by
				   ntfs driver internal entities.  For extent
				   mft records, this is always zero. */
	SInt32 nr_opens;	/* This is the number of VNOP_OPEN() calls that
				   have happened on the vnode of this inode
				   that have not had a matching VNOP_CLOSE()
				   call yet.  Note this applies only to base
				   inodes and is incremented/decremented in the
				   base inode for attribute/raw inode
				   opens/closes, too. */
	lck_rw_t lock;		/* Lock serializing changes to the inode such
				   as inode truncation and directory content
				   modification (both take the lock exclusive)
				   and calls like readdir and file read (these
				   take the lock shared). */
	u32 block_size;		/* Size in bytes of a logical block in the
				   inode.  For normal attributes this is the
				   sector size and for mst protected attributes
				   this is the size of an mst protected ntfs
				   record. */
	u8 block_size_shift; 	/* Log2 of the above. */
	lck_spin_t size_lock;	/* Lock serializing access to inode sizes. */
	s64 allocated_size;	/* Copy from the attribute record. */
	s64 data_size;		/* Copy from the attribute record. */
	s64 initialized_size;	/* Copy from the attribute record. */
	u32 flags;		/* NTFS specific flags describing this inode.
				   See ntfs_inode_flags_shift below. */
	ino64_t mft_no;		/* Number of the mft record / inode. */
	u16 seq_no;		/* Sequence number of the inode. */
	unsigned link_count;	/* Number of hard links to this inode.  Note we
				   make this field an integer, i.e. at least
				   32-bit to allow us to temporarily overflow
				   16-bits in ntfs_vnop_rename(). */
	uid_t uid;		/* Inode user owner. */
	gid_t gid;		/* Inode group owner. */
	mode_t mode;		/* Inode mode. */
	dev_t rdev;		/* For block and character device special
				   inodes this is the device. */
	FILE_ATTR_FLAGS file_attributes;	/* Cached file attributes from
						   the standard information
						   attribute. */
	struct timespec creation_time;		/* Cache of fields found in */
	struct timespec last_data_change_time;	/* the standard information */
	struct timespec last_mft_change_time;	/* attribute but in OS X time */
	struct timespec last_access_time;	/* format. */
	struct timespec backup_time;		/* Cache of field in the
						   AFP_AfpInfo stream but in
						   OS X time format. */
	FINDER_INFO finder_info;		/* Cached Finder info from the
						   AFP_AfpInfo stream. */
	/*
	 * If NInoAttr() is true, the below fields describe the attribute which
	 * this fake inode belongs to.  The actual inode of this attribute is
	 * pointed to by base_ni and nr_extents is set to -1 to indicate that
	 * base_ni is valid (see below).  For real inodes, we also set the type
	 * (AT_DATA for files and AT_INDEX_ALLOCATION for directories), with
	 * the name = NULL and name_len = 0 for files and name = I30 (ntfs
	 * driver wide constant) and name_len = 4 for directories.
	 */
	ATTR_TYPE type;		/* Attribute type of this fake inode. */
	ntfschar *name;		/* Attribute name of this fake inode. */
	u32 name_len;		/* Attribute name length of this fake inode. */
	ntfs_runlist rl;	/* If flags has the NI_NonResident bit set,
				   the runlist of the unnamed data attribute
				   (if a file) or of the index allocation
				   attribute (directory) or of the attribute
				   described by the fake inode (if NInoAttr()).
				   If rl.elements is 0, the runlist has not
				   been read in yet or has been unmapped. If
				   NI_NonResident is clear, the attribute is
				   resident (file and fake inode) or there is
				   no $I30 index allocation attribute
				   (small directory).  In the latter case
				   rl.elements is always zero. */
	ntfs_runlist url;	/* This runlist represents all uninitialized
				   regions such as holes or parts of holes that
				   have been instantiated but have not yet been
				   zeroed nor written to.  Another significance
				   is the initialized size.  When it is
				   incremented without zeroing the underlying
				   clusters these regions are entered in this
				   runlist.  This runlist must be consulted on
				   reads so that zeroes are read from the
				   uninitialized regions and must be updated on
				   writes so that uninitialized regions are
				   removed from the runlist when they are
				   zeroed or written.  Finally we have to go
				   through this runlist at sync time and have
				   to then zero out the uninitialized regions
				   and remove them from this runlist.  The way
				   we implement this runlist is that all
				   uninitialized regions are entered as
				   duplicates of their real counterparts in the
				   actual runlist @rl whilst all initialized
				   regions are stored as holes.  Thus the
				   fewer uninitialized regions an attribute has
				   the fewer elements will this runlist have.
				   And when there are no uninitialized elements
				   at all then this runlist is not in use and
				   its @url.elements field is set to zero thus
				   it consumes no extra memory allocation.
				   Storing things in this way means we do not
				   need to perform any lookups in the real
				   runlist whilst zeroing the uninitialized
				   regions.  TODO: What happens if we have
				   uninitialized regions outside the
				   initialized size and then someone increments
				   the initialized size?  We need to take care
				   that we don't get into a situation where
				   merging fragments of the real runlist and
				   fragments of the uninitialized runlist will
				   fail. */
	/*
	 * The following fields are only valid for real inodes and extent
	 * inodes.
	 */
	ntfs_inode *mft_ni;	/* Pointer to the ntfs inode of $MFT. */
	lck_mtx_t buf_lock;     /* Mutex to protect the buffer when we cannot
				   rely on buf_map(...) providing this service
				   for free. */
	buf_t m_buf;		/* Buffer containing the mft record of the
				   inode.  This should only be touched by the
				   ntfs_*mft_record_(un)map() functions. */
	u8 *m_dbuf;		/* Explicit in-memory copy (double buffer) of
				   the MFT record, only used when the MFT record
				   is not sector aligned. */
	MFT_RECORD *m;		/* Address of the buffer data and thus address
				   of the mft record.  This should only be
				   touched by the ntfs_*mft_record_(un)map()
				   functions. */
	/*
	 * Attribute list support (only for use by the attribute lookup
	 * functions).  Setup during read_inode for all inodes with attribute
	 * lists.  Only valid if NI_AttrList is set in flags, and attr_list_rl
	 * is further only valid if NI_AttrListNonResident is set.
	 */
	u32 attr_list_size;	/* Length of attribute list value in bytes. */
	u32 attr_list_alloc;	/* Number of bytes allocated for the attr_list
				   buffer. */
	u8 *attr_list;		/* Attribute list value itself. */
	ntfs_runlist attr_list_rl; /* Run list for the attribute list value. */
	union {
		struct { /* It is a directory, $MFT, or an index inode. */
			s64 last_set_bit;	/* The last bit that is set in
						   the index $BITMAP.  If not
						   known this is set to -1. */
			u32 vcn_size;		/* Size of a vcn in this
						   index. */
			COLLATION_RULE collation_rule; /* The collation rule
						   for the index. */
			u8 vcn_size_shift;	/* Log2 of the above. */
			u8 nr_dirhints;		/* Number of directory hints
						   attached to this index
						   inode. */
			u16 dirhint_tag;	/* The most recently created
						   directory hint tag. */
			TAILQ_HEAD(ntfs_dirhint_head, _ntfs_dirhint)
					dirhint_list;	/* List of directory
							   hints. */
		};
		struct { /* It is a compressed/sparse file/attribute inode. */
			s64 compressed_size;	/* Copy of compressed_size from
						   $DATA. */
			u32 compression_block_size;	/* Size of a compression
							   block (cb). */
			u8 compression_block_size_shift;/* Log2 of the size of
							   a cb. */
			u8 compression_block_clusters;	/* Number of clusters
							   per cb. */
		};
	};
	lck_mtx_t extent_lock;	/* Lock for accessing/modifying the below . */
	s32 nr_extents;	/* For a base mft record, the number of attached extent
			   inodes (0 if none), for extent records and for fake
			   inodes describing an attribute this is -1 if the
			   base inode at @base_ni is valid and 0 otherwise. */
	u32 extent_alloc; /* Number of bytes allocated for the extent_nis
			     array. */
	lck_mtx_t attr_nis_lock; /* Lock for accessing/modifying the below. */
	s32 nr_attr_nis;	/* For a base inode, the number of loaded
				   attribute inodes (0 if none).  Ignored for
				   attribut inodes and fake inodes. */
	u32 attr_nis_alloc; /* Number of bytes allocated for the attr_nis
			       array. */
	union {		/* This union is only used if nr_extents != 0. */
		struct {
			ntfs_inode **extent_nis; /* For nr_extents > 0, array
						    of the ntfs inodes of the
						    extent mft records
						    belonging to this base
						    inode which have been
						    loaded.  Allocated in
						    multiples of 4 elements. */
			ntfs_inode **attr_nis;	/* For nr_attr_nis > 0, array
						   of the loaded attribute
						   inodes.  Allocated in
						   multiples of 4 elements. */
		};
		struct {
			ntfs_inode *base_ni;	/* For nr_extents == -1, the
						   ntfs inode of the base mft
						   record. For fake inodes, the
						   real (base) inode to which
						   the attribute belongs. */
			lck_mtx_t *base_attr_nis_lock; /* Pointer to the base
							  inode
							  attr_nis_lock or
							  NULL. */
		};
	};
	ntfs_inode_list_entry inodes;	/* List of ntfs inodes attached to the
					   ntfs volume. */
};

/*
 * Defined bits for the flags field in the ntfs_inode structure.
 * (f) = files only, (d) = directories only, (a) = attributes/fake inodes only
 */
typedef enum {
	NI_Locked,		/* 1: Ntfs inode is locked. */
	NI_Alloc,		/* 1: Ntfs inode is being allocated now. */
	NI_Deleted,		/* 1: Ntfs inode has been deleted. */
	NI_Reclaim,		/* 1: Ntfs inode is being reclaimed now. */
	NI_AttrList,		/* 1: Mft record contains an attribute list. */
	NI_AttrListNonResident,	/* 1: Attribute list is non-resident. Implies
				      NI_AttrList is set. */
	NI_Attr,		/* 1: Fake inode for attribute i/o.
				   0: Real inode or extent inode. */
	NI_MstProtected,	/* 1: Attribute is protected by MST fixups.
				   0: Attribute is not protected by fixups. */
	NI_NonResident,		/* 1: Unnamed data attr is non-resident (f).
				   1: Attribute is non-resident (a). */
	NI_IndexAllocPresent = NI_NonResident,	/* 1: $I30 index alloc attr is
						   present (d). */
	NI_Compressed,		/* 1: Unnamed data attr is compressed (f).
				   1: Create compressed files by default (d).
				   1: Attribute is compressed (a). */
	NI_Encrypted,		/* 1: Unnamed data attr is encrypted (f).
				   1: Create encrypted files by default (d).
				   1: Attribute is encrypted (a). */
	NI_Sparse,		/* 1: Unnamed data attr is sparse (f).
				   1: Create sparse files by default (d).
				   1: Attribute is sparse (a). */
	NI_SparseDisabled,	/* 1: May not create sparse regions. */
	NI_NotMrecPageOwner,	/* 1: This inode does not own the mft record
				      page.
				   0: Thus inode does own the mft record
				      page. */
	NI_MrecNeedsDirtying,	/* 1: Page containing the mft record needs to
				      be marked dirty. */
	NI_Raw,			/* 1: Access the raw data of the inode rather
				      than decompressing or decrypting the
				      data for example.  Note, that NInoRaw()
				      implies NInoAttr() as well. */
	NI_DirtyTimes,		/* 1: Base ntfs inode contains updated times
				      that need to be written to the standard
				      information attribute and all directory
				      index entries pointing to the inode (f,
				      d).  Note this does not include the
				      backup time (see below). */
	NI_DirtyFileAttributes,	/* 1: Base ntfs inode contains updated file
				      attributes that need to be written to the
				      standard information attribute and all
				      directory index entries pointing to the
				      inode (f, d). */
	NI_DirtySizes,		/* 1: Base ntfs inode contains updated sizes
				      that need to be written to all directory
				      index entries pointing to the inode (f).
				      Directories always have both sizes set to
				      zero in their index entries so this is
				      not relevant. */
	NI_DirtySetFileBits,	/* 1: Base ntfs inode contains updated special
				      mode bits S_ISUID, S_ISGID, and/or
				      S_ISVTX that need to be written to the
				      SETFILEBITS EA (used by Interix POSIX
				      subsystem on Windows as installed by the
				      Windows Services For Unix, aka SFU) (f,
				      d). */
	NI_ValidBackupTime,	/* 1: Base ntfs inode contains valid backup
				      time, i.e. the backup time has either
				      been loaded from the AFP_AfpInfo stream
				      or it has been set via VNOP_SETATTR() in
				      which case it will also be marked dirty,
				      i.e. the NI_DirtyBackupTime bit will also
				      be set (f, d) if it has not been synced
				      to the AFP_AfpInfo attribute yet. */
	NI_DirtyBackupTime,	/* 1: Base ntfs inode contains updated
				      backup_time that needs to be written to
				      the AFP_AfpInfo stream (after creating it
				      if it does not exist already) (f, d). */
	NI_ValidFinderInfo,	/* 1: Base ntfs inode contains valid Finder
				      info, i.e. the Finder info has either
				      been loaded from the AFP_AfpInfo stream
				      or it has been set via VNOP_SETXATTR() in
				      which case it will also be marked dirty,
				      i.e. the NI_DirtyFinderInfo bit will also
				      be set (f, d) if it has not been synced
				      to the AFP_AfpInfo attribute yet. */
	NI_DirtyFinderInfo,	/* 1: Base ntfs inode contains updated Finder
				      info that needs to be writte to the
				      AFP_AfpInfo stream (after creating it
				      if it does not exist already) (f, d). */
} ntfs_inode_flags_shift;

/*
 * Macro to expand the NInoFoo(), NInoSetFoo(), and NInoClearFoo() functions.
 * Note, these functions are atomic but do not necessarily provide any ordering
 * guarantees.  This should be fine as they are always used with a lock held
 * and/or in places where the ordering does not matter.
 */
#define DEFINE_NINO_BIT_OPS(flag)					\
static inline u32 NIno##flag(ntfs_inode *ni)				\
{									\
	return (ni->flags >> NI_##flag) & 1;				\
}									\
static inline void NInoSet##flag(ntfs_inode *ni)			\
{									\
	(void)OSBitOrAtomic((u32)1 << NI_##flag, (UInt32*)&ni->flags);	\
}									\
static inline void NInoClear##flag(ntfs_inode *ni)			\
{									\
	(void)OSBitAndAtomic(~((u32)1 << NI_##flag), (UInt32*)&ni->flags); \
}

/*
 * As above for NInoTestSetFoo() and NInoTestClearFoo().
 */
#define DEFINE_NINO_TEST_AND_SET_BIT_OPS(flag)				\
static inline u32 NInoTestSet##flag(ntfs_inode *ni)			\
{									\
	return ((u32)OSBitOrAtomic((u32)1 << NI_##flag,			\
			(UInt32*)&ni->flags) >> NI_##flag) & 1;		\
}									\
static inline u32 NInoTestClear##flag(ntfs_inode *ni)			\
{									\
	return ((u32)OSBitAndAtomic(~((u32)1 << NI_##flag),		\
			(UInt32*)&ni->flags) >> NI_##flag) & 1;		\
}

/* Emit the ntfs inode bitops functions. */
DEFINE_NINO_BIT_OPS(Locked)
DEFINE_NINO_BIT_OPS(Alloc)

static inline void NInoClearAllocLocked(ntfs_inode *ni)
{
	(void)OSBitAndAtomic(~(((u32)1 << NI_Locked) | ((u32)1 << NI_Alloc)),
			(UInt32*)&ni->flags);
}

DEFINE_NINO_BIT_OPS(Deleted)
DEFINE_NINO_BIT_OPS(Reclaim)
DEFINE_NINO_BIT_OPS(AttrList)
DEFINE_NINO_BIT_OPS(AttrListNonResident)
DEFINE_NINO_BIT_OPS(Attr)
DEFINE_NINO_BIT_OPS(MstProtected)
DEFINE_NINO_BIT_OPS(NonResident)
DEFINE_NINO_BIT_OPS(IndexAllocPresent)
DEFINE_NINO_BIT_OPS(Compressed)
DEFINE_NINO_BIT_OPS(Encrypted)
DEFINE_NINO_BIT_OPS(Sparse)
DEFINE_NINO_BIT_OPS(SparseDisabled)
DEFINE_NINO_BIT_OPS(NotMrecPageOwner)
DEFINE_NINO_TEST_AND_SET_BIT_OPS(NotMrecPageOwner)
DEFINE_NINO_BIT_OPS(MrecNeedsDirtying)
DEFINE_NINO_TEST_AND_SET_BIT_OPS(MrecNeedsDirtying)
DEFINE_NINO_BIT_OPS(Raw)
DEFINE_NINO_BIT_OPS(DirtyTimes)
DEFINE_NINO_TEST_AND_SET_BIT_OPS(DirtyTimes)
DEFINE_NINO_BIT_OPS(DirtyFileAttributes)
DEFINE_NINO_TEST_AND_SET_BIT_OPS(DirtyFileAttributes)
DEFINE_NINO_BIT_OPS(DirtySizes)
DEFINE_NINO_TEST_AND_SET_BIT_OPS(DirtySizes)
DEFINE_NINO_BIT_OPS(DirtySetFileBits)
DEFINE_NINO_TEST_AND_SET_BIT_OPS(DirtySetFileBits)
DEFINE_NINO_BIT_OPS(ValidBackupTime)
DEFINE_NINO_BIT_OPS(DirtyBackupTime)
DEFINE_NINO_TEST_AND_SET_BIT_OPS(DirtyBackupTime)
DEFINE_NINO_BIT_OPS(ValidFinderInfo)
DEFINE_NINO_BIT_OPS(DirtyFinderInfo)
DEFINE_NINO_TEST_AND_SET_BIT_OPS(DirtyFinderInfo)

/* Function to bulk check all the Dirty* flags at once. */
static inline u32 NInoDirty(ntfs_inode *ni)
{
	return (ni->flags & (((u32)1 << NI_DirtyTimes) |
			((u32)1 << NI_DirtyFileAttributes) |
			((u32)1 << NI_DirtySizes) |
			((u32)1 << NI_DirtySetFileBits) |
			((u32)1 << NI_DirtyBackupTime) |
			((u32)1 << NI_DirtyFinderInfo))) ? 1 : 0;
}

/**
 * NTFS_I - return the ntfs inode given a vfs vnode
 * @vn:		VFS vnode
 *
 * NTFS_I() returns the ntfs inode associated with the VFS vnode @vn.
 */
static inline ntfs_inode *NTFS_I(vnode_t vn)
{
	return vnode_fsnode(vn);
}

/**
 * ntfs_attr - ntfs in memory attribute structure
 * @mft_no:	mft record number of the base mft record of this attribute
 * @name:	Unicode name of the attribute (NULL if unnamed)
 * @name_len:	length of @name in Unicode characters (0 if unnamed)
 * @type:	attribute type (see ntfs_layout.h)
 * @raw:	whether this is the raw inode (TRUE) or not (FALSE)
 *
 * This structure exists only to provide a small structure for the ntfs_inode
 * and ntfs_inode_hash related functions.
 *
 * NOTE: Elements are ordered by size to make the structure as compact as
 * possible on all architectures.
 */
struct _ntfs_attr {
	ino64_t mft_no;
	ntfschar *name;
	u32 name_len;
	ATTR_TYPE type;
	BOOL raw;
};

__private_extern__ BOOL ntfs_inode_test(ntfs_inode *ni, const ntfs_attr *na);

__private_extern__ errno_t ntfs_inode_init(ntfs_volume *vol, ntfs_inode *ni,
		const ntfs_attr *na);

/**
 * ntfs_inode_wait - wait for an ntfs inode
 * @ni:		ntfs inode to wait on
 * @lock:	drop this lock whilst waiting
 */
#define ntfs_inode_wait(ni, lock)					\
	do {								\
		(void)msleep(ni, lock, PDROP | PINOD, __FUNCTION__, 0);	\
	} while (0)

/**
 * ntfs_inode_wait_locked - wait for an ntfs inode to be unlocked
 * @ni:		ntfs inode to wait on
 * @lock:	drop this lock whilst waiting
 */
#define ntfs_inode_wait_locked(ni, lock)			\
	do {							\
	 	if (NInoLocked(ni)) {				\
			lck_mtx_t *lck = lock;			\
			do {					\
				/* Drops lock. */		\
	 			ntfs_inode_wait(ni, lck);	\
				/* Lock is dropped now. */	\
				lck = NULL;			\
	 		} while (NInoLocked(ni));		\
		} else						\
			lck_mtx_unlock(lock);			\
	} while (0)

/**
 * ntfs_inode_wakeup - wakeup all processes waiting on an ntfs inode
 * @ni:		ntfs inode to wake up
 */
static inline void ntfs_inode_wakeup(ntfs_inode *ni)
{
	wakeup(ni);
}

/**
 * ntfs_inode_unlock_alloc - unlock a newly allocated inode
 * @ni:		ntfs inode to unlock
 *
 * When a newly allocated inode is fully initialized, we need to clear the
 * NI_Alloc and NI_Locked flag and wakeup any waiters on the inode.
 */
static inline void ntfs_inode_unlock_alloc(ntfs_inode *ni)
{
	NInoClearAllocLocked(ni);
	ntfs_inode_wakeup(ni);
}

#define ntfs_inode_add_vnode(ni, is_system, parent_vn, cn)	\
	ntfs_inode_add_vnode_attr(ni, is_system, parent_vn, cn, FALSE/*isstream*/)
__private_extern__ errno_t ntfs_inode_add_vnode_attr(ntfs_inode *ni,
		const BOOL is_system, vnode_t parent_vn,
		struct componentname *cn, BOOL isstream);

__private_extern__ errno_t ntfs_inode_get(ntfs_volume *vol, ino64_t mft_no,
		const BOOL is_system, const lck_rw_type_t lock,
		ntfs_inode **nni, vnode_t parent_vn, struct componentname *cn);

__private_extern__ errno_t ntfs_attr_inode_lookup(ntfs_inode *base_ni,
		ATTR_TYPE type, ntfschar *name, u32 name_len, const BOOL raw,
		ntfs_inode **nni);

__private_extern__ errno_t ntfs_attr_inode_get_or_create(ntfs_inode *base_ni,
		ATTR_TYPE type, ntfschar *name, u32 name_len,
		const BOOL is_system, const BOOL raw, const int options,
		const lck_rw_type_t lock, ntfs_inode **nni);

/**
 * ntfs_attr_inode_get - obtain an ntfs inode corresponding to an attribute
 * @base_ni:	ntfs base inode containing the attribute
 * @type:	attribute type
 * @name:	Unicode name of the attribute (NULL if unnamed)
 * @name_len:	length of @name in Unicode characters (0 if unnamed)
 * @is_system:	true if the inode is a system inode and false otherwise
 * @lock:	locking options (see below)
 * @nni:	destination pointer for the obtained attribute ntfs inode
 *
 * Obtain the ntfs inode corresponding to the attribute specified by @type,
 * @name, and @name_len, which is present in the base mft record specified by
 * the ntfs inode @base_ni.  If @is_system is true the created vnode is marked
 * as a system vnode (via the VSYSTEM flag).
 *
 * If @lock is LCK_RW_TYPE_SHARED the attribute inode will be returned locked
 * for reading (@nni->lock) and if it is LCK_RW_TYPE_EXCLUSIVE the attribute
 * inode will be returned locked for writing (@nni->lock).  As a special case
 * if @lock is 0 it means the inode to be returned is already locked so do not
 * lock it.  This requires that the inode is already present in the inode
 * cache.  If it is not it cannot already be locked and thus you will get a
 * panic().
 *
 * If the attribute inode is in the cache, it is returned with an iocount
 * reference on the attached vnode.
 *
 * If the inode is not in the cache, a new ntfs inode is allocated and
 * initialized, ntfs_attr_inode_read_or_create() is called to read it in/create
 * it and fill in the remainder of the ntfs inode structure before finally a
 * new vnode is created and attached to the new ntfs inode.  The inode is then
 * returned with an iocount reference taken on its vnode.
 *
 * Note, for index allocation attributes, you need to use ntfs_index_inode_get()
 * instead of ntfs_attr_inode_get() as working with indices is a lot more
 * complex.
 *
 * Return 0 on success and errno on error.
 *
 * TODO: For now we do not store a name for attribute inodes.
 */
static inline errno_t ntfs_attr_inode_get(ntfs_inode *base_ni, ATTR_TYPE type,
		ntfschar *name, u32 name_len, const BOOL is_system,
		const lck_rw_type_t lock, ntfs_inode **nni)
{
	return ntfs_attr_inode_get_or_create(base_ni, type, name, name_len,
			is_system, FALSE, XATTR_REPLACE, lock, nni);
}

/**
 * ntfs_raw_inode_get - obtain the raw ntfs inode corresponding to an attribute
 * @ni:		non-raw ntfs inode containing the attribute
 * @lock:	locking options (see below)
 * @nni:	destination pointer for the obtained attribute ntfs inode
 *
 * Obtain the raw ntfs inode corresponding to the non-raw inode @ni.
 *
 * If @lock is LCK_RW_TYPE_SHARED the raw inode will be returned locked for
 * reading (@nni->lock) and if it is LCK_RW_TYPE_EXCLUSIVE the raw inode will
 * be returned locked for writing (@nni->lock).  As a special case if @lock is
 * 0 it means the inode to be returned is already locked so do not lock it.
 * This requires that the inode is already present in the inode cache.  If it
 * is not it cannot already be locked and thus you will get a panic().
 *
 * If the raw inode is in the cache, it is returned with an iocount reference
 * on the attached vnode.
 *
 * If the raw inode is not in the cache, a new ntfs inode is allocated and
 * initialized, ntfs_attr_inode_read_or_create() is called to read it in/create
 * it and fill in the remainder of the ntfs inode structure before finally a
 * new vnode is created and attached to the new ntfs inode.  The inode is then
 * returned with an iocount reference taken on its vnode.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: The non-raw ntfs inode @ni must be locked (@ni->lock).
 *
 * TODO: For now we do not store a name for attribute inodes.
 */
static inline errno_t ntfs_raw_inode_get(ntfs_inode *ni,
		const lck_rw_type_t lock, ntfs_inode **nni)
{
	if (NInoRaw(ni))
		panic("%s(): Function called for raw inode.\n", __FUNCTION__);
	return ntfs_attr_inode_get_or_create(ni, ni->type, ni->name,
			ni->name_len, FALSE, TRUE, XATTR_REPLACE, lock, nni);
}

__private_extern__ errno_t ntfs_index_inode_get(ntfs_inode *base_ni,
		ntfschar *name, u32 name_len, const BOOL is_system,
		ntfs_inode **nni);

__private_extern__ errno_t ntfs_extent_inode_get(ntfs_inode *base_ni,
		MFT_REF mref, ntfs_inode **ext_ni);

__private_extern__ void ntfs_inode_afpinfo_cache(ntfs_inode *ni, AFPINFO *afp,
		const unsigned afp_size);

__private_extern__ errno_t ntfs_inode_afpinfo_read(ntfs_inode *ni);

__private_extern__ errno_t ntfs_inode_afpinfo_write(ntfs_inode *ni);

__private_extern__ errno_t ntfs_inode_inactive(ntfs_inode *ni);

__private_extern__ errno_t ntfs_inode_reclaim(ntfs_inode *ni);

__private_extern__ errno_t ntfs_inode_sync(ntfs_inode *ni, const int sync,
		const BOOL skip_mft_record_sync);

__private_extern__ errno_t ntfs_inode_get_name_and_parent_mref(ntfs_inode *ni,
		BOOL have_parent, MFT_REF *mref, const char *name);

__private_extern__ errno_t ntfs_inode_is_parent(ntfs_inode *parent_ni,
		ntfs_inode *child_ni, BOOL *is_parent, ntfs_inode *forbid_ni);

#endif /* !_OSX_NTFS_INODE_H */
