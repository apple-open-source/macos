//
//  SecRecoveryKey.h
//
//

#ifndef SecRecoveryKey_h
#define SecRecoveryKey_h

#include <Security/Security.h>

#if __OBJC__
@class SecRecoveryKey;
#else
typedef struct __SecRecoveryKey SecRecoveryKey;
#endif

bool
SecRKRegisterBackupPublicKey(SecRecoveryKey *rk, CFErrorRef *error);

#if __OBJC__

SecRecoveryKey *
SecRKCreateRecoveryKey(NSString *recoveryKey);

NSString *
SecRKCreateRecoveryKeyString(NSError **error);

NSString *
SecRKCopyAccountRecoveryPassword(SecRecoveryKey *rk);

NSData *
SecRKCopyBackupFullKey(SecRecoveryKey *rk);

NSData *
SecRKCopyBackupPublicKey(SecRecoveryKey *rk);

#else

SecRecoveryKey *
SecRKCreateRecoveryKey(CFStringRef recoveryKey);

CFDataRef
SecRKCopyBackupFullKey(SecRecoveryKey *rk);

CFDataRef
SecRKCopyBackupPublicKey(SecRecoveryKey *rk);

#endif

#endif
