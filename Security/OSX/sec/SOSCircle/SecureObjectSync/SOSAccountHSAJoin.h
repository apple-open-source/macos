//
//  SOSAccountHSAJoin.h
//  sec
//
//  Created by Richard Murphy on 3/23/15.
//
//

#ifndef _sec_SOSAccountHSAJoin_
#define _sec_SOSAccountHSAJoin_

#include "SOSAccountPriv.h"

CFMutableSetRef SOSAccountCopyPreApprovedHSA2Info(SOSAccountRef account);
bool SOSAccountSetHSAPubKeyExpected(SOSAccountRef account, CFDataRef pubKeyBytes, CFErrorRef *error);
bool SOSAccountVerifyAndAcceptHSAApplicants(SOSAccountRef account, SOSCircleRef newCircle, CFErrorRef *error);
bool SOSAccountClientPing(SOSAccountRef account);

#endif /* defined(_sec_SOSAccountHSAJoin_) */
