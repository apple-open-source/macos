//
//  bc_apfs_utils.h
//  BootCache
//
//  Created by Brian Tearse-Doyle on 3/27/17.
//

#ifndef bc_apfs_utils_h
#define bc_apfs_utils_h

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// Possible flags are defined in BootCache_private.h
uint32_t apfs_get_fs_flags(dev_t dev);
	
// Returns dev_t for the container, and populates container_uuid_out with the container's UUID
dev_t apfs_container_devno(dev_t volume_dev, uuid_t _Nonnull container_uuid_out, int * _Nullable has_encrypted_volumes_out, int * _Nullable has_rolling_volumes_out);

// Get device string to pass to vnode_lookup for APFS container device
void lookup_dev_name(dev_t dev, char* _Nonnull name, int nmlen);

#ifdef __cplusplus
}
#endif

#endif /* bc_apfs_utils_h */
