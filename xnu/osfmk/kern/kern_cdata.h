/*
 * Copyright (c) 2015 Apple Inc. All rights reserved.
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

#ifndef _KERN_CDATA_H_
#define _KERN_CDATA_H_

#include <kern/kcdata.h>
#include <mach/mach_types.h>
#ifdef XNU_KERNEL_PRIVATE
#include <libkern/zlib.h>
#endif

/*
 * Do not use these macros!
 *
 * Instead, you should use kcdata_iter_* functions defined in kcdata.h.  These
 * macoros have no idea where the kcdata buffer ends, so they are all unsafe.
 */
#define KCDATA_ITEM_HEADER_SIZE         (sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint64_t))
#define KCDATA_ITEM_ITER(item)          kcdata_iter_unsafe((void*)(item))
#define KCDATA_ITEM_TYPE(item)          kcdata_iter_type(KCDATA_ITEM_ITER(item))
#define KCDATA_ITEM_SIZE(item)          kcdata_iter_size(KCDATA_ITEM_ITER(item))
#define KCDATA_ITEM_FLAGS(item)         kcdata_iter_flags(KCDATA_ITEM_ITER(item))
#define KCDATA_ITEM_ARRAY_GET_EL_TYPE(item)    kcdata_iter_array_elem_type(KCDATA_ITEM_ITER(item))
#define KCDATA_ITEM_ARRAY_GET_EL_COUNT(item)   kcdata_iter_array_elem_count(KCDATA_ITEM_ITER(item))
#define KCDATA_ITEM_ARRAY_GET_EL_SIZE(item)    kcdata_iter_array_elem_size(KCDATA_ITEM_ITER(item))
#define KCDATA_CONTAINER_ID(item)              kcdata_iter_container_id(KCDATA_ITEM_ITER(item))
#define KCDATA_ITEM_NEXT_HEADER(itemx)   (kcdata_iter_next(KCDATA_ITEM_ITER(itemx)).item)
#define KCDATA_ITEM_FOREACH(head)       for (; KCDATA_ITEM_TYPE(head) != KCDATA_TYPE_BUFFER_END; (head) = KCDATA_ITEM_NEXT_HEADER(head))
#define KCDATA_ITEM_DATA_PTR(item)      kcdata_iter_payload(KCDATA_ITEM_ITER(item))
#define KCDATA_ITEM_FIND_TYPE(itemx, type) (kcdata_iter_find_type(KCDATA_ITEM_ITER(itemx), type).item)
#define kcdata_get_container_type(buffer) kcdata_iter_container_type(KCDATA_ITEM_ITER(buffer))
#define kcdata_get_data_with_desc(buf, desc, data) kcdata_iter_get_data_with_desc(KCDATA_ITEM_ITER(buf),desc,data,NULL)
/* Do not use these macros! */

__options_decl(kcd_compression_type_t, uint64_t, {
	KCDCT_NONE = 0x00,
	KCDCT_ZLIB = 0x01,
});

#ifdef KERNEL
#ifdef XNU_KERNEL_PRIVATE

__options_decl(kcd_cd_flag_t, uint64_t, {
	KCD_CD_FLAG_IN_MARK  = 0x01,
	KCD_CD_FLAG_FINALIZE = 0x02,
});

/* Structure to save zstream and other compression metadata */
struct kcdata_compress_descriptor {
	z_stream kcd_cd_zs;
	void *kcd_cd_base;
	uint64_t kcd_cd_offset;
	size_t kcd_cd_maxoffset;
	uint64_t kcd_cd_mark_begin;
	kcd_cd_flag_t kcd_cd_flags;
	kcd_compression_type_t kcd_cd_compression_type;
	void (*kcd_cd_memcpy_f)(void *, const void *, size_t);
	mach_vm_address_t kcd_cd_totalout_addr;
	mach_vm_address_t kcd_cd_totalin_addr;
};

/*
 * Various, compression algorithm agnostic flags for controlling writes to the
 * output buffer.
 */
enum kcdata_compression_flush {
	/*
	 * Hint that no flush is needed because more data is expected. Doesn't
	 * guarantee that no data will be written to the output buffer, since the
	 * underlying algorithm may decide that it's running out of space and may
	 * flush to the output buffer.
	 */
	KCDCF_NO_FLUSH,
	/*
	 * Hint to flush all internal buffers to the output buffers.
	 */
	KCDCF_SYNC_FLUSH,
	/*
	 * Hint that this is going to be the last call to the compression function,
	 * so flush all output buffers and mark state as finished.
	 */
	KCDCF_FINISH,
};

/* Structure to save information about kcdata */
struct kcdata_descriptor {
	uint32_t            kcd_length;
	uint16_t kcd_flags;
#define KCFLAG_USE_MEMCOPY 0x0
#define KCFLAG_USE_COPYOUT 0x1
#define KCFLAG_NO_AUTO_ENDBUFFER 0x2
#define KCFLAG_USE_COMPRESSION 0x4
#define KCFLAG_ALLOC_CALLBACK 0x8
	uint16_t kcd_user_flags; /* reserved for subsystems using kcdata */
	mach_vm_address_t kcd_addr_begin;
	mach_vm_address_t kcd_addr_end;
	struct kcdata_compress_descriptor kcd_comp_d;
	uint32_t            kcd_endalloced;
	struct kcdata_descriptor * (*kcd_alloc_callback) (struct kcdata_descriptor*, size_t);
};

typedef struct kcdata_descriptor * kcdata_descriptor_t;

#define MAX_INFLIGHT_KCOBJECT_LW_CORPSE   15
__options_decl(kcdata_obj_flags_t, uint32_t, {
	KCDATA_OBJECT_TYPE_LW_CORPSE  = 0x1, /* for lightweight corpse */
});

struct kcdata_object {
	kcdata_descriptor_t ko_data;
	kcdata_obj_flags_t  ko_flags;
	ipc_port_t          ko_port;
	uint32_t            ko_alloc_size;
	os_refcnt_t         ko_refs;
};

kcdata_descriptor_t kcdata_memory_alloc_init(mach_vm_address_t crash_data_p, unsigned data_type, unsigned size, unsigned flags);
kern_return_t kcdata_memory_static_init(
	kcdata_descriptor_t data, mach_vm_address_t buffer_addr_p, unsigned data_type, unsigned size, unsigned flags);
kern_return_t kcdata_memory_destroy(kcdata_descriptor_t data);
kern_return_t
kcdata_add_container_marker(kcdata_descriptor_t data, uint32_t header_type, uint32_t container_type, uint64_t identifier);
kern_return_t kcdata_add_type_definition(kcdata_descriptor_t data,
    uint32_t type_id,
    char * type_name,
    struct kcdata_subtype_descriptor * elements_array_addr,
    uint32_t elements_count);

kern_return_t kcdata_add_uint64_with_description(kcdata_descriptor_t crashinfo, uint64_t data, const char * description);
kern_return_t kcdata_add_uint32_with_description(kcdata_descriptor_t crashinfo, uint32_t data, const char * description);

kern_return_t kcdata_undo_add_container_begin(kcdata_descriptor_t data);

kern_return_t kcdata_write_buffer_end(kcdata_descriptor_t data);
void *kcdata_memory_get_begin_addr(kcdata_descriptor_t data);
kern_return_t kcdata_init_compress(kcdata_descriptor_t, int hdr_tag, void (*memcpy_f)(void *, const void *, size_t), uint64_t type);
kern_return_t kcdata_push_data(kcdata_descriptor_t data, uint32_t type, uint32_t size, const void *input_data);
kern_return_t kcdata_push_array(kcdata_descriptor_t data, uint32_t type_of_element, uint32_t size_of_element, uint32_t count, const void *input_data);
kern_return_t kcdata_compress_memory_addr(kcdata_descriptor_t data, void *ptr);
void *kcdata_endalloc(kcdata_descriptor_t data, size_t length);
kern_return_t kcdata_finish(kcdata_descriptor_t data);
void kcdata_compression_window_open(kcdata_descriptor_t data);
kern_return_t kcdata_compression_window_close(kcdata_descriptor_t data);
void kcd_finalize_compression(kcdata_descriptor_t data);

/* kcdata mach port representation */
kern_return_t kcdata_object_throttle_get(kcdata_obj_flags_t flags);
void kcdata_object_throttle_release(kcdata_obj_flags_t flags);
kern_return_t kcdata_create_object(kcdata_descriptor_t data, kcdata_obj_flags_t flags, uint32_t size, kcdata_object_t *objp);
void kcdata_object_release(kcdata_object_t obj);
void kcdata_object_reference(kcdata_object_t obj);
kcdata_object_t convert_port_to_kcdata_object(ipc_port_t port);
ipc_port_t convert_kcdata_object_to_port(kcdata_object_t obj);
#else /* XNU_KERNEL_PRIVATE */

typedef void * kcdata_descriptor_t;

#endif /* XNU_KERNEL_PRIVATE */

uint32_t kcdata_estimate_required_buffer_size(uint32_t num_items, uint32_t payload_size);
uint64_t kcdata_memory_get_used_bytes(kcdata_descriptor_t kcd);
uint64_t kcdata_memory_get_uncompressed_bytes(kcdata_descriptor_t kcd);
kern_return_t kcdata_memcpy(kcdata_descriptor_t data, mach_vm_address_t dst_addr, const void * src_addr, uint32_t size);
kern_return_t kcdata_bzero(kcdata_descriptor_t data, mach_vm_address_t dst_addr, uint32_t size);
kern_return_t kcdata_get_memory_addr(kcdata_descriptor_t data, uint32_t type, uint32_t size, mach_vm_address_t * user_addr);
kern_return_t kcdata_get_memory_addr_for_array(
	kcdata_descriptor_t data, uint32_t type_of_element, uint32_t size_of_element, uint32_t count, mach_vm_address_t * user_addr);

#endif /* KERNEL */
#endif /* _KERN_CDATA_H_ */
