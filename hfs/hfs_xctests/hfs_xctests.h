//
//  hfs_xctests.h
//  hfs_xctests
//
//  Created by Tomer Afek on 07/14/2022.
//

#ifndef hfs_xctests_h
#define hfs_xctests_h

#import <XCTest/XCTest.h>
#import <UVFSPluginTesting/UVFSPluginTests.h>
#import <UVFSPluginTesting/UVFSPluginTestingUtilities.h>
#include "lf_hfs_fsops_handler.h"


@interface HFSUnitTests : UVFSPluginUnitTests
@end


@interface HFSPerformanceTests : UVFSPluginPerformanceTests
@end


@interface HFSFactory : UVFSPluginInterfacesFactory
@end


@interface HFSSetupDelegate : UVFSPluginSetupDelegate

@property (readonly) TestVolumeUtils* volumeUtils;

@end


@interface HFSPluginFilesystem : UVFSPluginFilesystem

-(void)mountTestVolume;

@end

#endif /* hfs_xctests_h */
