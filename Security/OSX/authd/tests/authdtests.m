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

#define AuthorizationFreeItemSetNull(IS) { AuthorizationItemSet *_is = (IS); \
if (_is) { (IS) = NULL; AuthorizationFreeItemSet(_is); } }

#define SAMPLE_RIGHT "com.apple.security.syntheticinput"
#define SAMPLE_SHARED_RIGHT "system.preferences"

#define CORRECT_UNAME "bats"
#define CORRECT_PWD "bats"
#define INCORRECT_UNAME "fs;lgp-984-25opsdakflasdg"
#define INCORRECT_PWD "654sa65gsqihr6hhsfd'lbo[0q2,m23-odasdf"

#define SA_TIMEOUT (20)

#define RAFT_FILL @"target.processes()[\"SecurityAgent\"].mainWindow().textFields()[\"User Name:\"].click();keyboard.typeString_withModifiersMask_(\"a\", (kUIACommandKeyMask));keyboard.typeVirtualKey_(117);keyboard.typeString_(\"%s\");target.processes()[\"SecurityAgent\"].mainWindow().textFields()[\"Password:\"].click();keyboard.typeString_withModifiersMask_(\"a\", (kUIACommandKeyMask));keyboard.typeVirtualKey_(117);keyboard.typeString_(\"%s\");target.processes()[\"SecurityAgent\"].mainWindow().buttons()[\"OK\"].click();quit();"

#define RAFT_CANCEL @"target.processes()[\"SecurityAgent\"].mainWindow().buttons()[\"Cancel\"].click();quit();"

AuthorizationItem validCredentials[] = {
	{AGENT_USERNAME, strlen(CORRECT_UNAME), (void *)CORRECT_UNAME, 0},
	{AGENT_PASSWORD, strlen(CORRECT_PWD), (void *)CORRECT_PWD,0}
};

AuthorizationItem invalidCredentials[] = {
	{AGENT_USERNAME, strlen(INCORRECT_UNAME), (void *)INCORRECT_UNAME, 0},
	{AGENT_PASSWORD, strlen(INCORRECT_PWD), (void *)INCORRECT_PWD,0}
};

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
	plan_tests(5);

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

	AuthorizationFree(authorizationRef, kAuthorizationFlagDestroyRights);
	return 0;
}

int authd_03_uiauthorization(int argc, char *const *argv)
{
	plan_tests(3);

	AuthorizationRef authorizationRef;

	OSStatus status = AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &authorizationRef);
	ok(status == errAuthorizationSuccess, "AuthorizationRef create");
    
	AuthorizationItem myItems = {SAMPLE_RIGHT, 0, NULL, 0};
	AuthorizationRights myRights = {1, &myItems};

	NSString *raftFillValid = [NSString stringWithFormat:RAFT_FILL, CORRECT_UNAME, CORRECT_PWD];
    
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
