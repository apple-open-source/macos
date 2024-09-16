//
//  hfs_xctests_utilities.h
//  hfs
//
//  Created by Adam Hijaze on 25/08/2022.
//

#ifndef hfs_xctests_utilities_h
#define hfs_xctests_utilities_h

#import <FSKitTesting/FSKitTesting.h>
#import <UVFSPluginTesting/UVFSPluginTests.h>
#import <UVFSPluginTesting/UVFSPluginFilesystem.h>
#import <UVFSPluginTesting/UVFSPluginPerformanceTests.h>
#include "lf_hfs_fsops_handler.h"

@interface HFSFactory : FSPluginInterfacesFactory
@end


@interface HFSSetupDelegate : FSPluginSetupDelegate

@property  TestVolumeUtils* volumeUtils;

@end

@interface HFSPluginSetupDelegate : HFSSetupDelegate
@end

@interface HFSModuleSetupDelegate : HFSSetupDelegate
@end

@interface HFSPluginFilesystem : UVFSPluginFilesystem
@end

#endif /* hfs_xctests_utilities_h */
