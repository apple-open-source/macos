/******************************************************************************
** 
**  $Id: p11_parallel.c,v 1.2 2003/02/13 20:06:39 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: Parallel function management (deprecated)
** 
******************************************************************************/

#include "cryptoki.h"

/* Parallel function management */

/* C_GetFunctionStatus is a legacy function; it obtains an
 * updated status of a function running in parallel with an
 * application. */
CK_DEFINE_FUNCTION(CK_RV, C_GetFunctionStatus)
(
  CK_SESSION_HANDLE hSession  /* the session's handle */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_GetFunctionStatus");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_PARALLEL;
    log_Log(LOG_MED, "Legacy function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_GetFunctionStatus");

    return rv;
}


/* C_CancelFunction is a legacy function; it cancels a function
 * running in parallel. */
CK_DEFINE_FUNCTION(CK_RV, C_CancelFunction)
(
  CK_SESSION_HANDLE hSession  /* the session's handle */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_CancelFunction");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_PARALLEL;
    log_Log(LOG_MED, "Legacy function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_CancelFunction");

    return rv;
}



