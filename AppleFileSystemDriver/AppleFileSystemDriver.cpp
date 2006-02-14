/* 
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
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
 
#include "AppleFileSystemDriver.h"

#include <IOKit/IOLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <storage/RAID/AppleRAIDUserLib.h>

#include <libkern/OSByteOrder.h>

#include <sys/param.h>
#include <hfs/hfs_format.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>
#include <sys/md5.h>
#include <uuid/uuid.h>
#include <uuid/namespace.h>

//------------------------------------------

//#define VERBOSE 1
//#define DEBUG 1
#if DEBUG
#define VERBOSE 1
#define DEBUG_LOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DEBUG_LOG(fmt, args...)
#endif

#if VERBOSE
#define VERBOSE_LOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define VERBOSE_LOG(fmt, args...)
#endif

//------------------------------------------

#define kClassName          "AppleFileSystemDriver"
#define kMediaMatchKey      "media-match"
#define kBootUUIDKey        "boot-uuid"
#define kBootUUIDMediaKey   "boot-uuid-media"

#define super IOService
OSDefineMetaClassAndStructors(AppleFileSystemDriver, IOService)


//------------------------------------------

#if VERBOSE

static OSString *
createStringFromUUID(const uuid_t uu) {
    char buf[64];

    uuid_unparse_upper(uu, buf);

    return OSString::withCString(buf);
}

#endif  // VERBOSE

//------------------------------------------

static IOReturn
createUUIDFromName( const uuid_t uu_space, uint8_t *name_p, unsigned int name_len, uuid_t uu_result )
{
    /*
     * Creates a UUID from a unique "name" in the given "name space".  See version 3 UUID.
     */

    MD5_CTX     md5c;

    assert( sizeof( uuid_t ) == MD5_DIGEST_LENGTH );

    memcpy( uu_result, uu_space, sizeof( uuid_t ) );

    MD5Init( &md5c );
    MD5Update( &md5c, uu_result, sizeof( uuid_t ) );
    MD5Update( &md5c, name_p, name_len );
    MD5Final( uu_result, &md5c );

	// this UUID has been made version 3 style (i.e. via namespace)
	// see "-uuid-urn-" IETF draft (which otherwise copies byte for byte)
    uu_result[6] = 0x30 | ( uu_result[6] & 0x0F );
    uu_result[8] = 0x80 | ( uu_result[8] & 0x3F );

    return kIOReturnSuccess;
}


//------------------------------------------

enum {
    kHFSBlockSize = 512,
    kVolumeUUIDValueLength = 8
};

typedef union VolumeUUID {
    uint8_t bytes[kVolumeUUIDValueLength];
    struct {
        uint32_t high;
        uint32_t low;
    } v;
} VolumeUUID;

//------------------------------------------

IOReturn
AppleFileSystemDriver::readHFSUUID(IOMedia *media, void **uuidPtr)
{
    bool                       mediaIsOpen    = false;
    UInt64                     mediaBlockSize = 0;
    IOBufferMemoryDescriptor * buffer         = 0;
    void *                     bytes          = 0;
    UInt32                     bufferReadAt   = 0;
    UInt32                     bufferSize     = 0;
    IOReturn                   status         = kIOReturnError;
    HFSMasterDirectoryBlock *  mdbPtr         = 0;
    HFSPlusVolumeHeader *      volHdrPtr      = 0;
    VolumeUUID *               volumeUUIDPtr  = (VolumeUUID *)uuidPtr;


    DEBUG_LOG("%s::%s\n", kClassName, __func__);
	
    do {
		
        mediaBlockSize = media->getPreferredBlockSize();
		
        bufferSize = IORound(sizeof(HFSMasterDirectoryBlock), mediaBlockSize);
        buffer     = IOBufferMemoryDescriptor::withCapacity(bufferSize, kIODirectionIn);
        if ( buffer == 0 ) break;
		
        bytes = (void *) buffer->getBytesNoCopy();
		
        mdbPtr = (HFSMasterDirectoryBlock *)bytes;
        volHdrPtr = (HFSPlusVolumeHeader *)bytes;
		
        // Open the media with read access.
		
        mediaIsOpen = media->open(media, 0, kIOStorageAccessReader);
        if ( mediaIsOpen == false ) break;
		
        bufferReadAt = 2 * kHFSBlockSize;
		
        status = media->read(media, bufferReadAt, buffer);
        if ( status != kIOReturnSuccess )  break;
		
        /*
         * If this is a wrapped HFS Plus volume, read the Volume Header from
         * sector 2 of the embedded volume.
         */
        if ( OSSwapBigToHostInt16(mdbPtr->drSigWord) == kHFSSigWord &&
             OSSwapBigToHostInt16(mdbPtr->drEmbedSigWord) == kHFSPlusSigWord) {
			
            u_int32_t   allocationBlockSize, firstAllocationBlock, startBlock, blockCount;
			
            if (OSSwapBigToHostInt16(mdbPtr->drSigWord) != kHFSSigWord) {
                break;
            }
			
            allocationBlockSize = OSSwapBigToHostInt32(mdbPtr->drAlBlkSiz);
            firstAllocationBlock = OSSwapBigToHostInt16(mdbPtr->drAlBlSt);
			
            if (OSSwapBigToHostInt16(mdbPtr->drEmbedSigWord) != kHFSPlusSigWord) {
                break;
            }
			
            startBlock = OSSwapBigToHostInt16(mdbPtr->drEmbedExtent.startBlock);
            blockCount = OSSwapBigToHostInt16(mdbPtr->drEmbedExtent.blockCount);
			
            bufferReadAt = ((u_int64_t)startBlock * (u_int64_t)allocationBlockSize) +
                ((u_int64_t)firstAllocationBlock * (u_int64_t)kHFSBlockSize) +
                (u_int64_t)(2 * kHFSBlockSize);
			
            status = media->read(media, bufferReadAt, buffer);
            if ( status != kIOReturnSuccess )  break;
        }
		
        /*
         * At this point, we have the MDB for plain HFS, or VHB for HFS Plus and HFSX
         * volumes (including wrapped HFS Plus).  Verify the signature and grab the
         * UUID from the Finder Info.
         */
        if (OSSwapBigToHostInt16(mdbPtr->drSigWord) == kHFSSigWord) {
            bcopy((void *)&mdbPtr->drFndrInfo[6], volumeUUIDPtr->bytes, kVolumeUUIDValueLength);
            status = kIOReturnSuccess;
			
        } else if (OSSwapBigToHostInt16(volHdrPtr->signature) == kHFSPlusSigWord ||
                   OSSwapBigToHostInt16(volHdrPtr->signature) == kHFSXSigWord) {
            bcopy((void *)&volHdrPtr->finderInfo[24], volumeUUIDPtr->bytes, kVolumeUUIDValueLength);
            status = kIOReturnSuccess;
        }
        /* else return error */

    } while (false);
	
    if ( mediaIsOpen )  media->close(media);
    if ( buffer )  buffer->release();

    DEBUG_LOG("%s::%s finishes with status %d\n", kClassName, __func__, status);

    return status;
}

//------------------------------------------

static UInt16
checksum(void *p, int count)
{
    UInt32 sum = 0;

    while( count > 1 )  {
        sum += *(UInt16 *)p;
        p = (UInt8 *)p + 2;
        count -= 2;
    }

    if( count > 0 ) {
        sum += OSSwapLittleToHostInt16((UInt16)*(UInt8 *)p);
    }

    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    return (~sum & 0xFFFF);
}

//------------------------------------------

IOReturn
AppleFileSystemDriver::readUFSUUID(IOMedia *media, void **uuidPtr)
{
    bool                       mediaIsOpen    = false;
    UInt64                     mediaBlockSize = 0;
    IOBufferMemoryDescriptor * buffer         = 0;
    void *                     bytes          = 0;
    UInt32                     bufferReadAt   = 0;
    UInt32                     bufferSize     = 0;
    IOReturn                   status         = kIOReturnError;
    VolumeUUID *               volumeUUIDPtr  = (VolumeUUID *)uuidPtr;
    struct ufslabel *          u_label;
    char                       u_magic[4]     = UFS_LABEL_MAGIC;
    UInt16                     cksum;

    DEBUG_LOG("%s::%s\n", kClassName, __func__);
	
    do {
		
        mediaBlockSize = media->getPreferredBlockSize();
		
        bufferSize = IORound(UFS_LABEL_SIZE, mediaBlockSize);
        buffer     = IOBufferMemoryDescriptor::withCapacity(bufferSize, kIODirectionIn);
        if ( buffer == 0 ) break;
		
        bytes = (void *) buffer->getBytesNoCopy();
        u_label = (struct ufslabel *) bytes;
		
        // Open the media with read access.
		
        mediaIsOpen = media->open(media, 0, kIOStorageAccessReader);
        if ( mediaIsOpen == false ) break;
		
        bufferReadAt = UFS_LABEL_OFFSET;
		
        status = media->read(media, bufferReadAt, buffer);
        if ( status != kIOReturnSuccess )  break;
		
        if (bcmp(&u_label->ul_magic, u_magic, sizeof(u_label->ul_magic))) {
            break;
        }
		
        if (OSSwapBigToHostInt32(u_label->ul_version) != UFS_LABEL_VERSION) {
            break;
        }
		
        if (OSSwapBigToHostInt16(u_label->ul_namelen) > UFS_MAX_LABEL_NAME) {
            break;
        }
		
        cksum = u_label->ul_checksum;
        u_label->ul_checksum = 0;
		
        if (checksum((void *)u_label, sizeof(*u_label)) != cksum) {
            break;
        }
        
        // Label is OK; now read UUID.
        bcopy((void *)&u_label->ul_uuid, volumeUUIDPtr->bytes, kVolumeUUIDValueLength);
		
        status = kIOReturnSuccess;

    } while (false);
	
    if ( mediaIsOpen )  media->close(media);
    if ( buffer )  buffer->release();

    DEBUG_LOG("%s::%s finishes with status %d\n", kClassName, __func__, status);

    return status;
}

//------------------------------------------

bool
AppleFileSystemDriver::mediaNotificationHandler(
                                        void * target, void * ref,
                                        IOService * service )
{
    AppleFileSystemDriver *    fs;
    IOMedia *                  media;
    IOReturn                   status         = kIOReturnError;
    OSString *                 contentHint;
    const char *               contentStr;
    VolumeUUID	               volumeUUID;
    OSString *                 uuidProperty;
    uuid_t                     uuid;
    bool                       matched        = false;

    DEBUG_LOG("%s[%p]::%s -> '%s'\n", kClassName, target, __func__, service->getName());

    do {
        fs = OSDynamicCast( AppleFileSystemDriver, (IOService *)target );
        if (fs == 0) break;
		
        media = OSDynamicCast( IOMedia, service );
        if (media == 0) break;
		
        if ( media->isFormatted() == false )  break;

        // If the media already has a UUID property, try that first.
        uuidProperty = OSDynamicCast( OSString, media->getProperty("UUID") );
        if (uuidProperty != NULL) {
            if (fs->_uuidString && uuidProperty->isEqualTo(fs->_uuidString)) {
                matched = true;
                break;
            }
        }
        
	// only IOMedia's with content hints (perhaps empty) are interesting
        contentHint = OSDynamicCast( OSString, media->getProperty(kIOMediaContentHintKey) );
        if (contentHint == NULL)  break;
        contentStr = contentHint->getCStringNoCopy();
        if (contentStr == NULL)  break;
        
        // probe based on content hint, but if the hint is 
        // empty and we see RAID, probe for anything we support
        if ( strcmp(contentStr, "Apple_HFS" ) == 0 ||
             strcmp(contentStr, "Apple_HFSX" ) == 0 ||
             strcmp(contentStr, "Apple_Boot" ) == 0 ||
             strcmp(contentStr, "48465300-0000-11AA-AA11-00306543ECAC" ) == 0 ||
             strcmp(contentStr, "426F6F74-0000-11AA-AA11-00306543ECAC" ) == 0 ) {
            status = readHFSUUID( media, (void **)&volumeUUID );
        } else if ( strcmp(contentStr, "Apple_UFS") == 0 ) {
            status = readUFSUUID( media, (void **)&volumeUUID );
        } else if (strlen(contentStr) == 0 &&
                   media->getProperty(kAppleRAIDIsRAIDKey) == kOSBooleanTrue) {
            // RAIDv1 has a content hint but is empty
            status = readHFSUUID( media, (void **)&volumeUUID );
            if (status != kIOReturnSuccess) {
                status = readUFSUUID( media, (void **)&volumeUUID );
            }
        } else {
            break;
        }
        
        if (status != kIOReturnSuccess) {
            break;
        }
		
        if (createUUIDFromName( kFSUUIDNamespaceSHA1,
                                volumeUUID.bytes, kVolumeUUIDValueLength,
                                uuid ) != kIOReturnSuccess) {
            break;
        }
        
#if VERBOSE
        OSString *str = createStringFromUUID(uuid);
        if (str) {
            IOLog("  UUID %s found on volume '%s'\n", str->getCStringNoCopy(), media->getName());
            str->release();
        }
#endif
        
        if (fs->_uuid) {
            if ( uuid_compare(uuid, fs->_uuid) == 0 ) {
                VERBOSE_LOG("  UUID matched on volume %s\n", media->getName());
                matched = true;
            }
        }
		
    } while (false);

    if (matched) {
			
        // prevent more notifications, if notifier is available
        if (fs->_notifier != NULL) {
            fs->_notifier->remove();
            fs->_notifier = NULL;
        }
			
        DEBUG_LOG("%s::%s publishing boot-uuid-media '%s'\n", kClassName, __func__, media->getName());
        IOService::publishResource( kBootUUIDMediaKey, media );

        // Now that our job is done, get rid of the matching property
        // and kill the driver.
        fs->getResourceService()->removeProperty( kBootUUIDKey );
        fs->terminate( kIOServiceRequired );

        VERBOSE_LOG("%s[%p]::%s returning TRUE\n", kClassName, target, __func__);
            
        return true;
    }
    
    DEBUG_LOG("%s[%p]::%s returning false\n", kClassName, target, __func__);
    return false;
}

//------------------------------------------

bool
AppleFileSystemDriver::start(IOService * provider)
{
    OSDictionary *      matching;
    OSString *          uuidString;
    const char *        uuidCString;
    IOService *         resourceService;
    OSDictionary *      dict;


    DEBUG_LOG("%s[%p]::%s\n", getName(), this, __func__);

    DEBUG_LOG("%s provider is '%s'\n", getName(), provider->getName());

    do {
        resourceService = getResourceService();
        if (resourceService == 0) break;

        uuidString = OSDynamicCast( OSString, resourceService->getProperty("boot-uuid") );
        if (uuidString) {
            _uuidString = uuidString;
            _uuidString->retain();
            uuidCString = uuidString->getCStringNoCopy();
            DEBUG_LOG("%s: got UUID string '%s'\n", getName(), uuidCString);
            if (uuid_parse(uuidCString, _uuid) != 0) {
                IOLog("%s: Invalid UUID string '%s'\n", getName(), uuidCString);
                break;
            }
        } else {
            IOLog("%s: Error getting boot-uuid property\n", getName());
            break;
        }
                
        // Match IOMedia objects matching our criteria (from kext .plist)
        dict = OSDynamicCast( OSDictionary, getProperty( kMediaMatchKey ) );
        if (dict == 0) break;
        
        dict = OSDictionary::withDictionary(dict);
        if (dict == 0) break;

        matching = IOService::serviceMatching( "IOMedia", dict );
        if ( matching == 0 )
            return false;
        
        // Set up notification for newly-appearing devices.
        // This will also notify us about existing devices.
        
        _notifier = IOService::addNotification( gIOMatchedNotification, matching,
                                                &mediaNotificationHandler,
                                                this, 0 );

        DEBUG_LOG("%s[%p]::%s finishes TRUE\n", getName(), this, __func__);

        return true;

    } while (false);
    
    DEBUG_LOG("%s[%p]::%s finishes false\n", getName(), this, __func__);

    return false;
}

//------------------------------------------

void
AppleFileSystemDriver::free()
{
    DEBUG_LOG("%s[%p]::%s\n", getName(), this, __func__);
    
    if (_uuidString) _uuidString->release();
    if (_notifier) _notifier->remove();

    super::free();
}

//------------------------------------------


