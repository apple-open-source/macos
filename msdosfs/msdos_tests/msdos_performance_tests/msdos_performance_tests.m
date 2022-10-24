//
//  msdos_performance_tests.m
//  msdos_performance_tests
//
//  Created by Tomer Afek on 30/06/2022.
//

#import "msdos_tests.h"
#import "msdos_tests_utilities.h"

@implementation MsdosPerformanceTests

-(void)setUp
{
    [super setDelegate:[[MsdosSetupDelegate alloc] initWithSize:[self getVolumeSize]]];
    [super setUp];
}

-(NSString*)getVolumeSize
{
    @throw [NSException exceptionWithName:NSInternalInconsistencyException
                                   reason:[NSString stringWithFormat:@"You must override %s in a subclass", __FUNCTION__]
                                 userInfo:nil];
}

@end


@implementation MsdosPerformanceTestsFAT12

-(NSString*)getVolumeSize
{
    return @"4M";
}

@end


@implementation MsdosPerformanceTestsFAT16

-(NSString*)getVolumeSize
{
    return @"500M";
}

@end


@implementation MsdosPerformanceTestsFAT32

-(NSString*)getVolumeSize
{
    return @"2G";
}

@end
