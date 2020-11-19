//
//  kextload.m
//  kextload
//
//  Created by jkb on 3/11/20.
//

#import <Foundation/Foundation.h>
#import "../kextload_main.h"
#import "ShimHelpers.h"


// This function will always result in a call to exit(). If calling this
// function spawns kmutil, then it will call exit() with kmutil's exit status.
void shimKextloadArgsToKMUtilAndRun(KextloadArgs *toolArgs)
{
    int exitCode = EX_OK; // what we'll exit with if we don't spawn kmutil

    initArgShimming();
    addArgument(@"load");

    if (toolArgs->kextIDs) {
	for (NSString *kextID in (__bridge NSArray<NSString *> *)toolArgs->kextIDs) {
	    addArguments(@[@"-b", kextID]);
	}
    }
    if (toolArgs->kextURLs) {
	for (NSURL *kextURL in (__bridge NSArray<NSURL *> *)toolArgs->kextURLs) {
	    addArguments(@[@"-p", kextURL.path]);
	}
    }

    /* TODO: Proper support for explicit dependencies of kexts */
    if (toolArgs->dependencyURLs) {
	for (NSURL *dependencyURL in (__bridge NSArray<NSURL *> *)toolArgs->dependencyURLs) {
	    addArguments(@[@"-p", dependencyURL.path]);
	}
    }
    if (toolArgs->repositoryURLs) {
	for (NSURL *repositoryURL in (__bridge NSArray<NSURL *> *)toolArgs->repositoryURLs) {
	    addArguments(@[@"-r", repositoryURL.path]);
	}
    }

    runWithShimmedArguments();
cancel:
    exit(exitCode);
}
