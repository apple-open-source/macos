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

@interface HFSUnitTests : UVFSPluginUnitTests

-(void) testFileChangeModeUpdateChangeTime;

@end

@interface HFSPluginUnitTests : HFSUnitTests
@end

@interface HFSModuleUnitTests : HFSUnitTests
@end

@interface HFSPerformanceTests : UVFSPluginPerformanceTests
@end

#endif /* hfs_xctests_h */
