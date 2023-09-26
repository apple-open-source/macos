//
//  hfs_xctests_utilities.h
//  hfs
//
//  Created by Adam Hijaze on 25/08/2022.
//

#ifndef hfs_xctests_utilities_h
#define hfs_xctests_utilities_h

#import <UVFSPluginTesting/UVFSPluginTests.h>
#import <UVFSPluginTesting/UVFSPluginTestingUtilities.h>
#include "lf_hfs_fsops_handler.h"

@interface HFSFactory : UVFSPluginInterfacesFactory
@end


@interface HFSSetupDelegate : UVFSPluginSetupDelegate

@property  TestVolumeUtils* volumeUtils;

@end

@interface HFSPluginSetupDelegate : HFSSetupDelegate
@end

@interface HFSModuleSetupDelegate : HFSSetupDelegate
@end

@interface HFSPluginFilesystem : UVFSPluginFilesystem
@end

#endif /* hfs_xctests_utilities_h */
