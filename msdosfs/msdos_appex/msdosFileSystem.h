/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>
#import <FSKit/FSKit.h>

#include "lib_newfs_msdos.h"
#import "msdosVolume.h"
#import "FATVolume.h"

NS_ASSUME_NONNULL_BEGIN

int wipeFSCallback(newfs_client_ctx_t ctx, WipeFSProperties wipeFSProps);

@interface msdosFileSystem : FSUnaryFileSystem <FSUnaryFileSystemOperations, FSBlockDeviceOperations, FSManageableResourceMaintenanceOperations>

@property msdosVolume            * _Nullable volume;
@property FSBlockDeviceResource  * _Nullable resource;

@end

NS_ASSUME_NONNULL_END
