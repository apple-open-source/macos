//
//  ShimHelpers.m
//  kextcache
//
//  Created by jkb on 3/11/20.
//

#import <Foundation/Foundation.h>

#import "Shims.h"
#import "../kext_tools_util.h"

#if !(TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR) && !TARGET_OS_BRIDGE

static NSTask *shimmedTask = nil;
static NSMutableArray *shimmedTaskArguments = nil;

void initArgShimming()
{
    shimmedTask = [[NSTask alloc] init];
    [shimmedTask setLaunchPath:@"/usr/bin/kmutil"];
    shimmedTaskArguments = [[NSMutableArray alloc] init];
}

void addArguments(NSArray<NSString *> *arguments)
{
    [shimmedTaskArguments addObjectsFromArray:arguments];
}

void addArgument(NSString *argument)
{
    [shimmedTaskArguments addObject:argument];
}

NSString *createStringFromShimmedArguments()
{
    NSString *allArguments = [shimmedTaskArguments componentsJoinedByString:@" "];
    return [NSString stringWithFormat:@"%@ %@", shimmedTask.launchPath, allArguments];
}

void runWithShimmedArguments()
{
    OSKextLogCFString(/* kext */ NULL,
	    kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
	    CFSTR("Executing: %@"),
		(__bridge CFStringRef)createStringFromShimmedArguments());

    [shimmedTask setArguments:shimmedTaskArguments];
    [shimmedTask launch];
    [shimmedTask waitUntilExit];
    exit([shimmedTask terminationStatus]);
}

#else // #if !(TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR) && !TARGET_OS_BRIDGE

void initArgShimming()
{
}

void addArguments(NSArray<NSString *> *arguments)
{
	(void)arguments;
}

void addArgument(NSString *argument)
{
	(void)argument;
}

NSString *createStringFromShimmedArguments()
{
	return NULL;
}

void runWithShimmedArguments()
{
}

#endif // #if !(TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR) && !TARGET_OS_BRIDGE
