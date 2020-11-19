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

#if TARGET_OS_IOS
#include <ManagedConfiguration/ManagedConfiguration.h>
#import <SoftLinking/SoftLinking.h>
SOFT_LINK_FRAMEWORK(PrivateFrameworks, ManagedConfiguration)
SOFT_LINK_CLASS(ManagedConfiguration, MCProfileConnection)
SOFT_LINK_CONSTANT(ManagedConfiguration, MCManagedAppsChangedNotification, NSString *)

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

- (BOOL)isManagedApp:(NSString*)bundleId
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
