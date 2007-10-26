/******************************************************************************
** 
**  $Id: p11_sign.c,v 1.2 2003/02/13 20:06:39 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: Signing and MACing
** 
******************************************************************************/

#include "cryptoki.h"
#include <openssl/rsa.h>

/* C_SignInit initializes a signature (private key encryption)
 * operation, where the signature is (will be) an appendix to
 * the data, and plaintext cannot be recovered from the
 *signature. */
CK_DEFINE_FUNCTION(CK_RV, C_SignInit)
(
  CK_SESSION_HANDLE hSession,    /* the session's handle */
  CK_MECHANISM_PTR  pMechanism,  /* the signature mechanism */
  CK_OBJECT_HANDLE  hKey         /* handle of signature key */
)
{
    CK_RV rv = CKR_OK;
    CK_OBJECT_HANDLE hObject = hKey; /* Needed for INVALID_OBJECT check */
    P11_Session *session = (P11_Session *)hSession;

    P11_LOG_START("C_SignInit");

    thread_MutexLock(st.async_lock);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (!pMechanism)
        rv = CKR_ARGUMENTS_BAD;
    else if (INVALID_SESSION)
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (INVALID_OBJECT)
        rv = CKR_OBJECT_HANDLE_INVALID;
    else if (!USER_MODE)
        rv = CKR_USER_NOT_LOGGED_IN;
    else
    {
        session->sign_mech = *pMechanism;
        session->sign_key = hObject;

        log_Log(LOG_LOW, "Sign object handle: 0x%lX", hObject);
    }

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_SignInit");

    return rv;
}


/* C_Sign signs (encrypts with private key) data in a single
 * part, where the signature is (will be) an appendix to the
 * data, and plaintext cannot be recovered from the signature. */
CK_DEFINE_FUNCTION(CK_RV, C_Sign)
(
  CK_SESSION_HANDLE hSession,        /* the session's handle */
  CK_BYTE_PTR       pData,           /* the data to sign */
  CK_ULONG          ulDataLen,       /* count of bytes to sign */
  CK_BYTE_PTR       pSignature,      /* gets the signature */
  CK_ULONG_PTR      pulSignatureLen  /* gets signature length */
)
{
    CK_RV rv = CKR_OK;
    MSCCryptInit cryptInit;
    P11_Session *session = (P11_Session *)hSession;
    P11_Object *key = (P11_Object *)session->sign_key;
    CK_BYTE *to = 0;
    CK_ULONG tlen;

    P11_LOG_START("C_Sign");

    thread_MutexLock(st.async_lock);

    log_Log(LOG_LOW, "Output buffer len: %lu", *pulSignatureLen);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (!pData || !pSignature)
        rv = CKR_ARGUMENTS_BAD;
    else if (INVALID_SESSION)
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (!key)
        rv = CKR_OPERATION_NOT_INITIALIZED;
    else if (!USER_MODE)
        rv = CKR_USER_NOT_LOGGED_IN;
    else if ((CK_ULONG)(key->msc_key->keySize / 8) > *pulSignatureLen)
    {
        *pulSignatureLen = key->msc_key->keySize / 8;
        rv = CKR_BUFFER_TOO_SMALL;
    }
    else if (CKR_ERROR(rv = slot_BeginTransaction(session->session.slotID)))
        /* Intentionally blank */;
    else if (session->sign_mech.mechanism == CKM_RSA_PKCS)
    {
        MSCULong32  ulValue, lenValue;

        if (MSC_ERROR(msc_GetCapabilities(&st.slots[session->session.slotID - 1].conn,
                        MSC_TAG_CAPABLE_RSA,
                        (CK_BYTE_PTR) &ulValue,
                        &lenValue)))
            rv = CKR_FUNCTION_FAILED;
        else if (ulValue & MSC_CAPABLE_RSA_NOPAD)
        {
        cryptInit.keyNum = key->msc_key->keyNum;
        cryptInit.cipherMode = MSC_MODE_RSA_NOPAD;
        cryptInit.cipherDirection = MSC_DIR_SIGN;
        cryptInit.optParams = 0;
        cryptInit.optParamsSize = 0;

        tlen = key->msc_key->keySize / 8;
        to = (CK_BYTE *)malloc(tlen);

            log_Log(LOG_LOW, "Pad and Sign object keyNum: %lu tlen: %lu", 
                    key->msc_key->keyNum, tlen);

        if (!to)
            rv = CKR_HOST_MEMORY;
        else if (!RSA_padding_add_PKCS1_type_1(to, tlen, pData, ulDataLen))
            rv = CKR_FUNCTION_FAILED;

            else
			{
				MSCULong32 outputDataSize = *pulSignatureLen;
				if (MSC_ERROR(msc_ComputeCrypt(
                            &st.slots[session->session.slotID - 1].conn,
                                           &cryptInit,
                                           to,
                                           tlen,
                                           pSignature,
                                           &outputDataSize)))
					rv = CKR_FUNCTION_FAILED;
				*pulSignatureLen = outputDataSize;
			}
        }
        else if (ulValue & MSC_CAPABLE_RSA_PKCS1)
        {
            cryptInit.keyNum = key->msc_key->keyNum;
            cryptInit.cipherMode = MSC_MODE_RSA_PAD_PKCS1;
            cryptInit.cipherDirection = MSC_DIR_SIGN;
            cryptInit.optParams = 0;
            cryptInit.optParamsSize = 0;

            log_Log(LOG_LOW, "Sign object keyNum: %lu DataLen: %lu", 
                    key->msc_key->keyNum, ulDataLen);

			MSCULong32 outputDataSize = *pulSignatureLen;
			if (MSC_ERROR(msc_ComputeCrypt(
                            &st.slots[session->session.slotID - 1].conn,
                            &cryptInit,
                            pData,
                            ulDataLen,
                            pSignature,
                            &outputDataSize)))
                rv = CKR_FUNCTION_FAILED;
			*pulSignatureLen = outputDataSize;
        }
        else
            rv = CKR_MECHANISM_PARAM_INVALID;

        (void)CKR_ERROR(slot_EndTransaction(session->session.slotID, MSC_LEAVE_TOKEN));
    }
    else
    {
        (void)CKR_ERROR(slot_EndTransaction(session->session.slotID, MSC_LEAVE_TOKEN));
        rv = CKR_MECHANISM_INVALID;
    }

    session->sign_key = 0;

    if (to)
        free(to);

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_Sign");

    return rv;
}


/* C_SignUpdate continues a multiple-part signature operation,
 * where the signature is (will be) an appendix to the data, 
 * and plaintext cannot be recovered from the signature. */
CK_DEFINE_FUNCTION(CK_RV, C_SignUpdate)
(
  CK_SESSION_HANDLE hSession,  /* the session's handle */
  CK_BYTE_PTR       pPart,     /* the data to sign */
  CK_ULONG          ulPartLen  /* count of bytes to sign */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_SignUpdate");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_SignUpdate");

    return rv;
}


/* C_SignFinal finishes a multiple-part signature operation, 
 * returning the signature. */
CK_DEFINE_FUNCTION(CK_RV, C_SignFinal)
(
  CK_SESSION_HANDLE hSession,        /* the session's handle */
  CK_BYTE_PTR       pSignature,      /* gets the signature */
  CK_ULONG_PTR      pulSignatureLen  /* gets signature length */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_SignFinal");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_SignFinal");

    return rv;
}


/* C_SignRecoverInit initializes a signature operation, where
 * the data can be recovered from the signature. */
CK_DEFINE_FUNCTION(CK_RV, C_SignRecoverInit)
(
  CK_SESSION_HANDLE hSession,   /* the session's handle */
  CK_MECHANISM_PTR  pMechanism, /* the signature mechanism */
  CK_OBJECT_HANDLE  hKey        /* handle of the signature key */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_SignRecoverInit");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_SignRecoverInit");

    return rv;
}


/* C_SignRecover signs data in a single operation, where the
 * data can be recovered from the signature. */
CK_DEFINE_FUNCTION(CK_RV, C_SignRecover)
(
  CK_SESSION_HANDLE hSession,        /* the session's handle */
  CK_BYTE_PTR       pData,           /* the data to sign */
  CK_ULONG          ulDataLen,       /* count of bytes to sign */
  CK_BYTE_PTR       pSignature,      /* gets the signature */
  CK_ULONG_PTR      pulSignatureLen  /* gets signature length */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_SignRecover");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_SignRecover");

    return rv;
}

