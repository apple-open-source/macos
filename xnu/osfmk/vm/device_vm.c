/*
 * Copyright (c) 2000-2020 Apple Inc. All rights reserved.
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

#include <sys/errno.h>

#include <mach/mach_types.h>
#include <mach/kern_return.h>
#include <mach/memory_object_control.h>
#include <mach/memory_object_types.h>
#include <mach/port.h>
#include <mach/policy.h>
#include <mach/upl.h>
#include <kern/kern_types.h>
#include <kern/ipc_kobject.h>
#include <kern/host.h>
#include <kern/thread.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>
#include <device/device_port.h>
#include <vm/memory_object.h>
#include <vm/vm_pageout.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_protos.h>
#include <mach/sdt.h>
#include <os/refcnt.h>


/* Device VM COMPONENT INTERFACES */


/*
 * Device PAGER
 */


/* until component support available */



/* until component support available */
const struct memory_object_pager_ops device_pager_ops = {
	.memory_object_reference = device_pager_reference,
	.memory_object_deallocate = device_pager_deallocate,
	.memory_object_init = device_pager_init,
	.memory_object_terminate = device_pager_terminate,
	.memory_object_data_request = device_pager_data_request,
	.memory_object_data_return = device_pager_data_return,
	.memory_object_data_initialize = device_pager_data_initialize,
	.memory_object_map = device_pager_map,
	.memory_object_last_unmap = device_pager_last_unmap,
	.memory_object_backing_object = NULL,
	.memory_object_pager_name = "device pager"
};

typedef uintptr_t device_port_t;

/*
 * The start of "struct device_pager" MUST match a "struct memory_object".
 */
typedef struct device_pager {
	/* mandatory generic header */
	struct memory_object dev_pgr_hdr;

	/* pager-specific data */
	lck_mtx_t       lock;
	device_port_t   device_handle;  /* device_handle */
	vm_size_t       size;
#if MEMORY_OBJECT_HAS_REFCOUNT
#define dev_pgr_hdr_ref dev_pgr_hdr.mo_ref
#else
	os_ref_atomic_t dev_pgr_hdr_ref;
#endif
	int             flags;
	boolean_t       is_mapped;
} *device_pager_t;

__header_always_inline os_ref_count_t
device_pager_get_refcount(device_pager_t device_object)
{
	return os_ref_get_count_raw(&device_object->dev_pgr_hdr_ref);
}

LCK_GRP_DECLARE(device_pager_lck_grp, "device_pager");

KALLOC_TYPE_DEFINE(device_pager_zone, struct device_pager, KT_DEFAULT);

#define device_pager_lock_init(pager) \
	lck_mtx_init(&(pager)->lock, &device_pager_lck_grp, LCK_ATTR_NULL)
#define device_pager_lock_destroy(pager) \
	lck_mtx_destroy(&(pager)->lock, &device_pager_lck_grp)
#define device_pager_lock(pager) lck_mtx_lock(&(pager)->lock)
#define device_pager_unlock(pager) lck_mtx_unlock(&(pager)->lock)

device_pager_t
device_pager_lookup(            /* forward */
	memory_object_t);

device_pager_t
device_object_create(void);     /* forward */

#define DEVICE_PAGER_NULL       ((device_pager_t) 0)

#define MAX_DNODE               10000


/*
 *
 */
memory_object_t
device_pager_setup(
	__unused memory_object_t device,
	uintptr_t               device_handle,
	vm_size_t       size,
	int             flags)
{
	device_pager_t  device_object;
	memory_object_control_t control;
	vm_object_t     object;

	device_object = device_object_create();
	if (device_object == DEVICE_PAGER_NULL) {
		panic("device_pager_setup: device_object_create() failed");
	}

	device_object->device_handle = device_handle;
	device_object->size = size;
	device_object->flags = flags;

	memory_object_create_named((memory_object_t) device_object,
	    size,
	    &control);
	object = memory_object_control_to_vm_object(control);

	memory_object_mark_trusted(control);

	assert(object != VM_OBJECT_NULL);
	vm_object_lock(object);
	VM_OBJECT_SET_TRUE_SHARE(object, TRUE);
	if (object->copy_strategy == MEMORY_OBJECT_COPY_SYMMETRIC) {
		object->copy_strategy = MEMORY_OBJECT_COPY_DELAY;
	}
	vm_object_unlock(object);

	return (memory_object_t)device_object;
}

/*
 *
 */
kern_return_t
device_pager_populate_object(
	memory_object_t         device,
	memory_object_offset_t  offset,
	ppnum_t                 page_num,
	vm_size_t               size)
{
	device_pager_t  device_object;
	vm_object_t     vm_object;
	kern_return_t   kr;
	upl_t           upl;

	device_object = device_pager_lookup(device);
	if (device_object == DEVICE_PAGER_NULL) {
		return KERN_FAILURE;
	}

	vm_object = (vm_object_t)memory_object_control_to_vm_object(
		device_object->dev_pgr_hdr.mo_control);
	if (vm_object == NULL) {
		return KERN_FAILURE;
	}

	kr =  vm_object_populate_with_private(
		vm_object, offset, page_num, size);
	if (kr != KERN_SUCCESS) {
		return kr;
	}

	if (!vm_object->phys_contiguous) {
		unsigned int null_size = 0;
		assert((upl_size_t) size == size);
		kr = vm_object_upl_request(vm_object,
		    (vm_object_offset_t)offset,
		    (upl_size_t) size, &upl, NULL,
		    &null_size,
		    (UPL_NO_SYNC | UPL_CLEAN_IN_PLACE),
		    VM_KERN_MEMORY_NONE);
		if (kr != KERN_SUCCESS) {
			panic("device_pager_populate_object: list_req failed");
		}

		upl_commit(upl, NULL, 0);
		upl_deallocate(upl);
	}


	return kr;
}

/*
 *
 */
device_pager_t
device_pager_lookup(
	memory_object_t mem_obj)
{
	device_pager_t  device_object;

	assert(mem_obj->mo_pager_ops == &device_pager_ops);
	device_object = (device_pager_t)mem_obj;
	assert(device_pager_get_refcount(device_object) > 0);
	return device_object;
}

/*
 *
 */
kern_return_t
device_pager_init(
	memory_object_t mem_obj,
	memory_object_control_t control,
	__unused memory_object_cluster_size_t pg_size)
{
	device_pager_t   device_object;
	kern_return_t   kr;
	memory_object_attr_info_data_t  attributes;

	vm_object_t     vm_object;


	if (control == MEMORY_OBJECT_CONTROL_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	device_object = device_pager_lookup(mem_obj);

	memory_object_control_reference(control);
	device_object->dev_pgr_hdr.mo_control = control;


/* The following settings should be done through an expanded change */
/* attributes call */

	vm_object = (vm_object_t)memory_object_control_to_vm_object(control);
	vm_object_lock(vm_object);
	VM_OBJECT_SET_PRIVATE(vm_object, TRUE);
	if (device_object->flags & DEVICE_PAGER_CONTIGUOUS) {
		VM_OBJECT_SET_PHYS_CONTIGUOUS(vm_object, TRUE);
	}
	if (device_object->flags & DEVICE_PAGER_NOPHYSCACHE) {
		VM_OBJECT_SET_NOPHYSCACHE(vm_object, TRUE);
	}

	vm_object->wimg_bits = device_object->flags & VM_WIMG_MASK;
	vm_object_unlock(vm_object);


	attributes.copy_strategy = MEMORY_OBJECT_COPY_NONE;
	/* attributes.cluster_size = (1 << (CLUSTER_SHIFT + PAGE_SHIFT));*/
	attributes.cluster_size = (1 << (PAGE_SHIFT));
	attributes.may_cache_object = FALSE;
	attributes.temporary = TRUE;

	kr = memory_object_change_attributes(
		control,
		MEMORY_OBJECT_ATTRIBUTE_INFO,
		(memory_object_info_t) &attributes,
		MEMORY_OBJECT_ATTR_INFO_COUNT);
	if (kr != KERN_SUCCESS) {
		panic("device_pager_init: memory_object_change_attributes() failed");
	}

	return KERN_SUCCESS;
}

static kern_return_t
device_pager_data_action(
	memory_object_t                 mem_obj,
	memory_object_offset_t          offset,
	memory_object_cluster_size_t    length,
	vm_prot_t                       protection)
{
	device_pager_t  device_object;
	memory_object_offset_t end_offset;
	kern_return_t kr;

	device_object = device_pager_lookup(mem_obj);

	if (device_object == DEVICE_PAGER_NULL) {
		panic("%s: lookup failed", __func__);
	}

	if (offset >= device_object->size ||
	    os_add_overflow(offset, length, &end_offset) ||
	    end_offset > device_object->size) {
		return KERN_INVALID_VALUE;
	}

	__IGNORE_WCASTALIGN(kr = device_data_action(device_object->device_handle,
	    (ipc_port_t) device_object, protection, offset, length));

	return kr;
}

/*
 *
 */
/*ARGSUSED6*/
kern_return_t
device_pager_data_return(
	memory_object_t                 mem_obj,
	memory_object_offset_t          offset,
	memory_object_cluster_size_t                    data_cnt,
	__unused memory_object_offset_t *resid_offset,
	__unused int                    *io_error,
	__unused boolean_t              dirty,
	__unused boolean_t              kernel_copy,
	__unused int                    upl_flags)
{
	return device_pager_data_action(mem_obj, offset, data_cnt,
	           VM_PROT_READ | VM_PROT_WRITE);
}

/*
 *
 */
kern_return_t
device_pager_data_request(
	memory_object_t         mem_obj,
	memory_object_offset_t  offset,
	memory_object_cluster_size_t            length,
	__unused vm_prot_t      protection_required,
	__unused memory_object_fault_info_t     fault_info)
{
	return device_pager_data_action(mem_obj, offset, length, VM_PROT_READ);
}

/*
 *
 */
void
device_pager_reference(
	memory_object_t         mem_obj)
{
	device_pager_t          device_object;

	device_object = device_pager_lookup(mem_obj);
	os_ref_retain_raw(&device_object->dev_pgr_hdr_ref, NULL);
	DTRACE_VM2(device_pager_reference,
	    device_pager_t, device_object,
	    unsigned int, device_pager_get_refcount(device_object));
}

/*
 *
 */
void
device_pager_deallocate(
	memory_object_t         mem_obj)
{
	device_pager_t          device_object;
	memory_object_control_t device_control;
	os_ref_count_t          ref_count;

	device_object = device_pager_lookup(mem_obj);

	DTRACE_VM2(device_pager_deallocate,
	    device_pager_t, device_object,
	    unsigned int, device_pager_get_refcount(device_object));

	ref_count = os_ref_release_raw(&device_object->dev_pgr_hdr_ref, NULL);

	if (ref_count == 1) {
		/*
		 * The last reference is our "named" reference.
		 * Close the device and "destroy" the VM object.
		 */

		DTRACE_VM2(device_pager_destroy,
		    device_pager_t, device_object,
		    unsigned int, device_pager_get_refcount(device_object));

		assert(device_object->is_mapped == FALSE);
		if (device_object->device_handle != (device_port_t) NULL) {
			device_close(device_object->device_handle);
			device_object->device_handle = (device_port_t) NULL;
		}
		device_control = device_object->dev_pgr_hdr.mo_control;
		memory_object_destroy(device_control, VM_OBJECT_DESTROY_UNKNOWN_REASON);
	} else if (ref_count == 0) {
		/*
		 * No more references: free the pager.
		 */
		DTRACE_VM2(device_pager_free,
		    device_pager_t, device_object,
		    unsigned int, device_pager_get_refcount(device_object));

		device_control = device_object->dev_pgr_hdr.mo_control;

		if (device_control != MEMORY_OBJECT_CONTROL_NULL) {
			memory_object_control_deallocate(device_control);
			device_object->dev_pgr_hdr.mo_control = MEMORY_OBJECT_CONTROL_NULL;
		}
		device_pager_lock_destroy(device_object);

		zfree(device_pager_zone, device_object);
	}
	return;
}

kern_return_t
device_pager_data_initialize(
	__unused memory_object_t                mem_obj,
	__unused memory_object_offset_t offset,
	__unused memory_object_cluster_size_t           data_cnt)
{
	panic("device_pager_data_initialize");
	return KERN_FAILURE;
}

kern_return_t
device_pager_terminate(
	__unused memory_object_t        mem_obj)
{
	return KERN_SUCCESS;
}


/*
 *
 */
kern_return_t
device_pager_map(
	memory_object_t mem_obj,
	__unused vm_prot_t              prot)
{
	device_pager_t          device_object;

	device_object = device_pager_lookup(mem_obj);

	device_pager_lock(device_object);
	assert(device_pager_get_refcount(device_object) > 0);
	if (device_object->is_mapped == FALSE) {
		/*
		 * First mapping of this pager: take an extra reference
		 * that will remain until all the mappings of this pager
		 * are removed.
		 */
		device_object->is_mapped = TRUE;
		device_pager_reference(mem_obj);
	}
	device_pager_unlock(device_object);

	return KERN_SUCCESS;
}

kern_return_t
device_pager_last_unmap(
	memory_object_t mem_obj)
{
	device_pager_t  device_object;
	boolean_t       drop_ref;

	device_object = device_pager_lookup(mem_obj);

	device_pager_lock(device_object);
	assert(device_pager_get_refcount(device_object) > 0);
	if (device_object->is_mapped) {
		device_object->is_mapped = FALSE;
		drop_ref = TRUE;
	} else {
		drop_ref = FALSE;
	}
	device_pager_unlock(device_object);

	if (drop_ref) {
		device_pager_deallocate(mem_obj);
	}

	return KERN_SUCCESS;
}



/*
 *
 */
device_pager_t
device_object_create(void)
{
	device_pager_t  device_object;

	device_object = zalloc_flags(device_pager_zone,
	    Z_WAITOK | Z_ZERO | Z_NOFAIL);

	device_object->dev_pgr_hdr.mo_ikot = IKOT_MEMORY_OBJECT;
	device_object->dev_pgr_hdr.mo_pager_ops = &device_pager_ops;
	device_object->dev_pgr_hdr.mo_control = MEMORY_OBJECT_CONTROL_NULL;

	device_pager_lock_init(device_object);
	os_ref_init_raw(&device_object->dev_pgr_hdr_ref, NULL);
	device_object->is_mapped = FALSE;

	DTRACE_VM2(device_pager_create,
	    device_pager_t, device_object,
	    unsigned int, device_pager_get_refcount(device_object));

	return device_object;
}

boolean_t
is_device_pager_ops(const struct memory_object_pager_ops *pager_ops)
{
	if (pager_ops == &device_pager_ops) {
		return TRUE;
	}
	return FALSE;
}
