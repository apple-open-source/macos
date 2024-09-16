//
//  hfs_xctests_utilities.m
//  hfs_xctests
//
//  Created by Tomer Afek on 07/14/2022.
//

#import "hfs_xctests_utilities.h"
#import "hfsFileSystem.h"

@implementation HFSPluginFilesystem

// Intentionally empty

@end


@implementation HFSFactory

// TODO: Implement this when enabling all the tests.

@end

@implementation HFSSetupDelegate

-(void)deinit
{
    [self.volumeUtils clearTestVolume];
}

-(FSPluginInterfacesFactory *)getFactory:(id<FSTestOps>)fs
{
    return [[HFSFactory alloc] initWithFS:fs];
}

@end

@implementation HFSPluginSetupDelegate

-(instancetype)init
{
    self = [super init];
    if(self) {
        self.volumeUtils = [[TestVolumeUtils alloc] initTestVolume:@"hfs"
                                                              size:@"2G"
                                                         newfsPath:@"/sbin/newfs_hfs"];
    }
    return self;
}

-(id<FSTestOps>)getFileSystem
{
    return [[HFSPluginFilesystem alloc] initWithFSOps:&HFS_fsOps
                                           devicePath:self.volumeUtils.devicePath
                                           volumeName:self.volumeUtils.volumeName
                                            newfsPath:self.volumeUtils.newfsPath];
}

@end

@implementation HFSModuleSetupDelegate

-(instancetype)init
{
    self = [super init];
    if(self) {
        self.volumeUtils = [[TestVolumeUtils alloc] initTestVolume:@"hfs"
                                                              size:@"2G"
                                                         newfsPath:nil];
    }
    return self;
}

-(id<FSTestOps>)getFileSystem
{
    return [[FSTestModuleOps alloc] initWithFSModule:[[HFSFileSystem alloc] init]
                                          devicePath:self.volumeUtils.volumeName
                                          volumeName:self.volumeUtils.newfsPath];
}
@end

