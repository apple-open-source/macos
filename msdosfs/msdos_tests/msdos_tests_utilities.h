//
//  msdos_tests_utilities.h
//  msdosfs
//
//  Created by Tomer Afek on 04/07/2022.
//

#ifndef msdos_tests_utilities_h
#define msdos_tests_utilities_h

#import "msdos_tests.h"

#define MSDOS_FSTYPENAME "msdos"
#define FAT32_DIRTY_BIT_BYTE_OFFSET_HEX (0x4007)
#define FAT32_DIRTY_BIT_LOCATION_IN_BYTE (3)
#define FAT16_DIRTY_BIT_BYTE_OFFSET_HEX (0x203)
#define FAT16_DIRTY_BIT_LOCATION_IN_BYTE (7)
#define MSDOS_DIRTY_BIT_DIRTY_VALUE (0)
#define MSDOS_TIME_GRANULARITY_IN_SECONDS 2 // according to MSDOS spec
#define MSDOS_OLDEST_SUPPORTED_DATE_UTC 315532800 // according to MSDOS spec

@interface MsdosUnicodeComparator : UnicodeComparator
@end

@interface MsdosSizesInfo : FileSystemSizesInfo

@property FileSystemRecord_s * psFSRecord;

@end


@interface MsdosDiskImageInfo : DiskImageInfo

@property FileSystemRecord_s * psFSRecord;

@end


@interface MsdosMiscInfo : FileSystemMiscInfo

@property FileSystemRecord_s * psFSRecord;

@end


@interface MsdosAttrsInfo : AttrsInfo
@end


@interface MsdosFactory : UVFSPluginInterfacesFactory
@end


@interface MsdosSetupDelegate : UVFSPluginSetupDelegate

@property TestVolumeUtils* volumeUtils;
-(instancetype)initWithSize:(NSString *)volumeSize;

@end

#endif /* msdos_tests_utilities_h */
