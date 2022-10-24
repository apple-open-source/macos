/*
 * Copyright (c) 2004-2021 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 2.0 (the
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

#pragma once

#include <IOKit/IOMapper.h>
#include <IOKit/IOKitKeysPrivate.h>
#include <libkern/tree.h>
#include <libkern/OSDebug.h>
#include <i386/cpuid.h>
#include <libkern/sysctl.h>

#include "dmar.h"

#if 0
#define VTHWLOCKTYPE   IOLock
#define VTHWLOCKALLOC  IOLockAlloc
#define VTHWLOCK(l)    IOLockLock(l)
#define VTHWUNLOCK(l)  IOLockUnlock(l)
#else
#define VTHWLOCKTYPE   IOSimpleLock
#define VTHWLOCKALLOC  IOSimpleLockAlloc
#define VTHWLOCK(l)    IOSimpleLockLock(l)
#define VTHWUNLOCK(l)  IOSimpleLockUnlock(l)
#endif

struct vtd_registers_t
{
	/*00*/ 	uint32_t version;
	/*04*/	uint32_t res1;
	/*08*/	uint64_t capability;
	/*10*/	uint64_t extended_capability;
	/*18*/	uint32_t global_command;
	/*1c*/	uint32_t global_status;
	/*20*/	uint64_t root_entry_table;
	/*28*/	uint64_t context_command;
	/*30*/	uint32_t res2;
	/*34*/	uint32_t fault_status;
	/*38*/	uint32_t fault_event_control;
	/*3c*/	uint32_t fault_event_data;
	/*40*/	uint32_t fault_event_address;
	/*44*/	uint32_t fault_event_upper_address;
	/*48*/	uint64_t res3[2];
	/*58*/	uint64_t advanced_fault;
	/*60*/	uint32_t res4;
	/*64*/	uint32_t protected_memory_enable;
	/*68*/	uint32_t protected_low_memory_base;
	/*6c*/	uint32_t protected_low_memory_limit;
	/*70*/	uint64_t protected_high_memory_base;
	/*78*/	uint64_t protected_high_memory_limit;
	/*80*/	uint64_t invalidation_queue_head;
	/*88*/	uint64_t invalidation_queue_tail;
	/*90*/	uint64_t invalidation_queue_address;
	/*98*/	uint32_t res5;
	/*9c*/	uint32_t invalidation_completion_status;
	/*a0*/	uint32_t invalidation_completion_event_control;
	/*a4*/	uint32_t invalidation_completion_event_data;
	/*a8*/	uint32_t invalidation_completion_event_address;
	/*ac*/	uint32_t invalidation_completion_event_upper_address;
	/*b0*/	uint64_t res6;
	/*b8*/	uint64_t interrupt_remapping_table;
	/*c0*/
};

struct vtd_iotlb_registers_t
{
	/*00*/	uint64_t address;
	/*08*/	uint64_t command;
};
struct vtd_fault_registers_t
{
	/*00*/	uint64_t fault_low;
	/*08*/	uint64_t fault_high;
};

enum
{
	kEntryPresent = 0x00000001ULL
};

struct root_entry_t
{
	uint64_t context_entry_ptr;
	uint64_t resv;
};

struct context_entry_t
{
	uint64_t address_space_root;
	uint64_t context_entry;
};

struct qi_descriptor_t
{
	uint64_t command;
	uint64_t address;
};

struct ir_descriptor_t
{
	uint64_t data;
	uint64_t source;
};

// address_space_root
enum
{
	//	kEntryPresent 			= 0x00000001ULL,
	kFaultProcessingDisable = 0x00000002ULL,
	kTranslationType0 		= 0x00000000ULL,
	kTranslationType1 		= 0x00000004ULL,
	kTranslationType2 		= 0x00000008ULL,
	kTranslationType3 		= 0x0000000CULL,
	kEvictionHint 		    = 0x00000010ULL,
	kAddressLocalityHint    = 0x00000020ULL,
};

// context_entry
enum
{
	kAddressWidth30			= 0x00000000ULL,
	kAddressWidth39			= 0x00000001ULL,
	kAddressWidth48			= 0x00000002ULL,
	kAddressWidth57			= 0x00000003ULL,
	kAddressWidth64			= 0x00000004ULL,
	
	kContextAvail1		    = 0x00000008ULL,	// 4b
	kDomainIdentifier1		= 0x00000100ULL,	// 16b
};

typedef uint64_t page_entry_t;

// page_entry_t
enum
{
	kReadAccess 			= 0x00000001ULL,
	kWriteAccess			= 0x00000002ULL,
	kPageAccess			= kReadAccess|kWriteAccess,
	kPageAvail1			= 0x00000004ULL,	// 5b
	kSuperPage			= 0x00000080ULL,
	kPageAvail2			= 0x00000100ULL,	// 3b
	kSnoopBehavior			= 0x00000800ULL,
	kTransientMapping		= 0x4000000000000000ULL,
	kPageAvail3			= 0x8000000000000000ULL, // 1b

	kPageAddrMask			= 0x3ffffffffffff000ULL
};

typedef char vtd_registers_t_check[(sizeof(vtd_registers_t) == 0xc0) ? 1 : -1];

#define kMaxUnits	(8)
#define kIRCount	(512)
#define kIRPageCount	(atop(kIRCount * sizeof(ir_descriptor_t)))

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
struct vtd_unit_t
{
	ACPI_DMAR_HARDWARE_UNIT * dmar;
	volatile vtd_registers_t * regs;
	volatile vtd_iotlb_registers_t * iotlb;
	volatile vtd_fault_registers_t * faults;

	IOMemoryMap *     qi_map;
	qi_descriptor_t *qi_table;
	uint32_t *qi_table_stamps;

	uint64_t root;
	uint64_t msi_address;
	uint64_t qi_address;
	uint64_t qi_stamp_address;

	uint64_t ir_address;

	uint32_t qi_tail;
	uint32_t qi_mask;
	volatile uint32_t qi_stamp;
	uint32_t qi_stalled_stamp;

	uint32_t msi_data;
	uint32_t num_fault;
	uint32_t hwrounding;
	uint32_t rounding;
	uint32_t domains;

	uint8_t  global:1,
		 ig:1,
		 caching:1,
		 translating:1,
		 selective:1,
		 qi:1,
		 intmapper:1,
		 x2apic_mode:1;
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
typedef uint32_t vtd_vaddr_t;

union vtd_table_entry
{
	struct
	{
		uint     read:1	__attribute__ ((packed));
		uint     write:1	__attribute__ ((packed));
		uint     resv:10	__attribute__ ((packed));
		uint64_t addr:51	__attribute__ ((packed));
		uint     used:1	__attribute__ ((packed));
	} used;
	struct
	{
		uint access:2	__attribute__ ((packed));
		uint next:28	__attribute__ ((packed));
		uint prev:28	__attribute__ ((packed));
		uint size:5	__attribute__ ((packed));
		uint free:1	__attribute__ ((packed));
	} free;
	uint64_t bits;
};
typedef union vtd_table_entry vtd_table_entry_t;

typedef uint32_t vtd_rbaddr_t;

struct vtd_rblock
{
	RB_ENTRY(vtd_rblock) address_link;
	RB_ENTRY(vtd_rblock) size_link;

	vtd_rbaddr_t start;
	vtd_rbaddr_t end;
};

RB_HEAD(vtd_rbaddr_list, vtd_rblock);
RB_HEAD(vtd_rbsize_list, vtd_rblock);

struct vtd_bitmap_t
{
	uint32_t count;
	uint32_t bitmapwords;
	uint64_t bitmap[0];
};
typedef struct vtd_bitmap_t vtd_bitmap_t;

struct vtd_space_stats
{
	ppnum_t vsize;
	ppnum_t tables;
	ppnum_t bused;
	ppnum_t rused;
	ppnum_t largest_paging;
	ppnum_t largest_32b;
	ppnum_t inserts;
	ppnum_t max_inval[2];
	ppnum_t breakups;
	ppnum_t merges;
	ppnum_t allocs[64];
	ppnum_t bcounts[20];
};
typedef struct vtd_space_stats vtd_space_stats_t;

struct vtd_free_queued_t
{
	ppnum_t  addr;
	ppnum_t  size;
	uint32_t stamp[kMaxUnits];
};

enum
{
	kFreeQCount = 2,
	kFreeQElems = 256
};

struct vtd_space
{
	IOSimpleLock *      block;
	IOLock *            rlock;
	ppnum_t             vsize;
	ppnum_t             rsize;
	uint32_t            domain;
	vtd_bitmap_t *      table_bitmap;
	IOMemoryMap *       table_map;
	vtd_table_entry_t * tables[6];
	uint64_t            levels_wired;
	uint32_t            cachelinesize;
	ppnum_t             root_page;
	uint8_t             max_level;
	uint8_t             waiting_space;
	uint8_t             bheads_count;
	vtd_table_entry_t * bheads;
	
	vtd_space_stats_t   stats;
	
	vtd_free_queued_t   free_queue[kFreeQCount][kFreeQElems];
	volatile uint32_t   free_head[kFreeQCount];
	volatile uint32_t   free_tail[kFreeQCount];
	uint32_t            free_mask;

	uint32_t            rentries;
	struct vtd_rbaddr_list rbaddr_list;
	struct vtd_rbsize_list rbsize_list;
};
typedef struct vtd_space vtd_space_t;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
class AppleVTDDeviceMapper;

class AppleVTD : public IOMapper
{
	OSDeclareDefaultStructors(AppleVTD);

public:
	VTHWLOCKTYPE           * fHWLock;
	const OSData           * fDMARData;
	IOWorkLoop             * fWorkLoop;
	IOInterruptEventSource * fIntES;
	IOInterruptEventSource * fFaultES;
	IOTimerEventSource     * fTimerES;

	vtd_unit_t *      units[kMaxUnits];

	uint32_t          fTreeBits;
	uint32_t          fMaxRoundSize;
	uint64_t          fContextWidth;
	uint32_t          fCacheLineSize;
	uint32_t          fQIStamp[kMaxUnits];
	IOMemoryMap     * fTableMap;
	IOMemoryMap     * fGlobalContextMap;
	IOMemoryMap    ** fContextMaps;
	root_entry_t    * fRootEntryTable;
	ppnum_t           fRootEntryPage;
	uint32_t          fDomainSize;
	vtd_bitmap_t    * fDomainBitmap;
	vtd_space_t     * fSpace;
	IOMemoryMap     * fIRMap;
	uint64_t          fIRAddress;
	ir_descriptor_t * fIRTable;
	uint8_t           fDisabled;
	bool              x2apic_mode;

	static void install(IOWorkLoop * wl, uint32_t flags,
						IOService * provider, const OSData * data,
						IOPCIMessagedInterruptController  * messagedInterruptController);
	static void installInterrupts(void);

	static void adjustDevice(IOService * device);
	static void removeDevice(IOService * device);
	static void relocateDevice(IOService * device, bool paused);

	bool init(IOWorkLoop * wl, const OSData * data);

	virtual void free() APPLE_KEXT_OVERRIDE;
	virtual bool initHardware(IOService *provider) APPLE_KEXT_OVERRIDE;

	vtd_space_t * space_create(ppnum_t vsize, uint32_t buddybits, ppnum_t rsize);
	void          space_destroy(vtd_space_t * space);
	vtd_vaddr_t space_alloc(vtd_space_t * bf, vtd_vaddr_t addr, vtd_vaddr_t size,
							uint32_t mapOptions, const IODMAMapSpecification * mapSpecification,
							const upl_page_info_t * pageList);
	void space_free(vtd_space_t * bf, vtd_vaddr_t addr, vtd_vaddr_t size);
	void space_alloc_fixed(vtd_space_t * bf, vtd_vaddr_t addr, vtd_vaddr_t size);
	
	IOReturn handleInterrupt(IOInterruptEventSource * source, int count);
	IOReturn handleFault(IOInterruptEventSource * source, int count);
	IOReturn timer(OSObject * owner, IOTimerEventSource * sender);
	virtual IOReturn callPlatformFunction(const OSSymbol * functionName,
										  bool waitForFunction,
										  void * param1, void * param2,
										  void * param3, void * param4) APPLE_KEXT_OVERRIDE;

	void checkFree(vtd_space_t * space, uint32_t queue);
	void contextInvalidate(uint16_t domainID);
	void interruptInvalidate(uint16_t index, uint16_t count);

	IOReturn deviceMapperActivate(AppleVTDDeviceMapper * mapper, uint32_t options);
	IOReturn newContextPage(uint32_t idx);

	// { Space

	IOReturn spaceMapMemory(vtd_space_t                 * space,
							IOMemoryDescriptor          * memory,
							uint64_t                      descriptorOffset,
							uint64_t                      length,
							uint32_t                      mapOptions,
							const IODMAMapSpecification * mapSpecification,
							IODMACommand                * dmaCommand,
							const IODMAMapPageList      * pageList,
							uint64_t                    * mapAddress,
							uint64_t                    * mapLength);

	IOReturn spaceUnmapMemory(vtd_space_t * space,
							  IOMemoryDescriptor * memory,
							  IODMACommand * dmaCommand,
							  uint64_t mapAddress, uint64_t mapLength);

	IOReturn spaceInsert(vtd_space_t * space,
						 uint32_t mapOptions,
						 uint64_t mapAddress, uint64_t offset,
						 uint64_t physicalAddress, uint64_t length);

	uint64_t spaceMapToPhysicalAddress(vtd_space_t * space, uint64_t mappedAddress);

	// }
	// { IOMapper

	// Get the mapper page size
	virtual uint64_t getPageSize(void) const APPLE_KEXT_OVERRIDE;

	virtual IOReturn iovmMapMemory(IOMemoryDescriptor          * memory,
								   uint64_t                      descriptorOffset,
								   uint64_t                      length,
								   uint32_t                      mapOptions,
								   const IODMAMapSpecification * mapSpecification,
								   IODMACommand                * dmaCommand,
								   const IODMAMapPageList      * pageList,
								   uint64_t                    * mapAddress,
								   uint64_t                    * mapLength) APPLE_KEXT_OVERRIDE;
	
	virtual IOReturn iovmUnmapMemory(IOMemoryDescriptor * memory,
									 IODMACommand * dmaCommand,
									 uint64_t mapAddress, uint64_t mapLength) APPLE_KEXT_OVERRIDE;

	virtual IOReturn iovmInsert(
								uint32_t mapOptions,
								uint64_t mapAddress, uint64_t offset,
								uint64_t physicalAddress, uint64_t length) APPLE_KEXT_OVERRIDE;

	virtual uint64_t mapToPhysicalAddress(uint64_t mappedAddress) APPLE_KEXT_OVERRIDE;

	// }
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class AppleVTDDeviceMapper : public IOMapper
{
	OSDeclareDefaultStructors(AppleVTDDeviceMapper);

public:

	IOPCIDevice * fDevice;
	AppleVTD    * fVTD;
	vtd_space_t * fSpace;
	uint32_t      fSourceID;
	uint8_t       fAllFunctions;
    IOLock      * fAppleVTDforDeviceLock;
	ppnum_t       vsize;

	static AppleVTDDeviceMapper * forDevice(IOService * device, uint32_t flags);

	virtual void free() APPLE_KEXT_OVERRIDE;

	// { IOMapper

	virtual bool initHardware(IOService *provider) APPLE_KEXT_OVERRIDE;

	// Get the mapper page size
	virtual uint64_t getPageSize(void) const APPLE_KEXT_OVERRIDE;

	virtual IOReturn iovmMapMemory(IOMemoryDescriptor          * memory,
								   uint64_t                      descriptorOffset,
								   uint64_t                      length,
								   uint32_t                      mapOptions,
								   const IODMAMapSpecification * mapSpecification,
								   IODMACommand                * dmaCommand,
								   const IODMAMapPageList      * pageList,
								   uint64_t                    * mapAddress,
								   uint64_t                    * mapLength) APPLE_KEXT_OVERRIDE;

	virtual IOReturn iovmUnmapMemory(IOMemoryDescriptor * memory,
									 IODMACommand * dmaCommand,
									 uint64_t mapAddress, uint64_t mapLength) APPLE_KEXT_OVERRIDE;

	virtual IOReturn iovmInsert(
								uint32_t mapOptions,
								uint64_t mapAddress, uint64_t offset,
								uint64_t physicalAddress, uint64_t length) APPLE_KEXT_OVERRIDE;

	virtual uint64_t mapToPhysicalAddress(uint64_t mappedAddress) APPLE_KEXT_OVERRIDE;

	// }
};
