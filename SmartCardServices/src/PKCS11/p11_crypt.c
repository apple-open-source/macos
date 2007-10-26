/******************************************************************************
** 
**  $Id: p11_crypt.c,v 1.3 2004/10/14 20:33:36 mb Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: Encryption and decryption
** 
******************************************************************************/

#include "cryptoki.h"
#include <openssl/rsa.h>

/* C_EncryptInit initializes an encryption operation. */
CK_DEFINE_FUNCTION(CK_RV, C_EncryptInit)
(
  CK_SESSION_HANDLE hSession,    /* the session's handle */
  CK_MECHANISM_PTR  pMechanism,  /* the encryption mechanism */
  CK_OBJECT_HANDLE  hKey         /* handle of encryption key */
)
{
    CK_RV rv = CKR_OK;
    P11_Session *session = (P11_Session *)hSession;

    P11_LOG_START("C_EncryptInit");

    thread_MutexLock(st.async_lock);

    log_Log(LOG_LOW, "Encrypt mech: %X", *pMechanism);
    log_Log(LOG_LOW, "Encrypt key: %lX", hKey);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (!pMechanism || !hKey)
        rv = CKR_ARGUMENTS_BAD;
    else if (INVALID_SESSION)
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (!USER_MODE)
        rv = CKR_USER_NOT_LOGGED_IN;
    else if (pMechanism->mechanism != CKM_RSA_PKCS)
        rv = CKR_MECHANISM_INVALID;
    else
    {
        /* Fixme: need to verify that the card supports the mechanism and its parameters */
        /* Fixme: should probably add session.encrypt_mech and session.encrypt_key */
        session->sign_mech = *pMechanism;
        session->sign_key = hKey;
    }

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_EncryptInit");

    return rv;
}


/* C_Encrypt encrypts single-part data. */
CK_DEFINE_FUNCTION(CK_RV, C_Encrypt)
(
  CK_SESSION_HANDLE hSession,            /* session's handle */
  CK_BYTE_PTR       pData,               /* the plaintext data */
  CK_ULONG          ulDataLen,           /* bytes of plaintext */
  CK_BYTE_PTR       pEncryptedData,      /* gets ciphertext */
  CK_ULONG_PTR      pulEncryptedDataLen  /* gets c-text size */
)
{
    CK_RV rv = CKR_OK;
    P11_Session *session = (P11_Session *)hSession;
    P11_Object *key = (P11_Object *)session->sign_key;
    MSCCryptInit cryptInit;
    CK_BYTE *t_data1 = 0;
    CK_ULONG t_data1_len;

    P11_LOG_START("C_Encrypt");

    thread_MutexLock(st.async_lock);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (!pEncryptedData || !pData)
        rv = CKR_ARGUMENTS_BAD;
    else if (INVALID_SESSION)
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (!key)
        rv = CKR_OPERATION_NOT_INITIALIZED;
    else if (!USER_MODE)
        rv = CKR_USER_NOT_LOGGED_IN;
    else if ((CK_ULONG)(key->msc_key->keySize / 8) > ulDataLen)
        rv = CKR_ENCRYPTED_DATA_LEN_RANGE;
    else if (CKR_ERROR(rv = slot_BeginTransaction(session->session.slotID)))
        /* Intentionally blank */;
    else if (session->sign_mech.mechanism == CKM_RSA_PKCS)
    {
        /* Fixme: this is not fully implemented since it doesn't look at the mechanism parameter */

        cryptInit.keyNum = key->msc_key->keyNum;
        cryptInit.cipherMode = MSC_MODE_RSA_NOPAD;
        cryptInit.cipherDirection = MSC_DIR_SIGN;
        cryptInit.optParams = 0;
        cryptInit.optParamsSize = 0;

        log_Log(LOG_LOW, "Using key number: %d", cryptInit.keyNum);

        t_data1_len = key->msc_key->keySize / 8;
        t_data1 = (CK_BYTE *)malloc(t_data1_len);

        if (!t_data1)
            rv = CKR_HOST_MEMORY;
        else if (t_data1_len > *pulEncryptedDataLen)
            rv = CKR_BUFFER_TOO_SMALL;
        else if (!RSA_padding_add_PKCS1_type_1(t_data1, t_data1_len, pData, ulDataLen))
            rv = CKR_FUNCTION_FAILED;
        else if (MSC_ERROR(msc_ComputeCrypt(&st.slots[session->session.slotID - 1].conn,
                                            &cryptInit,
                                            t_data1,
                                            t_data1_len,
                                            pEncryptedData,
                                            (MSCPULong32)pulEncryptedDataLen)))
        {
            P11_ERR("MSCComputeCrypt failed");
            rv = CKR_FUNCTION_FAILED;
        }

        if (t_data1)
            free(t_data1);

        (void)CKR_ERROR(slot_EndTransaction(session->session.slotID, MSC_LEAVE_TOKEN));
    }
    else
    {
        (void)CKR_ERROR(slot_EndTransaction(session->session.slotID, MSC_LEAVE_TOKEN));
        rv = CKR_MECHANISM_INVALID;
    }

    session->sign_key = 0;

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_Encrypt");

    return rv;
}


/* C_EncryptUpdate continues a multiple-part encryption
 * operation. */
CK_DEFINE_FUNCTION(CK_RV, C_EncryptUpdate)
(
  CK_SESSION_HANDLE hSession,           /* session's handle */
  CK_BYTE_PTR       pPart,              /* the plaintext data */
  CK_ULONG          ulPartLen,          /* plaintext data len */
  CK_BYTE_PTR       pEncryptedPart,     /* gets ciphertext */
  CK_ULONG_PTR      pulEncryptedPartLen /* gets c-text size */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_EncryptUpdate");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_EncryptUpdate");

    return rv;
}


/* C_EncryptFinal finishes a multiple-part encryption
 * operation. */
CK_DEFINE_FUNCTION(CK_RV, C_EncryptFinal)
(
  CK_SESSION_HANDLE hSession,                /* session handle */
  CK_BYTE_PTR       pLastEncryptedPart,      /* last c-text */
  CK_ULONG_PTR      pulLastEncryptedPartLen  /* gets last size */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_EncryptFinal");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_EncryptFinal");

    return rv;
}


/* C_DecryptInit initializes a decryption operation. */
CK_DEFINE_FUNCTION(CK_RV, C_DecryptInit)
(
  CK_SESSION_HANDLE hSession,    /* the session's handle */
  CK_MECHANISM_PTR  pMechanism,  /* the decryption mechanism */
  CK_OBJECT_HANDLE  hKey         /* handle of decryption key */
)
{
    CK_RV rv = CKR_OK;
    P11_Session *session = (P11_Session *)hSession;

    P11_LOG_START("C_DecryptInit");

    thread_MutexLock(st.async_lock);

    log_Log(LOG_LOW, "Decrypt mech: %X", *pMechanism);
    log_Log(LOG_LOW, "Decrypt key: %lX", hKey);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (!pMechanism || !hKey)
        rv = CKR_ARGUMENTS_BAD;
    else if (INVALID_SESSION)
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (!USER_MODE)
        rv = CKR_USER_NOT_LOGGED_IN;
    else if (pMechanism->mechanism != CKM_RSA_PKCS)
        rv = CKR_MECHANISM_INVALID;
    else
    {
        /* Fixme: need to verify that the card supports the mechanism and its parameters */
        /* Fixme: should probably add session.decrypt_mech and session.decrypt_key */
        session->sign_mech = *pMechanism;
        session->sign_key = hKey;
    }

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_DecryptInit");

    return rv;
}


/* C_Decrypt decrypts encrypted data in a single part. */
CK_DEFINE_FUNCTION(CK_RV, C_Decrypt)
(
  CK_SESSION_HANDLE hSession,           /* session's handle */
  CK_BYTE_PTR       pEncryptedData,     /* ciphertext */
  CK_ULONG          ulEncryptedDataLen, /* ciphertext length */
  CK_BYTE_PTR       pData,              /* gets plaintext */
  CK_ULONG_PTR      pulDataLen          /* gets p-text size */
)
{
    CK_RV rv = CKR_OK;
    P11_Session *session = (P11_Session *)hSession;
    P11_Object *key = (P11_Object *)session->sign_key;
    MSCCryptInit cryptInit;
    CK_BYTE *t_data1 = 0, *t_data2 = 0;
    CK_ULONG t_data1_len, t_data2_len;

    P11_LOG_START("C_Decrypt");

    thread_MutexLock(st.async_lock);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (!pEncryptedData || !pData)
        rv = CKR_ARGUMENTS_BAD;
    else if (INVALID_SESSION)
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (!key)
        rv = CKR_OPERATION_NOT_INITIALIZED;
    else if (!USER_MODE)
        rv = CKR_USER_NOT_LOGGED_IN;
    else if ((CK_ULONG)(key->msc_key->keySize / 8) > ulEncryptedDataLen)
        rv = CKR_ENCRYPTED_DATA_LEN_RANGE;
    else if (CKR_ERROR(rv = slot_BeginTransaction(session->session.slotID)))
        /* Intentionally blank */;
    else if (session->sign_mech.mechanism == CKM_RSA_PKCS)
    {
        MSCULong32  ulValue, lenValue;
        /* Fixme: this is not fully implemented since it doesn't look at the mechanism parameter */
        if (MSC_ERROR(msc_GetCapabilities(&st.slots[session->session.slotID - 1].conn,
                        MSC_TAG_CAPABLE_RSA,
                        (CK_BYTE_PTR) &ulValue,
                        &lenValue)))
                 rv = CKR_FUNCTION_FAILED;
        else if (ulValue & MSC_CAPABLE_RSA_NOPAD)
        {
            log_Log(LOG_LOW, "Decrypt: RSA NOPAD; key %i; keysize %i",
                    key->msc_key->keyNum, key->msc_key->keySize);

        cryptInit.keyNum = key->msc_key->keyNum;
        cryptInit.cipherMode = MSC_MODE_RSA_NOPAD;
            cryptInit.cipherDirection = MSC_DIR_DECRYPT;
        cryptInit.optParams = 0;
        cryptInit.optParamsSize = 0;

        t_data1 = (CK_BYTE *)malloc(key->msc_key->keySize / 8);
        t_data2 = (CK_BYTE *)malloc(key->msc_key->keySize / 8);
        t_data2_len = key->msc_key->keySize / 8;   /* FIX - bugzilla 1701 */

        if (!t_data1 || !t_data2)
            rv = CKR_HOST_MEMORY;
            else if (MSC_ERROR(msc_ComputeCrypt(
                            &st.slots[session->session.slotID - 1].conn,
                                       &cryptInit,
                                       pEncryptedData,
                                       ulEncryptedDataLen,
                                       t_data1,
                                       (MSCPULong32)&t_data1_len)))
        {
            P11_ERR("MSCComputeCrypt failed");
            rv = CKR_FUNCTION_FAILED;
        }
            else if (CKR_ERROR(rv = util_StripPKCS1(t_data1, t_data1_len, 
                            t_data2, &t_data2_len)))
            /* Intentionally blank */;
        else if (t_data2_len > *pulDataLen)
            rv = CKR_BUFFER_TOO_SMALL;
        else
        {
            memcpy(pData, t_data2, t_data2_len);
            *pulDataLen = t_data2_len;
        }
        }
        else if (ulValue & MSC_CAPABLE_RSA_PKCS1)
        {
            log_Log(LOG_LOW, "Decrypt: RSA PKCS1; key %i; keysize %i",
                    key->msc_key->keyNum, key->msc_key->keySize);
            
            cryptInit.keyNum = key->msc_key->keyNum;
            cryptInit.cipherDirection = MSC_DIR_DECRYPT;
            cryptInit.cipherMode = MSC_MODE_RSA_PAD_PKCS1;
            cryptInit.optParams = 0;
            cryptInit.optParamsSize = 0;

            t_data1 = (CK_BYTE *)malloc(key->msc_key->keySize / 8);

            if (!t_data1)
                rv = CKR_HOST_MEMORY;
            else if (MSC_ERROR(msc_ComputeCrypt(
                            &st.slots[session->session.slotID - 1].conn,
                            &cryptInit,
                            pEncryptedData,
                            ulEncryptedDataLen,
                            t_data1,
                            (MSCPULong32)&t_data1_len)))
            {
                P11_ERR("MSCComputeCrypt failed");
                rv = CKR_FUNCTION_FAILED;
            }
            if (t_data1_len > *pulDataLen)
                rv = CKR_BUFFER_TOO_SMALL;
            else
            {
                memcpy(pData, t_data1, t_data1_len);
                *pulDataLen = t_data1_len;
            }
        }
        else
            rv = CKR_MECHANISM_PARAM_INVALID;

        if (t_data1)
            free(t_data1);

        if (t_data2)
            free(t_data2);

        (void)CKR_ERROR(slot_EndTransaction(session->session.slotID, MSC_LEAVE_TOKEN));
    }
    else
    {
        (void)CKR_ERROR(slot_EndTransaction(session->session.slotID, MSC_LEAVE_TOKEN));
        rv = CKR_MECHANISM_INVALID;
    }

    session->sign_key = 0;

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_Decrypt");

    return rv;
}


/* C_DecryptUpdate continues a multiple-part decryption
 * operation. */
CK_DEFINE_FUNCTION(CK_RV, C_DecryptUpdate)
(
  CK_SESSION_HANDLE hSession,            /* session's handle */
  CK_BYTE_PTR       pEncryptedPart,      /* encrypted data */
  CK_ULONG          ulEncryptedPartLen,  /* input length */
  CK_BYTE_PTR       pPart,               /* gets plaintext */
  CK_ULONG_PTR      pulPartLen           /* p-text size */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_DecryptUpdate");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_DecryptUpdate");

    return rv;
}


/* C_DecryptFinal finishes a multiple-part decryption
 * operation. */
CK_DEFINE_FUNCTION(CK_RV, C_DecryptFinal)
(
  CK_SESSION_HANDLE hSession,       /* the session's handle */
  CK_BYTE_PTR       pLastPart,      /* gets plaintext */
  CK_ULONG_PTR      pulLastPartLen  /* p-text size */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_DecryptFinal");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_DecryptFinal");

    return rv;
}

