/*
 * Copyright (c) 2021 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#import <Foundation/Foundation.h>
#import <Bootability/Bootability.h>
#import <Bootability/BYManagerPriv.h>
#import <os/variant_private.h>
#import <pwd.h> // _PASSWORD_LEN
#include <paths.h>
#include <APFS/APFS.h>
#include "bless.h"
#include "bless_private.h"
#include "structs.h"
#include "protos.h"


NSString *_Nullable collectPasswordWithPrompt(NSString *prompt);
NSString *_Nullable collectStringFromStdin(const unsigned int len);


NSString *_Nullable collectPasswordWithPrompt(NSString *prompt)
{
	const char *password = getpass(prompt.UTF8String);
	if (password == NULL) {
		fprintf(stderr, "Failed to parse the passphrase?\n");
		return nil;
	}
	return [NSString stringWithUTF8String:password];
}

NSString *_Nullable collectStringFromStdin(const unsigned int len)
{
	char buf[len + 1]; // +1 for null terminator
	bzero(&buf, sizeof(buf));
	int i = 0;
	int c = 0;
	while (i < len) {
		c = getchar();
		if (c == EOF || c == '\n') {
			break;
		}
		buf[i++] = c;
	}
	if (i == 0)
		return nil;
	
	NSString *inputString = [NSString stringWithUTF8String:buf];
	return inputString;
}

int blessViaBootability(BLContextPtr context, struct clarg actargs[klast])
{
	NSMutableDictionary *options = [NSMutableDictionary dictionary];
	BYManager *manager = [[BYManager alloc] init];
	int ret = 0;
	BOOL success = YES;
	NSError *error = nil;
	NSString *path = nil;
	NSString *uuid = nil;
	CFStringRef		volUUID = NULL;
	CFStringRef		groupUUID = NULL;
	struct statfs sb;
	io_object_t devMediaObj;

	if (actargs[kdevice].present) {
		char *device = actargs[kdevice].argument;
		
		if (strstr(actargs[kdevice].argument, "/dev/") != NULL) {
			// Extract volume BSD name
			device = &actargs[kdevice].argument[5];
		}
		ret = GetVolumeUUIDs(context, device, &volUUID, &groupUUID);
		if (groupUUID) {
			uuid = (NSString*)(groupUUID);
		} else {
			blesscontextprintf(context, kBLLogLevelError, "Volume %s has no group UUID", actargs[kdevice].argument);
			ret = ENOTSUP;
			goto out;
		}
		devMediaObj = IOServiceGetMatchingService(kIOMasterPortDefault,
												  IOBSDNameMatching(kIOMasterPortDefault,
																	0,
																	actargs[kdevice].argument + strlen(_PATH_DEV)));
		if (!devMediaObj) {
			blesscontextprintf(context, kBLLogLevelError,
							   "Couldn't find I/O Registry information for device %s\n",
							   actargs[kdevice].argument);
			ret = ENOMEM;
			goto out;
		}
		if (IOObjectConformsTo(devMediaObj, APFS_VOLUME_OBJECT) &&
			(actargs[kcreatesnapshot].present || actargs[klastsealedsnapshot].present)) {
			// This is an APFS volume.  We need to mess with the preboot volume.
			ret = BlessPrebootVolume(context, actargs[kdevice].argument + strlen(_PATH_DEV), NULL, NULL, NULL,
									 actargs);
		}
		IOObjectRelease(devMediaObj);
		if (ret) {
			blesscontextprintf(context, kBLLogLevelError,
							   "Couldn't set bless data in preboot volume for device %s\n",
							   actargs[kdevice].argument);
			goto out;
		}
		
	} else {
		ret = extractMountPoint(context, actargs);
		if (ret != 0) {
			blesscontextprintf(context, kBLLogLevelError,  "Could not extract mount point\n");
			goto out;
		}
		if (actargs[kmount].argument) {
			path = [NSString stringWithUTF8String:actargs[kmount].argument];
		}

		if (!path) {
			blesscontextprintf(context, kBLLogLevelError,  "Could not extract path to mount point\n");
			ret = ENOTDIR;
			goto out;
		}
		ret = blsustatfs(actargs[kmount].argument, &sb);
		if (ret != 0) {
			blesscontextprintf(context, kBLLogLevelError,  "Can't statfs %s\n" ,
							   actargs[kmount].argument);
			goto out;
		}
		if (actargs[kcreatesnapshot].present || actargs[klastsealedsnapshot].present) {
			ret = BlessPrebootVolume(context, sb.f_mntfromname + strlen(_PATH_DEV), NULL, NULL, NULL, actargs);
			if (ret) {
				blesscontextprintf(context, kBLLogLevelError,
								   "Couldn't set bless data in preboot volume for mount point %s\n",
								   actargs[kmount].argument);
				goto out;
			}
		}
	}
	//Bootability is not required for create-snapshot option
	if (actargs[kcreatesnapshot].present) {
		goto out;
	}

	if (actargs[ksetboot].present) {
		options[BYBootOptionSetBoot] = @(YES);
	}
	if (actargs[knextonly].present) {
		options[BYBootOptionSetBootOnce] = @(YES);
	}
	if (actargs[kuser].present) {
		manager.username = [NSString stringWithUTF8String:actargs[kuser].argument];
	} else {
		// If we're in the BaseSystem, prompt for an owner username
        // We can never get here unless we're running at least macOS 11.0
        // because this code path is arm-only.  However, we need to protect
        // the call to os_variant_is_basesystem so the compiler won't complain.
        if (__builtin_available(macOS 11.0, *)) {
            if (os_variant_is_basesystem("com.apple.bootability")) {
                fprintf(stdout, "Local owner username: ");
                manager.username = collectStringFromStdin(LINE_MAX);
            } else {
                const char *loginuser = getlogin();
                if (loginuser == NULL) {
                    blesscontextprintf(context, kBLLogLevelError,
                                       "getlogin() failed: %d (%s)\n\n",
                                       errno,
                                       strerror(errno));
                    ret = errno;
                    goto out;
                }
                manager.username = [NSString stringWithUTF8String:loginuser];
            }
        } else {
            // Can never reach here, see above.
            abort();
        }
	}

	if (actargs[kstdinpass].present && actargs[kpasspromt].present) {
		blesscontextprintf(context, kBLLogLevelError,  "Bless doesn't support --stdinpass and --passpromt options at the same time.\n");
	}
	if (actargs[kstdinpass].present) {
		manager.password = collectStringFromStdin(_PASSWORD_LEN);
	} else {
		NSString *prompt = [NSString stringWithFormat:@"Password for %@: ", manager.username];
		manager.password = collectPasswordWithPrompt(prompt);
	}

	if (!manager.password) {
		blesscontextprintf(context, kBLLogLevelError,  "Failed to authenticate owner\n");
		ret = errno;
		goto out;
	}
	if (path) {
		success = [manager makeVolumeBootable:path options:options error:&error];
	} else {
		success = [manager makeVolumeBootableWithGroupUUID:uuid options:options error:&error];
	}

	if (success) {
		blesscontextprintf(context, kBLLogLevelVerbose,  "Bootability succeeded\n");
	} else {
		blesscontextprintf(context, kBLLogLevelError,  "Bootability failed %s\n", error.description.UTF8String);
		ret = (int)error.code;
	}
out:
	[manager release];
	return ret;
}
