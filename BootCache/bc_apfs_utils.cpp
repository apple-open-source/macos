#include <sys/types.h>
#include <APFS/APFSConstants.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOMedia.h>
#include <sys/systm.h>
#include "bc_apfs_utils.h"
#include "BootCache_private.h"


uint32_t
apfs_get_fs_flags(dev_t dev)
{
	uint32_t apfs_flags = 0;

	if (dev == nulldev()) {
		return apfs_flags;
	}
	
	OSDictionary *target, *filter;
	OSNumber *number;
	IOService *volume;

	if ((target = IOService::serviceMatching(kIOMediaClass)) != NULL) {
		filter = OSDictionary::withCapacity(2);
		number = OSNumber::withNumber(major(dev), 32);
		filter->setObject(kIOBSDMajorKey, number);
		number->release();
		number = OSNumber::withNumber(minor(dev), 32);
		filter->setObject(kIOBSDMinorKey, number);
		number->release();
		target->setObject(gIOPropertyMatchKey, filter);
		filter->release();
		if ((volume = IOService::copyMatchingService(target)) != NULL) {
			volume->waitQuiet();
			
			if (volume->metaCast(APFS_VOLUME_OBJECT) != NULL) {
				
				OSBoolean *boolean = OSDynamicCast(OSBoolean, volume->getProperty(kAPFSEncryptedKey));
				if (boolean == kOSBooleanTrue) {
					apfs_flags |= BC_FS_ENCRYPTED;
				}
				
				boolean = OSDynamicCast(OSBoolean, volume->getProperty(kAPFSEncryptionRolling));
				if (boolean == kOSBooleanTrue) {
					apfs_flags |= BC_FS_ENC_ROLLING;
				}
			}
			
			volume->release();
		}
		target->release();
	}
	
	return apfs_flags;
}

// Returns 0 on successfully checking, non-0 on error
int
apfs_container_has_encrypted_or_rolling_volumes(IORegistryEntry *container, int * _Nullable has_encrypted_volumes_out, int * _Nullable has_rolling_volumes_out)
{
	IORegistryEntry *volume = NULL;
	OSIterator *iterator = NULL;
	
	if (has_encrypted_volumes_out) {
		*has_encrypted_volumes_out = FALSE;
	}
	if (has_rolling_volumes_out) {
		*has_rolling_volumes_out = FALSE;
	}
	
	iterator = container->getChildIterator(gIOServicePlane);
	if (!iterator) {
		// Assume true since we're only checking to avoid caching unsupported containers, so this is the safer answer when we don't know the truth
		if (has_encrypted_volumes_out) {
			*has_encrypted_volumes_out = TRUE;
		}
		if (has_rolling_volumes_out) {
			*has_rolling_volumes_out = TRUE;
		}
		return ENOENT;
	}
	
	while ((volume = (IORegistryEntry *)iterator->getNextObject()) != NULL) {
		if (volume->metaCast(APFS_VOLUME_OBJECT) == NULL) {
			continue;
		}
		
		if (has_encrypted_volumes_out && *has_encrypted_volumes_out == FALSE) {
			OSBoolean *boolean = OSDynamicCast(OSBoolean, volume->getProperty(kAPFSEncryptedKey));
			if (boolean == kOSBooleanTrue) {
				*has_encrypted_volumes_out = TRUE;
			}
		}
		
		if (has_rolling_volumes_out && *has_rolling_volumes_out == FALSE) {
			OSBoolean *boolean = OSDynamicCast(OSBoolean, volume->getProperty(kAPFSEncryptionRolling));
			if (boolean == kOSBooleanTrue) {
				*has_rolling_volumes_out = TRUE;
			}
		}
		
		if ((!has_rolling_volumes_out || *has_rolling_volumes_out != FALSE) &&
			(!has_encrypted_volumes_out || *has_encrypted_volumes_out != FALSE)) {
			break;
		}
		
	}
	
	iterator->release();
	return 0;
}

dev_t
apfs_container_devno(dev_t volume_dev, uuid_t _Nonnull container_uuid_out, int * _Nullable has_encrypted_volumes_out, int * _Nullable has_rolling_volumes_out)
{
	if (volume_dev == nulldev()) {
		return nulldev();
	}
	
	dev_t container_dev = nulldev();
	
	OSDictionary *target, *filter;
	OSNumber *number;
	IOService *service;
	IORegistryEntry *container, *media;
	OSNumber *dev_major, *dev_minor;
	OSString* uuid_str;
	
	if ((target = IOService::serviceMatching(kIOMediaClass)) != NULL) {
		filter = OSDictionary::withCapacity(2);
		number = OSNumber::withNumber(major(volume_dev), 32);
		filter->setObject(kIOBSDMajorKey, number);
		number->release();
		number = OSNumber::withNumber(minor(volume_dev), 32);
		filter->setObject(kIOBSDMinorKey, number);
		number->release();
		target->setObject(gIOPropertyMatchKey, filter);
		filter->release();
		if ((service = IOService::copyMatchingService(target)) != NULL) {
			service->waitQuiet();
			
			if (service->metaCast(APFS_VOLUME_OBJECT) != NULL) {

				if ((container = service->getParentEntry(gIOServicePlane)) != NULL && container->metaCast(APFS_CONTAINER_OBJECT) != NULL &&
					(media = container->copyParentEntry(gIOServicePlane)) != NULL) {
					if (media->metaCast(APFS_MEDIA_OBJECT) != NULL) {
						
						if ((uuid_str = OSDynamicCast(OSString, media->getProperty(kIOMediaUUIDKey))) != NULL) {
							uuid_parse(uuid_str->getCStringNoCopy(), container_uuid_out);
						}
						
						if ((dev_major = OSDynamicCast(OSNumber, media->getProperty(kIOBSDMajorKey))) != NULL &&
							(dev_minor = OSDynamicCast(OSNumber, media->getProperty(kIOBSDMinorKey))) != NULL) {
							container_dev = makedev(dev_major->unsigned32BitValue(), dev_minor->unsigned32BitValue());
						}
						
						if (has_encrypted_volumes_out || has_rolling_volumes_out) {
							apfs_container_has_encrypted_or_rolling_volumes(container, has_encrypted_volumes_out, has_rolling_volumes_out);
						}
						
					}
					
					media->release();
				}
			}
			service->release();
		}
		target->release();
	}
	return container_dev;
}


// Get device string to pass to vnode_lookup for APFS container device
void
lookup_dev_name(dev_t dev, char* name, int nmlen)
{
	if (!name || nmlen == 0) {
		return;
	}
	name[0] = '\0';
	
	if (dev == nulldev()) {
		return;
	}
	
	OSDictionary *target, *filter;
	OSNumber *number;
	IOService *service;
	OSString* string;
	
	if ((target = IOService::serviceMatching(kIOMediaClass)) != NULL) {
		filter = OSDictionary::withCapacity(2);
		number = OSNumber::withNumber(major(dev), 32);
		filter->setObject(kIOBSDMajorKey, number);
		number->release();
		number = OSNumber::withNumber(minor(dev), 32);
		filter->setObject(kIOBSDMinorKey, number);
		number->release();
		target->setObject(gIOPropertyMatchKey, filter);
		filter->release();
		if ((service = IOService::copyMatchingService(target)) != NULL) {
			service->waitQuiet();
			
			if (service->metaCast(kIOMediaClass) != NULL && (string = OSDynamicCast(OSString, service->getProperty(kIOBSDNameKey))) != NULL) {
				if (name != NULL) {
					snprintf(name, nmlen, "/dev/%s", string->getCStringNoCopy());
				}
			}
			
			service->release();
		}
		target->release();
	}
}
