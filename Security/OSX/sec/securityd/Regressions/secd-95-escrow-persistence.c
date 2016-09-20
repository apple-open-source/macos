/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */


#include <Security/SecBase.h>
#include <Security/SecItem.h>

#include <CoreFoundation/CFDictionary.h>

#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSUserKeygen.h>
#include <Security/SecureObjectSync/SOSTransport.h>

#include <stdlib.h>
#include <unistd.h>

#include "secd_regressions.h"
#include "SOSTestDataSource.h"

#include "SOSRegressionUtilities.h"
#include <utilities/SecCFWrappers.h>
#include <Security/SecKeyPriv.h>

#include <securityd/SOSCloudCircleServer.h>

#include "SOSAccountTesting.h"

#include "SecdTestKeychainUtilities.h"

static int kTestTestCount = 63;

static void tests(void)
{
    CFErrorRef error = NULL;
    
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    CFStringRef cfaccount = CFSTR("test@test.org");
    
    CFMutableDictionaryRef changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSAccountRef alice_account = CreateAccountForLocalChanges(CFSTR("Alice"), CFSTR("TestSource"));
    SOSAccountRef bob_account = CreateAccountForLocalChanges(CFSTR("Bob"), CFSTR("TestSource"));
    
    ok(SOSAccountAssertUserCredentialsAndUpdate(bob_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    
    // Bob wins writing at this point, feed the changes back to alice.
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 1, "updates");
    
    ok(SOSAccountAssertUserCredentialsAndUpdate(alice_account, cfaccount, cfpassword, &error), "Credential setting (%@)", error);
    CFReleaseNull(error);
    
    CFReleaseNull(cfpassword);
    CFReleaseNull(error);
    
    ok(SOSAccountResetToOffering_wTxn(alice_account, &error), "Reset to offering (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");
    
    ok(SOSAccountJoinCircles_wTxn(bob_account, &error), "Bob Applies (%@)", error);
    CFReleaseNull(error);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        
        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicants %@ (%@)", applicants, error);
        CFReleaseNull(error);
        CFReleaseSafe(applicants);
    }
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 1, "updates");
    CFMutableStringRef timeDescription = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("["));
    CFAbsoluteTime currentTimeAndDate = CFAbsoluteTimeGetCurrent();
    
    withStringOfAbsoluteTime(currentTimeAndDate, ^(CFStringRef decription) {
        CFStringAppend(timeDescription, decription);
    });
    CFStringAppend(timeDescription, CFSTR("]"));
    
    int tries = 5;
    
    CFNumberRef attempts = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &tries);
    
    CFMutableArrayRef escrowTimeAndTries = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFArrayAppendValue(escrowTimeAndTries, timeDescription);
    CFArrayAppendValue(escrowTimeAndTries, attempts);
    CFDictionaryRef escrowRecord = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, CFSTR("account label"), escrowTimeAndTries, NULL);
    
    CFMutableDictionaryRef record = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryAddValue(record, CFSTR("12345"), escrowRecord);
    
    SOSFullPeerInfoRef alice_fpi = SOSAccountGetMyFullPeerInfo(alice_account);
    ok(SOSFullPeerInfoAddEscrowRecord(alice_fpi, CFSTR("12345"), escrowRecord, &error), "Adding Escrow records to Alice FPI(%@)", error);
    CFDictionaryRef fpi_escrow = SOSPeerInfoCopyEscrowRecord(SOSFullPeerInfoGetPeerInfo(alice_fpi));
    ok(CFEqualSafe(CFDictionaryGetValue(fpi_escrow, CFSTR("12345")), escrowRecord), "Alice's FPI has escrow (%@)", error);
       
    ok(SOSAccountAddEscrowRecords(bob_account, CFSTR("12345"), escrowRecord, &error), "Adding escrow to Bob's account (%@)", error);
    CFReleaseNull(fpi_escrow);

    fpi_escrow = (CFDictionaryRef)SOSAccountGetValue(bob_account, kSOSEscrowRecord, NULL);
    ok(CFEqualSafe(CFDictionaryGetValue(fpi_escrow, CFSTR("12345")), escrowRecord), "Bob has escrow records in account (%@)", error);
    
    ok(SOSAccountResetToEmpty(alice_account, &error), "Reset to offering (%@)", error);
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");

    ok(SOSAccountResetToEmpty(bob_account, &error), "Reset to offering (%@)", error);
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");

    ok(SOSAccountAddEscrowRecords(bob_account, CFSTR("12345"), escrowRecord, &error), "Adding escrow to Bob's account (%@)", error);

    ok(SOSAccountResetToOffering_wTxn(alice_account, &error), "Reset to offering (%@)", error);
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");

    CFDictionaryRef bob_fpi_escrow = SOSPeerInfoCopyEscrowRecord(SOSFullPeerInfoGetPeerInfo(bob_account->my_identity));
    ok(bob_fpi_escrow == NULL, "Bob's FPI escrow should be null");
    CFReleaseNull(bob_fpi_escrow);

    ok(SOSAccountJoinCircles_wTxn(bob_account, &error), "Bob Applies (%@)", error);
    bob_fpi_escrow = SOSPeerInfoCopyEscrowRecord(SOSFullPeerInfoGetPeerInfo(bob_account->my_identity));
    ok(CFEqualSafe(CFDictionaryGetValue(bob_fpi_escrow, CFSTR("12345")), escrowRecord), "Bob has escrow records in account (%@)", error);
    CFReleaseNull(bob_fpi_escrow);
    
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 2, "updates");
    
    {
        CFArrayRef applicants = SOSAccountCopyApplicants(alice_account, &error);
        ok(applicants && CFArrayGetCount(applicants) == 1, "See one applicants %@ (%@)", applicants, error);
        ok(SOSAccountAcceptApplicants(alice_account, applicants, &error), "Accept bob into the fold");
        CFReleaseNull(error);
        CFReleaseNull(applicants);
    }
    is(ProcessChangesUntilNoChange(changes, alice_account, bob_account, NULL), 3, "updates");

    fpi_escrow = (CFDictionaryRef)SOSAccountGetValue(bob_account, kSOSEscrowRecord, NULL);
    ok(isNull(fpi_escrow), "Bob's escrow records in the account object should be gone");
    
    CFReleaseNull(record);
    CFReleaseNull(escrowRecord);
    CFReleaseNull(timeDescription);
    CFReleaseNull(attempts);
}

int secd_95_escrow_persistence(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);

    tests();
    
    return 0;
}
