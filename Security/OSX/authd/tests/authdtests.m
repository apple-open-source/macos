//
//  authdtests.m
//
//

#import <Foundation/Foundation.h>
#import <TargetConditionals.h>
#import <CoreFoundation/CFBase.h>
#import <Availability.h>
#import <Security/Authorization.h>
#import <Security/AuthorizationPriv.h>
#import <Security/AuthorizationDB.h>
#import <Security/AuthorizationTagsPriv.h>
#import "authd/debugging.h"
#import <XCTest/XCTest.h>

#define AuthorizationFreeItemSetNull(IS) { AuthorizationItemSet *_is = (IS); \
if (_is) { (IS) = NULL; AuthorizationFreeItemSet(_is); } }

#define SAMPLE_RIGHT "com.apple.security.syntheticinput"
#define SAMPLE_SHARED_RIGHT "system.preferences"
#define SAMPLE_PASSWORD_RIGHT "system.csfde.requestpassword"
#define SCREENSAVER_RIGHT "system.login.screensaver"

NSString *correctUsername;
NSString *correctPassword;

#define INCORRECT_UNAME "fs;lgp-984-25opsdakflasdg"
#define INCORRECT_PWD "654sa65gsqihr6hhsfd'lbo[0q2,m23-odasdf"

#define SA_TIMEOUT (20)

AuthorizationItem validCredentials[] = {
	{AGENT_USERNAME, 0, NULL, 0},
	{AGENT_PASSWORD, 0, NULL, 0}
};

AuthorizationItem invalidCredentials[] = {
	{AGENT_USERNAME, strlen(INCORRECT_UNAME), (void *)INCORRECT_UNAME, 0},
	{AGENT_PASSWORD, strlen(INCORRECT_PWD), (void *)INCORRECT_PWD,0}
};
                          
@interface AuthorizationTests : XCTestCase
@end

@implementation AuthorizationTests

+ (bool)getCredentials {
    static dispatch_once_t onceToken = 0;
    dispatch_once(&onceToken, ^{
        NSDictionary *dict = [[NSDictionary alloc] initWithContentsOfFile:@"/etc/credentials.plist"];
        correctUsername = dict[@"username"];
        correctPassword = dict[@"password"];
        if (correctUsername) {
            validCredentials[0].value = (void *)correctUsername.UTF8String;
            if (validCredentials[0].value) {
                validCredentials[0].valueLength = strlen(correctUsername.UTF8String);
            }
        }
        if (correctPassword) {
            validCredentials[1].value = (void *)correctPassword.UTF8String;
            if (validCredentials[1].value) {
                validCredentials[1].valueLength = strlen(correctPassword.UTF8String);
            }
        }
    });
    return (correctUsername != nil) && (correctPassword != nil);
}

+ (void)setUp {
    [super setUp];
}

+ (void)tearDown {
    [super tearDown];
}

#ifdef PID_TESTS
static NSString *_runAuthorization(NSString *right)
{
    NSTask *task = [[NSTask alloc] init];
    task.launchPath = @"/usr/bin/security";
    task.arguments = @[ @"authorize", @"-u", right];
    NSPipe *outPipe = [NSPipe pipe];
    task.standardOutput = outPipe;
    task.standardError = outPipe;
    [task launch];
    [task waitUntilExit];

    NSFileHandle *read = [outPipe fileHandleForReading];
    NSString *output = [[NSString alloc] initWithData:[read readDataToEndOfFile] encoding:NSUTF8StringEncoding];
    [read closeFile];
    return output;
}
#endif /* PID_TESTS */

#ifdef PID_TESTS
static pid_t _get_pid(NSString *name)
{
    NSTask *task = [[NSTask alloc] init];
    task.launchPath = @"/usr/bin/pgrep";
    task.arguments = @[ @"-x", name ];
    NSPipe *outPipe = [NSPipe pipe];
    task.standardOutput = outPipe;
    task.standardError = nil;
    [task launch];
    [task waitUntilExit];
    NSFileHandle *read = [outPipe fileHandleForReading];
    NSString *output = [[NSString alloc] initWithData:[read readDataToEndOfFile] encoding:NSUTF8StringEncoding];
    [read closeFile];
    if (output.length) {
        return atoi(output.UTF8String);
    }
    return 0;
}
#endif /* PID_TESTS */

- (void)testAuthorizationDatabase {
    CFDictionaryRef outDict = NULL;
    OSStatus status = AuthorizationRightGet(SAMPLE_RIGHT, &outDict);
    XCTAssert(status == errAuthorizationSuccess, "AuthorizationRightGet existing right");
    CFReleaseNull(outDict);
    
    status = AuthorizationRightGet("non-existing-right", &outDict);
    XCTAssert(status == errAuthorizationDenied, "AuthorizationRightGet non-existing right");
}

- (void) testBasicAuthorizations {
    if (![AuthorizationTests getCredentials]) {
        XCTFail("Not able to read credentials for current user!");
    } else {
        AuthorizationRef authorizationRef;
        
        OSStatus status = AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &authorizationRef);
        printf("AuthrizationCreate result: %d\n", status);
        XCTAssert(status == errAuthorizationSuccess, "AuthorizationRef create");
        
        AuthorizationItem myItems = {SAMPLE_RIGHT, 0, NULL, 0};
        AuthorizationRights myRights = {1, &myItems};
        AuthorizationRights *authorizedRights = NULL;
        AuthorizationEnvironment environment = {sizeof(validCredentials)/sizeof(AuthorizationItem), validCredentials};
        status = AuthorizationCopyRights(authorizationRef, &myRights, &environment, kAuthorizationFlagDefaults, &authorizedRights);
        printf("AuthorizationCopyRights without kAuthorizationFlagExtendRights result: %d\n", status);
        XCTAssert(status == errAuthorizationDenied, "Standard authorization without kAuthorizationFlagExtendRights");
        AuthorizationFreeItemSetNull(authorizedRights);
        
        status = AuthorizationCopyRights(authorizationRef, &myRights, kAuthorizationEmptyEnvironment, kAuthorizationFlagExtendRights, &authorizedRights);
        printf("AuthorizationCopyRights without kAuthorizationFlagInteractionAllowed result: %d\n", status);
        XCTAssert(status == errAuthorizationInteractionNotAllowed, "Authorization fail with UI not allowed");
        AuthorizationFreeItemSetNull(authorizedRights);
        
        status = AuthorizationCopyRights(authorizationRef, &myRights, &environment, kAuthorizationFlagExtendRights, &authorizedRights);
        printf("Standard AuthorizationCopyRights result: %d\n", status);
        XCTAssert(status == errAuthorizationSuccess, "Standard authorization");
        AuthorizationFreeItemSetNull(authorizedRights);
        
        AuthorizationItem extendedItems = {SAMPLE_SHARED_RIGHT, 0, NULL, 0};
        AuthorizationRights extendedRights = {1, &extendedItems};
        
        status = AuthorizationCopyRights(authorizationRef, &extendedRights, &environment, kAuthorizationFlagExtendRights, &authorizedRights);
        printf("Extending AuthorizationCopyRights result: %d\n", status);
        XCTAssert(status == errAuthorizationSuccess, "Extending authorization rights");
        AuthorizationFreeItemSetNull(authorizedRights);
        
        AuthorizationItem pwdExtractItems = {SAMPLE_PASSWORD_RIGHT, 0, NULL, 0};
        AuthorizationRights pwdExtractRight = {1, &pwdExtractItems};
        
        // check that non-entitled process cannot extract password from AuthorizationRef
        status = AuthorizationCopyRights(authorizationRef, &pwdExtractRight, &environment, kAuthorizationFlagExtendRights, &authorizedRights);
        Boolean passwordFound = false;
        if (status == errAuthorizationSuccess) {
            AuthorizationItemSet *returnedInfo;
            status = AuthorizationCopyInfo(authorizationRef, NULL, &returnedInfo);
            if (status == errSecSuccess && returnedInfo) {
                for (uint32_t index = 0; index < returnedInfo->count; ++index) {
                    AuthorizationItem item = returnedInfo->items[index];
                    if (strncpy((char *)item.name, kAuthorizationEnvironmentPassword, strlen(kAuthorizationEnvironmentPassword)) == 0) {
                        passwordFound = true;
                    }
                }
                AuthorizationFreeItemSetNull(returnedInfo);
            }
        }
        printf("Extracting AuthorizationCopyRights result: %d, password extracted %d\n", status, passwordFound);
        XCTAssert(status == errAuthorizationSuccess && passwordFound == false, "Extracting password from AuthorizationRef");
        AuthorizationFreeItemSetNull(authorizedRights);
        AuthorizationFree(authorizationRef, kAuthorizationFlagDestroyRights);
        
        AuthorizationItem screenSaverItems = {SCREENSAVER_RIGHT, 0, NULL, 0};
        AuthorizationRights screenSaverRight = {1, &screenSaverItems};
        status = AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &authorizationRef);
        XCTAssert(status == errAuthorizationSuccess, "AuthorizationRef create");
        AuthorizationEnvironment invalidPasswordEnvironment = {sizeof(invalidCredentials)/sizeof(AuthorizationItem), invalidCredentials};
        NSDate *beforeAuth = [NSDate date];
        status = AuthorizationCopyRights(authorizationRef, &screenSaverRight, &invalidPasswordEnvironment, kAuthorizationFlagExtendRights, &authorizedRights);
        NSTimeInterval delay = [[NSDate date] timeIntervalSinceDate:beforeAuth];
        printf("Failed auth time: %f, status %d\n", delay, (int)status);
        XCTAssert(status == errAuthorizationDenied, "Rejected screensaver right with errAuthorizationDenied");
        XCTAssert(delay >= 2.0, "OpenDirectory 2s delay detected");
        XCTAssert(delay < 4.0, "OpenDirectory delay applied only once");
        AuthorizationFree(authorizationRef, kAuthorizationFlagDestroyRights);
    }
    
    // parallel authorizations test
    {
        AuthorizationRef localAuthRef;
        AuthorizationItem myItems = {SAMPLE_RIGHT, 0, NULL, 0};
        AuthorizationRights myRights = {1, &myItems};
        
        OSStatus status = AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &localAuthRef);
        printf("localAuthRef result: %d\n", status);
        XCTAssert(status == errAuthorizationSuccess, "localAuthRef create");
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
            AuthorizationCopyRights(localAuthRef, &myRights, kAuthorizationEmptyEnvironment, kAuthorizationFlagExtendRights | kAuthorizationFlagInteractionAllowed, NULL);
        });
        
        sleep(0.5); // to give authorization in the background queue time to stat
        AuthorizationRef localAuthRef2;
        status = AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &localAuthRef2);
        printf("localAuthRef2 result: %d\n", status);
        XCTAssert(status == errAuthorizationSuccess, "localAuthRef2 create");
        // this should not hang
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);
        __block OSStatus status2 = errAuthorizationInternal;
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
            status2 = AuthorizationCopyRights(localAuthRef2, &myRights, kAuthorizationEmptyEnvironment, kAuthorizationFlagExtendRights, NULL);
            dispatch_semaphore_signal(sem);
        });
        if (dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 3)) != 0) {
            // AuthorizationCopyRights which should finish instantly did not return in 3s
            XCTFail("Failed to run parallel AuthorizationCopyRights calls");
            printf("Failed to run parallel AuthorizationCopyRights calls\n");
        } else {
            printf("Parallel AuthorizationCopyRights result: %d\n", status2);
            XCTAssert(status2 == errAuthorizationInteractionNotAllowed, "Succeeded to run parallel AuthorizationCopyRights");
        }
        AuthorizationDismiss();
        AuthorizationFree(localAuthRef, kAuthorizationFlagDefaults);
        AuthorizationFree(localAuthRef2, kAuthorizationFlagDefaults);
    }
    
#ifdef PID_TESTS
    // run the UI authorization using security tool
    // kill the caller
    // verify that authd did not crash
    __block NSString *authorization_result;
    NSString *kAuthd_name = @"authd";
    NSString *kSecurity_internal_error = @"NO (-60008) \n";
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);

    pid_t authd_pid = _get_pid(kAuthd_name);
    if (authd_pid == 0) {
        fail("Authd is not running");
    }
    
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
        authorization_result = _runAuthorization(@SAMPLE_PASSWORD_RIGHT);
        dispatch_semaphore_signal(sem);
    });
    
    sleep(2); // give security some time to run
    pid_t security_pid = _get_pid(@"security");
    if (!security_pid) {
        fail("Unable to run security tool");
    }

    kill(security_pid, SIGKILL);
    if (dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * SA_TIMEOUT)) != 0) {
        fail("Security tool failed to finish");
    }
    pid_t new_authd_pid = _get_pid(kAuthd_name);
    ok(new_authd_pid == authd_pid, "Authd survives death of a caller proces");

    // Now check if authd does not crash and returns -60008 on SecurityAgent crash
    authorization_result = nil;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
        authorization_result = _runAuthorization(@SAMPLE_PASSWORD_RIGHT);
        dispatch_semaphore_signal(sem);
    });
    sleep(2); // give security some time to run
    pid_t security_agent_pid = _get_pid(@"SecurityAgent");

    kill(security_agent_pid, SIGKILL);
    if (dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * SA_TIMEOUT)) != 0) {
        fail("Security tool failed to finish");
    }
    new_authd_pid = _get_pid(kAuthd_name);
    ok(new_authd_pid == authd_pid, "Authd survives death of a SecurityAgent");
    ok([kSecurity_internal_error isEqualToString:authorization_result] , "Authd returns secInternalError on SecurityAgent death");
#endif /* PID_TESTS */
}

- (void) testExecuteWithPrivileges {
    const int NUMBER_OF_ITERATIONS = 10;
    
    if (![AuthorizationTests getCredentials]) {
        XCTFail("Not able to read credentials for current user!");
        return;
    }
    
    AuthorizationRef authorizationRef;
    OSStatus status = AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &authorizationRef);
    XCTAssert(status == errAuthorizationSuccess, "AuthorizationRef create");
    
    AuthorizationItem myItems = { kAuthorizationRightExecute, 0, NULL, 0};
    AuthorizationRights myRights = {1, &myItems};
    AuthorizationRights *authorizedRights = NULL;
    AuthorizationEnvironment environment = {sizeof(validCredentials)/sizeof(AuthorizationItem), validCredentials};
    status = AuthorizationCopyRights(authorizationRef, &myRights, &environment, kAuthorizationFlagExtendRights, &authorizedRights);
    XCTAssert(status == errAuthorizationSuccess, "Standard authorization");
    AuthorizationFreeItemSetNull(authorizedRights);

    for (int i = 0; i < NUMBER_OF_ITERATIONS; ++i) {
        NSString *guid = [[NSProcessInfo processInfo] globallyUniqueString];
        static const char *toolArgv[3];
        NSString *arg = [NSString stringWithFormat:@"%s %@", "/usr/bin/whoami && /bin/echo", guid];
        NSString *expected = [NSString stringWithFormat:@"root\n%@", guid];
        toolArgv[0] = "-c";
        toolArgv[1] = arg.UTF8String;
        toolArgv[2] = NULL;
        FILE *output = NULL;
        
        status = AuthorizationExecuteWithPrivileges(authorizationRef, "/bin/zsh", 0, (char *const *)toolArgv, &output);
        XCTAssert(status == errAuthorizationSuccess, "AuthorizationExecuteWithPrivileges call succeess");
        
        if (status != 0) {
            break;
        }
        
        char buffer[1024];
        size_t bytesRead = 0;
        size_t totalBytesRead = 0;
        size_t buffSize = sizeof(buffer);
        memset(buffer, 0, buffSize);
        while ((bytesRead = fread (buffer, 1, buffSize, output)) > 0) {
            totalBytesRead += bytesRead; // overwriting buffer is OK since we are reading just a small amount of data
        }
         
        XCTAssert(feof(output), "Authorized tool pipe closed with feof");

        fclose(output);
        if (strncmp(buffer, expected.UTF8String, guid.length) == 0) {
            printf("Authorized tool output matches\n");
        } else {
            XCTFail("AuthorizationExecuteWithPrivileges output %s does not match %s", buffer, expected.UTF8String);
        }
    }
    
    AuthorizationFree(authorizationRef, kAuthorizationFlagDestroyRights);
}

- (void)testRightProperties {
    NSDictionary *properties;
    CFDictionaryRef cfProperties;
    NSString *group;
    NSNumber *passwordOnly;
    
    OSStatus status = AuthorizationCopyRightProperties(SAMPLE_PASSWORD_RIGHT, &cfProperties);
    properties = CFBridgingRelease(cfProperties);
    if (status != errAuthorizationSuccess) {
        XCTFail("AuthorizationCopyRightProperties failed with %d", (int)status);
        return;
    }
    
    printf("AuthorizationCopyRightProperties call succeess\n");
    passwordOnly = properties[@(kAuthorizationRuleParameterPasswordOnly)];
    XCTAssert(passwordOnly.boolValue, "Returned system.csfde.requestpassword as password only right");
    group = properties[@(kAuthorizationRuleParameterGroup)];
    XCTAssert([group isEqualToString:@"admin"], "Returned admin as a required group for system.csfde.requestpassword");

    status = AuthorizationCopyRightProperties("com.apple.Safari.allow-unsigned-app-extensions", &cfProperties);
    properties = CFBridgingRelease(cfProperties);
    if (status != errAuthorizationSuccess) {
        XCTFail("AuthorizationCopyRightProperties failed with %d", (int)status);
        return;
    }
    group = properties[@(kAuthorizationRuleParameterGroup)];
    passwordOnly = properties[@(kAuthorizationRuleParameterPasswordOnly)];
    XCTAssert(group.length == 0 && passwordOnly.boolValue == NO, "Returned safari right as non-password only, no specific group");
    
    status = AuthorizationCopyRightProperties("non-existing-right", &cfProperties);
    XCTAssert(status == errAuthorizationSuccess, "Returned success for default for unknown right: %d", (int)status);
}

- (void)testAuthorizationCreateWithAuditToken {
    AuthorizationRef authRef = NULL;
    audit_token_t emptyToken = { { 0 } };
    
    OSStatus stat = AuthorizationCreateWithAuditToken(emptyToken, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, &authRef);
    if (geteuid() == 0) {
        XCTAssert(stat == errAuthorizationSuccess, "AuthorizationCreateWithAuditToken authRef for root process");
    } else {
        XCTAssert(stat != errAuthorizationSuccess, "AuthorizationCreateWithAuditToken should fail for non-root process");
    }
}

@end
