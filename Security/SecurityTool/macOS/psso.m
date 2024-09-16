//
//  psso.m
//  SecurityTool

#import <Foundation/Foundation.h>

#import "psso.h"
#import "fvunlock.h"
#import "security_tool.h"
#import "Security/AuthorizationPriv.h"
#import <Security/AuthorizationTagsPriv.h>
#import <LocalAuthentication/LAContext+Private.h>
#import <APFS/APFS.h>
#import <SoftLinking/SoftLinking.h>
#import <os/log.h>
#import <sys/stat.h>
#import <PlatformSSOCore/POLoginUserCore.h>
#import <StorageKit/SKManager.h>

#define kBuffLen (128)

SOFT_LINK_FRAMEWORK(PrivateFrameworks, PlatformSSOCore)
SOFT_LINK_CLASS(PlatformSSOCore, POLoginUserCore)
SOFT_LINK_FRAMEWORK(PrivateFrameworks, StorageKit)
SOFT_LINK_CLASS(StorageKit, SKManager)

static NSString *getVolumeUuid(NSString *node)
{
    SKDisk *disk = [[getSKManagerClass() syncSharedManager] diskForString:node];
    SKAPFSDisk *apfsDisk = (SKAPFSDisk *)disk;
    
    return apfsDisk.volumeUUID;
}

static NSString *volumeDescription(NSString *node)
{
    SKDisk *disk = [[getSKManagerClass() syncSharedManager] diskForString:node];
    SKAPFSDisk *apfsDisk = (SKAPFSDisk *)disk;
    return [NSString stringWithFormat:@"Volume %@ (name %@, UUUD %@)", node, apfsDisk.volumeName, apfsDisk.volumeName];
}

static Boolean checkUser(char user[kBuffLen], NSArray *users)
{
    if (!user[0]) {
        // user was not entered on a command line
        printf("Enter user name: ");
        if (fgets(user, kBuffLen , stdin) == NULL) {
            // no input
            os_log_error(OS_LOG_DEFAULT, "Unable to acquire username");
            return NO;
        }
        char *pos = strstr(user, "\n");
        if (pos) {
            *pos = 0;
        }
    }
    
    NSString *username = [NSString stringWithUTF8String:user].lowercaseString;
    for (NSDictionary *userRecord in users) {
        if ([username isEqualToString:userRecord[@PLUDB_USERNAME]]) {
            return YES;
        }
    }
    return NO;
}

static Boolean checkVolume(char volume[kBuffLen], char user[kBuffLen], NSArray *users)
{                                                                                                          // first make list of all volume candidates which contain the user
    NSString *str = [NSString stringWithUTF8String:user].lowercaseString;

    NSMutableSet<NSString *> *volumeCandidates = [NSMutableSet new];
    for (NSDictionary *item in users) {
        if ([str isEqualTo:item[@ PLUDB_USERNAME]]) {
            [volumeCandidates addObject:item[@PLUDB_DNODE]];
        }
    }
               
    NSInteger index = 1;
    if (!volume[0]) {
        // volume was not entered on a command line
        if (volumeCandidates.count > 1) {
            printf("Please select volume which contain user %s:\n", user);
            for (NSString *node in volumeCandidates) {
                NSString *volumeDesc = volumeDescription(node);
                printf("%ld. %s\n", (long)index, volumeDesc.UTF8String);
            }
            char indexBuffer[kBuffLen];
        retry:
            printf("Your choice: \n");
            if (fgets(indexBuffer, kBuffLen, stdin) == NULL) {
                // no input
                os_log_error(OS_LOG_DEFAULT, "Unable to acquire volume");
                return NO;
            }
            index = atoi(indexBuffer);
            if (index < 1 || index >= (NSInteger)volumeCandidates.count) {
                printf("Wrong line number, please enter 1-%lu.\n", (unsigned long)volumeCandidates.count);
                goto retry;
            }
        }
        strncpy(volume, volumeCandidates.allObjects[index - 1].UTF8String, strlen(volumeCandidates.allObjects[index - 1].UTF8String));
        printf("Volume %s selected.\n", volume);
    }
    
    NSString *volumeStr = [NSString stringWithUTF8String:volume].lowercaseString;
    for (NSDictionary *item in users) {
        if ([str isEqualTo:item[@PLUDB_USERNAME]] && [volumeStr isEqualToString:item[@PLUDB_DNODE]]) {
            return YES;
        }
    }

    return NO;
}

static OSStatus bypassLoginPolicy(int argc, char * const *argv)
{
    // first ensure we are in a Recovery
    if (!isInFVUnlock()) {
        fprintf(stderr, "This command is available only when booted to Recovery\n");
        return -1;
    }
    
    POLoginUserCore *poLoginCore = [[getPOLoginUserCoreClass() alloc] init];
    if (!poLoginCore) {
        fprintf(stderr, "No PlatformSSO support on this machine for this command\n");
        return -2;
    }

    /*
     *    security psso skip-idp -u username -v volume -k keytype
     *    "    -u  Specify username"
     *    "    -v  Specify volume"
     */
    char user[kBuffLen];
    char volume[kBuffLen];
    
    memset(user, 0, sizeof(user));
    memset(volume, 0, sizeof(volume));

    int ch;
    
    while ((ch = getopt(argc, argv, "u:v:")) != -1)
    {
        switch (ch)
        {
            case 'u':
                strncpy(user, optarg, sizeof(user));
                break;
            case 'v':
                strncpy(volume, optarg, sizeof(volume));
                break;
            case '?':
            default:
                return SHOW_USAGE_MESSAGE;
        }
    }
    
    // load Air for all volumes
    CFArrayRef cfUsers;
    OSStatus status = AuthorizationCopyPreloginUserDatabase(NULL, 0, &cfUsers);
    NSArray *users = CFBridgingRelease(cfUsers);
    if (status) {
        fprintf(stderr, "Volume users error %d\n", (int)status);
        return -3;
    }
    if (users.count == 0) {
        fprintf(stderr, "No avaiable users\n");
        return -4;
    }

    if (!checkUser(user, users)) {
        if (user[0]) {
            fprintf(stderr, "User %s is not available\n", user);
        } else {
            fprintf(stderr, "User was not specified.\n");
        }
        return -5;
    }
    
    if (!checkVolume(volume, user, users)) {
        if (volume[0]) {
            fprintf(stderr, "Volume %s is not available\n", volume);
        } else {
            fprintf(stderr, "Volume was not specified.\n");
        }
        return -6;
    }
    
    // ask for the Recovery Key value
    char *temp = getpass("Recovery Key: ");
    NSString *userStr = [NSString stringWithUTF8String:user];
    NSString *volumeStr = [NSString stringWithUTF8String:volume];
    NSString *keyStr = [NSString stringWithUTF8String:APFS_FV_PERSONAL_RECOVERY_KEY_UUID];
    NSString *password = [NSString stringWithUTF8String:temp];
    NSString *volumeUuid = getVolumeUuid(volumeStr);
    os_log_debug(OS_LOG_DEFAULT, "Volume UUID for %{public}@ is %{public}@", volumeStr, volumeUuid);

    if (userStr.length == 0 || password.length == 0) {
        // no credentials
        fprintf(stderr, "Unable to get the credentials\n");
        return -8;
    }
    
    NSDictionary *cred = @{
        @kLACredentialKeyUserGuid : keyStr,
        @kLACredentialKeyPassword : password,
        @kLACredentialKeyVolumeUuid : volumeUuid
    };
    NSError *error;
    NSData *credData = [NSKeyedArchiver archivedDataWithRootObject:cred requiringSecureCoding:YES error:&error];
    if (credData == nil) {
        fprintf(stderr, "Archiver failed: %s\n", error.description.UTF8String);
        return -9;
    }
    // now it is time to call PSSO SPI
    BOOL success = [poLoginCore bypassLoginPolicyForUserName:userStr volume:volumeStr contextData:credData];
    if (success) {
        printf("Success\n");
    } else {
        fprintf(stderr, "Failure\n");
    }
    
    return 0;
}

int psso(int argc, char * const *argv) {
    int result = SHOW_USAGE_MESSAGE;
    require_quiet(argc > 1, out); // three arguments needed
    @autoreleasepool {
        if (!strcmp("bypass-login-policy", argv[1])) {
            result = bypassLoginPolicy(argc - 1, argv + 1);
        }
    }

out:
    return result;
}
