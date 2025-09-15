//
//  DATelemetry.h
//  diskarbitrationd
//
//  Created by Andrew Tran on 8/8/24.
//

#ifndef __DISKARBITRATIOND_DATELEMETRY__
#define __DISKARBITRATIOND_DATELEMETRY__

#include <CoreFoundation/CoreFoundation.h>
#include "DADisk.h"

typedef enum DATelemetryFSImplementation {
    DATelemetryFSImplementationKext = 0,
    DATelemetryFSImplementationFSKit,
    DATelemetryFSImplementationUserFS
} DATelemetryFSImplementation;

int DATelemetrySendProbeEvent      ( int status ,
                                     CFStringRef fsType ,
                                     DADiskRef disk ,
                                     uint64_t durationNs ,
                                     int cleanStatus );

int DATelemetrySendFSCKEvent       ( int status ,
                                     DADiskRef disk ,
                                     uint64_t durationNs );

int DATelemetrySendMountEvent      ( int status ,
                                     CFStringRef fsType ,
                                     DATelemetryFSImplementation mountType ,
                                     bool automount ,
                                     bool isExternal ,
                                     uint64_t durationNs );

int DATelemetrySendEjectEvent      ( int status ,
                                     DADiskRef disk ,
                                     pid_t dissenterPid );

int DATelemetrySendTerminationEvent( DADiskRef disk );

int DATelemetrySendUnmountEvent    ( int status ,
                                     DADiskRef disk ,
                                     bool forced ,
                                     pid_t dissenterPid ,
                                     bool dissentedViaAPI ,
                                     uint64_t durationNs );

/* Special telemetry status codes */
#define DA_STATUS_FSTAB_MOUNT_SKIPPED 255
#define DA_STATUS_FSTAB_MOUNT_ADDED   256

/* Filesystem type name for MSDOS EFI volumes */
#define DA_TELEMETRY_TYPE_MSDOS_EFI "msdos-efi"

#endif /* __DISKARBITRATIOND_DATELEMETRY__ */
