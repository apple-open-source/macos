/******************************************************************************
** 
**  $Id: p11_session.c,v 1.2 2003/02/13 20:06:39 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: Session management
** 
******************************************************************************/

#include "cryptoki.h"

/* C_OpenSession opens a session between an application and a
 * token. */
CK_DEFINE_FUNCTION(CK_RV, C_OpenSession)
(
  CK_SLOT_ID            slotID,        /* the slot's ID */
  CK_FLAGS              flags,         /* from CK_SESSION_INFO */
  CK_VOID_PTR           pApplication,  /* passed to callback */
  CK_NOTIFY             Notify,        /* callback function */
  CK_SESSION_HANDLE_PTR phSession      /* gets session handle */
)
{
    CK_RV rv = CKR_OK;
    P11_Session *session;
    CK_ULONG pin_state;

    P11_LOG_START("C_OpenSession");

    thread_MutexLock(st.async_lock);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_DEVICE_REMOVED;
    else if (INVALID_SLOT)
        rv = CKR_SLOT_ID_INVALID;
    else if (!phSession)
        rv = CKR_ARGUMENTS_BAD;
    else
    {
        if (!(flags & CKF_RW_SESSION) && slot_CheckRWSOsession(slotID))
            rv = CKR_SESSION_READ_WRITE_SO_EXISTS;
        else if (CKR_ERROR(rv = slot_EstablishConnection(slotID)))
            /* Return error */;
        else if (CKR_ERROR(rv = session_AddSession(phSession)))
            /* Return error */;
        else
        {
            log_Log(LOG_LOW, "New session handle: %X", *phSession);
            session = (P11_Session *)*phSession;
            session->session.slotID = slotID;

            pin_state = st.slots[slotID - 1].pin_state;

            if (flags & CKF_RW_SESSION)
            {
                if (pin_state == 1)
                    session->session.state = CKS_RW_USER_FUNCTIONS;
                else if (pin_state == 2)
                    session->session.state = CKS_RW_SO_FUNCTIONS;
                else
                    session->session.state = CKS_RW_PUBLIC_SESSION;
            }
            else
            {
                if (pin_state == 1)
                    session->session.state = CKS_RO_USER_FUNCTIONS;
                else if (pin_state == 2)
                    session->session.state = CKS_RO_USER_FUNCTIONS;
                else
                    session->session.state = CKS_RO_PUBLIC_SESSION;
            }

            session->session.flags = flags;
            session->session.ulDeviceError = 0x1F; /* Fixme: what is this used for? */
            session->application = pApplication;
            session->notify = Notify;
        }
    }

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_OpenSession");

    return rv;
}


/* C_CloseSession closes a session between an application and a
 * token. */
CK_DEFINE_FUNCTION(CK_RV, C_CloseSession)
(
  CK_SESSION_HANDLE hSession  /* the session's handle */
)
{
    CK_RV rv = CKR_OK;
    P11_Session *session = (P11_Session *)hSession;
    CK_SLOT_ID slotID = session->session.slotID;

    P11_LOG_START("C_CloseSession");

    thread_MutexLock(st.async_lock);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_DEVICE_REMOVED;
    else if (INVALID_SESSION)
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (!CKR_ERROR(rv = session_FreeSession(hSession)))
        rv = slot_ReleaseConnection(slotID);

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_CloseSession");

    return rv;
}


/* C_CloseAllSessions closes all sessions with a token. */
CK_DEFINE_FUNCTION(CK_RV, C_CloseAllSessions)
(
  CK_SLOT_ID     slotID  /* the token's slot */
)
{
    CK_RV rv = CKR_OK;
    P11_Session *session_l;
    P11_Session *session_l_temp;

    P11_LOG_START("C_CloseAllSessions");

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_DEVICE_REMOVED;
    else
    {
        session_l = st.sessions;

        while (session_l)
        {
            session_l_temp = session_l->next;

            if (session_l->session.slotID == slotID)
                C_CloseSession((CK_SESSION_HANDLE)session_l); /* Fixme: ignore errors? */

            session_l = session_l_temp;
        }
    }

    P11_LOG_END("C_CloseAllSessions");

    return rv;
}


/* C_GetSessionInfo obtains information about the session. */
CK_DEFINE_FUNCTION(CK_RV, C_GetSessionInfo)
(
  CK_SESSION_HANDLE   hSession,  /* the session's handle */
  CK_SESSION_INFO_PTR pInfo      /* receives session info */
)
{
    CK_RV rv = CKR_OK;
    P11_Session *session = (P11_Session *)hSession;

    P11_LOG_START("C_GetSessionInfo");

    thread_MutexLock(st.async_lock);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_DEVICE_REMOVED;
    else if (!pInfo)
        rv = CKR_ARGUMENTS_BAD;
    else if (INVALID_SESSION)
        rv = CKR_SESSION_HANDLE_INVALID;
    else
    {
        log_Log(LOG_LOW, "Session state: %lu", session->session.state);
        memcpy(pInfo, &session->session, sizeof(CK_SESSION_INFO));
    }

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_GetSessionInfo");

    return rv;
}


/* C_GetOperationState obtains the state of the cryptographic operation
 * in a session. */
CK_DEFINE_FUNCTION(CK_RV, C_GetOperationState)
(
  CK_SESSION_HANDLE hSession,             /* session's handle */
  CK_BYTE_PTR       pOperationState,      /* gets state */
  CK_ULONG_PTR      pulOperationStateLen  /* gets state length */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_GetOperationState");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_GetOperationState");

    return rv;
}


/* C_SetOperationState restores the state of the cryptographic
 * operation in a session. */
CK_DEFINE_FUNCTION(CK_RV, C_SetOperationState)
(
  CK_SESSION_HANDLE hSession,            /* session's handle */
  CK_BYTE_PTR      pOperationState,      /* holds state */
  CK_ULONG         ulOperationStateLen,  /* holds state length */
  CK_OBJECT_HANDLE hEncryptionKey,       /* en/decryption key */
  CK_OBJECT_HANDLE hAuthenticationKey    /* sign/verify key */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_SetOperationState");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_SetOperationState");

    return rv;
}


/* C_Login logs a user into a token. */
CK_DEFINE_FUNCTION(CK_RV, C_Login)
(
  CK_SESSION_HANDLE hSession,  /* the session's handle */
  CK_USER_TYPE      userType,  /* the user type */
  CK_UTF8CHAR_PTR   pPin,      /* the user's PIN */
  CK_ULONG          ulPinLen   /* the length of the PIN */
)
{
    CK_RV rv = CKR_OK;
    P11_Session *session = (P11_Session *)hSession;

    P11_LOG_START("C_Login");

    thread_MutexLock(st.async_lock);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_DEVICE_REMOVED;
    else if (!pPin || !ulPinLen)
        rv = CKR_ARGUMENTS_BAD;
    else if (INVALID_SESSION)
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (!CKR_ERROR(rv = slot_BeginTransaction(session->session.slotID)))
    {
        if (userType == CKU_SO)
            log_Log(LOG_LOW, "Verifying SO PIN");
        else if (userType == CKU_USER)
            log_Log(LOG_LOW, "Verifying USER PIN");
        else
            log_Log(LOG_LOW, "Verifying Unknown user PIN: %lu", userType);

        rv = slot_VerifyPIN(session->session.slotID, userType, pPin, ulPinLen);
        (void)CKR_ERROR(slot_EndTransaction(session->session.slotID, MSC_LEAVE_TOKEN));

        if (rv == CKR_OK)
            slot_UserMode(session->session.slotID);
    }

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_Login");

    return rv;
}


/* C_Logout logs a user out from a token. */
CK_DEFINE_FUNCTION(CK_RV, C_Logout)
(
  CK_SESSION_HANDLE hSession  /* the session's handle */
)
{
    CK_RV rv = CKR_OK;
    P11_Session *session = (P11_Session *)hSession;

    P11_LOG_START("C_Logout");

    thread_MutexLock(st.async_lock);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_DEVICE_REMOVED;
    else if (INVALID_SESSION)
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (!CKR_ERROR(rv = slot_BeginTransaction(session->session.slotID)))
    {
        if (rv == CKR_OK)
            slot_PublicMode(session->session.slotID);

        memset(st.slots[session->session.slotID - 1].pins, 0x00, sizeof(st.slots[session->session.slotID - 1].pins));

        if (MSC_ERROR(msc_LogoutAll(&st.slots[session->session.slotID - 1].conn)))
            (void)CKR_ERROR(rv = slot_EndTransaction(session->session.slotID, MSC_RESET_TOKEN));
        else
            (void)CKR_ERROR(rv = slot_EndTransaction(session->session.slotID, MSC_LEAVE_TOKEN));
    }

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_Logout");

    return rv;
}

