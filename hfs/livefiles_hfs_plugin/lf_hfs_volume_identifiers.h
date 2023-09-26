//
//  lf_hfs_volume_identifiers.h
//  livefiles_hfs
//
// The routines on this file are derived from hfsutil_main.c
//
//  Created by Adam Hijaze on 04/10/2022.
//

#ifndef lf_hfs_volume_identifiers_h
#define lf_hfs_volume_identifiers_h

#include <stdio.h>

#include "lf_hfs_format.h"

#define HFS_BLOCK_SIZE   512

/* For requesting the UUID from the FS */
typedef struct UUIDAttrBuf {
    uint32_t info_length;
    uuid_t uu;
} UUIDAttrBuf_t;

/* Volume UUID */
typedef struct volUUID {
    uuid_t uuid;
} volUUID_t;

/* HFS+ internal representation of UUID */
typedef struct hfs_UUID {
    uint32_t high;
    uint32_t low;
} hfs_UUID_t;

/*
 * hfs_GetVolumeUUIDRaw
    
 * Read the UUID from an unmounted volume, by doing direct access to the device.
 * Assumes the caller has already determined that a volume is not mounted
 * on the device.  Once we have the HFS UUID from the finderinfo, convert it to a
 * full UUID and then write it into the output argument provided (volUUIDPtr)

 * Returns: FSUR_IO_SUCCESS, FSUR_IO_FAIL, FSUR_UNRECOGNIZED
 */
int hfs_GetVolumeUUIDRaw(int fd, volUUID_t *volUUIDPtr, int devBlockSize);

/*
 * GetNameFromHFSPlusVolumeStartingAt
 *
 * name_o is to be allocated and freed by the caller
 *
 * Returns: FSUR_IO_SUCCESS, FSUR_IO_FAIL
 */
int hfs_GetNameFromHFSPlusVolumeStartingAt(int fd, off_t hfsPlusVolumeOffset, unsigned char * name_o, int devBlockSize);

#endif /* lf_hfs_volume_identifiers_h */
