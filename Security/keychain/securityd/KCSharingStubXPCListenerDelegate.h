#pragma once

/*
 This header and its implementation are here because keychain/Sharing is excluded entirely for !KCSHARING.
 We want this so that we can install the mach service unconditionally and have a dummy service check-in at runtime
 if the feature itself is absent, which in turn is helpful for development.
 */

// server.c undefs KCSHARING on darwinOS / system securityd, we want to match those conditions here.
#if !KCSHARING || (defined(TARGET_DARWINOS) && TARGET_DARWINOS) || (defined(SECURITYD_SYSTEM) && SECURITYD_SYSTEM)

void KCSharingStubXPCServerInitialize(void);

#if __OBJC__

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface KCSharingStubXPCListenerDelegate : NSObject <NSXPCListenerDelegate>

+ (instancetype)sharedInstance;

@end

NS_ASSUME_NONNULL_END

#endif /* __OBJC__ */
#endif /* !KCSHARING || (defined(TARGET_DARWINOS) && TARGET_DARWINOS) || (defined(SECURITYD_SYSTEM) && SECURITYD_SYSTEM) */
