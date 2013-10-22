//
//  SOSUserKey.h
//  sec
//
//  Created by Richard Murphy on 2/13/13.
//
//

#ifndef sec_SOSUserKey_h
#define sec_SOSUserKey_h

#include <Security/Security.h>

bool
SOSUserKeyGenerate(int keysize, CFStringRef user_label, CFDataRef user_password, SecKeyRef *user_pubkey, SecKeyRef *user_privkey);


#endif
