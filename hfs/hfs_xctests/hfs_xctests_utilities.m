//
//  hfs_xctests_utilities.m
//  hfs_xctests
//
//  Created by Tomer Afek on 07/14/2022.
//

#import "hfs_xctests.h"

@implementation HFSPluginFilesystem

/** In HFS we skip the fsops_check operation in iOS, because it fails when using a flat image (which we use in iOS) */
-(void)mountTestVolume {
    UVFSScanVolsRequest sScanVolsReq = {0};
    UVFSScanVolsReply sScanVolsReply = {0};

    PRINT_IF_FAILED([self initFSOps]);
    PRINT_IF_FAILED([self taste:self.deviceFD]);
    PRINT_IF_FAILED([self scanVols:self.deviceFD request:&sScanVolsReq reply:&sScanVolsReply]);
#if TARGET_OS_OSX
    PRINT_IF_FAILED([self check:self.deviceFD volumeID:0 creds:NULL how:CHECK]);
#endif
    PRINT_IF_FAILED([self mount:self.deviceFD volumeID:sScanVolsReply.sr_volid flags:0 creds:NULL rootNode:self.rootNodeAddress]);
    XCTAssert(self.rootNode != NULL);
}

@end


@implementation HFSFactory

// TODO: Implement this when enabling all the tests.

@end


@implementation HFSSetupDelegate

-(instancetype)init
{
    self = [super init];
    if(self) {
        _volumeUtils = [[TestVolumeUtils alloc] initTestVolume:@"hfs"
                                                          size:@"512M"
                                                     newfsPath:@"/sbin/newfs_hfs"];
    }
    return self;
}

-(void)deinit
{
    [self.volumeUtils clearTestVolume];
}

-(id<FSTestOps>)getFileSystem
{
    return [[HFSPluginFilesystem alloc] initWithFSOps:&HFS_fsOps devicePath:_volumeUtils.devicePath volumeName:_volumeUtils.volumeName newfsPath:_volumeUtils.newfsPath];
}

-(UVFSPluginInterfacesFactory *)getFactory:(id<FSTestOps>)fs
{
    return [[HFSFactory alloc] initWithFS:fs];
}

@end
