//
//  SOSAccountLog.h
//  sec
//
//  Created by Richard Murphy on 6/1/16.
//
//

#ifndef SOSAccountLog_h
#define SOSAccountLog_h

#include <stdio.h>
#import <Security/SecureObjectSync/SOSAccountPriv.h>
#include <Security/SecureObjectSync/SOSTransportCircle.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSViews.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecureObjectSync/SOSPeerInfoPriv.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>
#include <Security/SecureObjectSync/SOSPeerInfoDER.h>
#import <Security/SecureObjectSync/SOSAccountPriv.h>

//#include <Security/SecureObjectSync/SOSBackupSliceKeyBag.h>
@class SOSAccount;

void SOSAccountLog(SOSAccount* account);
SOSAccount* SOSAccountCreateFromStringRef(CFStringRef hexString);

#endif /* SOSAccountLog_h */
