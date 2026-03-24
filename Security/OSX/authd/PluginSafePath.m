//
//  PreloginUserDb.m
//  authd
//
//  Copyright © 2019 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "PluginSafePath.h"
#import "debugging.h"
#include <libproc.h>

AUTHD_DEFINE_LOG

static NSString *kPluginStagingDir = @"/Library/Security/SecurityAgentPlugins/StagedPlugins";
NSMutableDictionary *gStagedPluginList = nil;
dispatch_queue_t gAccessQueue = NULL;

static void ensure_inited(void) {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        gStagedPluginList = @{}.mutableCopy;
        gAccessQueue = dispatch_queue_create("com.apple.security.auth.plugin.access", DISPATCH_QUEUE_SERIAL);
    });
}

// removes folder and all its subfolders at *path*
// force means that in case of the failue, permissions and owner are fixed to help
// the operation to succeed
// returns YES if everything was removed, NO otherwise
static BOOL _remove_dir_content(NSString *path, BOOL force, BOOL includingRoot) {
    OSStatus retval = YES;
    NSFileManager *fileManager = [NSFileManager defaultManager];
    __block NSError *error;

    BOOL (^removeItemAtPath)(NSString *itemPath) = ^BOOL(NSString *itemPath) {
        if (![fileManager removeItemAtPath:itemPath error:&error]) {
            os_log_error(AUTHD_LOG, "Failed to remove %{public}@: %{public}@", itemPath, error.description);
            if (!force) {
                return NO;
            }
            chown(itemPath.UTF8String, getuid(), getgid());
            chmod(itemPath.UTF8String, S_IRWXU | S_IRGRP | S_IXGRP); // 0700
            if (![fileManager removeItemAtPath:path error:&error]) {
                os_log_error(AUTHD_LOG, "Failed again to remove %{public}@: %{public}@", itemPath, error.description);
                return NO;
            } else {
                os_log_info(AUTHD_LOG, "After permissions were fixed, %{public}@ was removed", itemPath);
                return YES;
            }
        }
        return YES;
    };
    
    NSArray *contents = [fileManager contentsOfDirectoryAtPath:path error:&error];
    if (!contents) {
      os_log_error(AUTHD_LOG, "Error getting content of %{public}@: %{public}@", path, error.description);
      return errAuthorizationDenied;
    }
    for (NSString *item in contents) {
        NSString *fullPath = [path stringByAppendingPathComponent:item];
        BOOL isDir = NO;
        if ([fileManager fileExistsAtPath:fullPath isDirectory:&isDir] && isDir) {
            if (!_remove_dir_content(fullPath, force, YES)) {
                retval = NO;
            }
        } else {
            if (!removeItemAtPath(fullPath)) {
                retval = NO;
            }
        }
    }
    if (includingRoot) {
        if (!removeItemAtPath(path)) {
            retval = NO;
        }
    }
    return retval;
}

Boolean plugin_stagedir_clean(void) {
    os_log(AUTHD_LOG, "Clearing stagedir %{public}@", kPluginStagingDir);
    return _remove_dir_content(kPluginStagingDir, YES, NO);
}

OSStatus plugin_stage(pid_t instance, const char *originalPath, char **safePath) {
    __block OSStatus status = errAuthorizationDenied;
    
    ensure_inited();
    os_log_debug(AUTHD_LOG, "Going to stage plugin %{public}s", originalPath);
    // Input validation
    if (!originalPath || strlen(originalPath) == 0) {
        os_log_error(AUTHD_LOG, "Invalid plugin path provided");
        return errAuthorizationInvalidPointer;
    }
    
    NSString *originalPathString = [NSString stringWithUTF8String:originalPath];
    if (!originalPathString) {
        os_log_error(AUTHD_LOG, "Failed to create NSString from plugin path");
        return errAuthorizationInvalidPointer;
    }
    
    NSString *pluginName = [originalPathString lastPathComponent];
    
    // Path traversal protection - ensure plugin name doesn't contain path separators
    if ([pluginName containsString:@"/"] || [pluginName containsString:@".."] ||
        [pluginName isEqualToString:@"."] || [pluginName isEqualToString:@""]) {
        os_log_error(AUTHD_LOG, "Invalid plugin name: %{public}@", pluginName);
        return errAuthorizationDenied;
    }
        
    __block NSString *safePathString = [kPluginStagingDir stringByAppendingPathComponent:pluginName];
    dispatch_sync(gAccessQueue, ^{
        NSFileManager *fileManager = [NSFileManager defaultManager];
        if (![fileManager fileExistsAtPath:originalPathString]) {
            status = errAuthorizationInvalidPointer;
            os_log_debug(AUTHD_LOG, "Plugin %{public}s does not exist", originalPath);
            return;
        }

        NSMutableIndexSet *pids = [gStagedPluginList objectForKey:safePathString];
        if (pids) {
            // This plugin is already staged but not for this process
            [pids addIndex:instance];
            status = errAuthorizationSuccess;
            os_log_debug(AUTHD_LOG, "Plugin %{public}s is already safe", originalPath);
            return;
        }
        
        // This plugin is not yet staged
        pids = [NSMutableIndexSet new];
        [pids addIndex:instance];
        NSError *error = nil;
        
        os_log_debug(AUTHD_LOG, "Copying plugin from %{public}@ to %{public}@", originalPathString, safePathString);
        
        // Create staging directory if it doesn't exist
        if (![fileManager createDirectoryAtPath:kPluginStagingDir withIntermediateDirectories:YES attributes:nil error:&error]) {
            if (error.code != NSFileWriteFileExistsError) {
                os_log_error(AUTHD_LOG, "Failed to create staging directory: %{public}@", error.description);
                status = errAuthorizationInternal;
                return;
            }
        }

        // Set proper permissions on the staging directory
        if (chmod([kPluginStagingDir UTF8String], S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) { // 755
            os_log_error(AUTHD_LOG, "Failed to set permissions on staging directory");
            status = errAuthorizationInternal;
            return;
        }
        
        if ([fileManager fileExistsAtPath:safePathString]) {
            if (!_remove_dir_content(safePathString, YES, YES)) {
                os_log_error(AUTHD_LOG, "Failed to remove existing staged plugin: %{public}@", safePathString);
                NSString *uuid = [[NSUUID UUID] UUIDString];
                NSString *backupPath = [safePathString stringByAppendingFormat:@"-%@", uuid];
                os_log(AUTHD_LOG, "Moving existing staged plugin from %{public}@ to %{public}@", safePathString, backupPath);
                if (![fileManager moveItemAtPath:safePathString toPath:backupPath error:&error]) {
                    os_log_error(AUTHD_LOG, "Failed to move existing staged plugin: %{public}@", error.description);
                    // do not bail out, copy phase does possibly that
                }
            }
        }

        if ([fileManager copyItemAtPath:originalPathString toPath:safePathString error:&error]) {
            os_log_debug(AUTHD_LOG, "Plugin %{public}@ copied to the secure location %{public}@", pluginName, safePathString);
        } else {
            os_log_error(AUTHD_LOG, "Failed to copy plugin to secure location: %{public}@", error.description);
            // do not fail and give it a try. SecurityAgent/authorizationhost
            // cannot do anything else either
        }
        
        // A TOCTOU race condition is not possible here because the destination directory is
        // protected by SIP, preventing unentitled processes from interfering with file operations.
        // The API is also protected by entitlements, ensuring only trusted callers can invoke it.
        gStagedPluginList[safePathString] = pids;
        status = errAuthorizationSuccess;
    });
    
    if (safePath && (status == errAuthorizationSuccess)) {
        *safePath = strndup(safePathString.UTF8String, PATH_MAX);
    }

    return status;
}

OSStatus plugin_unstage(pid_t instance) {
    ensure_inited();

    __block OSStatus status = errAuthorizationSuccess;
    dispatch_sync(gAccessQueue, ^{
        os_log_info(AUTHD_LOG, "Going to unstage plugins for %d", instance);
        NSArray *allPluginPaths = [gStagedPluginList allKeys];
        NSMutableArray *pluginPathsToRemove = [NSMutableArray array];
        
        for (NSString *pluginSafePath in allPluginPaths) {
            NSMutableIndexSet *pids = gStagedPluginList[pluginSafePath];
            if (![pids containsIndex:instance]) {
                continue; // This instance does not use this plugin
            }
            
            [pids removeIndex:instance];
            if (pids.count == 0) {
                [pluginPathsToRemove addObject:pluginSafePath];
            }
        }
        if (pluginPathsToRemove.count == 0) {
            return;
        }
        
        // Remove from dictionary and filesystem atomically
        for (NSString *pluginPath in pluginPathsToRemove) {
            if (_remove_dir_content(pluginPath, YES, YES)) {
                os_log_info(AUTHD_LOG, "Successfully removed staged plugin: %{public}@", pluginPath);
            } else {
                status = errAuthorizationDenied;
            }
            [gStagedPluginList removeObjectForKey:pluginPath];
        }
    });
    
    return status;
}
