#include <sys/types.h>
#include <APFS/APFSConstants.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOMedia.h>
#include <sys/systm.h>
#include "bc_utils.h"
#include "BootCache_private.h"
#include "BootCache_debug.h"
#include <sys/vnode.h>
#include <uuid/uuid.h>
#include <apfs/apfs_fsctl.h>


// Grabbed from CoreStorageUserLib.h. Not sure where kernel lib is, but since CS is going away, I don't really exect it'll change
#ifndef kCoreStorageLogicalClassName
#define kCoreStorageLogicalClassName            "CoreStorageLogical"
#endif
#ifndef kCoreStorageIsCompositedKey
#define kCoreStorageIsCompositedKey        "CoreStorage CPDK"      // CFBoolean
#endif


// Returns 0 on successfully checking, non-0 on error
static int
apfs_container_has_encrypted_or_rolling_volumes(IORegistryEntry *container, bool * _Nullable has_encrypted_volumes_out, bool * _Nullable has_rolling_volumes_out)
{
	IORegistryEntry *volume = NULL;
	OSIterator *iterator = NULL;
	
	if (has_encrypted_volumes_out) {
		*has_encrypted_volumes_out = false;
	}
	if (has_rolling_volumes_out) {
		*has_rolling_volumes_out = false;
	}
	
	iterator = container->getChildIterator(gIOServicePlane);
	if (!iterator) {
		// Assume true since we're only checking to avoid caching unsupported containers, so this is the safer answer when we don't know the truth
		if (has_encrypted_volumes_out) {
			*has_encrypted_volumes_out = true;
		}
		if (has_rolling_volumes_out) {
			*has_rolling_volumes_out = true;
		}
		return ENOENT;
	}
	
	while ((volume = (IORegistryEntry *)iterator->getNextObject()) != NULL) {
		if (volume->metaCast(APFS_VOLUME_OBJECT) == NULL) {
			continue;
		}
		
		if (has_encrypted_volumes_out && *has_encrypted_volumes_out == false) {
			OSBoolean *boolean = OSDynamicCast(OSBoolean, volume->getProperty(kAPFSEncryptedKey));
			if (boolean == kOSBooleanTrue) {
				*has_encrypted_volumes_out = true;
			}
		}
		
		if (has_rolling_volumes_out && *has_rolling_volumes_out == false) {
			OSBoolean *boolean = OSDynamicCast(OSBoolean, volume->getProperty(kAPFSEncryptionRolling));
			if (boolean == kOSBooleanTrue) {
				*has_rolling_volumes_out = true;
			}
		}
		
		if ((!has_rolling_volumes_out || *has_rolling_volumes_out != false) &&
			(!has_encrypted_volumes_out || *has_encrypted_volumes_out != false)) {
			break;
		}
		
	}
	
	iterator->release();
	return 0;
}

void
bc_get_volume_info(dev_t volume_dev,
				   uint32_t * _Nullable fs_flags_out,
				   dev_t * _Nullable apfs_container_dev_out,
				   uuid_t _Nullable apfs_container_uuid_out,
				   bool * _Nullable apfs_container_has_encrypted_volumes_out,
				   bool * _Nullable apfs_container_has_rolling_volumes_out)
{
	if (fs_flags_out) {
		*fs_flags_out = 0;
	}
	if (apfs_container_dev_out) {
		*apfs_container_dev_out = nulldev();
	}
	if (apfs_container_uuid_out) {
		uuid_clear(apfs_container_uuid_out);
	}
	if (apfs_container_has_encrypted_volumes_out) {
		*apfs_container_has_encrypted_volumes_out = true;
	}
	if (apfs_container_has_rolling_volumes_out) {
		*apfs_container_has_rolling_volumes_out = true;
	}
	
	if (volume_dev == nulldev() || volume_dev == NODEV) {
		return;
	}
	
	OSDictionary *target, *filter;
	OSNumber *number;
	IOService *volume;
	IORegistryEntry *container, *media, *scheme;
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
		if ((volume = IOService::copyMatchingService(target)) != NULL) {
			volume->waitQuiet();
			
			if (volume->metaCast(APFS_VOLUME_OBJECT) != NULL) {
				if (fs_flags_out) {
					*fs_flags_out |= BC_FS_APFS;
					
					OSBoolean *boolean = OSDynamicCast(OSBoolean, volume->getProperty(kAPFSEncryptedKey));
					if (boolean == kOSBooleanTrue) {
						*fs_flags_out |= BC_FS_APFS_ENCRYPTED;
					}
					
					boolean = OSDynamicCast(OSBoolean, volume->getProperty(kAPFSEncryptionRolling));
					if (boolean == kOSBooleanTrue) {
						*fs_flags_out |= BC_FS_APFS_ENCRYPTION_ROLLING;
					}
				}
				
				if ((container = volume->getParentEntry(gIOServicePlane)) != NULL && container->metaCast(APFS_CONTAINER_OBJECT) != NULL) {
					if ((media = container->getParentEntry(gIOServicePlane)) != NULL && media->metaCast(APFS_MEDIA_OBJECT) != NULL) {
						if ((scheme = media->getParentEntry(gIOServicePlane)) != NULL && scheme->metaCast(APFS_SCHEME_OBJECT) != NULL) {
							
							if (apfs_container_uuid_out) {
								if ((uuid_str = OSDynamicCast(OSString, container->getProperty(kIOMediaUUIDKey))) != NULL) {
									uuid_parse(uuid_str->getCStringNoCopy(), apfs_container_uuid_out);
								}
							}
							
							if (apfs_container_dev_out) {
								if ((dev_major = OSDynamicCast(OSNumber, media->getProperty(kIOBSDMajorKey))) != NULL &&
									(dev_minor = OSDynamicCast(OSNumber, media->getProperty(kIOBSDMinorKey))) != NULL) {
									*apfs_container_dev_out = makedev(dev_major->unsigned32BitValue(), dev_minor->unsigned32BitValue());
								}
							}
							
							if (apfs_container_has_encrypted_volumes_out || apfs_container_has_rolling_volumes_out) {
								apfs_container_has_encrypted_or_rolling_volumes(container, apfs_container_has_encrypted_volumes_out, apfs_container_has_rolling_volumes_out);
							}
							
							if (fs_flags_out) {
								OSBoolean *boolean = OSDynamicCast(OSBoolean, scheme->getProperty(kAPFSContainerIsCompositedKey));
								if (boolean == kOSBooleanTrue) {
									*fs_flags_out |= BC_FS_APFS_FUSION;
								}
							}
							
							
						} else {
							message("No scheme for APFS media");
						}
					} else {
						message("No media for APFS container");
					}
				} else {
					message("No container for APFS volume");
				}
				
				
			} else if (volume->metaCast(kCoreStorageLogicalClassName) != NULL) {
				if (fs_flags_out) {
					*fs_flags_out |= BC_FS_CS;
					
					OSBoolean *boolean = OSDynamicCast(OSBoolean, volume->getProperty(kCoreStorageIsCompositedKey));
					if (boolean == kOSBooleanTrue) {
						*fs_flags_out |= BC_FS_CS_FUSION;
					}
				}
			} else {
				// Non-apfs and non-CoreStorage
			}
			volume->release();
		}
		target->release();
	}
}


/* group_uuid_out will be filled with the UUID of this device's group uuid,
 * or zero'ed out if the device has no grouping.
 */
void
bc_get_group_uuid_for_dev(dev_t dev, uuid_t _Nonnull group_uuid_out)
{
	if (!group_uuid_out) {
		return;
	}

	uuid_clear(group_uuid_out);
	
	if (dev == nulldev() || dev == NODEV) {
		return;
	}
	
	OSDictionary *target, *filter;
	OSNumber *number;
	IOService *matchingService;
	OSString* uuid_str;
	
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
		if ((matchingService = IOService::copyMatchingService(target)) != NULL) {
			matchingService->waitQuiet();
			
			if (matchingService->metaCast(APFS_MEDIA_OBJECT) != NULL) {
				// For APFS containers, the bsd device will match the container's parent APFS_MEDIA_OBJECT, so we need to search the APFS_MEDIA_OBJECT's children for an APFS container
				OSIterator* iterator = matchingService->getChildIterator(gIOServicePlane);
				if (iterator) {
					IORegistryEntry* container = NULL;
					bool foundContainer = false;
					while ((container = (IORegistryEntry *)iterator->getNextObject()) != NULL) {
						if (container->metaCast(APFS_CONTAINER_OBJECT)) {
							foundContainer = true;
							
							debug("Found apfs container via media for device %#x", dev);
							if ((uuid_str = OSDynamicCast(OSString, container->getProperty(kIOMediaUUIDKey))) != NULL) {
								uuid_parse(uuid_str->getCStringNoCopy(), group_uuid_out);
							}
							
							break;
						}
					}
					
					if (!foundContainer) {
						debug("No apfs container for media for device %#x", dev);
					}

					iterator->release();
				} else {
					message("No child iterator for media for device %#x", dev);
				}

			} else if (matchingService->metaCast(APFS_VOLUME_OBJECT) != NULL) {
				IORegistryEntry* container = NULL;

				if ((container = matchingService->getParentEntry(gIOServicePlane)) != NULL && container->metaCast(APFS_CONTAINER_OBJECT) != NULL) {
					
					debug("Found apfs container via volume for device %#x", dev);
					if ((uuid_str = OSDynamicCast(OSString, container->getProperty(kIOMediaUUIDKey))) != NULL) {
						uuid_parse(uuid_str->getCStringNoCopy(), group_uuid_out);
					}
					
				} else {
					message("No container for APFS volume");
				}
				
				
			} else if (matchingService->metaCast(APFS_CONTAINER_OBJECT) != NULL) {
				
				debug("Found apfs container directly for device %#x", dev);
				if ((uuid_str = OSDynamicCast(OSString, matchingService->getProperty(kIOMediaUUIDKey))) != NULL) {
					uuid_parse(uuid_str->getCStringNoCopy(), group_uuid_out);
				}

			} else if (matchingService->metaCast(kCoreStorageLogicalClassName) != NULL) {
				// No groupings for CoreStorage
				debug("No grouping for cs device %#x", dev);
			} else {
				// Non-apfs and non-CoreStorage, no groupings
				debug("No grouping for non-apfs, non-cs device %#x", dev);
			}
			matchingService->release();
		}
		target->release();
	}
}


// Get device string to pass to vnode_lookup for APFS container device
void
lookup_dev_name(dev_t dev, char* name, int nmlen)
{
	if (!name || nmlen == 0) {
		return;
	}
	name[0] = '\0';
	
	if (dev == nulldev() || dev == NODEV) {
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

u_int64_t
apfs_get_inode(vnode_t vp, u_int64_t lblkno, u_int64_t size, vfs_context_t vfs_context)
{
	apfs_bc_getinodenum_t ioctlargs;
	ioctlargs.bc_lblkno = lblkno;
	ioctlargs.bc_bcount = (uint32_t)size;
	ioctlargs.bc_inum = 0;
	
	if (VNOP_IOCTL(vp, APFSIOC_BC_GETINODENUM, (caddr_t)&ioctlargs, 0, vfs_context)) {
		return 0;
	}
	
	return ioctlargs.bc_inum;;
}
