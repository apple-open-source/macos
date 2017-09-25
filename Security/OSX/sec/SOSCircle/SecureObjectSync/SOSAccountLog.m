//
//  SOSAccountLog.c
//  sec
//
#import <Security/SecureObjectSync/SOSAccountPriv.h>
#include "SOSAccountLog.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <AssertMacros.h>
#include "SOSAccountPriv.h"
#include "SOSViews.h"
#include <utilities/SecCFWrappers.h>
#import <utilities/SecNSAdditions.h>
#include <utilities/SecCoreCrypto.h>
#include <utilities/SecBuffer.h>
#include <Security/SecureObjectSync/SOSPeerInfoDER.h>

#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <os/state_private.h>

// Keep these for later
void SOSAccountLog(SOSAccount* account) {
    NSString* hex = [[account encodedData: nil] asHexString];
    if(!hex) return;
    secdebug("accountLog", "Full contents: %@", hex);
}

SOSAccount* SOSAccountCreateFromStringRef(CFStringRef hexString) {
    CFDataRef accountDER = CFDataCreateFromHexString(kCFAllocatorDefault, hexString);
    if(!accountDER) return NULL;
    SOSAccount* account = [SOSAccount accountFromData:(__bridge NSData*) accountDER
                                              factory:NULL
                                                error:nil];
    CFReleaseNull(accountDER);
    return account;
}
