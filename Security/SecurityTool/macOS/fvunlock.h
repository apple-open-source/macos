//
//  fvunlock.h
//  SecurityTool

#ifndef fvunlock_h
#define fvunlock_h

// ensures we are in the Recovery and admin is verified
OSStatus recoverySetup(void);

// verifies that provided volume UUID is usable 
OSStatus verifyVolume(const char *uuid);

// writes resetdb file
OSStatus fvUnlockWriteResetDb(const char *uuid);

int fvunlock(int argc, char * const *argv);

Boolean isInFVUnlock(void);

#endif
