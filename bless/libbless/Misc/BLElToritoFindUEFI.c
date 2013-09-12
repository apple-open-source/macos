/*
 * Copyright (c) 2013 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/paths.h>
#include <errno.h>
#include <ctype.h>

#include <mach/mach.h>
#include <mach/mach_port.h>
#include <mach/mach_interface.h>
#include <mach/mach_init.h>

#include <CoreFoundation/CoreFoundation.h>
#include <syslog.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFSerialize.h>
#include <IOKit/storage/IODVDMedia.h>

#include "bless.h"
#include "bless_private.h"



// ISO 9660 CD "Primary Volume Descriptor" found at 2048-Sector #16:
//
typedef struct
{
	UInt8	volume_descriptor_type;                 //  type
	char	ident[5];                               //	characters 'CD001'
	UInt8	volume_descriptor_version;
	UInt8	unused1;
	char	system_id [32];
	char	volume_id [32];
	UInt8	unused2 [8];
	UInt32	volume_space_size_LE;                   //	provided in both endians
	UInt32	volume_space_size_BE;
	UInt8	other [1960];                           //	2048 bytes total
	
} __attribute__((packed)) ISO9660_PRIMARY_VOLUME_DESCRIPTOR;

// "El Torito" optical disc identification standard "Boot Record Volume Descriptor" found at 2048-Sector #17:
//
typedef struct
{
	UInt8	type;
	char    ident [5];
	UInt8   version;
	char    system_id [32];
	char    unused1 [32];
	UInt32  bootcat_ptr;
} __attribute__((packed)) EL_TORITO_BOOT_VOLUME_DESCRIPTOR;

typedef struct                                      //	32 Byte El Torito Validation Entry
{
	UInt8	id;                                     //	Header ID				= 1
	UInt8	arch;                                   //	Platform Architecture	= x86, ppc
	unsigned : 16;                                  //	Reserved				= 0
	char	creator_id [24];                        //	Creator Identity
	UInt16	checksum;                               //	Word sum				= 0
	UInt8	key55;                                  //	Key, must be 0x55
	UInt8	keyAA;                                  //	Key, must be 0xaa
} __attribute__((packed)) EL_TORITO_VALIDATION_ENTRY;

typedef struct                                      //	32 Byte Section + Initial/Default Entry
{
	UInt8	boot_indicator;                         //	Boot Indicator 			= 0x00 | 0x88
	UInt8	boot_media;                             //	Boot Emulation mode
	UInt16	load_segment;                           //	Load address			= 0x0000
	UInt8	system_type;                            //	MBR/PBR System Type		= E.G. 0xAF for Darwin_HFS
	UInt8	: 8;                                    //	Reserved				= 0x00
	UInt16	blockcount;                             //	Load size
	UInt32	lba;                                    //	Virtual Disk Address
                                                    //	Extra Section entries…
	UInt8	: 8;                                    //	Reserved				= 0x00
} __attribute__((packed)) EL_TORITO_INITIAL_DEFAULT_ENTRY;

typedef struct
{
	UInt8	header_indicator;                       //	Header Indicator = 0x90 (more headers follow), 0x91 (this is last)
	UInt8	platform_id;                            //	Platform ID	0=80x86; 1=PowerPC; 2=Mac; 0xEF=EFI
	UInt16	sections_count;                         //	Number of sections following this section header
	char	section_id [28];                        //	ID string; should be checkd by bios and boot software.
} __attribute__((packed)) EL_TORITO_SECTION_HEADER_ENTRY;

typedef struct
{
	UInt8	boot_indicator;                         //	0x88=bootable; 0x00=not bootable
	UInt8	boot_media;                             //	b0:b3=0..f,4=hard drive; b4=0; b5=continuation entry follows; b6=ATAPI driver incl; b7=SCSI drvr
	UInt16	load_segment;                           //	Load segment for the initial boot image
	UInt8	system_type;                            //	Must be a copy of byte 5 from the Partition Table found in the boot image
	UInt8	unused1;                                //	Must be 0
	UInt16	sector_count;                           //	Number of emulated sectors the system wil store at Load Segment during boot
	UInt32	load_rba;                               //	Start address of the virtual disk
	UInt8	selection_criteria_type;                //	What info follows in next field: 0=none, 1=Language&Version, 2..255=reserved
	UInt8	selection_criteria [19];                //	Vendor unique selection criteria
} __attribute__((packed)) EL_TORITO_SECTION_SECTION_ENTRY;

typedef struct
{
	UInt8	extension_indicator;                    //	Must be 0x44
	UInt8	bits;                                   //	b5=ExtensionRecordFollows; other bits unused
	UInt8	other[30];                              //	Vendor uqniue extra bytes
} __attribute__((packed)) EL_TORITO_SECTION_EXTENSION_ENTRY;



static void contextprintfhexdump16bytes (BLContextPtr inContext, int inLogLevel, char* inHeaderStr, uint8_t* inBytes)
{
    int i;
    
    contextprintf (inContext, inLogLevel, "%s", inHeaderStr);
    
    for (i = 0;   i <= 15;   i++) {
        contextprintf (inContext, inLogLevel, "%02x ", inBytes[i]);
    }
    
    contextprintf (inContext, inLogLevel, "| ");
    
    for (i = 0;   i <= 15;   i++) {
        contextprintf (inContext, inLogLevel, "%c", inBytes[i]);
    }
    
    contextprintf (inContext, inLogLevel, "\n");
}



//
// This routine opens the given device (which this assumes to be a disc) and looks for an ElTorito Boot Catalog
// and then looks for the first Entry which is set as bootable and as type of 0xEF (EFI). It will set outFoundIt
// to report if it found it or not; if so, then outOffsetBlocks and outSizeBlocks point to the MS-DOS region.
//
static void findMSDOSRegion (BLContextPtr inContext, const char* inBSDName, bool* outFoundIt, uint32_t* outOffsetBlocks, uint32_t* outSizeBlocks)
{
	bool					foundIt = false;
	int                     fd = -1;
	char					devPath [256];
	uint8_t 				buf2048 [2500];
	int                     sectionEntryIteratorForCurrentHeader = 0;
    
    // -------------------------------------------------------------------------------------------------
    // START OF TEST MODE; THIS IS A PLAYGROUND TO VERIFY CORRECT PARSING OPERATION
    // -------------------------------------------------------------------------------------------------
    
#define TESTMODE 0
    
#if TESTMODE
    
	char tbuf [2500];
    
	contextprintf (inContext, kBLLogLevelVerbose, "******** TESTING, WRITING TO STAGING AREA THEN READING AS IF IT WERE A PRETEND DISC.. *******\n");
    
    sprintf (devPath, "/tmp/testFindMSDOSRegion");
	fd = open (devPath, O_RDWR | O_CREAT);
	if (-1 == fd)
	{
		contextprintf (inContext, kBLLogLevelVerbose, "unable to open, errno=%d\n", errno);
		goto Exit;
	}
	contextprintf (inContext, kBLLogLevelVerbose, "opened pretend DVD for read/write\n");
    
	// ISO Primary Volume Descriptor at 2048-block #16
	bzero (tbuf, 2048);
	tbuf[0] = 0x01;
	strcpy (&(tbuf[1]), "CD001");
	tbuf[80]=0x33; tbuf[81]=0x22; tbuf[82]=0x11; tbuf[83]=0x00;
	pwrite (fd, tbuf, 2048, 2048*16);
    
	// "El Torito" Voume Descriptor at 2048-block #17
	bzero (tbuf, 2048);
	tbuf[0] = 0x00;
	strcpy (&(tbuf[1]), "CD001");
	tbuf[71]=0x20; tbuf[72]=0x00; tbuf[73]=0x00; tbuf[74]=0x00; //bootcat_ptr = first sector of Boot Catalog -- make it #32
	pwrite (fd, tbuf, 2048, 2048*17);
	
	// Boot Catalog Starts At #32 ... the Entries ... Entries are 0x20=32 bytes long each.  All in the one sector.
	bzero (tbuf, 2048);
    
	int en=0;
    
	// Validation Entry
	tbuf[en+0] = 0x01;
	tbuf[en+0x1e]=0x55;  tbuf[en+0x1f]=0xaa;
    
	en += 32;
    
	// Initial Default Entry
	tbuf[en+0] = 0x88;
	tbuf[en+0x08]=0x40;  tbuf[en+0x09]=0x00;  tbuf[en+0x0a]=0x00;  tbuf[en+0x0b]=0x00;  // LOAD RBA --- make it #64
    
	en +=32;
    
	// Section Header Entry
	tbuf[en+0]=0x90;
	tbuf[en+1]=0x33;
	tbuf[en+2]=0;  // just kidding this should never happen but test for it... NO sections to follow header!
	
	en += 32;
	
	// Section Header Entry
	tbuf[en+0]=0x90;
	tbuf[en+1]=0x34;
	tbuf[en+2]=2;  // two section entries for this header
	
	en += 32;
    
    // Section Entry 1of2
    tbuf[en+0]=0x00;
    tbuf[en+1]=0x20;  // b5=continuation entry follows YES
    tbuf[en+0x08]=0x40;  tbuf[en+0x09]=0x30;  tbuf[en+0x0a]=0x00;  tbuf[en+0x0b]=0x00;  // LOAD RBA
    
	en += 32;
    
    //Continuation Entry first
    tbuf[en+0]=0x44;
    tbuf[en+1]=0x20;   //b5=if more
    
    en += 32;
    
    //Continuation Entry second
    tbuf[en+0]=0x44;
    tbuf[en+1]=0x00;   //b5=if more
    
    en += 32;
    
    // Section Entry 2of2
    tbuf[en+0]=0x00;
    tbuf[en+1]=0x20;  // b5=continuation entry follows YES
    tbuf[en+0x08]=0x40;  tbuf[en+0x09]=0x30;  tbuf[en+0x0a]=0x20;  tbuf[en+0x0b]=0x00;  // LOAD RBA
    
	en += 32;
    
    //Continuation Entry first
    tbuf[en+0]=0x44;
    tbuf[en+1]=0x00;   //b5=if more
    
	en += 32;
    
	// Section Header Entry
	tbuf[en+0]=0x90;
	tbuf[en+1]=0x35;
	tbuf[en+2]=3;
    
	en += 32;
    
    // Section Entry 1of3
    tbuf[en+0]=0x00;
    tbuf[en+1]=0x00;  // b5=continuation entry follows NO
    tbuf[en+0x08]=0x41;  tbuf[en+0x09]=0x30;  tbuf[en+0x0a]=0x20;  tbuf[en+0x0b]=0x00;  // LOAD RBA
    
	en += 32;
    
    // Section Entry 2of3
    tbuf[en+0]=0x88;
    tbuf[en+1]=0x00;  // b5=continuation entry follows NO
    tbuf[en+0x08]=0x42;  tbuf[en+0x09]=0x30;  tbuf[en+0x0a]=0x20;  tbuf[en+0x0b]=0x00;  // LOAD RBA
    
	en += 32;
    
    // Section Entry 3of3
    tbuf[en+0]=0x88;
    tbuf[en+1]=0x00;  // b5=continuation entry follows NO
    tbuf[en+0x08]=0x43;  tbuf[en+0x09]=0x30;  tbuf[en+0x0a]=0x20;  tbuf[en+0x0b]=0x00;  // LOAD RBA
    
	en += 32;
    
	// Section Header
	tbuf[en+0]=0x90;
	tbuf[en+1]=0xEF;
	tbuf[en+2]=2;
    
	en += 32;
    
    // Section Entry 1of2
    tbuf[en+0]=0x00;
    tbuf[en+1]=0x00;  // b5=continuation entry follows NO
    tbuf[en+0x08]=0x42;  tbuf[en+0x09]=0x30;  tbuf[en+0x0a]=0x20;  tbuf[en+0x0b]=0x00;  // LOAD RBA
    
	en += 32;
    
    // Section Entry 2of2
    tbuf[en+0]=0x88;
    tbuf[en+1]=0x00;  // b5=continuation entry follows NO
    tbuf[en+0x08]=0x45;  tbuf[en+0x09]=0x35;  tbuf[en+0x0a]=0x25;  tbuf[en+0x0b]=0x00;  // LOAD RBA
	
	en += 32;
    
	// Section Header
	tbuf[en+0]=0x91;
	tbuf[en+1]=0xEF;
	tbuf[en+2]=1;
    
    // Section Entry 1of1
    tbuf[en+0]=0x88;
    tbuf[en+1]=0x00;  // b5=continuation entry follows NO
    tbuf[en+0x08]=0x95;  tbuf[en+0x09]=0x95;  tbuf[en+0x0a]=0x25;  tbuf[en+0x0b]=0x00;  // LOAD RBA
    
	// write out all Entries
	pwrite (fd, tbuf, 2048, 2048*32);
    
	close (fd);
	contextprintf (inContext, kBLLogLevelVerbose, "closed pretend DVD for read/write\n");
    
#endif
    
    // -------------------------------------------------------------------------------------------------
    // END OF TEST MODE; PARSING STARTS BELOW
    // -------------------------------------------------------------------------------------------------
    
    contextprintf (inContext, kBLLogLevelVerbose, "******** PARSING STARTS on %s *******\n", inBSDName);
    
#if TESTMODE
    sprintf (devPath, "/tmp/testFindMSDOSRegion");
#else
	sprintf (devPath, "/dev/r%s", inBSDName);
#endif
    
	fd = open (devPath, O_RDONLY | O_SHLOCK);
	if (-1 == fd)
	{
		contextprintf (inContext, kBLLogLevelVerbose, "unable to open, errno=%d\n", errno);
		goto Exit;
	}
	contextprintf (inContext, kBLLogLevelVerbose, "opened DVD for shared reading\n");
    
#if TESTMODE
	// Show something at the very beginning of disc:
	bzero (buf2048, 2048);
	pread (fd, buf2048, 1*2048, 0);
    contextprintfhexdump16bytes (inContext, kBLLogLevelVerbose, "disc[0*2048]            ", buf2048);
#endif
    
	// Read this ISO "Descriptor" of disc: 2048-Sector #16:
	bzero (buf2048, 2048);
	pread (fd, buf2048, 1*2048, 16*2048);
    contextprintfhexdump16bytes (inContext, kBLLogLevelVerbose, "disc[16*2048]           ", buf2048);
    contextprintfhexdump16bytes (inContext, kBLLogLevelVerbose, "buf2048[16*2048 + 80]   ", &(buf2048[80]));
	ISO9660_PRIMARY_VOLUME_DESCRIPTOR *   vd = (ISO9660_PRIMARY_VOLUME_DESCRIPTOR *) buf2048;
    
	// See if this is a Primary Volume Descriptor type of ISO9660 Volume Descriptor and is valid:
	if ((0 == memcmp (vd->ident, "CD001", sizeof (vd->ident)))		&&      // verify the Standard Identifer signature to be valid
		(1 == vd->volume_descriptor_type)				)       // verify this volume descriptor's Type to be 1=PrimaryVolumeDescriptor
	{
		contextprintf (inContext, kBLLogLevelVerbose, "Primary Volume Descriptor confirmed\n");
	} else {
		contextprintf (inContext, kBLLogLevelVerbose, "Primary Volume Descriptor not found\n");
		goto Exit;
	}
    
	// Get this field out:
    uint32_t volumeSpaceSize = OSSwapLittleToHostInt32(vd->volume_space_size_LE);
	contextprintf (inContext, kBLLogLevelVerbose, " .volumeSpaceSize=(in 2048-blocks)=0x%08x\n", volumeSpaceSize);
    
	// Read this ISO "Descriptor" of disc: 2048-Sector #17
	// This is the "ELTORITO header" ... it is a certain type of ISO9660VolumeDescriptor: a Boot Record volume descriptor.
	bzero (buf2048, 2048);
	pread (fd, buf2048, 1*2048, 17*2048);
	contextprintfhexdump16bytes (inContext, kBLLogLevelVerbose, "\n\ndisc[17*2048]       ", buf2048);
	EL_TORITO_BOOT_VOLUME_DESCRIPTOR *		bvd = (EL_TORITO_BOOT_VOLUME_DESCRIPTOR *) buf2048;
    
	// Read buffer at where ElTorito header should be and interpret buffer as El Torito header & check:
	pread (fd, buf2048, 1*2048, 17*2048);
	contextprintf (inContext, kBLLogLevelVerbose, "got putative EL_TORITO_BOOT_VOLUME_DESCRIPTOR:\n");
	contextprintf (inContext, kBLLogLevelVerbose, " (Boot Record Volume Descriptor)\n");
	contextprintf (inContext, kBLLogLevelVerbose, " .type=%d=0x%02x\n", bvd->type, bvd->type);
	contextprintf (inContext, kBLLogLevelVerbose, " .ident=%.5s\n", bvd->ident);
	contextprintf (inContext, kBLLogLevelVerbose, " .version=%d=0x%02x\n", bvd->version, bvd->version);
	contextprintf (inContext, kBLLogLevelVerbose, " .system_id=%.31s\n", bvd->system_id);
	contextprintf (inContext, kBLLogLevelVerbose, " .unused1=%.31s\n", bvd->unused1);
	contextprintf (inContext, kBLLogLevelVerbose, " .bootcat_ptr=HE0x%08x=LE0x%08x=LE%d\n", (int) bvd->bootcat_ptr, OSSwapLittleToHostInt32 (bvd->bootcat_ptr), OSSwapLittleToHostInt32 (bvd->bootcat_ptr));
	
	// See if this is an El Torito "Boot Record Volume Descriptor":
	if ((0 == memcmp (bvd->ident, "CD001", sizeof (bvd->ident))) &&     // verify the Standard Identifer signature to be valid
		(0 == bvd->type) &&                                             // verify this volume descriptor's Type to be 0=BootRecord
		(0 != OSSwapLittleToHostInt32 (bvd->bootcat_ptr))) {            // verify that the boot cat ptr is something nonzero
		contextprintf (inContext, kBLLogLevelVerbose, "Boot Record Volume Descriptor (El Torito header) confirmed\n");
	} else {
		contextprintf (inContext, kBLLogLevelVerbose, "Boot Record Volume Descriptor (El Torito header) not found\n");
		goto Exit;
	}
    
	// Get this field out:
	int firstSectorOfBootCatalog = OSSwapLittleToHostInt32 (bvd->bootcat_ptr);
	contextprintf (inContext, kBLLogLevelVerbose, "firstSectorOfBootCatalog (in 2048-sectors) = %d = 0x%08x\n", firstSectorOfBootCatalog, firstSectorOfBootCatalog);
    
    //
	// Now we will read 2048 bytes' worth of Entries.
	// each Entry is 32=0x20 bytes long.
    //
	// There can be up to 64 Entries per 2048-Sector, and there can be multiple 2048-Sectors in use for these Entries.
	// (Since it takes 2-3 Entries to make up a bootable entry, that makes for about 16 bootble entries per sector.)
	// We will limit our search to just the first 2048-Sector's worth of entries.
	// This is known as the Boot Catalog.
    //
	//  It starts with 1 Validation Entry.
	//  Then 1 Initial/Default Entry.
	//  Then one or more of:
	//     A Section Header Entry. Then zero or more of:
	//        Its Section Entry. Then zero or more of:
	//           This Section Entry's optional Section Extension Entry.
    //
    
	// Read buffer where the the first 64 Entries are:
	int ret;
	bzero (buf2048, 2048);
	ret = pread (fd, buf2048, 1*2048, firstSectorOfBootCatalog*2048);
	contextprintf (inContext, kBLLogLevelVerbose, "\n\npread 2048-buff of Entries; ret=%d\n", ret);
    
	uint8_t entryBuf [32];
	int entryNum = 0;
	
	// VALIDATION ENTRY
    
	bzero (entryBuf, 32);
	memcpy (entryBuf, &(buf2048[entryNum*32]), 32);
    
	contextprintf (inContext, kBLLogLevelVerbose, "\n\nVALIDATION ENTRY\n");
	contextprintfhexdump16bytes (inContext, kBLLogLevelVerbose, "     ", entryBuf);
	EL_TORITO_VALIDATION_ENTRY *		ve = (EL_TORITO_VALIDATION_ENTRY *) entryBuf;
    
	// interpret buffer read above as Validation Entry
	contextprintf (inContext, kBLLogLevelVerbose, "got putative EL_TORITO_VALIDATION_ENTRY:\n");
	contextprintf (inContext, kBLLogLevelVerbose, " .headerid=%d=0x%02x\n", ve->id, ve->id);
	contextprintf (inContext, kBLLogLevelVerbose, " .arch=%d=0x%02x\n", ve->arch, ve->arch);
	contextprintf (inContext, kBLLogLevelVerbose, " .creatorid=%.24s\n", ve->creator_id);
	contextprintf (inContext, kBLLogLevelVerbose, " .checksum=%d=0x%04x\n", ve->checksum, ve->checksum);
	contextprintf (inContext, kBLLogLevelVerbose, " .key55=%d=0x%02x\n", ve->key55, ve->key55);
	contextprintf (inContext, kBLLogLevelVerbose, " .keyAA=%d=0x%02x\n", ve->keyAA, ve->keyAA);
	
	// See if this is a good Validation Entry. We could compare a whole lot of details here,
	// but why limit it to e.g. Microsoft? So just verify the "must be 01". Really should
	// verify the checksum check here.
	//
	if ((1    == ve->id) &&
	    (0x55 == ve->key55) &&
	    (0xaa == ve->keyAA))
	{
		contextprintf (inContext, kBLLogLevelVerbose, "Validation Entry confirmed\n");
	} else {
		contextprintf (inContext, kBLLogLevelVerbose, "Validation Entry not found\n");
		goto Exit;
	}
    
    // -----
    
	entryNum++;
    
    // -----
	
	// INITIAL/DEFAULT ENTRY
    
	bzero (entryBuf, 32);
	memcpy (entryBuf, &(buf2048[entryNum*32]), 32);
	contextprintf (inContext, kBLLogLevelVerbose, "\n\nINITIAL/DEFAULT ENTRY\n");
	contextprintfhexdump16bytes (inContext, kBLLogLevelVerbose, "     ", entryBuf);
    
	EL_TORITO_INITIAL_DEFAULT_ENTRY *   de = (EL_TORITO_INITIAL_DEFAULT_ENTRY *) entryBuf;
    
	// interpret buffer copied above as the Initial/Default Entry (a special case of a "Section Entry")
	contextprintf (inContext, kBLLogLevelVerbose, "got putative EL_TORITO_INITIAL_DEFAULT_ENTRY:\n");
	contextprintf (inContext, kBLLogLevelVerbose, " .bootindicator=%d=0x%02x\n", de->boot_indicator, de->boot_indicator);
	contextprintf (inContext, kBLLogLevelVerbose, " .bootmedia=%d=0x%02x\n", de->boot_media, de->boot_media);
	contextprintf (inContext, kBLLogLevelVerbose, " .loadsegment=%d=0x%04x\n", de->load_segment, de->load_segment);
	contextprintf (inContext, kBLLogLevelVerbose, " .systemtype=%d=0x%02x\n", de->system_type, de->system_type);
	contextprintf (inContext, kBLLogLevelVerbose, " .blockcount=%d=0x%04x\n", de->blockcount, de->blockcount);
	contextprintf (inContext, kBLLogLevelVerbose, " .lba=%d=0x%08x\n", (int) de->lba, (int) de->lba);
	// we don't really care too much what's in this entry
    
    // -----
    
	entryNum++;
    
    // -----
    
	while (1)
	{
		// a SECTION.HEADER ENTRY
        
		bzero (entryBuf, 32);
		memcpy (entryBuf, &(buf2048[entryNum*32]), 32);
		contextprintf (inContext, kBLLogLevelVerbose, "\n\nSECTION HEADER ENTRY\n");
        contextprintfhexdump16bytes (inContext, kBLLogLevelVerbose, "     ", entryBuf);
        
		EL_TORITO_SECTION_HEADER_ENTRY *   he = (EL_TORITO_SECTION_HEADER_ENTRY *) entryBuf;
        
		contextprintf (inContext, kBLLogLevelVerbose, "got putative EL_TORITO_SECTION_HEADER_ENTRY:\n");
		contextprintf (inContext, kBLLogLevelVerbose, " .headerindicator=%d=0x%02x\n", he->header_indicator, he->header_indicator);
		contextprintf (inContext, kBLLogLevelVerbose, " .platformid=%d=0x%02x\n", he->platform_id, he->platform_id);
		contextprintf (inContext, kBLLogLevelVerbose, " .sectionscount=%d=0x%04x\n", he->sections_count, he->sections_count);
		contextprintf (inContext, kBLLogLevelVerbose, " .sectionid=%.28s\n", he->section_id);
        
		// header_indicator: look for 91 that means end or 90 that means continue
		// platform_id:look for EF that means EFI.
        
		if ((he->header_indicator != 0x90) && (he->header_indicator != 0x91))
		{
			contextprintf (inContext, kBLLogLevelVerbose, "invalid Section Header; stopping\n");
			goto Exit;
		}
        
		bool isFinalHeader = (0x91 == he->header_indicator);
		contextprintf (inContext, kBLLogLevelVerbose, "isFinalHeader=%d\n", isFinalHeader);
        
		bool isEFIPlatformHeader = (0xef == he->platform_id);
		contextprintf (inContext, kBLLogLevelVerbose, "isEFIPlatformHeader=%d\n", isEFIPlatformHeader);
        
		UInt16 sectionEntriesForCurrentHeader = OSSwapLittleToHostInt16 (he->sections_count);
		contextprintf (inContext, kBLLogLevelVerbose, "sectionEntriesForCurrentHeader=%d\n", sectionEntriesForCurrentHeader);
        
        // -----
        
		entryNum++;
		if (entryNum > 63) goto TooManyEntries;
        
        // -----
        
		if (0 == sectionEntriesForCurrentHeader) goto DoneSectionEntriesForCurrentHeader;
        
		sectionEntryIteratorForCurrentHeader = 0;
		while (1)
		{
			// a SECTION.SECTION ENTRY
            
			contextprintf (inContext, kBLLogLevelVerbose, "processing section.section entry #%d for current header\n", sectionEntryIteratorForCurrentHeader);
            
			bzero (entryBuf, 32);
			memcpy (entryBuf, &(buf2048[entryNum*32]), 32);
			contextprintf (inContext, kBLLogLevelVerbose, "\n\nSECTION SECTION ENTRY\n");
            contextprintfhexdump16bytes (inContext, kBLLogLevelVerbose, "     ", entryBuf);
            
			EL_TORITO_SECTION_SECTION_ENTRY *   se = (EL_TORITO_SECTION_SECTION_ENTRY *) entryBuf;
            
			contextprintf (inContext, kBLLogLevelVerbose, "got putative EL_TORITO_SECTION_SECTION_ENTRY:\n");
			contextprintf (inContext, kBLLogLevelVerbose, " .boot_indicator=(0x88=bootable)=%d=0x%02x\n", se->boot_indicator, se->boot_indicator);
			contextprintf (inContext, kBLLogLevelVerbose, " .boot_media (b5=ContinuationEntriesFollow)=0x%02x\n", se->boot_media);
			contextprintf (inContext, kBLLogLevelVerbose, " .load_rba=HE%d=HE0x%08x=LE%d=LE0x%08x\n", (int) se->load_rba, (int) se->load_rba, OSSwapLittleToHostInt32(se->load_rba), OSSwapLittleToHostInt32(se->load_rba));
            
			bool bootableSectionEntry = (0x88 == se->boot_indicator);
			contextprintf (inContext, kBLLogLevelVerbose, "isBootableSectionEntry=%d\n", bootableSectionEntry);
            
			bool sectionEntryHasExtensions = (0 != ((se->boot_media) & 0x20));
			contextprintf (inContext, kBLLogLevelVerbose, "sectionEntryHasExtensions=%d\n", sectionEntryHasExtensions);
            
			// if we found what we need, stop the considering-sections-loop:
			//
			if (isEFIPlatformHeader && bootableSectionEntry)
			{
				*outSizeBlocks = volumeSpaceSize - OSSwapLittleToHostInt32 (se->load_rba);      // in device blocks. 1stISORecord.VolumeSpaceSize - SectionEntry.LoadRBA
				*outOffsetBlocks = OSSwapLittleToHostInt32 (se->load_rba);                      // in device blocks.
				foundIt = true;
				goto StopSearch;
			}
            
			// if there is no more, stop the considering-sections-loop:
			//
			if (isFinalHeader)
			{
				goto StopSearch;
			}
            
            // -----
            
			entryNum++;
			if (entryNum > 63) goto TooManyEntries;
            
            // -----
            
			// OPTIONAL SECTION ENTRY EXTENSION(s) (if bit in section entry above is set)
            
			if (sectionEntryHasExtensions)
			{
				while (1)
				{
                    
					bzero (entryBuf, 32);
					memcpy (entryBuf, &(buf2048[entryNum*32]), 32);
					contextprintf (inContext, kBLLogLevelVerbose, "\n\nSECTION EXTENSION ENTRY\n");
                    contextprintfhexdump16bytes (inContext, kBLLogLevelVerbose, "     ", entryBuf);
                    
					EL_TORITO_SECTION_EXTENSION_ENTRY *   see = (EL_TORITO_SECTION_EXTENSION_ENTRY *) entryBuf;
                    
					contextprintf (inContext, kBLLogLevelVerbose, "got putative EL_TORITO_SECTION_EXTENSION_ENTRY:\n");
					contextprintf (inContext, kBLLogLevelVerbose, " .extension_indicator=(0x44=valid)=%d=0x%02x\n", see->extension_indicator, see->extension_indicator);
					contextprintf (inContext, kBLLogLevelVerbose, " .bits(b5=ContinuationEntriesFollow)=%d\n", see->bits);
                    
                    
					bool extensionsEntryHasMoreExtensions = (0 != ((see->bits) & 0x20));
					contextprintf (inContext, kBLLogLevelVerbose, "extensionsEntryaHasMoreExtensions=%d\n", extensionsEntryHasMoreExtensions);
                    
                    // -----

					entryNum++;
					if (entryNum > 63) goto TooManyEntries;
                    
                    // -----
                    
					if (false == extensionsEntryHasMoreExtensions)
					{
						break;
					}
				}
			}
            
			sectionEntryIteratorForCurrentHeader++;
			if (sectionEntryIteratorForCurrentHeader >= sectionEntriesForCurrentHeader)
			{
				contextprintf (inContext, kBLLogLevelVerbose, "no more sections for this section header\n");
				break;
			}
		}
        
        DoneSectionEntriesForCurrentHeader:;
        
		// AND THEN WE KEEP GOING WITH {SECTION HEADER, SECTION, (SECTION EXTENSION)}* UNTIL WE HIT header indicator=91 (90 means more coming)
	}
    
    StopSearch:;
    
	if (false == foundIt)
	{
		contextprintf (inContext, kBLLogLevelVerbose, "Section Header Entry with EFI as a PlatformID not found.\n");
		goto Exit;
	}
    
#if TESTMODE
	// Show something at where the ElTorito->BootCatalog->InitialDefault->MSDOS is supposed to be:
	bzero (buf2048, 2048);
	uint32_t byteOff = (*outOffsetBlocks)*2048;
	pread (fd, buf2048, 1*2048, byteOff);
    contextprintfhexdump16bytes (inContext, kBLLogLevelVerbose, "\n\n disc[%d x 2048=(%d)]     ", buf2048);
#endif
    
    // Success:;
	goto Exit;
    
    TooManyEntries:;
    contextprintf (inContext, kBLLogLevelVerbose, "More than 32 entries in the 2048-sector = the BootingCatalog sector.\n");
	goto Exit;
    
    Exit:;
	if (-1 != fd) close (fd);
	contextprintf (inContext, kBLLogLevelVerbose, "Closed DVD; FoundTheMSDOSRegion=%d\n", foundIt);
    *outFoundIt = foundIt;
}



static bool isPreBootEnvironmentUEFIWindowsBootCapable (BLContextPtr inContext)
{
    bool                    ret = false;
    
    io_registry_entry_t     optionsNode = IO_OBJECT_NULL;
	CFTypeRef				featureMaskDataRef = NULL;
	bool					featureMaskExists = false;
 	uint32_t				featureMaskValue = 0;
	CFTypeRef				featureFlagsDataRef = NULL;
	bool					featureFlagsExists = false;
	uint32_t				featureFlagsValue = 0;
    bool                    hasNVRAM_UEFIWindowsBootCapable = false;
    
    const uint32_t			kWindowsUEFIBootSupport = 0x20000000;
    
    // Get boot ROM capabilities that are cached in IOReg. See if it has what we're seeking.
	// Inability to get certain basic info is grounds for hard error, though:
	//
	optionsNode = IORegistryEntryFromPath (kIOMasterPortDefault, kIODeviceTreePlane ":/options");
	if (IO_OBJECT_NULL == optionsNode) goto Exit;
    
	featureMaskDataRef = IORegistryEntryCreateCFProperty (optionsNode,
                                                          CFSTR("4D1EDE05-38C7-4A6A-9CC6-4BCCA8B38C14:FirmwareFeaturesMask"),
                                                          kCFAllocatorDefault,
                                                          0);
    if (NULL != featureMaskDataRef)
    {
        if ((CFGetTypeID (featureMaskDataRef) == CFDataGetTypeID ()) &&
            (CFDataGetLength (featureMaskDataRef) == sizeof (uint32_t)))
        {
            const UInt8	* bytes = CFDataGetBytePtr (featureMaskDataRef);
            featureMaskValue = CFSwapInt32LittleToHost (*(uint32_t *)bytes);
            featureMaskExists = true;
            contextprintf (inContext, kBLLogLevelVerbose, "found ioreg \"FirmwareFeaturesMask\"; featureMaskValue=0x%08X\n", featureMaskValue);
        }
        else
        {
            contextprintf (inContext, kBLLogLevelVerbose, "ioreg \"FirmwareFeaturesMask\" has unexpected type\n");
        }
        CFRelease (featureMaskDataRef);
    }
    else
    {
        contextprintf (inContext, kBLLogLevelVerbose, "did not find ioreg \"FirmwareFeaturesMask\"\n");
    }
    
	featureFlagsDataRef = IORegistryEntryCreateCFProperty (optionsNode,
														   CFSTR("4D1EDE05-38C7-4A6A-9CC6-4BCCA8B38C14:FirmwareFeatures"),
														   kCFAllocatorDefault,
														   0);
    if (NULL != featureFlagsDataRef)
    {
        if ((CFGetTypeID (featureFlagsDataRef) == CFDataGetTypeID ()) &&
            (CFDataGetLength (featureFlagsDataRef) == sizeof (uint32_t)))
        {
            const UInt8	* bytes = CFDataGetBytePtr (featureFlagsDataRef);
            featureFlagsValue = CFSwapInt32LittleToHost (*(uint32_t *)bytes);
            featureFlagsExists = true;
            contextprintf (inContext, kBLLogLevelVerbose, "found ioreg \"FirmwareFeatures\"; featureFlagsValue=0x%08X\n", featureFlagsValue);
        }
        else
        {
            contextprintf (inContext, kBLLogLevelVerbose, "ioreg \"FirmwareFeatures\" has unexpected type\n");
        }
        CFRelease (featureFlagsDataRef);
    }
    else
    {
        contextprintf (inContext, kBLLogLevelVerbose, "did not find ioreg \"FirmwareFeatures\"\n");
    }
	
	// FIRST CHANCE: See if the ioreg properties existed if found above, and if so, do they indicate support.
	//               And exit now if support found, else drop through to other less canonical methods.
	//               So this will mean that 1) ioreg has the bits, 2) if the ROM publishes in mask, 3) final yes/no.
	//
	if (featureMaskExists && (0 != (featureMaskValue & kWindowsUEFIBootSupport)) &&
		featureFlagsExists && (0 != (featureFlagsValue & kWindowsUEFIBootSupport)))
	{
		ret = true;
		goto Exit;
	}
    
    // SECOND CHANCE: See the property "UEFIWindowsBootCapable" is present and set to 1 or "1".
    // This can be used to debug bless even on machines that don't have newer firmware, with the command
    // `nvram UEFIWindowsBootCapable=1`; see also `nvram -p`.
    //
    CFMutableDictionaryRef      matchForAppleEFINVRAMNode = IOServiceMatching ("AppleEFINVRAM");
    io_service_t                appleEFINVRAMNode = IOServiceGetMatchingService (kIOMasterPortDefault, matchForAppleEFINVRAMNode); // consumes match input
    
    if (0 != appleEFINVRAMNode)
    {
        CFDataRef   prop = IORegistryEntryCreateCFProperty (appleEFINVRAMNode, CFSTR("UEFIWindowsBootCapable"), kCFAllocatorDefault, 0);
        if (NULL != prop)
        {
            CFIndex propLen = CFDataGetLength (prop);
            if (1 == propLen)
            {        
                const uint8_t *   propDataPtr = CFDataGetBytePtr (prop);
                if ((0x31 == propDataPtr[0]) || (0x01 == propDataPtr[0]))
                    hasNVRAM_UEFIWindowsBootCapable = true;
            }
            CFRelease (prop);
        }
    }
    
    if (hasNVRAM_UEFIWindowsBootCapable)
    {
        contextprintf (inContext, kBLLogLevelVerbose, "NVRAM variable \"UEFIWindowsBootCapable\" is set\n");
        ret = true;
        goto Exit;
    }
    
    Exit:;
    contextprintf (inContext, kBLLogLevelVerbose, "isPreBootEnvironmentUEFIWindowsBootCapable=%d\n", ret);
    return ret;
}



//
// This routine makes sure we have a UEFI-booting-capable EFI ROM (preboot environment); if not it returns FALSE.
// It then takes the given BSD dev node disk and sees if it is a DVD. If it is, then it opens it and looks for an
// El Torito Boot Catalog; if it finds that then it looks for the first bootable MS-DOS region; if it finds that
// then (1) the function result will be TRUE and (2) the outputs will be filled in with fields suitable to tell
// EFI firmware to boot that region on that disc.
//
bool isDVDWithElToritoWithUEFIBootableOS (BLContextPtr inContext, const char* inDevBSD, int* outBootEntry, int* outPartitionStart, int* outPartitionSize)
{
    bool                    ret = false;
    
    CFMutableDictionaryRef  match;
    io_service_t            media;
    
    bool                    foundMSDOSRegion = false;
    uint32_t                msdosRegionInThisBootEntry = 0;
    uint32_t                msdosRegionOffset = 0;
    uint32_t                msdosRegionSize = 0;
    
    // See if we are on a UEFI-boot-capable machine; if not, we're done:
    if (false == isPreBootEnvironmentUEFIWindowsBootCapable (inContext))
    {
        contextprintf (inContext, kBLLogLevelVerbose, "preboot environment is not UEFI boot capable\n");
        goto Exit;
    }
    
    // See if given disk is a DVD medium (disc); if not, we're done:
    match = IOBSDNameMatching (kIOMasterPortDefault, 0, inDevBSD);
    media = IOServiceGetMatchingService (kIOMasterPortDefault, match);
    if (false == IOObjectConformsTo (media, kIODVDMediaClass))
    {
        contextprintf (inContext, kBLLogLevelVerbose, "given BSD is not a DVD disc medium\n");
        goto Exit;
    }
    
    // Parse ElTorito header and get msdos region's offset/size; if not found, we're done:
    findMSDOSRegion (inContext, inDevBSD, &foundMSDOSRegion, &msdosRegionOffset, &msdosRegionSize);
    if (false == foundMSDOSRegion)
    {
        contextprintf (inContext, kBLLogLevelVerbose, "given disc does not have ElTorito + Bootable image\n");
        goto Exit;
    }
    msdosRegionInThisBootEntry = 1;
    
    // Here if successfully found it:
    ret = true;
    
    Exit:;
    *outBootEntry = msdosRegionInThisBootEntry;
    *outPartitionStart = msdosRegionOffset;
    *outPartitionSize = msdosRegionSize;
    contextprintf (inContext, kBLLogLevelVerbose, "isDVDWithElToritoWithUEFIBootableOS=%d\n", ret);
    return ret;
}
