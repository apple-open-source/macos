/*
 * Copyright (c) 2008-2018 Apple Inc. All rights reserved.
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
#if !FS_COMPRESSION

/* We need these symbols even though compression is turned off */

#define UNUSED_SYMBOL(x)        asm(".global _" #x "\n.set _" #x ", 0\n");

UNUSED_SYMBOL(register_decmpfs_decompressor)
UNUSED_SYMBOL(unregister_decmpfs_decompressor)
UNUSED_SYMBOL(decmpfs_init)
UNUSED_SYMBOL(decmpfs_read_compressed)
UNUSED_SYMBOL(decmpfs_cnode_cmp_type)
UNUSED_SYMBOL(decmpfs_cnode_get_vnode_state)
UNUSED_SYMBOL(decmpfs_cnode_get_vnode_cached_size)
UNUSED_SYMBOL(decmpfs_cnode_get_vnode_cached_nchildren)
UNUSED_SYMBOL(decmpfs_cnode_get_vnode_cached_total_size)
UNUSED_SYMBOL(decmpfs_lock_compressed_data)
UNUSED_SYMBOL(decmpfs_cnode_free)
UNUSED_SYMBOL(decmpfs_cnode_alloc)
UNUSED_SYMBOL(decmpfs_cnode_destroy)
UNUSED_SYMBOL(decmpfs_decompress_file)
UNUSED_SYMBOL(decmpfs_unlock_compressed_data)
UNUSED_SYMBOL(decmpfs_cnode_init)
UNUSED_SYMBOL(decmpfs_cnode_set_vnode_state)
UNUSED_SYMBOL(decmpfs_hides_xattr)
UNUSED_SYMBOL(decmpfs_ctx)
UNUSED_SYMBOL(decmpfs_file_is_compressed)
UNUSED_SYMBOL(decmpfs_update_attributes)
UNUSED_SYMBOL(decmpfs_hides_rsrc)
UNUSED_SYMBOL(decmpfs_pagein_compressed)
UNUSED_SYMBOL(decmpfs_validate_compressed_file)

#else /* FS_COMPRESSION */
#include <sys/kernel.h>
#include <sys/vnode_internal.h>
#include <sys/file_internal.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/xattr.h>
#include <sys/namei.h>
#include <sys/user.h>
#include <sys/mount_internal.h>
#include <sys/ubc.h>
#include <sys/decmpfs.h>
#include <sys/uio_internal.h>
#include <libkern/OSByteOrder.h>
#include <libkern/section_keywords.h>
#include <sys/fsctl.h>

#include <sys/kdebug_triage.h>

#include <ptrauth.h>

#pragma mark --- debugging ---

#define COMPRESSION_DEBUG 0
#define COMPRESSION_DEBUG_VERBOSE 0
#define MALLOC_DEBUG 0

#if COMPRESSION_DEBUG
static char*
vnpath(vnode_t vp, char *path, int len)
{
	int origlen = len;
	path[0] = 0;
	vn_getpath(vp, path, &len);
	path[origlen - 1] = 0;
	return path;
}
#endif

#define ErrorLog(x, args...) \
	printf("%s:%d:%s: " x, __FILE_NAME__, __LINE__, __FUNCTION__, ## args)
#if COMPRESSION_DEBUG
#define ErrorLogWithPath(x, args...) do { \
	char *path = zalloc(ZV_NAMEI); \
	printf("%s:%d:%s: %s: " x, __FILE_NAME__, __LINE__, __FUNCTION__, \
	    vnpath(vp, path, PATH_MAX), ## args); \
	zfree(ZV_NAMEI, path); \
} while(0)
#else
#define ErrorLogWithPath(x, args...) do { \
	(void*)vp; \
	printf("%s:%d:%s: %s: " x, __FILE_NAME__, __LINE__, __FUNCTION__, \
	    "<private>", ## args); \
} while(0)
#endif

#if COMPRESSION_DEBUG
#define DebugLog ErrorLog
#define DebugLogWithPath ErrorLogWithPath
#else
#define DebugLog(x...) do { } while(0)
#define DebugLogWithPath(x...) do { } while(0)
#endif

#if COMPRESSION_DEBUG_VERBOSE
#define VerboseLog ErrorLog
#define VerboseLogWithPath ErrorLogWithPath
#else
#define VerboseLog(x...) do { } while(0)
#define VerboseLogWithPath(x...) do { } while(0)
#endif

#define decmpfs_ktriage_record(code, arg) ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_DECMPFS, KDBG_TRIAGE_RESERVED, code), arg);

enum ktriage_decmpfs_error_codes {
	KTRIAGE_DECMPFS_PREFIX = 0,
	KTRIAGE_DECMPFS_IVALID_OFFSET,
	KTRIAGE_DECMPFS_COMPRESSOR_NOT_REGISTERED,
	KTRIAGE_DECMPFS_FETCH_CALLBACK_FAILED,
	KTRIAGE_DECMPFS_FETCH_HEADER_FAILED,
	KTRIAGE_DECMPFS_UBC_UPL_MAP_FAILED,
	KTRIAGE_DECMPFS_FETCH_UNCOMPRESSED_DATA_FAILED,

	KTRIAGE_DECMPFS_MAX
};

const char *ktriage_decmpfs_strings[] = {
	[KTRIAGE_DECMPFS_PREFIX] = "decmpfs - ",
	[KTRIAGE_DECMPFS_IVALID_OFFSET] = "pagein offset is invalid\n",
	[KTRIAGE_DECMPFS_COMPRESSOR_NOT_REGISTERED] = "compressor is not registered\n",
	[KTRIAGE_DECMPFS_FETCH_CALLBACK_FAILED] = "fetch callback failed\n",
	[KTRIAGE_DECMPFS_FETCH_HEADER_FAILED] = "fetch decmpfs xattr failed\n",
	[KTRIAGE_DECMPFS_UBC_UPL_MAP_FAILED] = "failed to map a UBC UPL\n",
	[KTRIAGE_DECMPFS_FETCH_UNCOMPRESSED_DATA_FAILED] = "failed to fetch uncompressed data\n",
};

ktriage_strings_t ktriage_decmpfs_subsystem_strings = {KTRIAGE_DECMPFS_MAX, ktriage_decmpfs_strings};

#pragma mark --- globals ---

static LCK_GRP_DECLARE(decmpfs_lockgrp, "VFSCOMP");
static LCK_RW_DECLARE(decompressorsLock, &decmpfs_lockgrp);
static LCK_MTX_DECLARE(decompress_channel_mtx, &decmpfs_lockgrp);

static const decmpfs_registration *decompressors[CMP_MAX]; /* the registered compressors */
static int decompress_channel; /* channel used by decompress_file to wake up waiters */

vfs_context_t decmpfs_ctx;

#pragma mark --- decmp_get_func ---

#define offsetof_func(func) ((uintptr_t)offsetof(decmpfs_registration, func))

static void *
_func_from_offset(uint32_t type, uintptr_t offset, uint32_t discriminator)
{
	/* get the function at the given offset in the registration for the given type */
	const decmpfs_registration *reg = decompressors[type];

	switch (reg->decmpfs_registration) {
	case DECMPFS_REGISTRATION_VERSION_V1:
		if (offset > offsetof_func(free_data)) {
			return NULL;
		}
		break;
	case DECMPFS_REGISTRATION_VERSION_V3:
		if (offset > offsetof_func(get_flags)) {
			return NULL;
		}
		break;
	default:
		return NULL;
	}

	void *ptr = *(void * const *)((uintptr_t)reg + offset);
	if (ptr != NULL) {
		/* Resign as a function-in-void* */
		ptr = ptrauth_auth_and_resign(ptr, ptrauth_key_asia, discriminator, ptrauth_key_asia, 0);
	}
	return ptr;
}

extern void IOServicePublishResource( const char * property, boolean_t value );
extern boolean_t IOServiceWaitForMatchingResource( const char * property, uint64_t timeout );
extern boolean_t IOCatalogueMatchingDriversPresent( const char * property );

static void *
_decmp_get_func(vnode_t vp, uint32_t type, uintptr_t offset, uint32_t discriminator)
{
	/*
	 *  this function should be called while holding a shared lock to decompressorsLock,
	 *  and will return with the lock held
	 */

	if (type >= CMP_MAX) {
		return NULL;
	}

	if (decompressors[type] != NULL) {
		// the compressor has already registered but the function might be null
		return _func_from_offset(type, offset, discriminator);
	}

	// does IOKit know about a kext that is supposed to provide this type?
	char providesName[80];
	snprintf(providesName, sizeof(providesName), "com.apple.AppleFSCompression.providesType%u", type);
	if (IOCatalogueMatchingDriversPresent(providesName)) {
		// there is a kext that says it will register for this type, so let's wait for it
		char resourceName[80];
		uint64_t delay = 10000000ULL; // 10 milliseconds.
		snprintf(resourceName, sizeof(resourceName), "com.apple.AppleFSCompression.Type%u", type);
		ErrorLogWithPath("waiting for %s\n", resourceName);
		while (decompressors[type] == NULL) {
			lck_rw_unlock_shared(&decompressorsLock); // we have to unlock to allow the kext to register
			if (IOServiceWaitForMatchingResource(resourceName, delay)) {
				lck_rw_lock_shared(&decompressorsLock);
				break;
			}
			if (!IOCatalogueMatchingDriversPresent(providesName)) {
				//
				ErrorLogWithPath("the kext with %s is no longer present\n", providesName);
				lck_rw_lock_shared(&decompressorsLock);
				break;
			}
			ErrorLogWithPath("still waiting for %s\n", resourceName);
			delay *= 2;
			lck_rw_lock_shared(&decompressorsLock);
		}
		// IOKit says the kext is loaded, so it should be registered too!
		if (decompressors[type] == NULL) {
			ErrorLogWithPath("we found %s, but the type still isn't registered\n", providesName);
			return NULL;
		}
		// it's now registered, so let's return the function
		return _func_from_offset(type, offset, discriminator);
	}

	// the compressor hasn't registered, so it never will unless someone manually kextloads it
	ErrorLogWithPath("tried to access a compressed file of unregistered type %d\n", type);
	return NULL;
}

#define decmp_get_func(vp, type, func) (typeof(decompressors[0]->func))_decmp_get_func(vp, type, offsetof_func(func), ptrauth_function_pointer_type_discriminator(typeof(decompressors[0]->func)))

#pragma mark --- utilities ---

#if COMPRESSION_DEBUG
static int
vnsize(vnode_t vp, uint64_t *size)
{
	struct vnode_attr va;
	VATTR_INIT(&va);
	VATTR_WANTED(&va, va_data_size);
	int error = vnode_getattr(vp, &va, decmpfs_ctx);
	if (error != 0) {
		ErrorLogWithPath("vnode_getattr err %d\n", error);
		return error;
	}
	*size = va.va_data_size;
	return 0;
}
#endif /* COMPRESSION_DEBUG */

#pragma mark --- cnode routines ---

ZONE_DEFINE(decmpfs_cnode_zone, "decmpfs_cnode",
    sizeof(struct decmpfs_cnode), ZC_NONE);

decmpfs_cnode *
decmpfs_cnode_alloc(void)
{
	return zalloc(decmpfs_cnode_zone);
}

void
decmpfs_cnode_free(decmpfs_cnode *dp)
{
	zfree(decmpfs_cnode_zone, dp);
}

void
decmpfs_cnode_init(decmpfs_cnode *cp)
{
	memset(cp, 0, sizeof(*cp));
	lck_rw_init(&cp->compressed_data_lock, &decmpfs_lockgrp, NULL);
}

void
decmpfs_cnode_destroy(decmpfs_cnode *cp)
{
	lck_rw_destroy(&cp->compressed_data_lock, &decmpfs_lockgrp);
}

bool
decmpfs_trylock_compressed_data(decmpfs_cnode *cp, int exclusive)
{
	void *thread = current_thread();
	bool retval = false;

	if (cp->lockowner == thread) {
		/* this thread is already holding an exclusive lock, so bump the count */
		cp->lockcount++;
		retval = true;
	} else if (exclusive) {
		if ((retval = lck_rw_try_lock_exclusive(&cp->compressed_data_lock))) {
			cp->lockowner = thread;
			cp->lockcount = 1;
		}
	} else {
		if ((retval = lck_rw_try_lock_shared(&cp->compressed_data_lock))) {
			cp->lockowner = (void *)-1;
		}
	}
	return retval;
}

void
decmpfs_lock_compressed_data(decmpfs_cnode *cp, int exclusive)
{
	void *thread = current_thread();

	if (cp->lockowner == thread) {
		/* this thread is already holding an exclusive lock, so bump the count */
		cp->lockcount++;
	} else if (exclusive) {
		lck_rw_lock_exclusive(&cp->compressed_data_lock);
		cp->lockowner = thread;
		cp->lockcount = 1;
	} else {
		lck_rw_lock_shared(&cp->compressed_data_lock);
		cp->lockowner = (void *)-1;
	}
}

void
decmpfs_unlock_compressed_data(decmpfs_cnode *cp, __unused int exclusive)
{
	void *thread = current_thread();

	if (cp->lockowner == thread) {
		/* this thread is holding an exclusive lock, so decrement the count */
		if ((--cp->lockcount) > 0) {
			/* the caller still has outstanding locks, so we're done */
			return;
		}
		cp->lockowner = NULL;
	}

	lck_rw_done(&cp->compressed_data_lock);
}

uint32_t
decmpfs_cnode_get_vnode_state(decmpfs_cnode *cp)
{
	return cp->cmp_state;
}

void
decmpfs_cnode_set_vnode_state(decmpfs_cnode *cp, uint32_t state, int skiplock)
{
	if (!skiplock) {
		decmpfs_lock_compressed_data(cp, 1);
	}
	cp->cmp_state = (uint8_t)state;
	if (state == FILE_TYPE_UNKNOWN) {
		/* clear out the compression type too */
		cp->cmp_type = 0;
	}
	if (!skiplock) {
		decmpfs_unlock_compressed_data(cp, 1);
	}
}

static void
decmpfs_cnode_set_vnode_cmp_type(decmpfs_cnode *cp, uint32_t cmp_type, int skiplock)
{
	if (!skiplock) {
		decmpfs_lock_compressed_data(cp, 1);
	}
	cp->cmp_type = cmp_type;
	if (!skiplock) {
		decmpfs_unlock_compressed_data(cp, 1);
	}
}

static void
decmpfs_cnode_set_vnode_minimal_xattr(decmpfs_cnode *cp, int minimal_xattr, int skiplock)
{
	if (!skiplock) {
		decmpfs_lock_compressed_data(cp, 1);
	}
	cp->cmp_minimal_xattr = !!minimal_xattr;
	if (!skiplock) {
		decmpfs_unlock_compressed_data(cp, 1);
	}
}

uint64_t
decmpfs_cnode_get_vnode_cached_size(decmpfs_cnode *cp)
{
	return cp->uncompressed_size;
}

uint64_t
decmpfs_cnode_get_vnode_cached_nchildren(decmpfs_cnode *cp)
{
	return cp->nchildren;
}

uint64_t
decmpfs_cnode_get_vnode_cached_total_size(decmpfs_cnode *cp)
{
	return cp->total_size;
}

void
decmpfs_cnode_set_vnode_cached_size(decmpfs_cnode *cp, uint64_t size)
{
	while (1) {
		uint64_t old = cp->uncompressed_size;
		if (OSCompareAndSwap64(old, size, (UInt64*)&cp->uncompressed_size)) {
			return;
		} else {
			/* failed to write our value, so loop */
		}
	}
}

void
decmpfs_cnode_set_vnode_cached_nchildren(decmpfs_cnode *cp, uint64_t nchildren)
{
	while (1) {
		uint64_t old = cp->nchildren;
		if (OSCompareAndSwap64(old, nchildren, (UInt64*)&cp->nchildren)) {
			return;
		} else {
			/* failed to write our value, so loop */
		}
	}
}

void
decmpfs_cnode_set_vnode_cached_total_size(decmpfs_cnode *cp, uint64_t total_sz)
{
	while (1) {
		uint64_t old = cp->total_size;
		if (OSCompareAndSwap64(old, total_sz, (UInt64*)&cp->total_size)) {
			return;
		} else {
			/* failed to write our value, so loop */
		}
	}
}

static uint64_t
decmpfs_cnode_get_decompression_flags(decmpfs_cnode *cp)
{
	return cp->decompression_flags;
}

static void
decmpfs_cnode_set_decompression_flags(decmpfs_cnode *cp, uint64_t flags)
{
	while (1) {
		uint64_t old = cp->decompression_flags;
		if (OSCompareAndSwap64(old, flags, (UInt64*)&cp->decompression_flags)) {
			return;
		} else {
			/* failed to write our value, so loop */
		}
	}
}

uint32_t
decmpfs_cnode_cmp_type(decmpfs_cnode *cp)
{
	return cp->cmp_type;
}

#pragma mark --- decmpfs state routines ---

static int
decmpfs_fetch_compressed_header(vnode_t vp, decmpfs_cnode *cp, decmpfs_header **hdrOut, int returnInvalid, size_t *hdr_size)
{
	/*
	 *  fetches vp's compression xattr, converting it into a decmpfs_header; returns 0 or errno
	 *  if returnInvalid == 1, returns the header even if the type was invalid (out of range),
	 *  and return ERANGE in that case
	 */

	size_t read_size             = 0;
	size_t attr_size             = 0;
	size_t alloc_size            = 0;
	uio_t attr_uio               = NULL;
	int err                      = 0;
	char *data                   = NULL;
	const bool no_additional_data = ((cp != NULL)
	    && (cp->cmp_type != 0)
	    && (cp->cmp_minimal_xattr != 0));
	UIO_STACKBUF(uio_buf, 1);
	decmpfs_header *hdr = NULL;

	/*
	 * Trace the following parameters on entry with event-id 0x03120004
	 *
	 * @vp->v_id:       vnode-id for which to fetch compressed header.
	 * @no_additional_data: If set true then xattr didn't have any extra data.
	 * @returnInvalid:  return the header even though the type is out of range.
	 */
	DECMPFS_EMIT_TRACE_ENTRY(DECMPDBG_FETCH_COMPRESSED_HEADER, vp->v_id,
	    no_additional_data, returnInvalid);

	if (no_additional_data) {
		/* this file's xattr didn't have any extra data when we fetched it, so we can synthesize a header from the data in the cnode */

		alloc_size = sizeof(decmpfs_header);
		data = kalloc_data(sizeof(decmpfs_header), Z_WAITOK | Z_NOFAIL);
		hdr = (decmpfs_header*)data;
		hdr->attr_size = sizeof(decmpfs_disk_header);
		hdr->compression_magic = DECMPFS_MAGIC;
		hdr->compression_type  = cp->cmp_type;
		if (hdr->compression_type == DATALESS_PKG_CMPFS_TYPE) {
			if (!vnode_isdir(vp)) {
				err = EINVAL;
				goto out;
			}
			hdr->_size.value = DECMPFS_PKG_VALUE_FROM_SIZE_COUNT(
				decmpfs_cnode_get_vnode_cached_size(cp),
				decmpfs_cnode_get_vnode_cached_nchildren(cp));
		} else if (vnode_isdir(vp)) {
			hdr->_size.value = decmpfs_cnode_get_vnode_cached_nchildren(cp);
		} else {
			hdr->_size.value = decmpfs_cnode_get_vnode_cached_size(cp);
		}
	} else {
		/* figure out how big the xattr is on disk */
		err = vn_getxattr(vp, DECMPFS_XATTR_NAME, NULL, &attr_size, XATTR_NOSECURITY, decmpfs_ctx);
		if (err != 0) {
			goto out;
		}
		alloc_size = attr_size + sizeof(hdr->attr_size);

		if (attr_size < sizeof(decmpfs_disk_header) || attr_size > MAX_DECMPFS_XATTR_SIZE) {
			err = EINVAL;
			goto out;
		}

		/* allocation includes space for the extra attr_size field of a compressed_header */
		data = (char *)kalloc_data(alloc_size, Z_WAITOK);
		/*
		 * we know the decmpfs sizes tend to be "small", and
		 * vm_page_alloc_list() will actually wait for memory
		 * for asks of less than 1/4th of the physical memory.
		 *
		 * This is important because decmpfs failures result
		 * in pageins failing spuriously and eventually SIGBUS
		 * errors for example when faulting in TEXT.
		 */
		assert(data);

		/* read the xattr into our buffer, skipping over the attr_size field at the beginning */
		attr_uio = uio_createwithbuffer(1, 0, UIO_SYSSPACE, UIO_READ, &uio_buf[0], sizeof(uio_buf));
		uio_addiov(attr_uio, CAST_USER_ADDR_T(data + sizeof(hdr->attr_size)), attr_size);

		err = vn_getxattr(vp, DECMPFS_XATTR_NAME, attr_uio, &read_size, XATTR_NOSECURITY, decmpfs_ctx);
		if (err != 0) {
			goto out;
		}
		if (read_size != attr_size) {
			err = EINVAL;
			goto out;
		}
		hdr = (decmpfs_header*)data;
		hdr->attr_size = (uint32_t)attr_size;
		/* swap the fields to native endian */
		hdr->compression_magic = OSSwapLittleToHostInt32(hdr->compression_magic);
		hdr->compression_type  = OSSwapLittleToHostInt32(hdr->compression_type);
		hdr->uncompressed_size = OSSwapLittleToHostInt64(hdr->uncompressed_size);
	}

	if (hdr->compression_magic != DECMPFS_MAGIC) {
		ErrorLogWithPath("invalid compression_magic 0x%08x, should be 0x%08x\n", hdr->compression_magic, DECMPFS_MAGIC);
		err = EINVAL;
		goto out;
	}

	/*
	 * Special-case the DATALESS compressor here; that is a valid type,
	 * even through there will never be an entry in the decompressor
	 * handler table for it.  If we don't do this, then the cmp_state
	 * for this cnode will end up being marked NOT_COMPRESSED, and
	 * we'll be stuck in limbo.
	 */
	if (hdr->compression_type >= CMP_MAX && !decmpfs_type_is_dataless(hdr->compression_type)) {
		if (returnInvalid) {
			/* return the header even though the type is out of range */
			err = ERANGE;
		} else {
			ErrorLogWithPath("compression_type %d out of range\n", hdr->compression_type);
			err = EINVAL;
		}
		goto out;
	}

out:
	if (err && (err != ERANGE)) {
		DebugLogWithPath("err %d\n", err);
		kfree_data(data, alloc_size);
		*hdrOut = NULL;
	} else {
		*hdrOut = hdr;
		*hdr_size = alloc_size;
	}
	/*
	 * Trace the following parameters on return with event-id 0x03120004.
	 *
	 * @vp->v_id:       vnode-id for which to fetch compressed header.
	 * @err:            value returned from this function.
	 */
	DECMPFS_EMIT_TRACE_RETURN(DECMPDBG_FETCH_COMPRESSED_HEADER, vp->v_id, err);
	return err;
}

static int
decmpfs_fast_get_state(decmpfs_cnode *cp)
{
	/*
	 *  return the cached state
	 *  this should *only* be called when we know that decmpfs_file_is_compressed has already been called,
	 *  because this implies that the cached state is valid
	 */
	int cmp_state = decmpfs_cnode_get_vnode_state(cp);

	switch (cmp_state) {
	case FILE_IS_NOT_COMPRESSED:
	case FILE_IS_COMPRESSED:
	case FILE_IS_CONVERTING:
		return cmp_state;
	case FILE_TYPE_UNKNOWN:
		/*
		 *  we should only get here if decmpfs_file_is_compressed was not called earlier on this vnode,
		 *  which should not be possible
		 */
		ErrorLog("decmpfs_fast_get_state called on unknown file\n");
		return FILE_IS_NOT_COMPRESSED;
	default:
		/* */
		ErrorLog("unknown cmp_state %d\n", cmp_state);
		return FILE_IS_NOT_COMPRESSED;
	}
}

static int
decmpfs_fast_file_is_compressed(decmpfs_cnode *cp)
{
	int cmp_state = decmpfs_cnode_get_vnode_state(cp);

	switch (cmp_state) {
	case FILE_IS_NOT_COMPRESSED:
		return 0;
	case FILE_IS_COMPRESSED:
	case FILE_IS_CONVERTING:
		return 1;
	case FILE_TYPE_UNKNOWN:
		/*
		 *  we should only get here if decmpfs_file_is_compressed was not called earlier on this vnode,
		 *  which should not be possible
		 */
		ErrorLog("decmpfs_fast_get_state called on unknown file\n");
		return 0;
	default:
		/* */
		ErrorLog("unknown cmp_state %d\n", cmp_state);
		return 0;
	}
}

errno_t
decmpfs_validate_compressed_file(vnode_t vp, decmpfs_cnode *cp)
{
	/* give a compressor a chance to indicate that a compressed file is invalid */
	decmpfs_header *hdr = NULL;
	size_t alloc_size = 0;
	errno_t err = decmpfs_fetch_compressed_header(vp, cp, &hdr, 0, &alloc_size);

	if (err) {
		/* we couldn't get the header */
		if (decmpfs_fast_get_state(cp) == FILE_IS_NOT_COMPRESSED) {
			/* the file is no longer compressed, so return success */
			err = 0;
		}
		goto out;
	}

	if (!decmpfs_type_is_dataless(hdr->compression_type)) {
		lck_rw_lock_shared(&decompressorsLock);
		decmpfs_validate_compressed_file_func validate = decmp_get_func(vp, hdr->compression_type, validate);
		if (validate) { /* make sure this validation function is valid */
			/* is the data okay? */
			err = validate(vp, decmpfs_ctx, hdr);
		} else if (decmp_get_func(vp, hdr->compression_type, fetch) == NULL) {
			/* the type isn't registered */
			err = EIO;
		} else {
			/* no validate registered, so nothing to do */
			err = 0;
		}
		lck_rw_unlock_shared(&decompressorsLock);
	}
out:
	if (hdr != NULL) {
		kfree_data(hdr, alloc_size);
	}
#if COMPRESSION_DEBUG
	if (err) {
		DebugLogWithPath("decmpfs_validate_compressed_file ret %d, vp->v_flag %d\n", err, vp->v_flag);
	}
#endif
	return err;
}

int
decmpfs_file_is_compressed(vnode_t vp, decmpfs_cnode *cp)
{
	/*
	 *  determines whether vp points to a compressed file
	 *
	 *  to speed up this operation, we cache the result in the cnode, and do as little as possible
	 *  in the case where the cnode already has a valid cached state
	 *
	 */

	int ret = 0;
	int error = 0;
	uint32_t cmp_state;
	struct vnode_attr va_fetch;
	decmpfs_header *hdr = NULL;
	size_t alloc_size = 0;
	mount_t mp = NULL;
	int cnode_locked = 0;
	int saveInvalid = 0; // save the header data even though the type was out of range
	uint64_t decompression_flags = 0;
	bool is_mounted, is_local_fs;

	if (vnode_isnamedstream(vp)) {
		/*
		 *  named streams can't be compressed
		 *  since named streams of the same file share the same cnode,
		 *  we don't want to get/set the state in the cnode, just return 0
		 */
		return 0;
	}

	/* examine the cached a state in this cnode */
	cmp_state = decmpfs_cnode_get_vnode_state(cp);
	switch (cmp_state) {
	case FILE_IS_NOT_COMPRESSED:
		return 0;
	case FILE_IS_COMPRESSED:
		return 1;
	case FILE_IS_CONVERTING:
		/* treat the file as compressed, because this gives us a way to block future reads until decompression is done */
		return 1;
	case FILE_TYPE_UNKNOWN:
		/* the first time we encountered this vnode, so we need to check it out */
		break;
	default:
		/* unknown state, assume file is not compressed */
		ErrorLogWithPath("unknown cmp_state %d\n", cmp_state);
		return 0;
	}

	is_mounted = false;
	is_local_fs = false;
	mp = vnode_mount(vp);
	if (mp) {
		is_mounted = true;
	}
	if (is_mounted) {
		is_local_fs = ((mp->mnt_flag & MNT_LOCAL));
	}
	/*
	 * Trace the following parameters on entry with event-id 0x03120014.
	 *
	 * @vp->v_id:       vnode-id of the file being queried.
	 * @is_mounted:     set to true if @vp belongs to a mounted fs.
	 * @is_local_fs:    set to true if @vp belongs to local fs.
	 */
	DECMPFS_EMIT_TRACE_ENTRY(DECMPDBG_FILE_IS_COMPRESSED, vp->v_id,
	    is_mounted, is_local_fs);

	if (!is_mounted) {
		/*
		 *  this should only be true before we mount the root filesystem
		 *  we short-cut this return to avoid the call to getattr below, which
		 *  will fail before root is mounted
		 */
		ret = FILE_IS_NOT_COMPRESSED;
		goto done;
	}

	if (!is_local_fs) {
		/* compression only supported on local filesystems */
		ret = FILE_IS_NOT_COMPRESSED;
		goto done;
	}

	/* lock our cnode data so that another caller doesn't change the state under us */
	decmpfs_lock_compressed_data(cp, 1);
	cnode_locked = 1;

	VATTR_INIT(&va_fetch);
	VATTR_WANTED(&va_fetch, va_flags);
	error = vnode_getattr(vp, &va_fetch, decmpfs_ctx);
	if (error) {
		/* failed to get the bsd flags so the file is not compressed */
		ret = FILE_IS_NOT_COMPRESSED;
		goto done;
	}
	if (va_fetch.va_flags & UF_COMPRESSED) {
		/* UF_COMPRESSED is on, make sure the file has the DECMPFS_XATTR_NAME xattr */
		error = decmpfs_fetch_compressed_header(vp, cp, &hdr, 1, &alloc_size);
		if ((hdr != NULL) && (error == ERANGE)) {
			saveInvalid = 1;
		}
		if (error) {
			/* failed to get the xattr so the file is not compressed */
			ret = FILE_IS_NOT_COMPRESSED;
			goto done;
		}
		/*
		 * We got the xattr, so the file is at least tagged compressed.
		 * For DATALESS, regular files and directories can be "compressed".
		 * For all other types, only files are allowed.
		 */
		if (!vnode_isreg(vp) &&
		    !(decmpfs_type_is_dataless(hdr->compression_type) && vnode_isdir(vp))) {
			ret = FILE_IS_NOT_COMPRESSED;
			goto done;
		}
		ret = FILE_IS_COMPRESSED;
		goto done;
	}
	/* UF_COMPRESSED isn't on, so the file isn't compressed */
	ret = FILE_IS_NOT_COMPRESSED;

done:
	if (((ret == FILE_IS_COMPRESSED) || saveInvalid) && hdr) {
		/*
		 *  cache the uncompressed size away in the cnode
		 */

		if (!cnode_locked) {
			/*
			 *  we should never get here since the only place ret is set to FILE_IS_COMPRESSED
			 *  is after the call to decmpfs_lock_compressed_data above
			 */
			decmpfs_lock_compressed_data(cp, 1);
			cnode_locked = 1;
		}

		if (vnode_isdir(vp)) {
			decmpfs_cnode_set_vnode_cached_size(cp, 64);
			decmpfs_cnode_set_vnode_cached_nchildren(cp, decmpfs_get_directory_entries(hdr));
			if (hdr->compression_type == DATALESS_PKG_CMPFS_TYPE) {
				decmpfs_cnode_set_vnode_cached_total_size(cp, DECMPFS_PKG_SIZE(hdr->_size));
			}
		} else {
			decmpfs_cnode_set_vnode_cached_size(cp, hdr->uncompressed_size);
		}
		decmpfs_cnode_set_vnode_state(cp, ret, 1);
		decmpfs_cnode_set_vnode_cmp_type(cp, hdr->compression_type, 1);
		/* remember if the xattr's size was equal to the minimal xattr */
		if (hdr->attr_size == sizeof(decmpfs_disk_header)) {
			decmpfs_cnode_set_vnode_minimal_xattr(cp, 1, 1);
		}
		if (ret == FILE_IS_COMPRESSED) {
			/* update the ubc's size for this file */
			ubc_setsize(vp, hdr->uncompressed_size);

			/* update the decompression flags in the decmpfs cnode */
			lck_rw_lock_shared(&decompressorsLock);
			decmpfs_get_decompression_flags_func get_flags = decmp_get_func(vp, hdr->compression_type, get_flags);
			if (get_flags) {
				decompression_flags = get_flags(vp, decmpfs_ctx, hdr);
			}
			lck_rw_unlock_shared(&decompressorsLock);
			decmpfs_cnode_set_decompression_flags(cp, decompression_flags);
		}
	} else {
		/* we might have already taken the lock above; if so, skip taking it again by passing cnode_locked as the skiplock parameter */
		decmpfs_cnode_set_vnode_state(cp, ret, cnode_locked);
	}

	if (cnode_locked) {
		decmpfs_unlock_compressed_data(cp, 1);
	}

	if (hdr != NULL) {
		kfree_data(hdr, alloc_size);
	}

	/*
	 * Trace the following parameters on return with event-id 0x03120014.
	 *
	 * @vp->v_id:       vnode-id of the file being queried.
	 * @return:         set to 1 is file is compressed.
	 */
	switch (ret) {
	case FILE_IS_NOT_COMPRESSED:
		DECMPFS_EMIT_TRACE_RETURN(DECMPDBG_FILE_IS_COMPRESSED, vp->v_id, 0);
		return 0;
	case FILE_IS_COMPRESSED:
	case FILE_IS_CONVERTING:
		DECMPFS_EMIT_TRACE_RETURN(DECMPDBG_FILE_IS_COMPRESSED, vp->v_id, 1);
		return 1;
	default:
		/* unknown state, assume file is not compressed */
		DECMPFS_EMIT_TRACE_RETURN(DECMPDBG_FILE_IS_COMPRESSED, vp->v_id, 0);
		ErrorLogWithPath("unknown ret %d\n", ret);
		return 0;
	}
}

int
decmpfs_update_attributes(vnode_t vp, struct vnode_attr *vap)
{
	int error = 0;

	if (VATTR_IS_ACTIVE(vap, va_flags)) {
		/* the BSD flags are being updated */
		if (vap->va_flags & UF_COMPRESSED) {
			/* the compressed bit is being set, did it change? */
			struct vnode_attr va_fetch;
			int old_flags = 0;
			VATTR_INIT(&va_fetch);
			VATTR_WANTED(&va_fetch, va_flags);
			error = vnode_getattr(vp, &va_fetch, decmpfs_ctx);
			if (error) {
				return error;
			}

			old_flags = va_fetch.va_flags;

			if (!(old_flags & UF_COMPRESSED)) {
				/*
				 * Compression bit was turned on, make sure the file has the DECMPFS_XATTR_NAME attribute.
				 * This precludes anyone from using the UF_COMPRESSED bit for anything else, and it enforces
				 * an order of operation -- you must first do the setxattr and then the chflags.
				 */

				if (VATTR_IS_ACTIVE(vap, va_data_size)) {
					/*
					 * don't allow the caller to set the BSD flag and the size in the same call
					 * since this doesn't really make sense
					 */
					vap->va_flags &= ~UF_COMPRESSED;
					return 0;
				}

				decmpfs_header *hdr = NULL;
				size_t alloc_size = 0;
				error = decmpfs_fetch_compressed_header(vp, NULL, &hdr, 1, &alloc_size);
				if (error == 0) {
					/*
					 * Allow the flag to be set since the decmpfs attribute
					 * is present.
					 *
					 * If we're creating a dataless file we do not want to
					 * truncate it to zero which allows the file resolver to
					 * have more control over when truncation should happen.
					 * All other types of compressed files are truncated to
					 * zero.
					 */
					if (!decmpfs_type_is_dataless(hdr->compression_type)) {
						VATTR_SET_ACTIVE(vap, va_data_size);
						vap->va_data_size = 0;
					}
				} else if (error == ERANGE) {
					/* the file had a decmpfs attribute but the type was out of range, so don't muck with the file's data size */
				} else {
					/* no DECMPFS_XATTR_NAME attribute, so deny the update */
					vap->va_flags &= ~UF_COMPRESSED;
				}
				if (hdr != NULL) {
					kfree_data(hdr, alloc_size);
				}
			}
		}
	}

	return 0;
}

static int
wait_for_decompress(decmpfs_cnode *cp)
{
	int state;
	lck_mtx_lock(&decompress_channel_mtx);
	do {
		state = decmpfs_fast_get_state(cp);
		if (state != FILE_IS_CONVERTING) {
			/* file is not decompressing */
			lck_mtx_unlock(&decompress_channel_mtx);
			return state;
		}
		msleep((caddr_t)&decompress_channel, &decompress_channel_mtx, PINOD, "wait_for_decompress", NULL);
	} while (1);
}

#pragma mark --- decmpfs hide query routines ---

int
decmpfs_hides_rsrc(vfs_context_t ctx, decmpfs_cnode *cp)
{
	/*
	 *  WARNING!!!
	 *  callers may (and do) pass NULL for ctx, so we should only use it
	 *  for this equality comparison
	 *
	 *  This routine should only be called after a file has already been through decmpfs_file_is_compressed
	 */

	if (ctx == decmpfs_ctx) {
		return 0;
	}

	if (!decmpfs_fast_file_is_compressed(cp)) {
		return 0;
	}

	/* all compressed files hide their resource fork */
	return 1;
}

int
decmpfs_hides_xattr(vfs_context_t ctx, decmpfs_cnode *cp, const char *xattr)
{
	/*
	 *  WARNING!!!
	 *  callers may (and do) pass NULL for ctx, so we should only use it
	 *  for this equality comparison
	 *
	 *  This routine should only be called after a file has already been through decmpfs_file_is_compressed
	 */

	if (ctx == decmpfs_ctx) {
		return 0;
	}
	if (strncmp(xattr, XATTR_RESOURCEFORK_NAME, sizeof(XATTR_RESOURCEFORK_NAME) - 1) == 0) {
		return decmpfs_hides_rsrc(ctx, cp);
	}
	if (!decmpfs_fast_file_is_compressed(cp)) {
		/* file is not compressed, so don't hide this xattr */
		return 0;
	}
	if (strncmp(xattr, DECMPFS_XATTR_NAME, sizeof(DECMPFS_XATTR_NAME) - 1) == 0) {
		/* it's our xattr, so hide it */
		return 1;
	}
	/* don't hide this xattr */
	return 0;
}

#pragma mark --- registration/validation routines ---

static inline int
registration_valid(const decmpfs_registration *registration)
{
	return registration && ((registration->decmpfs_registration == DECMPFS_REGISTRATION_VERSION_V1) || (registration->decmpfs_registration == DECMPFS_REGISTRATION_VERSION_V3));
}

errno_t
register_decmpfs_decompressor(uint32_t compression_type, const decmpfs_registration *registration)
{
	/* called by kexts to register decompressors */

	errno_t ret = 0;
	int locked = 0;
	char resourceName[80];

	if ((compression_type >= CMP_MAX) || !registration_valid(registration)) {
		ret = EINVAL;
		goto out;
	}

	lck_rw_lock_exclusive(&decompressorsLock); locked = 1;

	/* make sure the registration for this type is zero */
	if (decompressors[compression_type] != NULL) {
		ret = EEXIST;
		goto out;
	}
	decompressors[compression_type] = registration;
	snprintf(resourceName, sizeof(resourceName), "com.apple.AppleFSCompression.Type%u", compression_type);
	IOServicePublishResource(resourceName, TRUE);

out:
	if (locked) {
		lck_rw_unlock_exclusive(&decompressorsLock);
	}
	return ret;
}

errno_t
unregister_decmpfs_decompressor(uint32_t compression_type, decmpfs_registration *registration)
{
	/* called by kexts to unregister decompressors */

	errno_t ret = 0;
	int locked = 0;
	char resourceName[80];

	if ((compression_type >= CMP_MAX) || !registration_valid(registration)) {
		ret = EINVAL;
		goto out;
	}

	lck_rw_lock_exclusive(&decompressorsLock); locked = 1;
	if (decompressors[compression_type] != registration) {
		ret = EEXIST;
		goto out;
	}
	decompressors[compression_type] = NULL;
	snprintf(resourceName, sizeof(resourceName), "com.apple.AppleFSCompression.Type%u", compression_type);
	IOServicePublishResource(resourceName, FALSE);

out:
	if (locked) {
		lck_rw_unlock_exclusive(&decompressorsLock);
	}
	return ret;
}

static int
compression_type_valid(vnode_t vp, decmpfs_header *hdr)
{
	/* fast pre-check to determine if the given compressor has checked in */
	int ret = 0;

	/* every compressor must have at least a fetch function */
	lck_rw_lock_shared(&decompressorsLock);
	if (decmp_get_func(vp, hdr->compression_type, fetch) != NULL) {
		ret = 1;
	}
	lck_rw_unlock_shared(&decompressorsLock);

	return ret;
}

#pragma mark --- compression/decompression routines ---

static int
decmpfs_fetch_uncompressed_data(vnode_t vp, decmpfs_cnode *cp, decmpfs_header *hdr, off_t offset, user_ssize_t size, int nvec, decmpfs_vector *vec, uint64_t *bytes_read)
{
	/* get the uncompressed bytes for the specified region of vp by calling out to the registered compressor */

	int err          = 0;

	*bytes_read = 0;

	if (offset >= (off_t)hdr->uncompressed_size) {
		/* reading past end of file; nothing to do */
		err = 0;
		goto out;
	}
	if (offset < 0) {
		/* tried to read from before start of file */
		err = EINVAL;
		goto out;
	}
	if (hdr->uncompressed_size - offset < size) {
		/* adjust size so we don't read past the end of the file */
		size = (user_ssize_t)(hdr->uncompressed_size - offset);
	}
	if (size == 0) {
		/* nothing to read */
		err = 0;
		goto out;
	}

	/*
	 * Trace the following parameters on entry with event-id 0x03120008.
	 *
	 * @vp->v_id:       vnode-id of the file being decompressed.
	 * @hdr->compression_type: compression type.
	 * @offset:         offset from where to fetch uncompressed data.
	 * @size:           amount of uncompressed data to fetch.
	 *
	 * Please NOTE: @offset and @size can overflow in theory but
	 * here it is safe.
	 */
	DECMPFS_EMIT_TRACE_ENTRY(DECMPDBG_FETCH_UNCOMPRESSED_DATA, vp->v_id,
	    hdr->compression_type, (int)offset, (int)size);
	lck_rw_lock_shared(&decompressorsLock);
	decmpfs_fetch_uncompressed_data_func fetch = decmp_get_func(vp, hdr->compression_type, fetch);
	if (fetch) {
		err = fetch(vp, decmpfs_ctx, hdr, offset, size, nvec, vec, bytes_read);
		lck_rw_unlock_shared(&decompressorsLock);
		if (err == 0) {
			uint64_t decompression_flags = decmpfs_cnode_get_decompression_flags(cp);
			if (decompression_flags & DECMPFS_FLAGS_FORCE_FLUSH_ON_DECOMPRESS) {
#if     !defined(__i386__) && !defined(__x86_64__)
				int i;
				for (i = 0; i < nvec; i++) {
					assert(vec[i].size >= 0 && vec[i].size <= UINT_MAX);
					flush_dcache64((addr64_t)(uintptr_t)vec[i].buf, (unsigned int)vec[i].size, FALSE);
				}
#endif
			}
		} else {
			decmpfs_ktriage_record(KTRIAGE_DECMPFS_FETCH_CALLBACK_FAILED, err);
		}
	} else {
		decmpfs_ktriage_record(KTRIAGE_DECMPFS_COMPRESSOR_NOT_REGISTERED, hdr->compression_type);
		err = ENOTSUP;
		lck_rw_unlock_shared(&decompressorsLock);
	}
	/*
	 * Trace the following parameters on return with event-id 0x03120008.
	 *
	 * @vp->v_id:       vnode-id of the file being decompressed.
	 * @bytes_read:     amount of uncompressed bytes fetched in bytes.
	 * @err:            value returned from this function.
	 *
	 * Please NOTE: @bytes_read can overflow in theory but here it is safe.
	 */
	DECMPFS_EMIT_TRACE_RETURN(DECMPDBG_FETCH_UNCOMPRESSED_DATA, vp->v_id,
	    (int)*bytes_read, err);
out:
	return err;
}

static kern_return_t
commit_upl(upl_t upl, upl_offset_t pl_offset, size_t uplSize, int flags, int abort)
{
	kern_return_t kr = 0;

#if CONFIG_IOSCHED
	upl_unmark_decmp(upl);
#endif /* CONFIG_IOSCHED */

	/* commit the upl pages */
	if (abort) {
		VerboseLog("aborting upl, flags 0x%08x\n", flags);
		kr = ubc_upl_abort_range(upl, pl_offset, (upl_size_t)uplSize, flags);
		if (kr != KERN_SUCCESS) {
			ErrorLog("ubc_upl_abort_range error %d\n", (int)kr);
		}
	} else {
		VerboseLog("committing upl, flags 0x%08x\n", flags | UPL_COMMIT_CLEAR_DIRTY);
		kr = ubc_upl_commit_range(upl, pl_offset, (upl_size_t)uplSize, flags | UPL_COMMIT_CLEAR_DIRTY | UPL_COMMIT_WRITTEN_BY_KERNEL);
		if (kr != KERN_SUCCESS) {
			ErrorLog("ubc_upl_commit_range error %d\n", (int)kr);
		}
	}
	return kr;
}


errno_t
decmpfs_pagein_compressed(struct vnop_pagein_args *ap, int *is_compressed, decmpfs_cnode *cp)
{
	/* handles a page-in request from vfs for a compressed file */

	int err                      = 0;
	vnode_t vp                   = ap->a_vp;
	upl_t pl                     = ap->a_pl;
	upl_offset_t pl_offset       = ap->a_pl_offset;
	off_t f_offset               = ap->a_f_offset;
	size_t size                  = ap->a_size;
	int flags                    = ap->a_flags;
	off_t uplPos                 = 0;
	user_ssize_t uplSize         = 0;
	user_ssize_t rounded_uplSize = 0;
	size_t verify_block_size     = 0;
	void *data                   = NULL;
	decmpfs_header *hdr = NULL;
	size_t alloc_size            = 0;
	uint64_t cachedSize          = 0;
	uint32_t fs_bsize            = 0;
	int cmpdata_locked           = 0;
	int  num_valid_pages         = 0;
	int  num_invalid_pages       = 0;
	bool file_tail_page_valid    = false;

	if (!decmpfs_trylock_compressed_data(cp, 0)) {
		return EAGAIN;
	}
	cmpdata_locked = 1;


	if (flags & ~(UPL_IOSYNC | UPL_NOCOMMIT | UPL_NORDAHEAD)) {
		DebugLogWithPath("pagein: unknown flags 0x%08x\n", (flags & ~(UPL_IOSYNC | UPL_NOCOMMIT | UPL_NORDAHEAD)));
	}

	err = decmpfs_fetch_compressed_header(vp, cp, &hdr, 0, &alloc_size);
	if (err != 0) {
		decmpfs_ktriage_record(KTRIAGE_DECMPFS_FETCH_HEADER_FAILED, err);
		goto out;
	}

	cachedSize = hdr->uncompressed_size;

	if (!compression_type_valid(vp, hdr)) {
		/* compressor not registered */
		decmpfs_ktriage_record(KTRIAGE_DECMPFS_COMPRESSOR_NOT_REGISTERED, hdr->compression_type);
		err = ENOTSUP;
		goto out;
	}

	/*
	 * can't page-in from a negative offset
	 * or if we're starting beyond the EOF
	 * or if the file offset isn't page aligned
	 * or the size requested isn't a multiple of PAGE_SIZE
	 */
	if (f_offset < 0 || f_offset >= cachedSize ||
	    (f_offset & PAGE_MASK_64) || (size & PAGE_MASK) || (pl_offset & PAGE_MASK)) {
		decmpfs_ktriage_record(KTRIAGE_DECMPFS_IVALID_OFFSET, 0);
		err = EINVAL;
		goto out;
	}

	/*
	 * If the verify block size is larger than the page size, the UPL needs
	 * to be aligned to it, Since the UPL has been created by the filesystem,
	 * we will only check if the passed in UPL length conforms to the
	 * alignment requirements.
	 */
	err = VNOP_VERIFY(vp, f_offset, NULL, 0, &verify_block_size, NULL,
	    VNODE_VERIFY_DEFAULT, NULL);
	if (err) {
		ErrorLogWithPath("VNOP_VERIFY returned error = %d\n", err);
		goto out;
	} else if (verify_block_size) {
		if (vp->v_mount->mnt_vfsstat.f_bsize > PAGE_SIZE) {
			fs_bsize = vp->v_mount->mnt_vfsstat.f_bsize;
		}
		if (verify_block_size & (verify_block_size - 1)) {
			ErrorLogWithPath("verify block size (%zu) is not power of 2, no verification will be done\n", verify_block_size);
			err = EINVAL;
		} else if (size % verify_block_size) {
			ErrorLogWithPath("upl size (%zu) is not a multiple of verify block size (%zu)\n", (size_t)size, verify_block_size);
			err = EINVAL;
		} else if (fs_bsize) {
			/*
			 * Filesystems requesting verification have to provide
			 * values for block sizes which are powers of 2.
			 */
			if (fs_bsize & (fs_bsize - 1)) {
				ErrorLogWithPath("FS block size (%u) is greater than PAGE_SIZE (%d) and is not power of 2, no verification will be done\n",
				    fs_bsize, PAGE_SIZE);
				err = EINVAL;
			} else if (fs_bsize > verify_block_size) {
				ErrorLogWithPath("FS block size (%u) is greater than verify block size (%zu), no verification will be done\n",
				    fs_bsize, verify_block_size);
				err = EINVAL;
			}
		}
		if (err) {
			goto out;
		}
	}

#if CONFIG_IOSCHED
	/* Mark the UPL as the requesting UPL for decompression */
	upl_mark_decmp(pl);
#endif /* CONFIG_IOSCHED */

	/* map the upl so we can fetch into it */
	kern_return_t kr = ubc_upl_map(pl, (vm_offset_t*)&data);
	if ((kr != KERN_SUCCESS) || (data == NULL)) {
		decmpfs_ktriage_record(KTRIAGE_DECMPFS_UBC_UPL_MAP_FAILED, kr);
		err = ENOSPC;
		data = NULL;
#if CONFIG_IOSCHED
		upl_unmark_decmp(pl);
#endif /* CONFIG_IOSCHED */
		goto out;
	}

	uplPos = f_offset;
	off_t max_size = cachedSize - f_offset;

	if (size < max_size) {
		rounded_uplSize = uplSize = size;
		file_tail_page_valid = true;
	} else {
		uplSize = (user_ssize_t)max_size;
		if (fs_bsize) {
			/* First round up to fs_bsize */
			rounded_uplSize = (uplSize + (fs_bsize - 1)) & ~(fs_bsize - 1);
			/* then to PAGE_SIZE */
			rounded_uplSize = MIN(size, round_page((vm_offset_t)rounded_uplSize));
		} else {
			rounded_uplSize = round_page((vm_offset_t)uplSize);
		}
	}

	/* do the fetch */
	decmpfs_vector vec;

decompress:
	/* the mapped data pointer points to the first page of the page list, so we want to start filling in at an offset of pl_offset */
	vec = (decmpfs_vector) {
		.buf = (char*)data + pl_offset,
		.size = size,
	};

	uint64_t did_read = 0;
	if (decmpfs_fast_get_state(cp) == FILE_IS_CONVERTING) {
		ErrorLogWithPath("unexpected pagein during decompress\n");
		/*
		 *  if the file is converting, this must be a recursive call to pagein from underneath a call to decmpfs_decompress_file;
		 *  pretend that it succeeded but don't do anything since we're just going to write over the pages anyway
		 */
		err = 0;
	} else {
		if (verify_block_size <= PAGE_SIZE) {
			err = decmpfs_fetch_uncompressed_data(vp, cp, hdr, uplPos, uplSize, 1, &vec, &did_read);
			/* zero out whatever wasn't read */
			if (did_read < rounded_uplSize) {
				memset((char*)vec.buf + did_read, 0, (size_t)(rounded_uplSize - did_read));
			}
		} else {
			off_t l_uplPos = uplPos;
			off_t l_pl_offset = pl_offset;
			user_ssize_t l_uplSize = uplSize;
			upl_page_info_t *pl_info = ubc_upl_pageinfo(pl);

			err = 0;
			/*
			 * When the system page size is less than the "verify block size",
			 * the UPL passed may not consist solely of absent pages.
			 * We have to detect the "absent" pages and only decompress
			 * into those absent/invalid page ranges.
			 *
			 * Things that will change in each iteration of the loop :
			 *
			 * l_pl_offset = where we are inside the UPL [0, caller_upl_created_size)
			 * l_uplPos = the file offset the l_pl_offset corresponds to.
			 * l_uplSize = the size of the upl still unprocessed;
			 *
			 * In this picture, we have to do the transfer on 2 ranges
			 * (One 2 page range and one 3 page range) and the loop
			 * below will skip the first two pages and then identify
			 * the next two as invalid and fill those in and
			 * then skip the next one and then do the last pages.
			 *
			 *                          uplPos(file_offset)
			 *                            |   uplSize
			 * 0                          V<-------------->    file_size
			 * |--------------------------------------------------->
			 *                        | | |V|V|I|I|V|I|I|I|
			 *                            ^
			 *                            |    upl
			 *                        <------------------->
			 *                            |
			 *                          pl_offset
			 *
			 * uplSize will be clipped in case the UPL range exceeds
			 * the file size.
			 *
			 */
			while (l_uplSize) {
				uint64_t l_did_read = 0;
				int pl_offset_pg = (int)(l_pl_offset / PAGE_SIZE);
				int pages_left_in_upl;
				int start_pg;
				int last_pg;

				/*
				 * l_uplSize may start off less than the size of the upl,
				 * we have to round it up to PAGE_SIZE to calculate
				 * how many more pages are left.
				 */
				pages_left_in_upl = (int)(round_page((vm_offset_t)l_uplSize) / PAGE_SIZE);

				/*
				 * scan from the beginning of the upl looking for the first
				 * non-valid page.... this will become the first page in
				 * the request we're going to make to
				 * 'decmpfs_fetch_uncompressed_data'... if all
				 * of the pages are valid, we won't call through
				 * to 'decmpfs_fetch_uncompressed_data'
				 */
				for (start_pg = 0; start_pg < pages_left_in_upl; start_pg++) {
					if (!upl_valid_page(pl_info, pl_offset_pg + start_pg)) {
						break;
					}
				}

				num_valid_pages += start_pg;

				/*
				 * scan from the starting invalid page looking for
				 * a valid page before the end of the upl is
				 * reached, if we find one, then it will be the
				 * last page of the request to 'decmpfs_fetch_uncompressed_data'
				 */
				for (last_pg = start_pg; last_pg < pages_left_in_upl; last_pg++) {
					if (upl_valid_page(pl_info, pl_offset_pg + last_pg)) {
						break;
					}
				}

				if (start_pg < last_pg) {
					off_t inval_offset = start_pg * PAGE_SIZE;
					int inval_pages = last_pg - start_pg;
					int inval_size = inval_pages * PAGE_SIZE;
					decmpfs_vector l_vec;

					num_invalid_pages += inval_pages;
					if (inval_offset) {
						did_read += inval_offset;
						l_pl_offset += inval_offset;
						l_uplPos += inval_offset;
						l_uplSize -= inval_offset;
					}

					l_vec = (decmpfs_vector) {
						.buf = (char*)data + l_pl_offset,
						.size = inval_size,
					};

					err = decmpfs_fetch_uncompressed_data(vp, cp, hdr, l_uplPos,
					    MIN(l_uplSize, inval_size), 1, &l_vec, &l_did_read);

					if (!err && (l_did_read != inval_size) && (l_uplSize > inval_size)) {
						ErrorLogWithPath("Unexpected size fetch of decompressed data, l_uplSize = %d, l_did_read = %d, inval_size = %d\n",
						    (int)l_uplSize, (int)l_did_read, (int)inval_size);
						err = EINVAL;
					}
				} else {
					/* no invalid pages left */
					l_did_read = l_uplSize;
					if (!file_tail_page_valid) {
						file_tail_page_valid = true;
					}
				}

				if (err) {
					break;
				}

				did_read += l_did_read;
				l_pl_offset += l_did_read;
				l_uplPos += l_did_read;
				l_uplSize -= l_did_read;
			}

			/* Zero out the region after EOF in the last page (if needed) */
			if (!err && !file_tail_page_valid && (uplSize < rounded_uplSize)) {
				memset((char*)vec.buf + uplSize, 0, (size_t)(rounded_uplSize - uplSize));
			}
		}
	}
	if (err) {
		decmpfs_ktriage_record(KTRIAGE_DECMPFS_FETCH_UNCOMPRESSED_DATA_FAILED, err)
		DebugLogWithPath("decmpfs_fetch_uncompressed_data err %d\n", err);
		int cmp_state = decmpfs_fast_get_state(cp);
		if (cmp_state == FILE_IS_CONVERTING) {
			DebugLogWithPath("cmp_state == FILE_IS_CONVERTING\n");
			cmp_state = wait_for_decompress(cp);
			if (cmp_state == FILE_IS_COMPRESSED) {
				DebugLogWithPath("cmp_state == FILE_IS_COMPRESSED\n");
				/* a decompress was attempted but it failed, let's try calling fetch again */
				goto decompress;
			}
		}
		if (cmp_state == FILE_IS_NOT_COMPRESSED) {
			DebugLogWithPath("cmp_state == FILE_IS_NOT_COMPRESSED\n");
			/* the file was decompressed after we started reading it */
			*is_compressed = 0; /* instruct caller to fall back to its normal path */
		}
	}

	if (!err && verify_block_size) {
		size_t cur_verify_block_size = verify_block_size;

		if ((err = VNOP_VERIFY(vp, uplPos, vec.buf, rounded_uplSize, &cur_verify_block_size, NULL, 0, NULL))) {
			ErrorLogWithPath("Verification failed with error %d, uplPos = %lld, uplSize = %d, did_read = %d, valid_pages = %d, invalid_pages = %d, tail_page_valid = %d\n",
			    err, (long long)uplPos, (int)rounded_uplSize, (int)did_read, num_valid_pages, num_invalid_pages, file_tail_page_valid);
		}
		/* XXX : If the verify block size changes, redo the read */
	}

#if CONFIG_IOSCHED
	upl_unmark_decmp(pl);
#endif /* CONFIG_IOSCHED */

	kr = ubc_upl_unmap(pl); data = NULL; /* make sure to set data to NULL so we don't try to unmap again below */
	if (kr != KERN_SUCCESS) {
		ErrorLogWithPath("ubc_upl_unmap error %d\n", (int)kr);
	} else {
		if (!err) {
			/* commit our pages */
			kr = commit_upl(pl, pl_offset, (size_t)rounded_uplSize, UPL_COMMIT_FREE_ON_EMPTY, 0 /* commit */);
			/* If there were any pages after the page containing EOF, abort them. */
			if (rounded_uplSize < size) {
				kr = commit_upl(pl, (upl_offset_t)(pl_offset + rounded_uplSize), (size_t)(size - rounded_uplSize),
				    UPL_ABORT_FREE_ON_EMPTY | UPL_ABORT_ERROR, 1 /* abort */);
			}
		}
	}

out:
	if (data) {
		ubc_upl_unmap(pl);
	}
	if (hdr != NULL) {
		kfree_data(hdr, alloc_size);
	}
	if (cmpdata_locked) {
		decmpfs_unlock_compressed_data(cp, 0);
	}
	if (err) {
#if 0
		if (err != ENXIO && err != ENOSPC) {
			char *path = zalloc(ZV_NAMEI);
			panic("%s: decmpfs_pagein_compressed: err %d", vnpath(vp, path, PATH_MAX), err);
			zfree(ZV_NAMEI, path);
		}
#endif /* 0 */
		ErrorLogWithPath("err %d\n", err);
	}
	return err;
}

errno_t
decmpfs_read_compressed(struct vnop_read_args *ap, int *is_compressed, decmpfs_cnode *cp)
{
	/* handles a read request from vfs for a compressed file */

	uio_t uio                    = ap->a_uio;
	vnode_t vp                   = ap->a_vp;
	int err                      = 0;
	int countInt                 = 0;
	off_t uplPos                 = 0;
	user_ssize_t uplSize         = 0;
	user_ssize_t uplRemaining    = 0;
	off_t curUplPos              = 0;
	user_ssize_t curUplSize      = 0;
	kern_return_t kr             = KERN_SUCCESS;
	int abort_read               = 0;
	void *data                   = NULL;
	uint64_t did_read            = 0;
	upl_t upl                    = NULL;
	upl_page_info_t *pli         = NULL;
	decmpfs_header *hdr          = NULL;
	size_t alloc_size            = 0;
	uint64_t cachedSize          = 0;
	off_t uioPos                 = 0;
	user_ssize_t uioRemaining    = 0;
	size_t verify_block_size     = 0;
	size_t alignment_size        = PAGE_SIZE;
	int cmpdata_locked           = 0;

	decmpfs_lock_compressed_data(cp, 0); cmpdata_locked = 1;

	uplPos = uio_offset(uio);
	uplSize = uio_resid(uio);
	VerboseLogWithPath("uplPos %lld uplSize %lld\n", uplPos, uplSize);

	cachedSize = decmpfs_cnode_get_vnode_cached_size(cp);

	if ((uint64_t)uplPos + uplSize > cachedSize) {
		/* truncate the read to the size of the file */
		uplSize = (user_ssize_t)(cachedSize - uplPos);
	}

	/* give the cluster layer a chance to fill in whatever it already has */
	countInt = (uplSize > INT_MAX) ? INT_MAX : (int)uplSize;
	err = cluster_copy_ubc_data(vp, uio, &countInt, 0);
	if (err != 0) {
		goto out;
	}

	/* figure out what's left */
	uioPos = uio_offset(uio);
	uioRemaining = uio_resid(uio);
	if ((uint64_t)uioPos + uioRemaining > cachedSize) {
		/* truncate the read to the size of the file */
		uioRemaining = (user_ssize_t)(cachedSize - uioPos);
	}

	if (uioRemaining <= 0) {
		/* nothing left */
		goto out;
	}

	err = decmpfs_fetch_compressed_header(vp, cp, &hdr, 0, &alloc_size);
	if (err != 0) {
		goto out;
	}
	if (!compression_type_valid(vp, hdr)) {
		err = ENOTSUP;
		goto out;
	}

	uplPos = uioPos;
	uplSize = uioRemaining;
#if COMPRESSION_DEBUG
	DebugLogWithPath("uplPos %lld uplSize %lld\n", (uint64_t)uplPos, (uint64_t)uplSize);
#endif

	lck_rw_lock_shared(&decompressorsLock);
	decmpfs_adjust_fetch_region_func adjust_fetch = decmp_get_func(vp, hdr->compression_type, adjust_fetch);
	if (adjust_fetch) {
		/* give the compressor a chance to adjust the portion of the file that we read */
		adjust_fetch(vp, decmpfs_ctx, hdr, &uplPos, &uplSize);
		VerboseLogWithPath("adjusted uplPos %lld uplSize %lld\n", (uint64_t)uplPos, (uint64_t)uplSize);
	}
	lck_rw_unlock_shared(&decompressorsLock);

	/* clip the adjusted size to the size of the file */
	if ((uint64_t)uplPos + uplSize > cachedSize) {
		/* truncate the read to the size of the file */
		uplSize = (user_ssize_t)(cachedSize - uplPos);
	}

	if (uplSize <= 0) {
		/* nothing left */
		goto out;
	}

	/*
	 *  since we're going to create a upl for the given region of the file,
	 *  make sure we're on page boundaries
	 */

	/* If the verify block size is larger than the page size, the UPL needs to aligned to it */
	err = VNOP_VERIFY(vp, uplPos, NULL, 0, &verify_block_size, NULL, VNODE_VERIFY_DEFAULT, NULL);
	if (err) {
		goto out;
	} else if (verify_block_size) {
		if (verify_block_size & (verify_block_size - 1)) {
			ErrorLogWithPath("verify block size is not power of 2, no verification will be done\n");
			verify_block_size = 0;
		} else if (verify_block_size > PAGE_SIZE) {
			alignment_size = verify_block_size;
		}
	}

	if (uplPos & (alignment_size - 1)) {
		/* round position down to page boundary */
		uplSize += (uplPos & (alignment_size - 1));
		uplPos &= ~(alignment_size - 1);
	}

	/* round size up to alignement_size multiple */
	uplSize = (uplSize + (alignment_size - 1)) & ~(alignment_size - 1);

	VerboseLogWithPath("new uplPos %lld uplSize %lld\n", (uint64_t)uplPos, (uint64_t)uplSize);

	uplRemaining = uplSize;
	curUplPos = uplPos;
	curUplSize = 0;

	while (uplRemaining > 0) {
		/* start after the last upl */
		curUplPos += curUplSize;

		/* clip to max upl size */
		curUplSize = uplRemaining;
		if (curUplSize > MAX_UPL_SIZE_BYTES) {
			curUplSize = MAX_UPL_SIZE_BYTES;
		}

		/* create the upl */
		kr = ubc_create_upl_kernel(vp, curUplPos, (int)curUplSize, &upl, &pli, UPL_SET_LITE, VM_KERN_MEMORY_FILE);
		if (kr != KERN_SUCCESS) {
			ErrorLogWithPath("ubc_create_upl error %d\n", (int)kr);
			err = EINVAL;
			goto out;
		}
		VerboseLogWithPath("curUplPos %lld curUplSize %lld\n", (uint64_t)curUplPos, (uint64_t)curUplSize);

#if CONFIG_IOSCHED
		/* Mark the UPL as the requesting UPL for decompression */
		upl_mark_decmp(upl);
#endif /* CONFIG_IOSCHED */

		/* map the upl */
		kr = ubc_upl_map(upl, (vm_offset_t*)&data);
		if (kr != KERN_SUCCESS) {
			commit_upl(upl, 0, curUplSize, UPL_ABORT_FREE_ON_EMPTY, 1);
#if 0
			char *path = zalloc(ZV_NAMEI);
			panic("%s: decmpfs_read_compressed: ubc_upl_map error %d", vnpath(vp, path, PATH_MAX), (int)kr);
			zfree(ZV_NAMEI, path);
#else /* 0 */
			ErrorLogWithPath("ubc_upl_map kr=0x%x\n", (int)kr);
#endif /* 0 */
			err = EINVAL;
			goto out;
		}

		/* make sure the map succeeded */
		if (!data) {
			commit_upl(upl, 0, curUplSize, UPL_ABORT_FREE_ON_EMPTY, 1);

			ErrorLogWithPath("ubc_upl_map mapped null\n");
			err = EINVAL;
			goto out;
		}

		/* fetch uncompressed data into the mapped upl */
		decmpfs_vector vec;
decompress:
		vec = (decmpfs_vector){ .buf = data, .size = curUplSize };
		err = decmpfs_fetch_uncompressed_data(vp, cp, hdr, curUplPos, curUplSize, 1, &vec, &did_read);
		if (err) {
			ErrorLogWithPath("decmpfs_fetch_uncompressed_data err %d\n", err);

			/* maybe the file is converting to decompressed */
			int cmp_state = decmpfs_fast_get_state(cp);
			if (cmp_state == FILE_IS_CONVERTING) {
				ErrorLogWithPath("cmp_state == FILE_IS_CONVERTING\n");
				cmp_state = wait_for_decompress(cp);
				if (cmp_state == FILE_IS_COMPRESSED) {
					ErrorLogWithPath("cmp_state == FILE_IS_COMPRESSED\n");
					/* a decompress was attempted but it failed, let's try fetching again */
					goto decompress;
				}
			}
			if (cmp_state == FILE_IS_NOT_COMPRESSED) {
				ErrorLogWithPath("cmp_state == FILE_IS_NOT_COMPRESSED\n");
				/* the file was decompressed after we started reading it */
				*is_compressed = 0; /* instruct caller to fall back to its normal path */
			}
			kr = KERN_FAILURE;
			abort_read = 1; /* we're not going to commit our data */
			did_read = 0;
		}

		/* zero out the remainder of the last page */
		memset((char*)data + did_read, 0, (size_t)(curUplSize - did_read));
		if (!err && verify_block_size) {
			size_t cur_verify_block_size = verify_block_size;

			if ((err = VNOP_VERIFY(vp, curUplPos, data, curUplSize, &cur_verify_block_size, NULL, 0, NULL))) {
				ErrorLogWithPath("Verification failed with error %d\n", err);
				abort_read = 1;
			}
			/* XXX : If the verify block size changes, redo the read */
		}

		kr = ubc_upl_unmap(upl);
		if (kr != KERN_SUCCESS) {
			/* A failure to unmap here will eventually cause a panic anyway */
			panic("ubc_upl_unmap returned error %d (kern_return_t)", (int)kr);
		}

		if (abort_read) {
			kr = commit_upl(upl, 0, curUplSize, UPL_ABORT_FREE_ON_EMPTY, 1);
		} else {
			VerboseLogWithPath("uioPos %lld uioRemaining %lld\n", (uint64_t)uioPos, (uint64_t)uioRemaining);
			if (uioRemaining) {
				off_t uplOff = uioPos - curUplPos;
				if (uplOff < 0) {
					ErrorLogWithPath("uplOff %lld should never be negative\n", (int64_t)uplOff);
					err = EINVAL;
				} else if (uplOff > INT_MAX) {
					ErrorLogWithPath("uplOff %lld too large\n", (int64_t)uplOff);
					err = EINVAL;
				} else {
					off_t count = curUplPos + curUplSize - uioPos;
					if (count < 0) {
						/* this upl is entirely before the uio */
					} else {
						if (count > uioRemaining) {
							count = uioRemaining;
						}
						int icount = (count > INT_MAX) ? INT_MAX : (int)count;
						int io_resid = icount;
						err = cluster_copy_upl_data(uio, upl, (int)uplOff, &io_resid);
						int copied = icount - io_resid;
						VerboseLogWithPath("uplOff %lld count %lld copied %lld\n", (uint64_t)uplOff, (uint64_t)count, (uint64_t)copied);
						if (err) {
							ErrorLogWithPath("cluster_copy_upl_data err %d\n", err);
						}
						uioPos += copied;
						uioRemaining -= copied;
					}
				}
			}
			kr = commit_upl(upl, 0, curUplSize, UPL_COMMIT_FREE_ON_EMPTY | UPL_COMMIT_INACTIVATE, 0);
		}

		if (err) {
			goto out;
		}

		uplRemaining -= curUplSize;
	}

out:

	if (hdr != NULL) {
		kfree_data(hdr, alloc_size);
	}
	if (cmpdata_locked) {
		decmpfs_unlock_compressed_data(cp, 0);
	}
	if (err) {/* something went wrong */
		ErrorLogWithPath("err %d\n", err);
		return err;
	}

#if COMPRESSION_DEBUG
	uplSize = uio_resid(uio);
	if (uplSize) {
		VerboseLogWithPath("still %lld bytes to copy\n", uplSize);
	}
#endif
	return 0;
}

int
decmpfs_free_compressed_data(vnode_t vp, decmpfs_cnode *cp)
{
	/*
	 *  call out to the decompressor to free remove any data associated with this compressed file
	 *  then delete the file's compression xattr
	 */
	decmpfs_header *hdr = NULL;
	size_t alloc_size = 0;

	/*
	 * Trace the following parameters on entry with event-id 0x03120010.
	 *
	 * @vp->v_id:       vnode-id of the file for which to free compressed data.
	 */
	DECMPFS_EMIT_TRACE_ENTRY(DECMPDBG_FREE_COMPRESSED_DATA, vp->v_id);

	int err = decmpfs_fetch_compressed_header(vp, cp, &hdr, 0, &alloc_size);
	if (err) {
		ErrorLogWithPath("decmpfs_fetch_compressed_header err %d\n", err);
	} else {
		lck_rw_lock_shared(&decompressorsLock);
		decmpfs_free_compressed_data_func free_data = decmp_get_func(vp, hdr->compression_type, free_data);
		if (free_data) {
			err = free_data(vp, decmpfs_ctx, hdr);
		} else {
			/* nothing to do, so no error */
			err = 0;
		}
		lck_rw_unlock_shared(&decompressorsLock);

		if (err != 0) {
			ErrorLogWithPath("decompressor err %d\n", err);
		}
	}
	/*
	 * Trace the following parameters on return with event-id 0x03120010.
	 *
	 * @vp->v_id:       vnode-id of the file for which to free compressed data.
	 * @err:            value returned from this function.
	 */
	DECMPFS_EMIT_TRACE_RETURN(DECMPDBG_FREE_COMPRESSED_DATA, vp->v_id, err);

	/* delete the xattr */
	err = vn_removexattr(vp, DECMPFS_XATTR_NAME, 0, decmpfs_ctx);

	if (hdr != NULL) {
		kfree_data(hdr, alloc_size);
	}
	return err;
}

#pragma mark --- file conversion routines ---

static int
unset_compressed_flag(vnode_t vp)
{
	int err = 0;
	struct vnode_attr va;
	struct fsioc_cas_bsdflags cas;
	int i;

# define MAX_CAS_BSDFLAGS_LOOPS 4
	/* UF_COMPRESSED should be manipulated only with FSIOC_CAS_BSDFLAGS */
	for (i = 0; i < MAX_CAS_BSDFLAGS_LOOPS; i++) {
		VATTR_INIT(&va);
		VATTR_WANTED(&va, va_flags);
		err = vnode_getattr(vp, &va, decmpfs_ctx);
		if (err != 0) {
			ErrorLogWithPath("vnode_getattr err %d, num retries %d\n", err, i);
			goto out;
		}

		cas.expected_flags = va.va_flags;
		cas.new_flags = va.va_flags & ~UF_COMPRESSED;
		err = VNOP_IOCTL(vp, FSIOC_CAS_BSDFLAGS, (caddr_t)&cas, FWRITE, decmpfs_ctx);

		if ((err == 0) && (va.va_flags == cas.actual_flags)) {
			goto out;
		}

		if ((err != 0) && (err != EAGAIN)) {
			break;
		}
	}

	/* fallback to regular chflags if FSIOC_CAS_BSDFLAGS is not supported */
	if (err == ENOTTY) {
		VATTR_INIT(&va);
		VATTR_SET(&va, va_flags, cas.new_flags);
		err = vnode_setattr(vp, &va, decmpfs_ctx);
		if (err != 0) {
			ErrorLogWithPath("vnode_setattr err %d\n", err);
		}
	} else if (va.va_flags != cas.actual_flags) {
		ErrorLogWithPath("FSIOC_CAS_BSDFLAGS err: flags mismatc. actual (%x) expected (%x), num retries %d\n", cas.actual_flags, va.va_flags, i);
	} else if (err != 0) {
		ErrorLogWithPath("FSIOC_CAS_BSDFLAGS err %d, num retries %d\n", err, i);
	}

out:
	return err;
}

int
decmpfs_decompress_file(vnode_t vp, decmpfs_cnode *cp, off_t toSize, int truncate_okay, int skiplock)
{
	/* convert a compressed file to an uncompressed file */

	int err                      = 0;
	char *data                   = NULL;
	uio_t uio_w                  = 0;
	off_t offset                 = 0;
	uint32_t old_state           = 0;
	uint32_t new_state           = 0;
	int update_file_state        = 0;
	size_t allocSize             = 0;
	decmpfs_header *hdr          = NULL;
	size_t hdr_size              = 0;
	int cmpdata_locked           = 0;
	off_t remaining              = 0;
	uint64_t uncompressed_size   = 0;

	/*
	 * Trace the following parameters on entry with event-id 0x03120000.
	 *
	 * @vp->v_id:		vnode-id of the file being decompressed.
	 * @toSize:		uncompress given bytes of the file.
	 * @truncate_okay:	on error it is OK to truncate.
	 * @skiplock:		compressed data is locked, skip locking again.
	 *
	 * Please NOTE: @toSize can overflow in theory but here it is safe.
	 */
	DECMPFS_EMIT_TRACE_ENTRY(DECMPDBG_DECOMPRESS_FILE, vp->v_id,
	    (int)toSize, truncate_okay, skiplock);

	if (!skiplock) {
		decmpfs_lock_compressed_data(cp, 1); cmpdata_locked = 1;
	}

decompress:
	old_state = decmpfs_fast_get_state(cp);

	switch (old_state) {
	case FILE_IS_NOT_COMPRESSED:
	{
		/* someone else decompressed the file */
		err = 0;
		goto out;
	}

	case FILE_TYPE_UNKNOWN:
	{
		/* the file is in an unknown state, so update the state and retry */
		(void)decmpfs_file_is_compressed(vp, cp);

		/* try again */
		goto decompress;
	}

	case FILE_IS_COMPRESSED:
	{
		/* the file is compressed, so decompress it */
		break;
	}

	default:
	{
		/*
		 *  this shouldn't happen since multiple calls to decmpfs_decompress_file lock each other out,
		 *  and when decmpfs_decompress_file returns, the state should be always be set back to
		 *  FILE_IS_NOT_COMPRESSED or FILE_IS_UNKNOWN
		 */
		err = EINVAL;
		goto out;
	}
	}

	err = decmpfs_fetch_compressed_header(vp, cp, &hdr, 0, &hdr_size);
	if (err != 0) {
		goto out;
	}

	uncompressed_size = hdr->uncompressed_size;
	if (toSize == -1) {
		toSize = hdr->uncompressed_size;
	}

	if (toSize == 0) {
		/* special case truncating the file to zero bytes */
		goto nodecmp;
	} else if ((uint64_t)toSize > hdr->uncompressed_size) {
		/* the caller is trying to grow the file, so we should decompress all the data */
		toSize = hdr->uncompressed_size;
	}

	allocSize = MIN(64 * 1024, (size_t)toSize);
	data = (char *)kalloc_data(allocSize, Z_WAITOK);
	if (!data) {
		err = ENOMEM;
		goto out;
	}

	uio_w = uio_create(1, 0LL, UIO_SYSSPACE, UIO_WRITE);
	if (!uio_w) {
		err = ENOMEM;
		goto out;
	}
	uio_w->uio_flags |= UIO_FLAGS_IS_COMPRESSED_FILE;

	remaining = toSize;

	/* tell the buffer cache that this is an empty file */
	ubc_setsize(vp, 0);

	/* if we got here, we need to decompress the file */
	decmpfs_cnode_set_vnode_state(cp, FILE_IS_CONVERTING, 1);

	while (remaining > 0) {
		/* loop decompressing data from the file and writing it into the data fork */

		uint64_t bytes_read = 0;
		decmpfs_vector vec = { .buf = data, .size = (user_ssize_t)MIN(allocSize, remaining) };
		err = decmpfs_fetch_uncompressed_data(vp, cp, hdr, offset, vec.size, 1, &vec, &bytes_read);
		if (err != 0) {
			ErrorLogWithPath("decmpfs_fetch_uncompressed_data err %d\n", err);
			goto out;
		}

		if (bytes_read == 0) {
			/* we're done reading data */
			break;
		}

		uio_reset(uio_w, offset, UIO_SYSSPACE, UIO_WRITE);
		err = uio_addiov(uio_w, CAST_USER_ADDR_T(data), (user_size_t)bytes_read);
		if (err != 0) {
			ErrorLogWithPath("uio_addiov err %d\n", err);
			err = ENOMEM;
			goto out;
		}

		err = VNOP_WRITE(vp, uio_w, 0, decmpfs_ctx);
		if (err != 0) {
			/* if the write failed, truncate the file to zero bytes */
			ErrorLogWithPath("VNOP_WRITE err %d\n", err);
			break;
		}
		offset += bytes_read;
		remaining -= bytes_read;
	}

	if (err == 0) {
		if (offset != toSize) {
			ErrorLogWithPath("file decompressed to %lld instead of %lld\n", offset, toSize);
			err = EINVAL;
			goto out;
		}
	}

	if (err == 0) {
		/* sync the data and metadata */
		err = VNOP_FSYNC(vp, MNT_WAIT, decmpfs_ctx);
		if (err != 0) {
			ErrorLogWithPath("VNOP_FSYNC err %d\n", err);
			goto out;
		}
	}

	if (err != 0) {
		/* write, setattr, or fsync failed */
		ErrorLogWithPath("aborting decompress, err %d\n", err);
		if (truncate_okay) {
			/* truncate anything we might have written */
			int error = vnode_setsize(vp, 0, 0, decmpfs_ctx);
			ErrorLogWithPath("vnode_setsize err %d\n", error);
		}
		goto out;
	}

nodecmp:
	/* if we're truncating the file to zero bytes, we'll skip ahead to here */

	/* unset the compressed flag */
	unset_compressed_flag(vp);

	/* free the compressed data associated with this file */
	err = decmpfs_free_compressed_data(vp, cp);
	if (err != 0) {
		ErrorLogWithPath("decmpfs_free_compressed_data err %d\n", err);
	}

	/*
	 *  even if free_compressed_data or vnode_getattr/vnode_setattr failed, return success
	 *  since we succeeded in writing all of the file data to the data fork
	 */
	err = 0;

	/* if we got this far, the file was successfully decompressed */
	update_file_state = 1;
	new_state = FILE_IS_NOT_COMPRESSED;

#if COMPRESSION_DEBUG
	{
		uint64_t filesize = 0;
		vnsize(vp, &filesize);
		DebugLogWithPath("new file size %lld\n", filesize);
	}
#endif

out:
	if (hdr != NULL) {
		kfree_data(hdr, hdr_size);
	}
	kfree_data(data, allocSize);

	if (uio_w) {
		uio_free(uio_w);
	}

	if (err != 0) {
		/* if there was a failure, reset compression flags to unknown and clear the buffer cache data */
		update_file_state = 1;
		new_state = FILE_TYPE_UNKNOWN;
		if (uncompressed_size) {
			ubc_setsize(vp, 0);
			ubc_setsize(vp, uncompressed_size);
		}
	}

	if (update_file_state) {
		lck_mtx_lock(&decompress_channel_mtx);
		decmpfs_cnode_set_vnode_state(cp, new_state, 1);
		wakeup((caddr_t)&decompress_channel); /* wake up anyone who might have been waiting for decompression */
		lck_mtx_unlock(&decompress_channel_mtx);
	}

	if (cmpdata_locked) {
		decmpfs_unlock_compressed_data(cp, 1);
	}
	/*
	 * Trace the following parameters on return with event-id 0x03120000.
	 *
	 * @vp->v_id:	vnode-id of the file being decompressed.
	 * @err:	value returned from this function.
	 */
	DECMPFS_EMIT_TRACE_RETURN(DECMPDBG_DECOMPRESS_FILE, vp->v_id, err);
	return err;
}

#pragma mark --- Type1 compressor ---

/*
 *  The "Type1" compressor stores the data fork directly in the compression xattr
 */

static int
decmpfs_validate_compressed_file_Type1(__unused vnode_t vp, __unused vfs_context_t ctx, decmpfs_header *hdr)
{
	int err          = 0;

	if (hdr->uncompressed_size + sizeof(decmpfs_disk_header) != (uint64_t)hdr->attr_size) {
		err = EINVAL;
		goto out;
	}
out:
	return err;
}

static int
decmpfs_fetch_uncompressed_data_Type1(__unused vnode_t vp, __unused vfs_context_t ctx, decmpfs_header *hdr, off_t offset, user_ssize_t size, int nvec, decmpfs_vector *vec, uint64_t *bytes_read)
{
	int err          = 0;
	int i;
	user_ssize_t remaining;

	if (hdr->uncompressed_size + sizeof(decmpfs_disk_header) != (uint64_t)hdr->attr_size) {
		err = EINVAL;
		goto out;
	}

#if COMPRESSION_DEBUG
	static int dummy = 0; // prevent syslog from coalescing printfs
	DebugLogWithPath("%d memcpy %lld at %lld\n", dummy++, size, (uint64_t)offset);
#endif

	remaining = size;
	for (i = 0; (i < nvec) && (remaining > 0); i++) {
		user_ssize_t curCopy = vec[i].size;
		if (curCopy > remaining) {
			curCopy = remaining;
		}
		memcpy(vec[i].buf, hdr->attr_bytes + offset, curCopy);
		offset += curCopy;
		remaining -= curCopy;
	}

	if ((bytes_read) && (err == 0)) {
		*bytes_read = (size - remaining);
	}

out:
	return err;
}

SECURITY_READ_ONLY_EARLY(static decmpfs_registration) Type1Reg =
{
	.decmpfs_registration = DECMPFS_REGISTRATION_VERSION,
	.validate          = decmpfs_validate_compressed_file_Type1,
	.adjust_fetch      = NULL,/* no adjust necessary */
	.fetch             = decmpfs_fetch_uncompressed_data_Type1,
	.free_data         = NULL,/* no free necessary */
	.get_flags         = NULL/* no flags */
};

#pragma mark --- decmpfs initialization ---

void
decmpfs_init(void)
{
	static int done = 0;
	if (done) {
		return;
	}

	decmpfs_ctx = vfs_context_create(vfs_context_kernel());

	register_decmpfs_decompressor(CMP_Type1, &Type1Reg);

	ktriage_register_subsystem_strings(KDBG_TRIAGE_SUBSYS_DECMPFS, &ktriage_decmpfs_subsystem_strings);

	done = 1;
}
#endif /* FS_COMPRESSION */
