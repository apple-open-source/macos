//
//  hfs_xctests.m
//  hfs_xctests
//
//  Created by Kujan Lauz on 07/12/2022.
//

#import "hfs_xctests.h"


@implementation HFSUnitTests

-(void)setUp {
    [super setDelegate:[[HFSSetupDelegate alloc] init]];
    [super setUp];
}

@end


@implementation HFSPerformanceTests

-(void)setUp {
    [super setDelegate:[[HFSSetupDelegate alloc] init]];
    [super setUp];
}

@end
