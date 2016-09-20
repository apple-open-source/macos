/*
 * attrib.h - Exports for attribute handling.
 *
 * Copyright (c) 2000-2004 Anton Altaparmakov
 * Copyright (c) 2004-2005 Yura Pakhuchiy
 * Copyright (c) 2006-2007 Szabolcs Szakacsits
 * Copyright (c) 2010      Jean-Pierre Andre
 * Copyright (c) 2008-2012 Tuxera Inc.
 *
 * See LICENSE file for licensing information.
 */

#ifndef _NTFS_ATTRIB_H
#define _NTFS_ATTRIB_H

/* Forward declarations */
typedef struct _ntfs_attr ntfs_attr;
typedef struct _ntfs_attr_search_ctx ntfs_attr_search_ctx;

#include "types.h"
#include "inode.h"
#include "unistr.h"
#include "runlist.h"
#include "volume.h"
#include "debug.h"
#include "logging.h"

extern ntfschar AT_UNNAMED[];
extern ntfschar STREAM_SDS[];

/* The little endian Unicode string $TXF_DATA as a global constant. */
extern ntfschar TXF_DATA[10];

/**
 * enum ntfs_lcn_special_values - special return values for ntfs_*_vcn_to_lcn()
 *
 * Special return values for ntfs_rl_vcn_to_lcn() and ntfs_attr_vcn_to_lcn().
 *
 * TODO: Describe them.
 */
typedef enum {
	LCN_HOLE		= -1,	/* Keep this as highest value or die! */
	LCN_RL_NOT_MAPPED	= -2,
	LCN_ENOENT		= -3,
	LCN_EINVAL		= -4,
	LCN_EIO			= -5,
} ntfs_lcn_special_values;

typedef enum {			/* ways of processing holes when expanding */
	HOLES_NO,
	HOLES_OK,
	HOLES_DELAY
} hole_type;

/**
 * struct ntfs_attr_search_ctx - search context used in attribute search functions
 * @mrec:	buffer containing mft record to search
 * @attr:	attribute record in @mrec where to begin/continue search
 * @is_first:	if true lookup_attr() begins search with @attr, else after @attr
 *
 * Structure must be initialized to zero before the first call to one of the
 * attribute search functions. Initialize @mrec to point to the mft record to
 * search, and @attr to point to the first attribute within @mrec (not necessary
 * if calling the _first() functions), and set @is_first to TRUE (not necessary
 * if calling the _first() functions).
 *
 * If @is_first is TRUE, the search begins with @attr. If @is_first is FALSE,
 * the search begins after @attr. This is so that, after the first call to one
 * of the search attribute functions, we can call the function again, without
 * any modification of the search context, to automagically get the next
 * matching attribute.
 */
struct _ntfs_attr_search_ctx {
	MFT_RECORD *mrec;
	ATTR_RECORD *attr;
	BOOL is_first;
	ntfs_inode *ntfs_ino;
	ATTR_LIST_ENTRY *al_entry;
	ntfs_inode *base_ntfs_ino;
	MFT_RECORD *base_mrec;
	ATTR_RECORD *base_attr;
};

extern void ntfs_attr_reinit_search_ctx(ntfs_attr_search_ctx *ctx);
extern ntfs_attr_search_ctx *ntfs_attr_get_search_ctx(ntfs_inode *ni,
		MFT_RECORD *mrec);
extern void ntfs_attr_put_search_ctx(ntfs_attr_search_ctx *ctx);

extern int ntfs_attr_lookup(const ATTR_TYPES type, const ntfschar *name,
		const u32 name_len, const IGNORE_CASE_BOOL ic,
		const VCN lowest_vcn, const u8 *val, const u32 val_len,
		ntfs_attr_search_ctx *ctx);

extern int ntfs_attr_position(const ATTR_TYPES type, ntfs_attr_search_ctx *ctx);

extern ATTR_DEF *ntfs_attr_find_in_attrdef(const ntfs_volume *vol,
		const ATTR_TYPES type);

/**
 * struct ntfs_attr - ntfs in memory non-resident attribute structure
 * @rl:			if not NULL, the decompressed runlist
 * @ni:			base ntfs inode to which this attribute belongs
 * @type:		attribute type
 * @name:		Unicode name of the attribute
 * @name_len:		length of @name in Unicode characters
 * @state:		NTFS attribute specific flags describing this attribute
 * @allocated_size:	copy from the attribute record
 * @data_size:		copy from the attribute record
 * @initialized_size:	copy from the attribute record
 * @compressed_size:	copy from the attribute record
 * @compression_block_size:		size of a compression block (cb)
 * @compression_block_size_bits:	log2 of the size of a cb
 * @compression_block_clusters:		number of clusters per cb
 *
 * This structure exists purely to provide a mechanism of caching the runlist
 * of an attribute. If you want to operate on a particular attribute extent,
 * you should not be using this structure at all. If you want to work with a
 * resident attribute, you should not be using this structure at all. As a
 * fail-safe check make sure to test NAttrNonResident() and if it is false, you
 * know you shouldn't be using this structure.
 *
 * If you want to work on a resident attribute or on a specific attribute
 * extent, you should use ntfs_lookup_attr() to retrieve the attribute (extent)
 * record, edit that, and then write back the mft record (or set the
 * corresponding ntfs inode dirty for delayed write back).
 *
 * @rl is the decompressed runlist of the attribute described by this
 * structure. Obviously this only makes sense if the attribute is not resident,
 * i.e. NAttrNonResident() is true. If the runlist hasn't been decompressed yet
 * @rl is NULL, so be prepared to cope with @rl == NULL.
 *
 * @ni is the base ntfs inode of the attribute described by this structure.
 *
 * @type is the attribute type (see layout.h for the definition of ATTR_TYPES),
 * @name and @name_len are the little endian Unicode name and the name length
 * in Unicode characters of the attribute, respectively.
 *
 * @state contains NTFS attribute specific flags describing this attribute
 * structure. See ntfs_attr_state_bits above.
 */
struct _ntfs_attr {
	runlist_element *rl;
	ntfs_inode *ni;
	ATTR_TYPES type;
	ATTR_FLAGS data_flags;
	ntfschar *name;
	u32 name_len;
	unsigned long state;
	s64 allocated_size;
	s64 data_size;
	s64 initialized_size;
	s64 compressed_size;
	u32 compression_block_size;
	u8 compression_block_size_bits;
	u8 compression_block_clusters;
	s8 unused_runs; /* pre-reserved entries available */
};

/**
 * enum ntfs_attr_state_bits - bits for the state field in the ntfs_attr
 * structure
 */
typedef enum {
	NA_Initialized,		/* 1: structure is initialized. */
	NA_NonResident,		/* 1: Attribute is not resident. */
	NA_BeingNonResident,	/* 1: Attribute is being made not resident. */
	NA_FullyMapped,		/* 1: Attribute has been fully mapped */
	NA_DataAppending,	/* 1: Attribute is being appended to */
	NA_ComprClosing,	/* 1: Compressed attribute is being closed */
} ntfs_attr_state_bits;

#define  test_nattr_flag(na, flag)	 test_bit(NA_##flag, (na)->state)
#define   set_nattr_flag(na, flag)	  set_bit(NA_##flag, (na)->state)
#define clear_nattr_flag(na, flag)	clear_bit(NA_##flag, (na)->state)

#define NAttrInitialized(na)		 test_nattr_flag(na, Initialized)
#define NAttrSetInitialized(na)		  set_nattr_flag(na, Initialized)
#define NAttrClearInitialized(na)	clear_nattr_flag(na, Initialized)

#define NAttrNonResident(na)		 test_nattr_flag(na, NonResident)
#define NAttrSetNonResident(na)		  set_nattr_flag(na, NonResident)
#define NAttrClearNonResident(na)	clear_nattr_flag(na, NonResident)

#define NAttrBeingNonResident(na)	test_nattr_flag(na, BeingNonResident)
#define NAttrSetBeingNonResident(na)	set_nattr_flag(na, BeingNonResident)
#define NAttrClearBeingNonResident(na)	clear_nattr_flag(na, BeingNonResident)

#define NAttrFullyMapped(na)		test_nattr_flag(na, FullyMapped)
#define NAttrSetFullyMapped(na)		set_nattr_flag(na, FullyMapped)
#define NAttrClearFullyMapped(na)	clear_nattr_flag(na, FullyMapped)

#define NAttrDataAppending(na)		test_nattr_flag(na, DataAppending)
#define NAttrSetDataAppending(na)	set_nattr_flag(na, DataAppending)
#define NAttrClearDataAppending(na)	clear_nattr_flag(na, DataAppending)

#define NAttrComprClosing(na)		test_nattr_flag(na, ComprClosing)
#define NAttrSetComprClosing(na)	set_nattr_flag(na, ComprClosing)
#define NAttrClearComprClosing(na)	clear_nattr_flag(na, ComprClosing)

#define GenNAttrIno(func_name, flag)			\
extern int NAttr##func_name(ntfs_attr *na);		\
extern void NAttrSet##func_name(ntfs_attr *na);		\
extern void NAttrClear##func_name(ntfs_attr *na);

GenNAttrIno(Compressed, FILE_ATTR_COMPRESSED)
GenNAttrIno(Encrypted, 	FILE_ATTR_ENCRYPTED)
GenNAttrIno(Sparse, 	FILE_ATTR_SPARSE_FILE)
#undef GenNAttrIno

extern void ntfs_attr_init(ntfs_attr *na, const BOOL non_resident,
		const ATTR_FLAGS data_flags, const BOOL encrypted,
		const BOOL sparse,
		const s64 allocated_size, const s64 data_size,
		const s64 initialized_size, const s64 compressed_size,
		const u8 compression_unit);

	/* warning : in the following "name" has to be freeable */
	/* or one of constants AT_UNNAMED, NTFS_INDEX_I30 or STREAM_SDS */
extern ntfs_attr *ntfs_attr_open(ntfs_inode *ni, const ATTR_TYPES type,
		ntfschar *name, u32 name_len);
extern void ntfs_attr_close(ntfs_attr *na);

extern s64 ntfs_attr_pread(ntfs_attr *na, const s64 pos, s64 count,
		void *b);
extern s64 ntfs_attr_pwrite(ntfs_attr *na, const s64 pos, s64 count,
		const void *b);

extern void *ntfs_attr_readall(ntfs_inode *ni, const ATTR_TYPES type,
			       ntfschar *name, u32 name_len, s64 *data_size);

extern s64 ntfs_attr_mst_pread(ntfs_attr *na, const s64 pos,
		const s64 bk_cnt, const u32 bk_size, void *dst);
extern s64 ntfs_attr_mst_pwrite(ntfs_attr *na, const s64 pos,
		s64 bk_cnt, const u32 bk_size, void *src);

extern int ntfs_attr_map_runlist(ntfs_attr *na, VCN vcn);
extern int ntfs_attr_map_whole_runlist(ntfs_attr *na);

extern runlist_element *ntfs_attr_find_vcn(ntfs_attr *na, const VCN vcn);

extern int ntfs_attr_size_bounds_check(const ntfs_volume *vol,
		const ATTR_TYPES type, const s64 size);
extern int ntfs_attr_can_be_resident(const ntfs_volume *vol,
		const ATTR_TYPES type);
int ntfs_attr_make_non_resident(ntfs_attr *na,
		ntfs_attr_search_ctx *ctx);
extern int ntfs_make_room_for_attr(MFT_RECORD *m, u8 *pos, u32 size);

extern int ntfs_resident_attr_record_add(ntfs_inode *ni, ATTR_TYPES type,
		ntfschar *name, u8 name_len, u8 *val, u32 size,
		ATTR_FLAGS flags);
extern int ntfs_non_resident_attr_record_add(ntfs_inode *ni, ATTR_TYPES type,
		ntfschar *name, u8 name_len, VCN lowest_vcn, int dataruns_size,
		ATTR_FLAGS flags);
extern int ntfs_attr_record_rm(ntfs_attr_search_ctx *ctx);

extern int ntfs_attr_add(ntfs_inode *ni, ATTR_TYPES type,
		ntfschar *name, u8 name_len, u8 *val, s64 size);
extern int ntfs_attr_rm(ntfs_attr *na);

extern int ntfs_attr_record_resize(MFT_RECORD *m, ATTR_RECORD *a, u32 new_size);

extern int ntfs_resident_attr_value_resize(MFT_RECORD *m, ATTR_RECORD *a,
		const u32 new_size);

extern int ntfs_attr_record_move_to(ntfs_attr_search_ctx *ctx, ntfs_inode *ni);
extern int ntfs_attr_record_move_away(ntfs_attr_search_ctx *ctx, int extra);

extern int ntfs_attr_update_mapping_pairs(ntfs_attr *na, VCN from_vcn);

extern int ntfs_attr_truncate(ntfs_attr *na, const s64 newsize);

/**
 * get_attribute_value_length - return the length of the value of an attribute
 * @a:	pointer to a buffer containing the attribute record
 *
 * Return the byte size of the attribute value of the attribute @a (as it
 * would be after eventual decompression and filling in of holes if sparse).
 * If we return 0, check errno. If errno is 0 the actual length was 0,
 * otherwise errno describes the error.
 *
 * FIXME: Describe possible errnos.
 */
extern s64 ntfs_get_attribute_value_length(const ATTR_RECORD *a);

/**
 * get_attribute_value - return the attribute value of an attribute
 * @vol:	volume on which the attribute is present
 * @a:		attribute to get the value of
 * @b:		destination buffer for the attribute value
 *
 * Make a copy of the attribute value of the attribute @a into the destination
 * buffer @b. Note, that the size of @b has to be at least equal to the value
 * returned by get_attribute_value_length(@a).
 *
 * Return number of bytes copied. If this is zero check errno. If errno is 0
 * then nothing was read due to a zero-length attribute value, otherwise
 * errno describes the error.
 */
extern s64 ntfs_get_attribute_value(const ntfs_volume *vol, 
				    const ATTR_RECORD *a, u8 *b);

extern int   ntfs_attr_exist(ntfs_inode *ni, const ATTR_TYPES type,
			     ntfschar *name, u32 name_len);

#endif /* defined _NTFS_ATTRIB_H */

