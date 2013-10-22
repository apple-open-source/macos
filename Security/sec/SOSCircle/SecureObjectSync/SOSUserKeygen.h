//
//  SOSUserKeygen.h
//  sec
//
//  Created by Richard Murphy on 2/21/13.
//
//

#ifndef sec_SOSUserKeygen_h
#define sec_SOSUserKeygen_h
#include <SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecKey.h>

CFDataRef SOSUserKeyCreateGenerateParameters(CFErrorRef *error);
SecKeyRef SOSUserKeygen(CFDataRef password, CFDataRef parameters, CFErrorRef *error);

void debugDumpUserParameters(CFStringRef message, CFDataRef parameters);

#endif
