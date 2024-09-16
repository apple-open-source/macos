/*
 * Copyright (c) 2014-2020 Apple Inc. All rights reserved.
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

#include <sys/cdefs.h>

#include <IOKit/assert.h>
#include <IOKit/system.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOMapper.h>
#include <IOKit/IODMACommand.h>
#include <IOKit/IOKitKeysPrivate.h>
#include <Kernel/IOKitKernelInternal.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOService.h>
#include "Tests.h"

#ifndef __LP64__
#include <IOKit/IOSubMemoryDescriptor.h>
#endif /* !__LP64__ */
#include <IOKit/IOSubMemoryDescriptor.h>
#include <IOKit/IOMultiMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOGuardPageMemoryDescriptor.h>

#include <IOKit/IOKitDebug.h>
#include <libkern/OSDebug.h>
#include <sys/uio.h>
#include <libkern/sysctl.h>
#include <sys/sysctl.h>

__BEGIN_DECLS
#include <vm/pmap.h>
#include <vm/vm_pageout.h>
#include <mach/memory_object_types.h>
#include <device/device_port.h>

#include <mach/vm_prot.h>
#include <mach/mach_vm.h>
#include <mach/vm_param.h>
#include <vm/vm_fault.h>
#include <vm/vm_protos.h>
#include <vm/vm_kern_xnu.h>
__END_DECLS


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if DEVELOPMENT || DEBUG

extern SInt32 gIOMemoryReferenceCount;

static int
IOMultMemoryDescriptorTest(int newValue)
{
	IOMemoryDescriptor * mds[3];
	IOMultiMemoryDescriptor * mmd;
	IOMemoryMap * map;
	void * addr;
	uint8_t * data;
	uint32_t i;
	IOAddressRange ranges[2];

	data = (typeof(data))IOMallocAligned(ptoa(8), page_size);
	for (i = 0; i < ptoa(8); i++) {
		data[i] = ((uint8_t) atop(i)) | 0xD0;
	}

	ranges[0].address = (IOVirtualAddress)(data + ptoa(4));
	ranges[0].length  = ptoa(4);
	ranges[1].address = (IOVirtualAddress)(data + ptoa(0));
	ranges[1].length  = ptoa(4);

	mds[0] = IOMemoryDescriptor::withAddressRange((mach_vm_address_t) data, 2, kIODirectionOutIn, kernel_task);
	assert(mds[0]);
	{
		uint64_t dmaLen, dmaOffset;
		dmaLen = mds[0]->getDMAMapLength(&dmaOffset);
		assert(0 == dmaOffset);
		assert(ptoa(1) == dmaLen);
	}
	mds[0]->release();
	mds[0] = IOMemoryDescriptor::withAddressRange((mach_vm_address_t) (data + page_size - 2), 4, kIODirectionOutIn, kernel_task);
	assert(mds[0]);
	{
		uint64_t dmaLen, dmaOffset;
		dmaLen = mds[0]->getDMAMapLength(&dmaOffset);
		assert((page_size - 2) == dmaOffset);
		assert(ptoa(2) == dmaLen);
	}
	mds[0]->release();

	mds[0] = IOMemoryDescriptor::withAddressRanges(&ranges[0], 2, kIODirectionOutIn, kernel_task);
	{
		uint64_t dmaLen, dmaOffset;
		dmaLen = mds[0]->getDMAMapLength(&dmaOffset);
		assert(0 == dmaOffset);
		assert(ptoa(8) == dmaLen);
	}
	mds[1] = IOSubMemoryDescriptor::withSubRange(mds[0], ptoa(3), ptoa(2), kIODirectionOutIn);
	{
		uint64_t dmaLen, dmaOffset;
		dmaLen = mds[1]->getDMAMapLength(&dmaOffset);
		assert(0 == dmaOffset);
		assert(ptoa(2) == dmaLen);
	}
	mds[2] = IOSubMemoryDescriptor::withSubRange(mds[0], ptoa(7), ptoa(1), kIODirectionOutIn);

	mmd = IOMultiMemoryDescriptor::withDescriptors(&mds[0], sizeof(mds) / sizeof(mds[0]), kIODirectionOutIn, false);
	{
		uint64_t dmaLen, dmaOffset;
		dmaLen = mmd->getDMAMapLength(&dmaOffset);
		assert(0 == dmaOffset);
		assert(ptoa(11) == dmaLen);
	}
	mds[2]->release();
	mds[1]->release();
	mds[0]->release();
	map = mmd->createMappingInTask(kernel_task, 0, kIOMapAnywhere | kIOMapGuardedSmall, ptoa(7), mmd->getLength() - ptoa(7));
	mmd->release();
	assert(map);

	addr = (void *) map->getVirtualAddress();
	assert(ptoa(4) == map->getLength());
	assert(0xd3d3d3d3 == ((uint32_t *)addr)[ptoa(0) / sizeof(uint32_t)]);
	assert(0xd7d7d7d7 == ((uint32_t *)addr)[ptoa(1) / sizeof(uint32_t)]);
	assert(0xd0d0d0d0 == ((uint32_t *)addr)[ptoa(2) / sizeof(uint32_t)]);
	assert(0xd3d3d3d3 == ((uint32_t *)addr)[ptoa(3) / sizeof(uint32_t)]);
	map->release();
	IOFreeAligned(data, ptoa(8));

	return 0;
}



// <rdar://problem/30102458>
static int
IODMACommandForceDoubleBufferTest(int newValue)
{
	IOReturn                   ret;
	IOBufferMemoryDescriptor * bmd;
	IODMACommand             * dma;
	uint32_t                   dir, data;
	IODMACommand::SegmentOptions segOptions =
	{
		.fStructSize      = sizeof(segOptions),
		.fNumAddressBits  = 64,
		.fMaxSegmentSize  = 0x2000,
		.fMaxTransferSize = 128 * 1024,
		.fAlignment       = 1,
		.fAlignmentLength = 1,
		.fAlignmentInternalSegments = 1
	};
	IODMACommand::Segment64 segments[1];
	UInt32                  numSegments;
	UInt64                  dmaOffset;


	for (dir = kIODirectionIn;; dir++) {
		bmd = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
		    dir | kIOMemoryPageable, ptoa(8));
		assert(bmd);
		{
			uint64_t dmaLen, dmaOffset;
			dmaLen = bmd->getDMAMapLength(&dmaOffset);
			assert(0 == dmaOffset);
			assert(ptoa(8) == dmaLen);
		}

		((uint32_t*) bmd->getBytesNoCopy())[0] = 0x53535300 | dir;

		ret = bmd->prepare((IODirection) dir);
		assert(kIOReturnSuccess == ret);

		dma = IODMACommand::withSpecification(kIODMACommandOutputHost64, &segOptions,
		    kIODMAMapOptionMapped,
		    NULL, NULL);
		assert(dma);
		ret = dma->setMemoryDescriptor(bmd, true);
		assert(kIOReturnSuccess == ret);

		ret = dma->synchronize(IODMACommand::kForceDoubleBuffer | kIODirectionOut);
		assert(kIOReturnSuccess == ret);

		dmaOffset   = 0;
		numSegments = 1;
		ret = dma->gen64IOVMSegments(&dmaOffset, &segments[0], &numSegments);
		assert(kIOReturnSuccess == ret);
		assert(1 == numSegments);

		if (kIODirectionOut & dir) {
			data = ((uint32_t*) bmd->getBytesNoCopy())[0];
			assertf((0x53535300 | dir) == data, "mismatch 0x%x", data);
		}
		if (kIODirectionIn & dir) {
			IOMappedWrite32(segments[0].fIOVMAddr, 0x11223300 | dir);
		}

		ret = dma->clearMemoryDescriptor(true);
		assert(kIOReturnSuccess == ret);
		dma->release();

		bmd->complete((IODirection) dir);

		if (kIODirectionIn & dir) {
			data = ((uint32_t*) bmd->getBytesNoCopy())[0];
			assertf((0x11223300 | dir) == data, "mismatch 0x%x", data);
		}

		bmd->release();

		if (dir == kIODirectionInOut) {
			break;
		}
	}

	return 0;
}

// <rdar://problem/34322778>
static int __unused
IODMACommandLocalMappedNonContig(int newValue)
{
	IOReturn                   kr;
	IOMemoryDescriptor       * md;
	IODMACommand             * dma;
	OSDictionary             * matching;
	IOService                * device;
	IOMapper                 * mapper;
	IODMACommand::SegmentOptions segOptions =
	{
		.fStructSize      = sizeof(segOptions),
		.fNumAddressBits  = 64,
		.fMaxSegmentSize  = 128 * 1024,
		.fMaxTransferSize = 128 * 1024,
		.fAlignment       = 1,
		.fAlignmentLength = 1,
		.fAlignmentInternalSegments = 1
	};
	IODMACommand::Segment64 segments[1];
	UInt32                  numSegments;
	UInt64                  dmaOffset;
	UInt64                  segPhys;
	mach_vm_address_t       buffer;
	vm_size_t               bufSize = ptoa(4);

	if (!IOMapper::gSystem) {
		return 0;
	}

	buffer = 0;
	kr = mach_vm_allocate_kernel(kernel_map, &buffer, bufSize,
	    VM_MAP_KERNEL_FLAGS_ANYWHERE(.vm_tag = VM_KERN_MEMORY_IOKIT));
	assert(KERN_SUCCESS == kr);

	// fragment the vmentries
	kr = mach_vm_inherit(kernel_map, buffer + ptoa(1), ptoa(1), VM_INHERIT_NONE);
	assert(KERN_SUCCESS == kr);

	md = IOMemoryDescriptor::withAddressRange(
		buffer + 0xa00, 0x2000, kIODirectionOutIn, kernel_task);
	assert(md);
	kr = md->prepare(kIODirectionOutIn);
	assert(kIOReturnSuccess == kr);

	segPhys = md->getPhysicalSegment(0, NULL, 0);

	matching = IOService::nameMatching("XHC1");
	assert(matching);
	device = IOService::copyMatchingService(matching);
	matching->release();
	mapper = device ? IOMapper::copyMapperForDeviceWithIndex(device, 0) : NULL;
	OSSafeReleaseNULL(device);

	dma = IODMACommand::withSpecification(kIODMACommandOutputHost64, &segOptions,
	    kIODMAMapOptionMapped,
	    mapper, NULL);
	assert(dma);
	kr = dma->setMemoryDescriptor(md, true);
	assert(kIOReturnSuccess == kr);

	dmaOffset   = 0;
	numSegments = 1;
	kr = dma->gen64IOVMSegments(&dmaOffset, &segments[0], &numSegments);
	assert(kIOReturnSuccess == kr);
	assert(1 == numSegments);

	if (mapper) {
		assertf(segments[0].fIOVMAddr != segPhys, "phys !local 0x%qx, 0x%qx, %p", segments[0].fIOVMAddr, segPhys, dma);
	}

	kr = dma->clearMemoryDescriptor(true);
	assert(kIOReturnSuccess == kr);
	dma->release();

	kr = md->complete(kIODirectionOutIn);
	assert(kIOReturnSuccess == kr);
	md->release();

	kr = mach_vm_deallocate(kernel_map, buffer, bufSize);
	assert(KERN_SUCCESS == kr);
	OSSafeReleaseNULL(mapper);

	return 0;
}

// <rdar://problem/30102458>
static int
IOMemoryRemoteTest(int newValue)
{
	IOReturn             ret;
	IOMemoryDescriptor * md;
	IOByteCount          offset, length;
	addr64_t             addr;
	uint32_t             idx;

	IODMACommand       * dma;
	IODMACommand::SegmentOptions segOptions =
	{
		.fStructSize      = sizeof(segOptions),
		.fNumAddressBits  = 64,
		.fMaxSegmentSize  = 0x2000,
		.fMaxTransferSize = 128 * 1024,
		.fAlignment       = 1,
		.fAlignmentLength = 1,
		.fAlignmentInternalSegments = 1
	};
	IODMACommand::Segment64 segments[1];
	UInt32                  numSegments;
	UInt64                  dmaOffset;

	IOAddressRange ranges[2] = {
		{ 0x1234567890123456ULL, 0x1000 }, { 0x5432109876543210, 0x2000 },
	};

	md = IOMemoryDescriptor::withAddressRanges(&ranges[0], 2, kIODirectionOutIn | kIOMemoryRemote, TASK_NULL);
	assert(md);

//    md->map();
//    md->readBytes(0, &idx, sizeof(idx));

	ret = md->prepare(kIODirectionOutIn);
	assert(kIOReturnSuccess == ret);

	printf("remote md flags 0x%qx, r %d\n",
	    md->getFlags(), (0 != (kIOMemoryRemote & md->getFlags())));

	for (offset = 0, idx = 0; true; offset += length, idx++) {
		addr = md->getPhysicalSegment(offset, &length, 0);
		if (!length) {
			break;
		}
		assert(idx < 2);
		assert(addr == ranges[idx].address);
		assert(length == ranges[idx].length);
	}
	assert(offset == md->getLength());

	dma = IODMACommand::withSpecification(kIODMACommandOutputHost64, &segOptions,
	    kIODMAMapOptionUnmapped | kIODMAMapOptionIterateOnly,
	    NULL, NULL);
	assert(dma);
	ret = dma->setMemoryDescriptor(md, true);
	assert(kIOReturnSuccess == ret);

	for (dmaOffset = 0, idx = 0; dmaOffset < md->getLength(); idx++) {
		numSegments = 1;
		ret = dma->gen64IOVMSegments(&dmaOffset, &segments[0], &numSegments);
		assert(kIOReturnSuccess == ret);
		assert(1 == numSegments);
		assert(idx < 2);
		assert(segments[0].fIOVMAddr == ranges[idx].address);
		assert(segments[0].fLength == ranges[idx].length);
	}
	assert(dmaOffset == md->getLength());

	ret = dma->clearMemoryDescriptor(true);
	assert(kIOReturnSuccess == ret);
	dma->release();
	md->complete(kIODirectionOutIn);
	md->release();

	return 0;
}

static IOReturn
IOMemoryPrefaultTest(uint32_t options)
{
	IOBufferMemoryDescriptor * bmd;
	IOMemoryMap              * map;
	IOReturn       kr;
	uint32_t       data;
	uint32_t *     p;
	IOSimpleLock * lock;

	lock = IOSimpleLockAlloc();
	assert(lock);

	bmd = IOBufferMemoryDescriptor::inTaskWithOptions(current_task(),
	    kIODirectionOutIn | kIOMemoryPageable, ptoa(8));
	assert(bmd);
	kr = bmd->prepare();
	assert(KERN_SUCCESS == kr);

	map = bmd->map(kIOMapPrefault);
	assert(map);

	p = (typeof(p))map->getVirtualAddress();
	IOSimpleLockLock(lock);
	data = p[0];
	IOSimpleLockUnlock(lock);

	IOLog("IOMemoryPrefaultTest %d\n", data);

	map->release();
	bmd->release();
	IOSimpleLockFree(lock);

	return kIOReturnSuccess;
}

static IOReturn
IOBMDOverflowTest(uint32_t options)
{
	IOBufferMemoryDescriptor * bmd;

	bmd = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task, kIOMemoryPageable | kIODirectionOut,
	    0xffffffffffffffff, 0);
	assert(NULL == bmd);

	return kIOReturnSuccess;
}

static IOReturn
IOBMDSetLengthMapTest(uint32_t options)
{
	IOBufferMemoryDescriptor * bmd;
	IOMemoryMap * map;

	bmd = IOBufferMemoryDescriptor::inTaskWithOptions(
		kernel_task, kIOMemoryDirectionOutIn | kIOMemoryKernelUserShared, 0x4000, 0x4000);
	assert(bmd);

	bmd->setLength(0x100);
	map = bmd->createMappingInTask(current_task(), 0, kIOMapAnywhere, 0, 0);
	assert(map);
	OSSafeReleaseNULL(map);

	bmd->setLength(0x200);
	map = bmd->createMappingInTask(current_task(), 0, kIOMapAnywhere, 0, 0);
	assert(map);
	OSSafeReleaseNULL(map);

	bmd->release();

	return kIOReturnSuccess;
}

// <rdar://problem/26375234>
static IOReturn
ZeroLengthTest(int newValue)
{
	IOMemoryDescriptor * md;

	md = IOMemoryDescriptor::withAddressRange(
		0, 0, kIODirectionNone, current_task());
	assert(md);
	md->prepare();
	md->complete();
	md->release();
	return 0;
}

// <rdar://problem/27002624>
static IOReturn
BadFixedAllocTest(int newValue)
{
	IOBufferMemoryDescriptor * bmd;
	IOMemoryMap              * map;

	bmd = IOBufferMemoryDescriptor::inTaskWithOptions(NULL,
	    kIODirectionIn | kIOMemoryPageable, ptoa(1));
	assert(bmd);
	map = bmd->createMappingInTask(kernel_task, 0x2000, 0);
	assert(!map);

	bmd->release();
	return 0;
}

// <rdar://problem/26466423>
static IOReturn
IODirectionPrepareNoZeroFillTest(int newValue)
{
	IOBufferMemoryDescriptor * bmd;

	bmd = IOBufferMemoryDescriptor::inTaskWithOptions(NULL,
	    kIODirectionIn | kIOMemoryPageable, ptoa(24));
	assert(bmd);
	bmd->prepare((IODirection)(kIODirectionIn | kIODirectionPrepareNoZeroFill));
	bmd->prepare(kIODirectionIn);
	bmd->complete((IODirection)(kIODirectionIn | kIODirectionCompleteWithDataValid));
	bmd->complete(kIODirectionIn);
	bmd->release();
	return 0;
}

// <rdar://problem/28190483>
static IOReturn
IOMemoryMapTest(uint32_t options)
{
	IOBufferMemoryDescriptor * bmd;
	IOMemoryDescriptor       * md;
	IOMemoryMap              * map;
	uint32_t    data;
	user_addr_t p;
	uint8_t *   p2;
	int         r;
	uint64_t    time, nano;

	bmd = IOBufferMemoryDescriptor::inTaskWithOptions(current_task(),
	    kIODirectionOutIn | kIOMemoryPageable, 0x4018 + 0x800);
	assert(bmd);
	p = (typeof(p))bmd->getBytesNoCopy();
	p += 0x800;
	data = 0x11111111;
	r = copyout(&data, p, sizeof(data));
	assert(r == 0);
	data = 0x22222222;
	r = copyout(&data, p + 0x1000, sizeof(data));
	assert(r == 0);
	data = 0x33333333;
	r = copyout(&data, p + 0x2000, sizeof(data));
	assert(r == 0);
	data = 0x44444444;
	r = copyout(&data, p + 0x3000, sizeof(data));
	assert(r == 0);

	md = IOMemoryDescriptor::withAddressRange(p, 0x4018,
	    kIODirectionOut | options,
	    current_task());
	assert(md);
	time = mach_absolute_time();
	map = md->map(kIOMapReadOnly);
	time = mach_absolute_time() - time;
	assert(map);
	absolutetime_to_nanoseconds(time, &nano);

	p2 = (typeof(p2))map->getVirtualAddress();
	assert(0x11 == p2[0]);
	assert(0x22 == p2[0x1000]);
	assert(0x33 == p2[0x2000]);
	assert(0x44 == p2[0x3000]);

	data = 0x99999999;
	r = copyout(&data, p + 0x2000, sizeof(data));
	assert(r == 0);

	assert(0x11 == p2[0]);
	assert(0x22 == p2[0x1000]);
	assert(0x44 == p2[0x3000]);
	if (kIOMemoryMapCopyOnWrite & options) {
		assert(0x33 == p2[0x2000]);
	} else {
		assert(0x99 == p2[0x2000]);
	}

	IOLog("IOMemoryMapCopyOnWriteTest map(%s) %lld ns\n",
	    kIOMemoryMapCopyOnWrite & options ? "kIOMemoryMapCopyOnWrite" : "",
	    nano);

	map->release();
	md->release();
	bmd->release();

	return kIOReturnSuccess;
}

static int
IOMemoryMapCopyOnWriteTest(int newValue)
{
	IOMemoryMapTest(0);
	IOMemoryMapTest(kIOMemoryMapCopyOnWrite);
	return 0;
}

static int
AllocationNameTest(int newValue)
{
	IOMemoryDescriptor * bmd;
	kern_allocation_name_t name, prior;

	name = kern_allocation_name_allocate("com.apple.iokit.test", 0);
	assert(name);

	prior = thread_set_allocation_name(name);

	bmd = IOBufferMemoryDescriptor::inTaskWithOptions(TASK_NULL,
	    kIODirectionOutIn | kIOMemoryPageable | kIOMemoryKernelUserShared,
	    ptoa(13));
	assert(bmd);
	bmd->prepare();

	thread_set_allocation_name(prior);
	kern_allocation_name_release(name);

	if (newValue != 7) {
		bmd->release();
	}

	return 0;
}

static IOReturn
IOGuardPageMDTest(int newValue)
{
	constexpr size_t MAX_LEFT_GUARD_PAGES = 5;
	constexpr size_t MAX_RIGHT_GUARD_PAGES = 5;

	IOMemoryDescriptor * mds[3];
	IOMemoryDescriptor * dataMD;
	IOMultiMemoryDescriptor * mmd;
	IOBufferMemoryDescriptor * iobmd;
	IOMemoryMap * map;
	void * addr;
	uint8_t * data;
	uint32_t i;

	data = (typeof(data))IOMallocAligned(page_size, page_size);
	for (i = 0; i < page_size; i++) {
		data[i] = (uint8_t)(i & 0xFF);
	}

	dataMD = IOMemoryDescriptor::withAddressRange((mach_vm_address_t) data, page_size, kIODirectionOutIn, kernel_task);
	assert(dataMD);


	for (size_t leftGuardSize = 1; leftGuardSize < MAX_LEFT_GUARD_PAGES; leftGuardSize++) {
		for (size_t rightGuardSize = 1; rightGuardSize < MAX_RIGHT_GUARD_PAGES; rightGuardSize++) {
			mds[0] = IOGuardPageMemoryDescriptor::withSize(page_size * leftGuardSize);
			assert(mds[0]);

			mds[1] = dataMD;
			mds[1]->retain();

			mds[2] = IOGuardPageMemoryDescriptor::withSize(page_size * rightGuardSize);
			assert(mds[2]);

			mmd = IOMultiMemoryDescriptor::withDescriptors(&mds[0], sizeof(mds) / sizeof(mds[0]), kIODirectionOutIn, false);

			OSSafeReleaseNULL(mds[2]);
			OSSafeReleaseNULL(mds[1]);
			OSSafeReleaseNULL(mds[0]);

			map = mmd->createMappingInTask(kernel_task, 0, kIOMapAnywhere, 0, mmd->getLength());

			OSSafeReleaseNULL(mmd);
			assert(map);
			addr = (void *)map->getAddress();

			// check data
			for (i = 0; i < page_size; i++) {
				assert(*(uint8_t *)((uintptr_t)addr + page_size * leftGuardSize + i) == (uint8_t)(i & 0xFF));
			}

			// check map length
			assert(page_size * leftGuardSize + page_size + page_size * rightGuardSize == map->getLength());

			// check page protections
			for (i = 0; i < leftGuardSize + 1 + rightGuardSize; i++) {
				mach_vm_address_t regionAddr = (vm_address_t)addr + i * page_size;
				mach_vm_size_t regionSize;
				vm_region_extended_info regionInfo;
				mach_msg_type_number_t count = VM_REGION_EXTENDED_INFO_COUNT;
				mach_port_t unused;
				kern_return_t kr = mach_vm_region(kernel_map, &regionAddr, &regionSize, VM_REGION_EXTENDED_INFO, (vm_region_info_t)&regionInfo, &count, &unused);
				assert(kr == KERN_SUCCESS);
				if (i < leftGuardSize || i > leftGuardSize + 1) {
					assert(regionInfo.protection == VM_PROT_NONE);
				}
			}
			OSSafeReleaseNULL(map);
		}
	}

	OSSafeReleaseNULL(dataMD);
	IOFreeAligned(data, page_size);

	for (size_t iobmdCapacity = page_size / 8; iobmdCapacity < page_size * 10; iobmdCapacity += page_size / 8) {
		iobmd = IOBufferMemoryDescriptor::inTaskWithGuardPages(kernel_task, kIODirectionOutIn, iobmdCapacity);

		// Capacity should be rounded up to page size
		assert(iobmd->getLength() == round_page(iobmdCapacity));

		// Buffer should be page aligned
		addr = iobmd->getBytesNoCopy();
		assert((vm_offset_t)addr == round_page((vm_offset_t)addr));

		// fill buffer
		for (size_t i = 0; i < iobmdCapacity; i++) {
			*((char *)addr + i) = (char)(i & 0xFF);
		}

		map = iobmd->createMappingInTask(kernel_task, 0, kIOMapAnywhere | kIOMapUnique, 0, iobmd->getLength());
		assert(map->getLength() == iobmd->getLength());

		// check buffer
		for (size_t i = 0; i < iobmdCapacity; i++) {
			assert(*((char *)map->getAddress() + i) == (char)(i & 0xFF));
		}

		OSSafeReleaseNULL(map);
		OSSafeReleaseNULL(iobmd);
	}

	return kIOReturnSuccess;
}

static IOReturn
IOMDContextTest(int newValue)
{
	IOBufferMemoryDescriptor * bmd = IOBufferMemoryDescriptor::inTaskWithOptions(TASK_NULL,
	    kIODirectionOutIn | kIOMemoryPageable | kIOMemoryKernelUserShared,
	    ptoa(13));

	OSObject * current = NULL;
	OSString * firstString = OSString::withCStringNoCopy("firstString");
	OSString * secondString = OSString::withCStringNoCopy("secondString");

	assert(bmd->copyContext() == NULL);

	bmd->setContext(NULL);
	assert(bmd->copyContext() == NULL);

	bmd->setContext(firstString);
	current = bmd->copyContext();
	assert(current == firstString);
	OSSafeReleaseNULL(current);

	bmd->setContext(NULL);
	assert(bmd->copyContext() == NULL);

	bmd->setContext(secondString);
	current = bmd->copyContext();
	assert(current == secondString);
	OSSafeReleaseNULL(current);

	bmd->release();

	assert(firstString->getRetainCount() == 1);
	assert(secondString->getRetainCount() == 1);

	firstString->release();
	secondString->release();

	return kIOReturnSuccess;
}

int
IOMemoryDescriptorTest(int newValue)
{
	int result;

	IOLog("/IOMemoryDescriptorTest %d\n", (int) gIOMemoryReferenceCount);

#if 0
	if (6 == newValue) {
		IOMemoryDescriptor * sbmds[3];
		IOMultiMemoryDescriptor * smmd;
		IOMemoryDescriptor * mds[2];
		IOMultiMemoryDescriptor * mmd;
		IOMemoryMap * map;

		sbmds[0] = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, kIODirectionOutIn | kIOMemoryKernelUserShared, ptoa(1));
		sbmds[1] = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, kIODirectionOutIn | kIOMemoryKernelUserShared, ptoa(2));
		sbmds[2] = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, kIODirectionOutIn | kIOMemoryKernelUserShared, ptoa(3));
		smmd = IOMultiMemoryDescriptor::withDescriptors(&sbmds[0], sizeof(sbmds) / sizeof(sbmds[0]), kIODirectionOutIn, false);

		mds[0] = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, kIODirectionOutIn | kIOMemoryKernelUserShared, ptoa(1));
		mds[1] = smmd;
		mmd = IOMultiMemoryDescriptor::withDescriptors(&mds[0], sizeof(mds) / sizeof(mds[0]), kIODirectionOutIn, false);
		map = mmd->createMappingInTask(kernel_task, 0, kIOMapAnywhere | kIOMapGuardedSmall);
		assert(map);
		map->release();
		mmd->release();
		mds[0]->release();
		mds[1]->release();
		sbmds[0]->release();
		sbmds[1]->release();
		sbmds[2]->release();

		return 0;
	} else if (5 == newValue) {
		IOReturn             ret;
		IOMemoryDescriptor * md;
		IODMACommand       * dma;
		IODMACommand::SegmentOptions segOptions =
		{
			.fStructSize      = sizeof(segOptions),
			.fNumAddressBits  = 64,
			.fMaxSegmentSize  = 4096,
			.fMaxTransferSize = 128 * 1024,
			.fAlignment       = 4,
			.fAlignmentLength = 4,
			.fAlignmentInternalSegments = 0x1000
		};

		IOAddressRange ranges[3][2] =
		{
			{
				{ (uintptr_t) &IOMemoryDescriptorTest, 0x2ffc },
				{ 0, 0 },
			},
			{
				{ ranges[0][0].address, 0x10 },
				{ 0x3000 + ranges[0][0].address, 0xff0 },
			},
			{
				{ ranges[0][0].address, 0x2ffc },
				{ trunc_page(ranges[0][0].address), 0x800 },
			},
		};
		static const uint32_t rangesCount[3] = { 1, 2, 2 };
		uint32_t test;

		for (test = 0; test < 3; test++) {
			kprintf("---[%d] address 0x%qx-0x%qx, 0x%qx-0x%qx\n", test,
			    ranges[test][0].address, ranges[test][0].length,
			    ranges[test][1].address, ranges[test][1].length);

			md = IOMemoryDescriptor::withAddressRanges((IOAddressRange*)&ranges[test][0], rangesCount[test], kIODirectionOut, kernel_task);
			assert(md);
			ret = md->prepare();
			assert(kIOReturnSuccess == ret);
			dma = IODMACommand::withSpecification(kIODMACommandOutputHost64, &segOptions,
			    IODMACommand::kMapped, NULL, NULL);
			assert(dma);
			ret = dma->setMemoryDescriptor(md, true);
			if (kIOReturnSuccess == ret) {
				IODMACommand::Segment64 segments[1];
				UInt32                  numSegments;
				UInt64                  offset;

				offset = 0;
				do{
					numSegments = 1;
					ret = dma->gen64IOVMSegments(&offset, &segments[0], &numSegments);
					assert(kIOReturnSuccess == ret);
					assert(1 == numSegments);
					kprintf("seg 0x%qx, 0x%qx\n", segments[0].fIOVMAddr, segments[0].fLength);
				}while (offset < md->getLength());

				ret = dma->clearMemoryDescriptor(true);
				assert(kIOReturnSuccess == ret);
				dma->release();
			}
			md->release();
		}

		return kIOReturnSuccess;
	} else if (4 == newValue) {
		IOService * isp;
		IOMapper *  mapper;
		IOBufferMemoryDescriptor * md1;
		IODMACommand * dma;
		IOReturn       ret;
		size_t         bufSize = 8192 * 8192 * sizeof(uint32_t);
		uint64_t start, time, nano;

		isp = IOService::copyMatchingService(IOService::nameMatching("isp"));
		assert(isp);
		mapper = IOMapper::copyMapperForDeviceWithIndex(isp, 0);
		assert(mapper);

		md1 = IOBufferMemoryDescriptor::inTaskWithOptions(TASK_NULL,
		    kIODirectionOutIn | kIOMemoryPersistent | kIOMemoryPageable,
		    bufSize, page_size);

		ret = md1->prepare();
		assert(kIOReturnSuccess == ret);

		IODMAMapSpecification mapSpec;
		bzero(&mapSpec, sizeof(mapSpec));
		uint64_t mapped;
		uint64_t mappedLength;

		start = mach_absolute_time();

		ret =  md1->dmaMap(mapper, NULL, &mapSpec, 0, bufSize, &mapped, &mappedLength);
		assert(kIOReturnSuccess == ret);

		time = mach_absolute_time() - start;

		absolutetime_to_nanoseconds(time, &nano);
		kprintf("time %lld us\n", nano / 1000ULL);
		kprintf("seg0 0x%qx, 0x%qx\n", mapped, mappedLength);

		assert(md1);

		dma = IODMACommand::withSpecification(kIODMACommandOutputHost32,
		    32, 0, IODMACommand::kMapped, 0, 1, mapper, NULL);

		assert(dma);

		start = mach_absolute_time();
		ret = dma->setMemoryDescriptor(md1, true);
		assert(kIOReturnSuccess == ret);
		time = mach_absolute_time() - start;

		absolutetime_to_nanoseconds(time, &nano);
		kprintf("time %lld us\n", nano / 1000ULL);


		IODMACommand::Segment32 segments[1];
		UInt32                  numSegments = 1;
		UInt64                  offset;

		offset = 0;
		ret = dma->gen32IOVMSegments(&offset, &segments[0], &numSegments);
		assert(kIOReturnSuccess == ret);
		assert(1 == numSegments);
		kprintf("seg0 0x%x, 0x%x\n", (int)segments[0].fIOVMAddr, (int)segments[0].fLength);

		ret = dma->clearMemoryDescriptor(true);
		assert(kIOReturnSuccess == ret);

		md1->release();

		return kIOReturnSuccess;
	}

	if (3 == newValue) {
		IOBufferMemoryDescriptor * md1;
		IOBufferMemoryDescriptor * md2;
		IOMemoryMap * map1;
		IOMemoryMap * map2;
		uint32_t * buf1;
		uint32_t * buf2;
		IOReturn err;

		md1 = IOBufferMemoryDescriptor::inTaskWithOptions(TASK_NULL,
		    kIODirectionOutIn | kIOMemoryPersistent | kIOMemoryPageable,
		    64 * 1024, page_size);
		assert(md1);
		map1 = md1->createMappingInTask(kernel_task, 0, kIOMapAnywhere | kIOMapUnique);
		assert(map1);
		buf1 = (uint32_t *) map1->getVirtualAddress();

		md2 = IOBufferMemoryDescriptor::inTaskWithOptions(TASK_NULL,
		    kIODirectionOutIn | kIOMemoryPersistent | kIOMemoryPageable,
		    64 * 1024, page_size);
		assert(md2);
		map2 = md2->createMappingInTask(kernel_task, 0, kIOMapAnywhere | kIOMapUnique);
		assert(map2);
		buf2 = (uint32_t *) map2->getVirtualAddress();

		memset(buf1, 0x11, 64 * 1024L);
		memset(buf2, 0x22, 64 * 1024L);

		kprintf("md1 %p, map1 %p, buf2 %p; md2 %p, map2 %p, buf2 %p\n", md1, map1, buf1, md2, map2, buf2);

		kprintf("no redir 0x%08x, 0x%08x\n", buf1[0], buf2[0]);
		assert(0x11111111 == buf1[0]);
		assert(0x22222222 == buf2[0]);
		err = map1->redirect(md2, 0, 0ULL);
		kprintf("redir md2(0x%x) 0x%08x, 0x%08x\n", err, buf1[0], buf2[0]);
		assert(0x11111111 == buf2[0]);
		assert(0x22222222 == buf1[0]);
		err = map1->redirect(md1, 0, 0ULL);
		kprintf("redir md1(0x%x) 0x%08x, 0x%08x\n", err, buf1[0], buf2[0]);
		assert(0x11111111 == buf1[0]);
		assert(0x22222222 == buf2[0]);
		map1->release();
		map2->release();
		md1->release();
		md2->release();
	}
#endif

//    result = IODMACommandLocalMappedNonContig(newValue);
//    if (result) return (result);

	result = IODMACommandForceDoubleBufferTest(newValue);
	if (result) {
		return result;
	}

	result = AllocationNameTest(newValue);
	if (result) {
		return result;
	}

	result = IOMemoryMapCopyOnWriteTest(newValue);
	if (result) {
		return result;
	}

	result = IOMultMemoryDescriptorTest(newValue);
	if (result) {
		return result;
	}

	result = IOBMDOverflowTest(newValue);
	if (result) {
		return result;
	}

	result = IOBMDSetLengthMapTest(newValue);
	if (result) {
		return result;
	}

	result = ZeroLengthTest(newValue);
	if (result) {
		return result;
	}

	result = IODirectionPrepareNoZeroFillTest(newValue);
	if (result) {
		return result;
	}

	result = BadFixedAllocTest(newValue);
	if (result) {
		return result;
	}

	result = IOMemoryRemoteTest(newValue);
	if (result) {
		return result;
	}

	result = IOMemoryPrefaultTest(newValue);
	if (result) {
		return result;
	}

	result = IOGuardPageMDTest(newValue);
	if (result) {
		return result;
	}

	result = IOMDContextTest(newValue);
	if (result) {
		return result;
	}

	IOGeneralMemoryDescriptor * md;
	mach_vm_offset_t data[2];
	vm_size_t  bsize = 16 * 1024 * 1024;
	vm_size_t  srcsize, srcoffset, mapoffset, size;
	kern_return_t kr;

	data[0] = data[1] = 0;
	kr = mach_vm_allocate_kernel(kernel_map, &data[0], bsize,
	    VM_MAP_KERNEL_FLAGS_ANYWHERE(.vm_tag = VM_KERN_MEMORY_IOKIT));
	assert(KERN_SUCCESS == kr);

	mach_vm_inherit(kernel_map, data[0] + ptoa(1), ptoa(1), VM_INHERIT_NONE);
	mach_vm_inherit(kernel_map, data[0] + ptoa(16), ptoa(4), VM_INHERIT_NONE);

	IOLog("data 0x%lx, 0x%lx\n", (long)data[0], (long)data[1]);

	uint32_t idx, offidx;
	for (idx = 0; idx < (bsize / sizeof(uint32_t)); idx++) {
		((uint32_t*)data[0])[idx] = idx;
	}

	for (srcoffset = 0; srcoffset < bsize; srcoffset = ((srcoffset << 2) + 0x40c)) {
		for (srcsize = 4; srcsize < (bsize - srcoffset - 1); srcsize = ((srcsize << 2) + 0x3fc)) {
			IOAddressRange ranges[3];
			uint32_t rangeCount = 1;

			bzero(&ranges[0], sizeof(ranges));
			ranges[0].address = data[0] + srcoffset;
			ranges[0].length  = srcsize;
			ranges[1].address = ranges[2].address = data[0];

			if (srcsize > ptoa(5)) {
				ranges[0].length  = 7634;
				ranges[1].length  = 9870;
				ranges[2].length  = srcsize - ranges[0].length - ranges[1].length;
				ranges[1].address = ranges[0].address + ranges[0].length;
				ranges[2].address = ranges[1].address + ranges[1].length;
				rangeCount = 3;
			} else if ((srcsize > ptoa(2)) && !(page_mask & srcoffset)) {
				ranges[0].length  = ptoa(1);
				ranges[1].length  = ptoa(1);
				ranges[2].length  = srcsize - ranges[0].length - ranges[1].length;
				ranges[0].address = data[0] + srcoffset + ptoa(1);
				ranges[1].address = data[0] + srcoffset;
				ranges[2].address = ranges[0].address + ranges[0].length;
				rangeCount = 3;
			}

			md = OSDynamicCast(IOGeneralMemoryDescriptor,
			    IOMemoryDescriptor::withAddressRanges(&ranges[0], rangeCount, kIODirectionInOut, kernel_task));
			assert(md);

			IOLog("IOMemoryDescriptor::withAddressRanges [0x%lx @ 0x%lx]\n[0x%llx, 0x%llx],\n[0x%llx, 0x%llx],\n[0x%llx, 0x%llx]\n",
			    (long) srcsize, (long) srcoffset,
			    (long long) ranges[0].address - data[0], (long long) ranges[0].length,
			    (long long) ranges[1].address - data[0], (long long) ranges[1].length,
			    (long long) ranges[2].address - data[0], (long long) ranges[2].length);

			if (kIOReturnSuccess == kr) {
				for (mapoffset = 0; mapoffset < srcsize; mapoffset = ((mapoffset << 1) + 0xf00)) {
					for (size = 4; size < (srcsize - mapoffset - 1); size = ((size << 2) + 0x200)) {
						IOMemoryMap     * map;
						mach_vm_address_t addr = 0;
						uint32_t          data;

//			IOLog("<mapRef [0x%lx @ 0x%lx]\n", (long) size, (long) mapoffset);

						map = md->createMappingInTask(kernel_task, 0, kIOMapAnywhere | kIOMapGuardedSmall, mapoffset, size);
						if (map) {
							addr = map->getAddress();
						} else {
							kr = kIOReturnError;
						}

//			IOLog(">mapRef 0x%x %llx\n", kr, addr);

						if (kIOReturnSuccess != kr) {
							break;
						}
						kr = md->prepare();
						if (kIOReturnSuccess != kr) {
							panic("prepare() fail 0x%x", kr);
							break;
						}
						for (idx = 0; idx < size; idx += sizeof(uint32_t)) {
							offidx = (typeof(offidx))(idx + mapoffset + srcoffset);
							if ((srcsize <= ptoa(5)) && (srcsize > ptoa(2)) && !(page_mask & srcoffset)) {
								if (offidx < ptoa(2)) {
									offidx ^= ptoa(1);
								}
							}
							offidx /= sizeof(uint32_t);

							if (offidx != ((uint32_t*)addr)[idx / sizeof(uint32_t)]) {
								panic("vm mismatch md %p map %p, @ 0x%x, 0x%lx, 0x%lx,", md, map, idx, (long) srcoffset, (long) mapoffset);
								kr = kIOReturnBadMedia;
							} else {
								if (sizeof(data) != md->readBytes(mapoffset + idx, &data, sizeof(data))) {
									data = 0;
								}
								if (offidx != data) {
									panic("phys mismatch md %p map %p, @ 0x%x, 0x%lx, 0x%lx,", md, map, idx, (long) srcoffset, (long) mapoffset);
									kr = kIOReturnBadMedia;
								}
							}
						}
						md->complete();
						map->release();
//			IOLog("unmapRef %llx\n", addr);
					}
					if (kIOReturnSuccess != kr) {
						break;
					}
				}
			}
			md->release();
			if (kIOReturnSuccess != kr) {
				break;
			}
		}
		if (kIOReturnSuccess != kr) {
			break;
		}
	}

	if (kIOReturnSuccess != kr) {
		IOLog("FAIL: src 0x%lx @ 0x%lx, map 0x%lx @ 0x%lx\n",
		    (long) srcsize, (long) srcoffset, (long) size, (long) mapoffset);
	}

	assert(kr == kIOReturnSuccess);

	mach_vm_deallocate(kernel_map, data[0], bsize);
	//mach_vm_deallocate(kernel_map, data[1], size);

	IOLog("IOMemoryDescriptorTest/ %d\n", (int) gIOMemoryReferenceCount);

	return 0;
}


#endif  /* DEVELOPMENT || DEBUG */
