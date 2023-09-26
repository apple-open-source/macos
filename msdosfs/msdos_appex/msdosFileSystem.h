//
//  msdosFileSystem.h
//  fsmodule
//
//  Created by Noa Osherovich on 27/07/2022.
//

#import <Foundation/Foundation.h>
#import <FSKit/FSKit.h>

NS_ASSUME_NONNULL_BEGIN

#define PRE_DEFINED_SECTOR_SIZE (512)
#define EXFAT_SIGNITURE_LENGTH (11)
#define MAX_DOS_BLOCKSIZE (4096)

@interface msdosFileSystem : FSSimpleFileSystem <FSSimpleFileSystemOperations, FSBlockDeviceOperations>

@end

NS_ASSUME_NONNULL_END
