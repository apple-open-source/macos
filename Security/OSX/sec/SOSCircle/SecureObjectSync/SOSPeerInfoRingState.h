//
//  SOSPeerInfoRingState.h
//  sec
//
//  Created by Richard Murphy on 3/6/15.
//
//

#ifndef _sec_SOSPeerInfoRingState_
#define _sec_SOSPeerInfoRingState_

#include <AssertMacros.h>
#include <TargetConditionals.h>

#include "SOSViews.h"
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFError.h>
#include <Security/SecureObjectSync/SOSInternal.h>

#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>
#include <Security/SecureObjectSync/SOSPeerInfoPriv.h>
#include <Security/SecureObjectSync/SOSRingTypes.h>
#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>

SOSRingStatus SOSPeerInfoGetRingState(SOSPeerInfoRef pi, CFStringRef ringname, CFErrorRef *error);

#endif /* defined(_sec_SOSPeerInfoRingState_) */
