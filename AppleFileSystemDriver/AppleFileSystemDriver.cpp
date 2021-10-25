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

#include <APFS/APFSConstants.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IODeviceTreeSupport.h>

#include <libkern/OSByteOrder.h>
#include <libkern/OSAtomic.h>

#include <sys/param.h>
#include <hfs/hfs_format.h>
#include <libkern/crypto/md5.h>
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

enum {
    STATE_IDLE = 0,
    STATE_MATCHING_BOOT_UUID,
    STATE_MATCHING_APFS_UUID,
    STATE_MATCHING_APFS_UUID_FOR_RECOVERY,
    STATE_MATCHING_APFS_RECOVERY_IN_CONTAINER,
    STATE_PUBLISHED,
};

//------------------------------------------

static OSString *
createUUIDStringFromUUID(const uuid_t uu)
{
    uuid_string_t buf;

    uuid_unparse_upper(uu, buf);

    return OSString::withCString(buf);
}

//------------------------------------------

static OSString *
createUUIDStringFromObject(const char *logname, OSObject *const value, uuid_t uuid)
{
    OSString *result = NULL;
    OSString *string = NULL;
    OSData *data = NULL;
    const char *uuidCString = NULL;
    
    if ((string = OSDynamicCast(OSString, value)) != NULL) {
        // use the C string directly
        uuidCString = string->getCStringNoCopy();
    } else if ((data = OSDynamicCast(OSData, value)) != NULL) {
        // use the data after ensuring it is null-terminated
        const int length = data->getLength();
        const char *bytes = (const char *) data->getBytesNoCopy();
        if ((bytes != NULL) && (length > 0) && (bytes[length-1] == '\0')) {
            uuidCString = bytes;
        }
    }
    
    DEBUG_LOG("%s: got UUID string '%s'\n", logname, uuidCString);
    if (!uuidCString) {
        IOLog("%s: Invalid UUID string input\n", logname);
    } else if (uuid_parse(uuidCString, uuid) != 0) {
        IOLog("%s: Invalid UUID string '%s'\n", logname, uuidCString);
    } else {
        result = createUUIDStringFromUUID(uuid);
    }
    
    return result;
}

#pragma mark -
#pragma mark HFS utilities

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
    uint8_t *                  bytes          = 0;
    UInt64                     bytesAt        = 0;
    UInt64                     bufferReadAt   = 0;
    vm_size_t                  bufferSize     = 0;
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
		
        bytes = (uint8_t *) buffer->getBytesNoCopy();
		
        // Open the media with read access.
		
        mediaIsOpen = media->open(media, 0, kIOStorageAccessReader);
        if ( mediaIsOpen == false ) break;
		
        bytesAt = 2 * kHFSBlockSize;
        bufferReadAt = IOTrunc( bytesAt, mediaBlockSize );
        bytesAt -= bufferReadAt;

        mdbPtr = (HFSMasterDirectoryBlock *)&bytes[bytesAt];
        volHdrPtr = (HFSPlusVolumeHeader *)&bytes[bytesAt];
		
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
			
            bytesAt = ((u_int64_t)startBlock * (u_int64_t)allocationBlockSize) +
                ((u_int64_t)firstAllocationBlock * (u_int64_t)kHFSBlockSize) +
                (u_int64_t)(2 * kHFSBlockSize);
			
            bufferReadAt = IOTrunc( bytesAt, mediaBlockSize );
            bytesAt -= bufferReadAt;

            mdbPtr = (HFSMasterDirectoryBlock *)&bytes[bytesAt];
            volHdrPtr = (HFSPlusVolumeHeader *)&bytes[bytesAt];

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
        } else {
	    // status = 0 from earlier successful media->read()
	    status = kIOReturnBadMedia;
	}

    } while (false);
	
    if ( mediaIsOpen )  media->close(media);
    if ( buffer )  buffer->release();

    DEBUG_LOG("%s::%s finishes with status %d\n", kClassName, __func__, status);

    return status;
}

#pragma mark -
#pragma mark APFS utilities

//------------------------------------------

uint16_t
AppleFileSystemDriver::getAPFSRoleForVolume(IOMedia *media)
{
    uint16_t role = APFS_VOL_ROLE_NONE;
    OSNumber *roleNumber = OSDynamicCast(OSNumber, media->getProperty(kAPFSRoleValueKey));
    if (roleNumber != NULL) {
        role = roleNumber->unsigned16BitValue();
    }
    return role;
}

OSString *
AppleFileSystemDriver::copyAPFSContainerUUIDForVolume(IOMedia *volume)
{
    // Find the first kIOMediaUUID among the volume's ancestors. This should
    // work for any topology leading to APFS_CONTAINER_OBJECT or APFS_MEDIA_OBJECT.
    IORegistryEntry *parent = volume->getParentEntry(gIOServicePlane);
    if (parent == NULL) {
        return NULL;
    }
    
    OSObject *value = parent->copyProperty(kIOMediaUUIDKey, gIOServicePlane);
    if (value == NULL) {
        return NULL;
    }
    
    OSString *result = OSDynamicCast(OSString, value);
    if (result == NULL) {
        value->release();
        return NULL;
    }
    
    return result;
}

bool
AppleFileSystemDriver::volumeMatchesAPFSUUID(IOMedia *media, OSString *uuidString, bool matchAnyRoleInGroup)
{
    OSString *uuidProperty = NULL;

    uuidProperty = OSDynamicCast( OSString, media->getProperty(kAPFSVolGroupUUIDKey) );
    if ((uuidProperty != NULL) && uuidProperty->isEqualTo(uuidString)) {
        // This apfs-preboot-uuid is a volume group.
        uint16_t role = getAPFSRoleForVolume(media);
        VERBOSE_LOG("%s: %s property matched (role = 0x%x)\n", __func__, kAPFSVolGroupUUIDKey, (unsigned int) role);
        return (role == APFS_VOL_ROLE_SYSTEM) || matchAnyRoleInGroup;
    }
    
    uuidProperty = OSDynamicCast( OSString, media->getProperty(kIOMediaUUIDKey) );
    if ((uuidProperty != NULL) && uuidProperty->isEqualTo(uuidString)) {
        // This apfs-preboot-uuid is a plain volume UUID.
        VERBOSE_LOG("%s: %s property matched\n", __func__, kIOMediaUUIDKey);
        return true;
    }
    
    return false;
}

bool
AppleFileSystemDriver::volumeMatchesAPFSContainerUUID(IOMedia *media, OSString *uuidString)
{
    bool result = false;
    OSString *containerUUID = copyAPFSContainerUUIDForVolume(media);
    if (containerUUID != NULL) {
        result = containerUUID->isEqualTo(uuidString);
        containerUUID->release();
    }
    return result;
}

bool
AppleFileSystemDriver::volumeMatchesAPFSRole(IOMedia *media, uint16_t expectedRole)
{
    return expectedRole == getAPFSRoleForVolume(media);
}

IONotifier *
AppleFileSystemDriver::startMatchingForAPFSVolumes(IOServiceMatchingNotificationHandler callback)
{
    OSDictionary *matching = NULL;
    IONotifier *notifier = NULL;

    matching = IOService::serviceMatching(APFS_VOLUME_OBJECT);
    if (matching != NULL) {
        notifier = IOService::addMatchingNotification(gIOMatchedNotification, matching, callback, this, NULL, 0);
        matching->release();
    }
    
    return notifier;
}

#pragma mark -
#pragma mark associated-volume-group

/*!
 * Finds the APFS recovery volume in the same container as the requested
 * volume or volume group UUID. Holds +1 refcount through this series of
 * of callbacks:
 *
 *     startWithAPFSUUIDForRecovery() // retained here
 *     checkAPFSUUIDForRecovery()
 *     checkAPFSRecoveryVolumeInContainer()
 *     publishBootMediaAndTerminate() // released here
 */
bool
AppleFileSystemDriver::startWithAPFSUUIDForRecovery(OSObject *uuidProperty)
{
    IONotifier *startedNotifier = NULL;
    
    do {
        if (!OSCompareAndSwap(STATE_IDLE, STATE_MATCHING_APFS_UUID_FOR_RECOVERY, &_state)) break;
        
        // Start matching APFS volumes with the given UUID, but only to
        // find the container whose recovery volume we actually want.
        _uuidString = createUUIDStringFromObject(getName(), uuidProperty, _uuid);
        if (_uuidString == NULL) break;

        retain(); // +1 while notification is active
        startedNotifier = startMatchingForAPFSVolumes(
            OSMemberFunctionCast(IOServiceMatchingNotificationHandler, this, &AppleFileSystemDriver::checkAPFSUUIDForRecovery));
    } while (false);
    
    return (startedNotifier != NULL);
}

/*!
 * Checks if an APFS volume matches the requested UUID. If it does, this latches
 * the container UUID and starts looking for its peer APFS recovery volume.
 */
bool
AppleFileSystemDriver::checkAPFSUUIDForRecovery(void *ref, IOService *service, IONotifier *notifier)
{
    IONotifier *chainedNotifier = NULL;
    IOMedia *media = NULL;
    DEBUG_LOG("%s[%p]::%s -> '%s'\n", getName(), this, __func__, service->getName());

    do {
        // Skip volumes that don't match our UUID.
        media = OSDynamicCast(IOMedia, service);
        if (media == NULL) break;
        if (!volumeMatchesAPFSUUID(media, _uuidString, /*matchAnyRoleInGroup*/ true)) break;
        
        // Stop notifications after the first match.
        if (!OSCompareAndSwap(STATE_MATCHING_APFS_UUID_FOR_RECOVERY, STATE_MATCHING_APFS_RECOVERY_IN_CONTAINER, &_state)) break;
        notifier->remove();
        notifier = NULL;
        
        // Start looking for APFS recovery volumes in this container.
        if (_containerUUID != NULL) _containerUUID->release(); // should never happen
        _containerUUID = copyAPFSContainerUUIDForVolume(media);
        if (_containerUUID == NULL) break;
        
        retain(); // +1 while notification is active
        chainedNotifier = startMatchingForAPFSVolumes(
            OSMemberFunctionCast(IOServiceMatchingNotificationHandler, this, &AppleFileSystemDriver::checkAPFSRecoveryVolumeInContainer));
        if (chainedNotifier == NULL) break;
        
        VERBOSE_LOG("%s[%p]::%s returning TRUE\n", getName(), this, __func__);
        release(); // -1 for completed notification
        return true;
    } while (false);
    
    DEBUG_LOG("%s[%p]::%s returning false\n", getName(), this, __func__);
    return false;
}

/*!
 * Checks if an APFS volume has the recovery role in the desired container. If it does,
 * publish the volume and terminates.
 */
bool
AppleFileSystemDriver::checkAPFSRecoveryVolumeInContainer(void *ref, IOService *service, IONotifier *notifier)
{
    IOMedia *media = NULL;
    DEBUG_LOG("%s[%p]::%s -> '%s'\n", getName(), this, __func__, service->getName());

    do {
        // Skip volumes that don't match our role or container UUID.
        media = OSDynamicCast(IOMedia, service);
        if (media == NULL) break;
        if (!volumeMatchesAPFSRole(media, APFS_VOL_ROLE_RECOVERY)) break;
        if (!volumeMatchesAPFSContainerUUID(media, _containerUUID)) break;
        
        // Stop notifications after the first match.
        if (!OSCompareAndSwap(STATE_MATCHING_APFS_RECOVERY_IN_CONTAINER, STATE_PUBLISHED, &_state)) break;
        notifier->remove();
        notifier = NULL;
        
        // Publish the matching volume.
        VERBOSE_LOG("%s[%p]::%s returning TRUE\n", getName(), this, __func__);
        return publishBootMediaAndTerminate(media); // calls release() for us
    } while (false);
    
    DEBUG_LOG("%s[%p]::%s returning false\n", getName(), this, __func__);
    return false;
}

#pragma mark -
#pragma mark apfs-preboot-uuid

/*!
 * Finds the APFS volume or volume group matching the reqeuested UUID. If the UUID
 * is a volume group, only matches volumes with the system role. Holds +1 refcount
 * through this series of callbacks:
 *
 *     startWithAPFSUUID() // retained here
 *     checkAPFSUUID()
 *     publishBootMediaAndTerminate() // released here
 */
bool
AppleFileSystemDriver::startWithAPFSUUID(OSObject *uuidProperty)
{
    IONotifier *startedNotifier = NULL;
    
    do {
        if (!OSCompareAndSwap(STATE_IDLE, STATE_MATCHING_APFS_UUID, &_state)) break;

        // Start matching APFS volumes with the given UUID.
        _uuidString = createUUIDStringFromObject(getName(), uuidProperty, _uuid);
        if (_uuidString == NULL) break;

        retain(); // +1 while notification is active
        startedNotifier = startMatchingForAPFSVolumes(
            OSMemberFunctionCast(IOServiceMatchingNotificationHandler, this, &AppleFileSystemDriver::checkAPFSUUID));
    } while (false);
    
    return (startedNotifier != NULL);
}

/*!
 * Checks if an APFS volume matches the requested UUID. If it does,
 * publish the volume and terminates.
 */
bool
AppleFileSystemDriver::checkAPFSUUID(void *ref, IOService *service, IONotifier *notifier)
{
    IOMedia *media = NULL;
    DEBUG_LOG("%s[%p]::%s -> '%s'\n", getName(), this, __func__, service->getName());
    
    do {
        // Skip volumes that do not match our UUID.
        media = OSDynamicCast(IOMedia, service);
        if (media == NULL) break;
        if (!volumeMatchesAPFSUUID(media, _uuidString)) break;
    
        // Stop notifications after first match.
        if (!OSCompareAndSwap(STATE_MATCHING_APFS_UUID, STATE_PUBLISHED, &_state)) break;
        notifier->remove();
        notifier = NULL;

        // Publish the matching volume.
        VERBOSE_LOG("%s[%p]::%s returning TRUE\n", getName(), this, __func__);
        return publishBootMediaAndTerminate(media); // calls release() for us
    } while (false);
    
    DEBUG_LOG("%s[%p]::%s returning false\n", getName(), this, __func__);
    return false;
}

#pragma mark -
#pragma mark boot-uuid

/*!
 * Check if an IOMedia or its filesystem match the requested UUID. If it
 * does, publish the volume and terminate. This will reach inside HFS
 * volumes to compute the volume UUID as hfs.util would.
 */
bool
AppleFileSystemDriver::checkBootUUID(void *ref, IOService * service, IONotifier * notifier)
{
    IOMedia *                  media;
    IOReturn                   status         = kIOReturnError;
    OSString *                 contentHint;
    const char *               contentStr;
    VolumeUUID	               volumeUUID;
    OSString *                 uuidProperty;
    uuid_t                     uuid;
    bool                       matched        = false;

    DEBUG_LOG("%s[%p]::%s -> '%s'\n", getName(), this, __func__, service->getName());

    do {
        media = OSDynamicCast( IOMedia, service );
        if (media == 0) break;
		
        // i.e. does it know how big it is / have a block size
        if ( media->isFormatted() == false )  break;
        
        // If the media already has a UUID property, try that first.
        uuidProperty = OSDynamicCast( OSString, media->getProperty(kIOMediaUUIDKey) );
        if (uuidProperty != NULL) {
            if (_uuidString && uuidProperty->isEqualTo(_uuidString)) {
                VERBOSE_LOG("existing UUID property matched\n");
                matched = true;
                break;
            }
        }
        
        // only IOMedia's with content hints are interesting
        contentHint = OSDynamicCast( OSString, media->getProperty(kIOMediaContentHintKey) );
        if (contentHint == NULL)  break;
        contentStr = contentHint->getCStringNoCopy();
        if (contentStr == NULL)  break;
        
        // probe based on content hint
        if ( strcmp(contentStr, "Apple_HFS" ) == 0 ||
             strcmp(contentStr, "Apple_HFSX" ) == 0 ||
             strcmp(contentStr, "Apple_Boot" ) == 0 ||
             strcmp(contentStr, "Apple_Recovery" ) == 0 ||
             strcmp(contentStr, "48465300-0000-11AA-AA11-00306543ECAC" ) == 0 ||  /* APPLE_HFS_UUID */
             strcmp(contentStr, "426F6F74-0000-11AA-AA11-00306543ECAC" ) == 0 ||  /* APPLE_BOOT_UUID */
             strcmp(contentStr, "5265636F-7665-11AA-AA11-00306543ECAC" ) == 0 ) { /* APPLE_RECOVERY_UUID */
            status = readHFSUUID( media, (void **)&volumeUUID );
        } else {
            DEBUG_LOG("contentStr %s\n", contentStr);
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
        OSString *str = createUUIDStringFromUUID(uuid);
        OSString *bsdn = OSDynamicCast(OSString,media->getProperty(kIOBSDNameKey));
        if (str) {
            IOLog("  UUID %s found on volume '%s' (%s)\n", str->getCStringNoCopy(),
		    media->getName(), bsdn ? bsdn->getCStringNoCopy():"");
            str->release();
        }
#endif
        
        if (_uuid) {
            if ( uuid_compare(uuid, _uuid) == 0 ) {
                VERBOSE_LOG("  UUID matched on volume %s\n", media->getName());
                matched = true;
            }
        }
		
    } while (false);

    if (matched && OSCompareAndSwap(STATE_MATCHING_BOOT_UUID, STATE_PUBLISHED, &_state)) {
        notifier->remove();
        notifier = NULL;
        VERBOSE_LOG("%s[%p]::%s returning TRUE\n", getName(), this, __func__);
        return publishBootMediaAndTerminate(media); // calls release() for us
    }
    
    DEBUG_LOG("%s[%p]::%s returning false\n", getName(), this, __func__);
#if DEBUG
//IOSleep(5000);
#endif
    return false;
}

//------------------------------------------

/*!
 * Finds a volume or IOMedia matching the requested UUID. The UUID may be
 * filesystem-specific. Holds +1 refcount through this series of callbacks:
 *
 *     startWithBootUUID() // retained here
 *     checkBootUUID()
 *     publishBootMediaAndTerminate() // released here
 */
bool
AppleFileSystemDriver::startWithBootUUID(OSObject *uuidProperty)
{
    IONotifier *startedNotifier = NULL;
    OSDictionary *matching = NULL;
    OSDictionary *dict = NULL;

    do {
        if (!OSCompareAndSwap(STATE_IDLE, STATE_MATCHING_BOOT_UUID, &_state)) break;
        
        _uuidString = createUUIDStringFromObject(getName(), uuidProperty, _uuid);
        if (_uuidString == NULL) break;

        // Match IOMedia objects matching our criteria (from kext .plist)
        dict = OSDynamicCast( OSDictionary, getProperty( kMediaMatchKey ) );
        if (dict == 0) break;
        
        matching = OSDictionary::withDictionary(dict);
        if (matching == 0) break;

        matching = IOService::serviceMatching( kIOMediaClass, matching );
        if (matching == 0) break;
        
        // Set up notification for newly-appearing devices.
        // This will also notify us about existing devices.
        retain(); // +1 while notification is active
        startedNotifier = IOService::addMatchingNotification(
            gIOMatchedNotification, matching,
            OSMemberFunctionCast(IOServiceMatchingNotificationHandler, this, &AppleFileSystemDriver::checkBootUUID),
            this, NULL, 0);
    } while (false);
    
    if (matching) matching->release();
    
    return (startedNotifier != NULL);
}

#pragma mark -

/*!
 * When AppleFileSystemDriver starts, it registers for notifications to find the
 * intended root volume, evaluating each volume against critieria specified by
 * xnu or the booter (via IODeviceTree:/chosen).
 *
 * AppleFileSystemDriver will retain itself while these matching notifications
 * are active, and finally drop that refcount when the root volume is found.
 * That happens as part of the final call to publishBootMediaAndTerminate(),
 * after which the AppleFileSystemDriver instance should be freed.
 */
bool
AppleFileSystemDriver::start(IOService * provider)
{
    OSObject *uuidProperty = NULL;
    IOService *resourceService = NULL;
    IORegistryEntry *chosenEntry = NULL;
    bool started = false;

    DEBUG_LOG("%s[%p]::%s\n", getName(), this, __func__);
    DEBUG_LOG("%s[%p] provider is '%s'\n", getName(), this, provider->getName());
    
    if (!super::start(provider)) {
        return false;
    }

    _state = STATE_IDLE;
    do {
        // We should never start without these being present.
        resourceService = getResourceService();
        if (resourceService == NULL) break;
        
        chosenEntry = IORegistryEntry::fromPath("/chosen", gIODTPlane);
        if (chosenEntry == NULL) break;
        
        // Check for normal APFS volume + volume group booting.
        // This comes first so associated-volume-group can still be passed
        // as a hint to kcgen even if it boots like FVUnlock in the future.
        uuidProperty = chosenEntry->getProperty("apfs-preboot-uuid");
        if (uuidProperty != NULL) {
            IOLog("%s: using apfs-preboot-uuid\n", getName());
            started = startWithAPFSUUID(uuidProperty);
            break;
        }

        // Check for recoveryOS-style booting. We can't trust boot-uuid here
        // since it may point at iSCRecovery if the target volume is external.
        uuidProperty = chosenEntry->getProperty("associated-volume-group");
        if (uuidProperty != NULL) {
            IOLog("%s: using associated-volume-group\n", getName());
            started = startWithAPFSUUIDForRecovery(uuidProperty);
            break;
        }
        
        // Check for regular IOMedia booting.
        uuidProperty = resourceService->getProperty(kBootUUIDKey);
        if (uuidProperty != NULL) {
            IOLog("%s: using %s\n", getName(), kBootUUIDKey);
            started = startWithBootUUID(uuidProperty);
            break;
        }
        
        // Nothing worked so error out.
        IOLog("%s: error getting boot-uuid property\n", getName());
        break;

    } while (false);
    
    if (!started) {
        _state = STATE_IDLE;
        super::stop(provider);
    }

    if (chosenEntry != NULL) {
        chosenEntry->release();
    }

    DEBUG_LOG("%s[%p]::%s finishes %s\n", getName(), this, __func__, started ? "TRUE" : "false");

    return started;
}

//------------------------------------------

/**
 * Publish the root volume in IOResources and terminate AppleFileSystemDriver.
 * This also calls release() to drop a refcount held by whatever matching notification
 * triggered this. It is generally unsafe to use `this` afterward, so callers should return
 * immediately.
 */
bool
AppleFileSystemDriver::publishBootMediaAndTerminate(IOMedia *media)
{
    OSString *bsdNameString = OSDynamicCast(OSString, media->getProperty(kIOBSDNameKey));
    const char *bsdName = (bsdNameString) ? bsdNameString->getCStringNoCopy() : "unknown";
    const char *mediaName = media->getName();

    IOLog("%s: publishing %s=%s (%s)\n", getName(), kBootUUIDMediaKey, bsdName, mediaName);
    IOService::publishResource( kBootUUIDMediaKey, media );

    // Now that our job is done, get rid of the matching property
    // and kill the driver.
    getResourceService()->removeProperty( kBootUUIDKey );
    terminate( kIOServiceRequired );

    // Drop the retain for asynchronous notifications.
    release( );

    return true;
}

//------------------------------------------

void
AppleFileSystemDriver::free()
{
    DEBUG_LOG("%s[%p]::%s\n", getName(), this, __func__);
    
    if (_uuidString) _uuidString->release();
    if (_containerUUID) _containerUUID->release();

    super::free();
}

//------------------------------------------


