//
//  SOSAccountLog.c
//  sec
//
//  Created by Richard Murphy on 6/1/16.
//
//

#include "SOSAccountLog.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <AssertMacros.h>
#include "SOSAccountPriv.h"
#include "SOSViews.h"
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCoreCrypto.h>
#include <utilities/SecBuffer.h>
#include <SOSPeerInfoDER.h>

#include <Security/SecureObjectSync/SOSTransport.h>

#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <os/state_private.h>

// Keep these for later
static CFStringRef SOSAccountCreateStringRef(SOSAccountRef account) {
    CFStringRef hex = NULL;
    
    CFDataRef derdata = SOSAccountCopyEncodedData(account, kCFAllocatorDefault, NULL);
    require_quiet(derdata, errOut);
    hex = CFDataCopyHexString(derdata);
errOut:
    CFRelease(derdata);
    return hex;
}

void SOSAccountLog(SOSAccountRef account) {
    CFStringRef hex = SOSAccountCreateStringRef(account);
    if(!hex) return;
    secdebug("accountLog", "Full contents: %@", hex);
    CFRelease(hex);
}

SOSAccountRef SOSAccountCreateFromStringRef(CFStringRef hexString) {
    CFDataRef accountDER = CFDataCreateFromHexString(kCFAllocatorDefault, hexString);
    if(!accountDER) return NULL;
    SOSAccountRef account = SOSAccountCreateFromData(kCFAllocatorDefault, accountDER, NULL, NULL);
    CFReleaseNull(accountDER);
    return account;
}
