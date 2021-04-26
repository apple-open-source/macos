//
//  fvunlock.m
//  SecurityTool

#import <Foundation/Foundation.h>

#import "fvunlock.h"
#import "security_tool.h"
#import "Security/AuthorizationPriv.h"
#import <LocalAuthentication/LAContext+Private.h>
#import <Security/AuthorizationTagsPriv.h>
#import <SoftLinking/SoftLinking.h>
#import <os/log.h>

#if TARGET_OS_OSX && TARGET_CPU_ARM64

SOFT_LINK_FRAMEWORK(Frameworks, LocalAuthentication)
SOFT_LINK_CLASS(LocalAuthentication, LAContext)

NSUUID *currentRecoveryVolumeUUID(void);

static Boolean isInFVUnlock()
{
    return YES;
    // temporary solution until we find a better way
  //  return getenv("__OSINSTALL_ENVIRONMENT") != NULL;
}

#define kEFISystemVolumeUUIDVariableName "SystemVolumeUUID"
NSUUID *currentRecoveryVolumeUUID()
{
    NSData *data;
    NSString * const LANVRAMNamespaceStartupManager = @"5EEB160F-45FB-4CE9-B4E3-610359ABF6F8";
    
    NSString *key = [NSString stringWithFormat:@"%@:%@", LANVRAMNamespaceStartupManager, @kEFISystemVolumeUUIDVariableName];
    
    io_registry_entry_t match = IORegistryEntryFromPath(kIOMasterPortDefault, "IODeviceTree:/options");
    if (match)  {
        CFTypeRef entry = IORegistryEntryCreateCFProperty(match, (__bridge CFStringRef)key, kCFAllocatorDefault, 0);
        IOObjectRelease(match);
        
        if (entry)
        {
            if (CFGetTypeID(entry) == CFDataGetTypeID())
                data = CFBridgingRelease(entry);
            else
                CFRelease(entry);
        }
    }
    
    if (data) {
        return [[NSUUID alloc] initWithUUIDBytes:data.bytes];
    } else {
        return nil;
    }
}

static Boolean verifyUser()
{
    // first check if policy was already satisfied
    __block Boolean verified = NO;
    dispatch_semaphore_t ds = dispatch_semaphore_create(0);
    LAContext *ctx = [[getLAContextClass() alloc] init];
    [ctx evaluatePolicy:LAPolicyUserAuthenticationWithPasscodeRecovery options:@{ @(LAOptionNotInteractive) : @YES } reply:^(NSDictionary *result, NSError *error) {
        if (result) {
            verified = YES;
        }
        dispatch_semaphore_signal(ds);
    }];
    dispatch_semaphore_wait(ds, DISPATCH_TIME_FOREVER);
    if (verified) {
        // user was already authenticated, no need to prompt
        return YES;
    }
    
    // we need to prompt for the credentials
    char buf[50];
    printf("Enter admin's username: ");
    if (fgets(buf, sizeof(buf), stdin) == NULL) {
        // no input
        os_log_error(OS_LOG_DEFAULT, "Unable to acquire username");
        return NO;
    }
    char *temp = getpass("Password: ");
    NSString *username = [[NSString stringWithUTF8String:buf] stringByReplacingOccurrencesOfString:@"\n" withString:@""];
    NSString *password = [NSString stringWithUTF8String:temp];
    
    if (username.length == 0 || password.length == 0) {
        // no credentials
        os_log_error(OS_LOG_DEFAULT, "Unable to get the credentials");
        return NO;
    }
    
    // get the user GUID
    CFArrayRef usersCf;
    OSStatus status = AuthorizationCopyPreloginUserDatabase(NULL, 0, &usersCf);
    if (status) {
        // cannot get AIR
        os_log_error(OS_LOG_DEFAULT, "AIR failed with error %d", status);
        return NO;
    }
    NSArray *users = CFBridgingRelease(usersCf);
    Boolean found = NO;
    for (NSDictionary *record in users) {
        if (![username isEqualToString:record[@PLUDB_USERNAME]]) {
            continue;
        }
        if (![record[@PLUDB_ADMIN] isEqual:@YES]) {
            // admins only
            continue;
        }
        found = YES;
        NSString *userGuid = record[@PLUDB_GUID];
        if (userGuid) {
            ctx = [[getLAContextClass() alloc] init];
            NSDictionary *cred = @{
                @kLACredentialKeyUserGuid : userGuid,
                @kLACredentialKeyPassword : password,
            };
            NSError *error;
            NSData *credData = [NSKeyedArchiver archivedDataWithRootObject:cred requiringSecureCoding:YES error:&error];
            if (credData == nil) {
                os_log_error(OS_LOG_DEFAULT, "NSKeyedArchiver failed with error %{public}@", error);
                continue; // try another user
            }
            
            if ([ctx setCredential:credData type:LACredentialTypeRecoveryData error:&error]) {
                return YES; // user was verified
            }
        }
    }
    
    os_log_error(OS_LOG_DEFAULT, "Unable to verify user (found %d)", found);
    return NO;
}

static int enforcementWorker(const char operation, NSUUID *volumeUuid)
{
    // first ensure we are in a Recovery
    if (!isInFVUnlock()) {
        fprintf(stderr, "This command is available only when booted to Recovery\n");
        return -1;
    }
    
    // then authenticate user
    if (!verifyUser()) {
        fprintf(stderr, "Unable to verify an administrator\n");
        return -3;
    }
    
    // then call authd
    Boolean enabled = false;
    OSStatus retval = AuthorizationHandlePreloginOverride(volumeUuid.UUIDString.UTF8String, operation, &enabled);
    switch (operation) {
        case kAuthorizationOverrideOperationSet:
            if (retval != noErr) {
                fprintf(stderr, "Error %d when trying to set the SmartCard enforcement override\n", retval);
            } else {
                fprintf(stdout, "SmartCard enforcement is temporarily turned off for the next boot\n");
            }
            break;
        case kAuthorizationOverrideOperationReset:
            if (retval != noErr) {
                fprintf(stderr, "Error %d when trying to reset the SmartCard enforcement override\n", retval);
            } else {
                fprintf(stdout, "SmartCard enforcement override was reset\n");
            }
            break;
        case kAuthorizationOverrideOperationQuery:
            if (retval != noErr) {
                fprintf(stderr, "Error %d when trying to get the SmartCard enforcement state\n", retval);
            } else {
                fprintf(stdout, "SmartCard enforcement is%s temporarily turned off for the next boot\n", enabled ? "" : " not");
            }
            break;
        default:
            fprintf(stderr, "Unsupported operation\n");
            break;
    }

    return retval;
}

static int skipScEnforcement(int argc, char * const *argv)
{
    NSUUID *systemVolumeUuid = [[NSUUID UUID] initWithUUIDString:[[NSString alloc] initWithUTF8String:argv[1]]];
    if (!systemVolumeUuid) {
        fprintf(stderr, "System volume UUID %s was not recognized\n", argv[1]);
        return -4;
    }
    // look if system volume exists at all
    CFArrayRef cfUsers;
    OSStatus status = AuthorizationCopyPreloginUserDatabase(argv[1], 0, &cfUsers);
    NSArray *users = CFBridgingRelease(cfUsers);
    if (status) {
        fprintf(stderr, "System volume error\n");
        return -5;
    }
    if (users.count == 0) {
        fprintf(stderr, "System volume with UUID %s is not supported\n", argv[1]);
        return -6;
    }
    
    if (!strcmp("set", argv[2])) {
        return enforcementWorker(kAuthorizationOverrideOperationSet, systemVolumeUuid);
    } else if (!strcmp("reset", argv[2])) {
        return enforcementWorker(kAuthorizationOverrideOperationReset, systemVolumeUuid);
    } else if (!strcmp("status", argv[2])) {
        return enforcementWorker(kAuthorizationOverrideOperationQuery, systemVolumeUuid);
    }
    
    return SHOW_USAGE_MESSAGE;
}

int fvunlock(int argc, char * const *argv) {
    int result = SHOW_USAGE_MESSAGE;
    require_quiet(argc > 3, out); // three arguments needed
    @autoreleasepool {
        if (!strcmp("skip-sc-enforcement", argv[1])) {
            result = skipScEnforcement(argc - 1, argv + 1);
        }
    }

out:
    return result;
}

#endif
