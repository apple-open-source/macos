//
//  bc_utils.h
//  BootCache
//
//  Created by Brian Tearse-Doyle on 3/27/17.
//

#ifndef bc_utils_h
#define bc_utils_h

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void bc_get_volume_info(dev_t volume_dev,
						uint32_t * _Nullable fs_flags_out, // Possible flags are defined in BootCache_private.h
						dev_t * _Nullable apfs_container_dev_out,
						uuid_t _Nullable apfs_container_uuid_out, // filled with uuid_is_null if no container container
						bool * _Nullable apfs_container_has_encrypted_volumes_out,
						bool * _Nullable apfs_container_has_rolling_volumes_out);

void bc_get_group_uuid_for_dev(dev_t dev, uuid_t _Nonnull group_uuid_out);

// Get device string to pass to vnode_lookup for APFS container device
void lookup_dev_name(dev_t dev, char* _Nonnull name, int nmlen);

// Returns an inode for the given vnode, or 0 if unknown
u_int64_t apfs_get_inode(vnode_t _Nonnull vp, u_int64_t lblkno, u_int64_t size, vfs_context_t _Nonnull vfs_context);

#ifdef __cplusplus
}
#endif

#endif /* bc_utils_h */
