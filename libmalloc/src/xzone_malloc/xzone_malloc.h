/* ----------------------------------------------------------------------------
Copyright (c) 2018-2022, Microsoft Research, Daan Leijen
Copyright © 2025 Apple Inc.
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" in the same directory as this file.
-----------------------------------------------------------------------------*/

#if CONFIG_XZONE_MALLOC

#ifndef __XZONE_MALLOC_H__
#define __XZONE_MALLOC_H__

#include <ptrcheck.h>
__ptrcheck_abi_assume_single()

#pragma mark Data structure types

#define XZM_SEGMENT_SLICE_SHIFT			14
#define XZM_SEGMENT_SHIFT				22
#define XZM_TINY_CHUNK_SHIFT			14
#define XZM_SMALL_CHUNK_SHIFT			16
#define XZM_SMALL_FREELIST_CHUNK_SHIFT	17

#define XZM_SEGMENT_SLICE_SIZE          (1ull << XZM_SEGMENT_SLICE_SHIFT) // 16KiB
#define XZM_SEGMENT_SLICE_MASK          (XZM_SEGMENT_SLICE_SIZE - 1)
#define XZM_SEGMENT_SIZE                (1ull << XZM_SEGMENT_SHIFT) // 4MiB
#define XZM_SEGMENT_MASK                (XZM_SEGMENT_SIZE - 1)

#define XZM_TINY_CHUNK_SIZE				(1ull << XZM_TINY_CHUNK_SHIFT) // 16KiB
#define XZM_SMALL_CHUNK_SIZE			(1ull << XZM_SMALL_CHUNK_SHIFT) // 64KiB
#define XZM_SMALL_FREELIST_CHUNK_SIZE	(1ull << XZM_SMALL_FREELIST_CHUNK_SHIFT) // 128KiB

#define XZM_SLICES_PER_SEGMENT			(XZM_SEGMENT_SIZE / XZM_SEGMENT_SLICE_SIZE) // 256

#define XZM_TINY_BLOCK_SIZE_MAX			(XZM_TINY_CHUNK_SIZE / 4) // 4KiB
#define XZM_SMALL_BLOCK_SIZE_MAX		(XZM_SMALL_CHUNK_SIZE / 2) // 32KiB
#define XZM_LARGE_BLOCK_SIZE_MAX		(XZM_SEGMENT_SIZE / 2) // 2MiB

// Note: MACH_VM_MAX_ADDRESS is incorrect in the simulator
#if   MALLOC_TARGET_IOS
#define XZM_LIMIT_ADDRESS				(1ull << 36)
#else
#define XZM_LIMIT_ADDRESS				(1ull << 47)
#endif

#if MALLOC_TARGET_IOS || MALLOC_TARGET_EXCLAVES
#define CONFIG_EXTERNAL_METADATA_LARGE 0
#else
#define CONFIG_EXTERNAL_METADATA_LARGE 1
#endif // MALLOC_TARGET_IOS || MALLOC_TARGET_EXCLAVES

#if CONFIG_EXTERNAL_METADATA_LARGE
// A segment table covers 64GB of VA. The "extended" segment table contains
// references to other segment tables, to allow us to cover the entire 128TB
// virtual address space
#define XZM_SEGMENT_TABLE_COVERAGE			GiB(64)
#define XZM_SEGMENT_TABLE_ENTRIES			\
		(XZM_SEGMENT_TABLE_COVERAGE / XZM_SEGMENT_SIZE)
#else
#define XZM_SEGMENT_TABLE_ENTRIES			\
		(XZM_LIMIT_ADDRESS / XZM_SEGMENT_SIZE)
#endif // CONFIG_EXTERNAL_METADATA_LARGE
#define XZM_SEGMENT_TABLE_SIZE				\
		(XZM_SEGMENT_TABLE_ENTRIES * sizeof(xzm_segment_table_entry_s))
#define XZM_EXTENDED_SEGMENT_TABLE_ENTRIES	\
		(XZM_LIMIT_ADDRESS / XZM_SEGMENT_TABLE_COVERAGE)
#define XZM_SEGMENT_TABLE_ALIGN				(XZM_SEGMENT_TABLE_SIZE)

#define XZM_GRANULE						16
#define XZM_SMALL_GRANULE				1024
#define XZM_CHUNK_MAX_BLOCK_COUNT		(XZM_TINY_CHUNK_SIZE / XZM_GRANULE)
#define XZM_ZERO_ON_FREE_THRESHOLD		1024
#define XZM_THREAD_CACHE_THRESHOLD		256
#define XZM_THREAD_CACHE_BINS			12

// This macro defines the largest alignment that will be served via an offset in
// a normal segment. All alignment requests larger than this will be served as a
// huge chunk by the VM. Set to quarter of the segment size (1M), since large
// blocks are also allowed to be half segment (the max we can request from the
// span queue is 3/4 segment size)
#define XZM_ALIGNMENT_MAX	(XZM_SEGMENT_SIZE / 4)
_Static_assert(XZM_ALIGNMENT_MAX + XZM_LARGE_BLOCK_SIZE_MAX < XZM_SEGMENT_SIZE,
		"Large block + alignment must fit into a segment");

// How many slices in a multi-slice chunk will have backpointers in
// xzsl_slice_offset_bytes
#define XZM_MAX_SLICE_OFFSET (XZM_SMALL_FREELIST_CHUNK_SIZE / XZM_SEGMENT_SLICE_SIZE - 1)

#define XZM_SPAN_QUEUE_COUNT 27

#if CONFIG_XZM_DEFERRED_RECLAIM

#if MALLOC_TARGET_IOS
#define XZM_HUGE_CACHE_SIZE_ENABLED 16
#define XZM_HUGE_CACHE_SIZE_DEFAULT 0
#define XZM_DEFERRED_RECLAIM_ENABLED_DEFAULT false
#else // MALLOC_TARGET_IOS
#define XZM_HUGE_CACHE_SIZE_ENABLED 64
#define XZM_HUGE_CACHE_SIZE_DEFAULT XZM_HUGE_CACHE_SIZE_ENABLED
#define XZM_DEFERRED_RECLAIM_ENABLED_DEFAULT true
#endif // MALLOC_TARGET_IOS

#if TARGET_OS_VISION
// Compatibility with medium
#define XZM_HUGE_CACHE_MAX_ENTRY_BYTES_DEFAULT MiB(8)
#else
#define XZM_HUGE_CACHE_MAX_ENTRY_BYTES_DEFAULT UINT32_MAX
#endif

#define XZM_RECLAIM_BUFFER_COUNT_DEFAULT 512
#define XZM_RECLAIM_BUFFER_MAX_COUNT_DEFAULT (1ul << 22)

#define XZM_RECLAIM_ID_COUNT 1024

typedef struct xzm_reclaim_id_cache_s {
	size_t ric_head;
	size_t ric_len;
	mach_vm_reclaim_id_t *ric_ids __counted_by(ric_len);
} *xzm_reclaim_id_cache_t;

// TODO: allocate dynamically from metapool (rdar://122824838)
typedef struct xzm_reclaim_buffer_s {
	mach_vm_reclaim_ring_t xrb_ringbuffer;
	mach_vm_reclaim_count_t xrb_len;
	_malloc_lock_s xrb_lock;
	struct xzm_reclaim_id_cache_s xrb_id_cache;
} *xzm_reclaim_buffer_t;

#else

typedef void *xzm_reclaim_buffer_t;

#endif // CONFIG_XZM_DEFERRED_RECLAIM

#define CONFIG_TINY_ALLOCATION_SLOT_LOCK MALLOC_TARGET_EXCLAVES

#define CONFIG_XZM_THREAD_CACHE 1

// offset (bytes) of block in chunk
typedef uint32_t xzm_block_offset_t;

typedef uint32_t xzm_block_index_t;

#define XZM_SEQNO_THREAD_LOCAL (1 << 12)
#define XZM_SEQNO_COUNTER_MASK (XZM_SEQNO_THREAD_LOCAL - 1)

union xzm_block_linkage_u {
	struct {
		uint64_t		xzbl_next_offset : 11;
		uint64_t		xzbl_next_seqno : 13;
		uint64_t		xzbl_seqno : 13;
		uint64_t		xzbl_next_sig : 27;
	};
	void *				xzbl_next;
	uintptr_t			xzbl_next_value;
};

// mimalloc: mi_block_t
typedef struct xzm_block_inline_meta_s {
	uint64_t					xzb_cookie;
	union xzm_block_linkage_u	xzb_linkage;
} * __single xzm_block_t;

typedef struct xzm_xzone_s * __single xzm_xzone_t;

// The (1-based) index in an xzone table for a particular (bin, type bucket)
typedef uint8_t xzm_xzone_index_t;

// The (1-based) index in the xzm malloc zone table for an mzone
typedef uint16_t xzm_mzone_index_t;

typedef struct xzm_reused_mzone_index_s {
	xzm_mzone_index_t xrmi_mzone_idx;
	SLIST_ENTRY(xzm_reused_mzone_index_s) xrmi_mzone_entry;
} *xzm_reused_mzone_index_t;

// Returned 0-based from _xzm_get_allocation_index(), but stored as 1-based to
// allow 0 to be the representation for XZM_SLOT_INDEX_EMPTY
typedef uint8_t xzm_allocation_index_t;

// An index in a zone's segment group table
typedef uint8_t xzm_segment_group_index_t;

// In general, we can store slice counts that take up to 32 bits to represent.
typedef uint32_t xzm_slice_count_t;

#define XZM_XZONE_INDEX_INVALID 0
#define XZM_XZONE_INDEX_FIRST 1

#define XZM_MZONE_INDEX_INVALID 0
#define XZM_MZONE_INDEX_MAIN 1
#define XZM_MZONE_INDEX_MAX UINT16_MAX

#define XZM_SLOT_INDEX_EMPTY 0
#define XZM_SLOT_INDEX_THREAD 62
#define XZM_SLOT_INDEX_THREAD_INSTALLED 63

// mimalloc: mi_page_kind_t
OS_ENUM(xzm_slice_kind, uint8_t,
	XZM_SLICE_KIND_INVALID,
	XZM_SLICE_KIND_SINGLE_FREE,
	XZM_SLICE_KIND_TINY_CHUNK,
	XZM_SLICE_KIND_MULTI_FREE,
	XZM_SLICE_KIND_MULTI_BODY,
	XZM_SLICE_KIND_SMALL_CHUNK,
	XZM_SLICE_KIND_SMALL_FREELIST_CHUNK,
	XZM_SLICE_KIND_LARGE_CHUNK,
	XZM_SLICE_KIND_HUGE_CHUNK,
	XZM_SLICE_KIND_GUARD,
);

typedef union xzm_chunk_bits_u {
	struct {
		xzm_slice_kind_t		xzcb_kind : 4;
		uint8_t					xzcb_is_pristine : 1;
		uint8_t					xzcb_on_partial_list : 1;
		uint8_t					xzcb_preallocated : 1;
		uint8_t					xzcb_unused : 1;
	};
	uint8_t						xzcb_value;
} xzm_chunk_bits_t;

// <= XZM_FREE_LIMIT -> valid freelist head
#define XZM_FREE_LIMIT			0x400
#define XZM_FREE_NULL			0x400

#define XZM_FREE_MADVISING		0x7ff
#define XZM_FREE_MADVISED		0x7fe

typedef union xzm_chunk_atomic_meta_u {
	struct {
		// Block offsets are encoded in 16-byte (XZM_GRANULE) increments
		uint64_t				xca_alloc_head : 11;
		uint64_t				xca_free_count : 11;
		uint64_t				xca_alloc_idx : 6;
		uint64_t				xca_on_partial_list : 1;
		uint64_t				xca_on_empty_list : 1;
		uint64_t				xca_walk_locked : 1;
		uint64_t				xca_head_seqno : 13;
		uint64_t				xca_seqno : 20;
	};
	struct {
		uint64_t				xca_value_lo;
	};
	uint64_t	 				xca_value;
} xzm_chunk_atomic_meta_u;

typedef enum {
	XZM_CHUNK_LINKAGE_MAIN,
	XZM_CHUNK_LINKAGE_ALL,
	XZM_CHUNK_LINKAGE_COUNT,
	// The batch linkage is a special out-of-line linkage that is stored in the
	// xzsm_batch_next field of xzm_xzone_slice_metadata_u, so it doesn't count
	XZM_CHUNK_LINKAGE_BATCH,
} xzm_chunk_linkage_t;

// mimalloc: mi_page_t
// zalloc: struct zone_page_metadata
typedef struct xzm_slice_s {
	union {
		// Valid in SMALL chunks
		struct {
			xzm_block_offset_t				xzc_free;
			uint32_t						xzc_used;
			_malloc_lock_s					xzc_lock;
			xzm_allocation_index_t			xzc_alloc_idx;
		};
		// Valid in TINY and SMALL_FREELIST chunks
		struct {
			xzm_chunk_atomic_meta_u				xzc_atomic_meta;
			uint16_t							xzc_freelist_block_size;
			uint16_t							xzc_freelist_chunk_capacity;
#if CONFIG_MTE
			bool								xzc_tagged;
#endif
		};
	};

	// xzc_linkages is used in tiny chunks for the lock-free chunk lists.
	// xzc_slist_entry is used when accumulating preallocated chunks and busy
	// empty tiny chunks.  In all other cases, xzc_entry is used.
	union {
		LIST_ENTRY(xzm_slice_s)		xzc_entry;
		SLIST_ENTRY(xzm_slice_s)	xzc_slist_entry;
		struct xzm_slice_s *		xzc_linkages[XZM_CHUNK_LINKAGE_COUNT];
	};

	// Note: access to this bitfield is a little tricky!  Some of the bits are
	// modified from under the range group lock and others under the xzone, but
	// none of these modifications may happen concurrently.
	//
	// xzcb_kind is the fundamental "type" of the slice, and should be in the
	// same position in all possible metadata views
	xzm_chunk_bits_t				xzc_bits;

	xzm_xzone_index_t				xzc_xzone_idx;
	xzm_mzone_index_t				xzc_mzone_idx;

	// Only valid in body slices of multi-slice slab chunks.  Will be unioned
	// with the head-only state like the free list eventually (will need to
	// make sure it's at the right offset in secondary metadata as well)
	uint32_t						xzsl_slice_offset_bytes;

	// Secondary metadata, not valid in tiny chunks (eventually won't be present
	// in them at all, will appear in the adjacent slots for multi-slice
	// chunks).  Managed by the segment code.
	xzm_slice_count_t				xzcs_slice_count;
} * __single xzm_slice_t;

_Static_assert(sizeof(((xzm_slice_t)NULL)->xzc_slist_entry) ==
		sizeof(((xzm_slice_t)NULL)->xzc_linkages[0]),
		"slist entry must match size of tiny main linkage");
_Static_assert(sizeof(struct xzm_slice_s) <= 48, "Slice metadata too large");

// A chunk is a contiguous span of slices that are allocated, either as a slab
// for smaller allocations or a block for a single larger allocation.
typedef struct xzm_slice_s *xzm_chunk_t;

// A free span is a contiguous set of slices that are free in a normal segment.
typedef struct xzm_slice_s *xzm_free_span_t;

typedef struct xzm_main_malloc_zone_s *xzm_main_malloc_zone_t;

OS_ENUM(xzm_range_group_id, uint8_t,
	XZM_RANGE_GROUP_DATA,
	XZM_RANGE_GROUP_PTR_LARGE, // for exclaves only
	XZM_RANGE_GROUP_PTR,
	XZM_RANGE_GROUP_COUNT,
);

typedef uint8_t xzm_range_group_index_t;

typedef uint8_t xzm_front_index_t;

#define XZM_FRONT_INDEX_DEFAULT 0

OS_ENUM(xzm_front_direction, uint8_t,
	XZM_FRONT_INCREASING, // default with 0-init
	XZM_FRONT_DECREASING,
);

typedef struct xzm_range_group_s {
	xzm_range_group_id_t			xzrg_id;
	xzm_front_index_t				xzrg_front;
	xzm_main_malloc_zone_t			xzrg_main_ref;

	// Used by exclaves and XZM_RANGE_GROUP_PTR on Darwin
	_malloc_lock_s					xzrg_lock;
	mach_vm_address_t				xzrg_base;
	size_t							xzrg_size;
	mach_vm_address_t				xzrg_skip_addr;
	size_t							xzrg_skip_size;
	mach_vm_address_t				xzrg_next;
	size_t							xzrg_remaining;
	xzm_front_direction_t			xzrg_direction;
	// Used to print warning when bump no longer possible
	bool							xzrg_warned_full;
} *xzm_range_group_t;

// mimalloc: mi_span_queue_t
typedef struct xzm_span_queue_s {
	LIST_HEAD(, xzm_slice_s)	xzsq_queue;
	xzm_slice_count_t			xzsq_slice_count;
} *xzm_span_queue_t;

typedef struct xzm_segment_cache_s {
	TAILQ_HEAD(xzm_segment_cache_head_s, xzm_segment_s) xzsc_head;
    uint16_t 			xzsc_max_count; // max number of entries in cache
	uint16_t 			xzsc_count; // number of entries currently in cache
    xzm_slice_count_t	xzsc_max_entry_slices; // maximum size of a single entry
	_malloc_lock_s		xzsc_lock;
} *xzm_segment_cache_t;

typedef struct xzm_segment_group_s *xzm_segment_group_t;

OS_ENUM(xzm_segment_group_id, uint8_t,
	XZM_SEGMENT_GROUP_DATA,
	XZM_SEGMENT_GROUP_DATA_LARGE, // only used under deferred reclamation
	XZM_SEGMENT_GROUP_POINTER_LARGE,
	// Special count value used while running under MallocXzoneDataOnly=1
	XZM_SEGMENT_GROUP_IDS_COUNT_DATA_ONLY = XZM_SEGMENT_GROUP_POINTER_LARGE,
	XZM_SEGMENT_GROUP_POINTER_XZONES,
	XZM_SEGMENT_GROUP_IDS_COUNT,
);

// mimalloc (roughly) mi_segment_tld_s
struct xzm_segment_group_s {
	xzm_segment_group_id_t		xzsg_id;
	xzm_front_index_t			xzsg_front;
	_malloc_lock_s				xzsg_lock;
	_malloc_lock_s				xzsg_alloc_lock;
	xzm_range_group_t			xzsg_range_group;
	xzm_main_malloc_zone_t		xzsg_main_ref;
	struct xzm_span_queue_s		xzsg_spans[XZM_SPAN_QUEUE_COUNT];
	struct xzm_segment_cache_s	xzsg_cache;
};

#define XZM_BATCH_SIZE_BITS 6

typedef union xzm_chunk_list_head_u {
	union {
		// Non-batch lists for tiny
		struct {
			uint64_t					xzch_ptr : 47;
			// Generation counter used to avoid ABA problem
			uint64_t					xzch_gen : 16;
			uint64_t					xzch_fork_locked : 1;
		};
		// Batch list for tiny
		struct {
			uint64_t					xzch_batch_ptr : 47;
			// Reduced-width generation counter for batch list
			uint64_t					xzch_batch_gen : 10;
			// Chunk counter for number of elements on batch list
			uint64_t					xzch_batch_count : XZM_BATCH_SIZE_BITS;
			uint64_t					xzch_batch_fork_locked : 1;
		};
	};
	uint64_t						xzch_value;
} xzm_chunk_list_head_u, *xzm_chunk_list_head_t;
_Static_assert(sizeof(xzm_chunk_list_head_u) == sizeof(uint64_t),
		"Chunk list head size is not 64 bits!");

typedef struct xzm_isolation_zone_s {
	LIST_HEAD(, xzm_slice_s)	xziz_chunkq;
	_malloc_lock_s				xziz_lock;
} * __single xzm_isolation_zone_t;

// TODO:
// - Dynamic bucket counts
// - Jagged bucket counts
#if TARGET_OS_OSX || MALLOC_TARGET_DK_OSX || \
		TARGET_OS_VISION || MALLOC_TARGET_DK_VISIONOS
#define XZM_XZONE_DEFAULT_POINTER_BUCKET_COUNT 4
#elif TARGET_OS_WATCH || MALLOC_TARGET_DK_WATCH
#define XZM_XZONE_DEFAULT_POINTER_BUCKET_COUNT 2
#else
#define XZM_XZONE_DEFAULT_POINTER_BUCKET_COUNT 3
#endif

#if TARGET_OS_WATCH || MALLOC_TARGET_DK_WATCH
#define XZM_NARROW_BUCKETING 1
#else
#define XZM_NARROW_BUCKETING 0
#endif

// Set this to put uninferred type descriptors and plain malloc calls in
// dedicated buckets
#define XZM_BUCKET_VISIBILITY 0

OS_ENUM(xzm_xzone_bucket, uint8_t,
	XZM_XZONE_BUCKET_DATA,
	XZM_XZONE_BUCKET_OBJC,
#if XZM_BUCKET_VISIBILITY
	XZM_XZONE_BUCKET_UNINFERRED,
	XZM_XZONE_BUCKET_PLAIN,
#endif // XZM_BUCKET_VISIBILITY
	XZM_XZONE_BUCKET_POINTER_BASE,
	XZM_XZONE_DEFAULT_BUCKET_COUNT = (XZM_XZONE_BUCKET_POINTER_BASE +
			XZM_XZONE_DEFAULT_POINTER_BUCKET_COUNT),
);

// TODO: need more than 8 bits for xzone IDs to support a wider configuration
// for all 40 current size classes
#define XZM_POINTER_BUCKETS_MAX (6 - XZM_XZONE_BUCKET_POINTER_BASE)

// Set this to put all allocations in data or pointer buckets, respectively, for
// performance testing
#define XZM_BUCKET_DATA_ONLY 0
#define XZM_BUCKET_POINTER_ONLY 0

#if XZM_BUCKET_DATA_ONLY || XZM_BUCKET_POINTER_ONLY
#define XZM_XZONE_DEFAULT_BUCKET_COUNT 1
#endif

OS_ENUM(xzm_slot_config, uint8_t,
	XZM_SLOT_SINGLE,
	XZM_SLOT_CLUSTER,
	XZM_SLOT_CPU,
	XZM_SLOT_LAST,
);

// The gencount is needed to avoid leaking chunks in the following scenario:
// - Thread A tries to allocate from Chunk A, fails, marks it uninstalled,
//   and then is pre-empted
// - Thread B also tries and fails to allocate, takes ownership of the
//   allocation slot and installs Chunk B
// - Eventually, Chunk A becomes unfull and is re-installed to the slot by
//   somebody
// - Thread A is finally scheduled and tries to take ownership of the slot
//
// The gencount allows Thread A to realize that Chunk A is no longer marked
// uninstalled and that it should not proceed with trying to become owner of the
// slot.
typedef union xzm_allocation_slot_atomic_meta_u {
	struct xzm_slot_gate_s {
		uint64_t					xsg_locked : 1;
		uint64_t					xsg_waiters : 1;
		uint64_t					xsg_owner : 30;
		uint64_t					xsg_unused : 17;
		uint64_t					xsg_gen : 15;
	} xasa_gate;
	struct xzm_slot_chunk_s {
		uint64_t					xsc_locked : 1;
		uint64_t					xsc_fork_locked : 1;
		uint64_t					xsc_ptr : 47;
		uint64_t					xsc_gen : 15;
	} xasa_chunk;
	uint32_t						xasa_ulock;
	uint64_t						xasa_value;
} xzm_allocation_slot_atomic_meta_u;

typedef union xzm_xzone_slot_counters_u {
	union {
		struct {
			uint32_t                        xsc_ops;
			uint32_t                        xsc_contentions : 24;
			uint32_t                        xsc_slot_config : 8;
		};
		uint64_t                            xsc_value;
	};
} xzm_xzone_slot_counters_u;

typedef struct xzm_xzone_allocation_slot_s {
	union {
		xzm_chunk_t							xas_chunk;
		xzm_allocation_slot_atomic_meta_u	xas_atomic;
	};
	_malloc_lock_s							xas_lock;
	union {
		struct {
			uint32_t                                xas_allocs;
			uint32_t                                xas_contentions;
		};
		xzm_xzone_slot_counters_u           xas_counters;
	};
	uint64_t                                xas_last_chunk_empty_ts;
} *xzm_xzone_allocation_slot_t;

typedef struct xzm_xzone_guard_config_s {
	uint8_t xxgc_max_run_length;
	uint8_t xxgc_density;
} *xzm_xzone_guard_config_t;

// Must not be a valid value of xztc_head
#define XZM_XZONE_NOT_CACHED   UINT16_MAX
#define XZM_XZONE_CACHE_EMPTY  (UINT16_MAX - 1)

typedef union xzm_xzone_thread_cache_atomic_meta_u {
	struct {
		uint16_t                            xztcam_head;
		uint16_t                            xztcam_free_count;
	};
	uint32_t                                xztcam_value;
} xzm_xzone_thread_cache_atomic_meta_u;

typedef union xzm_xzone_thread_cache_u {
	struct {
		xzm_chunk_t                         xztc_chunk;
		uint8_t *                           xztc_chunk_start;
		union {
			struct {
				uint16_t                    xztc_head;
				uint16_t                    xztc_free_count;
				uint16_t                    xztc_head_seqno;
				uint16_t                    xztc_seqno;
			};
			xzm_xzone_thread_cache_atomic_meta_u xztc_atomic_meta;
			uint64_t                        xztc_freelist_state;
		};
	};
	struct {
#if CONFIG_MTE
		uint64_t                            xztc_chunk_pad : 63;
		uint64_t                            xztc_tagged : 1;
#else
		uint64_t                            xztc_chunk_pad;
#endif
		uint64_t                            xztc_timestamp;
		uint16_t                            xztc_state;
		uint16_t                            xztc_contentions;
		uint32_t                            xztc_allocs;
	};
} xzm_xzone_thread_cache_u;

typedef xzm_xzone_thread_cache_u *xzm_xzone_thread_cache_t;

#if CONFIG_XZM_THREAD_CACHE
typedef pthread_t xzm_thread_type_t;
#else
typedef void *xzm_thread_type_t;
#endif

typedef struct xzm_thread_cache_s {
	LIST_ENTRY(xzm_thread_cache_s)          xtc_linkage;
	xzm_main_malloc_zone_t					xtc_main;
	xzm_thread_type_t						xtc_thread;
	uint64_t								xtc_teardown_gen;
	xzm_xzone_thread_cache_u                xtc_xz_caches[];
} *xzm_thread_cache_t;

// mimalloc: (roughly) mi_heap_t
// zalloc: zone_t
//
// This is the "xnu-style zone", the namesake abstraction of the allocator
struct xzm_xzone_s {
	// rw state
	// TODO: consider cacheline aligning
	union {
		// Used for small chunks
		struct {
			LIST_HEAD(, xzm_slice_s)		xz_chunkq_partial;
			LIST_HEAD(, xzm_slice_s)		xz_chunkq_full;
			LIST_HEAD(, xzm_slice_s)		xz_chunkq_preallocated;
			// Singly-linked list of empty chunks that are enqueued for empty list
			xzm_chunk_t						xz_chunkq_batch;
			// Chunk counter for number of elements on batch list
			uint32_t						xz_chunkq_batch_count;
			_malloc_lock_s					xz_lock;
		};
		// Used for tiny chunks
		struct {
			// See xzz_partial_lists below for tiny partial lists
			// Singly-linked list of empty chunks that are enqueued for empty list
			xzm_chunk_list_head_u			xz_batch_list;
			// Singly-linked list of empty chunks that have been madvised
			xzm_chunk_list_head_u			xz_empty_list;
			// Singly-linked list of all chunks, allocated or not
			xzm_chunk_list_head_u			xz_all_list;
			// Singly-linked list of preallocated chunks used as guards
			xzm_chunk_list_head_u			xz_preallocated_list;
		};
	};

	// Read-mostly state (not protected by xz_lock)
	uint16_t						xz_early_budget;
	xzm_segment_group_id_t			xz_segment_group_id;
	xzm_front_index_t				xz_front;
	uint64_t						xz_block_size; // TODO: pack?
	uint64_t						xz_quo_magic;
	uint32_t						xz_align_magic;
	uint32_t						xz_chunk_capacity;
	// counter for initial slot config, not used after chunk threshold is met
	uint64_t						xz_chunk_count;
	xzm_xzone_index_t				xz_idx;
	xzm_mzone_index_t				xz_mzone_idx;
	xzm_xzone_bucket_t				xz_bucket;
	xzm_slot_config_t				xz_list_config;
	xzm_slot_config_t               xz_slot_config;
	bool							xz_sequestered : 1;
#if CONFIG_MTE
	bool							xz_tagged : 1;
#endif
	struct xzm_xzone_guard_config_s	xz_guard_config;
};

// mimalloc: mi_segment_kind_t
OS_ENUM(xzm_segment_kind, uint8_t,
	XZM_SEGMENT_KIND_NORMAL,
	XZM_SEGMENT_KIND_HUGE,
);

typedef struct xzm_chunk_list_s {
	// head of the singly-linked chunk list
	xzm_chunk_list_head_u		xcl_list;
	// counter for operations/contentions on this list
	xzm_xzone_slot_counters_u	xcl_counters;
} *xzm_chunk_list_t;

typedef union xzm_xzone_slice_metadata_u {
#if CONFIG_XZM_DEFERRED_RECLAIM
	// Reclaim ID for slices under deferred reclaim
	mach_vm_reclaim_id_t xzsm_reclaim_id;
#endif // CONFIG_XZM_DEFERRED_RECLAIM
	// Pointer to next linked list entry for slices on the batch list
	xzm_chunk_t xzsm_batch_next;
} xzm_xzone_slice_metadata_u;

// mimalloc: mi_segment_t
typedef struct xzm_segment_s {
	xzm_segment_group_t		xzs_segment_group;
	uint32_t				xzs_used;
	xzm_slice_count_t		xzs_slice_count;
	xzm_slice_count_t		xzs_slice_entry_count;
	xzm_segment_kind_t		xzs_kind;
	// linkage in xzm_segment_cache (only valid for HUGE segments)
	TAILQ_ENTRY(xzm_segment_s)	xzs_cache_entry;
	void *					xzs_segment_body;
	// index in vm_reclaim buffer (only valid for HUGE segments)
	mach_vm_reclaim_id_t	xzs_reclaim_id;
	// TODO: fold reclaim indices into per-slice metadata
	xzm_xzone_slice_metadata_u	xzs_slice_metadata[XZM_SLICES_PER_SEGMENT];
	struct xzm_slice_s		xzs_slices[XZM_SLICES_PER_SEGMENT];
} * __single xzm_segment_t;

typedef struct xzm_segment_table_entry_s {
	// This struct encodes a reference to an XZM_METAPOOL_SEGMENT_ALIGN aligned
	// segment metadata structure.
	// TODO: Static asserts or other proof of how many bits are needed
	// TODO: This requires that segment metadata allocations on MacOS fall in
	// the bottom part of the address space, which we can't always guarantee
	uint32_t xste_val : 31;
	uint32_t xste_normal : 1;
} xzm_segment_table_entry_s;

#define XZM_SEGMENT_TABLE_LIMIT_ENTRY (1ull << 31)

typedef struct xzm_segment_table_entry_s *xzm_segment_table_entry_t;

typedef struct xzm_extended_segment_table_entry_s {
	// This structure encodes the address of a 64kB aligned table of 64kB of
	// xzm_segment_table_entry_s's. This is only used on MacOS, where we
	// need 47 - 16 = 31 bits to encode the address
	uint32_t xeste_val;
} xzm_extended_segment_table_entry_s;

OS_ENUM(xzm_metapool_id, uint8_t,
	XZM_METAPOOL_SEGMENT,
	XZM_METAPOOL_SEGMENT_TABLE,
	XZM_METAPOOL_MZONE_IDX,
	XZM_METAPOOL_THREAD_CACHE,
	// NOTE: The metadata metapool needs to be the last metapool locked in
	// _xzm_foreach_lock, since other metapools call into it while locked
	XZM_METAPOOL_METADATA,
	XZM_METAPOOL_COUNT,
);

// Slab sizes chosen arbitrarily
#define XZM_METAPOOL_SEGMENT_SLAB_SIZE		((uint32_t)KiB(512))
#define XZM_METAPOOL_SEGMENT_BLOCK_SHIFT	(64 - __builtin_clzl(sizeof(struct xzm_segment_s)-1))
#define XZM_METAPOOL_SEGMENT_BLOCK_SIZE		(1ull << XZM_METAPOOL_SEGMENT_BLOCK_SHIFT)
#define XZM_METAPOOL_SEGMENT_ALIGN			XZM_METAPOOL_SEGMENT_BLOCK_SIZE

#define XZM_METAPOOL_SEGMENT_TABLE_SLAB_SIZE	((uint32_t)KiB(256))
#define XZM_METAPOOL_SEGMENT_TABLE_BLOCK_SIZE	XZM_SEGMENT_TABLE_SIZE
#define XZM_METAPOOL_SEGMENT_TABLE_ALIGN		XZM_SEGMENT_TABLE_SIZE

#define XZM_METAPOOL_MZIDX_SLAB_SIZE	((uint32_t)KiB(16))
#define XZM_METAPOOL_MZIDX_BLOCK_SHIFT	\
		(64 - __builtin_clzl(sizeof(struct xzm_reused_mzone_index_s)-1))
// On Exclaves, metapool slabs are larger than a reused mzone index to hold the
// map, which causes a debug crash
#define XZM_METAPOOL_MZIDX_BLOCK_SIZE	\
		MAX((1ull << XZM_METAPOOL_MZIDX_BLOCK_SHIFT), \
		sizeof(struct xzm_metapool_slab_s))
#define XZM_METAPOOL_MZIDX_BLOCK_ALIGN	XZM_METAPOOL_MZIDX_BLOCK_SIZE

#define XZM_METAPOOL_THREAD_CACHE_SLAB_SIZE		((uint32_t)KiB(32))

typedef struct xzm_metapool_slab_s {
	SLIST_ENTRY(xzm_metapool_slab_s)	xzmps_entry;
	uint8_t *							xzmps_base;
} *xzm_metapool_slab_t;

typedef struct xzm_metapool_block_s {
	SLIST_ENTRY(xzm_metapool_block_s)	xzmpb_entry;
	uint8_t *							xzmpb_base;
} *xzm_metapool_block_t;

typedef struct xzm_metapool_s {
	_malloc_lock_s						xzmp_lock;
	xzm_metapool_id_t					xzmp_id;
	uint8_t								xzmp_vm_tag;
	uint32_t							xzmp_slab_size;
	uint32_t							xzmp_slab_limit;
	uint32_t							xzmp_block_align;
	uint32_t							xzmp_block_size;
	SLIST_HEAD(, xzm_metapool_slab_s)	xzmp_slabs;
	SLIST_HEAD(, xzm_metapool_block_s)	xzmp_blocks;
	xzm_metapool_slab_t					xzmp_current_slab;
	uint32_t							xzmp_current_block;
	struct xzm_metapool_s *				xzmp_metadata_metapool;
} *xzm_metapool_t;

#if CONFIG_MTE

struct xzm_memtag_config_s {
	// enable tagging support
	bool 		enabled;
	// tag data chunks
	bool 		tag_data;
	// maximum block size to tag
	uint64_t	max_block_size;
};

#else

struct xzm_padding_s {
	bool reserved1;
	bool reserved2;
	uint64_t reserved3;
};

#endif

typedef struct xzm_malloc_zone_s {
	malloc_zone_t				xzz_basic_zone;
	// NB: not padded out to a page boundary as done traditionally; saves a page
	// now that we have PAC

	// Only meaningful for non-main mzones
	uint64_t					xzz_total_size;
	xzm_mzone_index_t			xzz_mzone_idx;
	// Denormalized here; must be the same for all xzones
	uint8_t						xzz_xzone_count;
	uint8_t						xzz_slot_count;
	uint8_t						xzz_thread_cache_xzone_count;
	// pointer to tail xzone table(s)
	xzm_xzone_t __counted_by(xzz_xzone_count)		xzz_xzones;
	// pointer to tail allocation slot tables (2D: slot domain * xzones)
	xzm_xzone_allocation_slot_t __counted_by(xzz_slot_count * xzz_xzone_count)
			xzz_xzone_allocation_slots;
	// upgradable per-slot list of partially allocated tiny chunks
	xzm_chunk_list_t			xzz_partial_lists;
	// reference to the main mzone (NULL for the main mzone)
	xzm_main_malloc_zone_t		xzz_main_ref;
	// maximum slot config for chunk lists, clamped to xzz_max_slot_config
	xzm_slot_config_t			xzz_max_list_config : 2;
	// initial xzone slot config, after chunk threshold is reached
	xzm_slot_config_t			xzz_initial_slot_config : 2;
	// each xzone's slot config may be upgraded (up to a maximum config) if the
	// number of contentions on their slots exceeds the upgrade threshold
	xzm_slot_config_t           xzz_max_slot_config : 2;
	bool                        xzz_thread_cache_enabled;
	bool                        xzz_small_freelist_enabled;
	uint32_t					xzz_thread_cache_xzone_activation_period;
	uint32_t					xzz_thread_cache_xzone_activation_contentions;
	uint64_t					xzz_thread_cache_xzone_activation_time;
	uint32_t					xzz_list_upgrade_threshold[XZM_SLOT_LAST];
	uint32_t                    xzz_list_upgrade_period;
	// block size threshold for xzone initial slot config, after chunk
	// threshold is reached
	uint32_t					xzz_slot_initial_threshold;
	uint32_t                    xzz_slot_upgrade_threshold[XZM_SLOT_LAST];
	// reset contention counters at this period
	uint32_t                    xzz_slot_upgrade_period;
	// maximum period between last partial frees at the installed chunk will be
	// cached
	uint64_t                    xzz_tiny_thrash_threshold;
	uint64_t                    xzz_freelist_cookie;
	uint64_t                    xzz_small_thrash_threshold;
	uint64_t                    xzz_small_thrash_limit_size;

	_malloc_lock_s				xzz_lock;
	_malloc_lock_s				xzz_fork_lock;
	LIST_HEAD(, xzm_slice_s)	xzz_chunkq_large;

	// Zone debug flags
	uint64_t 					xzz_flags;

	// Ensure this struct is constant size, otherwise tests may read from the
	// wrong address for fields after this struct in xzm_main_malloc_zone_s
#if CONFIG_MTE
	struct xzm_memtag_config_s	xzz_memtag_config;
#else
	struct xzm_padding_s		xzz_padding;
#endif

} * __single xzm_malloc_zone_t;

typedef struct xzm_guard_page_config_s {
	// Will any guard pages be added anywhere
	bool xgpc_enabled;
	// Will we have guard pages in the data segment group
	bool xgpc_enabled_for_data;
	uint8_t xgpc_max_run_tiny;
	// How many guard pages should be allocated for 256 pages of tiny chunks
	uint8_t xgpc_tiny_guard_density;
	uint8_t xgpc_max_run_small;
	uint8_t xgpc_small_guard_density;
} * __single xzm_guard_page_config_t;

// Represents the keys used by the bucketing function to assign a bucket
// to a given type hash. We store 128 bits of key material obtained from
// the executable_boothash Apple string passed to the process.
typedef struct xzm_bucketing_keys_s {
	uint64_t xbk_key_data[2];
} xzm_bucketing_keys_t;

struct xzm_main_malloc_zone_s {
	struct xzm_malloc_zone_s		xzmz_base;
	uint64_t						xzmz_total_size;
	xzm_bucketing_keys_t			xzmz_bucketing_keys;
	// not in the bitfield for faster access
	bool							xzmz_narrow_bucketing;
	uint8_t							xzmz_use_ranges : 1;
	uint8_t							xzmz_madvise_workaround : 1;
	uint8_t							xzmz_defer_small: 1;
	uint8_t							xzmz_defer_tiny: 1;
	uint8_t							xzmz_defer_large: 1;
	uint8_t							xzmz_deallocate_segment : 1;
	uint8_t							xzmz_range_group_count;
	// count of segment group ids, up to XZM_SEGMENT_GROUP_IDS_COUNT
	uint8_t							xzmz_segment_group_ids_count;
	// count of segment group fronts
	uint8_t							xzmz_segment_group_front_count;
	// count of segment groups, up to ncpucluster * segment_group_fronts_count
	uint8_t							xzmz_segment_group_count;
	uint8_t							xzmz_metapool_count;
	uint8_t							xzmz_allocation_front_count;
	void *							xzmz_mfm_address;
	// Number of entries to batch before madvising
	uint8_t							xzmz_batch_size;
	uint8_t							xzmz_bin_count;
	uint8_t							xzmz_ptr_bucket_count;
	uint8_t							xzmz_xzone_chunk_threshold;
	// mapping from bin index to size
	uint64_t						*xzmz_xzone_bin_sizes;
	// mapping from bin index to bucket count
	uint8_t							*xzmz_xzone_bin_bucket_counts;
	// mapping from bin index to xzone start offsets
	uint8_t							*xzmz_xzone_bin_offsets;
	// Isolation zone table (by xzone index)
	xzm_isolation_zone_t			xzmz_isolation_zones;
	// Range group table
	xzm_range_group_t				xzmz_range_groups;
	// Segment group table
	xzm_segment_group_t				xzmz_segment_groups;
	// Metapool table
	struct xzm_metapool_s			*xzmz_metapools;
	// Global segment table
	xzm_segment_table_entry_s		*xzmz_segment_table;
	// In addition to the segment table that we use to cover the first 64GB of
	// VA on MacOS, there's a 2 level table to cover the remaining ~128TB. This
	// pointer is here unconditionally to make introspection easier, but will be
	// NULL on embedded
	size_t							xzmz_extended_segment_table_entries;
	xzm_extended_segment_table_entry_s	*xzmz_extended_segment_table;
	// Lock to be held while adding entries to the extended segment table
	_malloc_lock_s					xzmz_extended_segment_table_lock;
	// Maximum mzone index so far given out - incremented on new mzone creation
	xzm_mzone_index_t				xzmz_max_mzone_idx;
	SLIST_HEAD(, xzm_reused_mzone_index_s)	xzmz_reusable_mzidxq;
	_malloc_lock_s					xzmz_mzones_lock;
	struct xzm_guard_page_config_s	xzmz_guard_config;
	uint64_t						xzmz_thread_cache_teardown_gen;
	_malloc_lock_s					xzmz_thread_cache_list_lock;
	LIST_HEAD(, xzm_thread_cache_s)	xzmz_thread_cache_list;
	// Deferred reclamation metadata
	xzm_reclaim_buffer_t			xzmz_reclaim_buffer;
};

#pragma mark Deferred reclamation interfaces

#if CONFIG_XZM_DEFERRED_RECLAIM

MALLOC_NOEXPORT
bool
xzm_reclaim_init(xzm_main_malloc_zone_t main,
		mach_vm_reclaim_count_t initial_count, mach_vm_reclaim_count_t max_count);

MALLOC_NOEXPORT
mach_vm_reclaim_id_t
xzm_reclaim_mark_free_locked(xzm_reclaim_buffer_t buffer, uint8_t *addr,
		size_t size, bool reusable, bool *update_accounting_out);

MALLOC_NOEXPORT
void
xzm_reclaim_force_sync(xzm_reclaim_buffer_t buffer);

MALLOC_NOEXPORT
void
xzm_reclaim_sync_and_resize(xzm_reclaim_buffer_t buffer);

#endif // CONFIG_XZM_DEFERRED_RECLAIM

#pragma mark Range and segment interfaces

MALLOC_NOEXPORT
void
xzm_main_malloc_zone_init_range_groups(xzm_main_malloc_zone_t main);

typedef SLIST_HEAD(, xzm_slice_s) xzm_preallocate_list_s;

MALLOC_NOEXPORT
xzm_chunk_t
xzm_segment_group_alloc_chunk(xzm_segment_group_t sg, xzm_slice_kind_t kind,
		xzm_xzone_guard_config_t guard_config, xzm_slice_count_t slice_count,
		xzm_preallocate_list_s *preallocate_list, size_t alignment, bool clear,
		bool purgeable);

MALLOC_NOEXPORT
bool
xzm_segment_group_try_realloc_large_chunk(xzm_segment_group_t sg,
		xzm_segment_t segment, xzm_chunk_t chunk,
		xzm_slice_count_t new_slice_count);

MALLOC_NOEXPORT
bool
xzm_segment_group_try_realloc_huge_chunk(xzm_segment_group_t sg,
		xzm_malloc_zone_t zone, xzm_segment_t segment,
		xzm_chunk_t chunk, xzm_slice_count_t new_slice_count);

MALLOC_NOEXPORT
void
xzm_segment_group_free_chunk(xzm_segment_group_t sg, xzm_chunk_t chunk,
		bool purgeable, bool small_madvise_needed);

MALLOC_NOEXPORT
void
xzm_segment_group_segment_madvise_chunk(xzm_segment_group_t sg,
		xzm_chunk_t chunk);

#if CONFIG_XZM_DEFERRED_RECLAIM

MALLOC_NOEXPORT
void
xzm_chunk_mark_free(xzm_malloc_zone_t xzz, xzm_chunk_t chunk);

MALLOC_NOEXPORT
bool
xzm_chunk_mark_used(xzm_malloc_zone_t xzz, xzm_chunk_t chunk, bool *was_reclaimed);

#endif // CONFIG_XZM_DEFERRED_RECLAIM

MALLOC_NOEXPORT
void
xzm_segment_group_segment_madvise_span(xzm_segment_group_t sg,
		uint8_t *slice_start,
		xzm_slice_count_t count);

typedef kern_return_t (^xzm_span_enumerator_t)(xzm_slice_t span,
		xzm_slice_count_t slice_count);

MALLOC_NOEXPORT
kern_return_t
xzm_segment_group_segment_foreach_span(xzm_segment_t segment,
		MALLOC_NOESCAPE xzm_span_enumerator_t enumerator);

typedef kern_return_t (^xzm_metapool_enumerator_t)(vm_address_t slab_addr,
		vm_size_t slab_size, xzm_metapool_id_t metapool_id);

typedef kern_return_t (^xzm_segment_enumerator_t)(vm_address_t segment_addr,
		xzm_segment_t segment, const char *indent);

typedef kern_return_t (^xzm_chunk_enumerator_t)(vm_address_t segment_addr,
		xzm_segment_t segment, xzm_chunk_t chunk, xzm_slice_count_t slice_count,
		vm_address_t start_addr, xzm_xzone_t xz, vm_range_t *ranges,
		size_t count);

typedef kern_return_t (^xzm_free_span_enumerator_t)(vm_address_t segment_addr,
		xzm_segment_t segment, xzm_slice_t span,
		xzm_slice_count_t slice_count, vm_address_t start_addr);

typedef kern_return_t (^xzm_segment_table_enumerator_t)(
		vm_address_t segment_addr);

typedef kern_return_t (^xzm_thread_cache_enumerator_t)(
		vm_address_t thread_cache_addr, xzm_thread_cache_t tc);

MALLOC_NOEXPORT
kern_return_t
xzm_segment_table_foreach(xzm_segment_table_entry_s *segment_table,
		size_t num_entries,
		MALLOC_NOESCAPE xzm_segment_table_enumerator_t enumerator,
		xzm_segment_t *last_segment_enumerated);

#pragma mark Metapool

// The metadata pool passed to xzm_metapool_init is an optional second metapool
// allocator that the new metapool can use to allocate metadata (slab and block
// structures), so that it can move metadata out of line and madvise its slabs
MALLOC_NOEXPORT
void
xzm_metapool_init(xzm_metapool_t mp, xzm_metapool_id_t pool_id, uint8_t vm_tag,
		uint32_t slab_size, uint32_t block_align, uint32_t block_size,
		xzm_metapool_t metadata_pool);

MALLOC_NOEXPORT
void *
xzm_metapool_alloc(xzm_metapool_t mp);

MALLOC_NOEXPORT
void
xzm_metapool_free(xzm_metapool_t mp, void *block);

#pragma mark Module interface

MALLOC_NOEXPORT
malloc_zone_t *
xzm_main_malloc_zone_create(unsigned debug_flags,
		const char * __null_terminated * __null_terminated envp,
		const char * __null_terminated * __null_terminated apple,
		const char *bootargs);

MALLOC_NOEXPORT
malloc_zone_t *
xzm_malloc_zone_create(unsigned debug_flags, xzm_main_malloc_zone_t main_ref);

OS_OPTIONS(xzm_malloc_options, uint32_t,
	XZM_MALLOC_CLEAR	= 0x01,
	XZM_MALLOC_NO_MFM	= 0x02,
	XZM_MALLOC_CANONICAL_TAG	= 0x40000000,
);

MALLOC_NOEXPORT
void *
xzm_malloc(xzm_malloc_zone_t zone, size_t size,
		malloc_type_descriptor_t type_desc, xzm_malloc_options_t opt)
		__alloc_size(2);

// power-of-2 blocks are naturally aligned up to this size
#define XZM_TRIVIAL_MEMALIGN_SIZE_MAX XZM_TINY_BLOCK_SIZE_MAX

MALLOC_ALWAYS_INLINE MALLOC_NOEXPORT
void *
xzm_malloc_inline(xzm_malloc_zone_t zone, size_t size,
		malloc_type_descriptor_t type_desc, xzm_malloc_options_t opt)
		__alloc_size(2);

MALLOC_NOEXPORT
void *
xzm_realloc(xzm_malloc_zone_t zone, void * __unsafe_indexable ptr,
		size_t new_size, malloc_type_descriptor_t type_desc) __alloc_size(3);

MALLOC_NOEXPORT
void *
xzm_memalign(xzm_malloc_zone_t zone, size_t alignment, size_t size,
		malloc_type_descriptor_t type_desc, xzm_malloc_options_t opt)
		__alloc_align(2) __alloc_size(3);

bool
xzm_ptr_lookup_4test(xzm_malloc_zone_t zone, void *ptr,
		xzm_slice_kind_t *kind_out, xzm_segment_group_id_t *sgid_out,
		xzm_xzone_bucket_t *bucket_out);

uint8_t
xzm_type_choose_ptr_bucket_4test(const xzm_bucketing_keys_t *const keys,
		uint8_t ptr_bucket_count, malloc_type_descriptor_t type_desc);

#pragma mark Introspection interface

MALLOC_NOEXPORT
size_t
xzm_good_size(xzm_malloc_zone_t zone, size_t size);

MALLOC_NOEXPORT
boolean_t
xzm_check(xzm_malloc_zone_t zone);

MALLOC_NOEXPORT
void
xzm_log(xzm_malloc_zone_t zone, void *log_address);

MALLOC_NOEXPORT
boolean_t
xzm_locked(xzm_malloc_zone_t zone);

MALLOC_NOEXPORT
void
xzm_force_lock(xzm_malloc_zone_t zone);

MALLOC_NOEXPORT
void
xzm_force_unlock(xzm_malloc_zone_t zone);

MALLOC_NOEXPORT
void
xzm_reinit_lock(xzm_malloc_zone_t zone);

MALLOC_NOEXPORT
void
xzm_force_lock_global_state(malloc_zone_t *main_zone);

MALLOC_NOEXPORT
void
xzm_force_unlock_global_state(malloc_zone_t *main_zone);

MALLOC_NOEXPORT
void
xzm_force_reinit_lock_global_state(malloc_zone_t *main_zone);

#endif // __XZONE_MALLOC_H__

#endif // CONFIG_XZONE_MALLOC
