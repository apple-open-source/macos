#ifndef __PTLOCK_H__
#define __PTLOCK_H__

#ifdef __cplusplus
extern "C" {
#endif
    
#include <CoreFoundation/CFBase.h>
#include <pthread.h>

typedef struct __PTLock * PTLockRef;

PTLockRef  PTLockCreate(void);
void       PTLockFree(PTLockRef lock);

Boolean    PTLockTryLock(PTLockRef lock);
void       PTLockTakeLock(PTLockRef lock);
void       PTLockUnlock(PTLockRef lock);

#ifdef __cplusplus
}
#endif
#endif __PTLOCK_H__

