/******************************************************************************
** 
**  $Id: p11x_async.c,v 1.2 2003/02/13 20:06:40 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: Asynchronous functions (UNIX version)
** 
******************************************************************************/

#include "cryptoki.h"
#include <time.h>

#ifndef WIN32
#include <unistd.h>
#endif

/******************************************************************************
** Function: async_StartSlotWatcher
**
** Starts a thread that watches all the slots in the system.
**
** Parameters:
**  none
**
** Returns:
**  CKR_HOST_MEMORY on memory alloc error
**  CKR_FUNCTION_FAILED on general error
**  CKR_OK
*******************************************************************************/
CK_RV async_StartSlotWatcher()
{
    CK_RV rv = CKR_OK;
    CK_ULONG i;
    MSCTokenInfo *tokenArray;

    tokenArray = (MSCTokenInfo *)malloc(st.slot_count * sizeof(MSCTokenInfo));

    if (!tokenArray)
        return CKR_HOST_MEMORY;

    for (i = 0; i < st.slot_count; i++)
    {
        memcpy(&tokenArray[i], &st.slots[i].conn.tokenInfo, sizeof(MSCTokenInfo));
        tokenArray[i].tokenState = 0;
    }

    st.slot_status = (char *)malloc(st.slot_count);

    if (!st.slot_status)
        rv = CKR_HOST_MEMORY;
    else
    {
        memset(st.slot_status, 0x01, st.slot_count);

        if (st.prefs.threaded && st.create_threads)
        {
            if (MSC_ERROR(msc_CallbackForTokenEvent(tokenArray, 
                                                   st.slot_count, 
                                                   async_TokenEventCallback, 
                                                   0)))
            {
                free(st.slot_status);
                st.slot_status = 0;
                rv = CKR_FUNCTION_FAILED;
            }
        }
    }

    if (tokenArray)
        free(tokenArray);

    return rv;
}

/******************************************************************************
** Function: async_StopSlotWatcher
**
** Stops the slot watching thread.
**
** Parameters:
**  none
**
** Returns:
**  CKR_OK
*******************************************************************************/
CK_RV async_StopSlotWatcher()
{
    CK_RV rv = CKR_OK;

    if (st.prefs.threaded && st.create_threads)
        msc_CallbackCancelEvent();

    if (st.slot_status)
    {
        free(st.slot_status);
        st.slot_status = 0;    
    }

    return rv;
}

/******************************************************************************
** Function: async_TokenEventCallback
**
** Called for asynchronous token events.  Depending on the thread mode this
** will either set a flag that will be picked up later by any call into the
** P11 module, or if in fully threaded mode this will update all information
** about the token in the callback thread.
**
** Parameters:
**  tokenArray - Token status information
**  arrayLen   - Number of items in tokenArray
**  data       - Pointer to "data" that was set with MSCCallbackForTokenEvent
**
** Returns:
**  CKR_OK
*******************************************************************************/
MSCULong32 async_TokenEventCallback(MSCTokenInfo *tokenArray, MSCULong32 arrayLen, void *data)
{
    CK_ULONG i;

    for (i = 0; i < arrayLen; i++)
    {
        if (tokenArray[i].tokenState & MSC_STATE_CHANGED)
        {
            log_Log(LOG_LOW, "Async event on slot %ld: 0x%lX", i + 1, tokenArray[i].tokenState);
            st.slot_status[i] = 0x01;
        }
    }

    if (st.prefs.slot_watch_scheme == P11_SLOT_WATCH_THREAD_FULL)
    {
        thread_MutexLock(st.async_lock);
        slot_TokenChanged();
        thread_MutexUnlock(st.async_lock);
    }

    return MSC_SUCCESS;
}

