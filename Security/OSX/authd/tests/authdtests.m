//
//  authdtests.m
//
//

#import <Security/Authorization.h>
#import <Security/AuthorizationDB.h>
#import <Security/AuthorizationTagsPriv.h>
#import <Foundation/Foundation.h>
#import "authd/debugging.h"
#import "authdtestlist.h"

void runRaft(NSString *arguments);
int authd_03_uiauthorization(int argc, char *const *argv);
bool getCredentials(void);

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

bool getCredentials()
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

void runRaft(NSString *arguments)
{
    NSTask *task = [[NSTask alloc] init];
    [task setLaunchPath:@"/usr/local/bin/raft"];
    [task setArguments:@[ @"-b", @"-o", arguments]];
    [task launch];
    [task waitUntilExit];
}

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
	plan_tests(6);
    if (!getCredentials()) {
        fail("Not able to read credentials for current user!");
    }

	AuthorizationRef authorizationRef;

	OSStatus status = AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &authorizationRef);
	ok(status == errAuthorizationSuccess, "AuthorizationRef create");

	AuthorizationItem myItems = {SAMPLE_RIGHT, 0, NULL, 0};
	AuthorizationRights myRights = {1, &myItems};
	AuthorizationRights *authorizedRights = NULL;
	AuthorizationEnvironment environment = {sizeof(validCredentials)/sizeof(AuthorizationItem), validCredentials};
	status = AuthorizationCopyRights(authorizationRef, &myRights, &environment, kAuthorizationFlagDefaults, &authorizedRights);
	ok(status == errAuthorizationDenied, "Standard authorization without kAuthorizationFlagExtendRights");
	AuthorizationFreeItemSetNull(authorizedRights);

	status = AuthorizationCopyRights(authorizationRef, &myRights, kAuthorizationEmptyEnvironment, kAuthorizationFlagExtendRights, &authorizedRights);
	ok(status == errAuthorizationInteractionNotAllowed, "Authorization fail with UI not allowed");
	AuthorizationFreeItemSetNull(authorizedRights);

	status = AuthorizationCopyRights(authorizationRef, &myRights, &environment, kAuthorizationFlagExtendRights, &authorizedRights);
	ok(status == errAuthorizationSuccess, "Standard authorization");
	AuthorizationFreeItemSetNull(authorizedRights);

	AuthorizationItem extendedItems = {SAMPLE_SHARED_RIGHT, 0, NULL, 0};
	AuthorizationRights extendedRights = {1, &extendedItems};

	status = AuthorizationCopyRights(authorizationRef, &extendedRights, &environment, kAuthorizationFlagExtendRights, &authorizedRights);
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
    
    ok(status == errAuthorizationSuccess && passwordFound == false, "Extracting password from AuthorizationRef");
    AuthorizationFreeItemSetNull(authorizedRights);
    
    
    AuthorizationFree(authorizationRef, kAuthorizationFlagDestroyRights);
    return 0;
}

int authd_03_uiauthorization(int argc, char *const *argv)
{
	plan_tests(3);
    if (!getCredentials()) {
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
    runRaft(RAFT_CANCEL);
    if (dispatch_semaphore_wait(sem, SA_TIMEOUT * NSEC_PER_SEC) != 0) {
        fail("Async authorization cancel");
    }
	AuthorizationFree(authorizationRef, kAuthorizationFlagDefaults);

	AuthorizationCopyRightsAsync(authorizationRef, &myRights, kAuthorizationEmptyEnvironment, kAuthorizationFlagExtendRights | kAuthorizationFlagInteractionAllowed, allowBlock);
    sleep(3); // give some time to SecurityAgent to appear
    runRaft(raftFillValid);
    if (dispatch_semaphore_wait(sem, SA_TIMEOUT * NSEC_PER_SEC) != 0) {
        fail("Async authorization");
    }	AuthorizationFree(authorizationRef, kAuthorizationFlagDefaults);

	return 0;
}

int authd_04_executewithprivileges(int argc, char *const *argv)
{
    const int NUMBER_OF_ITERATIONS = 10;
    plan_tests(2 + 4 * NUMBER_OF_ITERATIONS);
    
    if (!getCredentials()) {
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
