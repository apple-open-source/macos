//
//  authdroottests.m
//
//

#import "authdtestsCommon.h"

@interface AuthorizationRootTests : XCTestCase
@end

@implementation AuthorizationRootTests

- (void)testAuthorizationCreateWithAuditToken {
    AuthorizationRef authRef = NULL;
    audit_token_t emptyToken = { { 0 } };
    
    OSStatus stat = AuthorizationCreateWithAuditToken(emptyToken, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, &authRef);
    XCTAssert(stat == errAuthorizationSuccess, "AuthorizationCreateWithAuditToken authRef for root process failed %d", stat);
}

- (void)testDatabaseProtection {
    CFDictionaryRef outDict = NULL;
    OSStatus status = AuthorizationRightGet(SAMPLE_RIGHT, &outDict);
    XCTAssert(status == errAuthorizationSuccess, "AuthorizationRightGet failed to get existing right %d", status);
    
    AuthorizationRef authRef;
    status = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, &authRef);
    XCTAssert(status == errAuthorizationSuccess, "AuthorizationCreate failed %d", status);
    
    // add a new right
    status = AuthorizationRightSet(authRef, NEW_RIGHT, outDict, NULL, NULL, NULL);
    XCTAssert(status == errAuthorizationSuccess, "AuthorizationRightSet failed to add a new right %d", status);
    
    // modify an existing right
    status = AuthorizationRightSet(authRef, UNPROTECTED_RIGHT, outDict, NULL, NULL, NULL);
    XCTAssert(status == errAuthorizationSuccess, "AuthorizationRightSet failed to update an unprotected right %d", status);
    
    // modify an existing protected right
    status = AuthorizationRightSet(authRef, PROTECTED_RIGHT, outDict, NULL, NULL, NULL);
    XCTAssert(status == errAuthorizationDenied, "AuthorizationRightSet failed to denial update of a protected right");
    
    AuthorizationFree(authRef, kAuthorizationFlagDefaults);
}

- (void) testAuthdLeaks {
    char *cmd = NULL;
    int ret = 0;
    FILE *fpipe;
    char *command = "pgrep ^authd";
    char c = 0;
    char buffer[256];
    UInt32 index = 0;
    pid_t pid;

    if (0 == (fpipe = (FILE*)popen(command, "r"))) {
        XCTFail("Unable to run pgrep");
    }

    memset(buffer, 0, sizeof(buffer));
    while (fread(&c, sizeof c, 1, fpipe)) {
        buffer[index++] = c;
    }
    pclose(fpipe);

    pid = atoi(buffer);
    XCTAssert(pid, "Unable to get authd PID");
    
    fprintf(stdout, "authd PID is %d", pid);
    
    asprintf(&cmd, "leaks %d >/dev/null", pid);
    if (cmd) {
        ret = system(cmd);
        free(cmd);
    }
    XCTAssert(ret == 0, "Leaks in authd detected");
}

@end
