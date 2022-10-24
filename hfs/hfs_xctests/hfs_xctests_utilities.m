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

    PRINT_IF_FAILED(self.operations->fsops_init());
    PRINT_IF_FAILED(self.operations->fsops_taste(self.deviceFD));
    PRINT_IF_FAILED(self.operations->fsops_scanvols(self.deviceFD, &sScanVolsReq, &sScanVolsReply));
#if TARGET_OS_OSX
    PRINT_IF_FAILED(self.operations->fsops_check(self.deviceFD, 0, NULL, CHECK));
#endif
    PRINT_IF_FAILED(self.operations->fsops_mount(self.deviceFD, sScanVolsReply.sr_volid, 0, NULL, self.rootNodeAddress));
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
        _volumeUtils = [[TestVolumeUtils alloc] initTestVolumeWithName:@"hfs" size:@"512M" newfsPath:@"/sbin/newfs_hfs"];
    }
    return self;
}

-(void)deinit
{
    [self.volumeUtils clearTestVolume];
}

-(UVFSPluginFilesystem *)getFileSystem
{
    return [[HFSPluginFilesystem alloc] initWithFSOps:&HFS_fsOps devicePath:_volumeUtils.devicePath volumeName:_volumeUtils.volumeName newfsPath:_volumeUtils.newfsPath];
}

-(UVFSPluginInterfacesFactory *)getFactory:(UVFSPluginFilesystem *)fs
{
    return [[HFSFactory alloc] initWithFS:fs];
}

@end
