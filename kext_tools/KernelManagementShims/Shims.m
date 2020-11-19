/*
 *  kernelmanagement_shims.c
 *  kext_tools
 *
 *  Copyright 2020 Apple Inc. All rights reserved.
 *
 */

#import <sysexits.h>

#import <pthread/pthread.h>
#import <Foundation/Foundation.h>

#if !(TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
#import <KernelManagement/KernelManagement.h>
#import <KernelManagement/KernelManagement_Private.h>

#import "kextcache_main.h"
#import "kext_tools_util.h"
#endif

bool isKernelManagementLinked() {
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
    /* KM doesn't exist on iPhone */
    return false;
#else
    return NSClassFromString(@"KernelManagementClient") ? true : false;
#endif
}

int KernelManagementLoadKextsWithURLs(CFArrayRef urls)
{
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
	(void)urls;
	return EX_OSERR;
#else
	int result = EX_OSERR;
	int count = (int)CFArrayGetCount(urls);

	NSMutableArray<NSString *> *paths = nil;
	NSArray<NSURL *> *nsurls = nil;
	NSError *error = nil;

	paths = [NSMutableArray arrayWithCapacity:count];
	if (!paths) {
		goto finish;
	}

	nsurls = (__bridge NSArray<NSURL *> *)urls;
	for (int i = 0; i < count; i++) {
		paths[i] = nsurls[i].path;
	}

	if (![[KernelManagementClient sharedClient] loadExtensionsWithPaths:paths withError:&error]) {
		OSKextLogCFString(/* kext */ NULL,
			kOSKextLogErrorLevel | kOSKextLogIPCFlag,
			CFSTR("Error contacting KernelManagement service: %@"),
			(__bridge CFStringRef)error.localizedDescription);
		goto finish;
	}

	result = EX_OK;
finish:
	return result;
#endif // #if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
}
