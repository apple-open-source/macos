//
//  migrateenergyprefs.m
//  PowerManagement
//
//  Created by dekom on 1/25/16.
//
//

#import <Foundation/Foundation.h>
#include <stdio.h>
#include <copyfile.h>

#define kPMPrefsFile  "/Library/Preferences/SystemConfiguration/com.apple.PowerManagement.plist"

@import SystemMigrationUtils.Private;

@import SystemMigration.Private.ConfMigrator;

@interface EnergyPrefsMigratorPlugin : SMConfMigratorPlugin

@end


@implementation EnergyPrefsMigratorPlugin

-(NSTimeInterval)estimateTime
{
    return 5;
}

-(void)run
{

    BOOL srcExists = NO;
    NSURL *file = [NSURL fileURLWithPath:@kPMPrefsFile];
    NSURL *source = [self.sourceFilesystem pathToRemoteFile:file exists:&srcExists makeAvailable:YES];
    if (!source) {
        SMLog(SMLogItemStatus, @"[PM Migration error] Failed to get the path to source\n");
        return;
    }

    if (!srcExists) {
        SMLog(SMLogItemStatus, @"[PM Migration error] Source file doesn't exist\n");
        return;
    }

    NSURL *dst = [self.targetFilesystem pathToRemoteFile:file exists:nil makeAvailable:YES];
    if (!dst) {
        SMLog(SMLogItemStatus, @"[PM Migration error] Failed to get the path to destination\n");
        return;
    }

    SMLog(SMLogItemStatus, @"[PM Migration] copying %s to %s", source.fileSystemRepresentation, dst.fileSystemRepresentation);

    if (copyfile(source.fileSystemRepresentation, dst.fileSystemRepresentation, NULL, COPYFILE_DATA | COPYFILE_NOFOLLOW)) {
        SMLog(SMLogItemStatus, @"[PM migration error] Failed to copy file: %s", strerror(errno));
        return;
    }

    SMLog(SMLogItemStatus, @"[PM Migration] Complete");
}

@end
