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
 
#include <libkern/OSByteOrder.h>
#include <stdio.h>
#include <unistd.h>

#include "FSFormatName.h"

static CFMutableDictionaryRef __FSLocalizedNameTable = NULL;
static OSSpinLock __FSLocalizedNameTableLock = 0;

CFStringRef FSCopyFormatNameForFSType(CFStringRef fsType, int16_t fsSubtype, bool localized) 
{
    CFStringRef formatName;
    CFStringRef formatNameTableKey;
    CFIndex indx;

    if (NULL == fsType) return NULL;

    // Create a key for cache localized name table (i.e. "0hfs0")
    formatNameTableKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d%@%d"), (localized ? 1 : 0), fsType, fsSubtype);

    // Use OSSpinLock to protect the table accessed from multiple threads
    OSSpinLockLock(&__FSLocalizedNameTableLock);
    formatName = ((NULL == __FSLocalizedNameTable) ? NULL : CFDictionaryGetValue(__FSLocalizedNameTable, (const void *)formatNameTableKey));
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
			}

			/* Get global FSPersonalities.  We need to access this since FSSubType exists only
			 * in global FSPersonalities 
			 */
            CFDictionaryRef globalPersonalities = CFDictionaryGetValue(bundleDict, (const void *) KEY_FS_PERSONALITIES);
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
					formatName = CFDictionaryGetValue(FSNameDict, (const void *)KEY_FS_NAME);
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
            formatName = unknownFSNameString;
        }
        
        // Cache the result
        OSSpinLockLock(&__FSLocalizedNameTableLock);
        if (NULL == __FSLocalizedNameTable) __FSLocalizedNameTable = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(__FSLocalizedNameTable, (const void *)formatNameTableKey, (const void *)formatName);
        OSSpinLockUnlock(&__FSLocalizedNameTableLock);
        
        if (NULL != bundle) CFRelease(bundle); // it has to be released here since formatName might be owned by the bundle
    }

    CFRelease(formatNameTableKey);

    return CFRetain(formatName);
}

CFStringRef _FSCopyLocalizedNameForVolumeFormatAtURL(CFURLRef url) 
{
    CFStringRef formatName = NULL;
    uint8_t buffer[MAXPATHLEN + 1];

    if ((NULL != url) && CFURLGetFileSystemRepresentation(url, true, buffer, MAXPATHLEN)) {
	struct statfs fsInfo;

        if (statfs((char *)buffer, &fsInfo) == 0) {
            CFStringRef fsType = CFStringCreateWithCString(NULL, fsInfo.f_fstypename, kCFStringEncodingASCII);

#ifdef _DARWIN_FEATURE_64_BIT_INODE
            formatName = FSCopyFormatNameForFSType(fsType, fsInfo.f_fssubtype, true);
#else
            formatName = FSCopyFormatNameForFSType(fsType, fsInfo.f_reserved1, true);
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

#ifdef _DARWIN_FEATURE_64_BIT_INODE
            formatName = FSCopyFormatNameForFSType(fsType, fsInfo.f_fssubtype, false);
#else
            formatName = FSCopyFormatNameForFSType(fsType, fsInfo.f_reserved1, false);
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
	
		/* get fsname and fssubtype */
		memset(fsname, MAX_FSNAME, 0);
		if (getfstype(devnodename, fsname, &fssubtype) == true) {
		
			/* get unlocalized string */
			CFStringRef fsType = CFStringCreateWithCString(NULL, fsname, kCFStringEncodingASCII);
			formatName = FSCopyFormatNameForFSType(fsType, fssubtype, true);
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
	
		/* get fsname and fssubtype */
		memset(fsname, MAX_FSNAME, 0);
		if (getfstype(devnodename, fsname, &fssubtype) == true) {
		
			/* get unlocalized string */
			CFStringRef fsType = CFStringCreateWithCString(NULL, fsname, kCFStringEncodingASCII);
			formatName = FSCopyFormatNameForFSType(fsType, fssubtype, false);
			CFRelease(fsType);
		}
	}
	return formatName;
}

/* Return the fsname and subtype number for devnode */
bool getfstype(char *devnode, char *fsname, int *fssubtype)
{
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

#define SW16(x)	OSSwapBigToHostInt16(x)
#define SW32(x)	OSSwapBigToHostInt32(x)

/* Check if the disk is HFS disk and return its subtypes */
bool is_hfs(char *devnode, int *fssubtype)
{
	HFSPlusVolumeHeader *vhp;
	off_t offset = 0;
	char *buffer = NULL;
	int fd = 0;
	bool retval = false;
		
	/* default fssubtype to non-existing value */
	*fssubtype = -1;
	
	buffer = (char *)malloc(MAX_HFS_BLOCK_READ);
	if (!buffer) {
		goto out;
	}
	
	/* open the device */	
	fd = open(devnode, O_RDONLY | O_NDELAY, 0);
	if (fd <= 0) {
		goto out;
	}
	
	/* read volume header (512 bytes, block 2) from the device */
	if (getblk(fd, 2, MAX_HFS_BLOCK_READ, buffer) < MAX_HFS_BLOCK_READ) {
		goto out;
	}
	
	/* Check if it is a HFS volume */
	if (getwrapper((HFSMasterDirectoryBlock *)buffer, &offset)) {
		if (getblk(fd, 2 + (offset/MAX_HFS_BLOCK_READ), MAX_HFS_BLOCK_READ, buffer) < MAX_HFS_BLOCK_READ) {
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
		BTHeaderRec *  bthp;
		off_t  foffset;
	
		foffset = (off_t)SW32(vhp->catalogFile.extents[0].startBlock) * (off_t)SW32(vhp->blockSize);
		if (getblk(fd, (offset/MAX_HFS_BLOCK_READ) + (foffset/MAX_HFS_BLOCK_READ) , MAX_HFS_BLOCK_READ, buffer) < MAX_HFS_BLOCK_READ) {
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
	
	/* read the block */
	if (getblk(fd, 0, MAX_DOS_BLOCKSIZE, buffer) < MAX_DOS_BLOCKSIZE) {
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
