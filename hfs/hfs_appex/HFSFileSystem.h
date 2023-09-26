//
//  HFSFileSystem.h
//  hfs_xctests
//
//  Created by Adam Hijaze on 24/08/2022.
//

#import <FSKit/FSKit.h>
#import <Foundation/Foundation.h>

#include "lf_hfs_endian.h"
#include "lf_hfs_vfsutils.h"
#include "lf_hfs_volume_identifiers.h"

NS_ASSUME_NONNULL_BEGIN

#define kMaxLogicalBlockSize (16*1024)


@interface HFSFileSystem : FSSimpleFileSystem <FSSimpleFileSystemOperations, FSBlockDeviceOperations>

@end

NS_ASSUME_NONNULL_END
