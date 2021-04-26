//
//  secd-202-recoverykey.c
//  sec
//

#import <Security/Security.h>
#import <Security/SecKeyPriv.h>

#import <Foundation/Foundation.h>

#import <Security/SecRecoveryKey.h>

#import <stdio.h>
#import <stdlib.h>
#import <unistd.h>

#import "secd_regressions.h"
#import "SOSTestDataSource.h"
#import "SOSTestDevice.h"

#import "SOSRegressionUtilities.h"
#import <utilities/SecCFWrappers.h>

#import "SecdTestKeychainUtilities.h"
#include "SOSAccountTesting.h"

#if SOS_ENABLED


const int kTestRecoveryKeyCount = 3;

static void testRecoveryKey(void)
{
    SecRecoveryKey *recoveryKey = NULL;

    recoveryKey = SecRKCreateRecoveryKeyWithError(@"AAAA-AAAA-AAAA-AAAA-AAAA-AAAA-AAGW", NULL);
    ok(recoveryKey, "got recovery key");

    NSData *publicKey = SecRKCopyBackupPublicKey(recoveryKey);
    ok(publicKey, "got publicKey");
}

const int kTestRecoveryKeyBasicNumberIterations = 100;
const int kTestRecoveryKeyBasicCount = 1 * kTestRecoveryKeyBasicNumberIterations;

static void testRecoveryKeyBasic(void)
{
    NSString *recoveryKey = NULL;
    NSError *error = NULL;
    int n;

    for (n = 0; n < kTestRecoveryKeyBasicNumberIterations; n++) {
        recoveryKey = SecRKCreateRecoveryKeyString(&error);
        ok(recoveryKey, "SecRKCreateRecoveryKeyString: %@", error);
    }
}

#endif

int secd_202_recoverykey(int argc, char *const *argv)
{
#if SOS_ENABLED
    plan_tests(kTestRecoveryKeyCount + kTestRecoveryKeyBasicCount);
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    testRecoveryKeyBasic();
    testRecoveryKey();
    secd_test_teardown_delete_temp_keychain(__FUNCTION__);
#else
    plan_tests(0);
#endif
    return 0;
}
