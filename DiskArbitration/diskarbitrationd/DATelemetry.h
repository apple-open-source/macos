//
//  DATelemetry.h
//  diskarbitrationd
//
//  Created by Andrew Tran on 8/8/24.
//

#ifndef __DISKARBITRATIOND_DATELEMETRY__
#define __DISKARBITRATIOND_DATELEMETRY__

#include <CoreFoundation/CoreFoundation.h>

int DATelemetrySendProbeEvent      ( int status , CFStringRef fsType , CFStringRef fsImplementation , uint64_t durationNs , int cleanStatus );
int DATelemetrySendFSCKEvent       ( int status , CFStringRef fsType , CFStringRef fsImplementation , uint64_t durationNs , uint64_t volumeSize );
int DATelemetrySendMountEvent      ( int status , CFStringRef fsType , bool useUserFS , uint64_t durationNs );
int DATelemetrySendEjectEvent      ( int status , CFStringRef fsType , pid_t dissenterPid );
int DATelemetrySendTerminationEvent( CFStringRef fsType ,
                                     CFStringRef fsImplementation ,
                                     bool isMounted ,
                                     bool isAppeared ,
                                     bool isProbing ,
                                     bool isFSCKRunning ,
                                     bool isMounting ,
                                     bool isUnrepairable ,
                                     bool isRemoved );
int DATelemetrySendUnmountEvent    ( int status , CFStringRef fsType , CFStringRef fsImplementation ,
                                     bool forced , pid_t dissenterPid ,
                                     bool dissentedViaAPI , uint64_t durationNs );

#endif /* Header_h */
