
#ifndef _PTLOCK_H_
#define _PTLOCK_H_

#ifdef __cplusplus
extern "C" {
#endif
    
#include <CoreFoundation/CFBase.h>
#include <pthread.h>

typedef struct __PTLock * PTLockRef;

PTLockRef 	PTLockCreate(void);
void 		PTLockFree(PTLockRef lock);

Boolean		PTLockTryLock(PTLockRef lock);
void 		PTLockTakeLock(PTLockRef lock);
void 		PTLockUnlock(PTLockRef lock);

#ifdef __cplusplus
}
#endif
#endif _PTLOCK_H_

