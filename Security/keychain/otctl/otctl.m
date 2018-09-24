//
//  Security
//

#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <Security/Security.h>
#import <Security/SecItemPriv.h>
#import <xpc/xpc.h>
#import <err.h>
#import "OT.h"
#import <utilities/debugging.h>
#import "keychain/ot/OTControl.h"
#import "keychain/ot/OTConstants.h"
#include "lib/SecArgParse.h"
#include <utilities/SecCFWrappers.h>
#include <utilities/SecInternalReleasePriv.h>

@interface OTControlCLI : NSObject
@property OTControl* control;
@end

@implementation OTControlCLI


- (instancetype) initWithOTControl:(OTControl*)control {
    if ((self = [super init])) {
        _control = control;
    }
    
    return self;
}

- (long)preflightBottledPeer:(NSString*)contextID dsid:(NSString*)dsid
{
    __block long ret = 0;
    
#if OCTAGON
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    
    [self.control preflightBottledPeer:contextID dsid:dsid reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
        if(error){
            printf("Error pushing: %s\n", [[error description] UTF8String]);
            ret = (error.code == 0 ? -1 : error.code);
        }else if(entropy && bottleID && signingPublicKey){
            printf("\nSuccessfully preflighted bottle ID: %s\n", [bottleID UTF8String]);
            printf("\nEntropy used: %s\n", [[entropy base64EncodedStringWithOptions:0] UTF8String]);
            printf("\nSigning Public Key: %s\n", [[signingPublicKey base64EncodedStringWithOptions:0] UTF8String]);
            ret = 0;
        }else{
            printf("Failed to preflight bottle and no error was returned..");
            ret = -1;
        }
        
        dispatch_semaphore_signal(sema);
    }];
    
    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 65)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
        return -1;
    }
    return ret;
#else
    return -1;
#endif
}

- (long)launchBottledPeer:(NSString*)contextID bottleID:(NSString*)bottleID
{
    __block long ret = 0;
    
#if OCTAGON
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    
    [self.control launchBottledPeer:contextID bottleID:bottleID reply:^(NSError * _Nullable error) {
        if(error)
        {
            printf("Error pushing: %s\n", [[error description] UTF8String]);
            ret = (error.code == 0 ? -1 : error.code);
        } else {
            printf("\nSuccessfully launched bottleID: %s\n", [bottleID UTF8String]);
            ret = 0;
        }
        
        dispatch_semaphore_signal(sema);
    }];
    
    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 65)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
        return -1;
    }
    return ret;
#else
    return -1;
#endif
}

- (long)scrubBottledPeer:(NSString*)contextID bottleID:(NSString*)bottleID
{
    __block long ret = 0;
    
#if OCTAGON
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    
    [self.control scrubBottledPeer:contextID bottleID:bottleID reply:^(NSError * _Nullable error) {
        if(error)
        {
            printf("Error pushing: %s\n", [[error description] UTF8String]);
            ret = (error.code == 0 ? -1 : error.code);
        } else {
            printf("\nSuccessfully scrubbed bottle ID: %s\n", [bottleID UTF8String]);
            ret = 0;
        }
        
        dispatch_semaphore_signal(sema);
    }];
    
    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 65)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
        return -1;
    }
    return ret;
#else
    ret = -1;
    return ret;
#endif
}


- (long)enroll:(NSString*)contextID dsid:(NSString*)dsid
{
    __block long ret = 0;
    
#if OCTAGON
    dispatch_semaphore_t semaForPreFlight = dispatch_semaphore_create(0);
    dispatch_semaphore_t semaForLaunch = dispatch_semaphore_create(0);
    __block NSString* bottleRecordID = nil;
    __block NSError* localError = nil;

    [self.control preflightBottledPeer:contextID dsid:dsid reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
        if(error)
        {
            localError = error;
            printf("Error pushing: %s\n", [[error description] UTF8String]);
            ret = (error.code == 0 ? -1 : error.code);
        } else {
            bottleRecordID = bottleID;
            printf("\nSuccessfully preflighted bottle ID: %s\n", [bottleID UTF8String]);
            printf("\nEntropy used: %s\n", [[entropy base64EncodedStringWithOptions:0] UTF8String]);
            printf("\nSigning Public Key: %s\n", [[signingPublicKey base64EncodedStringWithOptions:0] UTF8String]);
            ret = 0;
        }
        
        dispatch_semaphore_signal(semaForPreFlight);
    }];
    
    if(dispatch_semaphore_wait(semaForPreFlight, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 65)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
        return -1;
    }

    if(localError == nil){
        [self.control launchBottledPeer:contextID bottleID:bottleRecordID reply:^(NSError * _Nullable error) {
            if(error)
            {
                printf("Error pushing: %s\n", [[error description] UTF8String]);
                ret = (error.code == 0 ? -1 : error.code);
            } else {
                printf("\nSuccessfully launched bottleID: %s\n", [bottleRecordID UTF8String]);
                ret = 0;
            }

            dispatch_semaphore_signal(semaForLaunch);
        }];

        if(dispatch_semaphore_wait(semaForLaunch, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 65)) != 0) {
            printf("\n\nError: timed out waiting for response\n");
            return -1;
        }
    }
    printf("Complete.\n");
    return ret;
#else
    return -1;
#endif
}


- (long)restore:(NSString*)contextID dsid:(NSString*)dsid secret:(NSData*)secret escrowRecordID:(NSString*)escrowRecordID
{
    __block long ret = 0;

#if OCTAGON
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [self.control restore:contextID dsid:dsid secret:secret escrowRecordID:escrowRecordID reply:^(NSData* signingKeyData, NSData* encryptionKeyData, NSError *error) {
        if(error)
        {
            printf("Error pushing: %s\n", [[error description] UTF8String]);
            ret = (error.code == 0 ? -1 : error.code);
        } else {

            printf("Complete.\n");
            ret = 0;
        }

        NSString* signingKeyString = [signingKeyData base64EncodedStringWithOptions:0];
        NSString* encryptionKeyString = [encryptionKeyData base64EncodedStringWithOptions:0];

        printf("Signing Key:\n %s\n", [signingKeyString UTF8String]);
        printf("Encryption Key:\n %s\n", [encryptionKeyString UTF8String]);
        dispatch_semaphore_signal(sema);
    }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 65)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
        return -1;
    }
    return ret;
#else
    ret = -1;
    return ret;
#endif
}

- (long) reset
{
    __block long ret = 0;
    
#if OCTAGON
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    [self.control reset:^(BOOL reset, NSError* error){
        if(error)
        {
            printf("Error pushing: %s\n", [[error description] UTF8String]);
            ret = (error.code == 0 ? -1 : error.code);
        } else {
            printf("success\n");
        }
        
        dispatch_semaphore_signal(sema);
    }];
    
    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 65)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
        return -1;
    }
    
    printf("Complete.\n");
    return ret;
#else
    ret = -1;
    return ret;
#endif
}

- (long) listOfRecords
{
    __block long ret = 0;
    
#if OCTAGON
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    [self.control listOfRecords:^(NSArray* list, NSError* error){
        if(error)
        {
            printf("Error pushing: %s\n", [[error description] UTF8String]);
            ret = (error.code == 0 ? -1 : error.code);
        } else {
            [list enumerateObjectsUsingBlock:^(NSString*  _Nonnull escrowRecordID, NSUInteger idx, BOOL * _Nonnull stop) {
                printf("escrowRecordID: %s\n", [escrowRecordID UTF8String]);
            }];
            ret = 0;
        }
        
        dispatch_semaphore_signal(sema);
    }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 65)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
        return -1;
    }

    printf("Complete.\n");
    return ret;
#else
    ret = -1;
    return ret;
#endif
}

- (long)octagonKeys
{
    __block long ret = 0;

#if OCTAGON
    dispatch_semaphore_t semaForGettingEncryptionKey = dispatch_semaphore_create(0);
    dispatch_semaphore_t semaForGettingSigningKey = dispatch_semaphore_create(0);
    [self.control encryptionKey:^(NSData *encryptionKey, NSError * error) {
        if(error)
        {
            printf("Error pushing: %s\n", [[error description] UTF8String]);
            ret = (error.code == 0 ? -1 : error.code);
        } else {
            NSString* encryptionKeyString = [encryptionKey base64EncodedStringWithOptions:0];
            printf("Encryption Key:\n %s\n", [encryptionKeyString UTF8String]);
            ret = 0;
        }

        dispatch_semaphore_signal(semaForGettingEncryptionKey);
    }];

    [self.control signingKey:^(NSData *signingKey, NSError * error) {
        if(error)
        {
            printf("Error pushing: %s\n", [[error description] UTF8String]);
            ret = (error.code == 0 ? -1 : error.code);
        } else {
            NSString* signingKeyString = [signingKey base64EncodedStringWithOptions:0];
            printf("Signing Key:\n %s\n", [signingKeyString UTF8String]);
            ret = 0;
        }

        dispatch_semaphore_signal(semaForGettingSigningKey);
    }];


    if(dispatch_semaphore_wait(semaForGettingEncryptionKey, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 65)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
        return -1;
    }
    if(dispatch_semaphore_wait(semaForGettingSigningKey, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 65)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
        return -1;
    }
    printf("Complete.\n");
    return ret;
#else
    ret = -1;
    return ret;
#endif
}

@end

static int enroll = false;
static int restore = false;
static int octagonkeys = false;
static int reset = false;

static int prepbp = false;
static int launch = false;
static int scrub = false;

static int listOfRecords = false;
static char* bottleIDArg = NULL;
static char* contextNameArg = NULL;
static char* secretArg = NULL;

int main(int argc, char **argv)
{
    if(!SecIsInternalRelease())
    {
        secnotice("octagon", "Tool not available on non internal builds");
        return -1;
    }

    if(!SecOTIsEnabled())
    {
        printf("To use this tool, enable defaults write for EnableOTRestore\n defaults write (~)/Library/Preferences/com.apple.security EnableOTRestore -bool YES\n");
        return -1;
    }
    static struct argument options[] = {
        { .shortname='s', .longname="secret", .argument=&secretArg, .description="escrow secret"},
        { .shortname='e', .longname="bottleID", .argument=&bottleIDArg, .description="bottle record id"},
        { .shortname='c', .longname="context", .argument=&contextNameArg, .description="context name"},

        { .command="restore", .flag=&restore, .flagval=true, .description="Restore fake bottled peer"},
        { .command="enroll", .flag=&enroll, .flagval=true, .description="Enroll fake bottled peer"},
        { .command="keys", .flag=&octagonkeys, .shortname='k', .flagval=true, .description="Octagon Signing + Encryption Keys"},
        { .command="reset", .flag=&reset, .flagval=true, .description="Reset Octagon Trust Zone"},
        { .command="list", .flag=&listOfRecords, .flagval=true, .description="List of current Bottled Peer Records IDs"},
    
        { .command="prepbp", .flag=&prepbp, .shortname='p', .flagval=true, .description="Preflights a bottled peer"},
        { .command="launch", .flag=&launch, .flagval=true, .description="Launches a bottled peer"},
        { .command="scrub", .flag=&scrub, .flagval=true, .description="Scrub bottled peer"},

        {}
    };
    
    static struct arguments args = {
        .programname="otctl",
        .description="Control and report on Octagon Trust",
        .arguments = options,
    };
    
    if(!options_parse(argc, argv, &args)) {
        printf("\n");
        print_usage(&args);
        return -1;
    }
    
    @autoreleasepool {
        NSError* error = nil;
        
        OTControl* rpc = [OTControl controlObject:&error];
        if(error || !rpc) {
            errx(1, "no OTControl failed: %s", [[error description] UTF8String]);
        }
        
        OTControlCLI* ctl = [[OTControlCLI alloc] initWithOTControl:rpc];

        if(enroll) {
            long ret = 0;
            NSString* context = contextNameArg ? [NSString stringWithCString: contextNameArg encoding: NSUTF8StringEncoding] : OTDefaultContext;
            ret = [ctl enroll:context dsid:@"12345678"];
            return (int)ret;
        }
        if(prepbp){
            long ret = 0;           
            NSString* context = contextNameArg ? [NSString stringWithCString: contextNameArg encoding: NSUTF8StringEncoding] : OTDefaultContext;
           
            //requires secret, context is optional
            ret = [ctl preflightBottledPeer:context dsid:@"12345678"];
            return (int)ret;
        }
        if(launch){
            long ret = 0;

            NSString* bottleID = bottleIDArg ? [NSString stringWithCString: bottleIDArg encoding: NSUTF8StringEncoding] : nil;
            NSString* context = contextNameArg ? [NSString stringWithCString: contextNameArg encoding: NSUTF8StringEncoding] : OTDefaultContext;
            //requires bottleID, context is optional
            if(bottleID && [bottleID length] > 0 && ![bottleID isEqualToString:@"(null)"]){
                ret = [ctl launchBottledPeer:context bottleID:bottleID];
            }
            else{
                print_usage(&args);
                return -1;
            }
            
            return (int)ret;
        }
        if(scrub){
            long ret = 0;

            NSString* bottleID = bottleIDArg ? [NSString stringWithCString: bottleIDArg encoding: NSUTF8StringEncoding] : nil;
            NSString* context = contextNameArg ? [NSString stringWithCString: contextNameArg encoding: NSUTF8StringEncoding] : OTDefaultContext;
            
            //requires bottle ID, context is optional
            if(bottleID && [bottleID length] > 0 && ![bottleID isEqualToString:@"(null)"]){
                ret = [ctl scrubBottledPeer:context bottleID:bottleID];
            }
            else{
                print_usage(&args);
                return -1;
            }
            return (int)ret;
        }
        
        if(restore) {
            long ret = 0;
            NSData* secretData = nil;
            NSString* secretString = secretArg ? [NSString stringWithCString: secretArg encoding: NSUTF8StringEncoding] : nil;
            NSString* bottleID = bottleIDArg ? [NSString stringWithCString: bottleIDArg encoding: NSUTF8StringEncoding] : nil;
            NSString* context = contextNameArg ? [NSString stringWithCString: contextNameArg encoding: NSUTF8StringEncoding] : OTDefaultContext;

            //requires secret and bottle ID, context is optional
            if(secretString && [secretString length] > 0){
                secretData = [[NSData alloc] initWithBase64EncodedString:secretString options:0];;
            }
            else{
                print_usage(&args);
                return -1;
            }
            

            if(bottleID && [bottleID length] > 0 && ![bottleID isEqualToString:@"(null)"]){
                ret = [ctl restore:context dsid:@"12345678" secret:secretData escrowRecordID:bottleID];
            }
            else{
                print_usage(&args);
                return -1;
            }
            return (int)ret;
        }
        if(octagonkeys){
            long ret = 0;
            ret = [ctl octagonKeys];
            return (int)ret;
        }
        if(listOfRecords){
            long ret = 0;
            ret = [ctl listOfRecords];
            return (int)ret;
        }
        if(reset){
            long ret = 0;
            ret = [ctl reset];
            return (int)ret;
        }
        else {
            print_usage(&args);
            return -1;
        }


    }
    return 0;
}

