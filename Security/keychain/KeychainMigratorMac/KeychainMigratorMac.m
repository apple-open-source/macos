/*
 * Copyright (c) 2025 Apple Inc. All Rights Reserved.
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
#import <SystemMigration/SystemMigration.h>
#import <SystemMigration/SystemMigrationPriv.h>

@interface KeychainMigratorMac : SMSystemRulePlugin

@end

@implementation KeychainMigratorMac

- (NSTimeInterval)estimateTime {
    return 0.1; // 1/10 of a second
}

- (void)run {
    SMMigrationRequest* migrationRequest = self.migrationRequest;
    SMLog(SMLogItemStatus, @"KeychainMigratorMac plugin will run. uid=%d,  migrationRequest.type=%lu, migrationRequest.state=%lu", getuid(), migrationRequest.type, migrationRequest.state);

    if (migrationRequest.type == SMRequestTypeStandard) {
        // migrate protected entropy files
        NSURL *systemKeysDir = [NSURL fileURLWithPath:@"/var/db/SystemKeys/" isDirectory:YES];
        BOOL exists = NO;

        // first determine the source NSURL
        NSURL *source = [self.sourceFilesystem pathToRemoteFile:systemKeysDir exists:&exists makeAvailable:YES];
        if (!source || !exists) {
            SMLog(SMLogItemStatus, @"KeychainMigratorMac nothing to move from source file system.");
            return;
        }

        // then determine the target NSURL
        NSURL *target = [self.targetFilesystem pathToRemoteFile:systemKeysDir exists:&exists makeAvailable:YES];
        if (!target) {
            SMLog(SMLogItemStatus, @"KeychainMigratorMac could not get target directory on file system.");
            return;
        }

        // create the target directory if it doesn't exist
        NSError *error = nil;
        BOOL success = [[NSFileManager defaultManager] createDirectoryAtURL:target
                                                withIntermediateDirectories:YES
                                                                 attributes:nil
                                                                      error:&error];
        if (!success) {
            SMLog(SMLogItemStatus, @"KeychainMigratorMac failed to create target directory: %@", error);
            return;
        }

        // copy files from source to target directory
        NSFileManager *fileManager = [NSFileManager defaultManager];
        NSArray<NSURL *> *sourceFiles = [fileManager contentsOfDirectoryAtURL:source
                                                   includingPropertiesForKeys:@[NSURLIsRegularFileKey]
                                                                      options:NSDirectoryEnumerationSkipsHiddenFiles
                                                                        error:&error];
        if (!sourceFiles) {
            SMLog(SMLogItemStatus, @"KeychainMigratorMac failed to enumerate source directory: %@", error);
            return;
        }

        NSUInteger copiedCount = 0;
        for (NSURL *sourceFile in sourceFiles) {
            NSNumber *isFile = nil;
            if ([sourceFile getResourceValue:&isFile forKey:NSURLIsRegularFileKey error:nil] && [isFile boolValue]) {
                NSString *fileName = [sourceFile lastPathComponent];
                NSURL *targetFile = [target URLByAppendingPathComponent:fileName];

                // Remove existing file if it exists
                if ([fileManager fileExistsAtPath:[targetFile path]]) {
                    [fileManager removeItemAtURL:targetFile error:nil];
                }

                BOOL copySuccess = [fileManager copyItemAtURL:sourceFile toURL:targetFile error:&error];
                if (copySuccess) {
                    copiedCount++;
                    SMLog(SMLogItemStatus, @"KeychainMigratorMac copied file: %@", fileName);
                } else {
                    SMLog(SMLogItemStatus, @"KeychainMigratorMac failed to copy file %@: %@", fileName, error);
                }
            }
        }

        SMLog(SMLogItemStatus, @"KeychainMigratorMac migrated %lu files. systemKeyDir=%@, source=%@, target=%@", (unsigned long)copiedCount, systemKeysDir, source, target);
    }

    SMLog(SMLogItemStatus, @"KeychainMigratorMac ran plugin. uid=%d, migrationRequest.type=%lu, migrationRequest.state=%lu", getuid(), migrationRequest.type, migrationRequest.state);
}

@end
