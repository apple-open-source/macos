/******************************************************************************
** 
**  $Id: p11_digest.c,v 1.2 2003/02/13 20:06:37 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: Message digesting
** 
******************************************************************************/

#include "cryptoki.h"

/* C_DigestInit initializes a message-digesting operation. */
CK_DEFINE_FUNCTION(CK_RV, C_DigestInit)
(
  CK_SESSION_HANDLE hSession,   /* the session's handle */
  CK_MECHANISM_PTR  pMechanism  /* the digesting mechanism */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_DigestInit");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_DigestInit");

    return rv;
}


/* C_Digest digests data in a single part. */
CK_DEFINE_FUNCTION(CK_RV, C_Digest)
(
  CK_SESSION_HANDLE hSession,     /* the session's handle */
  CK_BYTE_PTR       pData,        /* data to be digested */
  CK_ULONG          ulDataLen,    /* bytes of data to digest */
  CK_BYTE_PTR       pDigest,      /* gets the message digest */
  CK_ULONG_PTR      pulDigestLen  /* gets digest length */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_Digest");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_Digest");

    return rv;
}


/* C_DigestUpdate continues a multiple-part message-digesting
 * operation. */
CK_DEFINE_FUNCTION(CK_RV, C_DigestUpdate)
(
  CK_SESSION_HANDLE hSession,  /* the session's handle */
  CK_BYTE_PTR       pPart,     /* data to be digested */
  CK_ULONG          ulPartLen  /* bytes of data to be digested */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_DigestUpdate");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_DigestUpdate");

    return rv;
}


/* C_DigestKey continues a multi-part message-digesting
 * operation, by digesting the value of a secret key as part of
 * the data already digested. */
CK_DEFINE_FUNCTION(CK_RV, C_DigestKey)
(
  CK_SESSION_HANDLE hSession,  /* the session's handle */
  CK_OBJECT_HANDLE  hKey       /* secret key to digest */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_DigestKey");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_DigestKey");

    return rv;
}


/* C_DigestFinal finishes a multiple-part message-digesting
 * operation. */
CK_DEFINE_FUNCTION(CK_RV, C_DigestFinal)
(
  CK_SESSION_HANDLE hSession,     /* the session's handle */
  CK_BYTE_PTR       pDigest,      /* gets the message digest */
  CK_ULONG_PTR      pulDigestLen  /* gets byte count of digest */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_DigestFinal");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_DigestFinal");

    return rv;
}

