//
//  authdtests.m
//
//

#import <Security/Authorization.h>
#import <Security/AuthorizationPriv.h>
#import <Security/AuthorizationDB.h>
#import <Security/AuthorizationTagsPriv.h>
#import <Foundation/Foundation.h>
#import "authd/debugging.h"
#import "authdtestlist.h"

#define AuthorizationFreeItemSetNull(IS) { AuthorizationItemSet *_is = (IS); \
if (_is) { (IS) = NULL; AuthorizationFreeItemSet(_is); } }

#define SAMPLE_RIGHT "com.apple.security.syntheticinput"
#define SAMPLE_SHARED_RIGHT "system.preferences"
#define SAMPLE_PASSWORD_RIGHT "system.csfde.requestpassword"

NSString *correctUsername;
NSString *correctPassword;

#define INCORRECT_UNAME "fs;lgp-984-25opsdakflasdg"
#define INCORRECT_PWD "654sa65gsqihr6hhsfd'lbo[0q2,m23-odasdf"

#define SA_TIMEOUT (20)

#define RAFT_FILL @"target.processes()[\"SecurityAgent\"].mainWindow().textFields()[\"User Name:\"].click();keyboard.typeString_withModifiersMask_(\"a\", (kUIACommandKeyMask));keyboard.typeVirtualKey_(117);keyboard.typeString_(\"%s\");target.processes()[\"SecurityAgent\"].mainWindow().textFields()[\"Password:\"].click();keyboard.typeString_withModifiersMask_(\"a\", (kUIACommandKeyMask));keyboard.typeVirtualKey_(117);keyboard.typeString_(\"%s\");target.processes()[\"SecurityAgent\"].mainWindow().buttons()[\"OK\"].click();quit();"

#define RAFT_CANCEL @"target.processes()[\"SecurityAgent\"].mainWindow().buttons()[\"Cancel\"].click();quit();"

AuthorizationItem validCredentials[] = {
	{AGENT_USERNAME, 0, NULL, 0},
	{AGENT_PASSWORD, 0, NULL, 0}
};

AuthorizationItem invalidCredentials[] = {
	{AGENT_USERNAME, strlen(INCORRECT_UNAME), (void *)INCORRECT_UNAME, 0},
	{AGENT_PASSWORD, strlen(INCORRECT_PWD), (void *)INCORRECT_PWD,0}
};

static bool _getCredentials(void)
{
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

static void _runRaft(NSString *arguments)
{
    NSTask *task = [[NSTask alloc] init];
    task.launchPath = @"/usr/local/bin/raft";
    task.arguments = @[ @"-b", @"-o", arguments];
    [task launch];
    [task waitUntilExit];
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

int authd_01_authorizationdb(int argc, char *const *argv)
{
	plan_tests(2);

	CFDictionaryRef outDict = NULL;
	OSStatus status = AuthorizationRightGet(SAMPLE_RIGHT, &outDict);
	ok(status == errAuthorizationSuccess, "AuthorizationRightGet existing right");
	CFReleaseNull(outDict);

	status = AuthorizationRightGet("non-existing-right", &outDict);
	ok(status == errAuthorizationDenied, "AuthorizationRightGet non-existing right");

	return 0;
}

int authd_02_basicauthorization(int argc, char *const *argv)
{
	plan_tests(9);
    if (!_getCredentials()) {
        fail("Not able to read credentials for current user!");
    } else {
        AuthorizationRef authorizationRef;
        
        OSStatus status = AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &authorizationRef);
        printf("AuthrizationCreate result: %d\n", status);
        ok(status == errAuthorizationSuccess, "AuthorizationRef create");
        
        AuthorizationItem myItems = {SAMPLE_RIGHT, 0, NULL, 0};
        AuthorizationRights myRights = {1, &myItems};
        AuthorizationRights *authorizedRights = NULL;
        AuthorizationEnvironment environment = {sizeof(validCredentials)/sizeof(AuthorizationItem), validCredentials};
        status = AuthorizationCopyRights(authorizationRef, &myRights, &environment, kAuthorizationFlagDefaults, &authorizedRights);
        printf("AuthorizationCopyRights without kAuthorizationFlagExtendRights result: %d\n", status);
        ok(status == errAuthorizationDenied, "Standard authorization without kAuthorizationFlagExtendRights");
        AuthorizationFreeItemSetNull(authorizedRights);
        
        status = AuthorizationCopyRights(authorizationRef, &myRights, kAuthorizationEmptyEnvironment, kAuthorizationFlagExtendRights, &authorizedRights);
        printf("AuthorizationCopyRights without kAuthorizationFlagInteractionAllowed result: %d\n", status);
        ok(status == errAuthorizationInteractionNotAllowed, "Authorization fail with UI not allowed");
        AuthorizationFreeItemSetNull(authorizedRights);
        
        status = AuthorizationCopyRights(authorizationRef, &myRights, &environment, kAuthorizationFlagExtendRights, &authorizedRights);
        printf("Standard AuthorizationCopyRights result: %d\n", status);
        ok(status == errAuthorizationSuccess, "Standard authorization");
        AuthorizationFreeItemSetNull(authorizedRights);
        
        AuthorizationItem extendedItems = {SAMPLE_SHARED_RIGHT, 0, NULL, 0};
        AuthorizationRights extendedRights = {1, &extendedItems};
        
        status = AuthorizationCopyRights(authorizationRef, &extendedRights, &environment, kAuthorizationFlagExtendRights, &authorizedRights);
        printf("Extending AuthorizationCopyRights result: %d\n", status);
        ok(status == errAuthorizationSuccess, "Extending authorization rights");
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
        ok(status == errAuthorizationSuccess && passwordFound == false, "Extracting password from AuthorizationRef");
        AuthorizationFreeItemSetNull(authorizedRights);
        AuthorizationFree(authorizationRef, kAuthorizationFlagDestroyRights);
    }
    
    // parallel authorizations test
    {
        AuthorizationRef localAuthRef;
        AuthorizationItem myItems = {SAMPLE_RIGHT, 0, NULL, 0};
        AuthorizationRights myRights = {1, &myItems};
        
        OSStatus status = AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &localAuthRef);
        printf("localAuthRef result: %d\n", status);
        ok(status == errAuthorizationSuccess, "localAuthRef create");
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
            AuthorizationCopyRights(localAuthRef, &myRights, kAuthorizationEmptyEnvironment, kAuthorizationFlagExtendRights | kAuthorizationFlagInteractionAllowed, NULL);
        });
        
        sleep(0.5); // to give authorization in the background queue time to stat
        AuthorizationRef localAuthRef2;
        status = AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &localAuthRef2);
        printf("localAuthRef2 result: %d\n", status);
        ok(status == errAuthorizationSuccess, "localAuthRef2 create");
        // this should not hang
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);
        __block OSStatus status2 = errAuthorizationInternal;
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
            status2 = AuthorizationCopyRights(localAuthRef2, &myRights, kAuthorizationEmptyEnvironment, kAuthorizationFlagExtendRights, NULL);
            dispatch_semaphore_signal(sem);
        });
        if (dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 3)) != 0) {
            // AuthorizationCopyRights which should finish instantly did not return in 3s
            fail("Failed to run parallel AuthorizationCopyRights calls");
            printf("Failed to run parallel AuthorizationCopyRights calls\n");
        } else {
            printf("Parallel AuthorizationCopyRights result: %d\n", status2);
            ok(status2 == errAuthorizationInteractionNotAllowed, "Succeeded to run parallel AuthorizationCopyRights");
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
    return 0;
}

int authd_03_uiauthorization(int argc, char *const *argv)
{
	plan_tests(4);
    if (!_getCredentials()) {
        fail("Not able to read credentials for current user!");
    }

	AuthorizationRef authorizationRef;

	OSStatus status = AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &authorizationRef);
	ok(status == errAuthorizationSuccess, "AuthorizationRef create");
    
	AuthorizationItem myItems = {SAMPLE_RIGHT, 0, NULL, 0};
	AuthorizationRights myRights = {1, &myItems};

	NSString *raftFillValid = [NSString stringWithFormat:RAFT_FILL, correctUsername.UTF8String, correctPassword.UTF8String];
    
	dispatch_semaphore_t sem = dispatch_semaphore_create(0);
	/*
	AuthorizationAsyncCallback internalBlock = ^(OSStatus err, AuthorizationRights *blockAuthorizedRights) {
		AuthorizationFreeItemSetNull(blockAuthorizedRights);
		ok(err == errAuthorizationInternal, "Async authorization interal error");
		dispatch_semaphore_signal(sem);
	};
	AuthorizationAsyncCallback denyBlock = ^(OSStatus err, AuthorizationRights *blockAuthorizedRights) {
		AuthorizationFreeItemSetNull(blockAuthorizedRights);
		ok(err == errAuthorizationDenied, "Async authorization denial");
		dispatch_semaphore_signal(sem);
	};*/
	AuthorizationAsyncCallback allowBlock = ^(OSStatus err, AuthorizationRights *blockAuthorizedRights) {
		AuthorizationFreeItemSetNull(blockAuthorizedRights);
		ok(err == errAuthorizationSuccess, "Async authorization");
		dispatch_semaphore_signal(sem);
	};
	AuthorizationAsyncCallback cancelBlock = ^(OSStatus err, AuthorizationRights *blockAuthorizedRights) {
		AuthorizationFreeItemSetNull(blockAuthorizedRights);
		ok(err == errAuthorizationCanceled, "Async authorization cancel");
		dispatch_semaphore_signal(sem);
	};
	AuthorizationCopyRightsAsync(authorizationRef, &myRights, kAuthorizationEmptyEnvironment, kAuthorizationFlagExtendRights | kAuthorizationFlagInteractionAllowed, cancelBlock);
	sleep(3); // give some time to SecurityAgent to appear
    _runRaft(RAFT_CANCEL);
    if (dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * SA_TIMEOUT)) != 0) {
        fail("Async authorization cancel");
    }
	AuthorizationFree(authorizationRef, kAuthorizationFlagDefaults);

    status = AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &authorizationRef);
    ok(status == errAuthorizationSuccess, "AuthorizationRef create");
    
	AuthorizationCopyRightsAsync(authorizationRef, &myRights, kAuthorizationEmptyEnvironment, kAuthorizationFlagExtendRights | kAuthorizationFlagInteractionAllowed, allowBlock);
    sleep(3); // give some time to SecurityAgent to appear
    _runRaft(raftFillValid);
    if (dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * SA_TIMEOUT)) != 0) {
        fail("Async authorization");
    }
    AuthorizationFree(authorizationRef, kAuthorizationFlagDefaults);

	return 0;
}

int authd_04_executewithprivileges(int argc, char *const *argv)
{
    const int NUMBER_OF_ITERATIONS = 10;
    plan_tests(2 + 4 * NUMBER_OF_ITERATIONS);
    
    if (!_getCredentials()) {
        fail("Not able to read credentials for current user!");
    }
    
    AuthorizationRef authorizationRef;
    OSStatus status = AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &authorizationRef);
    ok(status == errAuthorizationSuccess, "AuthorizationRef create");
    
    AuthorizationItem myItems = { kAuthorizationRightExecute, 0, NULL, 0};
    AuthorizationRights myRights = {1, &myItems};
    AuthorizationRights *authorizedRights = NULL;
    AuthorizationEnvironment environment = {sizeof(validCredentials)/sizeof(AuthorizationItem), validCredentials};
    status = AuthorizationCopyRights(authorizationRef, &myRights, &environment, kAuthorizationFlagExtendRights, &authorizedRights);
    ok(status == errAuthorizationSuccess, "Standard authorization");
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
        ok(status == errAuthorizationSuccess, "AuthorizationExecuteWithPrivileges call succeess");
        
        if (status != 0) {
            break;
        }
        
        char buffer[1024];
        size_t bytesRead = 0;
        size_t totalBytesRead = 0;
        size_t buffSize = sizeof(buffer);
        memset(buffer, 0, buffSize);
        while ((bytesRead = fread (buffer, 1, buffSize, output) > 0)) {
            totalBytesRead += bytesRead; // overwriting buffer is OK since we are reading just a small amount of data
        }
        
        ok(ferror(output) == 0, "Authorized tool pipe closed did not end with ferror");
        if (ferror(output)) {
            // test failed, ferror happened
            fclose(output);
            return 0;
        }
        
        ok(feof(output), "Authorized tool pipe closed with feof");
        if (!feof(output)) {
            // test failed, feof not happened
            fclose(output);
            return 0;
        }
        
        fclose(output);
        if (strncmp(buffer, expected.UTF8String, guid.length) == 0) {
            pass("Authorized tool output matches");
        } else {
            fail("AuthorizationExecuteWithPrivileges output %s does not match %s", buffer, expected.UTF8String);
        }
    }
    
    AuthorizationFree(authorizationRef, kAuthorizationFlagDestroyRights);
    return 0;
}

int authd_05_rightproperties(int argc, char *const *argv)
{
    plan_tests(5);

    NSDictionary *properties;
    CFDictionaryRef cfProperties;
    NSString *group;
    NSNumber *passwordOnly;
    
    OSStatus status = AuthorizationCopyRightProperties(SAMPLE_PASSWORD_RIGHT, &cfProperties);
    properties = CFBridgingRelease(cfProperties);
    if (status != errAuthorizationSuccess) {
        fail("AuthorizationCopyRightProperties failed with %d", (int)status);
        goto done;
    }
    
    pass("AuthorizationCopyRightProperties call succeess");
    passwordOnly = properties[@(kAuthorizationRuleParameterPasswordOnly)];
    ok(passwordOnly.boolValue, "Returned system.csfde.requestpassword as password only right");
    group = properties[@(kAuthorizationRuleParameterGroup)];
    ok([group isEqualToString:@"admin"], "Returned admin as a required group for system.csfde.requestpassword");

    status = AuthorizationCopyRightProperties("com.apple.Safari.allow-unsigned-app-extensions", &cfProperties);
    properties = CFBridgingRelease(cfProperties);
    if (status != errAuthorizationSuccess) {
        fail("AuthorizationCopyRightProperties failed with %d", (int)status);
        goto done;
    }
    group = properties[@(kAuthorizationRuleParameterGroup)];
    passwordOnly = properties[@(kAuthorizationRuleParameterPasswordOnly)];
    ok(group.length == 0 && passwordOnly.boolValue == NO, "Returned safari right as non-password only, no specific group");
    
    status = AuthorizationCopyRightProperties("non-existing-right", &cfProperties);
    ok(status == errAuthorizationSuccess, "Returned success for default for unknown right: %d", (int)status);

done:
    return 0;
}
