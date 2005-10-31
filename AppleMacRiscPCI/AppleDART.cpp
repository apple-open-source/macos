/*
 * Copyright (c) 1998-2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#include <libkern/OSAtomic.h>

#include <IOKit/IOLocks.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>

#include <IOKit/IOMapper.h>

extern "C" {
extern ppnum_t pmap_find_phys(pmap_t pmap, addr64_t va);
}

/*
 * Register layout defines
 */

//                00000000001111111111222222222233
//                01234567890123456789012345678901
//                10987654321098765432109876543210
//                33222222222211111111110000000000

// U3

// DARTCNTL Register format
// DARTBASE       11111111111111111111		        ffff f    20b x << 12
// IDARTTLB                            1	              4    1b x << 10
// DARTEN                               1	              2    1b x <<  9
// DARTSIZE                              111111111            1ff  9b x <<  0

// DARTTAG Register format
// DARTLPN        1111111111111	                        fff8      13b x << 19
// DARTSET                     11111                       7e      5b x << 14


// U4

// DARTCNTL Register format
// DARTEN         1                                     8          1b x << 31
// IDARTTLB         1                                   2          1b x << 29

// DARTBASE Register format
// DARTBASE              111111111111111111111111       ff ffff   24b x <<  0

// DARTSIZE Register format
// DARTSIZE                     11111111111111111          1fff   17b x <<  0

// DARTTAG Register format
// DARTLPN               1111111111111                  fff8      13b x << 19
// DARTSET                            11111                7e      5b x << 14

#define U3DARTBASEMASK	 0xfffff
#define U3DARTBASESHFT	      12
#define U4DARTBASEMASK	0xffffff
#define U4DARTBASESHFT	       0

#define U3IDARTTLB		0x00000400	// 1 << 10
#define U3DARTEN		0x00000200	// 1 <<  9
#define U4DARTEN		0x80000000	// 1 << 31
#define U4IALLTLB		0x20000000	// 1 << 29

#define U3DARTSIZEMASK	   0x1ff
#define U3DARTSIZESHFT	       0
#define U4DARTSIZEMASK	  0x1fff
#define U4DARTSIZESHFT	       0

// #define DARTLPNMASK	  0x1fff
// #define DARTLPNSHFT	      19

// #define DARTVxMASK	     0xf
// #define DARTVxSHFT	      14

// These are with respect to the PCI address, i.e. not page number based.
// #define DARTSETMASK         0x1f
// #define DARTSETSHFT           14

// #define DARTWAYSETMASK      0x7f
// #define DARTWAYSETSHFT        14

#define DARTCNTL	0x0000
#define DARTBASE	0x0010
#define DARTSIZE	0x0020
#define DARTEXCPU3	0x0010
#define DARTEXCPU4	0x0030
// #define DARTTAG	0x1000
// #define DARTTAGSZ	0x0740
// #define DARTDATA	0x5000
// #define DARTDATASZ	0x1ff0

#define DARTTLBPAGES	     4

#define frmDARTTLB(tlb) ((Uint32 *) \
    ( (vm_address_t) fRegBase + (tlb << DARTSETSHFT) + DARTTAG) )

// Convert physical ppnum_t base of DART to U3 DARTCNTL register DARTBASE bits
#define setDARTBASE(b)	(fU3 ? (((b) & U3DARTBASEMASK) << U3DARTBASESHFT) : (((b) & U4DARTBASEMASK) << U4DARTBASESHFT))

// Convert Size that DART maps in Bytes to U3 DARTCNTL register DARTSIZE bits
#define setDARTSIZE(p)	(fU3 ? (((p) & U3DARTSIZEMASK) << U3DARTSIZESHFT) : (((p) & U4DARTSIZEMASK) << U4DARTSIZESHFT))

// Set the DART Enable Bit
#define setDARTEN() (fU3 ? U3DARTEN : U4DARTEN)

// Set the DART Invalidate All Bit
#define setIALLTLB() (fU3 ? U3IDARTTLB : U4IALLTLB)

// Convert a physical ppnum_t into DARTTAG register DARTLPN bits
// #define invalidDARTLPN(a) ( (a) & (DARTLPNMASK << DARTLPNSHFT) )

// General constants about all VART/DART style Address Re-Mapping Tables
#define kMapperPage	    (4 * 1024)
#define kTransPerPage	    (kMapperPage / sizeof(ppnum_t))

// 2Gb System table and 32Mb Regular (i.e. not system) table
#define kSysTableMapped	    (2UL * 1024 * 1024 * 1024)
#define kRegTableMapped	    (32UL * 1024 * 1024)

#define kSysMappedPages	    (kSysTableMapped / kMapperPage)
#define kRegMappedPages	    (kRegTableMapped / kMapperPage)

#define kSysDARTSize	    (kSysMappedPages / kTransPerPage)
#define kRegDARTSize	    (kRegMappedPages / kTransPerPage)

#define kMinZoneSize	    4		// Minimum Zone size in pages
#define kMaxNumZones	    (31 - 14)	// 31 bit mapped in 16K super pages

#define super IOMapper

class AppleDART : public IOMapper
{
    OSDeclareDefaultStructors(AppleDART);

// alias the fTable variable into our mappings table
#define fMappings	((volatile ppnum_t *) super::fTable)

private:

    static vm_size_t	gCacheLineSize;

    UInt32		fFreeLists[kMaxNumZones];

    IOLock		*fTableLock;
    IOSimpleLock	*fInvalidateLock;

    void		*fDummyPage;

    IOMemoryMap		*fRegisterVMMap;
    UInt8		*fRegBase;

    volatile UInt32	*fDARTCNTLReg;
    volatile UInt32	*fDARTBASEReg;
    volatile UInt32	*fDARTSIZEReg;
    volatile UInt32	*fDARTEXCPReg;

//  volatile UInt32	*fRegTagBase;
//  IOMemoryMap		*fTagVMMap;
    
// #define fDARTTAGReg	((volatile UInt32 *) (fRegBase + DARTTAG))
// #define fDARTDATAPReg	((volatile UInt32 *) (fRegBase + DARTDATA))

    UInt32		 fDARTCNTLVal;	// Shadow value of DARTCNTL register
    UInt32		 fNumZones;
    UInt32		 fMapperRegionSize;
    UInt32		 fFreeSleepers;
    ppnum_t		 fDummyPageNumber;

    bool		fU3;

    // Internal functions
    void flushMappings(volatile void *vaddr, UInt32 numMappings);

    void breakUp(unsigned start, unsigned end, unsigned freeInd);
    void invalidateDART(ppnum_t pnum, IOItemCount size);
    void tlbInvalidate(ppnum_t pnum, IOItemCount size);

    virtual void free();

    virtual bool initHardware(IOService *provider);

    virtual ppnum_t iovmAlloc(IOItemCount pages);
    virtual void iovmFree(ppnum_t addr, IOItemCount pages);

    virtual void iovmInsert(ppnum_t addr, IOItemCount offset, ppnum_t page);
    virtual void iovmInsert(ppnum_t addr, IOItemCount offset,
                            ppnum_t *pageList, IOItemCount pageCount);
    virtual void iovmInsert(ppnum_t addr, IOItemCount offset,
                            upl_page_info_t *pageList, IOItemCount pageCount);

    virtual addr64_t mapAddr(IOPhysicalAddress addr);
};

vm_size_t AppleDART::gCacheLineSize = 32;

OSDefineMetaClassAndStructors(AppleDART, IOMapper);

static inline void writeReg(volatile UInt32 *reg, UInt32 value)
{
    *reg = value;
    OSSynchronizeIO();
}

static inline UInt32 readReg(volatile UInt32 *reg)
{
    return *reg;
}

// Remember no value can be bigger than 31 bits as the sign bit indicates
// that this entry is valid to the hardware and that would be bad if it wasn't
typedef struct FreeDARTEntry {
    unsigned int
    /* bool */	    fValid : 1,
    /* bool */	    fInUse : 1,	// Allocated but not inserted yet
    /* bool */		   : 5,	// Align size on nibble boundary for debugging
    /* uint */	    fSize  : 5,
    /* uint */	           : 2,
    /* uint */	    fNext  :18;	// offset of FreeDARTEntry's
    unsigned int
    /* uint */	           :14,
    /* uint */	    fPrev  :18;	// offset of FreeDARTEntry's
} FreeDARTEntry;

#define kInvalidInUse	0x40000000

typedef struct ActiveDARTEntry {
    unsigned int
    /* bool */	    fValid : 1,	// Must be set to one if valid
    /* uint */	           :12, // Don't care
    /* uint */	    fPPNum :19;	// ppnum_t page of translation
};
#define kValidEntry 0x80000000

#define kActivePerFree (sizeof(freeDART[0]) / sizeof(ActiveDARTEntry))

bool AppleDART::initHardware(IOService *provider)
{
    IOPlatformDevice *nub = OSDynamicCast(IOPlatformDevice, provider);

    if (!nub)
	return false;

    UInt32 dartSizePages = 0;

    fIsSystem = true;

    if (fIsSystem && !IOMapper::gSystem) {
	IOLog("DART disabled\n");
	kprintf("DART disabled\n");
	return false;
    }

    if (IODTMatchNubWithKeys(provider, "('u3-dart')"))
	fU3 = true;

    OSNumber *sizeNum = (OSNumber *) getProperty("AppleARTSize");
    if (sizeNum)
	dartSizePages = sizeNum->unsigned32BitValue();

    fTableLock = IOLockAlloc();
    fInvalidateLock = IOSimpleLockAlloc();

    if (!fTableLock || !fInvalidateLock)
	return false;

    if (fIsSystem) {
	int bootArg;

	// @@@ gvdl: Must be a guppy, there has to be a better way.
	gCacheLineSize = 128;
	if (PE_parse_boot_arg("artsize", &bootArg ))
	    dartSizePages = bootArg;
	if (!dartSizePages)
	    dartSizePages = kSysDARTSize;
    }
    else if (!dartSizePages)
	dartSizePages = kRegDARTSize;

    if (!allocTable(dartSizePages * kMapperPage))
	return false;

    fRegisterVMMap = nub->mapDeviceMemoryWithIndex(0);
    if (!fRegisterVMMap)
	return false;
    fRegBase = (UInt8 *) fRegisterVMMap->getVirtualAddress();

    if (fU3) {
	fDARTCNTLReg = (volatile UInt32 *)(fRegBase + DARTCNTL);
	fDARTBASEReg = (volatile UInt32 *)(fRegBase + DARTCNTL);
	fDARTSIZEReg = (volatile UInt32 *)(fRegBase + DARTCNTL);
	fDARTEXCPReg = (volatile UInt32 *)(fRegBase + DARTEXCPU3);
    } else {
	fDARTCNTLReg = (volatile UInt32 *)(fRegBase + DARTCNTL);
	fDARTBASEReg = (volatile UInt32 *)(fRegBase + DARTBASE);
	fDARTSIZEReg = (volatile UInt32 *)(fRegBase + DARTSIZE);
	fDARTEXCPReg = (volatile UInt32 *)(fRegBase + DARTEXCPU4);
    }

    UInt32 canMapPages = dartSizePages * kTransPerPage;
    fMapperRegionSize = canMapPages;
    for (fNumZones = 0; canMapPages; fNumZones++)
	canMapPages >>= 1;
    fNumZones -= 3; // correct for overshoot and minumum 16K pages allocation

    fDARTCNTLVal = readReg(fDARTSIZEReg) | setDARTSIZE(dartSizePages);
    writeReg(fDARTSIZEReg, fDARTCNTLVal);

    fDARTCNTLVal = readReg(fDARTBASEReg) | setDARTBASE(fTablePhys);
    writeReg(fDARTBASEReg, fDARTCNTLVal);

    fDARTCNTLVal |= setDARTEN(); // Enable the DART
    writeReg(fDARTCNTLReg, fDARTCNTLVal);

    fDARTCNTLVal |= setIALLTLB();	// Assert Invalidate TLB bit in shadow
    invalidateDART(0, dartSizePages * kTransPerPage);

    breakUp(0, fNumZones, 0);
    *(ppnum_t *) fTable = kInvalidInUse;

    fDummyPage = IOMallocAligned(0x1000, 0x1000);
    fDummyPageNumber =
	pmap_find_phys(kernel_pmap, (addr64_t) (uintptr_t) fDummyPage);

    IOLog("DART enabled\n");
    kprintf("DART enabled\n");

    return true;
}

void AppleDART::free()
{
    if (fDummyPage) {
	IOFreeAligned(fDummyPage, 0x1000);
	fDummyPage = 0;
	fDummyPageNumber = 0;
    }

    if (fRegisterVMMap) {
	fRegBase = 0;
	fRegisterVMMap->release();
	fRegisterVMMap = 0;
    }

#if 0
    if (fTagVMMap) {
	fRegTagBase = 0;
	fTagVMMap->release();
	fTagVMMap = 0;
    }
#endif

    if (fTableLock) {
	IOLockFree(fTableLock);
	fTableLock = 0;
    }
    if (fInvalidateLock) {
	IOSimpleLockFree(fInvalidateLock);
	fInvalidateLock = 0;
    }

    super::free();
}

// Must be called while locked
void AppleDART::breakUp(unsigned start, unsigned end, unsigned freeInd)
{
    unsigned int zoneSize;
    FreeDARTEntry *freeDART = (FreeDARTEntry *) fTable;

    do {
	// Need to break up bigger blocks of memory till we get one in our 
	// desired zone.
	end--;
	zoneSize = (kMinZoneSize/2 << end);
	ppnum_t tail = freeInd + zoneSize;

	// By definition free lists must be empty
	fFreeLists[end] = tail;
	freeDART[tail].fSize = end;
	freeDART[tail].fNext = freeDART[tail].fPrev = 0;
    } while (end != start);
    freeDART[freeInd].fSize = end;
}

// Zero is never a valid page to return
ppnum_t AppleDART::iovmAlloc(IOItemCount pages)
{
    unsigned int zone, zoneSize, z, cnt;
    ppnum_t next, ret = 0;
    FreeDARTEntry *freeDART = (FreeDARTEntry *) fTable;

    // Force an extra page on every allocation
    pages += 1;

    // Can't alloc anything of less than minumum
    if (pages < kMinZoneSize)
	pages = kMinZoneSize;

    // Can't alloc anything bigger than 1/2 table
    if (pages >= fMapperRegionSize/2)
    {
	panic("iovmAlloc");
	return 0;
    }

    // Find the appropriate zone for this allocation
    for (zone = 0, zoneSize = kMinZoneSize; pages > zoneSize; zone++)
	zoneSize <<= 1;

    {
	IOLockLock(fTableLock);

	for (;;) {
	    for (z = zone; z < fNumZones; z++) {
		if ( (ret = fFreeLists[z]) )
		    break;
	    }
	    if (ret)
		break;

	    fFreeSleepers++;
	    IOLockSleep(fTableLock, fFreeLists, THREAD_UNINT);
	    fFreeSleepers--;
	}

	// If we didn't find a entry in our size then break up the free block
	// that we did find.
	if (zone != z)
	    breakUp(zone, z, ret);

	freeDART[ret].fInUse = true;	// Mark entry as In Use
	next = freeDART[ret].fNext;
	fFreeLists[z] = next;
	if (next)
	    freeDART[next].fPrev = 0;

	IOLockUnlock(fTableLock);
    }

    // ret is free list offset not page offset;
    ret *= kActivePerFree;

    ppnum_t pageEntry = fDummyPageNumber | kValidEntry;
    for (cnt = 0; cnt < pages; cnt++) {
	volatile ppnum_t *activeDART = &fMappings[ret + cnt];
	*activeDART = pageEntry;
    }

    return ret;
}

void AppleDART::tlbInvalidate(ppnum_t pnum, IOItemCount size)
{
    IOSimpleLockLock(fInvalidateLock);

    writeReg(fDARTCNTLReg, fDARTCNTLVal); // Invalidate the tlb
    if (readReg(fDARTCNTLReg) & setIALLTLB()) {
	AbsoluteTime now, expirationTime;
	bool	     ok;
	UInt32       loops = 0;

	do {
	    clock_interval_to_deadline(100, kNanosecondScale, &expirationTime);
	    do {
		ok = !(readReg(fDARTCNTLReg) & setIALLTLB());
		if (ok)
		    break;
		clock_get_uptime(&now);
	    } while (CMP_ABSOLUTETIME(&now, &expirationTime) < 0);
	    if (ok)
		break;

	    // clear and reset IDARTTLB
	    writeReg(fDARTCNTLReg, fDARTCNTLVal & ~setIALLTLB());
	    writeReg(fDARTCNTLReg, fDARTCNTLVal);
	} while (loops < 10);
	if (!ok)
	    panic("AppleDART IDARTTLB");
    }

    IOSimpleLockUnlock(fInvalidateLock);
}

void AppleDART::invalidateDART(ppnum_t pnum, IOItemCount size)
{
    // Clear out all of those valid bits in the dart table
    if (size >= (2 * gCacheLineSize))
	bzero((void *) &fMappings[pnum], size * sizeof(fMappings[0]));
    else
    {	// Arbitrary break even point
	for (vm_size_t i = 0; i < size; i++)
	    fMappings[pnum+i] = 0;
    }

    // Flush changes out to the U3
    flushMappings(&fMappings[pnum], size);

    tlbInvalidate(pnum, size);
}

void AppleDART::iovmFree(ppnum_t addr, IOItemCount pages)
{
    unsigned int zone, zoneSize, z;
    FreeDARTEntry *freeDART = (FreeDARTEntry *) fTable;

    // Force an extra page on every allocation
    pages += 1;

    // Can't free anything of less than minumum
    if (pages < kMinZoneSize)
	pages = kMinZoneSize;

    // Can't free anything bigger than 1/2 table
    if (pages >= fMapperRegionSize/2)
	return;

    // Find the appropriate zone for this allocation
    for (zone = 0, zoneSize = kMinZoneSize; pages > zoneSize; zone++)
	zoneSize <<= 1;

    // Grab lock that protects the dart
    IOLockLock(fTableLock);

    invalidateDART(addr, pages);

    addr /= kActivePerFree;

    // We are freeing a block, check to see if pairs are available for 
    // coalescing.  We will walk up the entire chain if we can.
    for (z = zone; z < fNumZones; z++) {
	ppnum_t pair = addr ^ (kMinZoneSize/2 << z);	// Find pair address
	if (freeDART[pair].fValid || freeDART[pair].fInUse || (freeDART[pair].fSize != z))
	    break;

	// The paired alloc entry is free if we are here
	ppnum_t next = freeDART[pair].fNext;
	ppnum_t prev = freeDART[pair].fPrev;

	// Remove the pair from its freeList
	if (prev)
	    freeDART[prev].fNext = next;
	else
	    fFreeLists[z] = next;

	if (next)
	    freeDART[next].fPrev = prev;

	// Sort the addr and the pair
	if (addr > pair)
	    addr = pair;
    }

    // Add the allocation entry into it's free list and re-init it
    freeDART[addr].fSize = z;
    freeDART[addr].fNext = fFreeLists[z];
    if (fFreeLists[z])
	freeDART[fFreeLists[z]].fPrev = addr;
    freeDART[addr].fPrev = 0;
    fFreeLists[z] = addr;

    if (fFreeSleepers)
	IOLockWakeup(fTableLock, fFreeLists, /* oneThread */ false);

    IOLockUnlock(fTableLock);
}

addr64_t AppleDART::mapAddr(IOPhysicalAddress addr)
{
    if (addr >= ptoa_32(fMapperRegionSize))
    {
	return (addr64_t) addr;	// Not mapped by us anyway
    }
    else
    {
	ppnum_t *activeDART = (ppnum_t *) fTable;
	UInt offset = addr & PAGE_MASK;

	ppnum_t mappedPage = activeDART[atop_32(addr)];
	if (mappedPage &  kValidEntry) {
	    mappedPage ^= kValidEntry;	// Clear validity bit
	    return ptoa_64(mappedPage) | offset;
	}
	panic("%s::mapAddr(0x%08lx) not mapped for I/O\n", getName(), addr);
	return 0;
    }
}

void AppleDART::iovmInsert(ppnum_t addr, IOItemCount offset, ppnum_t page)
{
    addr += offset;	// Add the offset page to the base address

    volatile ppnum_t *activeDART = &fMappings[addr];
    *activeDART = page | kValidEntry;

    // Flush changes out to the U3
    flushMappings(activeDART, 1);

    tlbInvalidate(addr, 1);
}

void AppleDART::iovmInsert(ppnum_t addr, IOItemCount offset,
	ppnum_t *pageList, IOItemCount pageCount)
{
    addr += offset;	// Add the offset page to the base address

    IOItemCount i;
    volatile ppnum_t *activeDART = &fMappings[addr];

    for (i = 0; i < pageCount; i++)
	activeDART[i] = pageList[i] | kValidEntry;

    // Flush changes out to the U3
    flushMappings(activeDART, pageCount);

    tlbInvalidate(addr, pageCount);
}

void AppleDART::iovmInsert(ppnum_t addr, IOItemCount offset,
	upl_page_info_t *pageList, IOItemCount pageCount)
{
    addr += offset;	// Add the offset page to the base address

    IOItemCount i;
    volatile ppnum_t *activeDART = &fMappings[addr];

    for (i = 0; i < pageCount; i++)
	activeDART[i] = pageList[i].phys_addr | kValidEntry;

    // Flush changes out to the U3
    flushMappings(activeDART, pageCount);

    tlbInvalidate(addr, pageCount);
}

static inline void __sync(void) { __asm__ ("sync"); }

static inline void __isync(void) { __asm__ ("isync"); }

static inline void __dcbf(vm_address_t base, unsigned long offset)
{
    __asm__ ("dcbf %0, %1"
	    :
	    : "r" (base), "r" (offset)
	    : "r0");
}

static inline void __dcbst(vm_address_t base, unsigned long offset)
{
    __asm__ ("dcbst %0, %1"
	    :
	    : "r" (base), "r" (offset)
	    : "r0");
}

static inline unsigned long __lwzx(vm_address_t base, unsigned long offset)
{
    unsigned long result;

    __asm__ volatile("lwzx %0, %1, %2"
	    : "=r" (result)
	    : "r" (base), "r" (offset)
	    : "r0");
    return result;

    /* return *((unsigned long *) ((unsigned char *) base + offset)); */
}

// Note to workaround an issue where the memory controller does read-ahead
// of unmapped memory it is necessary to flush one more mapping than
// requested by the actual call, see len initialisation below.
void AppleDART::flushMappings(volatile void *vaddr, UInt32 numMappings)
{
    SInt32 csize = gCacheLineSize;
    vm_address_t arithAddr = (vm_address_t) vaddr;
    vm_address_t vaddr_cache_aligned = arithAddr & ~(csize-1);
    UInt32 len = (numMappings + 1) * sizeof(fMappings[0]);	// add one
    SInt c, end = ((SInt)((arithAddr & (csize-1)) + len)) - csize;

    for (c = 0; c < end; c += csize)
	__dcbf(vaddr_cache_aligned, c);

    __sync();
    __isync();
    __dcbf(vaddr_cache_aligned, c);
    __sync();
    __isync();
    __lwzx(vaddr_cache_aligned, c);
    __isync();
}

