/*
 * Copyright (c) 1998-2016 Apple Inc. All rights reserved.
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

#include <sys/sysctl.h>
extern "C" {
#include <vm/vm_kern_xnu.h>
#include <kern/task.h>
#include <kern/debug.h>
}

#include <libkern/c++/OSContainers.h>
#include <libkern/OSDebug.h>
#include <libkern/c++/OSCPPDebug.h>
#include <kern/backtrace.h>
#include <kern/btlog.h>

#include <IOKit/IOKitDebug.h>
#include <IOKit/IOLib.h>
#include <IOKit/assert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOService.h>

#include "IOKitKernelInternal.h"

TUNABLE_WRITEABLE(SInt64, gIOKitDebug, "io", DEBUG_INIT_VALUE);
TUNABLE_DEV_WRITEABLE(SInt64, gIOKitTrace, "iotrace", 0);

#if DEVELOPMENT || DEBUG
#define IODEBUG_CTLFLAGS        CTLFLAG_RW
#else
#define IODEBUG_CTLFLAGS        CTLFLAG_RD
#endif

SYSCTL_QUAD(_debug, OID_AUTO, iotrace, IODEBUG_CTLFLAGS | CTLFLAG_LOCKED, &gIOKitTrace, "trace io");

static int
sysctl_debug_iokit
(__unused struct sysctl_oid *oidp, __unused void *arg1, __unused int arg2, struct sysctl_req *req)
{
	SInt64 newValue;
	int changed, error = sysctl_io_number(req, gIOKitDebug, sizeof(gIOKitDebug), &newValue, &changed);
	if (changed) {
		gIOKitDebug = ((gIOKitDebug & ~kIOKitDebugUserOptions) | (newValue & kIOKitDebugUserOptions));
	}
	return error;
}

SYSCTL_PROC(_debug, OID_AUTO, iokit,
    CTLTYPE_QUAD | IODEBUG_CTLFLAGS | CTLFLAG_KERN | CTLFLAG_LOCKED,
    &gIOKitDebug, 0, sysctl_debug_iokit, "Q", "boot_arg io");

void           (*gIOTrackingLeakScanCallback)(uint32_t notification) = NULL;

size_t          debug_malloc_size;
size_t          debug_iomalloc_size;

vm_size_t       debug_iomallocpageable_size;
size_t          debug_container_malloc_size;
// int          debug_ivars_size; // in OSObject.cpp

extern "C" {
#if 0
#define DEBG(fmt, args...)   { kprintf(fmt, ## args); }
#else
#define DEBG(fmt, args...)   { IOLog(fmt, ## args); }
#endif

void
IOPrintPlane( const IORegistryPlane * plane )
{
	IORegistryEntry *           next;
	IORegistryIterator *        iter;
	OSOrderedSet *              all;
	IOService *                 service;

	iter = IORegistryIterator::iterateOver( plane );
	assert( iter );
	all = iter->iterateAll();
	if (all) {
		DEBG("Count %d\n", all->getCount());
		all->release();
	} else {
		DEBG("Empty\n");
	}

	iter->reset();
	while ((next = iter->getNextObjectRecursive())) {
		DEBG( "%*s\033[33m%s", 2 * next->getDepth( plane ), "", next->getName( plane ));
		if ((next->getLocation( plane ))) {
			DEBG("@%s", next->getLocation( plane ));
		}
		DEBG("\033[0m <class %s", next->getMetaClass()->getClassName());
		if ((service = OSDynamicCast(IOService, next))) {
			DEBG(", busy %ld", (long) service->getBusyState());
		}
		DEBG( ">\n");
//      IOSleep(250);
	}
	iter->release();

#undef IOPrintPlaneFormat
}

void
db_piokjunk(void)
{
}

void
db_dumpiojunk( const IORegistryPlane * plane __unused )
{
}

void
IOPrintMemory( void )
{
//    OSMetaClass::printInstanceCounts();

	IOLog("\n"
	    "ivar kalloc()       0x%08lx\n"
	    "malloc()            0x%08lx\n"
	    "containers kalloc() 0x%08lx\n"
	    "IOMalloc()          0x%08lx\n"
	    "----------------------------------------\n",
	    debug_ivars_size,
	    debug_malloc_size,
	    debug_container_malloc_size,
	    debug_iomalloc_size
	    );
}
} /* extern "C" */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super OSObject
OSDefineMetaClassAndStructors(IOKitDiagnostics, OSObject)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSObject * IOKitDiagnostics::diagnostics( void )
{
	IOKitDiagnostics * diags;

	diags = new IOKitDiagnostics;
	if (diags && !diags->init()) {
		diags->release();
		diags = NULL;
	}

	return diags;
}

void
IOKitDiagnostics::updateOffset( OSDictionary * dict,
    UInt64 value, const char * name )
{
	OSNumber * off;

	off = OSNumber::withNumber( value, 64 );
	if (!off) {
		return;
	}

	dict->setObject( name, off );
	off->release();
}

bool
IOKitDiagnostics::serialize(OSSerialize *s) const
{
	OSDictionary *      dict;
	bool                ok;

	dict = OSDictionary::withCapacity( 5 );
	if (!dict) {
		return false;
	}

	updateOffset( dict, debug_ivars_size, "Instance allocation" );
	updateOffset( dict, debug_container_malloc_size, "Container allocation" );
	updateOffset( dict, debug_iomalloc_size, "IOMalloc allocation" );
	updateOffset( dict, debug_iomallocpageable_size, "Pageable allocation" );

	OSMetaClass::serializeClassDictionary(dict);

	ok = dict->serialize( s );

	dict->release();

	return ok;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if IOTRACKING

#include <libkern/c++/OSCPPDebug.h>
#include <libkern/c++/OSKext.h>
#include <kern/zalloc.h>

__private_extern__ "C" void qsort(
	void * array,
	size_t nmembers,
	size_t member_size,
	int (*)(const void *, const void *));

extern "C" ppnum_t pmap_find_phys(pmap_t pmap, addr64_t va);
extern "C" ppnum_t pmap_valid_page(ppnum_t pn);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct IOTRecursiveLock {
	lck_mtx_t * mutex;
	thread_t    thread;
	UInt32      count;
};

struct IOTrackingQueue {
	queue_chain_t     link;
	IOTRecursiveLock  lock;
	const char *      name;
	uintptr_t         btEntry;
	size_t            allocSize;
	size_t            minCaptureSize;
	uint32_t          siteCount;
	uint32_t          type;
	uint32_t          numSiteQs;
	uint8_t           captureOn;
	queue_head_t      sites[];
};


struct IOTrackingCallSiteUser {
	pid_t         pid;
	uint8_t       user32;
	uint8_t       userCount;
	uintptr_t     bt[kIOTrackingCallSiteBTs];
};

struct IOTrackingCallSite {
	queue_chain_t          link;
	queue_head_t           instances;
	IOTrackingQueue *      queue;
	IOTracking **          addresses;
	size_t        size[2];
	uint32_t               crc;
	uint32_t      count;

	vm_tag_t      tag;
	uint8_t       user32;
	uint8_t       userCount;
	pid_t         btPID;

	uintptr_t     bt[kIOTrackingCallSiteBTs];
	IOTrackingCallSiteUser     user[0];
};

struct IOTrackingCallSiteWithUser {
	struct IOTrackingCallSite     site;
	struct IOTrackingCallSiteUser user;
};

static void IOTrackingFreeCallSite(uint32_t type, IOTrackingCallSite ** site);

struct IOTrackingLeaksRef {
	uintptr_t * instances;
	uint32_t    zoneSize;
	uint32_t    count;
	uint32_t    found;
	uint32_t    foundzlen;
	size_t      bytes;
};

lck_mtx_t *  gIOTrackingLock;
queue_head_t gIOTrackingQ;

enum{
	kTrackingAddressFlagAllocated    = 0x00000001
};

#if defined(__LP64__)
#define IOTrackingAddressFlags(ptr)     (ptr->flags)
#else
#define IOTrackingAddressFlags(ptr)     (ptr->tracking.flags)
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void
IOTRecursiveLockLock(IOTRecursiveLock * lock)
{
	if (lock->thread == current_thread()) {
		lock->count++;
	} else {
		lck_mtx_lock(lock->mutex);
		assert(lock->thread == NULL);
		assert(lock->count == 0);
		lock->thread = current_thread();
		lock->count = 1;
	}
}

static void
IOTRecursiveLockUnlock(IOTRecursiveLock * lock)
{
	assert(lock->thread == current_thread());
	if (0 == (--lock->count)) {
		lock->thread = NULL;
		lck_mtx_unlock(lock->mutex);
	}
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
IOTrackingInit(void)
{
	queue_init(&gIOTrackingQ);
	gIOTrackingLock = lck_mtx_alloc_init(IOLockGroup, LCK_ATTR_NULL);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOTrackingQueue *
IOTrackingQueueAlloc(const char * name, uintptr_t btEntry,
    size_t allocSize, size_t minCaptureSize,
    uint32_t type, uint32_t numSiteQs)
{
	IOTrackingQueue * queue;
	uint32_t          idx;

	if (!numSiteQs) {
		numSiteQs = 1;
	}
	queue = kalloc_type(IOTrackingQueue, queue_head_t, numSiteQs, Z_WAITOK_ZERO);
	queue->name           = name;
	queue->btEntry        = btEntry;
	queue->allocSize      = allocSize;
	queue->minCaptureSize = minCaptureSize;
	queue->lock.mutex     = lck_mtx_alloc_init(IOLockGroup, LCK_ATTR_NULL);
	queue->numSiteQs      = numSiteQs;
	queue->type           = type;
	enum { kFlags = (kIOTracking | kIOTrackingBoot) };
	queue->captureOn = (kFlags == (kFlags & gIOKitDebug))
	    || (kIOTrackingQueueTypeDefaultOn & type);

	for (idx = 0; idx < numSiteQs; idx++) {
		queue_init(&queue->sites[idx]);
	}

	lck_mtx_lock(gIOTrackingLock);
	queue_enter(&gIOTrackingQ, queue, IOTrackingQueue *, link);
	lck_mtx_unlock(gIOTrackingLock);

	return queue;
};

void
IOTrackingQueueCollectUser(IOTrackingQueue * queue)
{
	assert(0 == queue->siteCount);
	queue->type |= kIOTrackingQueueTypeUser;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
IOTrackingQueueFree(IOTrackingQueue * queue)
{
	lck_mtx_lock(gIOTrackingLock);
	IOTrackingReset(queue);
	remque(&queue->link);
	lck_mtx_unlock(gIOTrackingLock);

	lck_mtx_free(queue->lock.mutex, IOLockGroup);

	kfree_type(IOTrackingQueue, queue_head_t, queue->numSiteQs, queue);
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* fasthash
 *  The MIT License
 *
 *  Copyright (C) 2012 Zilong Tan (eric.zltan@gmail.com)
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use, copy,
 *  modify, merge, publish, distribute, sublicense, and/or sell copies
 *  of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 *  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 *  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 *  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */


// Compression function for Merkle-Damgard construction.
// This function is generated using the framework provided.
#define mix(h) ({                               \
	          (h) ^= (h) >> 23;             \
	          (h) *= 0x2127599bf4325c37ULL; \
	          (h) ^= (h) >> 47; })

static uint64_t
fasthash64(const void *buf, size_t len, uint64_t seed)
{
	const uint64_t    m = 0x880355f21e6d1965ULL;
	const uint64_t *pos = (const uint64_t *)buf;
	const uint64_t *end = pos + (len / 8);
	const unsigned char *pos2;
	uint64_t h = seed ^ (len * m);
	uint64_t v;

	while (pos != end) {
		v  = *pos++;
		h ^= mix(v);
		h *= m;
	}

	pos2 = (const unsigned char*)pos;
	v = 0;

	switch (len & 7) {
	case 7: v ^= (uint64_t)pos2[6] << 48;
		[[clang::fallthrough]];
	case 6: v ^= (uint64_t)pos2[5] << 40;
		[[clang::fallthrough]];
	case 5: v ^= (uint64_t)pos2[4] << 32;
		[[clang::fallthrough]];
	case 4: v ^= (uint64_t)pos2[3] << 24;
		[[clang::fallthrough]];
	case 3: v ^= (uint64_t)pos2[2] << 16;
		[[clang::fallthrough]];
	case 2: v ^= (uint64_t)pos2[1] << 8;
		[[clang::fallthrough]];
	case 1: v ^= (uint64_t)pos2[0];
		h ^= mix(v);
		h *= m;
	}

	return mix(h);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static uint32_t
fasthash32(const void *buf, size_t len, uint32_t seed)
{
	// the following trick converts the 64-bit hashcode to Fermat
	// residue, which shall retain information from both the higher
	// and lower parts of hashcode.
	uint64_t h = fasthash64(buf, len, seed);
	return (uint32_t) (h - (h >> 32));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
IOTrackingAddUser(IOTrackingQueue * queue, IOTrackingUser * mem, vm_size_t size)
{
	uint32_t num;
	int pid;

	if (!queue->captureOn) {
		return;
	}
	if (size < queue->minCaptureSize) {
		return;
	}

	assert(!mem->link.next);

	num = backtrace(&mem->bt[0], kIOTrackingCallSiteBTs, NULL, NULL);
	num = 0;
	if ((kernel_task != current_task()) && (pid = proc_selfpid())) {
		struct backtrace_user_info btinfo = BTUINFO_INIT;
		mem->btPID = pid;
		num = backtrace_user(&mem->btUser[0], kIOTrackingCallSiteBTs - 1,
		    NULL, &btinfo);
		mem->user32 = !(btinfo.btui_info & BTI_64_BIT);
	}
	assert(num <= kIOTrackingCallSiteBTs);
	static_assert(kIOTrackingCallSiteBTs <= UINT8_MAX);
	mem->userCount = ((uint8_t) num);

	IOTRecursiveLockLock(&queue->lock);
	queue_enter/*last*/ (&queue->sites[0], mem, IOTrackingUser *, link);
	queue->siteCount++;
	IOTRecursiveLockUnlock(&queue->lock);
}

void
IOTrackingRemoveUser(IOTrackingQueue * queue, IOTrackingUser * mem)
{
	if (!mem->link.next) {
		return;
	}

	IOTRecursiveLockLock(&queue->lock);
	if (mem->link.next) {
		remque(&mem->link);
		assert(queue->siteCount);
		queue->siteCount--;
	}
	IOTRecursiveLockUnlock(&queue->lock);
}

uint64_t gIOTrackingAddTime;

void
IOTrackingAdd(IOTrackingQueue * queue, IOTracking * mem, size_t size, bool address, vm_tag_t tag)
{
	IOTrackingCallSite * site;
	uint32_t             crc, num;
	uintptr_t            bt[kIOTrackingCallSiteBTs + 1];
	uintptr_t            btUser[kIOTrackingCallSiteBTs];
	queue_head_t       * que;
	bool                 user;
	int                  pid;
	int                  userCount;

	if (mem->site) {
		return;
	}
	if (!queue->captureOn) {
		return;
	}
	if (size < queue->minCaptureSize) {
		return;
	}

	user = (0 != (kIOTrackingQueueTypeUser & queue->type));

	assert(!mem->link.next);

	num  = backtrace(&bt[0], kIOTrackingCallSiteBTs + 1, NULL, NULL);
	if (!num) {
		return;
	}
	num--;
	crc = fasthash32(&bt[1], num * sizeof(bt[0]), 0x04C11DB7);

	userCount = 0;
	pid = 0;
	backtrace_info_t btinfo = BTI_NONE;
	if (user) {
		if ((kernel_task != current_task()) && (pid = proc_selfpid())) {
			struct backtrace_user_info btuinfo = BTUINFO_INIT;
			userCount = backtrace_user(&btUser[0], kIOTrackingCallSiteBTs,
			    NULL, &btuinfo);
			assert(userCount <= kIOTrackingCallSiteBTs);
			btinfo = btuinfo.btui_info;
			crc = fasthash32(&btUser[0], userCount * sizeof(bt[0]), crc);
		}
	}

	IOTRecursiveLockLock(&queue->lock);
	que = &queue->sites[crc % queue->numSiteQs];
	queue_iterate(que, site, IOTrackingCallSite *, link)
	{
		if (tag != site->tag) {
			continue;
		}
		if (user && (pid != site->user[0].pid)) {
			continue;
		}
		if (crc == site->crc) {
			break;
		}
	}

	if (queue_end(que, (queue_entry_t) site)) {
		if (user) {
			site = &kalloc_type(IOTrackingCallSiteWithUser,
			    Z_WAITOK_ZERO_NOFAIL)->site;
		} else {
			site = kalloc_type(IOTrackingCallSite,
			    Z_WAITOK_ZERO_NOFAIL);
		}

		queue_init(&site->instances);
		site->addresses  = NULL;
		site->queue      = queue;
		site->crc        = crc;
		site->count      = 0;
		site->tag        = tag;
		memset(&site->size[0], 0, sizeof(site->size));
		bcopy(&bt[1], &site->bt[0], num * sizeof(site->bt[0]));
		assert(num <= kIOTrackingCallSiteBTs);
		bzero(&site->bt[num], (kIOTrackingCallSiteBTs - num) * sizeof(site->bt[0]));
		if (user) {
			bcopy(&btUser[0], &site->user[0].bt[0], userCount * sizeof(site->user[0].bt[0]));
			assert(userCount <= kIOTrackingCallSiteBTs);
			bzero(&site->user[0].bt[userCount], (kIOTrackingCallSiteBTs - userCount) * sizeof(site->user[0].bt[0]));
			site->user[0].pid  = pid;
			site->user[0].user32 = !(btinfo & BTI_64_BIT);
			static_assert(kIOTrackingCallSiteBTs <= UINT8_MAX);
			site->user[0].userCount = ((uint8_t) userCount);
		}
		queue_enter_first(que, site, IOTrackingCallSite *, link);
		queue->siteCount++;
	}

	if (address) {
		IOTrackingAddress * memAddr = (typeof(memAddr))mem;
		uint32_t hashIdx;

		if (NULL == site->addresses) {
			site->addresses = kalloc_type(IOTracking *, queue->numSiteQs, Z_WAITOK_ZERO_NOFAIL);
			for (hashIdx = 0; hashIdx < queue->numSiteQs; hashIdx++) {
				site->addresses[hashIdx] = (IOTracking *) &site->instances;
			}
		}
		hashIdx = atop(memAddr->address) % queue->numSiteQs;
		if (queue_end(&site->instances, (queue_entry_t)site->addresses[hashIdx])) {
			queue_enter/*last*/ (&site->instances, mem, IOTracking *, link);
		} else {
			queue_insert_before(&site->instances, mem, site->addresses[hashIdx], IOTracking *, link);
		}
		site->addresses[hashIdx] = mem;
	} else {
		queue_enter_first(&site->instances, mem, IOTracking *, link);
	}

	mem->site      = site;
	site->size[0] += size;
	site->count++;

	IOTRecursiveLockUnlock(&queue->lock);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void
IOTrackingRemoveInternal(IOTrackingQueue * queue, IOTracking * mem, size_t size, uint32_t addressIdx)
{
	IOTrackingCallSite * site;
	IOTrackingAddress  * nextAddress;

	if (!mem->link.next) {
		return;
	}

	IOTRecursiveLockLock(&queue->lock);
	if (mem->link.next) {
		assert(mem->site);
		site = mem->site;

		if ((-1U != addressIdx) && (mem == site->addresses[addressIdx])) {
			nextAddress = (IOTrackingAddress *) queue_next(&mem->link);
			if (!queue_end(&site->instances, &nextAddress->tracking.link)
			    && (addressIdx != (atop(nextAddress->address) % queue->numSiteQs))) {
				nextAddress = (IOTrackingAddress *) &site->instances;
			}
			site->addresses[addressIdx] = &nextAddress->tracking;
		}

		remque(&mem->link);
		assert(site->count);
		site->count--;
		assert(site->size[0] >= size);
		site->size[0] -= size;
		if (!site->count) {
			assert(queue_empty(&site->instances));
			assert(!site->size[0]);
			assert(!site->size[1]);

			remque(&site->link);
			assert(queue->siteCount);
			queue->siteCount--;
			IOTrackingFreeCallSite(queue->type, &site);
		}
		mem->site = NULL;
	}
	IOTRecursiveLockUnlock(&queue->lock);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
IOTrackingRemove(IOTrackingQueue * queue, IOTracking * mem, size_t size)
{
	return IOTrackingRemoveInternal(queue, mem, size, -1U);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
IOTrackingRemoveAddress(IOTrackingQueue * queue, IOTrackingAddress * mem, size_t size)
{
	uint32_t addressIdx;
	uint64_t address;

	address = mem->address;
	addressIdx = atop(address) % queue->numSiteQs;

	return IOTrackingRemoveInternal(queue, &mem->tracking, size, addressIdx);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
IOTrackingAlloc(IOTrackingQueue * queue, uintptr_t address, size_t size)
{
	IOTrackingAddress * tracking;

	if (!queue->captureOn) {
		return;
	}
	if (size < queue->minCaptureSize) {
		return;
	}

	address = ~address;
	tracking = kalloc_type(IOTrackingAddress, (zalloc_flags_t)(Z_WAITOK | Z_ZERO));
	IOTrackingAddressFlags(tracking) |= kTrackingAddressFlagAllocated;
	tracking->address = address;
	tracking->size    = size;

	IOTrackingAdd(queue, &tracking->tracking, size, true, VM_KERN_MEMORY_NONE);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
IOTrackingFree(IOTrackingQueue * queue, uintptr_t address, size_t size)
{
	IOTrackingCallSite * site;
	IOTrackingAddress  * tracking;
	IOTrackingAddress  * nextAddress;
	uint32_t             idx, hashIdx;
	bool                 done;

	address = ~address;
	IOTRecursiveLockLock(&queue->lock);

	hashIdx = atop(address) % queue->numSiteQs;

	done = false;
	for (idx = 0; idx < queue->numSiteQs; idx++) {
		queue_iterate(&queue->sites[idx], site, IOTrackingCallSite *, link)
		{
			if (!site->addresses) {
				continue;
			}
			tracking = (IOTrackingAddress *) site->addresses[hashIdx];
			while (!queue_end(&site->instances, &tracking->tracking.link)) {
				nextAddress = (IOTrackingAddress *) queue_next(&tracking->tracking.link);
				if (!queue_end(&site->instances, &nextAddress->tracking.link)
				    && (hashIdx != (atop(nextAddress->address) % queue->numSiteQs))) {
					nextAddress = (IOTrackingAddress *) &site->instances;
				}
				if ((done = (address == tracking->address))) {
					if (tracking == (IOTrackingAddress *) site->addresses[hashIdx]) {
						site->addresses[hashIdx] = &nextAddress->tracking;
					}
					IOTrackingRemoveInternal(queue, &tracking->tracking, size, -1U);
					kfree_type(IOTrackingAddress, tracking);
					break;
				}
				tracking = nextAddress;
			}
			if (done) {
				break;
			}
		}
		if (done) {
			break;
		}
	}
	IOTRecursiveLockUnlock(&queue->lock);
}

static void
IOTrackingFreeCallSite(uint32_t type, IOTrackingCallSite ** pSite)
{
	IOTrackingCallSite * site;
	void ** ptr;

	site = *pSite;
	kfree_type(IOTracking *, site->queue->numSiteQs, site->addresses);

	ptr = reinterpret_cast<void **>(pSite);
	if (kIOTrackingQueueTypeUser & type) {
		kfree_type(IOTrackingCallSiteWithUser, *ptr);
	} else {
		kfree_type(IOTrackingCallSite, *ptr);
	}
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
IOTrackingAccumSize(IOTrackingQueue * queue, IOTracking * mem, size_t size)
{
	IOTRecursiveLockLock(&queue->lock);
	if (mem->link.next) {
		assert(mem->site);
		assert((size > 0) || (mem->site->size[1] >= -size));
		mem->site->size[1] += size;
	}
	;
	IOTRecursiveLockUnlock(&queue->lock);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
IOTrackingReset(IOTrackingQueue * queue)
{
	IOTrackingCallSite * site;
	IOTrackingUser     * user;
	IOTracking         * tracking;
	IOTrackingAddress  * trackingAddress;
	uint32_t             idx, hashIdx;
	bool                 addresses;

	IOTRecursiveLockLock(&queue->lock);
	for (idx = 0; idx < queue->numSiteQs; idx++) {
		while (!queue_empty(&queue->sites[idx])) {
			if (kIOTrackingQueueTypeMap & queue->type) {
				queue_remove_first(&queue->sites[idx], user, IOTrackingUser *, link);
				user->link.next = user->link.prev = NULL;
			} else {
				queue_remove_first(&queue->sites[idx], site, IOTrackingCallSite *, link);
				addresses = false;
				while (!queue_empty(&site->instances)) {
					queue_remove_first(&site->instances, tracking, IOTracking *, link);
					if (site->addresses) {
						for (hashIdx = 0; !addresses && (hashIdx < queue->numSiteQs); hashIdx++) {
							if (tracking == site->addresses[hashIdx]) {
								addresses = true;
							}
						}
					}
					if (addresses) {
						trackingAddress = (typeof(trackingAddress))tracking;
						if (kTrackingAddressFlagAllocated & IOTrackingAddressFlags(trackingAddress)) {
							kfree_type(IOTrackingAddress, trackingAddress);
						}
					}
				}
				IOTrackingFreeCallSite(queue->type, &site);
			}
		}
	}
	queue->siteCount = 0;
	IOTRecursiveLockUnlock(&queue->lock);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static int
IOTrackingCallSiteInfoCompare(const void * left, const void * right)
{
	IOTrackingCallSiteInfo * l = (typeof(l))left;
	IOTrackingCallSiteInfo * r = (typeof(r))right;
	size_t                   lsize, rsize;

	rsize = r->size[0] + r->size[1];
	lsize = l->size[0] + l->size[1];

	return (rsize > lsize) ? 1 : ((rsize == lsize) ? 0 : -1);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static int
IOTrackingAddressCompare(const void * left, const void * right)
{
	IOTracking * instance;
	uintptr_t    inst, laddr, raddr;

	inst = ((typeof(inst) *)left)[0];
	instance = (typeof(instance))INSTANCE_GET(inst);
	if (kInstanceFlagAddress & inst) {
		laddr = ~((IOTrackingAddress *)instance)->address;
	} else {
		laddr = (uintptr_t) (instance + 1);
	}

	inst = ((typeof(inst) *)right)[0];
	instance = (typeof(instance))(inst & ~kInstanceFlags);
	if (kInstanceFlagAddress & inst) {
		raddr = ~((IOTrackingAddress *)instance)->address;
	} else {
		raddr = (uintptr_t) (instance + 1);
	}

	return (laddr > raddr) ? 1 : ((laddr == raddr) ? 0 : -1);
}


static int
IOTrackingZoneElementCompare(const void * left, const void * right)
{
	uintptr_t    inst, laddr, raddr;

	inst = ((typeof(inst) *)left)[0];
	laddr = INSTANCE_PUT(inst);
	inst = ((typeof(inst) *)right)[0];
	raddr = INSTANCE_PUT(inst);

	return (laddr > raddr) ? 1 : ((laddr == raddr) ? 0 : -1);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void
CopyOutBacktraces(IOTrackingCallSite * site, IOTrackingCallSiteInfo * siteInfo)
{
	uint32_t j;
	mach_vm_address_t bt, btEntry;

	btEntry = site->queue->btEntry;
	for (j = 0; j < kIOTrackingCallSiteBTs; j++) {
		bt = site->bt[j];
		if (btEntry
		    && (!bt || (j == (kIOTrackingCallSiteBTs - 1)))) {
			bt = btEntry;
			btEntry = 0;
		}
		siteInfo->bt[0][j] = VM_KERNEL_UNSLIDE(bt);
	}

	siteInfo->btPID = 0;
	if (kIOTrackingQueueTypeUser & site->queue->type) {
		siteInfo->btPID = site->user[0].pid;
		uint32_t * bt32 = (typeof(bt32))((void *) &site->user[0].bt[0]);
		uint64_t * bt64 = (typeof(bt64))((void *) &site->user[0].bt[0]);
		for (uint32_t j = 0; j < kIOTrackingCallSiteBTs; j++) {
			if (j >= site->user[0].userCount) {
				siteInfo->bt[1][j] = 0;
			} else if (site->user[0].user32) {
				siteInfo->bt[1][j] = bt32[j];
			} else {
				siteInfo->bt[1][j] = bt64[j];
			}
		}
	}
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void
IOTrackingLeakScan(void * refcon)
{
	IOTrackingLeaksRef * ref = (typeof(ref))refcon;
	uintptr_t          * instances;
	IOTracking         * instance;
	uint64_t             vaddr, vincr;
	ppnum_t              ppn;
	uintptr_t            ptr, addr, vphysaddr, inst;
	size_t               size, origsize;
	uint32_t             baseIdx, lim, ptrIdx, count;
	boolean_t            is;
	AbsoluteTime         deadline;

	instances       = ref->instances;
	count           = ref->count;
	size = origsize = ref->zoneSize;

	if (gIOTrackingLeakScanCallback) {
		gIOTrackingLeakScanCallback(kIOTrackingLeakScanStart);
	}

	for (deadline = 0, vaddr = VM_MIN_KERNEL_AND_KEXT_ADDRESS;
	    ;
	    vaddr += vincr) {
		if ((mach_absolute_time() > deadline) || (vaddr >= VM_MAX_KERNEL_ADDRESS)) {
			if (deadline) {
#if SCHED_HYGIENE_DEBUG
				if (is) {
					// Reset the interrupt timeout to avoid panics
					ml_spin_debug_clear_self();
				}
#endif /* SCHED_HYGIENE_DEBUG */
				ml_set_interrupts_enabled(is);
				IODelay(10);
			}
			if (vaddr >= VM_MAX_KERNEL_ADDRESS) {
				break;
			}
			is = ml_set_interrupts_enabled(false);
			clock_interval_to_deadline(10, kMillisecondScale, &deadline);
		}

		ppn = kernel_pmap_present_mapping(vaddr, &vincr, &vphysaddr);
		// check noencrypt to avoid VM structs (map entries) with pointers
		if (ppn && (!pmap_valid_page(ppn) || (!ref->zoneSize && pmap_is_noencrypt(ppn)))) {
			ppn = 0;
		}
		if (!ppn) {
			continue;
		}

		for (ptrIdx = 0; ptrIdx < (page_size / sizeof(uintptr_t)); ptrIdx++) {
			ptr = ((uintptr_t *)vphysaddr)[ptrIdx];
#if defined(HAS_APPLE_PAC)
			// strip possible ptrauth signature from candidate data pointer
			ptr = (uintptr_t)ptrauth_strip((void*)ptr, ptrauth_key_process_independent_data);
#endif /* defined(HAS_APPLE_PAC) */

			for (lim = count, baseIdx = 0; lim; lim >>= 1) {
				inst = instances[baseIdx + (lim >> 1)];
				instance = (typeof(instance))INSTANCE_GET(inst);

				if (ref->zoneSize) {
					addr = INSTANCE_PUT(inst) & ~kInstanceFlags;
				} else if (kInstanceFlagAddress & inst) {
					addr            = ~((IOTrackingAddress *)instance)->address;
					origsize = size = ((IOTrackingAddress *)instance)->size;
					if (!size) {
						size = 1;
					}
				} else {
					addr            = (uintptr_t) (instance + 1);
					origsize = size = instance->site->queue->allocSize;
				}
				if ((ptr >= addr) && (ptr < (addr + size))

				    && (((vaddr + ptrIdx * sizeof(uintptr_t)) < addr)
				    || ((vaddr + ptrIdx * sizeof(uintptr_t)) >= (addr + size)))) {
					if (!(kInstanceFlagReferenced & inst)) {
						inst |= kInstanceFlagReferenced;
						instances[baseIdx + (lim >> 1)] = inst;
						ref->found++;
						if (!origsize) {
							ref->foundzlen++;
						}
					}
					break;
				}
				if (ptr > addr) {
					// move right
					baseIdx += (lim >> 1) + 1;
					lim--;
				}
				// else move left
			}
		}
		ref->bytes += page_size;
	}

	if (gIOTrackingLeakScanCallback) {
		gIOTrackingLeakScanCallback(kIOTrackingLeakScanEnd);
	}
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

extern "C" void
zone_leaks_scan(uintptr_t * instances, uint32_t count, uint32_t zoneSize, uint32_t * found)
{
	IOTrackingLeaksRef       ref;
	IOTrackingCallSiteInfo   siteInfo;
	uint32_t                 idx;

	qsort(instances, count, sizeof(*instances), &IOTrackingZoneElementCompare);

	bzero(&siteInfo, sizeof(siteInfo));
	bzero(&ref, sizeof(ref));
	ref.instances = instances;
	ref.count = count;
	ref.zoneSize = zoneSize;

	for (idx = 0; idx < 2; idx++) {
		ref.bytes = 0;
		IOTrackingLeakScan(&ref);
		IOLog("leaks(%d) scanned %ld MB, instance count %d, found %d\n", idx, ref.bytes / 1024 / 1024, count, ref.found);
		if (count <= ref.found) {
			break;
		}
	}

	*found = ref.found;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static OSData *
IOTrackingLeaks(LIBKERN_CONSUMED OSData * data)
{
	IOTrackingLeaksRef       ref;
	IOTrackingCallSiteInfo   siteInfo;
	IOTrackingCallSite     * site;
	OSData                 * leakData;
	uintptr_t              * instances;
	IOTracking             * instance;
	uintptr_t                inst;
	uint32_t                 count, idx, numSites, dups, siteCount;

	/* BEGIN IGNORE CODESTYLE */
	__typed_allocators_ignore_push
	instances = (typeof(instances))data->getBytesNoCopy();
	__typed_allocators_ignore_pop
	/* END IGNORE CODESTYLE */
	count = (data->getLength() / sizeof(*instances));
	qsort(instances, count, sizeof(*instances), &IOTrackingAddressCompare);

	bzero(&siteInfo, sizeof(siteInfo));
	bzero(&ref, sizeof(ref));
	ref.instances = instances;
	ref.count = count;
	for (idx = 0; idx < 2; idx++) {
		ref.bytes = 0;
		IOTrackingLeakScan(&ref);
		IOLog("leaks(%d) scanned %ld MB, instance count %d, found %d (zlen %d)\n", idx, ref.bytes / 1024 / 1024, count, ref.found, ref.foundzlen);
		if (count <= ref.found) {
			break;
		}
	}

	/* BEGIN IGNORE CODESTYLE */
	__typed_allocators_ignore_push
	leakData = OSData::withCapacity(128 * sizeof(IOTrackingCallSiteInfo));
	__typed_allocators_ignore_pop
	/* END IGNORE CODESTYLE */

	for (numSites = 0, idx = 0; idx < count; idx++) {
		inst = instances[idx];
		if (kInstanceFlagReferenced & inst) {
			continue;
		}
		instance = (typeof(instance))INSTANCE_GET(inst);
		site = instance->site;
		instances[numSites] = (uintptr_t) site;
		numSites++;
	}

	for (idx = 0; idx < numSites; idx++) {
		inst = instances[idx];
		if (!inst) {
			continue;
		}
		site = (typeof(site))inst;
		for (siteCount = 1, dups = (idx + 1); dups < numSites; dups++) {
			if (instances[dups] == (uintptr_t) site) {
				siteCount++;
				instances[dups] = 0;
			}
		}
		// leak byte size is reported as:
		// (total bytes allocated by the callsite * number of leaked instances)
		// divided by (number of allocations by callsite)
		siteInfo.count   = siteCount;
		siteInfo.size[0] = (site->size[0] * siteCount) / site->count;
		siteInfo.size[1] = (site->size[1] * siteCount) / site->count;
		CopyOutBacktraces(site, &siteInfo);
		__typed_allocators_ignore_push
		leakData->appendBytes(&siteInfo, sizeof(siteInfo));
		__typed_allocators_ignore_pop
	}
	data->release();

	return leakData;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static bool
SkipName(uint32_t options, const char * name, size_t namesLen, const char * names)
{
	const char * scan;
	const char * next;
	bool         exclude, found;
	size_t       qLen, sLen;

	if (!namesLen || !names) {
		return false;
	}
	// <len><name>...<len><name><0>
	exclude = (0 != (kIOTrackingExcludeNames & options));
	qLen    = strlen(name);
	scan    = names;
	found   = false;
	do{
		sLen = scan[0];
		scan++;
		next = scan + sLen;
		if (next >= (names + namesLen)) {
			break;
		}
		found = ((sLen == qLen) && !strncmp(scan, name, sLen));
		scan = next;
	}while (!found && (scan < (names + namesLen)));

	return !(exclude ^ found);
}

#endif /* IOTRACKING */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static kern_return_t
IOTrackingDebug(uint32_t selector, uint32_t options, uint64_t value,
    uint32_t intag, uint32_t inzsize,
    const char * names, size_t namesLen,
    size_t size, OSObject ** result)
{
	kern_return_t            ret;
	OSData                 * data;

	if (result) {
		*result = NULL;
	}
	data = NULL;
	ret = kIOReturnNotReady;

#if IOTRACKING

	kern_return_t            kr;
	IOTrackingQueue        * queue;
	IOTracking             * instance;
	IOTrackingCallSite     * site;
	IOTrackingCallSiteInfo   siteInfo;
	IOTrackingUser         * user;
	task_t                   mapTask;
	mach_vm_address_t        mapAddress;
	mach_vm_size_t           mapSize;
	uint32_t                 num, idx, qIdx;
	uintptr_t                instFlags;
	proc_t                   proc;
	bool                     addresses;

	ret = kIOReturnNotFound;
	proc = NULL;
	if (kIOTrackingGetMappings == selector) {
		if (value != -1ULL) {
			proc = proc_find((pid_t) value);
			if (!proc) {
				return kIOReturnNotFound;
			}
		}
	}

	bzero(&siteInfo, sizeof(siteInfo));
	lck_mtx_lock(gIOTrackingLock);
	queue_iterate(&gIOTrackingQ, queue, IOTrackingQueue *, link)
	{
		if (SkipName(options, queue->name, namesLen, names)) {
			continue;
		}

		if (!(kIOTracking & gIOKitDebug) && (kIOTrackingQueueTypeAlloc & queue->type)) {
			continue;
		}

		switch (selector) {
		case kIOTrackingResetTracking:
		{
			IOTrackingReset(queue);
			ret = kIOReturnSuccess;
			break;
		}

		case kIOTrackingStartCapture:
		case kIOTrackingStopCapture:
		{
			queue->captureOn = (kIOTrackingStartCapture == selector);
			ret = kIOReturnSuccess;
			break;
		}

		case kIOTrackingSetMinCaptureSize:
		{
			queue->minCaptureSize = size;
			ret = kIOReturnSuccess;
			break;
		}

		case kIOTrackingLeaks:
		{
			if (!(kIOTrackingQueueTypeAlloc & queue->type)) {
				break;
			}

			if (!data) {
				/* BEGIN IGNORE CODESTYLE */
				__typed_allocators_ignore_push
				data = OSData::withCapacity(1024 * sizeof(uintptr_t));
				__typed_allocators_ignore_pop
				/* END IGNORE CODESTYLE */
			}

			IOTRecursiveLockLock(&queue->lock);
			for (idx = 0; idx < queue->numSiteQs; idx++) {
				queue_iterate(&queue->sites[idx], site, IOTrackingCallSite *, link)
				{
					addresses = false;
					queue_iterate(&site->instances, instance, IOTracking *, link)
					{
						if (site->addresses) {
							for (uint32_t hashIdx = 0; !addresses && (hashIdx < queue->numSiteQs); hashIdx++) {
								if (instance == site->addresses[hashIdx]) {
									addresses = true;
								}
							}
						}
						instFlags = (typeof(instFlags))instance;
						if (addresses) {
							instFlags |= kInstanceFlagAddress;
						}
						data->appendValue(instFlags);
					}
				}
			}
			// queue is locked
			ret = kIOReturnSuccess;
			break;
		}


		case kIOTrackingGetTracking:
		{
			if (kIOTrackingQueueTypeMap & queue->type) {
				break;
			}

			if (!data) {
				/* BEGIN IGNORE CODESTYLE */
				__typed_allocators_ignore_push
				data = OSData::withCapacity(128 * sizeof(IOTrackingCallSiteInfo));
				__typed_allocators_ignore_pop
				/* END IGNORE CODESTYLE */
			}

			IOTRecursiveLockLock(&queue->lock);
			num = queue->siteCount;
			idx = 0;
			for (qIdx = 0; qIdx < queue->numSiteQs; qIdx++) {
				queue_iterate(&queue->sites[qIdx], site, IOTrackingCallSite *, link)
				{
					assert(idx < num);
					idx++;

					size_t tsize[2];
					uint32_t count = site->count;
					tsize[0] = site->size[0];
					tsize[1] = site->size[1];

					if (intag || inzsize) {
						uintptr_t addr;
						vm_size_t size, zoneSize;
						vm_tag_t  tag;

						if (kIOTrackingQueueTypeAlloc & queue->type) {
							addresses = false;
							count = 0;
							tsize[0] = tsize[1] = 0;
							queue_iterate(&site->instances, instance, IOTracking *, link)
							{
								if (site->addresses) {
									for (uint32_t hashIdx = 0; !addresses && (hashIdx < queue->numSiteQs); hashIdx++) {
										if (instance == site->addresses[hashIdx]) {
											addresses = true;
										}
									}
								}

								if (addresses) {
									addr = ~((IOTrackingAddress *)instance)->address;
								} else {
									addr = (uintptr_t) (instance + 1);
								}

								kr = vm_kern_allocation_info(addr, &size, &tag, &zoneSize);
								if (KERN_SUCCESS != kr) {
									continue;
								}

								if ((VM_KERN_MEMORY_NONE != intag) && (intag != tag)) {
									continue;
								}
								if (inzsize && (inzsize != zoneSize)) {
									continue;
								}

								count++;
								tsize[0] += size;
							}
						} else {
							if (!intag || inzsize || (intag != site->tag)) {
								continue;
							}
						}
					}

					if (!count) {
						continue;
					}
					if (size && ((tsize[0] + tsize[1]) < size)) {
						continue;
					}
					siteInfo.count   = count;
					siteInfo.size[0] = tsize[0];
					siteInfo.size[1] = tsize[1];
					CopyOutBacktraces(site, &siteInfo);
					__typed_allocators_ignore_push
					data->appendBytes(&siteInfo, sizeof(siteInfo));
					__typed_allocators_ignore_pop
				}
			}
			assert(idx == num);
			IOTRecursiveLockUnlock(&queue->lock);
			ret = kIOReturnSuccess;
			break;
		}

		case kIOTrackingGetMappings:
		{
			if (!(kIOTrackingQueueTypeMap & queue->type)) {
				break;
			}
			if (!data) {
				data = OSData::withCapacity((unsigned int) page_size);
			}

			IOTRecursiveLockLock(&queue->lock);
			num = queue->siteCount;
			idx = 0;
			for (qIdx = 0; qIdx < queue->numSiteQs; qIdx++) {
				queue_iterate(&queue->sites[qIdx], user, IOTrackingUser *, link)
				{
					assert(idx < num);
					idx++;

					kr = IOMemoryMapTracking(user, &mapTask, &mapAddress, &mapSize);
					if (kIOReturnSuccess != kr) {
						continue;
					}
					if (proc && (mapTask != proc_task(proc))) {
						continue;
					}
					if (size && (mapSize < size)) {
						continue;
					}

					siteInfo.count      = 1;
					siteInfo.size[0]    = mapSize;
					siteInfo.address    = mapAddress;
					siteInfo.addressPID = task_pid(mapTask);
					siteInfo.btPID      = user->btPID;

					for (uint32_t j = 0; j < kIOTrackingCallSiteBTs; j++) {
						siteInfo.bt[0][j] = VM_KERNEL_UNSLIDE(user->bt[j]);
					}
					uint32_t * bt32 = (typeof(bt32)) & user->btUser[0];
					uint64_t * bt64 = (typeof(bt64))((void *) &user->btUser[0]);
					for (uint32_t j = 0; j < kIOTrackingCallSiteBTs; j++) {
						if (j >= user->userCount) {
							siteInfo.bt[1][j] = 0;
						} else if (user->user32) {
							siteInfo.bt[1][j] = bt32[j];
						} else {
							siteInfo.bt[1][j] = bt64[j];
						}
					}
					__typed_allocators_ignore_push
					data->appendBytes(&siteInfo, sizeof(siteInfo));
					__typed_allocators_ignore_pop
				}
			}
			assert(idx == num);
			IOTRecursiveLockUnlock(&queue->lock);
			ret = kIOReturnSuccess;
			break;
		}

		default:
			ret = kIOReturnUnsupported;
			break;
		}
	}

	if ((kIOTrackingLeaks == selector) && data) {
		data = IOTrackingLeaks(data);
		queue_iterate(&gIOTrackingQ, queue, IOTrackingQueue *, link)
		{
			if (SkipName(options, queue->name, namesLen, names)) {
				continue;
			}
			if (!(kIOTrackingQueueTypeAlloc & queue->type)) {
				continue;
			}
			IOTRecursiveLockUnlock(&queue->lock);
		}
	}

	lck_mtx_unlock(gIOTrackingLock);

	if ((kIOTrackingLeaks == selector) && namesLen && names) {
		const char * scan;
		const char * next;
		uint8_t      sLen;

		if (!data) {
			/* BEGIN IGNORE CODESTYLE */
			__typed_allocators_ignore_push
			data = OSData::withCapacity(4096 * sizeof(uintptr_t));
			__typed_allocators_ignore_pop
			/* END IGNORE CODESTYLE */
		}

		// <len><name>...<len><name><0>
		scan    = names;
		do{
			sLen = ((uint8_t) scan[0]);
			scan++;
			next = scan + sLen;
			if (next >= (names + namesLen)) {
				break;
			}
			kr = zone_leaks(scan, sLen, ^(uint32_t count, uint32_t eSize, btref_t ref) {
				IOTrackingCallSiteInfo siteInfo = {
				        .count   = count,
				        .size[0] = eSize * count,
				};

				btref_decode_unslide(ref, siteInfo.bt[0]);
				__typed_allocators_ignore_push
				data->appendBytes(&siteInfo, sizeof(siteInfo));
				__typed_allocators_ignore_pop
			});
			if (KERN_SUCCESS == kr) {
				ret = kIOReturnSuccess;
			} else if (KERN_INVALID_NAME != kr) {
				ret = kIOReturnVMError;
			}
			scan = next;
		}while (scan < (names + namesLen));
	}

	if (data) {
		switch (selector) {
		case kIOTrackingLeaks:
		case kIOTrackingGetTracking:
		case kIOTrackingGetMappings:
		{
			IOTrackingCallSiteInfo * siteInfos;
			/* BEGIN IGNORE CODESTYLE */
			__typed_allocators_ignore_push
			siteInfos = (typeof(siteInfos))data->getBytesNoCopy();
			__typed_allocators_ignore_pop
			/* END IGNORE CODESTYLE */
			num = (data->getLength() / sizeof(*siteInfos));
			qsort(siteInfos, num, sizeof(*siteInfos), &IOTrackingCallSiteInfoCompare);
			break;
		}
		default: assert(false); break;
		}
	}

	*result = data;
	if (proc) {
		proc_rele(proc);
	}

#endif /* IOTRACKING */

	return ret;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <IOKit/IOKitDiagnosticsUserClient.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOUserClient2022

OSDefineMetaClassAndStructors(IOKitDiagnosticsClient, IOUserClient2022)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOUserClient * IOKitDiagnosticsClient::withTask(task_t owningTask)
{
#if IOTRACKING
	IOKitDiagnosticsClient * inst;

	inst = new IOKitDiagnosticsClient;
	if (inst && !inst->init()) {
		inst->release();
		inst = NULL;
	}

	inst->setProperty(kIOUserClientDefaultLockingKey, kOSBooleanTrue);
	inst->setProperty(kIOUserClientDefaultLockingSetPropertiesKey, kOSBooleanTrue);
	inst->setProperty(kIOUserClientDefaultLockingSingleThreadExternalMethodKey, kOSBooleanTrue);

	inst->setProperty(kIOUserClientEntitlementsKey, kOSBooleanFalse);

	return inst;
#else
	return NULL;
#endif
}

IOReturn
IOKitDiagnosticsClient::clientClose(void)
{
	terminate();
	return kIOReturnSuccess;
}

IOReturn
IOKitDiagnosticsClient::setProperties(OSObject * properties)
{
	IOReturn kr = kIOReturnUnsupported;
	return kr;
}


IOReturn
IOTrackingMethodDispatched(OSObject * target, void * reference,
    IOExternalMethodArguments * args)
{
	IOReturn                           ret = kIOReturnBadArgument;
	const IOKitDiagnosticsParameters * params;
	const char * names;
	size_t       namesLen;
	OSObject   * result;

	if (args->structureInputSize < sizeof(IOKitDiagnosticsParameters)) {
		return kIOReturnBadArgument;
	}
	params = (typeof(params))args->structureInput;
	if (!params) {
		return kIOReturnBadArgument;
	}

	names = NULL;
	namesLen = args->structureInputSize - sizeof(IOKitDiagnosticsParameters);
	if (namesLen) {
		names = (typeof(names))(params + 1);
	}

	ret = IOTrackingDebug(args->selector, params->options, params->value, params->tag, params->zsize, names, namesLen, params->size, &result);
	if ((kIOReturnSuccess == ret) && args->structureVariableOutputData) {
		*args->structureVariableOutputData = result;
	} else if (result) {
		result->release();
	}
	return ret;
}

IOReturn
IOKitDiagnosticsClient::externalMethod(uint32_t selector, IOExternalMethodArgumentsOpaque * args)
{
	static const IOExternalMethodDispatch2022 dispatchArray[] = {
		[kIOTrackingGetTracking] = {
			.function                             = &IOTrackingMethodDispatched,
			.checkScalarInputCount    = 0,
			.checkStructureInputSize  = kIOUCVariableStructureSize,
			.checkScalarOutputCount   = 0,
			.checkStructureOutputSize = 0,
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
		[kIOTrackingGetMappings] = {
			.function                             = &IOTrackingMethodDispatched,
			.checkScalarInputCount    = 0,
			.checkStructureInputSize  = kIOUCVariableStructureSize,
			.checkScalarOutputCount   = 0,
			.checkStructureOutputSize = 0,
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
		[kIOTrackingResetTracking] = {
			.function                             = &IOTrackingMethodDispatched,
			.checkScalarInputCount    = 0,
			.checkStructureInputSize  = kIOUCVariableStructureSize,
			.checkScalarOutputCount   = 0,
			.checkStructureOutputSize = 0,
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
		[kIOTrackingStartCapture] = {
			.function                             = &IOTrackingMethodDispatched,
			.checkScalarInputCount    = 0,
			.checkStructureInputSize  = kIOUCVariableStructureSize,
			.checkScalarOutputCount   = 0,
			.checkStructureOutputSize = 0,
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
		[kIOTrackingStopCapture] = {
			.function                             = &IOTrackingMethodDispatched,
			.checkScalarInputCount    = 0,
			.checkStructureInputSize  = kIOUCVariableStructureSize,
			.checkScalarOutputCount   = 0,
			.checkStructureOutputSize = 0,
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
		[kIOTrackingSetMinCaptureSize] = {
			.function                             = &IOTrackingMethodDispatched,
			.checkScalarInputCount    = 0,
			.checkStructureInputSize  = kIOUCVariableStructureSize,
			.checkScalarOutputCount   = 0,
			.checkStructureOutputSize = 0,
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
		[kIOTrackingLeaks] = {
			.function                             = &IOTrackingMethodDispatched,
			.checkScalarInputCount    = 0,
			.checkStructureInputSize  = kIOUCVariableStructureSize,
			.checkScalarOutputCount   = 0,
			.checkStructureOutputSize = 0,
			.allowAsync               = false,
			.checkEntitlement         = NULL,
		},
	};

	return dispatchExternalMethod(selector, args, dispatchArray, sizeof(dispatchArray) / sizeof(dispatchArray[0]), this, NULL);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
