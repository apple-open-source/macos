//
//  kextunload.m
//  kextunload
//
//  Created by jca on 6/25/20
//

#import <Foundation/Foundation.h>
#import <IOKit/kext/OSKextPrivate.h>
#import "../kextunload_main.h"
#import "ShimHelpers.h"


// This function will always result in a call to exit(). If calling this
// function spawns kmutil, then it will call exit() with kmutil's exit status.
void shimKextunloadArgsToKMUtilAndRun(KextunloadArgs *toolArgs)
{
	int exitCode = EX_OK; // what we'll exit with if we don't spawn kmutil

	initArgShimming();
	addArgument(@"unload");

	if (toolArgs->kextBundleIDs) {
		CFIndex count, i;
		count = CFArrayGetCount(toolArgs->kextBundleIDs);
		for (i = 0; i < count; i++) {
			char *kextID = (char *)CFArrayGetValueAtIndex(toolArgs->kextBundleIDs, i);
			CFStringRef kextIDRef = CFStringCreateWithCString(kCFAllocatorDefault, kextID, kCFStringEncodingUTF8);
			if (!kextIDRef) {
				OSKextLogStringError(NULL);
				exitCode = EX_OSERR;
				goto finish;
			}
			addArguments(@[@"-b", (__bridge NSString *)kextIDRef]);
			SAFE_RELEASE_NULL(kextIDRef);
		}
	}
	if (toolArgs->kextURLs) {
		for (NSURL *kextURL in (__bridge NSArray<NSURL *> *)toolArgs->kextURLs) {
			addArguments(@[@"-p", kextURL.path]);
		}
	}
	if (toolArgs->kextClassNames) {
		CFIndex count, i;
		count = CFArrayGetCount(toolArgs->kextClassNames);
		for (i = 0; i < count; i++) {
			char *kextClass = (char *)CFArrayGetValueAtIndex(toolArgs->kextClassNames, i);
			CFStringRef kextClassRef = CFStringCreateWithCString(kCFAllocatorDefault, kextClass, kCFStringEncodingUTF8);
			if (!kextClassRef) {
				OSKextLogStringError(NULL);
				exitCode = EX_OSERR;
				goto finish;
			}
			addArguments(@[@"--class-name", (__bridge NSString *)kextClassRef]);
			SAFE_RELEASE_NULL(kextClassRef);
		}
	}

	if (toolArgs->unloadPersonalities) {
		addArgument(@"--personalities-only");
	}
finish:
	if (exitCode == EX_OK) {
		runWithShimmedArguments();
	}
	exit(exitCode);
}
