/* 
 * Copyright (c) 1999-2004 Apple Computer, Inc.  All Rights Reserved.
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

#include <errno.h>
#include <libkern/OSByteOrder.h>
#include <stdio.h>
#include <unistd.h>
#include <paths.h>
#include <sys/disk.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <fsproperties.h>

#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/CoreStorage/CoreStorageUserLib.h>
#include <IOKit/IOKitLib.h>

#include "FSFormatName.h"

static CFMutableDictionaryRef __FSLocalizedNameTable = NULL;
static OSSpinLock __FSLocalizedNameTableLock = 0;
static bool IsEncrypted(const char *bsdname);

CFStringRef FSCopyFormatNameForFSType(CFStringRef fsType, int16_t fsSubtype, bool localized, bool encrypted) 
{
    CFTypeRef formatName;
    CFStringRef formatNameTableKey;
    CFIndex indx;

    if (NULL == fsType) return NULL;

    // Create a key for cache localized name table (i.e. "0hfs0")
    formatNameTableKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d%@%d"), (localized ? 1 : 0), fsType, fsSubtype);

    // Use OSSpinLock to protect the table accessed from multiple threads
    OSSpinLockLock(&__FSLocalizedNameTableLock);
    formatName = (void*)((NULL == __FSLocalizedNameTable) ? NULL : CFDictionaryGetValue(__FSLocalizedNameTable, (const void *)formatNameTableKey));
    OSSpinLockUnlock(&__FSLocalizedNameTableLock);

    if (NULL == formatName) { // not in the cache
        CFBundleRef bundle = NULL;
        CFURLRef bundleURL;
        CFStringRef fsTypeName;
	static CFArrayRef searchPaths = NULL;

        /* Construct a bundle path URL from the fsType argument and create a CFBundle.  We search (using CFCopySearchPathForDirectoriesInDomains) /Network/Library/Filesystems, /Library/Filesystems, and /System/Library/Filesystems. */

        // Create CFURL for /System/Library/Filesystems and cache it
	if (NULL == searchPaths) {
		CFArrayRef tmpPaths = CFCopySearchPathForDirectoriesInDomains(kCFLibraryDirectory, kCFSystemDomainMask | kCFNetworkDomainMask | kCFLocalDomainMask, true);
		CFMutableArrayRef tmpStrings;
		CFIndex i;

		if (NULL == tmpPaths)
			return NULL;	// No directories to search?!?!

		tmpStrings = CFArrayCreateMutable(NULL, CFArrayGetCount(tmpPaths), NULL);
		if (tmpStrings == NULL)
			goto done;
		for (i = 0; i < CFArrayGetCount(tmpPaths); i++) {
			CFStringRef tStr;
			CFURLRef tURL;
			char path[PATH_MAX + 1];
			CFTypeRef tobject = CFArrayGetValueAtIndex(tmpPaths, i);

			if (CFGetTypeID(tobject) == CFURLGetTypeID()) {
				if (false ==
					CFURLGetFileSystemRepresentation(
						tobject,
						false,
						(UInt8*)path,
						sizeof(path))) {
					goto done;
				}
			} else if (CFGetTypeID(tobject) == CFStringGetTypeID()) {
				CFStringGetCString(tobject, path, sizeof(path), kCFStringEncodingUTF8);
			} else {
				goto done;
			}
			strlcat(path, "/Filesystems", sizeof(path));
			tStr = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
			if (tStr == NULL)
				goto done;
			tURL = CFURLCreateWithFileSystemPath(NULL, tStr, kCFURLPOSIXPathStyle, true);
			if (tURL) {
				CFArrayAppendValue(tmpStrings, tURL);
			}
			CFRelease(tStr);
		}
		searchPaths = CFArrayCreateCopy(NULL, tmpStrings);
done:
		CFRelease(tmpStrings);
		CFRelease(tmpPaths);
		if (searchPaths == NULL)
			return NULL;
	}

	for (indx = 0; indx < CFArrayGetCount(searchPaths); indx++) {
		CFURLRef libRef = CFArrayGetValueAtIndex(searchPaths, indx);

		fsTypeName = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@.fs"), fsType);
		bundleURL = CFURLCreateWithFileSystemPathRelativeToBase(NULL, fsTypeName, kCFURLPOSIXPathStyle, true, libRef);
		bundle = CFBundleCreate(NULL, bundleURL);

		CFRelease(fsTypeName);
		CFRelease(bundleURL);
		if (NULL != bundle) {
			break;
		}
	}

        if (NULL != bundle) { // the bundle exists at path	
			CFDictionaryRef localPersonalities = NULL;

			// Access the Info dictionary in the bundle 
			CFDictionaryRef bundleDict = CFBundleGetInfoDictionary(bundle);

			// Get localized FSPersonalities only if we want localized name
			if (localized == true) {
				localPersonalities = CFBundleGetValueForInfoDictionaryKey(bundle, KEY_FS_PERSONALITIES);
//NSLog(CFSTR("localPersonalities = %@\n"), localPersonalities);
			}

			/* Get global FSPersonalities.  We need to access this since FSSubType exists only
			 * in global FSPersonalities 
			 */
            CFDictionaryRef globalPersonalities = CFDictionaryGetValue(bundleDict, (const void *) KEY_FS_PERSONALITIES);
//NSLog(CFSTR("globalPersonalities = %@\n"), globalPersonalities);
			CFIndex numPersonalities;
            if (((NULL != localPersonalities) || (localized == false)) &&	// localPersonalities or we don't want localizations 
			    (NULL != globalPersonalities) && 
				((numPersonalities = CFDictionaryGetCount(globalPersonalities)) > 0)) {

				// read all FSPersonalities keys and values 
                CFDictionaryRef valuesBuffer[MAX_FS_SUBTYPES];
				CFStringRef keysBuffer[MAX_FS_SUBTYPES];
				CFDictionaryRef *values = ((numPersonalities > MAX_FS_SUBTYPES) ? (CFDictionaryRef *)malloc(sizeof(CFDictionaryRef) * numPersonalities) : valuesBuffer);
				CFStringRef *keys = ((numPersonalities > MAX_FS_SUBTYPES) ? (CFStringRef *)malloc(sizeof(CFStringRef) * numPersonalities) : keysBuffer);
                CFDictionaryGetKeysAndValues(globalPersonalities, (const void **)keys, (const void **)values);

				// create CFNumberRef for the FSSubType 
		        CFNumberRef subTypeID = CFNumberCreate(NULL, kCFNumberSInt16Type, (const void *)&fsSubtype);
				CFStringRef FSNameKey = NULL;
				
				// search for valid FSSubType - we will use its key from global FSPersonalties to 
				// access FSName from localized FSPersonalities
				CFIndex index;
				CFNumberRef readSubTypeID;
				for (index = 0; index < numPersonalities; index++) {
					if ((true == CFDictionaryGetValueIfPresent(values[index], (const void *)KEY_FS_SUBTYPE, (const void **)&readSubTypeID)) &&
						(CFNumberCompare(subTypeID, readSubTypeID, NULL) == 0)) {
						FSNameKey = keys[index];
						break;
					}
				}

                CFRelease(subTypeID);
				
                /* If a personality hasn't been found, use the last value in the dictionary (note the content of CFDictionary is unordered so choosing the last doesn't produce consistent result) */
                if (NULL == FSNameKey) {
                    FSNameKey = keys[numPersonalities - 1]; // is selecting the last entry right ?
                }

				// Get FSName from the FSPersonalities entry
				CFDictionaryRef FSNameDict;
				if (localized == true) { 
					FSNameDict = CFDictionaryGetValue(localPersonalities, FSNameKey);
				} else {
					FSNameDict = CFDictionaryGetValue(globalPersonalities, FSNameKey);
				}
				if (NULL != FSNameDict) {
					CFStringRef tempName = CFDictionaryGetValue(FSNameDict, (const void *)KEY_FS_NAME);
					CFStringRef encrName = CFDictionaryGetValue(FSNameDict, CFSTR(kFSEncryptNameKey));
					if (tempName) {
						if (encrName) {
							formatName = (void*)CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
							if (formatName != NULL) {
								(void)CFDictionarySetValue((void*)formatName, tempName, encrName);
							}
						} else {
							formatName = tempName;
						}
					}
				}

				if (values != valuesBuffer) free(values);
				if (keys != keysBuffer) free(keys);
            }
        }

        // If all failed, return "Unknown format (f_fstypename)" as the last resort
        if (NULL == formatName) {
            static CFStringRef unknownTypeString = NULL;
			CFStringRef unknownFSNameString = NULL;

            // This should use the framework bundle this code resides. CarbonCore ??? */
            if (NULL == unknownTypeString) unknownTypeString = CFCopyLocalizedString(UNKNOWN_FS_NAME, "This string is displayed when localized file system name cannot be determined.");
			
			unknownFSNameString = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@ (%@)"), unknownTypeString, fsType);
			formatName = (void*)unknownFSNameString;
        }
        
        // Cache the result
        OSSpinLockLock(&__FSLocalizedNameTableLock);
        if (NULL == __FSLocalizedNameTable) __FSLocalizedNameTable = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
//	NSLog(CFSTR("Setting value %@ for key %@\n"), formatName, formatNameTableKey);	
        CFDictionarySetValue(__FSLocalizedNameTable, (const void *)formatNameTableKey, (const void *)formatName);
        OSSpinLockUnlock(&__FSLocalizedNameTableLock);
//	NSLog(CFSTR("Localized Name Table = %@\n"), __FSLocalizedNameTable);
        
        if (NULL != bundle) CFRelease(bundle); // it has to be released here since formatName might be owned by the bundle
    }

     CFRelease(formatNameTableKey);

     if (CFGetTypeID(formatName) == CFStringGetTypeID()) {
	     return CFRetain(formatName);
     } else if (CFGetTypeID(formatName) == CFDictionaryGetTypeID()) {
	     // Dictionary with the (possibly localized) name as the key, and the encrypted name as the value
	     // If we want the encrypted name, we return the value, else we return the key
	     size_t numEntries = CFDictionaryGetCount((void*)formatName);
	     void *keyNames[numEntries];
	     void *values[numEntries];
	     CFDictionaryGetKeysAndValues((void*)formatName, (const void**)&keyNames, (const void**)&values);
	     if (encrypted)
		     return CFRetain(values[0]);
	     else
		     return CFRetain(keyNames[0]);
     }
	     
    return CFRetain(formatName);
}

CFStringRef _FSCopyLocalizedNameForVolumeFormatAtURL(CFURLRef url) 
{
    CFStringRef formatName = NULL;
    uint8_t buffer[MAXPATHLEN + 1];

    if ((NULL != url) && CFURLGetFileSystemRepresentation(url, true, buffer, MAXPATHLEN)) {
		struct statfs fsInfo;
		bool encrypted = false;

        if (statfs((char *)buffer, &fsInfo) == 0) {
            CFStringRef fsType = CFStringCreateWithCString(NULL, fsInfo.f_fstypename, kCFStringEncodingASCII);

			encrypted = IsEncrypted(fsInfo.f_mntfromname);
#ifdef _DARWIN_FEATURE_64_BIT_INODE
            formatName = FSCopyFormatNameForFSType(fsType, fsInfo.f_fssubtype, true, encrypted);
#else
            formatName = FSCopyFormatNameForFSType(fsType, fsInfo.f_reserved1, true, encrypted);
#endif

            CFRelease(fsType);
		}
    }

    return formatName;
}

CFStringRef _FSCopyNameForVolumeFormatAtURL(CFURLRef url) 
{
    CFStringRef formatName = NULL;
    uint8_t buffer[MAXPATHLEN + 1];

    if ((NULL != url) && CFURLGetFileSystemRepresentation(url, true, buffer, MAXPATHLEN)) {
	struct statfs fsInfo;

        if (statfs((char *)buffer, &fsInfo) == 0) {
            CFStringRef fsType = CFStringCreateWithCString(NULL, fsInfo.f_fstypename, kCFStringEncodingASCII);
			bool encrypted = false;
			
			encrypted = IsEncrypted(fsInfo.f_mntfromname);

#ifdef _DARWIN_FEATURE_64_BIT_INODE
            formatName = FSCopyFormatNameForFSType(fsType, fsInfo.f_fssubtype, false, encrypted);
#else
            formatName = FSCopyFormatNameForFSType(fsType, fsInfo.f_reserved1, false, encrypted);
#endif

            CFRelease(fsType);
        }
    }

    return formatName;
}

CFStringRef _FSCopyLocalizedNameForVolumeFormatAtNode(CFStringRef devnode) 
{
	CFStringRef formatName = NULL;
	char devnodename[MAXPATHLEN + 1];
	char fsname[MAX_FSNAME];
	int fssubtype = 0;
	    
	/* convert CFStringRef devnode to CSTR */
	if (true == CFStringGetCString(devnode, devnodename, MAXPATHLEN + 1, kCFStringEncodingUTF8)) {
		bool encrypted = false;

		encrypted = IsEncrypted(devnodename);

		/* get fsname and fssubtype */
		memset(fsname, 0, MAX_FSNAME);
		if (getfstype(devnodename, fsname, &fssubtype) == true) {
		
			/* get unlocalized string */
			CFStringRef fsType = CFStringCreateWithCString(NULL, fsname, kCFStringEncodingASCII);
			formatName = FSCopyFormatNameForFSType(fsType, fssubtype, true, encrypted);
			CFRelease(fsType);
		}
	}
	return formatName;
}

CFStringRef _FSCopyNameForVolumeFormatAtNode(CFStringRef devnode) 
{
	CFStringRef formatName = NULL;
	char devnodename[MAXPATHLEN + 1];
	char fsname[MAX_FSNAME];
	int fssubtype = 0;
	    
	/* convert CFStringRef devnode to CSTR */
	if (true == CFStringGetCString(devnode, devnodename, MAXPATHLEN + 1, kCFStringEncodingUTF8)) {
		bool encrypted;

		encrypted = IsEncrypted(devnodename);
		/* get fsname and fssubtype */
		memset(fsname, 0, MAX_FSNAME);
		if (getfstype(devnodename, fsname, &fssubtype) == true) {
		
			/* get unlocalized string */
			CFStringRef fsType = CFStringCreateWithCString(NULL, fsname, kCFStringEncodingASCII);
			formatName = FSCopyFormatNameForFSType(fsType, fssubtype, false, encrypted);
			CFRelease(fsType);
		}
	}
	return formatName;
}

/* Return the fsname and subtype number for devnode */
bool getfstype(char *devnode, char *fsname, int *fssubtype)
{
	/* Check if APFS */
	if (is_apfs(devnode, fssubtype) == true) {
		strcpy(fsname, APFS_NAME);
		return true;
	}

	/* Check if HFS */
	if (is_hfs(devnode, fssubtype) == true) {
		strcpy(fsname, HFS_NAME); 
		return true;
	}

	/* Check if MSDOS */
	if (is_msdos(devnode, fssubtype) == true) {
		strcpy(fsname, MSDOS_NAME);
		return true;
	}

	return false;
}

/*
 * Check if the disk is APFS disk and return its subtypes
 * (unless pointer is NULL, in which case wil only determine
 * if disk is APFS).
 */
bool is_apfs(char *devnode, int *fssubtype)
{
	bool result = false;
	io_object_t ioObj = IO_OBJECT_NULL;
	char * bsdName = NULL;

	if (!strncmp(devnode, "/dev/r", 6)) {
		// Strip off the /dev/r from /dev/rdiskX
		bsdName = &devnode[6];
	} else if (!strncmp(devnode, "/dev/", 5)) {
		// Strip off the /dev/r from /dev/rdiskX
		bsdName = &devnode[5];
	} else {
		bsdName = devnode;
	}

	ioObj = IOServiceGetMatchingService (
		kIOMasterPortDefault,
		IOBSDNameMatching ( kIOMasterPortDefault, 0, bsdName ) );

	if (ioObj != IO_OBJECT_NULL) {
		if (IOObjectConformsTo(ioObj, "AppleAPFSVolume")) {
			if (NULL != fssubtype)
				*fssubtype = kAPFSSubType;
			result = true;
			// Check if we have sensitivity info in IOReg
			CFBooleanRef sensitivity = ( CFBooleanRef ) IORegistryEntrySearchCFProperty (
				ioObj,
				kIOServicePlane,
				CASE_SENSITIVE,
				kCFAllocatorDefault,
				0 );
			if (sensitivity) {
				if (CFEqual(sensitivity, kCFBooleanTrue) && (NULL != fssubtype)) {
					*fssubtype = kAPFSXSubType;
				}
				CFRelease(sensitivity);
			}
		}
		IOObjectRelease(ioObj);
	}

	return result;
}

#define SW16(x)	OSSwapBigToHostInt16(x)
#define SW32(x)	OSSwapBigToHostInt32(x)

/* Check if the disk is HFS disk and return its subtypes */
bool is_hfs(char *devnode, int *fssubtype)
{
	HFSPlusVolumeHeader *vhp;
	off_t hfs_plus_data_offset = 0;
	char *buffer = NULL;
	int fd = 0;
	bool retval = false;
	size_t block_size = 0;

	/* default fssubtype to non-existing value */
	*fssubtype = -1;

	/* open the device */
	fd = open(devnode, O_RDONLY | O_NDELAY, 0);
	if (fd <= 0) {
		goto out;
	}

	// Try to find the block size of this device.
	if (ioctl(fd, DKIOCGETBLOCKSIZE, &block_size) == -1) {
		block_size = DEV_BSIZE; // Assume an overwhelmingly common disk block size.
	}

	if (block_size > MAX_HFS_BLOCKSIZE) {
		goto out;
	}

	buffer = (char *)malloc(block_size);
	if (!buffer) {
		goto out;
	}

	/* attempt to read volume header (512 bytes, at disk offset 1024) from the device */
	if (readdisk(fd, 1024, sizeof(HFSPlusVolumeHeader), block_size, buffer)
		< sizeof(HFSPlusVolumeHeader)) {
		goto out;
	}

	/*
	 * Check if the volume header is actually a master directory block.
	 * This is used in HFS and the HFS wrapper for HFS+.
	 */
	if (getwrapper((HFSMasterDirectoryBlock *)buffer, &hfs_plus_data_offset)) {
		// Make sure we can actually get the HFS+ data.
		off_t volume_header_offset = 1024 + hfs_plus_data_offset;
		if (readdisk(fd, volume_header_offset, sizeof(HFSPlusVolumeHeader), block_size, buffer)
			< sizeof(HFSPlusVolumeHeader)) {
			goto out;
		}
	}

	vhp = (HFSPlusVolumeHeader *)buffer;
	/* Validate signature */
	switch (SW16(vhp->signature)) {
		case kHFSPlusSigWord: {
			if (SW16(vhp->version) != kHFSPlusVersion) {
				goto out;
			}
			break;
		}

		case kHFSXSigWord: {
			if (SW16(vhp->version) != kHFSXVersion) {
				goto out;
			}
			break;
		}

		case kHFSSigWord: {
			/* HFS */
			*fssubtype = kHFSSubType;
			retval = true;
			goto out;
		}

		default: {
			goto out;
		}
	};

	if ((vhp->journalInfoBlock != 0) && (SW32(vhp->attributes) & kHFSVolumeJournaledMask)) {
		/* Journaled */
		*fssubtype = kHFSJSubType;
	}

	if (SW16(vhp->signature) == kHFSXSigWord) {
		BTHeaderRec * bthp;
		off_t foffset;

		foffset = (off_t)SW32(vhp->catalogFile.extents[0].startBlock) * (off_t)SW32(vhp->blockSize);
		if (readdisk(fd, hfs_plus_data_offset + foffset, sizeof(BTHeaderRec) + sizeof(BTNodeDescriptor), block_size, buffer)
			< sizeof(BTHeaderRec) + sizeof(BTNodeDescriptor)) {
			goto out;
		}

		bthp = (BTHeaderRec *)&buffer[sizeof(BTNodeDescriptor)];

		if ((SW16(bthp->maxKeyLength) == kHFSPlusCatalogKeyMaximumLength) &&
			(bthp->keyCompareType == kHFSBinaryCompare)) {
			/* HFSX */
			if (*fssubtype == kHFSJSubType) {
				/* Journaled HFSX */
				*fssubtype = kHFSXJSubType;
			} else {
				/* HFSX */
				*fssubtype = kHFSXSubType;
			}
		}
	}

	if (*fssubtype < 0) {
		/* default HFS Plus */
		*fssubtype = kHFSPlusSubType;
	}

	retval = true;

out:
	if (buffer) {
		free(buffer);
	}
	if (fd > 0) {
		close(fd);
	}
	return retval;
}

/* Check if the disk is MSDOS disk and return its subtypes */
bool is_msdos(char *devnode, int *fssubtype)
{
	union bootsector *bsp;
	struct byte_bpb710 *b710;
	u_int32_t FATSectors;
	u_int32_t TotalSectors;
	u_int32_t countOfClusters;
	u_int32_t DataSectors;
	u_int32_t RootDirSectors;
    u_int16_t BytesPerSector;
    u_int8_t SectorsPerCluster;
    size_t block_size = 0;
	char *buffer = NULL;
	int fd = 0;
	bool retval = false;

	/* default fssubtype to non-existing value */
	*fssubtype = -1;
	
	buffer = (char *)malloc(MAX_DOS_BLOCKSIZE);
	if (!buffer) {
		goto out;
	}
	
	/* open the device */	
	fd = open(devnode, O_RDONLY | O_NDELAY, 0);
	if (fd <= 0) {
		goto out;
	}

    // Try to find the block size of this device.
    if (ioctl(fd, DKIOCGETBLOCKSIZE, &block_size) == -1) {
        block_size = DEV_BSIZE; // Assume an overwhelmingly common disk block size.
    }

    if (block_size > MAX_DOS_BLOCKSIZE) {
        goto out;
    }

	/* read the 'bootsector' */
	if (readdisk(fd, 0, sizeof(union bootsector), block_size, buffer) < sizeof(union bootsector)) {
		goto out;
	}

    bsp = (union bootsector *)buffer;
    b710 = (struct byte_bpb710 *)bsp->bs710.bsBPB;
   
	/* The first three bytes are an Intel x86 jump instruction.  It should be one
     * of the following forms:
     *    0xE9 0x?? 0x??
     *    0xEC 0x?? 0x90
     * where 0x?? means any byte value is OK.
     */
    if (bsp->bs50.bsJump[0] != 0xE9
        && (bsp->bs50.bsJump[0] != 0xEB || bsp->bs50.bsJump[2] != 0x90)) {
		goto out;
    }

    /* We only work with 512, 1024, and 2048 byte sectors */
	BytesPerSector = getushort(b710->bpbBytesPerSec);
	if ((BytesPerSector < 0x200) || (BytesPerSector & (BytesPerSector - 1)) || (BytesPerSector > 0x800)) {
		goto out;
	}
	
	/* Check to make sure valid sectors per cluster */
    SectorsPerCluster = b710->bpbSecPerClust;
    if ((SectorsPerCluster == 0 ) || (SectorsPerCluster & (SectorsPerCluster - 1))) {
		goto out;
	}

	RootDirSectors = ((getushort(b710->bpbRootDirEnts) * 32) + (BytesPerSector - 1)) / BytesPerSector;

	if (getushort(b710->bpbFATsecs)) {
		FATSectors = getushort(b710->bpbFATsecs);
	} else {
		FATSectors = getulong(b710->bpbBigFATsecs);
	}

	if (getushort(b710->bpbSectors)) {
		TotalSectors = getushort(b710->bpbSectors);
	} else {
		TotalSectors = getulong(b710->bpbHugeSectors);
	}

	DataSectors = TotalSectors - (getushort(b710->bpbResSectors) + (b710->bpbFATs * FATSectors) + RootDirSectors); 
	
	countOfClusters = DataSectors/(b710->bpbSecPerClust);

	if (countOfClusters < 4085) {
		/* FAT12 */
		*fssubtype = 0;
	} else if (countOfClusters < 65525) {
		/* FAT16 */
		*fssubtype = 1;
	} else {
		/* FAT32 */
		*fssubtype = 2;
	}

	retval = true;
	
out:
	if (buffer) {
		free(buffer);
	}
	if (fd > 0) {
		close(fd);
	}
	return retval;
}

/* read raw block from disk */
static int getblk(int fd, unsigned long blknum, int blksize, char* buf)
{
	off_t offset;
	int bytes_read;
	
	offset = (off_t)blknum * (off_t)blksize;
	
	if ((bytes_read = pread(fd, buf, blksize, offset)) != blksize) {
		return (-1);
	}
	
	return (bytes_read);
}

/* read data from an arbitrary disk address */
ssize_t readdisk(int fd, off_t startaddr, size_t length, size_t blocksize, char* buf)
{
    ssize_t bytes_read = 0;

    // Find the starting block of this read and the number of blocks this read will take.
    size_t start_block = startaddr / blocksize;
    size_t num_blocks_to_read = ((startaddr + length) / blocksize) - start_block + 1;

    // Allocate a temporary buffer to copy those blocks into.
    void* tmpbuf = malloc(num_blocks_to_read * blocksize);

    void* bufaddr = tmpbuf;
    ssize_t block_bytes_read;
    // Read each block into our temporary buffer.
    // If we fail to read any of the data, exit.
    for (size_t i = 0; i < num_blocks_to_read; i++, bufaddr += blocksize) {
        block_bytes_read = getblk(fd, i + start_block, blocksize, bufaddr);
        if (block_bytes_read != blocksize) {
            goto cleanup;
        }

        bytes_read += block_bytes_read;
    }

    /*
     * We've read all the blocks that contain data requested:
     * Copy only the relevant portions into the buf provided.
     * Here we get subtract the first address of the block that
     * startaddr is in from startaddr.
     */
    size_t tmpbuf_offset = startaddr - ((startaddr / blocksize) * blocksize);
    memcpy(buf, tmpbuf + tmpbuf_offset, length);

cleanup:
    free(tmpbuf);
    return bytes_read;
}

/* get HFS wrapper information */
static int getwrapper(const HFSMasterDirectoryBlock *mdbp, off_t *offset)
{
	if ((SW16(mdbp->drSigWord) != kHFSSigWord) ||
	    (SW16(mdbp->drEmbedSigWord) != kHFSPlusSigWord)) {
		return(0);
	}
	*offset = SW16(mdbp->drAlBlSt) * 512;
	*offset += (u_int64_t)SW16(mdbp->drEmbedExtent.startBlock) * (u_int64_t)SW32(mdbp->drAlBlkSiz);
	
	return (1);
}

/* get the APFS fs index (slice) from a string like "/dev/disk0s1s2" */
static uint32_t
fsindex_parse(const char *device)
{
	const char *cp = &device[strlen(device)];
	int scale = 1, fsindex = 0;

	while (--cp >= device && *cp >= '0' && *cp <= '9') {
		fsindex += (*cp - '0') * scale;
		scale *= 10;
	}
	return (cp > device && cp[0] == 's' && cp[-1] != '/') ? --fsindex : -1;
}

// Return whether or not the VEK state of a APFS volume suspected to be encrypted
// indicates that FileVault is on. If we cannot determine this status, we return true.
static bool
IsVEKStateEncrypted(io_object_t apfs_obj, const uint32_t fsindex)
{
	OSStatus status;
	io_connect_t port;
	volume_vek_state_input io_input = {0};
	volume_vek_state_output io_output = {0};
	size_t o_size = sizeof(volume_vek_state_output);

	status = IOServiceOpen(apfs_obj, mach_task_self(), 0, &port);
	if (status != kIOReturnSuccess) {
		return true;
	}

	// We must ask APFS for this information. Because we are a static library, and several of our consumers
	// (including MediaKit) do not necessarily link the APFS framework, we cannot link directly to the APFS framework.
	// Instead, we go the back way around through IOKit to get this information.
	// To call this a hack would be an understatement.
	io_input.index = fsindex;
	status = IOConnectCallStructMethod(port, APFS_IOUC_VOLUME_GET_VEK_STATE, &io_input, sizeof(volume_vek_state_input),
									   &io_output, &o_size);
	IOServiceClose(port);
	if (status == kIOReturnSuccess) {
		if (!(io_output.user_protected && !io_output.sys_protected)) {
			// Volume is encrypted, but FileVault is not on. Treat as unencrypted.
			return false;
		}
	}

	return true;
}

/* get an io_object_t for IO Registry lookups - caller must release */
static errno_t
get_lookup_io_obj(const char *bsdname, io_object_t *lookup_obj)
{
	const char *diskname = bsdname;
	CFMutableDictionaryRef ioMatch; // IOServiceGetMatchingService() releases!

	if (!bsdname || !lookup_obj)
		return EINVAL;
	*lookup_obj = IO_OBJECT_NULL;

	// Get "diskXsY" portion of name.
	if (strncmp(bsdname, _PATH_DEV, strlen(_PATH_DEV)) == 0) {
		diskname = bsdname + strlen(_PATH_DEV);
	}

	if (diskname == NULL)
		return EINVAL;

	if (strncmp(diskname, "rdisk", 5) == 0)
		diskname++;

	//look up the IOMedia object
	ioMatch = IOBSDNameMatching(kIOMasterPortDefault, 0, diskname);
	if (!ioMatch)
		return ENOENT;

	// Setting this allows a fast-path lookup to happen
	// see 10248763
	CFDictionarySetValue(ioMatch, CFSTR(kIOProviderClassKey), CFSTR(kIOMediaClass));

	*lookup_obj = IOServiceGetMatchingService(kIOMasterPortDefault, ioMatch);

	// Do NOT release lookup_obj - that is our caller's responsibility.
	return 0;
}

// Get encryption information for the filesystem at the provided devnode.
// A filesystem is considered encrypted if there's no error and encryption_status is set to true
// (encryption_details will hold detailed information on what encryption properties were found).
errno_t
GetFSEncryptionStatus(const char *bsdname, bool *encryption_status, bool require_FDE, fs_media_encryption_details_t *encryption_details)
{
	bool fs_encrypted = false;
	io_object_t lookup_obj = IO_OBJECT_NULL, parent_obj = IO_OBJECT_NULL;
	CFBooleanRef lvfIsEncr = NULL, rolling;
	errno_t error;

	if (!encryption_status) {
		return EINVAL;
	}

	// Get the io_object_t to begin our registry entry search.
	error = get_lookup_io_obj(bsdname, &lookup_obj);
	if (error) {
		goto finish;
	}

	// Initialize our output.
	if (encryption_details)
		*encryption_details = 0;

	if (is_apfs((char*)bsdname, NULL)) {
		lvfIsEncr = IORegistryEntryCreateCFProperty(lookup_obj, CFSTR(kAPFSEncryptedKey), kCFAllocatorDefault, 0);
		if (lvfIsEncr != NULL && CFBooleanGetValue(lvfIsEncr) == true) {
			const uint32_t fsindex = fsindex_parse(bsdname);

			// Check for additional properties.
			if (IORegistryEntryGetParentEntry(lookup_obj, kIOServicePlane, &parent_obj) == 0) {
				if (IOObjectConformsTo(parent_obj, APFS_CONTAINER_CLASS)) {
					// As of now we think the volume is encrypted,
					// but we must check the VEK state to be entirely sure.
					if (IsVEKStateEncrypted(parent_obj, fsindex)) {
						fs_encrypted = true;

						if (encryption_details)
							*encryption_details |= FS_MEDIA_FDE_ENCRYPTED;
					} else if (!require_FDE) {
						fs_encrypted = true;
					}
				}
				IOObjectRelease(parent_obj);
			}

			if (encryption_details) {
				// Also check for encryption rolling.
				rolling = IORegistryEntryCreateCFProperty(lookup_obj, CFSTR(kAPFSEncryptionRolling), kCFAllocatorDefault, 0);
				if (rolling && CFBooleanGetValue(rolling) == true) {
					*encryption_details |= FS_MEDIA_ENCRYPTION_CONVERTING;
					CFRelease(rolling);
				}
			}
		}
	} else {
		lvfIsEncr = IORegistryEntryCreateCFProperty(lookup_obj, CFSTR(kCoreStorageIsEncryptedKey), nil, 0);
		if (lvfIsEncr != NULL) {
			fs_encrypted = CFBooleanGetValue(lvfIsEncr);
		}
	}

	*encryption_status = fs_encrypted;

finish:
	if (lvfIsEncr)
		CFRelease(lvfIsEncr);
	if (lookup_obj != IO_OBJECT_NULL) {
		IOObjectRelease(lookup_obj);
	}

	return error;
}

static bool
IsEncrypted(const char *bsdname)
{
	bool fs_encrypted = false;

	// Get our encryption status, requiring FDE, and ignoring any errors.
	(void)GetFSEncryptionStatus(bsdname, &fs_encrypted, true, NULL);
	return fs_encrypted;
}

// Get disk image encryption information for the provided devnode.
// The devnode is considered encrypted if there's no error and encryption_status is set to true.
errno_t
GetDiskImageEncryptionStatus(const char *bsdname, bool *encryption_status)
{
	io_object_t lookup_obj = IO_OBJECT_NULL;
	CFBooleanRef encrypted_property;
	errno_t error;

	if (!bsdname || !encryption_status) {
		return EINVAL;
	}

	// Get the io_object_t to begin our registry entry search.
	error = get_lookup_io_obj(bsdname, &lookup_obj);
	if (error) {
		goto finish;
	}

	// Search for the image-encrypted property through all our parent entries.
	encrypted_property = IORegistryEntrySearchCFProperty(lookup_obj,
														 kIOServicePlane,
														 CFSTR(kIOHDIXImageEncryptedProperty),
														 kCFAllocatorDefault,
														 kIORegistryIterateRecursively | kIORegistryIterateParents);
	if (encrypted_property) {
		*encryption_status = (encrypted_property == kCFBooleanTrue);
		CFRelease(encrypted_property);
	} else {
		*encryption_status = false;
	}

finish:
	if (lookup_obj != IO_OBJECT_NULL)
		IOObjectRelease(lookup_obj);

	return error;
}

errno_t
_FSGetMediaEncryptionStatus(CFStringRef devnode, bool *encryption_status, fs_media_encryption_details_t *encryption_details)
{
	char bsdname[MAXPATHLEN + 1];
	bool fs_encrypted = false, di_encrypted = false;
	errno_t error;

	// Check our arguments.
	if (!CFStringGetCString(devnode, bsdname, MAXPATHLEN + 1, kCFStringEncodingUTF8)) {
		return EINVAL;
	} else if (!encryption_status) {
		return EINVAL;
	}

	// First, check if the FS is encrypted (ignoring FDE status).
	error = GetFSEncryptionStatus(bsdname, &fs_encrypted, false, encryption_details);
	if (error) {
		return error;
	}

	if (!fs_encrypted || encryption_details) {
		// If the result will be visible, also check for disk-image encryption.
		if (GetDiskImageEncryptionStatus(bsdname, &di_encrypted) == 0) {
			if (di_encrypted && encryption_details) {
				*encryption_details |= FS_MEDIA_DEV_ENCRYPTED;
			}
		}
	}

	*encryption_status = (fs_encrypted || di_encrypted);
	return 0;
}
