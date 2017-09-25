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
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>

#define kSCPrefsFile  "/Library/Preferences/SystemConfiguration/com.apple.PowerManagement.plist"
#define kCFPrefsFile  "/Library/Preferences/com.apple.PowerManagement.plist"

@import SystemMigrationUtils.Private;

@import SystemMigration.Private.ConfMigrator;

@interface EnergyPrefsMigratorPlugin : SMConfMigratorPlugin

-(void)pruneSettings;
@end


@implementation EnergyPrefsMigratorPlugin

-(NSTimeInterval)estimateTime
{
    return 5;
}

-(void)pruneSettings
{
    BOOL dstExists = NO;
    NSURL *file = [NSURL fileURLWithPath:@kCFPrefsFile];

    // CFPrefs settings from source are already copied to target filesystem by the time
    // this plugin is called.
    NSURL *dst = [self.targetFilesystem pathToRemoteFile:file exists:&dstExists makeAvailable:YES];
    if (!dst) {
        SMLog(SMLogItemStatus, @"[PM Migration error] Failed to get URL to destination file\n");
        return;
    }

    NSMutableDictionary *prefs = [[NSMutableDictionary alloc] initWithContentsOfURL: dst];
    if (!prefs) {
        SMLog(SMLogItemStatus, @"[PM Migration error] Failed to create preferences dictionary\n");
        return;
    }
    else {
        SMLog(SMLogItemStatus, @"[PM Migration] Target Preferences: %@\n", prefs);
    }
    bool slider = IOPMFeatureIsAvailable(CFSTR(kIOPMUnifiedSleepSliderPrefKey), CFSTR(kIOPMACPowerKey));
    if (slider) {
        // Remove 'System Sleep' setting when target doesn't support changing that setting from
        // System Preferences application
        NSMutableDictionary *acprefs = prefs[@kIOPMACPowerKey];
        NSMutableDictionary *battprefs = prefs[@kIOPMBatteryPowerKey];
        NSMutableDictionary *upsprefs = prefs[@kIOPMUPSPowerKey];

        if (acprefs) {
            [acprefs removeObjectForKey:@kIOPMSystemSleepKey];
        }
        if (battprefs) {
            [battprefs removeObjectForKey:@kIOPMSystemSleepKey];
        }
        if (upsprefs) {
            [upsprefs removeObjectForKey:@kIOPMSystemSleepKey];
        }
    }
    else {
        SMLog(SMLogItemStatus, @"[PM Migration] Unfied slider is not supported\n");
    }
    
    // Apply the settings on the target system
    IOPMSetPMPreferences((__bridge CFDictionaryRef)prefs);

    SMLog(SMLogItemStatus, @"[PM Migration] Prune settings completed\n");
}

-(void)run
{

    NSURL *dst = NULL;
    BOOL srcExists = NO;
    NSURL *file = [NSURL fileURLWithPath:@kSCPrefsFile];
    NSURL *source = [self.sourceFilesystem pathToRemoteFile:file exists:&srcExists makeAvailable:YES];
    if (!source) {
        SMLog(SMLogItemStatus, @"[PM Migration error] Failed to get the path to SC Prefs file\n");
        goto exit;
    }

    if (!srcExists) {
        SMLog(SMLogItemStatus, @"[PM Migration error] No SC Prefs file found\n");
        goto exit;
    }

    dst = [self.targetFilesystem pathToRemoteFile:file exists:nil makeAvailable:YES];
    if (!dst) {
        SMLog(SMLogItemStatus, @"[PM Migration error] Failed to get the path to destination\n");
        goto exit;
    }

    SMLog(SMLogItemStatus, @"[PM Migration] copying %s to %s", source.fileSystemRepresentation, dst.fileSystemRepresentation);

    if (copyfile(source.fileSystemRepresentation, dst.fileSystemRepresentation, NULL, COPYFILE_DATA | COPYFILE_NOFOLLOW)) {
        SMLog(SMLogItemStatus, @"[PM migration error] Failed to copy file: %s", strerror(errno));
        goto exit;
    }

    SMLog(SMLogItemStatus, @"[PM Migration] Complete");

exit:
    [self pruneSettings];

}

@end
