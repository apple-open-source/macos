/******************************************************************************
**
**  $Id: p11x_thread.c,v 1.2 2003/02/13 20:06:42 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: Thread functions.  This should probably just use the Musclecard
**           framework functions instead (if in native mode).
**
******************************************************************************/

#include "cryptoki.h"

#include "thread_generic.h"

static int (*p11_pthread_mutex_init)(PCSCLITE_MUTEX_T mMutex) = 0;
static int (*p11_pthread_mutex_destroy)(PCSCLITE_MUTEX_T mMutex) = 0;
static int (*p11_pthread_mutex_lock)(PCSCLITE_MUTEX_T mMutex) = 0;
static int (*p11_pthread_mutex_unlock)(PCSCLITE_MUTEX_T mMutex) = 0;

static CK_CREATEMUTEX p11_createmutex = 0;
static CK_DESTROYMUTEX p11_destroymutex = 0;
static CK_LOCKMUTEX p11_lockmutex = 0;
static CK_UNLOCKMUTEX p11_unlockmutex = 0;

/******************************************************************************
** Function: thread_Initialize
**
** Initializes the thread subsystem
**
** Parameters:
**  none
**
** Returns:
**  CKR_FUNCTION_FAILED on error
**  CKR_OK
*******************************************************************************/
CK_RV thread_Initialize()
{
    CK_RV rv = CKR_OK;

#ifdef HAVE_LIBPTHREAD
    if (st.prefs.threaded && st.native_locks)
    {
        p11_pthread_mutex_init = SYS_MutexInit;
        p11_pthread_mutex_destroy = SYS_MutexDestroy;
        p11_pthread_mutex_lock = SYS_MutexLock;
        p11_pthread_mutex_unlock = SYS_MutexUnLock;
    }
#else
    P11_ERR("Trying to initialize thread subsystem while threads are disabled");
    rv = CKR_FUNCTION_FAILED;
#endif

    return rv;
}

/******************************************************************************
** Function: thread_InitFunctions
**
** Initializes the thread subsystem to use external locking mechanisms
**
** Parameters:
**  none
**
** Returns:
**  CKR_ARGUMENTS_BAD if any function is null
**  CKR_FUNCTION_FAILED on error
**  CKR_OK
*******************************************************************************/
CK_RV thread_InitFunctions(CK_CREATEMUTEX fn_createmutex,
                           CK_DESTROYMUTEX fn_destroymutex,
                           CK_LOCKMUTEX fn_lockmutex,
                           CK_UNLOCKMUTEX fn_unlockmutex)
{
    CK_RV rv = CKR_OK;

    if (!st.prefs.threaded)
        rv = CKR_FUNCTION_FAILED;
    else
    {
        if (!fn_createmutex || !fn_destroymutex || !fn_lockmutex || !fn_unlockmutex)
            rv = CKR_ARGUMENTS_BAD;
        else
        {
            log_Log(LOG_LOW, "Using application supplied locking functions");
            p11_createmutex = fn_createmutex;
            p11_destroymutex = fn_destroymutex;
            p11_lockmutex = fn_lockmutex;
            p11_unlockmutex = fn_unlockmutex;
        }
    }

    return rv;
}

/******************************************************************************
** Function: thread_Finalize
**
** Unloads the Pthread library
**
** Parameters:
**  none
**
** Returns:
**  CKR_FUNCTION_FAILED on general error
**  CKR_OK
*******************************************************************************/
CK_RV thread_Finalize()
{
    CK_RV rv = CKR_OK;

    if (st.prefs.threaded)
    {
        p11_pthread_mutex_init = 0;
        p11_pthread_mutex_destroy = 0;
        p11_pthread_mutex_lock = 0;
        p11_pthread_mutex_unlock = 0;
        p11_createmutex = 0;
        p11_destroymutex = 0;
        p11_lockmutex = 0;
        p11_unlockmutex = 0;
    }

    return rv;
}

/******************************************************************************
** Function: thread_MutexInit
**
** Initializes a mutex
**
** Parameters:
**  mutex - Pointer to mutex memory
**
** Returns:
**  CKR_HOST_MEMORY on memory alloc error
**  CKR_FUNCTION_FAILED on general error
**  CKR_OK
*******************************************************************************/
CK_RV thread_MutexInit(P11_Mutex *mutex)
{
    CK_RV rv = CKR_OK;

    if (st.prefs.threaded)
    {
        if (st.native_locks && mutex && p11_pthread_mutex_init)
        {
            *mutex = (P11_Mutex *)malloc(sizeof(PCSCLITE_MUTEX));

            if (!*mutex)
                rv = CKR_HOST_MEMORY;
            else
                p11_pthread_mutex_init((PCSCLITE_MUTEX_T)*mutex);
        }
        else if (!st.native_locks && mutex && p11_createmutex)
            p11_createmutex(mutex);
        else
            rv = CKR_FUNCTION_FAILED;
    }

    return rv;
}

/******************************************************************************
** Function: thread_MutexDestroy
**
** Destroys a mutex
**
** Parameters:
**  mutex - Pointer to mutex memory
**
** Returns:
**  CKR_FUNCTION_FAILED on general error
**  CKR_OK
*******************************************************************************/
CK_RV thread_MutexDestroy(P11_Mutex mutex)
{
    CK_RV rv = CKR_OK;

    if (st.prefs.threaded)
    {
        if (st.native_locks && mutex && p11_pthread_mutex_destroy)
        {
            p11_pthread_mutex_destroy((PCSCLITE_MUTEX_T)mutex);
            free(mutex);
        }
        else if (!st.native_locks && mutex && p11_destroymutex)
            p11_destroymutex(mutex);
        else
            rv = CKR_FUNCTION_FAILED;
    }

    return rv;
}

/******************************************************************************
** Function: thread_MutexLock
**
** Locks a mutex
**
** Parameters:
**  mutex - Pointer to mutex memory
**
** Returns:
**  CKR_FUNCTION_FAILED on general error
**  CKR_OK
*******************************************************************************/
CK_RV thread_MutexLock(P11_Mutex mutex)
{
    CK_RV rv = CKR_OK;

    if (st.prefs.threaded)
    {
        if (st.native_locks && mutex && p11_pthread_mutex_lock)
            p11_pthread_mutex_lock((PCSCLITE_MUTEX_T)mutex);
        else if (!st.native_locks && mutex && p11_lockmutex)
            p11_lockmutex(mutex);
        else
            rv = CKR_FUNCTION_FAILED;
    }

    return rv;
}

/******************************************************************************
** Function: thread_MutexUnlock
**
** Unlocks a mutex
**
** Parameters:
**  mutex - Pointer to mutex memory
**
** Returns:
**  CKR_FUNCTION_FAILED on general error
**  CKR_OK
*******************************************************************************/
CK_RV thread_MutexUnlock(P11_Mutex mutex)
{
    CK_RV rv = CKR_OK;

    if (st.prefs.threaded)
    {
        if (st.native_locks && mutex && p11_pthread_mutex_unlock)
            p11_pthread_mutex_unlock((PCSCLITE_MUTEX_T)mutex);
        else if (!st.native_locks && mutex && p11_unlockmutex)
            p11_unlockmutex(mutex);
        else
            rv = CKR_FUNCTION_FAILED;
    }

    return rv;
}

