/******************************************************************************
** 
**  $Id: p11_token.c,v 1.2 2003/02/13 20:06:39 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: Slot and token management
** 
******************************************************************************/

#include "cryptoki.h"

/* C_GetSlotList obtains a list of slots in the system. */
CK_DEFINE_FUNCTION(CK_RV, C_GetSlotList)
(
  CK_BBOOL       tokenPresent,  /* only slots with tokens? */
  CK_SLOT_ID_PTR pSlotList,     /* receives array of slot IDs */
  CK_ULONG_PTR   pulCount       /* receives number of slots */
)
{
    CK_RV rv = CKR_OK;
    CK_RV token_rv;
    CK_ULONG i, count = 0;

    P11_LOG_START("C_GetSlotList");

    thread_MutexLock(st.async_lock);

    (void)CKR_ERROR(slot_TokenChanged());

    if (!pulCount)
        rv = CKR_ARGUMENTS_BAD;
    else if (!st.slots)
    {
        *pulCount = 0;
        log_Log(LOG_LOW, "No active slots");
    }
    else if (!pSlotList && !tokenPresent)
    {
        *pulCount = st.slot_count;
        log_Log(LOG_LOW, "Returning slot count: %ld", *pulCount);
    }
    else if ((*pulCount < st.slot_count) && !tokenPresent)
        rv = CKR_BUFFER_TOO_SMALL;
    else if (!tokenPresent)
    {
        *pulCount = st.slot_count;

        for (i = 0; i < *pulCount; i++)
        {
             pSlotList[i] = i + 1;
             log_Log(LOG_MED, "Found reader at slot: %ld", pSlotList[i]);
        }
    } 
    else /* Look for readers with tokens present */
    {
        for (i = 1, count = 0; i <= st.slot_count; i++)
        {
            token_rv = slot_TokenPresent(i);
            if ((token_rv == CKR_OK) || (token_rv == CKR_TOKEN_NOT_RECOGNIZED))
            {
                if (pSlotList)
                    pSlotList[count] = i;

                log_Log(LOG_MED, "Found reader with token at slot: %ld", i);
                count++;
            }
        }

        *pulCount = count;
    }

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_GetSlotList");
    return rv;
}


/* C_GetSlotInfo obtains information about a particular slot in
 * the system. */
CK_DEFINE_FUNCTION(CK_RV, C_GetSlotInfo)
(
  CK_SLOT_ID       slotID,  /* the ID of the slot */
  CK_SLOT_INFO_PTR pInfo    /* receives the slot information */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_GetSlotInfo");

    thread_MutexLock(st.async_lock);
    log_Log(LOG_LOW, "Checking slot: %ld", slotID);

    (void)CKR_ERROR(slot_TokenChanged());

    if (!pInfo)
        rv = CKR_ARGUMENTS_BAD;
    else if (INVALID_SLOT)
        rv = CKR_SLOT_ID_INVALID;
    else
    {
        memcpy(pInfo, &st.slots[slotID - 1].slot_info, sizeof(CK_SLOT_INFO));
        log_Log(LOG_LOW, "SlotInfo.flags: %lX", pInfo->flags);
    }

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_GetSlotInfo");

    return rv;
}


/* C_GetTokenInfo obtains information about a particular token
 * in the system. */
CK_DEFINE_FUNCTION(CK_RV, C_GetTokenInfo)
(
  CK_SLOT_ID        slotID,  /* ID of the token's slot */
  CK_TOKEN_INFO_PTR pInfo    /* receives the token information */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_GetTokenInfo");

    thread_MutexLock(st.async_lock);
    log_Log(LOG_LOW, "Checking slot: %ld", slotID);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_DEVICE_REMOVED;
    else if (!pInfo)
        rv = CKR_ARGUMENTS_BAD;
    else if (INVALID_SLOT)
        rv = CKR_SLOT_ID_INVALID;
    else
        memcpy(pInfo, &st.slots[slotID - 1].token_info, sizeof(CK_TOKEN_INFO));

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_GetTokenInfo");

    return rv;
}


/* C_GetMechanismList obtains a list of mechanism types
 * supported by a token. */
CK_DEFINE_FUNCTION(CK_RV, C_GetMechanismList)
(
  CK_SLOT_ID            slotID,          /* ID of token's slot */
  CK_MECHANISM_TYPE_PTR pMechanismList,  /* gets mech. array */
  CK_ULONG_PTR          pulCount         /* gets # of mechs. */
)
{
    CK_RV rv = CKR_OK;
    P11_Slot *slot = &st.slots[slotID - 1];
    P11_MechInfo *mech;
    CK_ULONG i;

    P11_LOG_START("C_GetMechanismList");

    thread_MutexLock(st.async_lock);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_DEVICE_REMOVED;
    else if (INVALID_SLOT)
        rv = CKR_SLOT_ID_INVALID;
    else if (!pMechanismList)
    {
        log_Log(LOG_LOW, "Returning count only");
        *pulCount = slot_MechanismCount(slot->mechanisms);
    }
    else if (*pulCount < slot_MechanismCount(slot->mechanisms))
        rv = CKR_BUFFER_TOO_SMALL;
    else
    {
        mech = slot->mechanisms;

        for (i = 0; mech; i++)
        {
            pMechanismList[i] = mech->type;
            mech = mech->next;
        }

        *pulCount = i;
    }
    
    log_Log(LOG_LOW, "Returning %lu mechanisms", *pulCount);

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_GetMechanismList");

    return rv;
}


/* C_GetMechanismInfo obtains information about a particular
 * mechanism possibly supported by a token. */
CK_DEFINE_FUNCTION(CK_RV, C_GetMechanismInfo)
(
  CK_SLOT_ID            slotID,  /* ID of the token's slot */
  CK_MECHANISM_TYPE     type,    /* type of mechanism */
  CK_MECHANISM_INFO_PTR pInfo    /* receives mechanism info */
)
{
    CK_RV rv = CKR_OK;
    P11_MechInfo *mech;

    P11_LOG_START("C_GetMechanismInfo");

    thread_MutexLock(st.async_lock);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_DEVICE_REMOVED;
    else if (INVALID_SLOT)
        rv = CKR_SLOT_ID_INVALID;
    else if (!pInfo)
        rv = CKR_ARGUMENTS_BAD;
    else
    {
        mech = st.slots[slotID - 1].mechanisms;

        rv = CKR_MECHANISM_INVALID;

        while (mech)
        {
            if (mech->type == type)
            {
                memcpy(pInfo, &mech->info, sizeof(CK_MECHANISM_INFO));
                rv = CKR_OK;
                break;
            }

            mech = mech->next;
        }
    }

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_GetMechanismInfo");

    return rv;
}


/* C_InitToken initializes a token. */
CK_DEFINE_FUNCTION(CK_RV, C_InitToken)
/* pLabel changed from CK_CHAR_PTR to CK_UTF8CHAR_PTR for v2.10 */
(
  CK_SLOT_ID      slotID,    /* ID of the token's slot */
  CK_UTF8CHAR_PTR pPin,      /* the SO's initial PIN */
  CK_ULONG        ulPinLen,  /* length in bytes of the PIN */
  CK_UTF8CHAR_PTR pLabel     /* 32-byte token label (blank padded) */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_InitToken");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");
    /* msc_WriteFramework() */

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_InitToken");

    return rv;
}


/* C_InitPIN initializes the normal user's PIN. */
CK_DEFINE_FUNCTION(CK_RV, C_InitPIN)
(
  CK_SESSION_HANDLE hSession,  /* the session's handle */
  CK_UTF8CHAR_PTR   pPin,      /* the normal user's PIN */
  CK_ULONG          ulPinLen   /* length in bytes of the PIN */
)
{
    CK_RV rv = CKR_OK;
    P11_Slot *slot;
    P11_Session *session;

    P11_LOG_START("C_InitPIN");

    thread_MutexLock(st.async_lock);

//    if (CKR_ERROR(rv = slot_TokenChanged()))
//        rv = CKR_DEVICE_REMOVED;
//    else 
    if (!pPin)
        rv = CKR_ARGUMENTS_BAD;
    else if (INVALID_SESSION)
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (READ_ONLY_SESSION)
        rv = CKR_SESSION_READ_ONLY;
    else
    {
        session = (P11_Session *)hSession;
        slot = &st.slots[session->session.slotID - 1];

        if (CKR_ERROR(rv = slot_BeginTransaction(session->session.slotID)))
            /* Return error */;
        else if (MSC_ERROR(msc_CreatePIN(&slot->conn, 
                                        (MSCUChar8)st.prefs.user_pin_num, 
                                        (MSCUChar8)st.prefs.max_pin_tries, 
                                        pPin, 
                                        ulPinLen, 
                                        (MSCUChar8 *)"PIN_UNBK", 8)))
        {
            (void)CKR_ERROR(slot_EndTransaction(session->session.slotID, MSC_LEAVE_TOKEN));
            rv = CKR_FUNCTION_FAILED;
        }
        else
            rv = slot_EndTransaction(session->session.slotID, MSC_LEAVE_TOKEN);
    }

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_InitPIN");

    return rv;
}


/* C_SetPIN modifies the PIN of the user who is logged in. */
CK_DEFINE_FUNCTION(CK_RV, C_SetPIN)
(
  CK_SESSION_HANDLE hSession,  /* the session's handle */
  CK_UTF8CHAR_PTR   pOldPin,   /* the old PIN */
  CK_ULONG          ulOldLen,  /* length of the old PIN */
  CK_UTF8CHAR_PTR   pNewPin,   /* the new PIN */
  CK_ULONG          ulNewLen   /* length of the new PIN */
)
{
    CK_RV rv = CKR_OK;
    P11_Slot *slot;
    P11_Session *session;

    P11_LOG_START("C_SetPIN");

    thread_MutexLock(st.async_lock);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_DEVICE_REMOVED;
    else if (!pOldPin || !pNewPin)
        rv = CKR_ARGUMENTS_BAD;
    else if (INVALID_SESSION)
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (READ_ONLY_SESSION)
        rv = CKR_SESSION_READ_ONLY;
    else
    {
        session = (P11_Session *)hSession;
        slot = &st.slots[session->session.slotID - 1];

        if (CKR_ERROR(rv = slot_BeginTransaction(session->session.slotID)))
            rv = rv;
        else if (MSC_ERROR(msc_ChangePIN(&slot->conn,
										 (MSCUChar8)st.prefs.user_pin_num,
										 pOldPin,
										 (CK_BYTE)ulOldLen,
										 pNewPin,
										 (CK_BYTE)ulNewLen)))
        {
            (void)CKR_ERROR(slot_EndTransaction(session->session.slotID, MSC_LEAVE_TOKEN));
            rv = CKR_FUNCTION_FAILED;
        }
        else
            rv = slot_EndTransaction(session->session.slotID, MSC_LEAVE_TOKEN);
    }

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_SetPIN");

    return rv;
}

