/*
 * Copyright (c) 2013-2020 Apple Inc. All rights reserved.
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

#include <os/overflow.h>
#include <mach/mach_types.h>
#include <mach/mach_traps.h>
#include <mach/notify.h>
#include <ipc/ipc_types.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_voucher.h>
#include <kern/ipc_kobject.h>
#include <kern/ipc_tt.h>
#include <kern/mach_param.h>
#include <kern/kalloc.h>
#include <kern/zalloc.h>
#include <kern/smr_hash.h>

#include <libkern/OSAtomic.h>

#include <mach/mach_voucher_server.h>
#include <mach/mach_host_server.h>
#include <voucher/ipc_pthread_priority_types.h>

/*
 * Sysctl variable; enable and disable tracing of voucher contents
 */
uint32_t ipc_voucher_trace_contents = 0;

ZONE_DEFINE_ID(ZONE_ID_IPC_VOUCHERS, "ipc vouchers", struct ipc_voucher,
    ZC_ZFREE_CLEARMEM);

/* deliver voucher notifications */
static void ipc_voucher_no_senders(ipc_port_t, mach_port_mscount_t);

IPC_KOBJECT_DEFINE(IKOT_VOUCHER,
    .iko_op_stable     = true,
    .iko_op_no_senders = ipc_voucher_no_senders);

#define voucher_require(v) \
	zone_id_require(ZONE_ID_IPC_VOUCHERS, sizeof(struct ipc_voucher), v)

/*
 * Voucher hash table
 */
static struct smr_shash voucher_table;

/*
 * Global table of resource manager registrations
 */
static ipc_voucher_attr_manager_t ivam_global_table[MACH_VOUCHER_ATTR_KEY_NUM_WELL_KNOWN];
static struct ipc_voucher_attr_control ivac_global_table[MACH_VOUCHER_ATTR_KEY_NUM_WELL_KNOWN];

static void     iv_dealloc(ipc_voucher_t iv, bool unhash);
static uint32_t iv_obj_hash(const struct smrq_slink *, uint32_t);
static bool     iv_obj_equ(const struct smrq_slink *, smrh_key_t);
static bool     iv_obj_try_get(void *);

SMRH_TRAITS_DEFINE_MEM(voucher_traits, struct ipc_voucher, iv_hash_link,
    .domain      = &smr_ipc,
    .obj_hash    = iv_obj_hash,
    .obj_equ     = iv_obj_equ,
    .obj_try_get = iv_obj_try_get);

os_refgrp_decl(static, iv_refgrp, "voucher", NULL);

static inline void
iv_reference(ipc_voucher_t iv)
{
	os_ref_retain_raw(&iv->iv_refs, &iv_refgrp);
}

static inline bool
iv_try_reference(ipc_voucher_t iv)
{
	return os_ref_retain_try_raw(&iv->iv_refs, &iv_refgrp);
}

static inline void
iv_release(ipc_voucher_t iv)
{
	if (os_ref_release_raw(&iv->iv_refs, &iv_refgrp) == 0) {
		iv_dealloc(iv, TRUE);
	}
}

/*
 * freelist helper macros
 */
#define IV_FREELIST_END ((iv_index_t) 0)

/*
 * Attribute value hashing helper macros
 */
#define IV_HASH_END UINT32_MAX
#define IV_HASH_VAL(sz, val) \
	(((val) >> 3) % (sz))

static inline iv_index_t
iv_hash_value(
	ipc_voucher_attr_control_t ivac,
	mach_voucher_attr_value_handle_t value)
{
	return IV_HASH_VAL(ivac->ivac_init_table_size, value);
}

/*
 * Convert a key to an index.  This key-index is used to both index
 * into the voucher table of attribute cache indexes and also the
 * table of resource managers by key.
 *
 * For now, well-known keys have a one-to-one mapping of indexes
 * into these tables.  But as time goes on, that may not always
 * be the case (sparse use over time).  This isolates the code from
 * having to change in these cases - yet still lets us keep a densely
 * packed set of tables.
 */
static inline iv_index_t
iv_key_to_index(mach_voucher_attr_key_t key)
{
	if (MACH_VOUCHER_ATTR_KEY_ALL == key ||
	    MACH_VOUCHER_ATTR_KEY_NUM < key) {
		return IV_UNUSED_KEYINDEX;
	}
	return (iv_index_t)key - 1;
}

static inline mach_voucher_attr_key_t
iv_index_to_key(iv_index_t key_index)
{
	if (MACH_VOUCHER_ATTR_KEY_NUM_WELL_KNOWN > key_index) {
		return key_index + 1;
	}
	return MACH_VOUCHER_ATTR_KEY_NONE;
}

static void ivace_release(iv_index_t key_index, iv_index_t value_index);
static ivac_entry_t ivace_lookup(ipc_voucher_attr_control_t ivac,
    iv_index_t index);

static iv_index_t iv_lookup(ipc_voucher_t, iv_index_t);


static void ivgt_lookup(iv_index_t,
    ipc_voucher_attr_manager_t *,
    ipc_voucher_attr_control_t *);

static kern_return_t
ipc_voucher_prepare_processing_recipe(
	ipc_voucher_t voucher,
	ipc_voucher_attr_raw_recipe_array_t recipes,
	ipc_voucher_attr_raw_recipe_array_size_t *in_out_size,
	mach_voucher_attr_recipe_command_t command,
	ipc_voucher_attr_manager_flags flags,
	int *need_processing);

__startup_func
static void
ipc_voucher_init(void)
{
	zone_enable_smr(zone_by_id(ZONE_ID_IPC_VOUCHERS), &smr_ipc, bzero);
	smr_shash_init(&voucher_table, SMRSH_BALANCED, 128);
}
STARTUP(MACH_IPC, STARTUP_RANK_FIRST, ipc_voucher_init);

static ipc_voucher_t
iv_alloc(void)
{
	ipc_voucher_t iv;

	iv = zalloc_id_smr(ZONE_ID_IPC_VOUCHERS, Z_WAITOK_ZERO_NOFAIL);
	os_ref_init_raw(&iv->iv_refs, &iv_refgrp);

	return iv;
}

/*
 *	Routine:	iv_set
 *	Purpose:
 *		Set the voucher's value index for a given key index.
 *	Conditions:
 *		This is only called during voucher creation, as
 *		they are permanent once references are distributed.
 */
static void
iv_set(
	ipc_voucher_t           iv,
	iv_index_t              key_index,
	iv_index_t              value_index)
{
	if (key_index >= MACH_VOUCHER_ATTR_KEY_NUM) {
		panic("key_index >= MACH_VOUCHER_ATTR_KEY_NUM");
	}
	iv->iv_table[key_index] = value_index;
}

static smrh_key_t
iv_key(ipc_voucher_t iv)
{
	smrh_key_t key = {
		.smrk_opaque = iv->iv_table,
		.smrk_len    = sizeof(iv->iv_table),
	};

	return key;
}

static uint32_t
iv_obj_hash(const struct smrq_slink *link, uint32_t seed)
{
	ipc_voucher_t iv;

	iv = __container_of(link, struct ipc_voucher, iv_hash_link);
	return smrh_key_hash_mem(iv_key(iv), seed);
}

static bool
iv_obj_equ(const struct smrq_slink *link, smrh_key_t key)
{
	ipc_voucher_t iv;

	iv = __container_of(link, struct ipc_voucher, iv_hash_link);
	return smrh_key_equ_mem(iv_key(iv), key);
}

static bool
iv_obj_try_get(void *iv)
{
	return iv_try_reference(iv);
}

static void
iv_dealloc(ipc_voucher_t iv, bool unhash)
{
	ipc_port_t port = iv->iv_port;

	/*
	 * Do we have to remove it from the hash?
	 */
	if (unhash) {
		smr_shash_remove(&voucher_table, &iv->iv_hash_link,
		    &voucher_traits);
		KERNEL_DEBUG_CONSTANT(MACHDBG_CODE(DBG_MACH_IPC, MACH_IPC_VOUCHER_DESTROY) | DBG_FUNC_NONE,
		    VM_KERNEL_ADDRPERM((uintptr_t)iv), 0,
		    counter_load(&voucher_table.smrsh_count),
		    0, 0);
	}

	/*
	 * if a port was allocated for this voucher,
	 * it must not have any remaining send rights,
	 * because the port's reference on the voucher
	 * is gone.  We can just discard it now.
	 */
	if (IP_VALID(port)) {
		assert(port->ip_srights == 0);
		ipc_kobject_dealloc_port(port, 0, IKOT_VOUCHER);
		iv->iv_port = MACH_PORT_NULL;
	}

	/* release the attribute references held by this voucher */
	for (natural_t i = 0; i < MACH_VOUCHER_ATTR_KEY_NUM; i++) {
		ivace_release(i, iv->iv_table[i]);
		iv_set(iv, i, IV_UNUSED_VALINDEX);
	}

	zfree_id_smr(ZONE_ID_IPC_VOUCHERS, iv);
}

/*
 *	Routine:	iv_lookup
 *	Purpose:
 *		Find the voucher's value index for a given key_index
 *	Conditions:
 *		Vouchers are permanent, so no locking required to do
 *		a lookup.
 */
static inline iv_index_t
iv_lookup(ipc_voucher_t iv, iv_index_t key_index)
{
	if (key_index < MACH_VOUCHER_ATTR_KEY_NUM) {
		return iv->iv_table[key_index];
	}
	return IV_UNUSED_VALINDEX;
}

/*
 *	Routine:	unsafe_convert_port_to_voucher
 *	Purpose:
 *		Unsafe conversion of a port to a voucher.
 *		Intended only for use by trace and debugging
 *		code. Consumes nothing, validates very little,
 *		produces an unreferenced voucher, which you
 *		MAY NOT use as a voucher, only log as an
 *		address.
 *	Conditions:
 *		Caller has a send-right reference to port.
 *		Port may or may not be locked.
 */
uintptr_t
unsafe_convert_port_to_voucher(
	ipc_port_t      port)
{
	if (IP_VALID(port)) {
		return (uintptr_t)ipc_kobject_get_stable(port, IKOT_VOUCHER);
	}
	return (uintptr_t)IV_NULL;
}

static ipc_voucher_t
ip_get_voucher(ipc_port_t port)
{
	ipc_voucher_t voucher = ipc_kobject_get_stable(port, IKOT_VOUCHER);
	voucher_require(voucher);
	return voucher;
}

/*
 *	Routine:	convert_port_to_voucher
 *	Purpose:
 *		Convert from a port to a voucher.
 *		Doesn't consume the port [send-right] ref;
 *		produces a voucher ref,	which may be null.
 *	Conditions:
 *		Caller has a send-right reference to port.
 *		Port may or may not be locked.
 */
ipc_voucher_t
convert_port_to_voucher(
	ipc_port_t      port)
{
	if (IP_VALID(port) && ip_kotype(port) == IKOT_VOUCHER) {
		/*
		 * No need to lock because we have a reference on the
		 * port, and if it is a true voucher port, that reference
		 * keeps the voucher bound to the port (and active).
		 */
		ipc_voucher_t voucher = ip_get_voucher(port);
		ipc_voucher_reference(voucher);
		return voucher;
	}
	return IV_NULL;
}

/*
 *	Routine:	convert_port_name_to_voucher
 *	Purpose:
 *		Convert from a port name in the current space to a voucher.
 *		Produces a voucher ref,	which may be null.
 *	Conditions:
 *		Nothing locked.
 */

ipc_voucher_t
convert_port_name_to_voucher(
	mach_port_name_t        voucher_name)
{
	ipc_voucher_t iv;
	kern_return_t kr;
	ipc_port_t port;

	if (MACH_PORT_VALID(voucher_name)) {
		kr = ipc_port_translate_send(current_space(), voucher_name, &port);
		if (KERN_SUCCESS != kr) {
			return IV_NULL;
		}

		iv = convert_port_to_voucher(port);
		ip_mq_unlock(port);
		return iv;
	}
	return IV_NULL;
}


void
ipc_voucher_reference(ipc_voucher_t voucher)
{
	if (IPC_VOUCHER_NULL == voucher) {
		return;
	}

	iv_reference(voucher);
}

void
ipc_voucher_release(ipc_voucher_t voucher)
{
	if (IPC_VOUCHER_NULL != voucher) {
		iv_release(voucher);
	}
}

/*
 * Routine:	ipc_voucher_no_senders
 * Purpose:
 *	Called whenever the Mach port system detects no-senders
 *	on the voucher port.
 */
static void
ipc_voucher_no_senders(ipc_port_t port, __unused mach_port_mscount_t mscount)
{
	ipc_voucher_t voucher = ip_get_voucher(port);

	assert(IKOT_VOUCHER == ip_kotype(port));

	/* consume the reference donated by convert_voucher_to_port */
	ipc_voucher_release(voucher);
}

/*
 * Convert a voucher to a port.
 */
ipc_port_t
convert_voucher_to_port(ipc_voucher_t voucher)
{
	if (IV_NULL == voucher) {
		return IP_NULL;
	}

	voucher_require(voucher);
	assert(os_ref_get_count_raw(&voucher->iv_refs) > 0);

	/*
	 * make a send right and donate our reference for ipc_voucher_no_senders
	 * if this is the first send right
	 */
	if (!ipc_kobject_make_send_lazy_alloc_port(&voucher->iv_port,
	    voucher, IKOT_VOUCHER, IPC_KOBJECT_ALLOC_NONE)) {
		ipc_voucher_release(voucher);
	}
	return voucher->iv_port;
}

#define ivace_reset_data(ivace_elem, next_index) {       \
	(ivace_elem)->ivace_value = 0xDEADC0DEDEADC0DE;  \
	(ivace_elem)->ivace_refs = 0;                    \
	(ivace_elem)->ivace_persist = 0;                 \
	(ivace_elem)->ivace_made = 0;                    \
	(ivace_elem)->ivace_free = TRUE;                 \
	(ivace_elem)->ivace_releasing = FALSE;           \
	(ivace_elem)->ivace_layered = 0;                 \
	(ivace_elem)->ivace_index = IV_HASH_END;         \
	(ivace_elem)->ivace_next = (next_index);         \
}

#define ivace_copy_data(ivace_src_elem, ivace_dst_elem) {  \
	(ivace_dst_elem)->ivace_value = (ivace_src_elem)->ivace_value; \
	(ivace_dst_elem)->ivace_refs = (ivace_src_elem)->ivace_refs;   \
	(ivace_dst_elem)->ivace_persist = (ivace_src_elem)->ivace_persist; \
	(ivace_dst_elem)->ivace_made = (ivace_src_elem)->ivace_made;   \
	(ivace_dst_elem)->ivace_free = (ivace_src_elem)->ivace_free;   \
	(ivace_dst_elem)->ivace_layered = (ivace_src_elem)->ivace_layered;   \
	(ivace_dst_elem)->ivace_releasing = (ivace_src_elem)->ivace_releasing; \
	(ivace_dst_elem)->ivace_index = (ivace_src_elem)->ivace_index; \
	(ivace_dst_elem)->ivace_next = (ivace_src_elem)->ivace_next; \
}

static ipc_voucher_attr_control_t
ivac_init_well_known_voucher_attr_control(iv_index_t key_index)
{
	ipc_voucher_attr_control_t ivac = &ivac_global_table[key_index];
	ivac_entry_t table;
	natural_t i;


	/* start with just the inline table */
	table = kalloc_type(struct ivac_entry_s, IVAC_ENTRIES_MIN, Z_WAITOK | Z_ZERO);
	ivac->ivac_table = table;
	ivac->ivac_table_size = IVAC_ENTRIES_MIN;
	ivac->ivac_init_table_size = IVAC_ENTRIES_MIN;
	for (i = 0; i < ivac->ivac_table_size; i++) {
		ivace_reset_data(&table[i], i + 1);
	}

	/* the default table entry is never on freelist */
	table[0].ivace_next = IV_HASH_END;
	table[0].ivace_free = FALSE;
	table[i - 1].ivace_next = IV_FREELIST_END;
	ivac->ivac_freelist = 1;
	ivac_lock_init(ivac);
	ivac->ivac_key_index = key_index;
	return ivac;
}


/*
 * Look up the values for a given <key, index> pair.
 */
static void
ivace_lookup_values(
	ipc_voucher_attr_control_t                      ivac,
	iv_index_t                                      value_index,
	mach_voucher_attr_value_handle_array_t          values,
	mach_voucher_attr_value_handle_array_size_t     *count)
{
	ivac_entry_t ivace;

	if (IV_UNUSED_VALINDEX == value_index) {
		*count = 0;
		return;
	}

	/*
	 * Get the entry and then the linked values.
	 */
	ivac_lock(ivac);
	ivace = ivace_lookup(ivac, value_index);

	assert(ivace->ivace_refs > 0);
	values[0] = ivace->ivace_value;
	ivac_unlock(ivac);
	*count = 1;
}

/*
 * Lookup the entry at the given index into the table
 */
static inline ivac_entry_t
ivace_lookup(ipc_voucher_attr_control_t ivac, iv_index_t index)
{
	if (index >= ivac->ivac_table_size) {
		panic("index >= ivac->ivac_table_size");
	}
	return &ivac->ivac_table[index];
}

/*
 *  ivac_grow_table - Allocate a bigger table of attribute values
 *
 *  Conditions:	ivac is locked on entry and again on return
 */
static void
ivac_grow_table(ipc_voucher_attr_control_t ivac)
{
	iv_index_t i = 0;

	/* NOTE: do not modify *_table and *_size values once set */
	ivac_entry_t new_table = NULL, old_table = NULL;
	iv_index_t new_size, old_size;

	if (ivac->ivac_is_growing) {
		ivac_sleep(ivac);
		return;
	}

	ivac->ivac_is_growing = 1;
	if (ivac->ivac_table_size >= IVAC_ENTRIES_MAX) {
		panic("Cannot grow ipc space beyond IVAC_ENTRIES_MAX. Some process is leaking vouchers");
		return;
	}

	old_size = ivac->ivac_table_size;
	ivac_unlock(ivac);

	new_size = old_size * 2;

	assert(new_size > old_size);
	assert(new_size < IVAC_ENTRIES_MAX);

	new_table = kalloc_type(struct ivac_entry_s, new_size, Z_WAITOK | Z_ZERO);
	if (!new_table) {
		panic("Failed to grow ivac table to size %d", new_size);
		return;
	}

	/* setup the free list for new entries */
	for (i = old_size; i < new_size; i++) {
		ivace_reset_data(&new_table[i], i + 1);
	}

	ivac_lock(ivac);

	for (i = 0; i < ivac->ivac_table_size; i++) {
		ivace_copy_data(&ivac->ivac_table[i], &new_table[i]);
	}

	old_table = ivac->ivac_table;

	ivac->ivac_table = new_table;
	ivac->ivac_table_size = new_size;

	/* adding new free entries at head of freelist */
	ivac->ivac_table[new_size - 1].ivace_next = ivac->ivac_freelist;
	ivac->ivac_freelist = old_size;
	ivac->ivac_is_growing = 0;
	ivac_wakeup(ivac);

	if (old_table) {
		ivac_unlock(ivac);
		kfree_type(struct ivac_entry_s, old_size, old_table);
		ivac_lock(ivac);
	}
}

/*
 * ivace_reference_by_index
 *
 * Take an additional reference on the <key_index, val_index>
 * cached value. It is assumed the caller already holds a
 * reference to the same cached key-value pair.
 */
static void
ivace_reference_by_index(
	iv_index_t      key_index,
	iv_index_t      val_index)
{
	ipc_voucher_attr_control_t ivac;
	ivac_entry_t ivace;

	if (IV_UNUSED_VALINDEX == val_index) {
		return;
	}

	ivgt_lookup(key_index, NULL, &ivac);
	assert(IVAC_NULL != ivac);

	ivac_lock(ivac);
	ivace = ivace_lookup(ivac, val_index);

	assert(0xdeadc0dedeadc0de != ivace->ivace_value);
	assert(0 < ivace->ivace_refs);
	assert(!ivace->ivace_free);

	/* Take ref only on non-persistent values */
	if (!ivace->ivace_persist) {
		ivace->ivace_refs++;
	}
	ivac_unlock(ivac);
}


/*
 * Look up the values for a given <key, index> pair.
 *
 * Consumes a reference on the passed voucher control.
 * Either it is donated to a newly-created value cache
 * or it is released (if we piggy back on an existing
 * value cache entry).
 */
static iv_index_t
ivace_reference_by_value(
	ipc_voucher_attr_control_t      ivac,
	mach_voucher_attr_value_handle_t        value,
	mach_voucher_attr_value_flags_t          flag)
{
	ivac_entry_t ivace = IVACE_NULL;
	iv_index_t hash_index;
	iv_index_t *index_p;
	iv_index_t index;

	if (IVAC_NULL == ivac) {
		return IV_UNUSED_VALINDEX;
	}

	ivac_lock(ivac);
restart:
	hash_index = IV_HASH_VAL(ivac->ivac_init_table_size, value);
	index_p = &ivace_lookup(ivac, hash_index)->ivace_index;
	index = *index_p;
	while (index != IV_HASH_END) {
		ivace = ivace_lookup(ivac, index);
		assert(!ivace->ivace_free);

		if (ivace->ivace_value == value) {
			break;
		}

		assert(ivace->ivace_next != index);
		index = ivace->ivace_next;
	}

	/* found it? */
	if (index != IV_HASH_END) {
		/* only add reference on non-persistent value */
		if (!ivace->ivace_persist) {
			ivace->ivace_refs++;
			ivace->ivace_made++;
		}

		ivac_unlock(ivac);
		return index;
	}

	/* insert new entry in the table */
	index = ivac->ivac_freelist;
	if (IV_FREELIST_END == index) {
		/* freelist empty */
		ivac_grow_table(ivac);
		goto restart;
	}

	/* take the entry off the freelist */
	ivace = ivace_lookup(ivac, index);
	ivac->ivac_freelist = ivace->ivace_next;

	/* initialize the new entry */
	ivace->ivace_value = value;
	ivace->ivace_refs = 1;
	ivace->ivace_made = 1;
	ivace->ivace_free = FALSE;
	ivace->ivace_persist = (flag & MACH_VOUCHER_ATTR_VALUE_FLAGS_PERSIST) ? TRUE : FALSE;

	/* insert the new entry in the proper hash chain */
	ivace->ivace_next = *index_p;
	*index_p = index;
	ivac_unlock(ivac);

	/* donated passed in ivac reference to new entry */

	return index;
}

/*
 * Release a reference on the given <key_index, value_index> pair.
 *
 * Conditions:	called with nothing locked, as it may cause
 *		callouts and/or messaging to the resource
 *		manager.
 */
static void
ivace_release(
	iv_index_t key_index,
	iv_index_t value_index)
{
	ipc_voucher_attr_control_t ivac;
	ipc_voucher_attr_manager_t ivam;
	mach_voucher_attr_value_handle_t value;
	mach_voucher_attr_value_reference_t made;
	mach_voucher_attr_key_t key;
	iv_index_t hash_index;
	ivac_entry_t ivace;
	ivac_entry_t ivace_tmp;
	kern_return_t kr;

	/* cant release the default value */
	if (IV_UNUSED_VALINDEX == value_index) {
		return;
	}

	ivgt_lookup(key_index, &ivam, &ivac);
	assert(IVAC_NULL != ivac);
	assert(IVAM_NULL != ivam);

	ivac_lock(ivac);
	ivace = ivace_lookup(ivac, value_index);

	assert(0 < ivace->ivace_refs);

	/* cant release persistent values */
	if (ivace->ivace_persist) {
		ivac_unlock(ivac);
		return;
	}

	if (0 < --ivace->ivace_refs) {
		ivac_unlock(ivac);
		return;
	}

	key = iv_index_to_key(key_index);
	assert(MACH_VOUCHER_ATTR_KEY_NONE != key);

	/*
	 * if last return reply is still pending,
	 * let it handle this later return when
	 * the previous reply comes in.
	 */
	if (ivace->ivace_releasing) {
		ivac_unlock(ivac);
		return;
	}

	/* claim releasing */
	ivace->ivace_releasing = TRUE;
	value = ivace->ivace_value;

redrive:
	assert(value == ivace->ivace_value);
	assert(!ivace->ivace_free);
	made = ivace->ivace_made;
	ivac_unlock(ivac);

	/* callout to manager's release_value */
	kr = (ivam->ivam_release_value)(ivam, key, value, made);

	/* recalculate entry address as table may have changed */
	ivac_lock(ivac);
	ivace = ivace_lookup(ivac, value_index);
	assert(value == ivace->ivace_value);

	/*
	 * new made values raced with this return.  If the
	 * manager OK'ed the prior release, we have to start
	 * the made numbering over again (pretend the race
	 * didn't happen). If the entry has zero refs again,
	 * re-drive the release.
	 */
	if (ivace->ivace_made != made) {
		if (KERN_SUCCESS == kr) {
			ivace->ivace_made -= made;
		}

		if (0 == ivace->ivace_refs) {
			goto redrive;
		}

		ivace->ivace_releasing = FALSE;
		ivac_unlock(ivac);
		return;
	} else {
		/*
		 * If the manager returned FAILURE, someone took a
		 * reference on the value but have not updated the ivace,
		 * release the lock and return since thread who got
		 * the new reference will update the ivace and will have
		 * non-zero reference on the value.
		 */
		if (KERN_SUCCESS != kr) {
			ivace->ivace_releasing = FALSE;
			ivac_unlock(ivac);
			return;
		}
	}

	assert(0 == ivace->ivace_refs);

	/*
	 * going away - remove entry from its hash
	 * If its at the head of the hash bucket list (common), unchain
	 * at the head. Otherwise walk the chain until the next points
	 * at this entry, and remove it from the the list there.
	 */
	hash_index = iv_hash_value(ivac, value);
	ivace_tmp = ivace_lookup(ivac, hash_index);
	if (ivace_tmp->ivace_index == value_index) {
		ivace_tmp->ivace_index = ivace->ivace_next;
	} else {
		hash_index = ivace_tmp->ivace_index;
		ivace_tmp = ivace_lookup(ivac, hash_index);
		assert(IV_HASH_END != hash_index);
		while (ivace_tmp->ivace_next != value_index) {
			hash_index = ivace_tmp->ivace_next;
			assert(IV_HASH_END != hash_index);
			ivace_tmp = ivace_lookup(ivac, hash_index);
		}
		ivace_tmp->ivace_next = ivace->ivace_next;
	}

	/* Put this entry on the freelist */
	ivace->ivace_value = 0xdeadc0dedeadc0de;
	ivace->ivace_releasing = FALSE;
	ivace->ivace_free = TRUE;
	ivace->ivace_made = 0;
	ivace->ivace_next = ivac->ivac_freelist;
	ivac->ivac_freelist = value_index;
	ivac_unlock(ivac);
}


/*
 * ivgt_looup
 *
 * Lookup an entry in the global table from the context of a manager
 * registration.  Adds a reference to the control to keep the results
 * around (if needed).
 *
 * Because of the calling point, we can't be sure the manager is
 * [fully] registered yet.  So, we must hold the global table lock
 * during the lookup to synchronize with in-parallel registrations
 * (and possible table growth).
 */
static void
ivgt_lookup(
	iv_index_t            key_index,
	ipc_voucher_attr_manager_t *ivamp,
	ipc_voucher_attr_control_t *ivacp)
{
	ipc_voucher_attr_manager_t ivam = IVAM_NULL;
	ipc_voucher_attr_control_t ivac = IVAC_NULL;

	if (key_index < MACH_VOUCHER_ATTR_KEY_NUM_WELL_KNOWN) {
		ivam = ivam_global_table[key_index];
		if (ivam) {
			ivac = &ivac_global_table[key_index];
		}
	}

	if (ivamp) {
		*ivamp = ivam;
	}
	if (ivacp) {
		*ivacp = ivac;
	}
}

/*
 *	Routine:	ipc_replace_voucher_value
 *	Purpose:
 *		Replace the <voucher, key> value with the results of
 *		running the supplied command through the resource
 *		manager's get-value callback.
 *	Conditions:
 *		Nothing locked (may invoke user-space repeatedly).
 *		Caller holds references on voucher and previous voucher.
 */
static kern_return_t
ipc_replace_voucher_value(
	ipc_voucher_t                           voucher,
	mach_voucher_attr_key_t                 key,
	mach_voucher_attr_recipe_command_t      command,
	ipc_voucher_t                           prev_voucher,
	mach_voucher_attr_content_t             content,
	mach_voucher_attr_content_size_t        content_size)
{
	mach_voucher_attr_value_handle_t previous_vals[MACH_VOUCHER_ATTR_VALUE_MAX_NESTED];
	mach_voucher_attr_value_handle_array_size_t previous_vals_count;
	mach_voucher_attr_value_handle_t new_value;
	mach_voucher_attr_value_flags_t new_flag;
	ipc_voucher_t new_value_voucher;
	ipc_voucher_attr_manager_t ivam;
	ipc_voucher_attr_control_t ivac;
	iv_index_t prev_val_index;
	iv_index_t save_val_index;
	iv_index_t val_index;
	iv_index_t key_index;
	kern_return_t kr;

	/*
	 * Get the manager for this key_index.
	 * Returns a reference on the control.
	 */
	key_index = iv_key_to_index(key);
	ivgt_lookup(key_index, &ivam, &ivac);
	if (IVAM_NULL == ivam) {
		return KERN_INVALID_ARGUMENT;
	}

	/* save the current value stored in the forming voucher */
	save_val_index = iv_lookup(voucher, key_index);

	/*
	 * Get the previous value(s) for this key creation.
	 * If a previous voucher is specified, they come from there.
	 * Otherwise, they come from the intermediate values already
	 * in the forming voucher.
	 */
	prev_val_index = (IV_NULL != prev_voucher) ?
	    iv_lookup(prev_voucher, key_index) :
	    save_val_index;
	ivace_lookup_values(ivac, prev_val_index,
	    previous_vals, &previous_vals_count);

	/* Call out to resource manager to get new value */
	new_value_voucher = IV_NULL;
	kr = (ivam->ivam_get_value)(
		ivam, key, command,
		previous_vals, previous_vals_count,
		content, content_size,
		&new_value, &new_flag, &new_value_voucher);
	if (KERN_SUCCESS != kr) {
		return kr;
	}

	/* TODO: value insertion from returned voucher */
	if (IV_NULL != new_value_voucher) {
		iv_release(new_value_voucher);
	}

	/*
	 * Find or create a slot in the table associated
	 * with this attribute value.  The ivac reference
	 * is transferred to a new value, or consumed if
	 * we find a matching existing value.
	 */
	val_index = ivace_reference_by_value(ivac, new_value, new_flag);
	iv_set(voucher, key_index, val_index);

	/*
	 * release saved old value from the newly forming voucher
	 * This is saved until the end to avoid churning the
	 * release logic in cases where the same value is returned
	 * as was there before.
	 */
	ivace_release(key_index, save_val_index);

	return KERN_SUCCESS;
}

/*
 *	Routine:	ipc_directly_replace_voucher_value
 *	Purpose:
 *		Replace the <voucher, key> value with the value-handle
 *		supplied directly by the attribute manager.
 *	Conditions:
 *		Nothing locked.
 *		Caller holds references on voucher.
 *		A made reference to the value-handle is donated by the caller.
 */
static kern_return_t
ipc_directly_replace_voucher_value(
	ipc_voucher_t                           voucher,
	mach_voucher_attr_key_t                 key,
	mach_voucher_attr_value_handle_t        new_value)
{
	ipc_voucher_attr_manager_t ivam;
	ipc_voucher_attr_control_t ivac;
	iv_index_t save_val_index;
	iv_index_t val_index;
	iv_index_t key_index;

	/*
	 * Get the manager for this key_index.
	 * Returns a reference on the control.
	 */
	key_index = iv_key_to_index(key);
	ivgt_lookup(key_index, &ivam, &ivac);
	if (IVAM_NULL == ivam) {
		return KERN_INVALID_ARGUMENT;
	}

	/* save the current value stored in the forming voucher */
	save_val_index = iv_lookup(voucher, key_index);

	/*
	 * Find or create a slot in the table associated
	 * with this attribute value.  The ivac reference
	 * is transferred to a new value, or consumed if
	 * we find a matching existing value.
	 */
	val_index = ivace_reference_by_value(ivac, new_value,
	    MACH_VOUCHER_ATTR_VALUE_FLAGS_NONE);
	iv_set(voucher, key_index, val_index);

	/*
	 * release saved old value from the newly forming voucher
	 * This is saved until the end to avoid churning the
	 * release logic in cases where the same value is returned
	 * as was there before.
	 */
	ivace_release(key_index, save_val_index);

	return KERN_SUCCESS;
}

static kern_return_t
ipc_execute_voucher_recipe_command(
	ipc_voucher_t                           voucher,
	mach_voucher_attr_key_t                 key,
	mach_voucher_attr_recipe_command_t      command,
	ipc_voucher_t                           prev_iv,
	mach_voucher_attr_content_t             content,
	mach_voucher_attr_content_size_t        content_size,
	boolean_t                               key_priv)
{
	iv_index_t prev_val_index;
	iv_index_t val_index;
	kern_return_t kr;

	switch (command) {
	/*
	 * MACH_VOUCHER_ATTR_COPY
	 *	Copy the attribute(s) from the previous voucher to the new
	 *	one.  A wildcard key is an acceptable value - indicating a
	 *	desire to copy all the attribute values from the previous
	 *	voucher.
	 */
	case MACH_VOUCHER_ATTR_COPY:

		/* no recipe data on a copy */
		if (0 < content_size) {
			return KERN_INVALID_ARGUMENT;
		}

		/* nothing to copy from? - done */
		if (IV_NULL == prev_iv) {
			return KERN_SUCCESS;
		}

		if (MACH_VOUCHER_ATTR_KEY_ALL == key) {
			/* wildcard matching */
			for (iv_index_t j = 0; j < MACH_VOUCHER_ATTR_KEY_NUM; j++) {
				/* release old value being replaced */
				val_index = iv_lookup(voucher, j);
				ivace_release(j, val_index);

				/* replace with reference to prev voucher's value */
				prev_val_index = iv_lookup(prev_iv, j);
				ivace_reference_by_index(j, prev_val_index);
				iv_set(voucher, j, prev_val_index);
			}
		} else {
			iv_index_t key_index;

			/* copy just one key */
			key_index = iv_key_to_index(key);
			if (MACH_VOUCHER_ATTR_KEY_NUM < key_index) {
				return KERN_INVALID_ARGUMENT;
			}

			/* release old value being replaced */
			val_index = iv_lookup(voucher, key_index);
			ivace_release(key_index, val_index);

			/* replace with reference to prev voucher's value */
			prev_val_index = iv_lookup(prev_iv, key_index);
			ivace_reference_by_index(key_index, prev_val_index);
			iv_set(voucher, key_index, prev_val_index);
		}
		break;

	/*
	 * MACH_VOUCHER_ATTR_REMOVE
	 *	Remove the attribute(s) from the under construction voucher.
	 *	A wildcard key is an acceptable value - indicating a desire
	 *	to remove all the attribute values set up so far in the voucher.
	 *	If a previous voucher is specified, only remove the value it
	 *	it matches the value in the previous voucher.
	 */
	case MACH_VOUCHER_ATTR_REMOVE:
		/* no recipe data on a remove */
		if (0 < content_size) {
			return KERN_INVALID_ARGUMENT;
		}

		if (MACH_VOUCHER_ATTR_KEY_ALL == key) {
			/* wildcard matching */
			for (iv_index_t j = 0; j < MACH_VOUCHER_ATTR_KEY_NUM; j++) {
				val_index = iv_lookup(voucher, j);

				/* If not matched in previous, skip */
				if (IV_NULL != prev_iv) {
					prev_val_index = iv_lookup(prev_iv, j);
					if (val_index != prev_val_index) {
						continue;
					}
				}
				/* release and clear */
				ivace_release(j, val_index);
				iv_set(voucher, j, IV_UNUSED_VALINDEX);
			}
		} else {
			iv_index_t key_index;

			/* copy just one key */
			key_index = iv_key_to_index(key);
			if (MACH_VOUCHER_ATTR_KEY_NUM < key_index) {
				return KERN_INVALID_ARGUMENT;
			}

			val_index = iv_lookup(voucher, key_index);

			/* If not matched in previous, skip */
			if (IV_NULL != prev_iv) {
				prev_val_index = iv_lookup(prev_iv, key_index);
				if (val_index != prev_val_index) {
					break;
				}
			}

			/* release and clear */
			ivace_release(key_index, val_index);
			iv_set(voucher, key_index, IV_UNUSED_VALINDEX);
		}
		break;

	/*
	 * MACH_VOUCHER_ATTR_SET_VALUE_HANDLE
	 *	Use key-privilege to set a value handle for the attribute directly,
	 *	rather than triggering a callback into the attribute manager to
	 *	interpret a recipe to generate the value handle.
	 */
	case MACH_VOUCHER_ATTR_SET_VALUE_HANDLE:
		if (key_priv) {
			mach_voucher_attr_value_handle_t new_value;

			if (sizeof(mach_voucher_attr_value_handle_t) != content_size) {
				return KERN_INVALID_ARGUMENT;
			}

			new_value = *(mach_voucher_attr_value_handle_t *)(void *)content;
			kr = ipc_directly_replace_voucher_value(voucher,
			    key, new_value);
			if (KERN_SUCCESS != kr) {
				return kr;
			}
		} else {
			return KERN_INVALID_CAPABILITY;
		}
		break;

	/*
	 * MACH_VOUCHER_ATTR_REDEEM
	 *	Redeem the attribute(s) from the previous voucher for a possibly
	 *	new value in the new voucher. A wildcard key is an acceptable value,
	 *	indicating a desire to redeem all the values.
	 */
	case MACH_VOUCHER_ATTR_REDEEM:

		if (MACH_VOUCHER_ATTR_KEY_ALL == key) {
			/* wildcard matching */
			for (iv_index_t j = 0; j < MACH_VOUCHER_ATTR_KEY_NUM; j++) {
				mach_voucher_attr_key_t j_key;

				j_key = iv_index_to_key(j);

				/* skip non-existent managers */
				if (MACH_VOUCHER_ATTR_KEY_NONE == j_key) {
					continue;
				}

				/* get the new value from redeem (skip empty previous) */
				kr = ipc_replace_voucher_value(voucher,
				    j_key,
				    command,
				    prev_iv,
				    content,
				    content_size);
				if (KERN_SUCCESS != kr) {
					return kr;
				}
			}
			break;
		}
		OS_FALLTHROUGH; /* fall thru for single key redemption */

	/*
	 * DEFAULT:
	 *	Replace the current value for the <voucher, key> pair with whatever
	 *	value the resource manager returns for the command and recipe
	 *	combination provided.
	 */
	default:
		kr = ipc_replace_voucher_value(voucher,
		    key,
		    command,
		    prev_iv,
		    content,
		    content_size);
		if (KERN_SUCCESS != kr) {
			return kr;
		}

		break;
	}
	return KERN_SUCCESS;
}

/*
 *	Routine:	iv_dedup
 *	Purpose:
 *		See if the set of values represented by this new voucher
 *		already exist in another voucher.  If so return a reference
 *		to the existing voucher and deallocate the voucher provided.
 *		Otherwise, insert this one in the hash and return it.
 *	Conditions:
 *		A voucher reference is donated on entry.
 *	Returns:
 *		A voucher reference (may be different than on entry).
 */
static ipc_voucher_t
iv_dedup(ipc_voucher_t new_iv)
{
	ipc_voucher_t dupe_iv;

	dupe_iv = smr_shash_get_or_insert(&voucher_table,
	    iv_key(new_iv), &new_iv->iv_hash_link, &voucher_traits);
	if (dupe_iv) {
		/* referenced previous, so deallocate the new one */
		iv_dealloc(new_iv, false);
		return dupe_iv;
	}

	/*
	 * This code is disabled for KDEBUG_LEVEL_IST and KDEBUG_LEVEL_NONE
	 */
#if (KDEBUG_LEVEL >= KDEBUG_LEVEL_STANDARD)
	if (kdebug_enable & ~KDEBUG_ENABLE_PPT) {
		uintptr_t voucher_addr = VM_KERNEL_ADDRPERM((uintptr_t)new_iv);
		uintptr_t attr_tracepoints_needed = 0;
		uint64_t ivht_count = counter_load(&voucher_table.smrsh_count);

		if (ipc_voucher_trace_contents) {
			/*
			 * voucher_contents sizing is a bit more constrained
			 * than might be obvious.
			 *
			 * This is typically a uint8_t typed array. However,
			 * we want to access it as a uintptr_t to efficiently
			 * copyout the data in tracepoints.
			 *
			 * This constrains the size to uintptr_t bytes, and
			 * adds a minimimum alignment requirement equivalent
			 * to a uintptr_t.
			 *
			 * Further constraining the size is the fact that it
			 * is copied out 4 uintptr_t chunks at a time. We do
			 * NOT want to run off the end of the array and copyout
			 * random stack data.
			 *
			 * So the minimum size is 4 * sizeof(uintptr_t), and
			 * the minimum alignment is uintptr_t aligned.
			 */

#define PAYLOAD_PER_TRACEPOINT (4 * sizeof(uintptr_t))
#define PAYLOAD_SIZE 1024

			static_assert(PAYLOAD_SIZE % PAYLOAD_PER_TRACEPOINT == 0, "size invariant violated");

			mach_voucher_attr_raw_recipe_array_size_t payload_size = PAYLOAD_SIZE;
			uintptr_t payload[PAYLOAD_SIZE / sizeof(uintptr_t)];
			kern_return_t kr;

			kr = mach_voucher_extract_all_attr_recipes(new_iv, (mach_voucher_attr_raw_recipe_array_t)payload, &payload_size);
			if (KERN_SUCCESS == kr) {
				attr_tracepoints_needed = (payload_size + PAYLOAD_PER_TRACEPOINT - 1) / PAYLOAD_PER_TRACEPOINT;

				/*
				 * To prevent leaking data from the stack, we
				 * need to zero data to the end of a tracepoint
				 * payload.
				 */
				size_t remainder = payload_size % PAYLOAD_PER_TRACEPOINT;
				if (remainder) {
					bzero((uint8_t*)payload + payload_size,
					    PAYLOAD_PER_TRACEPOINT - remainder);
				}
			}

			KDBG(MACHDBG_CODE(DBG_MACH_IPC, MACH_IPC_VOUCHER_CREATE),
			    voucher_addr, ivht_count,
			    payload_size);

			uintptr_t index = 0;
			while (attr_tracepoints_needed--) {
				KDBG(MACHDBG_CODE(DBG_MACH_IPC,
				    MACH_IPC_VOUCHER_CREATE_ATTR_DATA), payload[index],
				    payload[index + 1], payload[index + 2],
				    payload[index + 3]);
				index += 4;
			}
		} else {
			KDBG(MACHDBG_CODE(DBG_MACH_IPC, MACH_IPC_VOUCHER_CREATE),
			    voucher_addr, ivht_count);
		}
	}
#endif /* KDEBUG_LEVEL >= KDEBUG_LEVEL_STANDARD */

	return new_iv;
}

/*
 *	Routine:	ipc_create_mach_voucher_internal
 *	Purpose:
 *		Create a new mach voucher and initialize it with the
 *		value(s) created by having the appropriate resource
 *		managers interpret the supplied recipe commands and
 *		data.
 *
 *      Coming in on the attribute control port denotes special privileges
 *		over the key associated with the control port.
 *
 *      Coming in from user-space, each recipe item will have a previous
 *		recipe port name that needs to be converted to a voucher.  Because
 *		we can't rely on the port namespace to hold a reference on each
 *		previous voucher port for the duration of processing that command,
 *		we have to convert the name to a voucher reference and release it
 *		after the command processing is done.
 *
 *	Conditions:
 *		Nothing locked (may invoke user-space repeatedly).
 *		Caller holds references on previous vouchers.
 *		Previous vouchers are passed as voucher indexes.
 */
static kern_return_t
ipc_create_mach_voucher_internal(
	ipc_voucher_attr_control_t  control,
	uint8_t                     *recipes,
	size_t                      recipe_size,
	bool                        is_user_recipe,
	ipc_voucher_t               *new_voucher)
{
	mach_voucher_attr_key_t control_key = 0;
	ipc_voucher_attr_recipe_t sub_recipe_kernel;
	mach_voucher_attr_recipe_t sub_recipe_user;
	size_t recipe_struct_size = 0;
	size_t recipe_used = 0;
	ipc_voucher_t voucher;
	ipc_voucher_t prev_iv;
	bool key_priv = false;
	kern_return_t kr = KERN_SUCCESS;

	/* if nothing to do ... */
	if (0 == recipe_size) {
		*new_voucher = IV_NULL;
		return KERN_SUCCESS;
	}

	/* allocate a voucher */
	voucher = iv_alloc();
	assert(voucher != IV_NULL);

	if (IPC_VOUCHER_ATTR_CONTROL_NULL != control) {
		control_key = iv_index_to_key(control->ivac_key_index);
	}

	/*
	 * account for recipe struct size diff between user and kernel
	 * (mach_voucher_attr_recipe_t vs ipc_voucher_attr_recipe_t)
	 */
	recipe_struct_size = (is_user_recipe) ?
	    sizeof(*sub_recipe_user) :
	    sizeof(*sub_recipe_kernel);

	/* iterate over the recipe items */
	while (0 < recipe_size - recipe_used) {
		if (recipe_size - recipe_used < recipe_struct_size) {
			kr = KERN_INVALID_ARGUMENT;
			break;
		}

		if (is_user_recipe) {
			sub_recipe_user =
			    (mach_voucher_attr_recipe_t)(void *)&recipes[recipe_used];

			if (recipe_size - recipe_used - recipe_struct_size <
			    sub_recipe_user->content_size) {
				kr = KERN_INVALID_ARGUMENT;
				break;
			}

			/*
			 * convert voucher port name (current space) into a voucher
			 * reference
			 */
			prev_iv = convert_port_name_to_voucher(
				sub_recipe_user->previous_voucher);
			if (MACH_PORT_NULL != sub_recipe_user->previous_voucher &&
			    IV_NULL == prev_iv) {
				kr = KERN_INVALID_CAPABILITY;
				break;
			}

			recipe_used += recipe_struct_size + sub_recipe_user->content_size;
			key_priv =  (IPC_VOUCHER_ATTR_CONTROL_NULL != control) ?
			    (sub_recipe_user->key == control_key) :
			    false;

			kr = ipc_execute_voucher_recipe_command(voucher,
			    sub_recipe_user->key, sub_recipe_user->command, prev_iv,
			    sub_recipe_user->content, sub_recipe_user->content_size,
			    key_priv);
			ipc_voucher_release(prev_iv);
		} else {
			sub_recipe_kernel =
			    (ipc_voucher_attr_recipe_t)(void *)&recipes[recipe_used];

			if (recipe_size - recipe_used - recipe_struct_size <
			    sub_recipe_kernel->content_size) {
				kr = KERN_INVALID_ARGUMENT;
				break;
			}

			recipe_used += recipe_struct_size + sub_recipe_kernel->content_size;
			key_priv =  (IPC_VOUCHER_ATTR_CONTROL_NULL != control) ?
			    (sub_recipe_kernel->key == control_key) :
			    false;

			kr = ipc_execute_voucher_recipe_command(voucher,
			    sub_recipe_kernel->key, sub_recipe_kernel->command,
			    sub_recipe_kernel->previous_voucher, sub_recipe_kernel->content,
			    sub_recipe_kernel->content_size, key_priv);
		}

		if (KERN_SUCCESS != kr) {
			break;
		}
	}

	if (KERN_SUCCESS == kr) {
		*new_voucher = iv_dedup(voucher);
	} else {
		iv_dealloc(voucher, FALSE);
		*new_voucher = IV_NULL;
	}
	return kr;
}

/*
 *	Routine:	ipc_create_mach_voucher
 *	Purpose:
 *		Create a new mach voucher and initialize it with the
 *		value(s) created by having the appropriate resource
 *		managers interpret the supplied recipe commands and
 *		data.
 *	Conditions:
 *		Nothing locked (may invoke user-space repeatedly).
 *		Caller holds references on previous vouchers.
 *		Previous vouchers are passed as voucher indexes.
 */
kern_return_t
ipc_create_mach_voucher(
	ipc_voucher_attr_raw_recipe_array_t             recipes,
	ipc_voucher_attr_raw_recipe_array_size_t        recipe_size,
	ipc_voucher_t                                   *new_voucher)
{
	return ipc_create_mach_voucher_internal(IPC_VOUCHER_ATTR_CONTROL_NULL,
	           recipes, recipe_size, false, new_voucher);
}

/*
 *	Routine:	ipc_voucher_attr_control_create_mach_voucher
 *	Purpose:
 *		Create a new mach voucher and initialize it with the
 *		value(s) created by having the appropriate resource
 *		managers interpret the supplied recipe commands and
 *		data.
 *
 *		The resource manager control's privilege over its
 *		particular key value is reflected on to the execution
 *		code, allowing internal commands (like setting a
 *		key value handle directly, rather than having to
 *		create a recipe, that will generate a callback just
 *		to get the value.
 *
 *	Conditions:
 *		Nothing locked (may invoke user-space repeatedly).
 *		Caller holds references on previous vouchers.
 *		Previous vouchers are passed as voucher indexes.
 */
kern_return_t
ipc_voucher_attr_control_create_mach_voucher(
	ipc_voucher_attr_control_t                      control,
	ipc_voucher_attr_raw_recipe_array_t             recipes,
	ipc_voucher_attr_raw_recipe_array_size_t        recipe_size,
	ipc_voucher_t                                   *new_voucher)
{
	if (IPC_VOUCHER_ATTR_CONTROL_NULL == control) {
		return KERN_INVALID_CAPABILITY;
	}

	return ipc_create_mach_voucher_internal(control, recipes,
	           recipe_size, false, new_voucher);
}

/*
 *	ipc_register_well_known_mach_voucher_attr_manager
 *
 *	Register the resource manager responsible for a given key value.
 */
void
ipc_register_well_known_mach_voucher_attr_manager(
	ipc_voucher_attr_manager_t manager,
	mach_voucher_attr_value_handle_t default_value,
	mach_voucher_attr_key_t key,
	ipc_voucher_attr_control_t *control)
{
	ipc_voucher_attr_control_t ivac;
	iv_index_t key_index;
	iv_index_t hash_index;

	key_index = iv_key_to_index(key);

	assert(startup_phase < STARTUP_SUB_MACH_IPC);
	assert(manager);
	assert(key_index != IV_UNUSED_KEYINDEX);
	assert(ivam_global_table[key_index] == IVAM_NULL);

	ivac = ivac_init_well_known_voucher_attr_control(key_index);
	/* insert the default value into slot 0 */
	ivac->ivac_table[IV_UNUSED_VALINDEX].ivace_value = default_value;
	ivac->ivac_table[IV_UNUSED_VALINDEX].ivace_refs = IVACE_REFS_MAX;
	ivac->ivac_table[IV_UNUSED_VALINDEX].ivace_made = IVACE_REFS_MAX;
	ivac->ivac_table[IV_UNUSED_VALINDEX].ivace_persist = TRUE;

	assert(IV_HASH_END == ivac->ivac_table[IV_UNUSED_VALINDEX].ivace_next);

	/* fill in the global table slot for this key */
	os_atomic_store(&ivam_global_table[key_index], manager, release);

	/* insert the default value into the hash (in case it is returned later) */
	hash_index = iv_hash_value(ivac, default_value);
	assert(IV_HASH_END == ivac->ivac_table[hash_index].ivace_index);
	ivace_lookup(ivac, hash_index)->ivace_index = IV_UNUSED_VALINDEX;

	/* return the reference on the new cache control to the caller */
	*control = ivac;
}

/*
 *	Routine:	mach_voucher_extract_attr_content
 *	Purpose:
 *		Extract the content for a given <voucher, key> pair.
 *
 *		If a value other than the default is present for this
 *		<voucher,key> pair, we need to contact the resource
 *		manager to extract the content/meaning of the value(s)
 *		present.  Otherwise, return success (but no data).
 *
 *	Conditions:
 *		Nothing locked - as it may upcall to user-space.
 *		The caller holds a reference on the voucher.
 */
kern_return_t
mach_voucher_extract_attr_content(
	ipc_voucher_t                           voucher,
	mach_voucher_attr_key_t                 key,
	mach_voucher_attr_content_t             content,
	mach_voucher_attr_content_size_t        *in_out_size)
{
	mach_voucher_attr_value_handle_t vals[MACH_VOUCHER_ATTR_VALUE_MAX_NESTED];
	mach_voucher_attr_value_handle_array_size_t vals_count;
	mach_voucher_attr_recipe_command_t command;
	ipc_voucher_attr_manager_t manager;
	ipc_voucher_attr_control_t ivac;
	iv_index_t value_index;
	iv_index_t key_index;
	kern_return_t kr;


	if (IV_NULL == voucher) {
		return KERN_INVALID_ARGUMENT;
	}

	key_index = iv_key_to_index(key);

	value_index = iv_lookup(voucher, key_index);
	if (IV_UNUSED_VALINDEX == value_index) {
		*in_out_size = 0;
		return KERN_SUCCESS;
	}

	/*
	 * Get the manager for this key_index.  The
	 * existence of a non-default value for this
	 * slot within our voucher will keep the
	 * manager referenced during the callout.
	 */
	ivgt_lookup(key_index, &manager, &ivac);
	if (IVAM_NULL == manager) {
		return KERN_INVALID_ARGUMENT;
	}

	/*
	 * Get the value(s) to pass to the manager
	 * for this value_index.
	 */
	ivace_lookup_values(ivac, value_index,
	    vals, &vals_count);
	assert(0 < vals_count);

	/* callout to manager */

	kr = (manager->ivam_extract_content)(manager, key,
	    vals, vals_count, &command, content, in_out_size);
	return kr;
}

/*
 *	Routine:	mach_voucher_extract_attr_recipe
 *	Purpose:
 *		Extract a recipe for a given <voucher, key> pair.
 *
 *		If a value other than the default is present for this
 *		<voucher,key> pair, we need to contact the resource
 *		manager to extract the content/meaning of the value(s)
 *		present.  Otherwise, return success (but no data).
 *
 *	Conditions:
 *		Nothing locked - as it may upcall to user-space.
 *		The caller holds a reference on the voucher.
 */
kern_return_t
mach_voucher_extract_attr_recipe(
	ipc_voucher_t                           voucher,
	mach_voucher_attr_key_t                 key,
	mach_voucher_attr_raw_recipe_t          raw_recipe,
	mach_voucher_attr_raw_recipe_size_t     *in_out_size)
{
	mach_voucher_attr_value_handle_t vals[MACH_VOUCHER_ATTR_VALUE_MAX_NESTED];
	mach_voucher_attr_value_handle_array_size_t vals_count;
	ipc_voucher_attr_manager_t manager;
	ipc_voucher_attr_control_t ivac;
	mach_voucher_attr_recipe_t recipe;
	iv_index_t value_index;
	iv_index_t key_index;
	kern_return_t kr;


	if (IV_NULL == voucher) {
		return KERN_INVALID_ARGUMENT;
	}

	key_index = iv_key_to_index(key);

	value_index = iv_lookup(voucher, key_index);
	if (IV_UNUSED_VALINDEX == value_index) {
		*in_out_size = 0;
		return KERN_SUCCESS;
	}

	if (*in_out_size < sizeof(*recipe)) {
		return KERN_NO_SPACE;
	}

	recipe = (mach_voucher_attr_recipe_t)(void *)raw_recipe;
	recipe->key = key;
	recipe->command = MACH_VOUCHER_ATTR_NOOP;
	recipe->previous_voucher = MACH_VOUCHER_NAME_NULL;
	recipe->content_size = *in_out_size - sizeof(*recipe);

	/*
	 * Get the manager for this key_index.  The
	 * existence of a non-default value for this
	 * slot within our voucher will keep the
	 * manager referenced during the callout.
	 */
	ivgt_lookup(key_index, &manager, &ivac);
	if (IVAM_NULL == manager) {
		return KERN_INVALID_ARGUMENT;
	}

	/*
	 * Get the value(s) to pass to the manager
	 * for this value_index.
	 */
	ivace_lookup_values(ivac, value_index,
	    vals, &vals_count);
	assert(0 < vals_count);

	/* callout to manager */
	kr = (manager->ivam_extract_content)(manager, key,
	    vals, vals_count,
	    &recipe->command,
	    recipe->content, &recipe->content_size);
	if (KERN_SUCCESS == kr) {
		assert(*in_out_size - sizeof(*recipe) >= recipe->content_size);
		*in_out_size = sizeof(*recipe) + recipe->content_size;
	}

	return kr;
}



/*
 *	Routine:	mach_voucher_extract_all_attr_recipes
 *	Purpose:
 *		Extract all the (non-default) contents for a given voucher,
 *		building up a recipe that could be provided to a future
 *		voucher creation call.
 *	Conditions:
 *		Nothing locked (may invoke user-space).
 *		Caller holds a reference on the supplied voucher.
 */
kern_return_t
mach_voucher_extract_all_attr_recipes(
	ipc_voucher_t                                   voucher,
	mach_voucher_attr_raw_recipe_array_t            recipes,
	mach_voucher_attr_raw_recipe_array_size_t       *in_out_size)
{
	mach_voucher_attr_recipe_size_t recipe_size = *in_out_size;
	mach_voucher_attr_recipe_size_t recipe_used = 0;

	if (IV_NULL == voucher) {
		return KERN_INVALID_ARGUMENT;
	}

	for (iv_index_t key_index = 0; key_index < MACH_VOUCHER_ATTR_KEY_NUM; key_index++) {
		mach_voucher_attr_value_handle_t vals[MACH_VOUCHER_ATTR_VALUE_MAX_NESTED];
		mach_voucher_attr_value_handle_array_size_t vals_count;
		mach_voucher_attr_content_size_t content_size;
		ipc_voucher_attr_manager_t manager;
		ipc_voucher_attr_control_t ivac;
		mach_voucher_attr_recipe_t recipe;
		mach_voucher_attr_key_t key;
		iv_index_t value_index;
		kern_return_t kr;

		/* don't output anything for a default value */
		value_index = iv_lookup(voucher, key_index);
		if (IV_UNUSED_VALINDEX == value_index) {
			continue;
		}

		if (recipe_size - recipe_used < sizeof(*recipe)) {
			return KERN_NO_SPACE;
		}

		/*
		 * Get the manager for this key_index.  The
		 * existence of a non-default value for this
		 * slot within our voucher will keep the
		 * manager referenced during the callout.
		 */
		ivgt_lookup(key_index, &manager, &ivac);
		assert(IVAM_NULL != manager);
		if (IVAM_NULL == manager) {
			continue;
		}

		recipe = (mach_voucher_attr_recipe_t)(void *)&recipes[recipe_used];
		if (os_sub3_overflow(recipe_size, recipe_used, sizeof(*recipe), &content_size)) {
			panic("voucher recipe underfow");
		}

		/*
		 * Get the value(s) to pass to the manager
		 * for this value_index.
		 */
		ivace_lookup_values(ivac, value_index,
		    vals, &vals_count);
		assert(0 < vals_count);

		key = iv_index_to_key(key_index);

		recipe->key = key;
		recipe->command = MACH_VOUCHER_ATTR_NOOP;
		recipe->content_size = content_size;

		/* callout to manager */
		kr = (manager->ivam_extract_content)(manager, key,
		    vals, vals_count,
		    &recipe->command,
		    recipe->content, &recipe->content_size);
		if (KERN_SUCCESS != kr) {
			return kr;
		}

		assert(recipe->content_size <= content_size);
		recipe_used += sizeof(*recipe) + recipe->content_size;
	}

	*in_out_size = recipe_used;
	return KERN_SUCCESS;
}

/*
 *	Routine:	mach_voucher_debug_info
 *	Purpose:
 *		Extract all the (non-default) contents for a given mach port name,
 *		building up a recipe that could be provided to a future
 *		voucher creation call.
 *	Conditions:
 *		Nothing locked (may invoke user-space).
 *		Caller may not hold a reference on the supplied voucher.
 */
#if !(DEVELOPMENT || DEBUG)
kern_return_t
mach_voucher_debug_info(
	ipc_space_t                                     __unused space,
	mach_port_name_t                                __unused voucher_name,
	mach_voucher_attr_raw_recipe_array_t            __unused recipes,
	mach_voucher_attr_raw_recipe_array_size_t       __unused *in_out_size)
{
	return KERN_NOT_SUPPORTED;
}
#else
kern_return_t
mach_voucher_debug_info(
	ipc_space_t                                     space,
	mach_port_name_t                                voucher_name,
	mach_voucher_attr_raw_recipe_array_t            recipes,
	mach_voucher_attr_raw_recipe_array_size_t       *in_out_size)
{
	ipc_voucher_t voucher = IPC_VOUCHER_NULL;
	kern_return_t kr;
	ipc_port_t port = MACH_PORT_NULL;

	if (space == IS_NULL) {
		return KERN_INVALID_TASK;
	}

	if (!MACH_PORT_VALID(voucher_name)) {
		return KERN_INVALID_ARGUMENT;
	}

	kr = ipc_port_translate_send(space, voucher_name, &port);
	if (KERN_SUCCESS != kr) {
		return KERN_INVALID_ARGUMENT;
	}

	voucher = convert_port_to_voucher(port);
	ip_mq_unlock(port);

	if (voucher) {
		kr = mach_voucher_extract_all_attr_recipes(voucher, recipes, in_out_size);
		ipc_voucher_release(voucher);
		return kr;
	}

	return KERN_FAILURE;
}
#endif

/*
 *	Routine:	mach_voucher_attr_command
 *	Purpose:
 *		Invoke an attribute-specific command through this voucher.
 *
 *		The voucher layout, membership, etc... is not altered
 *		through the execution of this command.
 *
 *	Conditions:
 *		Nothing locked - as it may upcall to user-space.
 *		The caller holds a reference on the voucher.
 */
kern_return_t
mach_voucher_attr_command(
	ipc_voucher_t                                           voucher,
	mach_voucher_attr_key_t                         key,
	mach_voucher_attr_command_t                     command,
	mach_voucher_attr_content_t                     in_content,
	mach_voucher_attr_content_size_t        in_content_size,
	mach_voucher_attr_content_t                     out_content,
	mach_voucher_attr_content_size_t        *out_content_size)
{
	mach_voucher_attr_value_handle_t vals[MACH_VOUCHER_ATTR_VALUE_MAX_NESTED];
	mach_voucher_attr_value_handle_array_size_t vals_count;
	ipc_voucher_attr_manager_t manager;
	ipc_voucher_attr_control_t control;
	iv_index_t value_index;
	iv_index_t key_index;
	kern_return_t kr;


	if (IV_NULL == voucher) {
		return KERN_INVALID_ARGUMENT;
	}

	key_index = iv_key_to_index(key);

	/*
	 * Get the manager for this key_index.
	 * Allowing commands against the default value
	 * for an attribute means that we have to hold
	 * reference on the attribute manager control
	 * to keep the manager around during the command
	 * execution.
	 */
	ivgt_lookup(key_index, &manager, &control);
	if (IVAM_NULL == manager) {
		return KERN_INVALID_ARGUMENT;
	}

	/*
	 * Get the values for this <voucher, key> pair
	 * to pass to the attribute manager.  It is still
	 * permissible to execute a command against the
	 * default value (empty value array).
	 */
	value_index = iv_lookup(voucher, key_index);
	ivace_lookup_values(control, value_index,
	    vals, &vals_count);

	/* callout to manager */
	kr = (manager->ivam_command)(manager, key,
	    vals, vals_count,
	    command,
	    in_content, in_content_size,
	    out_content, out_content_size);

	return kr;
}

/*
 *	Routine:	mach_voucher_attr_control_get_values
 *	Purpose:
 *		For a given voucher, get the value handle associated with the
 *		specified attribute manager.
 */
kern_return_t
mach_voucher_attr_control_get_values(
	ipc_voucher_attr_control_t control,
	ipc_voucher_t voucher,
	mach_voucher_attr_value_handle_array_t out_values,
	mach_voucher_attr_value_handle_array_size_t *in_out_size)
{
	iv_index_t value_index;

	if (IPC_VOUCHER_ATTR_CONTROL_NULL == control) {
		return KERN_INVALID_CAPABILITY;
	}

	if (IV_NULL == voucher) {
		return KERN_INVALID_ARGUMENT;
	}

	if (0 == *in_out_size) {
		return KERN_SUCCESS;
	}

	assert(os_ref_get_count_raw(&voucher->iv_refs) > 0);
	value_index = iv_lookup(voucher, control->ivac_key_index);
	ivace_lookup_values(control, value_index,
	    out_values, in_out_size);
	return KERN_SUCCESS;
}

/*
 *	Routine:	host_create_mach_voucher
 *	Purpose:
 *		Create a new mach voucher and initialize it by processing the
 *		supplied recipe(s).
 */
kern_return_t
host_create_mach_voucher(
	host_t host,
	mach_voucher_attr_raw_recipe_array_t recipes,
	mach_voucher_attr_raw_recipe_size_t recipe_size,
	ipc_voucher_t *new_voucher)
{
	if (host == HOST_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	return ipc_create_mach_voucher_internal(IPC_VOUCHER_ATTR_CONTROL_NULL,
	           recipes, recipe_size, true, new_voucher);
}

#if CONFIG_VOUCHER_DEPRECATED
/*
 *	Routine:	ipc_get_pthpriority_from_kmsg_voucher
 *	Purpose:
 *		Get the canonicalized pthread priority from the voucher attached in the kmsg.
 */
kern_return_t
ipc_get_pthpriority_from_kmsg_voucher(
	ipc_kmsg_t kmsg,
	ipc_pthread_priority_value_t *canonicalize_priority_value)
{
	mach_port_t voucher_port;
	ipc_voucher_t pthread_priority_voucher;
	uint8_t content_data[sizeof(mach_voucher_attr_recipe_data_t) +
	sizeof(ipc_pthread_priority_value_t)];
	mach_voucher_attr_raw_recipe_size_t content_size = sizeof(content_data);
	mach_voucher_attr_recipe_t cur_content;

	kern_return_t kr = KERN_SUCCESS;

	voucher_port = ipc_kmsg_get_voucher_port(kmsg);
	if (!IP_VALID(voucher_port)) {
		return KERN_FAILURE;
	}

	pthread_priority_voucher = ip_get_voucher(voucher_port);
	kr = mach_voucher_extract_attr_recipe(pthread_priority_voucher,
	    MACH_VOUCHER_ATTR_KEY_PTHPRIORITY,
	    content_data,
	    &content_size);
	if (kr != KERN_SUCCESS) {
		return kr;
	}

	/* return KERN_INVALID_VALUE for default value */
	if (content_size < sizeof(mach_voucher_attr_recipe_t)) {
		return KERN_INVALID_VALUE;
	}

	cur_content = (mach_voucher_attr_recipe_t) (void *) &content_data[0];
	assert(cur_content->content_size == sizeof(ipc_pthread_priority_value_t));
	memcpy(canonicalize_priority_value, cur_content->content, sizeof(ipc_pthread_priority_value_t));

	return KERN_SUCCESS;
}
#endif /* CONFIG_VOUCHER_DEPRECATED */

/*
 *	Routine:	ipc_voucher_get_default_voucher
 *	Purpose:
 *		Creates process default voucher and returns it.
 */
ipc_voucher_t
ipc_voucher_get_default_voucher(void)
{
	uint8_t recipes[sizeof(ipc_voucher_attr_recipe_data_t)];
	ipc_voucher_attr_recipe_t recipe;
	ipc_voucher_attr_raw_recipe_array_size_t recipe_size = sizeof(ipc_voucher_attr_recipe_data_t);
	kern_return_t kr;
	ipc_voucher_t recv_voucher = IPC_VOUCHER_NULL;
	task_t task = current_task();

	if (task == kernel_task || task->bank_context == NULL) {
		return IPC_VOUCHER_NULL;
	}

	recipe = (ipc_voucher_attr_recipe_t)(void *)&recipes[0];
	recipe->key = MACH_VOUCHER_ATTR_KEY_BANK;
	recipe->command = MACH_VOUCHER_ATTR_BANK_CREATE;
	recipe->previous_voucher = IPC_VOUCHER_NULL;
	recipe->content_size = 0;

	kr = ipc_create_mach_voucher(recipes,
	    recipe_size,
	    &recv_voucher);
	assert(KERN_SUCCESS == kr);

	return recv_voucher;
}

/*
 *	Routine:	ipc_voucher_send_preprocessing
 *	Purpose:
 *		Processing of the voucher in the kmsg before sending it.
 *		Currently use to switch PERSONA_TOKEN in case of process with
 *		no com.apple.private.personas.propagate entitlement.
 */
void
ipc_voucher_send_preprocessing(ipc_kmsg_t kmsg)
{
	uint8_t recipes[(MACH_VOUCHER_ATTR_KEY_NUM + 1) * sizeof(ipc_voucher_attr_recipe_data_t)];
	ipc_voucher_attr_raw_recipe_array_size_t recipe_size = (MACH_VOUCHER_ATTR_KEY_NUM + 1) *
	    sizeof(ipc_voucher_attr_recipe_data_t);
	ipc_voucher_t pre_processed_voucher;
	ipc_voucher_t voucher_to_send;
	ipc_port_t voucher_port;
	kern_return_t kr;
	int need_preprocessing = FALSE;

	voucher_port = ipc_kmsg_get_voucher_port(kmsg);
	if (!IP_VALID(voucher_port) || current_task() == kernel_task) {
		return;
	}

	/* setup recipe for preprocessing of all the attributes. */
	pre_processed_voucher = ip_get_voucher(voucher_port);

	kr = ipc_voucher_prepare_processing_recipe(pre_processed_voucher,
	    (mach_voucher_attr_raw_recipe_array_t)recipes,
	    &recipe_size, MACH_VOUCHER_ATTR_SEND_PREPROCESS,
	    IVAM_FLAGS_SUPPORT_SEND_PREPROCESS, &need_preprocessing);

	assert(KERN_SUCCESS == kr);
	/*
	 * Only do send preprocessing if the voucher needs any pre processing.
	 * Replace the voucher port in the kmsg, but preserve the original type.
	 */
	if (need_preprocessing) {
		kr = ipc_create_mach_voucher(recipes,
		    recipe_size,
		    &voucher_to_send);
		assert(KERN_SUCCESS == kr);
		ipc_port_release_send(voucher_port);
		voucher_port = convert_voucher_to_port(voucher_to_send);
		ipc_kmsg_set_voucher_port(kmsg, voucher_port, kmsg->ikm_voucher_type);
	}
}

/*
 *	Routine:	ipc_voucher_receive_postprocessing
 *	Purpose:
 *		Redeems the voucher attached to the kmsg.
 *	Note:
 *		Although it is possible to call ipc_importance_receive
 *		here, it is called in mach_msg_receive_results and not here
 *		in order to maintain symmetry with ipc_voucher_send_preprocessing.
 */
void
ipc_voucher_receive_postprocessing(
	ipc_kmsg_t              kmsg,
	mach_msg_option64_t     option)
{
	uint8_t recipes[(MACH_VOUCHER_ATTR_KEY_NUM + 1) * sizeof(ipc_voucher_attr_recipe_data_t)];
	ipc_voucher_attr_raw_recipe_array_size_t recipe_size = (MACH_VOUCHER_ATTR_KEY_NUM + 1) *
	    sizeof(ipc_voucher_attr_recipe_data_t);
	ipc_voucher_t recv_voucher;
	ipc_voucher_t sent_voucher;
	ipc_port_t voucher_port;
	kern_return_t kr;
	int need_postprocessing = FALSE;

	voucher_port = ipc_kmsg_get_voucher_port(kmsg);
	if ((option & MACH_RCV_VOUCHER) == 0 || (!IP_VALID(voucher_port)) ||
	    current_task() == kernel_task) {
		return;
	}

	/* setup recipe for auto redeem of all the attributes. */
	sent_voucher = ip_get_voucher(voucher_port);

	kr = ipc_voucher_prepare_processing_recipe(sent_voucher,
	    (mach_voucher_attr_raw_recipe_array_t)recipes,
	    &recipe_size, MACH_VOUCHER_ATTR_AUTO_REDEEM,
	    IVAM_FLAGS_SUPPORT_RECEIVE_POSTPROCESS, &need_postprocessing);

	assert(KERN_SUCCESS == kr);

	/*
	 * Only do receive postprocessing if the voucher needs any post processing.
	 */
	if (need_postprocessing) {
		kr = ipc_create_mach_voucher(recipes,
		    recipe_size,
		    &recv_voucher);
		assert(KERN_SUCCESS == kr);
		/* swap the voucher port (and set voucher bits in case it didn't already exist) */
		ikm_header(kmsg)->msgh_bits |= (MACH_MSG_TYPE_MOVE_SEND << 16);
		ipc_port_release_send(voucher_port);
		voucher_port = convert_voucher_to_port(recv_voucher);
		ipc_kmsg_set_voucher_port(kmsg, voucher_port, MACH_MSG_TYPE_MOVE_SEND);
	}
}

/*
 *	Routine:	ipc_voucher_prepare_processing_recipe
 *	Purpose:
 *		Check if the given voucher has an attribute which supports
 *		the given flag and prepare a recipe to apply that supported
 *		command.
 */
static kern_return_t
ipc_voucher_prepare_processing_recipe(
	ipc_voucher_t voucher,
	ipc_voucher_attr_raw_recipe_array_t recipes,
	ipc_voucher_attr_raw_recipe_array_size_t *in_out_size,
	mach_voucher_attr_recipe_command_t command,
	ipc_voucher_attr_manager_flags flags,
	int *need_processing)
{
	ipc_voucher_attr_raw_recipe_array_size_t recipe_size = *in_out_size;
	ipc_voucher_attr_raw_recipe_array_size_t recipe_used = 0;
	ipc_voucher_attr_recipe_t recipe;

	if (IV_NULL == voucher) {
		return KERN_INVALID_ARGUMENT;
	}

	/* Setup a recipe to copy all attributes. */
	if (recipe_size < sizeof(*recipe)) {
		return KERN_NO_SPACE;
	}

	*need_processing = FALSE;
	recipe = (ipc_voucher_attr_recipe_t)(void *)&recipes[recipe_used];
	recipe->key = MACH_VOUCHER_ATTR_KEY_ALL;
	recipe->command = MACH_VOUCHER_ATTR_COPY;
	recipe->previous_voucher = voucher;
	recipe->content_size = 0;
	recipe_used += sizeof(*recipe) + recipe->content_size;

	for (iv_index_t key_index = 0; key_index < MACH_VOUCHER_ATTR_KEY_NUM; key_index++) {
		ipc_voucher_attr_manager_t manager;
		mach_voucher_attr_key_t key;
		iv_index_t value_index;

		/* don't output anything for a default value */
		value_index = iv_lookup(voucher, key_index);
		if (IV_UNUSED_VALINDEX == value_index) {
			continue;
		}

		if (recipe_size - recipe_used < sizeof(*recipe)) {
			return KERN_NO_SPACE;
		}

		recipe = (ipc_voucher_attr_recipe_t)(void *)&recipes[recipe_used];

		/*
		 * Get the manager for this key_index. The
		 * existence of a non-default value for this
		 * slot within our voucher will keep the
		 * manager referenced during the callout.
		 */
		ivgt_lookup(key_index, &manager, NULL);
		assert(IVAM_NULL != manager);
		if (IVAM_NULL == manager) {
			continue;
		}

		/* Check if the supported flag is set in the manager */
		if ((manager->ivam_flags & flags) == 0) {
			continue;
		}

		key = iv_index_to_key(key_index);

		recipe->key = key;
		recipe->command = command;
		recipe->content_size = 0;
		recipe->previous_voucher = voucher;

		recipe_used += sizeof(*recipe) + recipe->content_size;
		*need_processing = TRUE;
	}

	*in_out_size = recipe_used;
	return KERN_SUCCESS;
}

/*
 * Activity id Generation
 */
uint64_t voucher_activity_id;

#define generate_activity_id(x) \
	((uint64_t)OSAddAtomic64((x), (int64_t *)&voucher_activity_id))

/*
 *	Routine:	mach_init_activity_id
 *	Purpose:
 *		Initialize voucher activity id.
 */
void
mach_init_activity_id(void)
{
	voucher_activity_id = 1;
}

/*
 *	Routine:	mach_generate_activity_id
 *	Purpose:
 *		Generate a system wide voucher activity id.
 */
kern_return_t
mach_generate_activity_id(
	struct mach_generate_activity_id_args *args)
{
	uint64_t activity_id;
	kern_return_t kr = KERN_SUCCESS;

	if (args->count <= 0 || args->count > MACH_ACTIVITY_ID_COUNT_MAX) {
		return KERN_INVALID_ARGUMENT;
	}

	activity_id = generate_activity_id(args->count);
	kr = copyout(&activity_id, args->activity_id, sizeof(activity_id));

	return kr;
}

/* User data manager is removed on !macOS */
#if CONFIG_VOUCHER_DEPRECATED
#if defined(MACH_VOUCHER_ATTR_KEY_USER_DATA) || defined(MACH_VOUCHER_ATTR_KEY_TEST)

/*
 * Build-in a simple User Data Resource Manager
 */
#define USER_DATA_MAX_DATA      (16*1024)

struct user_data_value_element {
	mach_voucher_attr_value_reference_t     e_made;
	mach_voucher_attr_content_size_t        e_size;
	iv_index_t                              e_sum;
	iv_index_t                              e_hash;
	queue_chain_t                           e_hash_link;
	uint8_t                                *e_data;
};

typedef struct user_data_value_element *user_data_element_t;

/*
 * User Data Voucher Hash Table
 */
#define USER_DATA_HASH_BUCKETS 127
#define USER_DATA_HASH_BUCKET(x) ((x) % USER_DATA_HASH_BUCKETS)

static queue_head_t user_data_bucket[USER_DATA_HASH_BUCKETS];
static LCK_SPIN_DECLARE_ATTR(user_data_lock_data, &ipc_lck_grp, &ipc_lck_attr);

#define user_data_lock_destroy() \
	lck_spin_destroy(&user_data_lock_data, &ipc_lck_grp)
#define user_data_lock() \
	lck_spin_lock_grp(&user_data_lock_data, &ipc_lck_grp)
#define user_data_lock_try() \
	lck_spin_try_lock_grp(&user_data_lock_data, &ipc_lck_grp)
#define user_data_unlock() \
	lck_spin_unlock(&user_data_lock_data)

static kern_return_t
user_data_release_value(
	ipc_voucher_attr_manager_t              manager,
	mach_voucher_attr_key_t                 key,
	mach_voucher_attr_value_handle_t        value,
	mach_voucher_attr_value_reference_t     sync);

static kern_return_t
user_data_get_value(
	ipc_voucher_attr_manager_t                      manager,
	mach_voucher_attr_key_t                         key,
	mach_voucher_attr_recipe_command_t              command,
	mach_voucher_attr_value_handle_array_t          prev_values,
	mach_voucher_attr_value_handle_array_size_t     prev_value_count,
	mach_voucher_attr_content_t                     content,
	mach_voucher_attr_content_size_t                content_size,
	mach_voucher_attr_value_handle_t                *out_value,
	mach_voucher_attr_value_flags_t                 *out_flags,
	ipc_voucher_t                                   *out_value_voucher);

static kern_return_t
user_data_extract_content(
	ipc_voucher_attr_manager_t                      manager,
	mach_voucher_attr_key_t                         key,
	mach_voucher_attr_value_handle_array_t          values,
	mach_voucher_attr_value_handle_array_size_t     value_count,
	mach_voucher_attr_recipe_command_t              *out_command,
	mach_voucher_attr_content_t                     out_content,
	mach_voucher_attr_content_size_t                *in_out_content_size);

static kern_return_t
user_data_command(
	ipc_voucher_attr_manager_t                              manager,
	mach_voucher_attr_key_t                                 key,
	mach_voucher_attr_value_handle_array_t  values,
	mach_msg_type_number_t                                  value_count,
	mach_voucher_attr_command_t                             command,
	mach_voucher_attr_content_t                             in_content,
	mach_voucher_attr_content_size_t                in_content_size,
	mach_voucher_attr_content_t                             out_content,
	mach_voucher_attr_content_size_t                *out_content_size);

const struct ipc_voucher_attr_manager user_data_manager = {
	.ivam_release_value =   user_data_release_value,
	.ivam_get_value =       user_data_get_value,
	.ivam_extract_content = user_data_extract_content,
	.ivam_command =         user_data_command,
	.ivam_flags =           IVAM_FLAGS_NONE,
};

ipc_voucher_attr_control_t user_data_control;
ipc_voucher_attr_control_t test_control;

#if defined(MACH_VOUCHER_ATTR_KEY_USER_DATA) && defined(MACH_VOUCHER_ATTR_KEY_TEST)
#define USER_DATA_ASSERT_KEY(key)                               \
	assert(MACH_VOUCHER_ATTR_KEY_USER_DATA == (key) ||      \
	       MACH_VOUCHER_ATTR_KEY_TEST == (key));
#elif defined(MACH_VOUCHER_ATTR_KEY_USER_DATA)
#define USER_DATA_ASSERT_KEY(key) assert(MACH_VOUCHER_ATTR_KEY_USER_DATA == (key))
#else
#define USER_DATA_ASSERT_KEY(key) assert(MACH_VOUCHER_ATTR_KEY_TEST == (key))
#endif

static void
user_data_value_element_free(user_data_element_t elem)
{
	kfree_data(elem->e_data, elem->e_size);
	kfree_type(struct user_data_value_element, elem);
}

/*
 *	Routine:	user_data_release_value
 *	Purpose:
 *		Release a made reference on a specific value managed by
 *		this voucher attribute manager.
 *	Conditions:
 *		Must remove the element associated with this value from
 *		the hash if this is the last know made reference.
 */
static kern_return_t
user_data_release_value(
	ipc_voucher_attr_manager_t              __assert_only manager,
	mach_voucher_attr_key_t                 __assert_only key,
	mach_voucher_attr_value_handle_t        value,
	mach_voucher_attr_value_reference_t     sync)
{
	user_data_element_t elem;
	iv_index_t hash;

	assert(&user_data_manager == manager);
	USER_DATA_ASSERT_KEY(key);

	elem = (user_data_element_t)value;
	hash = elem->e_hash;

	user_data_lock();
	if (sync == elem->e_made) {
		queue_remove(&user_data_bucket[hash], elem, user_data_element_t, e_hash_link);
		user_data_unlock();
		user_data_value_element_free(elem);
		return KERN_SUCCESS;
	}
	assert(sync < elem->e_made);
	user_data_unlock();

	return KERN_FAILURE;
}

/*
 *	Routine:	user_data_checksum
 *	Purpose:
 *		Provide a rudimentary checksum for the data presented
 *		to these voucher attribute managers.
 */
static iv_index_t
user_data_checksum(
	mach_voucher_attr_content_t                     content,
	mach_voucher_attr_content_size_t                content_size)
{
	mach_voucher_attr_content_size_t i;
	iv_index_t cksum = 0;

	for (i = 0; i < content_size; i++, content++) {
		cksum = (cksum << 8) ^ (cksum + *(unsigned char *)content);
	}

	return ~cksum;
}

/*
 *	Routine:	user_data_dedup
 *	Purpose:
 *		See if the content represented by this request already exists
 *		in another user data element.  If so return a made reference
 *		to the existing element.  Otherwise, create a new element and
 *		return that (after inserting it in the hash).
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		A made reference on the user_data_element_t
 */
static user_data_element_t
user_data_dedup(
	mach_voucher_attr_content_t                     content,
	mach_voucher_attr_content_size_t                content_size)
{
	iv_index_t sum;
	iv_index_t hash;
	user_data_element_t elem;
	user_data_element_t alloc = NULL;

	sum = user_data_checksum(content, content_size);
	hash = USER_DATA_HASH_BUCKET(sum);

retry:
	user_data_lock();
	queue_iterate(&user_data_bucket[hash], elem, user_data_element_t, e_hash_link) {
		assert(elem->e_hash == hash);

		/* if sums match... */
		if (elem->e_sum == sum && elem->e_size == content_size) {
			iv_index_t i;

			/* and all data matches */
			for (i = 0; i < content_size; i++) {
				if (elem->e_data[i] != content[i]) {
					break;
				}
			}
			if (i < content_size) {
				continue;
			}

			/* ... we found a match... */

			elem->e_made++;
			user_data_unlock();

			if (NULL != alloc) {
				user_data_value_element_free(alloc);
			}

			return elem;
		}
	}

	if (NULL == alloc) {
		user_data_unlock();

		alloc = kalloc_type(struct user_data_value_element,
		    Z_WAITOK | Z_NOFAIL);
		alloc->e_made = 1;
		alloc->e_size = content_size;
		alloc->e_sum = sum;
		alloc->e_hash = hash;
		alloc->e_data = kalloc_data(content_size, Z_WAITOK | Z_NOFAIL);
		memcpy(alloc->e_data, content, content_size);
		goto retry;
	}

	queue_enter(&user_data_bucket[hash], alloc, user_data_element_t, e_hash_link);
	user_data_unlock();

	return alloc;
}

static kern_return_t
user_data_get_value(
	ipc_voucher_attr_manager_t                      __assert_only manager,
	mach_voucher_attr_key_t                         __assert_only key,
	mach_voucher_attr_recipe_command_t              command,
	mach_voucher_attr_value_handle_array_t          prev_values,
	mach_voucher_attr_value_handle_array_size_t     prev_value_count,
	mach_voucher_attr_content_t                     content,
	mach_voucher_attr_content_size_t                content_size,
	mach_voucher_attr_value_handle_t                *out_value,
	mach_voucher_attr_value_flags_t                 *out_flags,
	ipc_voucher_t                                   *out_value_voucher)
{
	user_data_element_t elem;

	assert(&user_data_manager == manager);
	USER_DATA_ASSERT_KEY(key);

	/* never an out voucher */
	*out_value_voucher = IPC_VOUCHER_NULL;
	*out_flags = MACH_VOUCHER_ATTR_VALUE_FLAGS_NONE;

	switch (command) {
	case MACH_VOUCHER_ATTR_REDEEM:

		/* redeem of previous values is the value */
		if (0 < prev_value_count) {
			elem = (user_data_element_t)prev_values[0];

			user_data_lock();
			assert(0 < elem->e_made);
			elem->e_made++;
			user_data_unlock();

			*out_value = (mach_voucher_attr_value_handle_t)elem;
			return KERN_SUCCESS;
		}

		/* redeem of default is default */
		*out_value = 0;
		return KERN_SUCCESS;

	case MACH_VOUCHER_ATTR_USER_DATA_STORE:
		if (USER_DATA_MAX_DATA < content_size) {
			return KERN_RESOURCE_SHORTAGE;
		}

		/* empty is the default */
		if (0 == content_size) {
			*out_value = 0;
			return KERN_SUCCESS;
		}

		elem = user_data_dedup(content, content_size);
		*out_value = (mach_voucher_attr_value_handle_t)elem;
		return KERN_SUCCESS;

	default:
		/* every other command is unknown */
		return KERN_INVALID_ARGUMENT;
	}
}

static kern_return_t
user_data_extract_content(
	ipc_voucher_attr_manager_t                      __assert_only manager,
	mach_voucher_attr_key_t                         __assert_only key,
	mach_voucher_attr_value_handle_array_t          values,
	mach_voucher_attr_value_handle_array_size_t     value_count,
	mach_voucher_attr_recipe_command_t              *out_command,
	mach_voucher_attr_content_t                     out_content,
	mach_voucher_attr_content_size_t                *in_out_content_size)
{
	mach_voucher_attr_content_size_t size = 0;
	user_data_element_t elem;
	unsigned int i;

	assert(&user_data_manager == manager);
	USER_DATA_ASSERT_KEY(key);

	/* concatenate the stored data items */
	for (i = 0; i < value_count && *in_out_content_size > 0; i++) {
		elem = (user_data_element_t)values[i];
		assert(USER_DATA_MAX_DATA >= elem->e_size);

		if (size + elem->e_size > *in_out_content_size) {
			return KERN_NO_SPACE;
		}

		memcpy(&out_content[size], elem->e_data, elem->e_size);
		size += elem->e_size;
	}
	*out_command = MACH_VOUCHER_ATTR_BITS_STORE;
	*in_out_content_size = size;
	return KERN_SUCCESS;
}

static kern_return_t
user_data_command(
	ipc_voucher_attr_manager_t                              __assert_only manager,
	mach_voucher_attr_key_t                                 __assert_only key,
	mach_voucher_attr_value_handle_array_t  __unused values,
	mach_msg_type_number_t                                  __unused value_count,
	mach_voucher_attr_command_t                             __unused command,
	mach_voucher_attr_content_t                             __unused in_content,
	mach_voucher_attr_content_size_t                __unused in_content_size,
	mach_voucher_attr_content_t                             __unused out_content,
	mach_voucher_attr_content_size_t                __unused *out_content_size)
{
	assert(&user_data_manager == manager);
	USER_DATA_ASSERT_KEY(key);
	return KERN_FAILURE;
}

__startup_func
static void
user_data_attr_manager_init(void)
{
#if defined(MACH_VOUCHER_ATTR_KEY_USER_DATA)
	ipc_register_well_known_mach_voucher_attr_manager(&user_data_manager,
	    (mach_voucher_attr_value_handle_t)0,
	    MACH_VOUCHER_ATTR_KEY_USER_DATA,
	    &user_data_control);
#endif
#if defined(MACH_VOUCHER_ATTR_KEY_TEST)
	ipc_register_well_known_mach_voucher_attr_manager(&user_data_manager,
	    (mach_voucher_attr_value_handle_t)0,
	    MACH_VOUCHER_ATTR_KEY_TEST,
	    &test_control);
#endif
#if defined(MACH_VOUCHER_ATTR_KEY_USER_DATA) || defined(MACH_VOUCHER_ATTR_KEY_TEST)
	for (int i = 0; i < USER_DATA_HASH_BUCKETS; i++) {
		queue_init(&user_data_bucket[i]);
	}
#endif
}
STARTUP(MACH_IPC, STARTUP_RANK_FIRST, user_data_attr_manager_init);

#endif /* MACH_VOUCHER_ATTR_KEY_USER_DATA || MACH_VOUCHER_ATTR_KEY_TEST */
#endif /* CONFIG_VOUCHER_DEPRECATED */
