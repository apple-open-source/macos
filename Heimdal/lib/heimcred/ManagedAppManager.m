/*-
* Copyright (c) 2013 Kungliga Tekniska Högskolan
* (Royal Institute of Technology, Stockholm, Sweden).
* All rights reserved.
*
* Portions Copyright (c) 2020 Apple Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
*/

#import "ManagedAppManager.h"

#import <SoftLinking/SoftLinking.h>

#if TARGET_OS_IOS
#import <ManagedConfiguration/ManagedConfiguration.h>
SOFT_LINK_FRAMEWORK(PrivateFrameworks, ManagedConfiguration)
SOFT_LINK_CLASS(ManagedConfiguration, MCProfileConnection)
SOFT_LINK_CONSTANT(ManagedConfiguration, MCManagedAppsChangedNotification, NSString *)
#elif TARGET_OS_OSX
#import <ConfigurationProfiles/ConfigurationProfiles.h>
#import <libproc.h>
SOFT_LINK_FRAMEWORK(PrivateFrameworks, ConfigurationProfiles)
SOFT_LINK_FUNCTION(ConfigurationProfiles, CP_ManagedAppsIsAppManagedAtURL, __CP_ManagedAppsIsAppManagedAtURL, BOOL, (NSURL* appURL, NSString* bundleID), (appURL, bundleID));
#endif

@interface ManagedAppManager ()

@property (nonatomic) NSArray<NSString *> *managedApps;

@end

@implementation ManagedAppManager
@synthesize managedApps;

- (instancetype)init
{
    self = [super init];
    if (self) {
	managedApps = @[];
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (BOOL)isManagedApp:(NSString*)bundleId auditToken:(audit_token_t)auditToken
{
#if TARGET_OS_IOS
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
	[self listenForChanges];
	[self updateManagedApps:nil];
    });
    
    @synchronized (self) {
	return [self.managedApps containsObject:bundleId];
    }
#elif TARGET_OS_OSX
    NSURL *appURL = nil;
    char buffer[PROC_PIDPATHINFO_MAXSIZE] = {};
    int size = 0;
    size = proc_pidpath_audittoken(&auditToken, buffer, sizeof(buffer));
    if (size <= 0) {
	os_log_error(GSSOSLog(), "isManagedApp: proc_pidpath_audittoken failed for %{public}@", @(audit_token_to_pid(auditToken)));
	return false;
    }
    NSString *path = [NSString stringWithCString:buffer encoding:NSUTF8StringEncoding];
    os_log_debug(GSSOSLog(), "isManagedApp: %{public}@: %{public}@", @(audit_token_to_pid(auditToken)), path);
    if (!path) {
	os_log_error(GSSOSLog(), "isManagedApp: path not found for %{public}@", @(audit_token_to_pid(auditToken)));
	return false;
    }

    appURL = CFBridgingRelease(CFURLCreateWithFileSystemPath(kCFAllocatorDefault, (__bridge CFStringRef)path, kCFURLPOSIXPathStyle, FALSE));
    os_log_debug(GSSOSLog(), "isManagedApp: CFURLCreateWithFileSystemPath: %{public}@", appURL);


    BOOL managed = appURL ? __CP_ManagedAppsIsAppManagedAtURL(appURL, bundleId) : NO;
    return managed;
#else
    return false;
#endif
}

#if TARGET_OS_IOS
- (void)updateManagedApps:(NSNotification *)notification
{
    os_log_info(GSSOSLog(), "Updating Managed App list");
    os_log_debug(GSSOSLog(), "Old Managed App list: %{private}@", self.managedApps);
    @synchronized (self) {
	self.managedApps = [[getMCProfileConnectionClass() sharedConnection] managedAppBundleIDs];
    }
    os_log_debug(GSSOSLog(), "New Managed App list: %{private}@", self.managedApps);
}

- (void)listenForChanges
{
    NSString *notificationName = getMCManagedAppsChangedNotification();
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(updateManagedApps:) name:notificationName object:[getMCProfileConnectionClass() sharedConnection]];
}
#endif

@end
