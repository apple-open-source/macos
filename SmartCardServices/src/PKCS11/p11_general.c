/******************************************************************************
** 
**  $Id: p11_general.c,v 1.2 2003/02/13 20:06:38 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: General-purpose
** 
******************************************************************************/

#include "cryptoki.h"

/* Win32 defines CreateMutex as a macro (CreateMutexA) */
/* We need the preprocessor to not process the variable name in pInitArgs */
#ifdef WIN32
#undef CreateMutex
#endif

/* C_Initialize initializes the Cryptoki library. */
CK_DEFINE_FUNCTION(CK_RV, C_Initialize)
(
    CK_VOID_PTR pInitArgs  /* if this is not NULL_PTR, it gets * cast to CK_C_INITIALIZE_ARGS_PTR * and dereferenced */
)
{
    CK_RV rv = CKR_OK;
    CK_C_INITIALIZE_ARGS blank_init_args;
    CK_C_INITIALIZE_ARGS *init_args;

    memset(&blank_init_args, 0x00, sizeof(blank_init_args));

    /* Must initialize the state and preferences before any logging starts */
    rv = state_Init(); 

    P11_LOG_START("C_Initialize");

    if (CKR_ERROR(rv))
        /* Do nothing */;
    else if (st.initialized)
        rv = CKR_CRYPTOKI_ALREADY_INITIALIZED;
    else
    {
        if (!pInitArgs)
            init_args = &blank_init_args;
        else 
        {
            init_args = (CK_C_INITIALIZE_ARGS *)pInitArgs;
            log_Log(LOG_LOW, "Init flags: 0x%X", init_args->flags);
        }

        /* If preferences request non-threaded mode then force this flag */
        if (!st.prefs.threaded)
            init_args->flags |= CKF_LIBRARY_CANT_CREATE_OS_THREADS;

        if ((CKF_OS_LOCKING_OK & init_args->flags))
        {
            log_Log(LOG_LOW, "Native thread locks are fine");
            st.native_locks = 1;
        }
        else
        {
            log_Log(LOG_LOW, "Using non-native (application supplied) thread locks");
            st.native_locks = 0;
        }

        if ((CKF_LIBRARY_CANT_CREATE_OS_THREADS & init_args->flags))
        {
            log_Log(LOG_LOW, "Application does not allow thread creation; disabling threaded slot watcher");
            st.create_threads = 0;
        }
        else
        {
            log_Log(LOG_LOW, "Creating threads is fine; using threaded slot watcher");
            st.create_threads = 1;
        }

        if (init_args->pReserved)
            rv = CKR_ARGUMENTS_BAD;
        else if ((init_args->CreateMutex || init_args->DestroyMutex || init_args->LockMutex || init_args->UnlockMutex) &&
                 (!init_args->CreateMutex || !init_args->DestroyMutex || !init_args->LockMutex || !init_args->UnlockMutex))
        {
            rv = CKR_ARGUMENTS_BAD;
        }
        else 
        {
            if (!st.prefs.threaded)
                log_Log(LOG_LOW, "All threading disabled");
            else
            {
                if (st.native_locks)
                    rv = thread_Initialize();
                else
                    rv = thread_InitFunctions(init_args->CreateMutex,
                                              init_args->DestroyMutex,
                                              init_args->LockMutex,
                                              init_args->UnlockMutex);

                if (CKR_ERROR(rv))
                    log_Log(LOG_MED, "Could not initialize thread subsystem");
                else
                {
                    thread_MutexInit(&st.log_lock);
                    thread_MutexInit(&st.async_lock);
                }
            }

            if (!CKR_ERROR_NOLOG(rv))
            {
                if (CKR_ERROR(rv = slot_UpdateSlotList()))
                    rv = CKR_DEVICE_REMOVED;
                else if (!CKR_ERROR(rv = async_StartSlotWatcher()))
                {
                    st.initialized = TRUE;
                    (void)CKR_ERROR(slot_TokenChanged());
                }
            }
        }
    }

    P11_LOG_END("C_Initialize");
    return rv;
}


/* C_Finalize indicates that an application is done with the
 * Cryptoki library. */
CK_DEFINE_FUNCTION(CK_RV, C_Finalize)
(
  CK_VOID_PTR   pReserved  /* reserved.  Should be NULL_PTR */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_Finalize");

    /* Pause in case there are pending active operations */
    thread_MutexLock(st.async_lock);
    thread_MutexUnlock(st.async_lock);

    if (!st.initialized)
        rv = CKR_CRYPTOKI_NOT_INITIALIZED;
    else if (pReserved)
        rv = CKR_ARGUMENTS_BAD;
    else
    {
        (void)CKR_ERROR(async_StopSlotWatcher());

        if (st.log_lock)
        {
            thread_MutexDestroy(st.log_lock);
            st.log_lock = 0;
        }

        if (st.async_lock)
        {
            thread_MutexDestroy(st.async_lock);
            st.async_lock = 0;
        }

        if (st.prefs.threaded)
            thread_Finalize();

        (void)CKR_ERROR(state_Free());
    }

    P11_LOG_END("C_Finalize");

    return rv;
}


/* C_GetInfo returns general information about Cryptoki. */
CK_DEFINE_FUNCTION(CK_RV, C_GetInfo)
(
  CK_INFO_PTR   pInfo  /* location that receives information */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_GetInfo");

    thread_MutexLock(st.async_lock);

    if (!st.initialized)
        rv = CKR_CRYPTOKI_NOT_INITIALIZED;
    else if(!pInfo)
        rv = CKR_ARGUMENTS_BAD;
    else
    {
        log_Log(LOG_LOW, "PKCS #11 version: %d.%d", st.prefs.version_major, st.prefs.version_minor);
        pInfo->cryptokiVersion.major = (CK_BYTE)st.prefs.version_major;
        pInfo->cryptokiVersion.minor = (CK_BYTE)st.prefs.version_minor;
        util_PadStrSet(pInfo->manufacturerID, (CK_CHAR *)PKCS11_MFR_ID, sizeof(pInfo->manufacturerID));
        pInfo->flags = 0;
        util_PadStrSet(pInfo->libraryDescription, (CK_CHAR *)PKCS11_DESC, sizeof(pInfo->libraryDescription));
        pInfo->libraryVersion.major = PKCS11_LIB_MAJOR;
        pInfo->libraryVersion.minor = PKCS11_LIB_MINOR;
    }

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_GetInfo");
    return rv;
}


/* C_GetFunctionList returns the function list. */
CK_DEFINE_FUNCTION(CK_RV, C_GetFunctionList)
(
  CK_FUNCTION_LIST_PTR_PTR ppFunctionList  /* receives pointer to
                                            * function list */
)
{
    CK_RV rv = CKR_OK;
    static CK_FUNCTION_LIST fnList;

    /* Must initialize the state and preferences before any logging starts */
    rv = state_Init();

    P11_LOG_START("C_GetFunctionList");

    if (CKR_ERROR(rv))
        /* Do nothing */;
    else if (!ppFunctionList)
        rv = CKR_ARGUMENTS_BAD;
    else
    {
        fnList.version.major = (CK_BYTE)st.prefs.version_major;
        fnList.version.minor = (CK_BYTE)st.prefs.version_minor;

#define CK_PKCS11_FUNCTION_INFO(name) fnList.name = name;
#include "pkcs11f.h"
#undef CK_PKCS11_FUNCTION_INFO

        *ppFunctionList = &fnList;
    }

    P11_LOG_END("C_GetFunctionList");
    return rv;
}

