/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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

/*
 *  bless.h
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Wed Feb 21 2002.
 *  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: bless.h,v 1.15 2002/05/03 04:23:54 ssen Exp $
 *
 */
 
#ifndef _BLESS_H_
#define _BLESS_H_

#ifdef _cplusplus
extern "C" {
#endif
 

/*!
 * @header Bless Library
 * @discussion Common functions for setting on-disk and Open Firmware-base
 * bootability options
 */

/***** Structs *****/

/*!
 * @struct BLContextStruct
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
 *    <b>printf</b> function.
 * @field logstring function used for messages from the library. It
 *    will be called with <b>printfrefcon</b> and a log level, which
 *    can be used to tailor the output
 * @field logrefcon arbitrary data passed to <b>printf</b>
 */
typedef struct {
  int (*logstring)(void *refcon, int level, char const *string);
  void *logrefcon;
} BLContextStruct, *BLContext;

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


/***** BootBlocks *****/

/*!
 * @function BLGetBootBlocksFromFolder
 * @abstract Read boot blocks from a Mac OS 9-style System file
 * @discussion Using the system folder specified by <b>dirID</b>
 *   on <b>mountpoint</b>, find the system file named &quot;System&quot;
 *   and use the <i>Carbon Resource Manager</i> to read boot blocks
 *   from the 'boot' 1 resource
 * @param context Bless Library context
 * @param mountpoint mountpoint of volume to use
 * @param dir9 directory ID of system folder on <b>mountpoint</b>
 * @param bootBlocks buffer of at least 1024 bytes to hold boot blocks
 */

int BLGetBootBlocksFromFolder(BLContext context,
			      unsigned char mountpoint[],
			      u_int32_t dir9,
			      unsigned char bootBlocks[]);

/*!
 * @function BLGetBootBlocksFromFile
 * @abstract Read boot blocks from a Mac OS 9-style System file
 * @discussion Using the system file <b>file</b>, use the
 *   <i>Carbon Resource Manager</i> to read boot blocks
 *   from the 'boot' 1 resource
 * @param context Bless Library context
 * @param path full path to Mac OS 9 System file
 * @param bootBlocks buffer of at least 1024 bytes to hold boot blocks
 */

int BLGetBootBlocksFromFile(BLContext context,
			    unsigned char file[],
			    unsigned char bootBlocks[]);

/*!
 * @function BLGetBootBlocksDataForkFromFile
 * @abstract Read boot blocks from a data file
 * @discussion Read the first 1024 bytes from the data fork of the
 *   file <b>file</b> as boot blocks
 * @param context Bless Library context
 * @param path Full path to data file
 * @param bootBlocks buffer of at least 1024 bytes to hold boot blocks
 */
int BLGetBootBlocksFromDataForkFile(BLContext context,
				    unsigned char file[],
				    unsigned char bootBlocks[]);

/*!
 * @function BLSetBootBlocks
 * @abstract Write boot blocks to a volume
 * @discussion Write the first 1024 bytes from the buffer <b>bbPtr</b>
 *   as the boot blocks of the volume mounted at <b>mountpoint</b>
 * @param context Bless Library context
 * @param mountpoint mountpoint of volume to modify
 * @param bbPtr buffer of at least 1024 bytes to hold boot blocks
 */
int BLSetBootBlocks(BLContext context,
		    unsigned char mountpoint[],
		    unsigned char bbPtr[]);

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
int BLCreateVolumeInformationDictionary(BLContext context,
					unsigned char mount[],
					/* CFDictionaryRef */ void **outDict);

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
int BLGetFinderFlag(BLContext context,
		    unsigned char path[],
		    u_int16_t flag,
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
int BLSetFinderFlag(BLContext context,
		    unsigned char path[],
		    u_int16_t flag,
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
 */
int BLGetVolumeFinderInfo(BLContext context,
			  unsigned char mountpoint[],
			  u_int32_t words[]);

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
 */
int BLSetVolumeFinderInfo(BLContext context,
			  unsigned char mountpoint[],
			  u_int32_t words[]);

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
int BLSetTypeAndCreator(BLContext context,
			unsigned char path[],
			u_int32_t type,
			u_int32_t creator);

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

int BLBlessDir(BLContext context,
	       unsigned char mountpoint[],
	       u_int32_t dirX,
	       u_int32_t dir9,
	       int useX);

/*!
 * @function BLFormatHFS
 * @abstract Format a volume as HFS+
 * @discussion Format a volume as HFS+,
 *    and leave space in the wrapper, and pass
 *    options to <i>newfs_hfs</i>
 * @param context Bless Library context
 * @param devicepath block device to format
 * @param bytesLeftFree leave space in the wrapper
 * @param fslabel volume label
 * @param fsargs additional arguments to
 *    <i>newfs_hfs</i>
 */
int BLFormatHFS(BLContext context,
		unsigned char devicepath[],
                off_t bytesLeftFree,
		unsigned char fslabel[],
		unsigned char fsargs[]);

/*!
 * @function BLGetFileID
 * @abstract Get the file ID for a file
 * @discussion Get the file ID for a file,
 *    relative to the volume its on
 * @param context Bless Library context
 * @param path path to file
 * @param folderID file ID of <b>path</b>
 */
int BLGetFileID(BLContext context,
		unsigned char path[],
		u_int32_t *folderID);

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
int BLIsMountHFS(BLContext context,
		 unsigned char mountpt[],
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
 * @param out resulting path
 */
int BLLookupFileIDOnMount(BLContext context,
			  unsigned char mount[],
			  u_int32_t fileID,
			  unsigned char out[]);

#if !defined(DARWIN)

/*!
 * @function BLWriteStartupFile
 * @abstract Write the StartupFile for Old World booting
 * @discussion Read the xcoff at from <b>xcoff</b> and
 *    write it into the HFS+ partition <b>partitionDev</b>
 * @param context Bless Library context
 * @param xcoff source of Secondary Loader (i.e.
 *    /usr/standalone/ppc/bootx.xcoff
 * @param partitionDev partition to install onto
 * @param parentDev whole device containing <b>partitionDev</b>
 * @param partitionNum which partition of <b>parentDev</b>
 *    is <b>partitionDev</b>
 */
int BLWriteStartupFile(BLContext context,
		       unsigned char xcoff[],
		       unsigned char partitionDev[],
		       unsigned char parentDev[],
		       unsigned long partitionNum);

#endif /* !DARWIN */

/*!
 * @function BLLoadXCOFFLoader
 * @abstract Load Old World Secondary Loader into memory
 * @description Decode the BootX secondary
 *    loader at <b>xcoff</b> and calculate parameters
 *    for the partition entry
 * @param context Bless Library context
 * @param xcoff path to bootx.coff, usually 
 *    /usr/standalone/ppc/bootx.xcoff
 * @param entrypoint pmBootEntry field in partition entry
 * @param loadbase pmBootAddr field in partition entry
 * @param size pmBootSize field in partition entry
 * @param checksum pmBootCksum field in partition entry
 * @param data CFDataRef containing decoded XCOFF
 */
int BLLoadXCOFFLoader(BLContext context,
                        unsigned char xcoff[],
                        u_int32_t *entrypoint,
                        u_int32_t *loadbase,
                        u_int32_t *size,
                        u_int32_t *checksum,
                        void /* CFDataRef */ **data);

/***** HFSWrapper *****/

/*!
 * @function BLMountHFSWrapper
 * @abstract Mount the wrapper of the wrapped HFS+ volume
 * @discussion Switch the signature of the wrapper
 *    volume to HFS- and mount it at <b>mountpt</b>
 * @param context Bless Library context
 * @param device wrapped HFS+ partition
 * @param mountpt mountpoint to use
 */
int BLMountHFSWrapper(BLContext context,
		      unsigned char device[],
		      unsigned char mountpt[]);

/*!
 * @function BLUnmountHFSWrapper
 * @abstract Unount the wrapper of the wrapped HFS+ volume
 * @discussion Unmount and switch the signature of the wrapper
 *    volume back to HFS+/embedded
 * @param context Bless Library context
 * @param device wrapped HFS+ partition
 * @param mountpt mountpoint to unmount from
 */
int BLUnmountHFSWrapper(BLContext context,
			unsigned char device[],
			unsigned char mountpt[]);

/*!
 * @function BLUpdateHFSWrapper
 * @abstract Add a system file to the wrapper
 * @discussion Use the data-fork system file
 *    to update the wrapper, and add other bits
 *    for Mac OS X booting
 * @param context Bless Library context
 * @param mountpt mountpoint to use
 * @param system data-fork system file
 */
int BLUpdateHFSWrapper(BLContext context,
		       unsigned char mountpt[],
		       unsigned char system[]);

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
 * @param dest destination folder
 * @param file destination file in <b>dest</b>
 * @param useRsrcFork place data in resource fork
 * @param type an OSType representing the type of the new file
 * @param creator an OSType representing the creator of the new file
 */
int BLCreateFile(BLContext context,
                 void * /* CFDataRef */ data,
                 unsigned char dest[],
		 unsigned char file[],
		 int useRsrcFork,
                 u_int32_t type,
                 u_int32_t creator);

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
int BLGetCommonMountPoint(BLContext context,
			  unsigned char f1[],
			  unsigned char f2[],
			  unsigned char mountp[]);

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
int BLGetParentDevice(BLContext context,
		      unsigned char partitionDev[],
		      unsigned char parentDev[],
		      unsigned long *partitionNum);

/*!
 * @function BLIsNewWorld
 * @abstract Is the machine a New World machine
 * @discussion Get the hardware type of the
 *    current machine
 * @param context Bless Library context
 */
int BLIsNewWorld(BLContext context);

/*!
 * @function BLGenerateOFLabel
 * @abstract Generate a bitmap label
 * @discussion Use CoreGraphics to render
 *    a bitmap for an OF label
 * @param context Bless Library context
 * @param label UTF-8 encoded text to use
 * @param data bitmap data
 */
int BLGenerateOFLabel(BLContext context,
                    unsigned char label[],
                    void /* CFDataRef */ **data);

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
int BLLoadFile(BLContext context,
               unsigned char src[],
               int useRsrcFork,
               void ** /* CFDataRef* */ data);


/***** OpenFirmware *****/

/*!
 * @function BLGetOpenFirmwareBootDevice
 * @abstract Determine to <i>boot-device</i>
 *    string for a partition
 * @discussion Determine the OF path to
 *    boot from a partition. If the partition
 *    is not HFS+, point to the secondary loader
 *    partition
 * @param context Bless Library context
 * @param mntfrm partition device to use
 * @param ostring resulting OF string
 */
int BLGetOpenFirmwareBootDevice(BLContext context,
				unsigned char mntfrm[],
				char ofstring[]);

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
 * @param ostring resulting OF string
 */
int BLGetOpenFirmwareBootDeviceForMountPoint(BLContext context,
					     unsigned char mountpoint[],
					     char ofstring[]);

/*!
 * @function BLSetOpenFirmwareBootDevice
 * @abstract Set OF <i>boot-device</i>
 *    to boot from a device
 * @discussion Set the OF path to
 *    boot from a partition. If the partition
 *    is not HFS+, point to the secondary loader
 *    partition
 * @param context Bless Library context
 * @param mntfrom device to use
 */
int BLSetOpenFirmwareBootDevice(BLContext context,
				unsigned char mntfrm[]);

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
int BLSetOpenFirmwareBootDeviceForMountPoint(BLContext context,
					     unsigned char mountpoint[]);


/*!
 * @function BLGetDeviceForOpenFirmwarePath
 * @abstract Convert an OF string to a mountpoint
 * @discussion Determine which mountpoint corresponds
 *    to an OF <b>boot-device</b> string
 * @param context Bless Library context
 * @param ofstring OF string
 * @param mntfrom resulting mountpoint
 */
int BLGetDeviceForOpenFirmwarePath(BLContext context,
				   char ofstring[],
				   unsigned char mntfrm[]);


int contextprintf(BLContext context, int loglevel, char const *fmt, ...);

#ifdef _cplusplus
}
#endif

#endif // _BLESS_H_
