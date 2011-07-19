/*
 * Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
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

/*
 *  bless.h
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Wed Feb 21 2002.
 *  Copyright (c) 2002-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: bless.h,v 1.79 2006/07/19 00:15:38 ssen Exp $
 *
 */
 
#ifndef _BLESS_H_
#define _BLESS_H_

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/cdefs.h>

__BEGIN_DECLS 

/*!
 * @header Bless Library
 * @discussion Common functions for setting on-disk and Open Firmware-base
 * bootability options
 */

/***** Structs *****/

/*!
 * @struct BLContext
 * @abstract Bless Library Context
 * @discussion Each thread of a multi-threaded program should
 *    have their own bless context, which is the first argument
 *    to all functions in the Bless Library. Only one function
 *    should be performed with a given context at the same time,
 *    although multiple functions can be called simultaneously if
 *    they are provided unique contexts. All bless functions can
 *    be called with a null context, or a non-null context with
 *    a null <b>logstring</b> member. a null <b>logrefcon</b>
 *    may or may not be allowed depending on the user-defined
 *    <b>logstring</b> function.
 * @field version version of BLContext in use by client. Currently
 *    0
 * @field logstring function used for messages from the library. It
 *    will be called with <b>logrefcon</b> and a log level, which
 *    can be used to tailor the output
 * @field logrefcon arbitrary data passed to <b>logrefcon</b>
 */
typedef struct {
  int32_t	version;
  int32_t	(*logstring)(void *refcon, int32_t level, char const *string);
  void		*logrefcon;
} BLContext, *BLContextPtr;

/*!
 * @define kBLLogLevelNormal
 * @discussion Normal output indicating status
 */
#define kBLLogLevelNormal  0x00000001

/*!
 * @define kBLLogLevelVerbose
 * @discussion Verbose output for greater feedback
 */
#define kBLLogLevelVerbose 0x00000002

/*!
 * @define kBLLogLevelError
 * @discussion Error output for warnings and unexpected conditions
 */
#define kBLLogLevelError   0x00000004

/***** Constants *****/

/*!
 * @define kBL_PATH_PPC_BOOTBLOCKDATA
 * @discussion Boot blocks for Darwin PPC on HFS/HFS+ partitions
 */
#define kBL_PATH_PPC_BOOTBLOCKDATA "/usr/share/misc/bootblockdata"

/*!
 * @define kBL_PATH_PPC_CDBOOTDATA
 * @discussion Fake System file for an HFS+ wrapper to repatch OF
 */
#define kBL_PATH_PPC_CDBOOTDATA "/usr/share/misc/cdblockdata"

/*!
 * @define kBL_PATH_PPC_HDBOOTDATA
 * @discussion Fake System file for an HFS+ volume to repatch OF
 */
#define kBL_PATH_PPC_HDBOOTDATA "/usr/share/misc/hdblockdata"

/*!
 * @define kBL_PATH_PPC_BOOTX_XCOFF
 * @discussion XCOFF format Secondary loader for Old World
 */
#define kBL_PATH_PPC_BOOTX_XCOFF "/usr/standalone/ppc/bootx.xcoff"

/*!
 * @define kBL_PATH_PPC_BOOTX_BOOTINFO
 * @discussion bootinfo format Secondary loader for New World
 */
#define kBL_PATH_PPC_BOOTX_BOOTINFO "/usr/standalone/ppc/bootx.bootinfo"

/*!
 * @define kBL_PATH_CORESERVICES
 * @discussion Default blessed system folder for Mac OS X
 */
#define kBL_PATH_CORESERVICES "/System/Library/CoreServices"

/*!
 * @define kBL_PATH_I386_BOOT0
 * @discussion MBR boot code
 */
#define kBL_PATH_I386_BOOT0 "/usr/standalone/i386/boot0"

/*!
 * @define kBL_PATH_I386_BOOT1HFS
 * @discussion partition boot record boot code for HFS+
 */
#define kBL_PATH_I386_BOOT1HFS "/usr/standalone/i386/boot1h"

/*!
 * @define kBL_PATH_I386_BOOT1UFS
 * @discussion partition boot record boot code for UFS
 */
#define kBL_PATH_I386_BOOT1UFS "/usr/standalone/i386/boot1u"

/*!
 * @define kBL_PATH_I386_BOOT2
 * @discussion Second stage loader for Darwin x86 on BIOS-based systems
 */
#define kBL_PATH_I386_BOOT2 "/usr/standalone/i386/boot"

/*!
 * @define kBL_PATH_I386_BOOT_EFI
 * @discussion Booter for Darwin x86 on EFI-based systems
 */
#define kBL_PATH_I386_BOOT_EFI "/usr/standalone/i386/boot.efi"

/*!
 * @define kBL_PATH_I386_BOOT2_CONFIG_PLIST
 * @discussion Second stage loader config file for Darwin x86
 */
#define kBL_PATH_I386_BOOT2_CONFIG_PLIST "/Library/Preferences/SystemConfiguration/com.apple.Boot.plist"


/*!
 * @define kBL_OSTYPE_PPC_TYPE_BOOTX
 * @discussion HFS+ type for Secondary loader for New World
 */
#define kBL_OSTYPE_PPC_TYPE_BOOTX 'tbxi'

/*!
 * @define kBL_OSTYPE_PPC_TYPE_OFLABEL
 * @discussion HFS+ type for OpenFirmware volume label
 */
#define kBL_OSTYPE_PPC_TYPE_OFLABEL 'tbxj'


/*!
* @define kBL_OSTYPE_PPC_TYPE_OFLABEL_PLACEHOLDER
 * @discussion Placeholder for OpenFirmware volume label
 */
#define kBL_OSTYPE_PPC_TYPE_OFLABEL_PLACEHOLDER 'xxxx'

/*!
 * @define kBL_OSTYPE_PPC_CREATOR_CHRP
 * @discussion HFS+ creator for all OF support files
 */
#define kBL_OSTYPE_PPC_CREATOR_CHRP 'chrp'

/***** Enumerations *****/

typedef enum {

    kBLPartitionType_None = 0x00000001,

    kBLPartitionType_MBR  = 0x00000002,

    kBLPartitionType_APM  = 0x00000004,

    kBLPartitionType_GPT  = 0x00000008
    
} BLPartitionType;

typedef enum {
    
    kBLBootRootRole_Unknown             = 0x00000001,
    
    kBLBootRootRole_None                = 0x00000002,

    kBLBootRootRole_AuxiliaryPartition  = 0x00000004,

    kBLBootRootRole_MemberPartition     = 0x00000008,

    kBLBootRootRole_BootRootDevice      = 0x00000010,

} BLBootRootRole;

typedef enum {
	
    kBLPreBootEnvType_Unknown		= 0x00000001,
	
    kBLPreBootEnvType_OpenFirmware	= 0x00000002,
	
    kBLPreBootEnvType_BIOS			= 0x00000004,
	
    kBLPreBootEnvType_EFI			= 0x00000008,
    
    kBLPreBootEnvType_iBoot			= 0x00000010,
    
} BLPreBootEnvType;

typedef enum {
	
	kBLNetBootProtocol_Unknown		= 0x00000001,
	
	kBLNetBootProtocol_BSDP			= 0x00000002,
	
	kBLNetBootProtocol_PXE			= 0x00000004
	
} BLNetBootProtocolType;

/***** BootBlocks *****/


/*!
 * @function BLSetBootBlocks
 * @abstract Write boot blocks to a volume
 * @discussion Write the bytes from <b>bootblocks</b>
 *   as the boot blocks of the HFS+ volume mounted at <b>mountpoint</b>
 * @param context Bless Library context
 * @param mountpoint mountpoint of volume to modify
 * @param bootblocks buffer of at most 1024 bytes to hold boot blocks
 */
int BLSetBootBlocks(BLContextPtr context,
		    const char * mountpoint,
		    const CFDataRef bootblocks);

/***** FinderInfo *****/

/*!
 * @function BLCreateVolumeInformationDictionary
 * @abstract Gather bootability information on a volume
 * @discussion Return information on the volume at <b>mount</b>,
 *     including blessed folder IDs and paths, UUID, boot blocks,
 *     and return it as a CFDictionary
 * @param context Bless Library context
 * @param mount mountpoint of volume to gather information on
 * @param outDict result dictionary
 */
int BLCreateVolumeInformationDictionary(BLContextPtr context,
					const char * mountpoint,
					CFDictionaryRef *outDict);

/*!
 * @function BLGetFinderFlag
 * @abstract Fetch Finder flags for a file/directory
 * @discussion Get the value of the flag <b>flag</b>, as defined
 *    &lt;CoreServices/.../CarbonCore/Finder.h&gt;, for the file
 *    or directory <b>path</b>
 * @param context Bless Library context
 * @param path a file or directory
 * @param flag bitmask of flag to be fetched
 * @param retval value of <b>flag</b> for <b>path</b>.
 */
int BLGetFinderFlag(BLContextPtr context,
		    const char * path,
		    uint16_t flag,
		    int *retval);
/*!
 * @function BLSetFinderFlag
 * @abstract Set Finder flags for a file/directory
 * @discussion Set the value of the flag <b>flag</b>, as defined
 *    &lt;CoreServices/.../CarbonCore/Finder.h&gt;, for the file
 *    or directory <b>path</b>
 * @param context Bless Library context
 * @param path a file or directory
 * @param flag bitmask of flag to be set
 * @param setval value of <b>flag</b> for <b>path</b>.
 */
int BLSetFinderFlag(BLContextPtr context,
		    const char * path,
		    uint16_t flag,
		    int setval);

/*!
 * @function BLGetVolumeFinderInfo
 * @abstract Get Finder info words for a volume
 * @discussion Get the Finder info words for a volume,
 *    which specifies the dirIDs of blessed system
 *    folders for Open Firmware to set
 * @param context Bless Library context
 * @param mountpoint mountpoint of volume
 * @param words array of at least length 8
 *    (directory IDs in host endianness)
 */
int BLGetVolumeFinderInfo(BLContextPtr context,
			  const char * mountpoint,
			  uint32_t * words);

/*!
 * @function BLSetVolumeFinderInfo
 * @abstract Set Finder info words for a volume
 * @discussion Set the Finder info words for a volume,
 *    which specifies the dirIDs of blessed system
 *    folders for Open Firmware to set
 * @param context Bless Library context
 * @param mountpoint mountpoint of volume
 * @param words array of at least length 6, which will
 *    replace the first 6 words on-disk
 *    (directory IDs in host endianness)
 */
int BLSetVolumeFinderInfo(BLContextPtr context,
			  const char * mountpoint,
			  uint32_t * words);

/*!
 * @function BLSetTypeAndCreator
 * @abstract Set the HFS Type and Creator for a file
 * @discussion Set te HFS Type and Creator for
 *    a file using the OSType's <b>type</b>
 *    and <b>creator</b>
 * @param context Bless Library context
 * @param path file to set
 * @param type OSType with type
 * @param creator OSType with creator
 */
int BLSetTypeAndCreator(BLContextPtr context,
			const char * path,
			uint32_t type,
			uint32_t creator);

/***** HFS *****/

/*!
 * @function BLBlessDir
 * @abstract Bless the volume
 * @discussion Bless the volume, using <b>dirX</b>
 *    for <i>finderinfo[5]</i>, <b>dir9</b>
 *    for <i>finderinfo[3]</i>, and selecting one
 *    of them for <i>finderinfo[0]</i> based
 *    on <b>useX</b>
 * @param context Bless Library context
 * @param mountpoint mountpoint of volume
 * @param dirX directory ID of Mac OS X 
 *    <i>/System/Library/CoreServices</i>
 *    folder
 * @param dir9 directory ID of Mac OS 9
 *    <i>/System Folder</i> folder
 * @param useX preferentially use <b>dirX</b>
 *    for <i>finderinfo[0]</i>, which is
 *    the only thing Open Firmware uses for
 *    loading the secondary loader
 */

int BLBlessDir(BLContextPtr context,
	       const char * mountpoint,
	       uint32_t dirX,
	       uint32_t dir9,
	       int useX);

/*!
 * @function BLGetFileID
 * @abstract Get the file ID for a file
 * @discussion Get the file ID for a file,
 *    relative to the volume its on
 * @param context Bless Library context
 * @param path path to file
 * @param folderID file ID of <b>path</b>
 */
int BLGetFileID(BLContextPtr context,
		const char * path,
		uint32_t *folderID);

/*!
 * @function BLIsMountHFS
 * @abstract Test if the volume is HFS/HFS+
 * @discussion Perform a statfs(2) on the
 *    the volume at <b>mountpoint</b> and
 *    report whether it is an HFS/HFS+ volume
 * @param context Bless Library context
 * @param mountpt Mountpoint of volume
 * @param isHFS is the mount hfs?
 */
int BLIsMountHFS(BLContextPtr context,
		 const char * mountpt,
		 int *isHFS);

/*!
 * @function BLLookupFileIDOnMount
 * @abstract Get path of file with ID <b>fileID</b>
 *    on <b>mount</b>
 * @discussion Use volfs to do reverse-resolution of
 *    <b>fileID</b> on <b>mount</b> to a full path
 * @param context Bless Library context
 * @param mount Mountpoint of volume
 * @param fileID file ID to look up
 * @param out resulting path (up to MAXPATHLEN characeters will be written)
 */
int BLLookupFileIDOnMount(BLContextPtr context,
			  const char * mountpoint,
			  uint32_t fileID,
			  char * out);


/*!
 * @function BLGetDiskSectorsForFile
 * @abstract Determine the on-disk location of a file
 * @discussion Get the HFS/HFS+ extents for the
 *    file <b>path</b>, and translate it to 512-byte
 *    sector start/length pairs, relative to the beginning
 *    of the partition.
 * @param context Bless Library context
 * @param path path to file
 * @param extents array to hold start/length pairs for disk extents
 * @param device filled in with the device the extent information applies to
 */

int BLGetDiskSectorsForFile(BLContextPtr context,
                        const char * path,
                        off_t extents[8][2],
                        char * device);

/***** Misc *****/


/*!
 * @function BLCreateFile
 * @abstract Create a new file with contents of old one
 * @discussion Copy <b>source</b> to <b>dest</b>/<b>file</b>,
 *    with the new file being contiguously allocated.
 *    Optionally, write the data into the resource fork
 *    of the destination
 * @param context Bless Library context
 * @param data source data
 * @param file destination file
 * @param setImmutable mark new file with "uchg" flag
 * @param type an OSType representing the type of the new file
 * @param creator an OSType representing the creator of the new file
 */
int BLCreateFile(BLContextPtr context,
                 const CFDataRef data,
				 const char * file,
				 int setImmutable,
                 uint32_t type,
                 uint32_t creator);

/*!
 * @function BLGetCommonMountPoint
 * @abstract Get the volume that both paths reside on
 * @discussion Determine the mountpoint command
 *    to both paths
 * @param context Bless Library context
 * @param f1 First path
 * @param f2 Second path
 * @param mountp Resulting mount path
 */
int BLGetCommonMountPoint(BLContextPtr context,
			  const char * f1,
			  const char * f2,
			  char * mountp);

/*!
 * @function BLGetParentDevice
 * @abstract Get the parent of a leaf device
 * @discussion Get the parent whole device for
 *    a leaf device, and return which slice this is
 * @param context Bless Library context
 * @param partitionDev partition to use
 * @param parentDev parent partition of <b>partitionDev</b>
 * @param partitionNum which partition of <b>parentDev</b>
 *    is <b>partitionDev</b>
 */
int BLGetParentDevice(BLContextPtr context,
		      const char * partitionDev,
		      char * parentDev,
		      uint32_t *partitionNum);

/*!
* @function BLGetParentDeviceAndPartitionType
 * @abstract Get the parent and type of a leaf device
 * @discussion Get the parent whole device for
 *    a leaf device, and return which slice this is. Als
 *    what type of container it is
 * @param context Bless Library context
 * @param partitionDev partition to use
 * @param parentDev parent partition of <b>partitionDev</b>
 * @param partitionNum which partition of <b>parentDev</b>
 *    is <b>partitionDev</b>
 * @param pmapType the partition type
 */
int BLGetParentDeviceAndPartitionType(BLContextPtr context,
		      const char * partitionDev,
		      char * parentDev,
		      uint32_t *partitionNum,
		    BLPartitionType *pmapType);


/*!
 * @function BLIsNewWorld
 * @abstract Is the machine a New World machine
 * @discussion Get the hardware type of the
 *    current machine. Deprecated, since it
 *    only applies to OpenFirmware-based systems
 * @param context Bless Library context
 */
int BLIsNewWorld(BLContextPtr context);

/*!
 * @function BLGenerateOFLabel
 * @abstract Generate a bitmap label
 * @discussion Use CoreGraphics to render
 *    a bitmap for an OF label
 * @param context Bless Library context
 * @param label UTF-8 encoded text to use
 * @param data bitmap data
 */
int BLGenerateOFLabel(BLContextPtr context,
                    const char * label,
                    CFDataRef *data);

/*!
 * @function BLSetOFLabelForDevice
 * @abstract Set the OpenFirmware label for an
 *    unmounted volume
 * @discussion Use MediaKit to analyze an HFS+
 *    volume header and catalog to find the OF
 *    label, and if it exists write the new data.
 *    If an existing label is not present, or if
 *    the on-disk allocated extent is too small,
 *    return an error
 * @param context Bless Library context
 * @param device an HFS+ (wrapped or not) device node
 * @param label a correctly formatted OF label,
 *    as returned by BLGenerateOFLabel
 */
int BLSetOFLabelForDevice(BLContextPtr context,
			  const char * device,
			  const CFDataRef label);

/*!
 * @function BLLoadFile
 * @abstract Load the contents of a file into a CFDataRef
 * @discussion use URLAccess to load <b>src</b> into
 *     a newly allocated CFDataRef. Caller must release
 *     it.
 * @param context Bless Library context
 * @param src path to source
 * @param useRsrcFork whether to copy data from resource fork
 * @param data pointer to new data
 */
int BLLoadFile(BLContextPtr context,
               const char * src,
               int useRsrcFork,
               CFDataRef* data);

/*!
    @function 
    @abstract   Determine pre-boot environment
    @discussion Determine the pre-boot environment type in
		order to set the active boot partition, which requires
		communicating information to the pre-boot environment, either
		directly via nvram, or indirectly via on-disk files/locations.
 * @param context Bless Library context
 * @param pbType	type of environment
 * @result     0 on success
*/

int BLGetPreBootEnvironmentType(BLContextPtr context,
								BLPreBootEnvType *pbType);


/***** OpenFirmware *****/

/*!
 * @function BLIsOpenFirmwarePresent
 * @abstract Does the host use Open Firmware
 * @discussion Attempts to access the IODeviceTree
 *     plane to see if Open Firmware is present
 * @param context Bless Library context
 */
int BLIsOpenFirmwarePresent(BLContextPtr context);

/*!
 * @function BLGetOpenFirmwareBootDevice
 * @abstract Determine the <i>boot-device</i>
 *    string for a partition
 * @discussion Determine the OF path to
 *    boot from a partition. If the partition
 *    is not HFS+, point to the secondary loader
 *    partition
 * @param context Bless Library context
 * @param mntfrm partition device to use
 * @param ofstring resulting OF string
 */
int BLGetOpenFirmwareBootDevice(BLContextPtr context,
				const char * mntfrm,
				char * ofstring);

/*!
 * @function BLGetOpenFirmwareBootDeviceForMountPoint
 * @abstract Determine to <i>boot-device</i>
 *    string for a mountpoint
 * @discussion Determine the OF path to
 *    boot from a partition. If the partition
 *    is not HFS+, point to the secondary loader
 *    partition
 * @param context Bless Library context
 * @param mountpoint mountpoint to use
 * @param ofstring resulting OF string
 */
int BLGetOpenFirmwareBootDeviceForMountPoint(BLContextPtr context,
					     const char * mountpoint,
					     char * ofstring);

int BLGetOpenFirmwareBootDeviceForNetworkPath(BLContextPtr context,
                                               const char *interface,
                                               const char *host,
                                               const char *path,
											   char * ofstring);

/*!
 * @function BLSetOpenFirmwareBootDevice
 * @abstract Set OF <i>boot-device</i>
 *    to boot from a device
 * @discussion Set the OF path to
 *    boot from a partition. If the partition
 *    is not HFS+, point to the secondary loader
 *    partition
 * @param context Bless Library context
 * @param mntfrm device to use
 */
int BLSetOpenFirmwareBootDevice(BLContextPtr context,
				const char * mntfrm);

/*!
 * @function BLSetOpenFirmwareBootDevice
 * @abstract Set OF <i>boot-device</i>
 *    to boot from a mountpoint
 * @discussion Set the OF path to
 *    boot from a partition. If the partition
 *    is not HFS+, point to the secondary loader
 *    partition
 * @param context Bless Library context
 * @param mountpoint mountpoint to use
 */
int BLSetOpenFirmwareBootDeviceForMountPoint(BLContextPtr context,
					     const char * mountpoint);


/*!
 * @function BLGetDeviceForOpenFirmwarePath
 * @abstract Convert an OF string to a mountpoint
 * @discussion Determine which mountpoint corresponds
 *    to an OF <b>boot-device</b> string
 * @param context Bless Library context
 * @param ofstring OF string
 * @param mntfrm resulting mountpoint
 */
int BLGetDeviceForOpenFirmwarePath(BLContextPtr context,
				   const char * ofstring,
				   char * mntfrm);


int BLCopyOpenFirmwareNVRAMVariableAsString(BLContextPtr context,
                                           CFStringRef  name,
                                           CFStringRef *value);

/* RAID info */
int BLGetRAIDBootDataForDevice(BLContextPtr context, const char * device,
							   CFTypeRef *bootData);
int BLUpdateRAIDBooters(BLContextPtr context, const char * device,
						CFTypeRef bootData,
						CFDataRef bootxData, CFDataRef labelData);

typedef struct {
	uint32_t	version;
	uint32_t	reqType;
	uint32_t	reqCreator;
	char *		reqFilename;
	
	CFDataRef	payloadData;
	
	uint32_t	postType;
	uint32_t	postCreator;
	
	uint8_t		foundFile;
	uint8_t		updatedFile;
	
} BLUpdateBooterFileSpec;

int BLUpdateBooter(BLContextPtr context, const char * device,
				   BLUpdateBooterFileSpec *specs,
				   int32_t specCount);

int BLGetIOServiceForDeviceName(BLContextPtr context, const char * devName,
								io_service_t *service);

int BLDeviceNeedsBooter(BLContextPtr context, const char * device,
						int32_t *needsBooter,
						int32_t *isBooter,
						io_service_t *booterPartition);

// if want to netboot but don't know what interface to use,
// use this function. name ifname must be > IF_NAMESIZE 
int BLGetPreferredNetworkInterface(BLContextPtr context,
                                   char *ifname);

bool BLIsValidNetworkInterface(BLContextPtr context,
                               const char *ifname);

// optionalData is optional
int BLCreateEFIXMLRepresentationForPath(BLContextPtr context,
                                          const char *path,
                                          const char *optionalData,
                                          CFStringRef *xmlString,
                                          bool shortForm);

int BLCreateEFIXMLRepresentationForDevice(BLContextPtr context,
                                          const char *bsdName,
                                          const char *optionalData,
                                          CFStringRef *xmlString,
                                          bool shortForm);

int BLCreateEFIXMLRepresentationForLegacyDevice(BLContextPtr context,
                                          const char *bsdName,
                                          CFStringRef *xmlString);

int BLCreateEFIXMLRepresentationForNetworkPath(BLContextPtr context,
                                               BLNetBootProtocolType protocol,
                                               const char *interface,
                                               const char *host,
                                               const char *path,
                                               const char *optionalData,
                                               CFStringRef *xmlString);

int BLInterpretEFIXMLRepresentationAsNetworkPath(BLContextPtr context,
                                                 CFStringRef xmlString,
                                                 BLNetBootProtocolType *protocol,
                                                 char *interface,
                                                 char *host,
                                                 char *path);

int BLInterpretEFIXMLRepresentationAsDevice(BLContextPtr context,
                                            CFStringRef xmlString,
                                            char *bsdName);

int BLInterpretEFIXMLRepresentationAsLegacyDevice(BLContextPtr context,
                                            CFStringRef xmlString,
                                            char *bsdName);

int BLCopyEFINVRAMVariableAsString(BLContextPtr context,
                                   CFStringRef  name,
                                   CFStringRef *value);

int BLValidateXMLBootOption(BLContextPtr context,
							CFStringRef	 xmlName,
							CFStringRef	 binaryName);

kern_return_t BLSetEFIBootDevice(BLContextPtr context, char *bsdName);

bool BLSupportsLegacyMode(BLContextPtr context);

bool BLIsEFIRecoveryAccessibleDevice(BLContextPtr context, CFStringRef bsdName);


// filter out bad boot-args
int BLPreserveBootArgs(BLContextPtr context,
                       const char *input,
                       char *output);

#define kBLDataPartitionsKey        CFSTR("Data Partitions")
#define kBLAuxiliaryPartitionsKey   CFSTR("Auxiliary Partitions")
#define kBLSystemPartitionsKey      CFSTR("System Partitions")

int BLCreateBooterInformationDictionary(BLContextPtr context, const char * bsdName,
                                        CFDictionaryRef *outDict);

__END_DECLS

#endif // _BLESS_H_
