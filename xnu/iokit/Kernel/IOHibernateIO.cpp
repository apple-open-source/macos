/*
 * Copyright (c) 2004-2024 Apple Inc. All rights reserved.
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

/*
 *
 *  Sleep:
 *
 *  - PMRootDomain calls IOHibernateSystemSleep() before system sleep
 *  (devices awake, normal execution context)
 *  - IOHibernateSystemSleep opens the hibernation file (or partition) at the bsd level,
 *  grabs its extents and searches for a polling driver willing to work with that IOMedia.
 *  The BSD code makes an ioctl to the storage driver to get the partition base offset to
 *  the disk, and other ioctls to get the transfer constraints
 *  If successful, the file is written to make sure its initially not bootable (in case of
 *  later failure) and nvram set to point to the first block of the file. (Has to be done
 *  here so blocking is possible in nvram support).
 *  hibernate_setup() in osfmk is called to allocate page bitmaps for all dram, and
 *  page out any pages it wants to (currently zero, but probably some percentage of memory).
 *  Its assumed just allocating pages will cause the VM system to naturally select the best
 *  pages for eviction. It also copies processor flags needed for the restore path and sets
 *  a flag in the boot processor proc info.
 *  gIOHibernateState = kIOHibernateStateHibernating.
 *  - Regular sleep progresses - some drivers may inspect the root domain property
 *  kIOHibernateStateKey to modify behavior. The platform driver saves state to memory
 *  as usual but leaves motherboard I/O on.
 *  - Eventually the platform calls ml_ppc_sleep() in the shutdown context on the last cpu,
 *  at which point memory is ready to be saved. mapping_hibernate_flush() is called to get
 *  all ppc RC bits out of the hash table and caches into the mapping structures.
 *  - hibernate_write_image() is called (still in shutdown context, no blocking or preemption).
 *  hibernate_page_list_setall() is called to get a bitmap of dram pages that need to be saved.
 *  All pages are assumed to be saved (as part of the wired image) unless explicitly subtracted
 *  by hibernate_page_list_setall(), avoiding having to find arch dependent low level bits.
 *  The image header and block list are written. The header includes the second file extent so
 *  only the header block is needed to read the file, regardless of filesystem.
 *  The kernel segment "__HIB" is written uncompressed to the image. This segment of code and data
 *  (only) is used to decompress the image during wake/boot.
 *  Some additional pages are removed from the bitmaps - the buffers used for hibernation.
 *  The bitmaps are written to the image.
 *  More areas are removed from the bitmaps (after they have been written to the image) - the
 *  segment "__HIB" pages and interrupt stack.
 *  Each wired page is compressed and written and then each non-wired page. Compression and
 *  disk writes are in parallel.
 *  The image header is written to the start of the file and the polling driver closed.
 *  The machine powers down (or sleeps).
 *
 *  Boot/Wake:
 *
 *  - BootX sees the boot-image nvram variable containing the device and block number of the image,
 *  reads the header and if the signature is correct proceeds. The boot-image variable is cleared.
 *  - BootX reads the portion of the image used for wired pages, to memory. Its assumed this will fit
 *  in the OF memory environment, and the image is decrypted. There is no decompression in BootX,
 *  that is in the kernel's __HIB section.
 *  - BootX copies the "__HIB" section to its correct position in memory, quiesces and calls its entry
 *  hibernate_kernel_entrypoint(), passing the location of the image in memory. Translation is off,
 *  only code & data in that section is safe to call since all the other wired pages are still
 *  compressed in the image.
 *  - hibernate_kernel_entrypoint() removes pages occupied by the raw image from the page bitmaps.
 *  It uses the bitmaps to work out which pages can be uncompressed from the image to their final
 *  location directly, and copies those that can't to interim free pages. When the image has been
 *  completed, the copies are uncompressed, overwriting the wired image pages.
 *  hibernate_restore_phys_page() (in osfmk since its arch dependent, but part of the "__HIB" section)
 *  is used to get pages into place for 64bit.
 *  - the reset vector is called (at least on ppc), the kernel proceeds on a normal wake, with some
 *  changes conditional on the per proc flag - before VM is turned on the boot cpu, all mappings
 *  are removed from the software strutures, and the hash table is reinitialized.
 *  - After the platform CPU init code is called, hibernate_machine_init() is called to restore the rest
 *  of memory, using the polled mode driver, before other threads can run or any devices are turned on.
 *  This reduces the memory usage for BootX and allows decompression in parallel with disk reads,
 *  for the remaining non wired pages.
 *  - The polling driver is closed down and regular wake proceeds. When the kernel calls iokit to wake
 *  (normal execution context) hibernate_teardown() in osmfk is called to release any memory, the file
 *  is closed via bsd.
 *
 *  Polled Mode I/O:
 *
 *  IOHibernateSystemSleep() finds a polled mode interface to the ATA controller via a property in the
 *  registry, specifying an object of calls IOPolledInterface.
 *
 *  Before the system goes to sleep it searches from the IOMedia object (could be a filesystem or
 *  partition) that the image is going to live, looking for polled interface properties. If it finds
 *  one the IOMedia object is passed to a "probe" call for the interface to accept or reject. All the
 *  interfaces found are kept in an ordered list.
 *
 *  There is an Open/Close pair of calls made to each of the interfaces at various stages since there are
 *  few different contexts things happen in:
 *
 *  - there is an Open/Close (Preflight) made before any part of the system has slept (I/O is all
 *  up and running) and after wake - this is safe to allocate memory and do anything. The device
 *  ignores sleep requests from that point since its a waste of time if it goes to sleep and
 *  immediately wakes back up for the image write.
 *
 *  - there is an Open/Close (BeforeSleep) pair made around the image write operations that happen
 *  immediately before sleep. These can't block or allocate memory - the I/O system is asleep apart
 *  from the low level bits (motherboard I/O etc). There is only one thread running. The close can be
 *  used to flush and set the disk to sleep.
 *
 *  - there is an Open/Close (AfterSleep) pair made around the image read operations that happen
 *  immediately after sleep. These can't block or allocate memory. This is happening after the platform
 *  expert has woken the low level bits of the system, but most of the I/O system has not. There is only
 *  one thread running.
 *
 *  For the actual I/O, all the ops are with respect to a single IOMemoryDescriptor that was passed
 *  (prepared) to the Preflight Open() call. There is a read/write op, buffer offset to the IOMD for
 *  the data, an offset to the disk and length (block aligned 64 bit numbers), and completion callback.
 *  Each I/O is async but only one is ever outstanding. The polled interface has a checkForWork call
 *  that is called for the hardware to check for events, and complete the I/O via the callback.
 *  The hibernate path uses the same transfer constraints the regular cluster I/O path in BSD uses
 *  to restrict I/O ops.
 */

#include <sys/systm.h>

#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOKitDebug.h>
#include <IOKit/IOTimeStamp.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/pwr_mgt/IOPMPrivate.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitKeysPrivate.h>
#include "RootDomainUserClient.h"
#include <IOKit/pwr_mgt/IOPowerConnection.h>
#include "IOPMPowerStateQueue.h"
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/AppleKeyStoreInterface.h>
#include <libkern/crypto/aes.h>

#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/fcntl.h>                       // (FWRITE, ...)
#include <sys/sysctl.h>
#include <sys/kdebug.h>
#include <stdint.h>

#include <IOKit/IOHibernatePrivate.h>
#include <IOKit/IOPolledInterface.h>
#include <IOKit/IONVRAM.h>
#include "IOHibernateInternal.h"
#include <vm/vm_protos.h>
#include <vm/vm_kern_xnu.h>
#include <vm/vm_iokit.h>
#include "IOKitKernelInternal.h"
#include <pexpert/device_tree.h>

#include <machine/pal_routines.h>
#include <machine/pal_hibernate.h>
#if defined(__i386__) || defined(__x86_64__)
#include <i386/tsc.h>
#include <i386/cpuid.h>
#include <vm/WKdm_new.h>
#elif defined(__arm64__)
#include <arm64/amcc_rorgn.h>
#include <kern/ecc.h>
#endif /* defined(__i386__) || defined(__x86_64__) */
#include <san/kasan.h>


extern "C" addr64_t             kvtophys(vm_offset_t va);
extern "C" vm_offset_t          phystokv(addr64_t phys);
extern "C" ppnum_t              pmap_find_phys(pmap_t pmap, addr64_t va);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define DISABLE_TRIM            0
#define TRIM_DELAY              25000

extern unsigned int             save_kdebug_enable;
extern uint32_t                 gIOHibernateState;
uint32_t                        gIOHibernateMode;
static char                     gIOHibernateBootSignature[256 + 1];
static char                     gIOHibernateFilename[MAXPATHLEN + 1];
uint32_t                        gIOHibernateCount;

static uuid_string_t            gIOHibernateBridgeBootSessionUUIDString;

static uint32_t                 gIOHibernateFreeRatio = 0;       // free page target (percent)
uint32_t                        gIOHibernateFreeTime  = 0 * 1000;  // max time to spend freeing pages (ms)

enum {
	HIB_COMPR_RATIO_ARM64  = (0xa5),  // compression ~65%. Since we don't support retries we start higher.
	HIB_COMPR_RATIO_INTEL  = (0x80)   // compression 50%
};

#if defined(__arm64__)
static uint64_t                 gIOHibernateCompression = HIB_COMPR_RATIO_ARM64;
#else
static uint64_t                 gIOHibernateCompression = HIB_COMPR_RATIO_INTEL;
#endif /* __arm64__ */
boolean_t                       gIOHibernateStandbyDisabled;

static IODTNVRAM *              gIOOptionsEntry;
static IORegistryEntry *        gIOChosenEntry;

static const OSSymbol *         gIOHibernateBootImageKey;
static const OSSymbol *         gIOHibernateBootSignatureKey;
static const OSSymbol *         gIOBridgeBootSessionUUIDKey;

#if defined(__i386__) || defined(__x86_64__)

static const OSSymbol *         gIOHibernateRTCVariablesKey;
static const OSSymbol *         gIOHibernateBoot0082Key;
static const OSSymbol *         gIOHibernateBootNextKey;
static OSData *                 gIOHibernateBoot0082Data;
static OSData *                 gIOHibernateBootNextData;
static OSObject *               gIOHibernateBootNextSave;

#endif /* defined(__i386__) || defined(__x86_64__) */

static IOLock *                           gFSLock;
uint32_t                           gFSState;
static thread_call_t                      gIOHibernateTrimCalloutEntry;
static IOPolledFileIOVars                 gFileVars;
static IOHibernateVars                    gIOHibernateVars;
static IOPolledFileCryptVars              gIOHibernateCryptWakeContext;
static hibernate_graphics_t               _hibernateGraphics;
static hibernate_graphics_t *             gIOHibernateGraphicsInfo = &_hibernateGraphics;
static hibernate_statistics_t             _hibernateStats;
static hibernate_statistics_t *           gIOHibernateStats = &_hibernateStats;

enum{
	kFSIdle      = 0,
	kFSOpening   = 2,
	kFSOpened    = 3,
	kFSTimedOut  = 4,
	kFSTrimDelay = 5
};

static IOReturn IOHibernateDone(IOHibernateVars * vars);
static IOReturn IOWriteExtentsToFile(IOPolledFileIOVars * vars, uint32_t signature);
static void     IOSetBootImageNVRAM(OSData * data);
static void     IOHibernateSystemPostWakeTrim(void * p1, void * p2);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

enum { kVideoMapSize  = 80 * 1024 * 1024 };

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if CONFIG_SPTM
/**
 * Copies the handoff pages in the order passed in, into the already-IOKit-allocated
 * handoff region memory pages.
 *
 * @param page_array The source page array to use that contains the handoff region's pages.
 * @param page_count The number of pages to copy from the page array.
 */
void
HibernationCopyHandoffRegionFromPageArray(uint32_t page_array[], uint32_t page_count)
{
	IOHibernateVars *vars = &gIOHibernateVars;

	if (!vars->handoffBuffer) {
		/* Nothing to do! */
		return;
	}

	uint8_t *copyDest = (uint8_t *)vars->handoffBuffer->getBytesNoCopy();

	for (unsigned i = 0; i < page_count; i++) {
		/*
		 * Each entry in the page array is a physical page number, so convert
		 * that to a physical address, then access it via the physical aperture.
		 */
		memcpy(&copyDest[i * PAGE_SIZE], (void *)phystokv(ptoa_64(page_array[i])), PAGE_SIZE);
	}
}
#endif /* CONFIG_SPTM */

// copy from phys addr to MD

static IOReturn
IOMemoryDescriptorWriteFromPhysical(IOMemoryDescriptor * md,
    IOByteCount offset, addr64_t bytes, IOByteCount length)
{
	addr64_t srcAddr = bytes;
	IOByteCount remaining;

	remaining = length = min(length, md->getLength() - offset);
	while (remaining) { // (process another target segment?)
		addr64_t    dstAddr64;
		IOByteCount dstLen;

		dstAddr64 = md->getPhysicalSegment(offset, &dstLen, kIOMemoryMapperNone);
		if (!dstAddr64) {
			break;
		}

		// Clip segment length to remaining
		if (dstLen > remaining) {
			dstLen = remaining;
		}

#if 1
		bcopy_phys(srcAddr, dstAddr64, dstLen);
#else
		copypv(srcAddr, dstAddr64, dstLen,
		    cppvPsnk | cppvFsnk | cppvNoRefSrc | cppvNoModSnk | cppvKmap);
#endif
		srcAddr   += dstLen;
		offset    += dstLen;
		remaining -= dstLen;
	}

	assert(!remaining);

	return remaining ? kIOReturnUnderrun : kIOReturnSuccess;
}

// copy from MD to phys addr

static IOReturn
IOMemoryDescriptorReadToPhysical(IOMemoryDescriptor * md,
    IOByteCount offset, addr64_t bytes, IOByteCount length)
{
	addr64_t dstAddr = bytes;
	IOByteCount remaining;

	remaining = length = min(length, md->getLength() - offset);
	while (remaining) { // (process another target segment?)
		addr64_t    srcAddr64;
		IOByteCount dstLen;

		srcAddr64 = md->getPhysicalSegment(offset, &dstLen, kIOMemoryMapperNone);
		if (!srcAddr64) {
			break;
		}

		// Clip segment length to remaining
		if (dstLen > remaining) {
			dstLen = remaining;
		}

#if 1
		bcopy_phys(srcAddr64, dstAddr, dstLen);
#else
		copypv(srcAddr, dstAddr64, dstLen,
		    cppvPsnk | cppvFsnk | cppvNoRefSrc | cppvNoModSnk | cppvKmap);
#endif
		dstAddr    += dstLen;
		offset     += dstLen;
		remaining  -= dstLen;
	}

	assert(!remaining);

	return remaining ? kIOReturnUnderrun : kIOReturnSuccess;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
hibernate_set_page_state(hibernate_page_list_t * page_list, hibernate_page_list_t * page_list_wired,
    vm_offset_t ppnum, vm_offset_t count, uint32_t kind)
{
	count += ppnum;

	if (count > UINT_MAX) {
		panic("hibernate_set_page_state ppnum");
	}

	switch (kind) {
	case kIOHibernatePageStateUnwiredSave:
		// unwired save
		for (; ppnum < count; ppnum++) {
			hibernate_page_bitset(page_list, FALSE, (uint32_t) ppnum);
			hibernate_page_bitset(page_list_wired, TRUE, (uint32_t) ppnum);
		}
		break;
	case kIOHibernatePageStateWiredSave:
		// wired save
		for (; ppnum < count; ppnum++) {
			hibernate_page_bitset(page_list, FALSE, (uint32_t) ppnum);
			hibernate_page_bitset(page_list_wired, FALSE, (uint32_t) ppnum);
		}
		break;
	case kIOHibernatePageStateFree:
		// free page
		for (; ppnum < count; ppnum++) {
			hibernate_page_bitset(page_list, TRUE, (uint32_t) ppnum);
			hibernate_page_bitset(page_list_wired, TRUE, (uint32_t) ppnum);
		}
		break;
	default:
		panic("hibernate_set_page_state");
	}
}

static void
hibernate_set_descriptor_page_state(IOHibernateVars *vars,
    IOMemoryDescriptor *descriptor,
    uint32_t kind,
    uint32_t *pageCount)
{
	IOItemCount  count;
	addr64_t     phys64;
	IOByteCount  segLen;
	if (descriptor) {
		for (count = 0;
		    (phys64 = descriptor->getPhysicalSegment(count, &segLen, kIOMemoryMapperNone));
		    count += segLen) {
			hibernate_set_page_state(vars->page_list, vars->page_list_wired,
			    atop_64(phys64), atop_32(segLen),
			    kind);
			*pageCount -= atop_32(segLen);
		}
	}
}

static vm_offset_t
hibernate_page_list_iterate(hibernate_page_list_t * list, ppnum_t * pPage)
{
	uint32_t             page = ((typeof(page)) * pPage);
	uint32_t             count;
	hibernate_bitmap_t * bitmap;

	while ((bitmap = hibernate_page_bitmap_pin(list, &page))) {
		count = hibernate_page_bitmap_count(bitmap, TRUE, page);
		if (!count) {
			break;
		}
		page += count;
		if (page <= bitmap->last_page) {
			break;
		}
	}

	*pPage = page;
	if (bitmap) {
		count = hibernate_page_bitmap_count(bitmap, FALSE, page);
	} else {
		count = 0;
	}

	return count;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn
IOHibernateSystemSleep(void)
{
	IOReturn   err;
	OSData *   nvramData;
	OSObject * obj;
	OSString * str;
	OSNumber * num;
	bool       dsSSD, vmflush, swapPinned;
	IOHibernateVars * vars;
	uint64_t   setFileSize = 0;

	gIOHibernateState = kIOHibernateStateInactive;

#if defined(__arm64__)
#endif /* __arm64__ */

	gIOHibernateDebugFlags = 0;
	if (kIOLogHibernate & gIOKitDebug) {
		gIOHibernateDebugFlags |= kIOHibernateDebugRestoreLogs;
	}

	if (IOService::getPMRootDomain()->getHibernateSettings(
		    &gIOHibernateMode, &gIOHibernateFreeRatio, &gIOHibernateFreeTime)) {
		if (kIOHibernateModeSleep & gIOHibernateMode) {
			// default to discard clean for safe sleep
			gIOHibernateMode ^= (kIOHibernateModeDiscardCleanInactive
			    | kIOHibernateModeDiscardCleanActive);
		}
	}

	if ((obj = IOService::getPMRootDomain()->copyProperty(kIOHibernateFileKey))) {
		if ((str = OSDynamicCast(OSString, obj))) {
			strlcpy(gIOHibernateFilename, str->getCStringNoCopy(),
			    sizeof(gIOHibernateFilename));
		}
		obj->release();
	}

	if (!gIOHibernateMode || !gIOHibernateFilename[0]) {
		return kIOReturnUnsupported;
	}

	HIBLOG("hibernate image path: %s\n", gIOHibernateFilename);

	vars = IOMallocType(IOHibernateVars);

	IOLockLock(gFSLock);
	if (!gIOHibernateTrimCalloutEntry) {
		gIOHibernateTrimCalloutEntry = thread_call_allocate(&IOHibernateSystemPostWakeTrim, &gFSLock);
	}
	IOHibernateSystemPostWakeTrim(NULL, NULL);
	thread_call_cancel(gIOHibernateTrimCalloutEntry);
	if (kFSIdle != gFSState) {
		HIBLOG("hibernate file busy\n");
		IOLockUnlock(gFSLock);
		IOFreeType(vars, IOHibernateVars);
		return kIOReturnBusy;
	}
	gFSState = kFSOpening;
	IOLockUnlock(gFSLock);

	swapPinned = false;
	do{
		vars->srcBuffer = IOBufferMemoryDescriptor::withOptions(kIODirectionOutIn,
		    HIBERNATION_SRC_BUFFER_SIZE, page_size);

		vars->handoffBuffer = IOBufferMemoryDescriptor::withOptions(kIODirectionOutIn,
		    ptoa_64(gIOHibernateHandoffPageCount), page_size);

		if (!vars->srcBuffer || !vars->handoffBuffer) {
			err = kIOReturnNoMemory;
			break;
		}

		if ((obj = IOService::getPMRootDomain()->copyProperty(kIOHibernateFileMinSizeKey))) {
			if ((num = OSDynamicCast(OSNumber, obj))) {
				vars->fileMinSize = num->unsigned64BitValue();
			}
			obj->release();
		}
		if ((obj = IOService::getPMRootDomain()->copyProperty(kIOHibernateFileMaxSizeKey))) {
			if ((num = OSDynamicCast(OSNumber, obj))) {
				vars->fileMaxSize = num->unsigned64BitValue();
			}
			obj->release();
		}

		boolean_t encryptedswap = true;
		uint32_t pageCount;
		AbsoluteTime startTime, endTime;
		uint64_t nsec;

		bzero(gIOHibernateCurrentHeader, sizeof(IOHibernateImageHeader));
		gIOHibernateCurrentHeader->debugFlags = gIOHibernateDebugFlags;
		gIOHibernateCurrentHeader->signature = kIOHibernateHeaderInvalidSignature;

		vmflush = ((kOSBooleanTrue == IOService::getPMRootDomain()->getProperty(kIOPMDeepSleepEnabledKey)));
		err = hibernate_alloc_page_lists(&vars->page_list,
		    &vars->page_list_wired,
		    &vars->page_list_pal);
		if (KERN_SUCCESS != err) {
			HIBLOG("%s err, hibernate_alloc_page_lists return 0x%x\n", __FUNCTION__, err);
			break;
		}

		err = hibernate_pin_swap(TRUE);
		if (KERN_SUCCESS != err) {
			HIBLOG("%s error, hibernate_pin_swap return 0x%x\n", __FUNCTION__, err);
			break;
		}
		swapPinned = true;

		if (vars->fileMinSize || (kIOHibernateModeFileResize & gIOHibernateMode)) {
			hibernate_page_list_setall(vars->page_list,
			    vars->page_list_wired,
			    vars->page_list_pal,
			    true /* preflight */,
			    vmflush /* discard */,
			    &pageCount);
			PE_Video consoleInfo;
			bzero(&consoleInfo, sizeof(consoleInfo));
			IOService::getPlatform()->getConsoleInfo(&consoleInfo);

			// estimate: 6% increase in pages compressed
			// screen preview 2 images compressed 0%
			setFileSize = ((ptoa_64((106 * pageCount) / 100) * gIOHibernateCompression) >> 8)
			    + vars->page_list->list_size
			    + (consoleInfo.v_width * consoleInfo.v_height * 8);
			enum { setFileRound = 1024 * 1024ULL };
			setFileSize = ((setFileSize + setFileRound) & ~(setFileRound - 1));

			HIBLOG("hibernate_page_list_setall preflight pageCount %d est comp %qd setfile %qd min %qd\n",
			    pageCount, (100ULL * gIOHibernateCompression) >> 8,
			    setFileSize, vars->fileMinSize);

			if (!(kIOHibernateModeFileResize & gIOHibernateMode)
			    && (setFileSize < vars->fileMinSize)) {
				setFileSize = vars->fileMinSize;
			}
		}

		vars->volumeCryptKeySize = sizeof(vars->volumeCryptKey);
		err = IOPolledFileOpen(gIOHibernateFilename,
		    (kIOPolledFileCreate | kIOPolledFileHibernate),
		    setFileSize, 0,
		    gIOHibernateCurrentHeader, sizeof(gIOHibernateCurrentHeader),
		    &vars->fileVars, &nvramData,
		    &vars->volumeCryptKey[0], &vars->volumeCryptKeySize);

		if (KERN_SUCCESS != err) {
			IOLockLock(gFSLock);
			if (kFSOpening != gFSState) {
				err = kIOReturnTimeout;
			}
			IOLockUnlock(gFSLock);
		}

		if (KERN_SUCCESS != err) {
			HIBLOG("IOPolledFileOpen(%x)\n", err);
			OSSafeReleaseNULL(nvramData);
			break;
		}

		// write extents for debug data usage in EFI
		IOWriteExtentsToFile(vars->fileVars, kIOHibernateHeaderOpenSignature);

		err = IOPolledFilePollersSetup(vars->fileVars, kIOPolledPreflightState);
		if (KERN_SUCCESS != err) {
			OSSafeReleaseNULL(nvramData);
			break;
		}

		clock_get_uptime(&startTime);
		err = hibernate_setup(gIOHibernateCurrentHeader,
		    vmflush,
		    vars->page_list, vars->page_list_wired, vars->page_list_pal);
		clock_get_uptime(&endTime);
		SUB_ABSOLUTETIME(&endTime, &startTime);
		absolutetime_to_nanoseconds(endTime, &nsec);

		boolean_t haveSwapPin, hibFileSSD;
		haveSwapPin = vm_swap_files_pinned();

		hibFileSSD = (kIOPolledFileSSD & vars->fileVars->flags);

		HIBLOG("hibernate_setup(%d) took %qd ms, swapPin(%d) ssd(%d)\n",
		    err, nsec / 1000000ULL,
		    haveSwapPin, hibFileSSD);
		if (KERN_SUCCESS != err) {
			OSSafeReleaseNULL(nvramData);
			break;
		}

		gIOHibernateStandbyDisabled = ((!haveSwapPin || !hibFileSSD));

		dsSSD = ((0 != (kIOPolledFileSSD & vars->fileVars->flags))
		    && (kOSBooleanTrue == IOService::getPMRootDomain()->getProperty(kIOPMDeepSleepEnabledKey)));

		if (dsSSD) {
			gIOHibernateCurrentHeader->options |= kIOHibernateOptionSSD | kIOHibernateOptionColor;
		} else {
			gIOHibernateCurrentHeader->options |= kIOHibernateOptionProgress;
		}


#if defined(__i386__) || defined(__x86_64__)
		if (vars->volumeCryptKeySize &&
		    (kOSBooleanTrue != IOService::getPMRootDomain()->getProperty(kIOPMDestroyFVKeyOnStandbyKey))) {
			OSData * smcData;
			smcData = OSData::withBytesNoCopy(&gIOHibernateVars.volumeCryptKey[0], (unsigned int)vars->volumeCryptKeySize);
			if (smcData) {
				smcData->setSerializable(false);
				IOService::getPMRootDomain()->setProperty(kIOHibernateSMCVariablesKey, smcData);
				smcData->release();
			}
		}
#endif /* defined(__i386__) || defined(__x86_64__) */

		if (encryptedswap || vars->volumeCryptKeySize) {
			gIOHibernateMode ^= kIOHibernateModeEncrypt;
		}

		if (kIOHibernateOptionProgress & gIOHibernateCurrentHeader->options) {
			vars->videoAllocSize = kVideoMapSize;
			if (KERN_SUCCESS != kmem_alloc(kernel_map, &vars->videoMapping, vars->videoAllocSize,
			    (kma_flags_t)(KMA_PAGEABLE | KMA_DATA), VM_KERN_MEMORY_IOKIT)) {
				vars->videoMapping = 0;
			}
		}

		// generate crypt keys
		for (uint32_t i = 0; i < sizeof(vars->wiredCryptKey); i++) {
			vars->wiredCryptKey[i] = ((uint8_t) random());
		}
		for (uint32_t i = 0; i < sizeof(vars->cryptKey); i++) {
			vars->cryptKey[i] = ((uint8_t) random());
		}

		// set nvram

		IOSetBootImageNVRAM(nvramData);
		OSSafeReleaseNULL(nvramData);

#if defined(__i386__) || defined(__x86_64__)
		{
			struct AppleRTCHibernateVars {
				uint8_t     signature[4];
				uint32_t    revision;
				uint8_t     booterSignature[20];
				uint8_t     wiredCryptKey[16];
			};
			AppleRTCHibernateVars rtcVars;
			OSData * data;

			rtcVars.signature[0] = 'A';
			rtcVars.signature[1] = 'A';
			rtcVars.signature[2] = 'P';
			rtcVars.signature[3] = 'L';
			rtcVars.revision     = 1;
			bcopy(&vars->wiredCryptKey[0], &rtcVars.wiredCryptKey[0], sizeof(rtcVars.wiredCryptKey));

			if (gIOChosenEntry
			    && (data = OSDynamicCast(OSData, gIOChosenEntry->getProperty(gIOHibernateBootSignatureKey)))
			    && (sizeof(rtcVars.booterSignature) <= data->getLength())) {
				bcopy(data->getBytesNoCopy(), &rtcVars.booterSignature[0], sizeof(rtcVars.booterSignature));
			} else if (gIOHibernateBootSignature[0]) {
				char c;
				uint8_t value = 0;
				uint32_t in, out, digits;
				for (in = out = digits = 0;
				    (c = gIOHibernateBootSignature[in]) && (in < sizeof(gIOHibernateBootSignature));
				    in++) {
					if ((c >= 'a') && (c <= 'f')) {
						c -= 'a' - 10;
					} else if ((c >= 'A') && (c <= 'F')) {
						c -= 'A' - 10;
					} else if ((c >= '0') && (c <= '9')) {
						c -= '0';
					} else {
						if (c == '=') {
							out = digits = value = 0;
						}
						continue;
					}
					value = ((uint8_t) ((value << 4) | c));
					if (digits & 1) {
						rtcVars.booterSignature[out++] = value;
						if (out >= sizeof(rtcVars.booterSignature)) {
							break;
						}
					}
					digits++;
				}
			}
#if DEBUG || DEVELOPMENT
			if (kIOLogHibernate & gIOKitDebug) {
				IOKitKernelLogBuffer("H> rtc:",
				    &rtcVars, sizeof(rtcVars), &kprintf);
			}
#endif /* DEBUG || DEVELOPMENT */

			data = OSData::withValue(rtcVars);
			if (data) {
				if (gIOHibernateRTCVariablesKey) {
					IOService::getPMRootDomain()->setProperty(gIOHibernateRTCVariablesKey, data);
				}
				data->release();
			}
			if (gIOChosenEntry && gIOOptionsEntry) {
				data = OSDynamicCast(OSData, gIOChosenEntry->getProperty(kIOHibernateMachineSignatureKey));
				if (data) {
					gIOHibernateCurrentHeader->machineSignature = *((UInt32 *)data->getBytesNoCopy());
				}
				// set BootNext
				if (!gIOHibernateBoot0082Data) {
					OSData * fileData = NULL;
					data = OSDynamicCast(OSData, gIOChosenEntry->getProperty("boot-device-path"));
					if (data && data->getLength() >= 4) {
						fileData = OSDynamicCast(OSData, gIOChosenEntry->getProperty("boot-file-path"));
					}
					if (data && (data->getLength() <= UINT16_MAX)) {
						// AppleNVRAM_EFI_LOAD_OPTION
						struct {
							uint32_t Attributes;
							uint16_t FilePathLength;
							uint16_t Desc;
						} loadOptionHeader;
						loadOptionHeader.Attributes     = 1;
						loadOptionHeader.FilePathLength = ((uint16_t) data->getLength());
						loadOptionHeader.Desc           = 0;
						if (fileData) {
							loadOptionHeader.FilePathLength -= 4;
							loadOptionHeader.FilePathLength += fileData->getLength();
						}
						gIOHibernateBoot0082Data = OSData::withCapacity(sizeof(loadOptionHeader) + loadOptionHeader.FilePathLength);
						if (gIOHibernateBoot0082Data) {
							gIOHibernateBoot0082Data->appendValue(loadOptionHeader);
							if (fileData) {
								gIOHibernateBoot0082Data->appendBytes(data->getBytesNoCopy(), data->getLength() - 4);
								gIOHibernateBoot0082Data->appendBytes(fileData);
							} else {
								gIOHibernateBoot0082Data->appendBytes(data);
							}
						}
					}
				}
				if (!gIOHibernateBootNextData) {
					uint16_t bits = 0x0082;
					gIOHibernateBootNextData = OSData::withValue(bits);
				}

#if DEBUG || DEVELOPMENT
				if (kIOLogHibernate & gIOKitDebug) {
					IOKitKernelLogBuffer("H> bootnext:",
					    gIOHibernateBoot0082Data->getBytesNoCopy(), gIOHibernateBoot0082Data->getLength(), &kprintf);
				}
#endif /* DEBUG || DEVELOPMENT */
				if (gIOHibernateBoot0082Key && gIOHibernateBoot0082Data && gIOHibernateBootNextKey && gIOHibernateBootNextData) {
					gIOHibernateBootNextSave = gIOOptionsEntry->copyProperty(gIOHibernateBootNextKey);
					gIOOptionsEntry->setProperty(gIOHibernateBoot0082Key, gIOHibernateBoot0082Data);
					gIOOptionsEntry->setProperty(gIOHibernateBootNextKey, gIOHibernateBootNextData);
				}
				// BootNext
			}
		}
#endif /* !i386 && !x86_64 */
	}while (false);

	if (swapPinned) {
		hibernate_pin_swap(FALSE);
	}

	IOLockLock(gFSLock);
	if ((kIOReturnSuccess == err) && (kFSOpening != gFSState)) {
		HIBLOG("hibernate file close due timeout\n");
		err = kIOReturnTimeout;
	}
	if (kIOReturnSuccess == err) {
		gFSState = kFSOpened;
		gIOHibernateVars = *vars;
		gFileVars = *vars->fileVars;
		gFileVars.allocated = false;
		gIOHibernateVars.fileVars = &gFileVars;
		gIOHibernateCurrentHeader->signature = kIOHibernateHeaderSignature;
		gIOHibernateCurrentHeader->kernVirtSlide = vm_kernel_slide;
		gIOHibernateState = kIOHibernateStateHibernating;

#if DEBUG || DEVELOPMENT
#if defined(__i386__) || defined(__x86_64__)
		if (kIOLogHibernate & gIOKitDebug) {
			OSData * data = OSDynamicCast(OSData, IOService::getPMRootDomain()->getProperty(kIOHibernateSMCVariablesKey));
			if (data) {
				IOKitKernelLogBuffer("H> smc:",
				    data->getBytesNoCopy(), data->getLength(), &kprintf);
			}
		}
#endif /* defined(__i386__) || defined(__x86_64__) */
#endif /* DEBUG || DEVELOPMENT */
	} else {
		IOPolledFileIOVars * fileVars = vars->fileVars;
		IOHibernateDone(vars);
		IOPolledFileClose(&fileVars,
#if DISABLE_TRIM
		    0, NULL, 0, 0, 0, false);
#else
		    0, NULL, 0, sizeof(IOHibernateImageHeader), setFileSize, false);
#endif
		gFSState = kFSIdle;
	}
	IOLockUnlock(gFSLock);

	if (vars->fileVars) {
		IOFreeType(vars->fileVars, IOPolledFileIOVars);
	}
	IOFreeType(vars, IOHibernateVars);

	return err;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void
IOSetBootImageNVRAM(OSData * data)
{
	IORegistryEntry * regEntry;

	if (!gIOOptionsEntry) {
		regEntry = IORegistryEntry::fromPath("/options", gIODTPlane);
		gIOOptionsEntry = OSDynamicCast(IODTNVRAM, regEntry);
		if (regEntry && !gIOOptionsEntry) {
			regEntry->release();
		}
	}
	if (gIOOptionsEntry && gIOHibernateBootImageKey) {
		if (data) {
			gIOOptionsEntry->setProperty(gIOHibernateBootImageKey, data);
#if DEBUG || DEVELOPMENT
			if (kIOLogHibernate & gIOKitDebug) {
				IOKitKernelLogBuffer("H> boot-image:",
				    data->getBytesNoCopy(), data->getLength(), &kprintf);
			}
#endif /* DEBUG || DEVELOPMENT */
		} else {
			gIOOptionsEntry->removeProperty(gIOHibernateBootImageKey);
#if __x86_64__
			gIOOptionsEntry->sync();
#else
			if (gIOHibernateState == kIOHibernateStateWakingFromHibernate) {
				// if we woke from hibernation, the booter may have changed the state of NVRAM, so force a sync
				gIOOptionsEntry->sync();
			}
#endif
		}
	}
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*
 * Writes header to disk with signature, block size and file extents data.
 * If there are more than 2 extents, then they are written on second block.
 */
static IOReturn
IOWriteExtentsToFile(IOPolledFileIOVars * vars, uint32_t signature)
{
	IOHibernateImageHeader hdr;
	IOItemCount            count;
	IOReturn               err = kIOReturnSuccess;
	int                    rc;
	IOPolledFileExtent *   fileExtents;

	fileExtents = (typeof(fileExtents))vars->fileExtents->getBytesNoCopy();

	memset(&hdr, 0, sizeof(IOHibernateImageHeader));
	count = vars->fileExtents->getLength();
	if (count > sizeof(hdr.fileExtentMap)) {
		hdr.fileExtentMapSize = count;
		count = sizeof(hdr.fileExtentMap);
	} else {
		hdr.fileExtentMapSize = sizeof(hdr.fileExtentMap);
	}

	bcopy(fileExtents, &hdr.fileExtentMap[0], count);

	// copy file block extent list if larger than header
	if (hdr.fileExtentMapSize > sizeof(hdr.fileExtentMap)) {
		count = hdr.fileExtentMapSize - sizeof(hdr.fileExtentMap);
		rc = kern_write_file(vars->fileRef, vars->blockSize,
		    (caddr_t)(((uint8_t *)fileExtents) + sizeof(hdr.fileExtentMap)),
		    count, IO_SKIP_ENCRYPTION);
		if (rc != 0) {
			HIBLOG("kern_write_file returned %d\n", rc);
			err = kIOReturnIOError;
			goto exit;
		}
	}
	hdr.signature = signature;
	hdr.deviceBlockSize = vars->blockSize;

	rc = kern_write_file(vars->fileRef, 0, (char *)&hdr, sizeof(hdr), IO_SKIP_ENCRYPTION);
	if (rc != 0) {
		HIBLOG("kern_write_file returned %d\n", rc);
		err = kIOReturnIOError;
		goto exit;
	}

exit:
	return err;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

DECLARE_IOHIBERNATEPROGRESSALPHA

static void
ProgressInit(hibernate_graphics_t * display, uint8_t * screen, uint8_t * saveunder, uint32_t savelen)
{
	uint32_t    rowBytes, pixelShift;
	uint32_t    x, y;
	int32_t     blob;
	uint32_t    alpha, color, result;
	uint8_t *   out, in;
	uint32_t    saveindex[kIOHibernateProgressCount] = { 0 };

	rowBytes = display->rowBytes;
	pixelShift = display->depth >> 4;
	if (pixelShift < 1) {
		return;
	}

	screen += ((display->width
	    - kIOHibernateProgressCount * (kIOHibernateProgressWidth + kIOHibernateProgressSpacing)) << (pixelShift - 1))
	    + (display->height - kIOHibernateProgressOriginY - kIOHibernateProgressHeight) * rowBytes;

	for (y = 0; y < kIOHibernateProgressHeight; y++) {
		out = screen + y * rowBytes;
		for (blob = 0; blob < kIOHibernateProgressCount; blob++) {
			color = blob ? kIOHibernateProgressDarkGray : kIOHibernateProgressMidGray;
			for (x = 0; x < kIOHibernateProgressWidth; x++) {
				alpha  = gIOHibernateProgressAlpha[y][x];
				result = color;
				if (alpha) {
					if (0xff != alpha) {
						if (1 == pixelShift) {
							in = *((uint16_t *)out) & 0x1f; // 16
							in = ((uint8_t)(in << 3)) | ((uint8_t)(in >> 2));
						} else {
							in = *((uint32_t *)out) & 0xff; // 32
						}
						saveunder[blob * kIOHibernateProgressSaveUnderSize + saveindex[blob]++] = in;
						result = ((255 - alpha) * in + alpha * result + 0xff) >> 8;
					}
					if (1 == pixelShift) {
						result >>= 3;
						*((uint16_t *)out) = ((uint16_t)((result << 10) | (result << 5) | result)); // 16
					} else {
						*((uint32_t *)out) = (result << 16) | (result << 8) | result; // 32
					}
				}
				out += (1 << pixelShift);
			}
			out += (kIOHibernateProgressSpacing << pixelShift);
		}
	}
}


static void
ProgressUpdate(hibernate_graphics_t * display, uint8_t * screen, int32_t firstBlob, int32_t select)
{
	uint32_t  rowBytes, pixelShift;
	uint32_t  x, y;
	int32_t   blob, lastBlob;
	uint32_t  alpha, in, color, result;
	uint8_t * out;
	uint32_t  saveindex[kIOHibernateProgressCount] = { 0 };

	pixelShift = display->depth >> 4;
	if (pixelShift < 1) {
		return;
	}

	rowBytes = display->rowBytes;

	screen += ((display->width
	    - kIOHibernateProgressCount * (kIOHibernateProgressWidth + kIOHibernateProgressSpacing)) << (pixelShift - 1))
	    + (display->height - kIOHibernateProgressOriginY - kIOHibernateProgressHeight) * rowBytes;

	lastBlob  = (select < kIOHibernateProgressCount) ? select : (kIOHibernateProgressCount - 1);

	screen += (firstBlob * (kIOHibernateProgressWidth + kIOHibernateProgressSpacing)) << pixelShift;

	for (y = 0; y < kIOHibernateProgressHeight; y++) {
		out = screen + y * rowBytes;
		for (blob = firstBlob; blob <= lastBlob; blob++) {
			color = (blob < select) ? kIOHibernateProgressLightGray : kIOHibernateProgressMidGray;
			for (x = 0; x < kIOHibernateProgressWidth; x++) {
				alpha  = gIOHibernateProgressAlpha[y][x];
				result = color;
				if (alpha) {
					if (0xff != alpha) {
						in = display->progressSaveUnder[blob][saveindex[blob]++];
						result = ((255 - alpha) * in + alpha * result + 0xff) / 255;
					}
					if (1 == pixelShift) {
						result >>= 3;
						*((uint16_t *)out) = ((uint16_t)((result << 10) | (result << 5) | result)); // 16
					} else {
						*((uint32_t *)out) = (result << 16) | (result << 8) | result; // 32
					}
				}
				out += (1 << pixelShift);
			}
			out += (kIOHibernateProgressSpacing << pixelShift);
		}
	}
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn
IOHibernateIOKitSleep(void)
{
	IOReturn ret = kIOReturnSuccess;
	IOLockLock(gFSLock);
	if (kFSOpening == gFSState) {
		gFSState = kFSTimedOut;
		HIBLOG("hibernate file open timed out\n");
		ret = kIOReturnTimeout;
	}
	IOLockUnlock(gFSLock);
	return ret;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn
IOHibernateSystemHasSlept(void)
{
	IOReturn          ret = kIOReturnSuccess;
	IOHibernateVars * vars  = &gIOHibernateVars;
	OSObject        * obj = NULL;
	OSData          * data;

	IOLockLock(gFSLock);
	if ((kFSOpened != gFSState) && gIOHibernateMode) {
		ret = kIOReturnTimeout;
	}
	IOLockUnlock(gFSLock);
	if (kIOReturnSuccess != ret) {
		return ret;
	}

	if (gIOHibernateMode) {
		obj = IOService::getPMRootDomain()->copyProperty(kIOHibernatePreviewBufferKey);
	}
	vars->previewBuffer = OSDynamicCast(IOMemoryDescriptor, obj);
	if (obj && !vars->previewBuffer) {
		obj->release();
	}
	if (vars->previewBuffer && (vars->previewBuffer->getLength() > UINT_MAX)) {
		OSSafeReleaseNULL(vars->previewBuffer);
	}

	vars->consoleMapping = NULL;
	if (vars->previewBuffer && (kIOReturnSuccess != vars->previewBuffer->prepare())) {
		vars->previewBuffer->release();
		vars->previewBuffer = NULL;
	}

	if ((kIOHibernateOptionProgress & gIOHibernateCurrentHeader->options)
	    && vars->previewBuffer
	    && (data = OSDynamicCast(OSData,
	    IOService::getPMRootDomain()->getProperty(kIOHibernatePreviewActiveKey)))) {
		UInt32 flags = *((UInt32 *)data->getBytesNoCopy());
		HIBPRINT("kIOHibernatePreviewActiveKey %08lx\n", (long)flags);

		IOService::getPMRootDomain()->removeProperty(kIOHibernatePreviewActiveKey);

		if (kIOHibernatePreviewUpdates & flags) {
			PE_Video           consoleInfo;
			hibernate_graphics_t * graphicsInfo = gIOHibernateGraphicsInfo;

			IOService::getPlatform()->getConsoleInfo(&consoleInfo);

			graphicsInfo->width    = (uint32_t)  consoleInfo.v_width;
			graphicsInfo->height   = (uint32_t)  consoleInfo.v_height;
			graphicsInfo->rowBytes = (uint32_t)  consoleInfo.v_rowBytes;
			graphicsInfo->depth    = (uint32_t)  consoleInfo.v_depth;
			vars->consoleMapping   = (uint8_t *) consoleInfo.v_baseAddr;

			HIBPRINT("video %p %d %d %d\n",
			    vars->consoleMapping, graphicsInfo->depth,
			    graphicsInfo->width, graphicsInfo->height);
			if (vars->consoleMapping) {
				ProgressInit(graphicsInfo, vars->consoleMapping,
				    &graphicsInfo->progressSaveUnder[0][0], sizeof(graphicsInfo->progressSaveUnder));
			}
		}
	}

	if (gIOOptionsEntry) {
#if __x86_64__
		gIOOptionsEntry->sync();
#else
		if (gIOHibernateMode) {
			gIOOptionsEntry->sync();
		}
#endif
	}

	return ret;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static const DeviceTreeNode *
MergeDeviceTree(const DeviceTreeNode * entry, IORegistryEntry * regEntry, OSSet * entriesToUpdate, vm_offset_t region_start, vm_size_t region_size)
{
	DeviceTreeNodeProperty * prop;
	const DeviceTreeNode *   child;
	IORegistryEntry *        childRegEntry;
	const char *             nameProp;
	unsigned int             propLen, idx;

	bool updateEntry = true;
	if (!regEntry) {
		updateEntry = false;
	} else if (entriesToUpdate && !entriesToUpdate->containsObject(regEntry)) {
		updateEntry = false;
	}

	prop = (DeviceTreeNodeProperty *) (entry + 1);
	for (idx = 0; idx < entry->nProperties; idx++) {
		if (updateEntry && (0 != strcmp("name", prop->name))) {
			regEntry->setProperty((const char *) prop->name, (void *) (prop + 1), prop->length);
//	    HIBPRINT("%s: %s, %d\n", regEntry->getName(), prop->name, prop->length);
		}
		prop = (DeviceTreeNodeProperty *) (((uintptr_t)(prop + 1)) + ((prop->length + 3) & ~3));
	}

	if (entriesToUpdate) {
		entriesToUpdate->removeObject(regEntry);
		if (entriesToUpdate->getCount() == 0) {
			// we've updated all the entries we care about so we can stop
			return NULL;
		}
	}

	child = (const DeviceTreeNode *) prop;
	for (idx = 0; idx < entry->nChildren; idx++) {
		if (kSuccess != SecureDTGetPropertyRegion(child, "name", (void const **) &nameProp, &propLen,
		    region_start, region_size)) {
			panic("no name");
		}
		childRegEntry = regEntry ? regEntry->childFromPath(nameProp, gIODTPlane) : NULL;
//	HIBPRINT("%s == %p\n", nameProp, childRegEntry);
		child = MergeDeviceTree(child, childRegEntry, entriesToUpdate, region_start, region_size);
		OSSafeReleaseNULL(childRegEntry);
		if (!child) {
			// the recursive call updated the last entry we cared about, so we can stop
			break;
		}
	}
	return child;
}

IOReturn
IOHibernateSystemWake(void)
{
	if (kFSOpened == gFSState) {
		IOPolledFilePollersClose(gIOHibernateVars.fileVars, kIOPolledPostflightState);
		IOHibernateDone(&gIOHibernateVars);
	} else {
		IOService::getPMRootDomain()->removeProperty(kIOHibernateOptionsKey);
		IOService::getPMRootDomain()->removeProperty(kIOHibernateGfxStatusKey);
	}

	if (gIOOptionsEntry && gIOHibernateBootImageKey) {
		// if we got this far, clear boot-image
		// we don't need to sync immediately; the booter should have already removed this entry
		// we just want to make sure that if anyone syncs nvram after this point, we don't re-write
		// a stale boot-image value
		gIOOptionsEntry->removeProperty(gIOHibernateBootImageKey);
	}

	return kIOReturnSuccess;
}

static IOReturn
IOHibernateDone(IOHibernateVars * vars)
{
	IOReturn err;
	OSData * data;

	hibernate_teardown(vars->page_list, vars->page_list_wired, vars->page_list_pal);

	if (vars->videoMapping) {
		if (vars->videoMapSize) {
			// remove mappings
			IOUnmapPages(kernel_map, vars->videoMapping, vars->videoMapSize);
		}
		if (vars->videoAllocSize) {
			// dealloc range
			kmem_free(kernel_map, trunc_page(vars->videoMapping), vars->videoAllocSize);
		}
	}

	if (vars->previewBuffer) {
		vars->previewBuffer->release();
		vars->previewBuffer = NULL;
	}

	if (kIOHibernateStateWakingFromHibernate == gIOHibernateState) {
		IOService::getPMRootDomain()->setProperty(kIOHibernateOptionsKey,
		    gIOHibernateCurrentHeader->options, 32);
	} else {
		IOService::getPMRootDomain()->removeProperty(kIOHibernateOptionsKey);
	}

	if ((kIOHibernateStateWakingFromHibernate == gIOHibernateState)
	    && (kIOHibernateGfxStatusUnknown != gIOHibernateGraphicsInfo->gfxStatus)) {
		IOService::getPMRootDomain()->setProperty(kIOHibernateGfxStatusKey,
		    &gIOHibernateGraphicsInfo->gfxStatus,
		    sizeof(gIOHibernateGraphicsInfo->gfxStatus));
	} else {
		IOService::getPMRootDomain()->removeProperty(kIOHibernateGfxStatusKey);
	}

	// invalidate nvram properties - (gIOOptionsEntry != 0) => nvram was touched

#if defined(__i386__) || defined(__x86_64__)
	IOService::getPMRootDomain()->removeProperty(gIOHibernateRTCVariablesKey);
	IOService::getPMRootDomain()->removeProperty(kIOHibernateSMCVariablesKey);

	/*
	 * Hibernate variable is written to NVRAM on platforms in which RtcRam
	 * is not backed by coin cell.  Remove Hibernate data from NVRAM.
	 */
	if (gIOOptionsEntry) {
		if (gIOHibernateRTCVariablesKey) {
			if (gIOOptionsEntry->getProperty(gIOHibernateRTCVariablesKey)) {
				gIOOptionsEntry->removeProperty(gIOHibernateRTCVariablesKey);
			}
		}

		if (gIOHibernateBootNextKey) {
			if (gIOHibernateBootNextSave) {
				gIOOptionsEntry->setProperty(gIOHibernateBootNextKey, gIOHibernateBootNextSave);
				gIOHibernateBootNextSave->release();
				gIOHibernateBootNextSave = NULL;
			} else {
				gIOOptionsEntry->removeProperty(gIOHibernateBootNextKey);
			}
		}
		if (kIOHibernateStateWakingFromHibernate != gIOHibernateState) {
			gIOOptionsEntry->sync();
		}
	}
#endif

	if (vars->srcBuffer) {
		vars->srcBuffer->release();
	}


	bzero(&gIOHibernateHandoffPages[0], gIOHibernateHandoffPageCount * sizeof(gIOHibernateHandoffPages[0]));
	if (vars->handoffBuffer) {
		if (kIOHibernateStateWakingFromHibernate == gIOHibernateState) {
			IOHibernateHandoff * handoff;
			bool done = false;
			for (handoff = (IOHibernateHandoff *) vars->handoffBuffer->getBytesNoCopy();
			    !done;
			    handoff = (IOHibernateHandoff *) &handoff->data[handoff->bytecount]) {
				HIBPRINT("handoff %p, %x, %x\n", handoff, handoff->type, handoff->bytecount);
				uint8_t * __unused data = &handoff->data[0];
				switch (handoff->type) {
				case kIOHibernateHandoffTypeEnd:
					done = true;
					break;

				case kIOHibernateHandoffTypeDeviceTree:
				{
#if defined(__i386__) || defined(__x86_64__)
					// On Intel, process the entirety of the passed in device tree
					OSSet * entriesToUpdate = NULL;
#elif defined(__arm64__)
					// On ARM, only allow hibernation to update specific entries
					const char *mergePaths[] = {
						kIODeviceTreePlane ":/chosen/boot-object-manifests",
						kIODeviceTreePlane ":/chosen/secure-boot-hashes",
					};
					const size_t mergePathCount = sizeof(mergePaths) / sizeof(mergePaths[0]);
					OSSet * entriesToUpdate = OSSet::withCapacity(mergePathCount);
					for (size_t i = 0; i < mergePathCount; i++) {
						IORegistryEntry *entry = IORegistryEntry::fromPath(mergePaths[i]);
						if (!entry) {
							panic("failed to find %s in IORegistry", mergePaths[i]);
						}
						entriesToUpdate->setObject(entry);
						OSSafeReleaseNULL(entry);
					}
#endif
					MergeDeviceTree((DeviceTreeNode *) data, IOService::getServiceRoot(), entriesToUpdate,
					    (vm_offset_t)data, (vm_size_t)handoff->bytecount);
					OSSafeReleaseNULL(entriesToUpdate);
					break;
				}

				case kIOHibernateHandoffTypeKeyStore:
#if defined(__i386__) || defined(__x86_64__)
					{
						IOBufferMemoryDescriptor *
						    md = IOBufferMemoryDescriptor::withBytes(data, handoff->bytecount, kIODirectionOutIn);
						if (md) {
							IOSetKeyStoreData(md);
						}
					}
#endif
					break;

				default:
					done = (kIOHibernateHandoffType != (handoff->type & 0xFFFF0000));
					break;
				}
			}
#if defined(__i386__) || defined(__x86_64__)
			if (vars->volumeCryptKeySize) {
				IOBufferMemoryDescriptor *
				    bmd = IOBufferMemoryDescriptor::withBytes(&vars->volumeCryptKey[0],
				    vars->volumeCryptKeySize, kIODirectionOutIn);
				if (!bmd) {
					panic("IOBufferMemoryDescriptor");
				}
				IOSetAPFSKeyStoreData(bmd);
				bzero(&vars->volumeCryptKey[0], sizeof(vars->volumeCryptKey));
			}
#endif
		}
		vars->handoffBuffer->release();
	}

	if (gIOChosenEntry
	    && (data = OSDynamicCast(OSData, gIOChosenEntry->getProperty(gIOBridgeBootSessionUUIDKey)))
	    && (sizeof(gIOHibernateBridgeBootSessionUUIDString) <= data->getLength())) {
		bcopy(data->getBytesNoCopy(), &gIOHibernateBridgeBootSessionUUIDString[0],
		    sizeof(gIOHibernateBridgeBootSessionUUIDString));
	}

	if (vars->hwEncrypt) {
		err = IOPolledFilePollersSetEncryptionKey(vars->fileVars, NULL, 0);
		HIBLOG("IOPolledFilePollersSetEncryptionKey(0,%x)\n", err);
	}

	bzero(vars, sizeof(*vars));

//    gIOHibernateState = kIOHibernateStateInactive;       // leave it for post wake code to see
	gIOHibernateCount++;

	return kIOReturnSuccess;
}

static void
IOHibernateSystemPostWakeTrim(void * p1, void * p2)
{
	// invalidate & close the image file
	if (p1) {
		IOLockLock(gFSLock);
	}
	if (kFSTrimDelay == gFSState) {
		IOPolledFileIOVars * vars = &gFileVars;
		IOPolledFileClose(&vars,
#if DISABLE_TRIM
		    0, NULL, 0, 0, 0, false);
#else
		    0, (caddr_t)gIOHibernateCurrentHeader, sizeof(IOHibernateImageHeader),
		    sizeof(IOHibernateImageHeader), gIOHibernateCurrentHeader->imageSize, false);
#endif
		gFSState = kFSIdle;
	}
	if (p1) {
		IOLockUnlock(gFSLock);
	}
}

IOReturn
IOHibernateSystemPostWake(bool now)
{
	gIOHibernateCurrentHeader->signature = kIOHibernateHeaderInvalidSignature;
	IOSetBootImageNVRAM(NULL);

	IOLockLock(gFSLock);
	if (kFSTrimDelay == gFSState) {
		thread_call_cancel(gIOHibernateTrimCalloutEntry);
		IOHibernateSystemPostWakeTrim(NULL, NULL);
	} else if (kFSOpened != gFSState) {
		gFSState = kFSIdle;
	} else {
		gFSState = kFSTrimDelay;
		if (now) {
			thread_call_cancel(gIOHibernateTrimCalloutEntry);
			IOHibernateSystemPostWakeTrim(NULL, NULL);
		} else {
			AbsoluteTime deadline;
			clock_interval_to_deadline(TRIM_DELAY, kMillisecondScale, &deadline );
			thread_call_enter1_delayed(gIOHibernateTrimCalloutEntry, NULL, deadline);
		}
	}
	IOLockUnlock(gFSLock);

	return kIOReturnSuccess;
}

uint32_t
IOHibernateWasScreenLocked(void)
{
	uint32_t ret = 0;
	if (gIOChosenEntry) {
		if (kIOHibernateStateWakingFromHibernate == gIOHibernateState) {
			OSData *
			    data = OSDynamicCast(OSData, gIOChosenEntry->getProperty(kIOScreenLockStateKey));
			if (data) {
				ret = ((uint32_t *)data->getBytesNoCopy())[0];
				gIOChosenEntry->setProperty(kIOBooterScreenLockStateKey, data);
			}
		} else {
			gIOChosenEntry->removeProperty(kIOBooterScreenLockStateKey);
		}
	}

	return ret;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

SYSCTL_STRING(_kern, OID_AUTO, hibernatefile,
    CTLFLAG_RW | CTLFLAG_KERN | CTLFLAG_LOCKED,
    gIOHibernateFilename, sizeof(gIOHibernateFilename), "");
SYSCTL_STRING(_kern, OID_AUTO, bootsignature,
    CTLFLAG_RW | CTLFLAG_KERN | CTLFLAG_LOCKED,
    gIOHibernateBootSignature, sizeof(gIOHibernateBootSignature), "");
SYSCTL_UINT(_kern, OID_AUTO, hibernatemode,
    CTLFLAG_RW | CTLFLAG_KERN | CTLFLAG_LOCKED,
    &gIOHibernateMode, 0, "");
SYSCTL_STRUCT(_kern, OID_AUTO, hibernatestatistics,
    CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_KERN | CTLFLAG_LOCKED,
    &_hibernateStats, hibernate_statistics_t, "");
SYSCTL_OID_MANUAL(_kern_bridge, OID_AUTO, bootsessionuuid,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NOAUTO | CTLFLAG_KERN | CTLFLAG_LOCKED,
    gIOHibernateBridgeBootSessionUUIDString, sizeof(gIOHibernateBridgeBootSessionUUIDString),
    sysctl_handle_string, "A", "");

SYSCTL_UINT(_kern, OID_AUTO, hibernategraphicsready,
    CTLFLAG_RW | CTLFLAG_KERN | CTLFLAG_ANYBODY,
    &_hibernateStats.graphicsReadyTime, 0, "");
SYSCTL_UINT(_kern, OID_AUTO, hibernatewakenotification,
    CTLFLAG_RW | CTLFLAG_KERN | CTLFLAG_ANYBODY,
    &_hibernateStats.wakeNotificationTime, 0, "");
SYSCTL_UINT(_kern, OID_AUTO, hibernatelockscreenready,
    CTLFLAG_RW | CTLFLAG_KERN | CTLFLAG_ANYBODY,
    &_hibernateStats.lockScreenReadyTime, 0, "");
SYSCTL_UINT(_kern, OID_AUTO, hibernatehidready,
    CTLFLAG_RW | CTLFLAG_KERN | CTLFLAG_ANYBODY,
    &_hibernateStats.hidReadyTime, 0, "");

SYSCTL_UINT(_kern, OID_AUTO, hibernatecount,
    CTLFLAG_RD | CTLFLAG_KERN | CTLFLAG_ANYBODY,
    &gIOHibernateCount, 0, "");

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static int
hibernate_set_preview SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)

	if (!IOCurrentTaskHasEntitlement(kIOHibernateSetPreviewEntitlementKey)) {
		return EPERM;
	}

	if ((req->newptr == USER_ADDR_NULL) || (!req->newlen)) {
		IOService::getPMRootDomain()->removeProperty(kIOHibernatePreviewBufferKey);
		return 0;
	}
	size_t rounded_size;
	if (round_page_overflow(req->newlen, &rounded_size)) {
		return ENOMEM;
	}
	IOBufferMemoryDescriptor *md = IOBufferMemoryDescriptor::withOptions(kIODirectionOutIn, rounded_size, page_size);
	if (!md) {
		return ENOMEM;
	}

	uint8_t *bytes = (uint8_t *)md->getBytesNoCopy();
	int error = SYSCTL_IN(req, bytes, req->newlen);
	if (error) {
		md->release();
		return error;
	}

	IOService::getPMRootDomain()->setProperty(kIOHibernatePreviewBufferKey, md);
	md->release();

	return 0;
}

SYSCTL_PROC(_kern, OID_AUTO, hibernatepreview,
    CTLTYPE_OPAQUE | CTLFLAG_WR | CTLFLAG_LOCKED | CTLFLAG_ANYBODY, NULL, 0,
    hibernate_set_preview, "S", "");

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
IOHibernateSystemInit(IOPMrootDomain * rootDomain)
{
	gIOHibernateBootImageKey     = OSSymbol::withCStringNoCopy(kIOHibernateBootImageKey);
	gIOHibernateBootSignatureKey = OSSymbol::withCStringNoCopy(kIOHibernateBootSignatureKey);
	gIOBridgeBootSessionUUIDKey  = OSSymbol::withCStringNoCopy(kIOBridgeBootSessionUUIDKey);

#if defined(__i386__) || defined(__x86_64__)
	gIOHibernateRTCVariablesKey = OSSymbol::withCStringNoCopy(kIOHibernateRTCVariablesKey);
	gIOHibernateBoot0082Key     = OSSymbol::withCString("8BE4DF61-93CA-11D2-AA0D-00E098032B8C:Boot0082");
	gIOHibernateBootNextKey     = OSSymbol::withCString("8BE4DF61-93CA-11D2-AA0D-00E098032B8C:BootNext");
	gIOHibernateRTCVariablesKey = OSSymbol::withCStringNoCopy(kIOHibernateRTCVariablesKey);
#endif /* defined(__i386__) || defined(__x86_64__) */

	OSData * data = OSData::withValueNoCopy(gIOHibernateState);
	if (data) {
		rootDomain->setProperty(kIOHibernateStateKey, data);
		data->release();
	}

	if (PE_parse_boot_argn("hfile", gIOHibernateFilename, sizeof(gIOHibernateFilename))) {
		gIOHibernateMode = kIOHibernateModeOn;
	} else {
		gIOHibernateFilename[0] = 0;
	}

	gIOChosenEntry = IORegistryEntry::fromPath("/chosen", gIODTPlane);

	if (gIOChosenEntry
	    && (data = OSDynamicCast(OSData, gIOChosenEntry->getProperty(gIOBridgeBootSessionUUIDKey)))
	    && (sizeof(gIOHibernateBridgeBootSessionUUIDString) <= data->getLength())) {
		sysctl_register_oid(&sysctl__kern_bridge_bootsessionuuid);
		bcopy(data->getBytesNoCopy(), &gIOHibernateBridgeBootSessionUUIDString[0], sizeof(gIOHibernateBridgeBootSessionUUIDString));
	}

	gFSLock = IOLockAlloc();
	gIOHibernateCount = 0;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static IOReturn
IOHibernatePolledFileWrite(IOHibernateVars * vars,
    const uint8_t * bytes, IOByteCount size,
    IOPolledFileCryptVars * cryptvars)
{
	IOReturn err;


	err = IOPolledFileWrite(vars->fileVars, bytes, size, cryptvars);
	if ((kIOReturnSuccess == err) && hibernate_should_abort()) {
		err = kIOReturnAborted;
	}


	return err;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

extern "C" uint32_t
hibernate_write_image(void)
{
	IOHibernateImageHeader * header = gIOHibernateCurrentHeader;
	IOHibernateVars *        vars  = &gIOHibernateVars;
	IOPolledFileExtent *     fileExtents;

#if !defined(__arm64__)
	_static_assert_1_arg(sizeof(IOHibernateImageHeader) == 512);
#endif /* !defined(__arm64__) */

	uint32_t     pageCount, pagesDone;
	IOReturn     err;
	ppnum_t      ppnum, page;
	vm_offset_t  count;
	uint8_t *    src;
	uint8_t *    data;
	uint8_t *    compressed;
	uint8_t *    scratch;
	IOByteCount  pageCompressedSize;
	uint64_t     compressedSize, uncompressedSize;
	uint64_t     image1Size = 0;
	uint32_t     bitmap_size;
	bool         iterDone, pollerOpen, needEncrypt;
	int          wkresult;
	uint32_t     tag;
	uint32_t     pageType;
	uint32_t     pageAndCount[2];
	addr64_t     phys64;
	IOByteCount  segLen;
	uint32_t     restore1Sum = 0, sum = 0, sum1 = 0, sum2 = 0;
	uintptr_t    hibernateBase;
	uintptr_t    hibernateEnd;

	AbsoluteTime startTime, endTime;
	AbsoluteTime allTime, compTime;
	uint64_t     compBytes;
	uint64_t     nsec;
	uint64_t     lastProgressStamp = 0;
	uint64_t     progressStamp;
	uint32_t     blob, lastBlob = (uint32_t) -1L;

	uint32_t     wiredPagesEncrypted;
	uint32_t     dirtyPagesEncrypted;
	uint32_t     wiredPagesClear;
	uint32_t     svPageCount;
	uint32_t     zvPageCount;

	IOPolledFileCryptVars _cryptvars;
	IOPolledFileCryptVars * cryptvars = NULL;

	wiredPagesEncrypted = 0;
	dirtyPagesEncrypted = 0;
	wiredPagesClear     = 0;
	svPageCount         = 0;
	zvPageCount         = 0;

	if (!vars->fileVars
	    || !vars->fileVars->pollers
	    || !(kIOHibernateModeOn & gIOHibernateMode)) {
		return kIOHibernatePostWriteSleep;
	}


#if !defined(__arm64__)
	if (kIOHibernateModeSleep & gIOHibernateMode) {
		kdebug_enable = save_kdebug_enable;
	}
#endif /* !defined(__arm64__) */

	pal_hib_write_hook();

	KDBG(IOKDBG_CODE(DBG_HIBERNATE, 1) | DBG_FUNC_START);
	IOService::getPMRootDomain()->tracePoint(kIOPMTracePointHibernate);

#if CRYPTO
	// encryption data. "iv" is the "initial vector".
	if (kIOHibernateModeEncrypt & gIOHibernateMode) {
		static const unsigned char first_iv[AES_BLOCK_SIZE]
		        = {  0xa3, 0x63, 0x65, 0xa9, 0x0b, 0x71, 0x7b, 0x1c,
			     0xdf, 0x9e, 0x5f, 0x32, 0xd7, 0x61, 0x63, 0xda };

		cryptvars = &gIOHibernateCryptWakeContext;
		bzero(cryptvars, sizeof(IOPolledFileCryptVars));
		aes_encrypt_key(vars->cryptKey,
		    kIOHibernateAESKeySize,
		    &cryptvars->ctx.encrypt);
		aes_decrypt_key(vars->cryptKey,
		    kIOHibernateAESKeySize,
		    &cryptvars->ctx.decrypt);

		cryptvars = &_cryptvars;
		bzero(cryptvars, sizeof(IOPolledFileCryptVars));
		for (pageCount = 0; pageCount < sizeof(vars->wiredCryptKey); pageCount++) {
			vars->wiredCryptKey[pageCount] ^= vars->volumeCryptKey[pageCount];
		}
		aes_encrypt_key(vars->wiredCryptKey,
		    kIOHibernateAESKeySize,
		    &cryptvars->ctx.encrypt);

		bcopy(&first_iv[0], &cryptvars->aes_iv[0], AES_BLOCK_SIZE);
		bzero(&vars->wiredCryptKey[0], sizeof(vars->wiredCryptKey));
		bzero(&vars->cryptKey[0], sizeof(vars->cryptKey));
	}
#endif /* CRYPTO */

	hibernate_page_list_setall(vars->page_list,
	    vars->page_list_wired,
	    vars->page_list_pal,
	    false /* !preflight */,
	    /* discard_all */
	    ((0 == (kIOHibernateModeSleep & gIOHibernateMode))
	    && (0 != ((kIOHibernateModeDiscardCleanActive | kIOHibernateModeDiscardCleanInactive) & gIOHibernateMode))),
	    &pageCount);

	HIBLOG("hibernate_page_list_setall found pageCount %d\n", pageCount);

	fileExtents = (IOPolledFileExtent *) vars->fileVars->fileExtents->getBytesNoCopy();

#if 0
	count = vars->fileVars->fileExtents->getLength() / sizeof(IOPolledFileExtent);
	for (page = 0; page < count; page++) {
		HIBLOG("fileExtents[%d] %qx, %qx (%qx)\n", page,
		    fileExtents[page].start, fileExtents[page].length,
		    fileExtents[page].start + fileExtents[page].length);
	}
#endif

	needEncrypt = (0 != (kIOHibernateModeEncrypt & gIOHibernateMode));
	AbsoluteTime_to_scalar(&compTime) = 0;
	compBytes = 0;

	clock_get_uptime(&allTime);
	IOService::getPMRootDomain()->pmStatsRecordEvent(
		kIOPMStatsHibernateImageWrite | kIOPMStatsEventStartFlag, allTime);
	do{
		compressedSize   = 0;
		uncompressedSize = 0;
		svPageCount      = 0;
		zvPageCount      = 0;

		IOPolledFileSeek(vars->fileVars, vars->fileVars->blockSize);

		HIBLOG("IOHibernatePollerOpen, ml_get_interrupts_enabled %d\n",
		    ml_get_interrupts_enabled());
		err = IOPolledFilePollersOpen(vars->fileVars, kIOPolledBeforeSleepState,
		    // abortable if not low battery
		    !IOService::getPMRootDomain()->mustHibernate());
		HIBLOG("IOHibernatePollerOpen(%x)\n", err);
		pollerOpen = (kIOReturnSuccess == err);
		if (!pollerOpen) {
			break;
		}


		if (vars->volumeCryptKeySize) {
			err = IOPolledFilePollersSetEncryptionKey(vars->fileVars, &vars->volumeCryptKey[0], vars->volumeCryptKeySize);
			HIBLOG("IOPolledFilePollersSetEncryptionKey(%x)\n", err);
			vars->hwEncrypt = (kIOReturnSuccess == err);
			bzero(&vars->volumeCryptKey[0], sizeof(vars->volumeCryptKey));
			if (vars->hwEncrypt) {
				header->options |= kIOHibernateOptionHWEncrypt;
			}
		}

		// copy file block extent list if larger than header

		count = vars->fileVars->fileExtents->getLength();
		if (count > sizeof(header->fileExtentMap)) {
			count -= sizeof(header->fileExtentMap);
			err = IOHibernatePolledFileWrite(vars,
			    ((uint8_t *) &fileExtents[0]) + sizeof(header->fileExtentMap), count, cryptvars);
			if (kIOReturnSuccess != err) {
				break;
			}
		}

		// copy out restore1 code

		for (count = 0;
		    (phys64 = vars->handoffBuffer->getPhysicalSegment(count, &segLen, kIOMemoryMapperNone));
		    count += segLen) {
			for (pagesDone = 0; pagesDone < atop_32(segLen); pagesDone++) {
				gIOHibernateHandoffPages[atop_32(count) + pagesDone] = atop_64_ppnum(phys64) + pagesDone;
			}
		}

		hibernateBase = HIB_BASE; /* Defined in PAL headers */
		hibernateEnd = (segHIBB + segSizeHIB);

		page = atop_32(kvtophys(hibernateBase));
		count = atop_32(round_page(hibernateEnd) - hibernateBase);
		uintptr_t entrypoint = ((uintptr_t) &hibernate_machine_entrypoint)        - hibernateBase;
		uintptr_t stack      = ((uintptr_t) &gIOHibernateRestoreStackEnd[0]) - 64 - hibernateBase;
		if ((count > UINT_MAX) || (entrypoint > UINT_MAX) || (stack > UINT_MAX)) {
			panic("malformed kernel layout");
		}
		header->restore1CodePhysPage = (ppnum_t) page;
		header->restore1CodeVirt = hibernateBase;
		header->restore1PageCount = (uint32_t) count;
		header->restore1CodeOffset = (uint32_t) entrypoint;
		header->restore1StackOffset = (uint32_t) stack;

		if (uuid_parse(&gIOHibernateBridgeBootSessionUUIDString[0], &header->bridgeBootSessionUUID[0])) {
			bzero(&header->bridgeBootSessionUUID[0], sizeof(header->bridgeBootSessionUUID));
		}

		// sum __HIB seg, with zeros for the stack
		src = (uint8_t *) trunc_page(hibernateBase);
		for (page = 0; page < count; page++) {
			if ((src < &gIOHibernateRestoreStack[0]) || (src >= &gIOHibernateRestoreStackEnd[0])) {
				restore1Sum += hibernate_sum_page(src, (uint32_t) (header->restore1CodeVirt + page));
			} else {
				restore1Sum += 0x00000000;
			}
			src += page_size;
		}
		sum1 = restore1Sum;

		// write the __HIB seg, with zeros for the stack

		src = (uint8_t *) trunc_page(hibernateBase);
		count = ((uintptr_t) &gIOHibernateRestoreStack[0]) - trunc_page(hibernateBase);
		if (count) {
			err = IOHibernatePolledFileWrite(vars, src, count, cryptvars);
			if (kIOReturnSuccess != err) {
				break;
			}
		}
		err = IOHibernatePolledFileWrite(vars,
		    (uint8_t *) NULL,
		    &gIOHibernateRestoreStackEnd[0] - &gIOHibernateRestoreStack[0],
		    cryptvars);
		if (kIOReturnSuccess != err) {
			break;
		}
		src = &gIOHibernateRestoreStackEnd[0];
		count = round_page(hibernateEnd) - ((uintptr_t) src);
		if (count) {
			err = IOHibernatePolledFileWrite(vars, src, count, cryptvars);
			if (kIOReturnSuccess != err) {
				break;
			}
		}

		if (!vars->hwEncrypt && (kIOHibernateModeEncrypt & gIOHibernateMode)) {
			vars->fileVars->encryptStart = (vars->fileVars->position & ~(AES_BLOCK_SIZE - 1));
			vars->fileVars->encryptEnd   = UINT64_MAX;
			HIBLOG("encryptStart %qx\n", vars->fileVars->encryptStart);
		}

		// write the preview buffer

		if (vars->previewBuffer) {
			ppnum = 0;
			count = 0;
			do{
				phys64 = vars->previewBuffer->getPhysicalSegment(count, &segLen, kIOMemoryMapperNone);
				pageAndCount[0] = atop_64_ppnum(phys64);
				pageAndCount[1] = atop_64_ppnum(segLen);
				err = IOHibernatePolledFileWrite(vars,
				    (const uint8_t *) &pageAndCount, sizeof(pageAndCount),
				    cryptvars);
				if (kIOReturnSuccess != err) {
					break;
				}
				count += segLen;
				ppnum += sizeof(pageAndCount);
			}while (phys64);
			if (kIOReturnSuccess != err) {
				break;
			}

			src = (uint8_t *) vars->previewBuffer->getPhysicalSegment(0, NULL, _kIOMemorySourceSegment);

			((hibernate_preview_t *)src)->lockTime = gIOConsoleLockTime;

			count = (uint32_t) vars->previewBuffer->getLength();

			header->previewPageListSize = ((uint32_t) ppnum);
			header->previewSize         = ((uint32_t) (count + ppnum));

			for (page = 0; page < count; page += page_size) {
				phys64 = vars->previewBuffer->getPhysicalSegment(page, NULL, kIOMemoryMapperNone);
				sum1 += hibernate_sum_page(src + page, atop_64_ppnum(phys64));
			}
			if (kIOReturnSuccess != err) {
				break;
			}
			err = IOHibernatePolledFileWrite(vars, src, count, cryptvars);
			if (kIOReturnSuccess != err) {
				break;
			}
		}

		// mark areas for no save
		hibernate_set_descriptor_page_state(vars, IOPolledFileGetIOBuffer(vars->fileVars),
		    kIOHibernatePageStateFree, &pageCount);
		hibernate_set_descriptor_page_state(vars, vars->srcBuffer,
		    kIOHibernatePageStateFree, &pageCount);

		// copy out bitmap of pages available for trashing during restore

		bitmap_size = vars->page_list_wired->list_size;
		src = (uint8_t *) vars->page_list_wired;
		err = IOHibernatePolledFileWrite(vars, src, bitmap_size, cryptvars);
		if (kIOReturnSuccess != err) {
			break;
		}

		// mark more areas for no save, but these are not available
		// for trashing during restore

		hibernate_page_list_set_volatile(vars->page_list, vars->page_list_wired, &pageCount);

#if defined(__i386__) || defined(__x86_64__)
		// __HIB is explicitly saved above so we don't have to save it again
		page = atop_32(KERNEL_IMAGE_TO_PHYS(hibernateBase));
		count = atop_32(round_page(KERNEL_IMAGE_TO_PHYS(hibernateEnd))) - page;
		hibernate_set_page_state(vars->page_list, vars->page_list_wired,
		    page, count,
		    kIOHibernatePageStateFree);
		pageCount -= count;
#elif defined(__arm64__)
		// the segments described in IOHibernateHibSegInfo are stored directly in the
		// hibernation file, so they don't need to be saved again
		extern unsigned long gPhysBase, gPhysSize, gVirtBase;
		for (size_t i = 0; i < NUM_HIBSEGINFO_SEGMENTS; i++) {
			page = segInfo->segments[i].physPage;
			count = segInfo->segments[i].pageCount;
			uint64_t physAddr = ptoa_64(page);
			uint64_t size = ptoa_64(count);
			if (size &&
			    (physAddr >= gPhysBase) &&
			    (physAddr + size <= gPhysBase + gPhysSize)) {
				hibernate_set_page_state(vars->page_list, vars->page_list_wired,
				    page, count,
				    kIOHibernatePageStateFree);
				pageCount -= count;
			}
		}
#else
#error unimplemented
#endif

		hibernate_set_descriptor_page_state(vars, vars->previewBuffer,
		    kIOHibernatePageStateFree, &pageCount);
		hibernate_set_descriptor_page_state(vars, vars->handoffBuffer,
		    kIOHibernatePageStateFree, &pageCount);

#if KASAN
		vm_size_t shadow_pages_free = atop_64(shadow_ptop) - atop_64(shadow_pnext);

		/* no need to save unused shadow pages */
		hibernate_set_page_state(vars->page_list, vars->page_list_wired,
		    atop_64(shadow_pnext),
		    shadow_pages_free,
		    kIOHibernatePageStateFree);
#endif

		src = (uint8_t *) vars->srcBuffer->getBytesNoCopy();
		compressed = src + page_size;
		scratch    = compressed + page_size;

		pagesDone  = 0;
		lastBlob   = 0;

		HIBLOG("bitmap_size 0x%x, previewSize 0x%x, writing %d pages @ 0x%llx\n",
		    bitmap_size, header->previewSize,
		    pageCount, vars->fileVars->position);


		enum
		// pageType
		{
			kWired          = 0x02,
			kEncrypt        = 0x01,
			kWiredEncrypt   = kWired | kEncrypt,
			kWiredClear     = kWired,
			kUnwiredEncrypt = kEncrypt
		};

#if defined(__i386__) || defined(__x86_64__)
		bool cpuAES = (0 != (CPUID_FEATURE_AES & cpuid_features()));
#else /* defined(__i386__) || defined(__x86_64__) */
		static const bool cpuAES = true;
#endif /* defined(__i386__) || defined(__x86_64__) */

		for (pageType = kWiredEncrypt; pageType >= kUnwiredEncrypt; pageType--) {
			if (kUnwiredEncrypt == pageType) {
				// start unwired image
				if (!vars->hwEncrypt && (kIOHibernateModeEncrypt & gIOHibernateMode)) {
					vars->fileVars->encryptStart = (vars->fileVars->position & ~(((uint64_t)AES_BLOCK_SIZE) - 1));
					vars->fileVars->encryptEnd   = UINT64_MAX;
					HIBLOG("encryptStart %qx\n", vars->fileVars->encryptStart);
				}
				bcopy(&cryptvars->aes_iv[0],
				    &gIOHibernateCryptWakeContext.aes_iv[0],
				    sizeof(cryptvars->aes_iv));
				cryptvars = &gIOHibernateCryptWakeContext;
			}
			for (iterDone = false, ppnum = 0; !iterDone;) {
				if (cpuAES && (pageType == kWiredClear)) {
					count = 0;
				} else {
					count = hibernate_page_list_iterate((kWired & pageType) ? vars->page_list_wired : vars->page_list,
					    &ppnum);
					if (count > UINT_MAX) {
						count = UINT_MAX;
					}
				}
//              kprintf("[%d](%x : %x)\n", pageType, ppnum, count);
				iterDone = !count;

				if (!cpuAES) {
					if (count && (kWired & pageType) && needEncrypt) {
						uint32_t checkIndex;
						for (checkIndex = 0;
						    (checkIndex < count)
						    && (((kEncrypt & pageType) == 0) == pmap_is_noencrypt(((ppnum_t)(ppnum + checkIndex))));
						    checkIndex++) {
						}
						if (!checkIndex) {
							ppnum++;
							continue;
						}
						count = checkIndex;
					}
				}

				switch (pageType) {
				case kWiredEncrypt:   wiredPagesEncrypted += count; break;
				case kWiredClear:     wiredPagesClear     += count; break;
				case kUnwiredEncrypt: dirtyPagesEncrypted += count; break;
				}

				if (iterDone && (kWiredEncrypt == pageType)) {/* not yet end of wired list */
				} else {
					pageAndCount[0] = (uint32_t) ppnum;
					pageAndCount[1] = (uint32_t) count;
					err = IOHibernatePolledFileWrite(vars,
					    (const uint8_t *) &pageAndCount, sizeof(pageAndCount),
					    cryptvars);
					if (kIOReturnSuccess != err) {
						break;
					}
				}

				for (page = ppnum; page < (ppnum + count); page++) {
					err = IOMemoryDescriptorWriteFromPhysical(vars->srcBuffer, 0, ptoa_64(page), page_size);
					if (err) {
						HIBLOG("IOMemoryDescriptorWriteFromPhysical %d [%ld] %x\n", __LINE__, (long)page, err);
						break;
					}

					sum = hibernate_sum_page(src, (uint32_t) page);
					if (kWired & pageType) {
						sum1 += sum;
					} else {
						sum2 += sum;
					}

					clock_get_uptime(&startTime);
					wkresult = WKdm_compress_new((const WK_word*) src,
					    (WK_word*) compressed,
					    (WK_word*) scratch,
					    (uint32_t) (page_size - 4));

					clock_get_uptime(&endTime);
					ADD_ABSOLUTETIME(&compTime, &endTime);
					SUB_ABSOLUTETIME(&compTime, &startTime);

					compBytes += page_size;
					pageCompressedSize = (-1 == wkresult) ? page_size : wkresult;

					if (pageCompressedSize == 0) {
						pageCompressedSize = 4;
						data = src;

						if (*(uint32_t *)src) {
							svPageCount++;
						} else {
							zvPageCount++;
						}
					} else {
						if (pageCompressedSize != page_size) {
							data = compressed;
						} else {
							data = src;
						}
					}

					assert(pageCompressedSize <= page_size);
					tag = ((uint32_t) pageCompressedSize) | kIOHibernateTagSignature;
					err = IOHibernatePolledFileWrite(vars, (const uint8_t *) &tag, sizeof(tag), cryptvars);
					if (kIOReturnSuccess != err) {
						break;
					}

					err = IOHibernatePolledFileWrite(vars, data, (pageCompressedSize + 3) & ~3, cryptvars);
					if (kIOReturnSuccess != err) {
						break;
					}

					compressedSize += pageCompressedSize;
					uncompressedSize += page_size;
					pagesDone++;

					if (vars->consoleMapping && (0 == (1023 & pagesDone))) {
						blob = ((pagesDone * kIOHibernateProgressCount) / pageCount);
						if (blob != lastBlob) {
							ProgressUpdate(gIOHibernateGraphicsInfo, vars->consoleMapping, lastBlob, blob);
							lastBlob = blob;
						}
					}
					if (0 == (8191 & pagesDone)) {
						clock_get_uptime(&endTime);
						SUB_ABSOLUTETIME(&endTime, &allTime);
						absolutetime_to_nanoseconds(endTime, &nsec);
						progressStamp = nsec / 750000000ULL;
						if (progressStamp != lastProgressStamp) {
							lastProgressStamp = progressStamp;
							HIBPRINT("pages %d (%d%%)\n", pagesDone, (100 * pagesDone) / pageCount);
						}
					}
				}
				if (kIOReturnSuccess != err) {
					break;
				}
				ppnum = page;
			}

			if (kIOReturnSuccess != err) {
				break;
			}

			if ((kEncrypt & pageType) && vars->fileVars->encryptStart) {
				vars->fileVars->encryptEnd = ((vars->fileVars->position + 511) & ~511ULL);
				HIBLOG("encryptEnd %qx\n", vars->fileVars->encryptEnd);
			}

			if (kWiredEncrypt != pageType) {
				// end of image1/2 - fill to next block
				err = IOHibernatePolledFileWrite(vars, NULL, 0, cryptvars);
				if (kIOReturnSuccess != err) {
					break;
				}
			}
			if (kWiredClear == pageType) {
				// enlarge wired image for test
				// err = IOHibernatePolledFileWrite(vars, 0, 0x60000000, cryptvars);

				// end wired image
				header->encryptStart = vars->fileVars->encryptStart;
				header->encryptEnd   = vars->fileVars->encryptEnd;
				image1Size = vars->fileVars->position;
				HIBLOG("image1Size 0x%qx, encryptStart1 0x%qx, End1 0x%qx\n",
				    image1Size, header->encryptStart, header->encryptEnd);
			}
		}
		if (kIOReturnSuccess != err) {
			if (kIOReturnOverrun == err) {
				// update actual compression ratio on not enough space (for retry)
				gIOHibernateCompression = (compressedSize << 8) / uncompressedSize;
			}

			// update partial amount written (for IOPolledFileClose cleanup/unmap)
			header->imageSize = vars->fileVars->position;
			break;
		}


		// Header:

		header->imageSize    = vars->fileVars->position;
		header->image1Size   = image1Size;
		header->bitmapSize   = bitmap_size;
		header->pageCount    = pageCount;

		header->restore1Sum  = restore1Sum;
		header->image1Sum    = sum1;
		header->image2Sum    = sum2;
		header->sleepTime    = gIOLastSleepTime.tv_sec;

		header->compression     = ((uint32_t)((compressedSize << 8) / uncompressedSize));
#if defined(__arm64__)
		/*
		 * We don't support retry on hibernation failure and so
		 * we don't want to set this value to anything smaller
		 * just because we may have been lucky this time around.
		 * Though we'll let it go higher.
		 */
		if (header->compression < HIB_COMPR_RATIO_ARM64) {
			header->compression  = HIB_COMPR_RATIO_ARM64;
		}

		/* Compute the "mem slide" -- difference between the virtual base and the physical base */
		header->kernelSlide = gVirtBase - gPhysBase;
#endif /* __arm64__ */

		gIOHibernateCompression = header->compression;

		count = vars->fileVars->fileExtents->getLength();
		if (count > sizeof(header->fileExtentMap)) {
			header->fileExtentMapSize = ((uint32_t) count);
			count = sizeof(header->fileExtentMap);
		} else {
			header->fileExtentMapSize = sizeof(header->fileExtentMap);
		}
		bcopy(&fileExtents[0], &header->fileExtentMap[0], count);

		header->deviceBase      = vars->fileVars->block0;
		header->deviceBlockSize = vars->fileVars->blockSize;
		header->lastHibAbsTime  = mach_absolute_time();
		header->lastHibContTime = mach_continuous_time();


		IOPolledFileSeek(vars->fileVars, 0);
		err = IOHibernatePolledFileWrite(vars,
		    (uint8_t *) header, sizeof(IOHibernateImageHeader),
		    cryptvars);
		if (kIOReturnSuccess != err) {
#if DEVELOPMENT || DEBUG
			printf("Polled write of header failed (error %x)\n", err);
#endif
			break;
		}

		err = IOHibernatePolledFileWrite(vars, NULL, 0, cryptvars);
#if DEVELOPMENT || DEBUG
		if (kIOReturnSuccess != err) {
			printf("NULL polled write (flush) failed (error %x)\n", err);
		}
#endif
	} while (false);

	clock_get_uptime(&endTime);

	IOService::getPMRootDomain()->pmStatsRecordEvent(
		kIOPMStatsHibernateImageWrite | kIOPMStatsEventStopFlag, endTime);

	SUB_ABSOLUTETIME(&endTime, &allTime);
	absolutetime_to_nanoseconds(endTime, &nsec);
	HIBLOG("all time: %qd ms, ", nsec / 1000000ULL);

	absolutetime_to_nanoseconds(compTime, &nsec);
	HIBLOG("comp bytes: %qd time: %qd ms %qd Mb/s, ",
	    compBytes,
	    nsec / 1000000ULL,
	    nsec ? (((compBytes * 1000000000ULL) / 1024 / 1024) / nsec) : 0);

	absolutetime_to_nanoseconds(vars->fileVars->cryptTime, &nsec);
	HIBLOG("crypt bytes: %qd time: %qd ms %qd Mb/s, ",
	    vars->fileVars->cryptBytes,
	    nsec / 1000000ULL,
	    nsec ? (((vars->fileVars->cryptBytes * 1000000000ULL) / 1024 / 1024) / nsec) : 0);

	HIBLOG("\nimage %qd (%lld%%), uncompressed %qd (%d), compressed %qd (%d%%)\n",
	    header->imageSize, (header->imageSize * 100) / vars->fileVars->fileSize,
	    uncompressedSize, atop_32(uncompressedSize), compressedSize,
	    uncompressedSize ? ((int) ((compressedSize * 100ULL) / uncompressedSize)) : 0);

	HIBLOG("\nsum1 %x, sum2 %x\n", sum1, sum2);

	HIBLOG("svPageCount %d, zvPageCount %d, wiredPagesEncrypted %d, wiredPagesClear %d, dirtyPagesEncrypted %d\n",
	    svPageCount, zvPageCount, wiredPagesEncrypted, wiredPagesClear, dirtyPagesEncrypted);

	if (pollerOpen) {
		IOPolledFilePollersClose(vars->fileVars, (kIOReturnSuccess == err) ? kIOPolledBeforeSleepState : kIOPolledBeforeSleepStateAborted );
	}

	if (vars->consoleMapping) {
		ProgressUpdate(gIOHibernateGraphicsInfo,
		    vars->consoleMapping, 0, kIOHibernateProgressCount);
	}

	HIBLOG("hibernate_write_image done(%x)\n", err);

	// should we come back via regular wake, set the state in memory.
	gIOHibernateState = kIOHibernateStateInactive;

	KDBG(IOKDBG_CODE(DBG_HIBERNATE, 1) | DBG_FUNC_END, wiredPagesEncrypted,
	    wiredPagesClear, dirtyPagesEncrypted);

#if defined(__arm64__)
	if (kIOReturnSuccess == err) {
		return kIOHibernatePostWriteHalt;
	} else {
		// on ARM, once ApplePMGR decides we're hibernating, we can't turn back
		// see: <rdar://problem/63848862> Tonga ApplePMGR diff quiesce path support
		vm_panic_hibernate_write_image_failed(err);
		return err; //not coming here post panic
	}
#else
	if (kIOReturnSuccess == err) {
		if (kIOHibernateModeSleep & gIOHibernateMode) {
			return kIOHibernatePostWriteSleep;
		} else if (kIOHibernateModeRestart & gIOHibernateMode) {
			return kIOHibernatePostWriteRestart;
		} else {
			/* by default, power down */
			return kIOHibernatePostWriteHalt;
		}
	} else if (kIOReturnAborted == err) {
		return kIOHibernatePostWriteWake;
	} else {
		/* on error, sleep */
		return kIOHibernatePostWriteSleep;
	}
#endif
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

extern "C" void
hibernate_machine_init(void)
{
	IOReturn     err;
	uint32_t     sum;
	uint32_t     pagesDone;
	uint32_t     pagesRead = 0;
	AbsoluteTime startTime, compTime;
	AbsoluteTime allTime, endTime;
	AbsoluteTime startIOTime, endIOTime;
	uint64_t     nsec, nsecIO;
	uint64_t     compBytes;
	uint64_t     lastProgressStamp = 0;
	uint64_t     progressStamp;
	IOPolledFileCryptVars * cryptvars = NULL;

	IOHibernateVars * vars  = &gIOHibernateVars;
	bzero(gIOHibernateStats, sizeof(hibernate_statistics_t));

	if (!vars->fileVars || !vars->fileVars->pollers) {
		return;
	}

	sum = gIOHibernateCurrentHeader->actualImage1Sum;
	pagesDone = gIOHibernateCurrentHeader->actualUncompressedPages;

	if (kIOHibernateStateWakingFromHibernate != gIOHibernateState) {
		HIBLOG("regular wake\n");
		return;
	}

	HIBPRINT("diag %x %x %x %x\n",
	    gIOHibernateCurrentHeader->diag[0], gIOHibernateCurrentHeader->diag[1],
	    gIOHibernateCurrentHeader->diag[2], gIOHibernateCurrentHeader->diag[3]);

#if defined(__i386__) || defined(__x86_64__)
#define t40ms(x)        ((uint32_t)((tmrCvt((((uint64_t)(x)) << 8), tscFCvtt2n) / 1000000)))
#else /* defined(__i386__) || defined(__x86_64__) */
#define t40ms(x)        x
#endif /* defined(__i386__) || defined(__x86_64__) */
#define tStat(x, y)     gIOHibernateStats->x = t40ms(gIOHibernateCurrentHeader->y);
	tStat(booterStart, booterStart);
	gIOHibernateStats->smcStart = gIOHibernateCurrentHeader->smcStart;
	tStat(booterDuration0, booterTime0);
	tStat(booterDuration1, booterTime1);
	tStat(booterDuration2, booterTime2);
	tStat(booterDuration, booterTime);
	tStat(booterConnectDisplayDuration, connectDisplayTime);
	tStat(booterSplashDuration, splashTime);
	tStat(trampolineDuration, trampolineTime);

	gIOHibernateStats->image1Size  = gIOHibernateCurrentHeader->image1Size;
	gIOHibernateStats->imageSize   = gIOHibernateCurrentHeader->imageSize;
	gIOHibernateStats->image1Pages = pagesDone;

	/* HIBERNATE_stats */
	KDBG(IOKDBG_CODE(DBG_HIBERNATE, 14), gIOHibernateStats->smcStart,
	    gIOHibernateStats->booterStart, gIOHibernateStats->booterDuration,
	    gIOHibernateStats->trampolineDuration);

	HIBLOG("booter start at %d ms smc %d ms, [%d, %d, %d] total %d ms, dsply %d, %d ms, tramp %d ms\n",
	    gIOHibernateStats->booterStart,
	    gIOHibernateStats->smcStart,
	    gIOHibernateStats->booterDuration0,
	    gIOHibernateStats->booterDuration1,
	    gIOHibernateStats->booterDuration2,
	    gIOHibernateStats->booterDuration,
	    gIOHibernateStats->booterConnectDisplayDuration,
	    gIOHibernateStats->booterSplashDuration,
	    gIOHibernateStats->trampolineDuration);

	HIBLOG("hibernate_machine_init: state %d, image pages %d, sum was %x, imageSize 0x%qx, image1Size 0x%qx, conflictCount %d, nextFree %x\n",
	    gIOHibernateState, pagesDone, sum, gIOHibernateStats->imageSize, gIOHibernateStats->image1Size,
	    gIOHibernateCurrentHeader->conflictCount, gIOHibernateCurrentHeader->nextFree);

	if ((0 != (kIOHibernateModeSleep & gIOHibernateMode))
	    && (0 != ((kIOHibernateModeDiscardCleanActive | kIOHibernateModeDiscardCleanInactive) & gIOHibernateMode))) {
		hibernate_page_list_discard(vars->page_list);
	}

	if (vars->hwEncrypt) {
		// if vars->hwEncrypt is true, we don't need cryptvars since we supply the
		// decryption key via IOPolledFilePollersSetEncryptionKey
		cryptvars = NULL;
	} else {
		cryptvars = (kIOHibernateModeEncrypt & gIOHibernateMode) ? &gIOHibernateCryptWakeContext : NULL;
	}

	if (gIOHibernateCurrentHeader->handoffPageCount > gIOHibernateHandoffPageCount) {
		panic("handoff overflow");
	}

	IOHibernateHandoff * handoff;
	bool                 done                   = false;
	bool                 foundCryptData         = false;
	bool                 foundVolumeEncryptData = false;
	const uint8_t      * handoffStart           = (const uint8_t*)vars->handoffBuffer->getBytesNoCopy();
	const uint8_t      * handoffEnd             = handoffStart + vars->handoffBuffer->getLength();

	for (handoff = (IOHibernateHandoff *) vars->handoffBuffer->getBytesNoCopy();
	    !done;
	    handoff = (IOHibernateHandoff *) &handoff->data[handoff->bytecount]) {
		if (((uint8_t*)handoff < handoffStart) ||
		    (&handoff->data[handoff->bytecount] > handoffEnd)) {
			panic("handoff out of range");
		}
//	HIBPRINT("handoff %p, %x, %x\n", handoff, handoff->type, handoff->bytecount);
		uint8_t * data = &handoff->data[0];
		switch (handoff->type) {
		case kIOHibernateHandoffTypeEnd:
			done = true;
			break;

		case kIOHibernateHandoffTypeGraphicsInfo:
			if (handoff->bytecount == sizeof(*gIOHibernateGraphicsInfo)) {
				bcopy(data, gIOHibernateGraphicsInfo, sizeof(*gIOHibernateGraphicsInfo));
			}
			break;

		case kIOHibernateHandoffTypeCryptVars:
			if (cryptvars) {
				hibernate_cryptwakevars_t *
				    wakevars = (hibernate_cryptwakevars_t *) &handoff->data[0];
				if (handoff->bytecount == sizeof(*wakevars)) {
					bcopy(&wakevars->aes_iv[0], &cryptvars->aes_iv[0], sizeof(cryptvars->aes_iv));
				} else {
					panic("kIOHibernateHandoffTypeCryptVars(%d)", handoff->bytecount);
				}
			}
			foundCryptData = true;
			bzero(data, handoff->bytecount);
			break;

		case kIOHibernateHandoffTypeVolumeCryptKey:
			if (handoff->bytecount == vars->volumeCryptKeySize) {
				bcopy(data, &vars->volumeCryptKey[0], vars->volumeCryptKeySize);
				foundVolumeEncryptData = true;
			} else {
				panic("kIOHibernateHandoffTypeVolumeCryptKey(%d)", handoff->bytecount);
			}
			break;

#if defined(__i386__) || defined(__x86_64__)
		case kIOHibernateHandoffTypeMemoryMap:

			clock_get_uptime(&allTime);

			hibernate_newruntime_map(data, handoff->bytecount,
			    gIOHibernateCurrentHeader->systemTableOffset);

			clock_get_uptime(&endTime);

			SUB_ABSOLUTETIME(&endTime, &allTime);
			absolutetime_to_nanoseconds(endTime, &nsec);

			HIBLOG("hibernate_newruntime_map time: %qd ms, ", nsec / 1000000ULL);

			break;

		case kIOHibernateHandoffTypeDeviceTree:
		{
//		    DTEntry chosen = NULL;
//		    HIBPRINT("SecureDTLookupEntry %d\n", SecureDTLookupEntry((const DTEntry) data, "/chosen", &chosen));
		}
		break;
#endif /* defined(__i386__) || defined(__x86_64__) */

		default:
			done = (kIOHibernateHandoffType != (handoff->type & 0xFFFF0000));
			break;
		}
	}

	if (vars->hwEncrypt && !foundVolumeEncryptData) {
		panic("no volumeCryptKey");
	} else if (cryptvars && !foundCryptData) {
		panic("hibernate handoff");
	}

	HIBPRINT("video 0x%llx %d %d %d status %x\n",
	    gIOHibernateGraphicsInfo->physicalAddress, gIOHibernateGraphicsInfo->depth,
	    gIOHibernateGraphicsInfo->width, gIOHibernateGraphicsInfo->height, gIOHibernateGraphicsInfo->gfxStatus);

	if (vars->videoMapping && gIOHibernateGraphicsInfo->physicalAddress) {
		vars->videoMapSize = round_page(gIOHibernateGraphicsInfo->height
		    * gIOHibernateGraphicsInfo->rowBytes);
		if (vars->videoMapSize > vars->videoAllocSize) {
			vars->videoMapSize = 0;
		} else {
			IOMapPages(kernel_map,
			    vars->videoMapping, gIOHibernateGraphicsInfo->physicalAddress,
			    vars->videoMapSize, kIOMapInhibitCache );
		}
	}

	if (vars->videoMapSize) {
		ProgressUpdate(gIOHibernateGraphicsInfo,
		    (uint8_t *) vars->videoMapping, 0, kIOHibernateProgressCount);
	}


	uint8_t * src = (uint8_t *) vars->srcBuffer->getBytesNoCopy();
	uint8_t * compressed = src + page_size;
	uint8_t * scratch    = compressed + page_size;
	uint32_t  decoOffset;

	clock_get_uptime(&allTime);
	AbsoluteTime_to_scalar(&compTime) = 0;
	compBytes = 0;

	HIBLOG("IOPolledFilePollersOpen(), ml_get_interrupts_enabled %d\n", ml_get_interrupts_enabled());
	err = IOPolledFilePollersOpen(vars->fileVars, kIOPolledAfterSleepState, false);
	clock_get_uptime(&startIOTime);
	endTime = startIOTime;
	SUB_ABSOLUTETIME(&endTime, &allTime);
	absolutetime_to_nanoseconds(endTime, &nsec);
	HIBLOG("IOPolledFilePollersOpen(%x) %qd ms\n", err, nsec / 1000000ULL);

	if (vars->hwEncrypt) {
		err = IOPolledFilePollersSetEncryptionKey(vars->fileVars,
		    &vars->volumeCryptKey[0], vars->volumeCryptKeySize);
		HIBLOG("IOPolledFilePollersSetEncryptionKey(%x) %ld\n", err, vars->volumeCryptKeySize);
		if (kIOReturnSuccess != err) {
			panic("IOPolledFilePollersSetEncryptionKey(0x%x)", err);
		}
		cryptvars = NULL;
	}

	IOPolledFileSeek(vars->fileVars, gIOHibernateCurrentHeader->image1Size);

	// kick off the read ahead
	vars->fileVars->bufferHalf   = 0;
	vars->fileVars->bufferLimit  = 0;
	vars->fileVars->lastRead     = 0;
	vars->fileVars->readEnd      = gIOHibernateCurrentHeader->imageSize;
	vars->fileVars->bufferOffset = vars->fileVars->bufferLimit;
	vars->fileVars->cryptBytes   = 0;
	AbsoluteTime_to_scalar(&vars->fileVars->cryptTime) = 0;

	err = IOPolledFileRead(vars->fileVars, NULL, 0, cryptvars);
	if (kIOReturnSuccess != err) {
		panic("Hibernate restore error %x", err);
	}
	vars->fileVars->bufferOffset = vars->fileVars->bufferLimit;
	// --

	HIBLOG("hibernate_machine_init reading\n");

	uint32_t * header = (uint32_t *) src;
	sum = 0;

	while (kIOReturnSuccess == err) {
		unsigned int count;
		unsigned int page;
		uint32_t     tag;
		vm_offset_t  compressedSize;
		ppnum_t      ppnum;

		err = IOPolledFileRead(vars->fileVars, src, 8, cryptvars);
		if (kIOReturnSuccess != err) {
			panic("Hibernate restore error %x", err);
		}

		ppnum = header[0];
		count = header[1];

//	HIBPRINT("(%x, %x)\n", ppnum, count);

		if (!count) {
			break;
		}

		for (page = 0; page < count; page++) {
			err = IOPolledFileRead(vars->fileVars, (uint8_t *) &tag, 4, cryptvars);
			if (kIOReturnSuccess != err) {
				panic("Hibernate restore error %x", err);
			}

			compressedSize = kIOHibernateTagLength & tag;
			if (kIOHibernateTagSignature != (tag & ~kIOHibernateTagLength)) {
				err = kIOReturnIPCError;
				panic("Hibernate restore error %x", err);
			}

			err = IOPolledFileRead(vars->fileVars, src, (compressedSize + 3) & ~3, cryptvars);
			if (kIOReturnSuccess != err) {
				panic("Hibernate restore error %x", err);
			}

			if (compressedSize < page_size) {
				decoOffset = ((uint32_t) page_size);
				clock_get_uptime(&startTime);

				if (compressedSize == 4) {
					int i;
					uint32_t *s, *d;

					s = (uint32_t *)src;
					d = (uint32_t *)(uintptr_t)compressed;

					for (i = 0; i < (int)(PAGE_SIZE / sizeof(int32_t)); i++) {
						*d++ = *s;
					}
				} else {
					pal_hib_decompress_page(src, compressed, scratch, ((unsigned int) compressedSize));
				}
				clock_get_uptime(&endTime);
				ADD_ABSOLUTETIME(&compTime, &endTime);
				SUB_ABSOLUTETIME(&compTime, &startTime);
				compBytes += page_size;
			} else {
				decoOffset = 0;
			}

			sum += hibernate_sum_page((src + decoOffset), ((uint32_t) ppnum));
			err = IOMemoryDescriptorReadToPhysical(vars->srcBuffer, decoOffset, ptoa_64(ppnum), page_size);
			if (err) {
				HIBLOG("IOMemoryDescriptorReadToPhysical [%ld] %x\n", (long)ppnum, err);
				panic("Hibernate restore error %x", err);
			}


			ppnum++;
			pagesDone++;
			pagesRead++;

			if (0 == (8191 & pagesDone)) {
				clock_get_uptime(&endTime);
				SUB_ABSOLUTETIME(&endTime, &allTime);
				absolutetime_to_nanoseconds(endTime, &nsec);
				progressStamp = nsec / 750000000ULL;
				if (progressStamp != lastProgressStamp) {
					lastProgressStamp = progressStamp;
					HIBPRINT("pages %d (%d%%)\n", pagesDone,
					    (100 * pagesDone) / gIOHibernateCurrentHeader->pageCount);
				}
			}
		}
	}
	if ((kIOReturnSuccess == err) && (pagesDone == gIOHibernateCurrentHeader->actualUncompressedPages)) {
		err = kIOReturnLockedRead;
	}

	if (kIOReturnSuccess != err) {
		panic("Hibernate restore error %x", err);
	}


	gIOHibernateCurrentHeader->actualImage2Sum = sum;
	gIOHibernateCompression = gIOHibernateCurrentHeader->compression;

	clock_get_uptime(&endIOTime);

	err = IOPolledFilePollersClose(vars->fileVars, kIOPolledAfterSleepState);

	clock_get_uptime(&endTime);

	IOService::getPMRootDomain()->pmStatsRecordEvent(
		kIOPMStatsHibernateImageRead | kIOPMStatsEventStartFlag, allTime);
	IOService::getPMRootDomain()->pmStatsRecordEvent(
		kIOPMStatsHibernateImageRead | kIOPMStatsEventStopFlag, endTime);

	SUB_ABSOLUTETIME(&endTime, &allTime);
	absolutetime_to_nanoseconds(endTime, &nsec);

	SUB_ABSOLUTETIME(&endIOTime, &startIOTime);
	absolutetime_to_nanoseconds(endIOTime, &nsecIO);

	gIOHibernateStats->kernelImageReadDuration = ((uint32_t) (nsec / 1000000ULL));
	gIOHibernateStats->imagePages              = pagesDone;

	HIBLOG("hibernate_machine_init pagesDone %d sum2 %x, time: %d ms, disk(0x%x) %qd Mb/s, ",
	    pagesDone, sum, gIOHibernateStats->kernelImageReadDuration, kDefaultIOSize,
	    nsecIO ? ((((gIOHibernateCurrentHeader->imageSize - gIOHibernateCurrentHeader->image1Size) * 1000000000ULL) / 1024 / 1024) / nsecIO) : 0);

	absolutetime_to_nanoseconds(compTime, &nsec);
	HIBLOG("comp bytes: %qd time: %qd ms %qd Mb/s, ",
	    compBytes,
	    nsec / 1000000ULL,
	    nsec ? (((compBytes * 1000000000ULL) / 1024 / 1024) / nsec) : 0);

	absolutetime_to_nanoseconds(vars->fileVars->cryptTime, &nsec);
	HIBLOG("crypt bytes: %qd time: %qd ms %qd Mb/s\n",
	    vars->fileVars->cryptBytes,
	    nsec / 1000000ULL,
	    nsec ? (((vars->fileVars->cryptBytes * 1000000000ULL) / 1024 / 1024) / nsec) : 0);

	KDBG(IOKDBG_CODE(DBG_HIBERNATE, 2), pagesRead, pagesDone);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
IOHibernateSetWakeCapabilities(uint32_t capability)
{
	if (kIOHibernateStateWakingFromHibernate == gIOHibernateState) {
		gIOHibernateStats->wakeCapability = capability;

		if (kIOPMSystemCapabilityGraphics & capability) {
			vm_compressor_do_warmup();
		}
	}
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
IOHibernateSystemRestart(void)
{
#if defined(__i386__) || defined(__x86_64__)
	static uint8_t    noteStore[32] __attribute__((aligned(32)));
	IORegistryEntry * regEntry;
	const OSSymbol *  sym;
	OSData *          noteProp;
	OSData *          data;
	uint8_t *         smcBytes;
	size_t            len;
	addr64_t          element;

	data = OSDynamicCast(OSData, IOService::getPMRootDomain()->getProperty(kIOHibernateSMCVariablesKey));
	if (!data) {
		return;
	}

	smcBytes = (typeof(smcBytes))data->getBytesNoCopy();
	len = data->getLength();
	if (len > sizeof(noteStore)) {
		len = sizeof(noteStore);
	}
	noteProp = OSData::withCapacity(3 * sizeof(element));
	if (!noteProp) {
		return;
	}
	element = len;
	noteProp->appendValue(element);
	element = crc32(0, smcBytes, len);
	noteProp->appendValue(element);

	bcopy(smcBytes, noteStore, len);
	element = (addr64_t) &noteStore[0];
	element = (element & page_mask) | ptoa_64(pmap_find_phys(kernel_pmap, element));
	noteProp->appendValue(element);

	if (!gIOOptionsEntry) {
		regEntry = IORegistryEntry::fromPath("/options", gIODTPlane);
		gIOOptionsEntry = OSDynamicCast(IODTNVRAM, regEntry);
		if (regEntry && !gIOOptionsEntry) {
			regEntry->release();
		}
	}

	sym = OSSymbol::withCStringNoCopy(kIOHibernateBootNoteKey);
	if (gIOOptionsEntry && sym) {
		gIOOptionsEntry->setProperty(sym, noteProp);
	}
	if (noteProp) {
		noteProp->release();
	}
	if (sym) {
		sym->release();
	}
#endif /* defined(__i386__) || defined(__x86_64__) */
}
