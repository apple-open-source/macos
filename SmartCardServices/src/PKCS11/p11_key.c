/******************************************************************************
** 
**  $Id: p11_key.c,v 1.2 2003/02/13 20:06:38 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: Key management
** 
******************************************************************************/

#include "cryptoki.h"

/* C_GenerateKey generates a secret key, creating a new key
 * object. */
CK_DEFINE_FUNCTION(CK_RV, C_GenerateKey)
(
  CK_SESSION_HANDLE    hSession,    /* the session's handle */
  CK_MECHANISM_PTR     pMechanism,  /* key generation mech. */
  CK_ATTRIBUTE_PTR     pTemplate,   /* template for new key */
  CK_ULONG             ulCount,     /* # of attrs in template */
  CK_OBJECT_HANDLE_PTR phKey        /* gets handle of new key */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_GenerateKey");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_GenerateKey");

    return rv;
}


/* C_GenerateKeyPair generates a public-key/private-key pair, 
 * creating new key objects. */
CK_DEFINE_FUNCTION(CK_RV, C_GenerateKeyPair)
(
  CK_SESSION_HANDLE    hSession,                    /* session
                                                     * handle */
  CK_MECHANISM_PTR     pMechanism,                  /* key-gen
                                                     * mech. */
  CK_ATTRIBUTE_PTR     pPublicKeyTemplate,          /* template
                                                     * for pub.
                                                     * key */
  CK_ULONG             ulPublicKeyAttributeCount,   /* # pub.
                                                     * attrs. */
  CK_ATTRIBUTE_PTR     pPrivateKeyTemplate,         /* template
                                                     * for priv.
                                                     * key */
  CK_ULONG             ulPrivateKeyAttributeCount,  /* # priv.
                                                     * attrs. */
  CK_OBJECT_HANDLE_PTR phPublicKey,                 /* gets pub.
                                                     * key
                                                     * handle */
  CK_OBJECT_HANDLE_PTR phPrivateKey                 /* gets
                                                     * priv. key
                                                     * handle */
)
{
    CK_RV rv = CKR_OK;
    P11_Session *session = (P11_Session *)hSession;
    CK_ULONG i;
    CK_ATTRIBUTE *attrib;

    P11_LOG_START("C_GenerateKeyPair");

    thread_MutexLock(st.async_lock);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (!pMechanism || !pPublicKeyTemplate ||
             !pPrivateKeyTemplate || !phPublicKey || !phPrivateKey)
        rv = CKR_ARGUMENTS_BAD;
    else if (INVALID_SESSION)
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (READ_ONLY_SESSION)
        rv = CKR_SESSION_READ_ONLY;
    else if (CKR_ERROR(rv = slot_BeginTransaction(session->session.slotID)))
        /* Intentionally blank */;
    else
    {
        log_Log(LOG_LOW, "Mech type: %lX", pMechanism->mechanism);
        log_Log(LOG_LOW, "Param Len: %lu", pMechanism->ulParameterLen);
        log_Log(LOG_LOW, "Pub attrib count: %lu", ulPublicKeyAttributeCount);
        for (i = 0; i < ulPublicKeyAttributeCount; i++)
            log_Log(LOG_LOW, "Attrib type: %lX", pPublicKeyTemplate[i].type);
        log_Log(LOG_LOW, "Priv attrib count: %lu", ulPrivateKeyAttributeCount);
        for (i = 0; i < ulPrivateKeyAttributeCount; i++)
            log_Log(LOG_LOW, "Attrib type: %lX", pPrivateKeyTemplate[i].type);

        /* Fixme: Only supports generating keys on card */
        if (CKR_ERROR(object_TemplateGetAttrib(CKA_TOKEN, pPublicKeyTemplate, ulPublicKeyAttributeCount, &attrib)) ||
            CKR_ERROR(object_TemplateGetAttrib(CKA_TOKEN, pPrivateKeyTemplate, ulPrivateKeyAttributeCount, &attrib)))
            rv = CKR_ATTRIBUTE_VALUE_INVALID;
        else if (pMechanism->mechanism == CKM_RSA_PKCS_KEY_PAIR_GEN)
            (void)CKR_ERROR(rv = object_RSAGenKeyPair(hSession, pPublicKeyTemplate, 
                                                ulPublicKeyAttributeCount, 
                                                pPrivateKeyTemplate, 
                                                ulPrivateKeyAttributeCount,
                                                phPublicKey,
                                                phPrivateKey));
        else
            rv = CKR_MECHANISM_INVALID;

        (void)CKR_ERROR(slot_EndTransaction(session->session.slotID, MSC_LEAVE_TOKEN));
    }

    log_Log(LOG_LOW, "Returning handles for public key: %lX and private key: %lX", *phPublicKey, *phPrivateKey);

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_GenerateKeyPair");

    return rv;
}


/* C_WrapKey wraps (i.e., encrypts) a key. */
CK_DEFINE_FUNCTION(CK_RV, C_WrapKey)
(
  CK_SESSION_HANDLE hSession,        /* the session's handle */
  CK_MECHANISM_PTR  pMechanism,      /* the wrapping mechanism */
  CK_OBJECT_HANDLE  hWrappingKey,    /* wrapping key */
  CK_OBJECT_HANDLE  hKey,            /* key to be wrapped */
  CK_BYTE_PTR       pWrappedKey,     /* gets wrapped key */
  CK_ULONG_PTR      pulWrappedKeyLen /* gets wrapped key size */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_WrapKey");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_WrapKey");

    return rv;
}


/* C_UnwrapKey unwraps (decrypts) a wrapped key, creating a new
 * key object. */
CK_DEFINE_FUNCTION(CK_RV, C_UnwrapKey)
(
  CK_SESSION_HANDLE    hSession,          /* session's handle */
  CK_MECHANISM_PTR     pMechanism,        /* unwrapping mech. */
  CK_OBJECT_HANDLE     hUnwrappingKey,    /* unwrapping key */
  CK_BYTE_PTR          pWrappedKey,       /* the wrapped key */
  CK_ULONG             ulWrappedKeyLen,   /* wrapped key len */
  CK_ATTRIBUTE_PTR     pTemplate,         /* new key template */
  CK_ULONG             ulAttributeCount,  /* template length */
  CK_OBJECT_HANDLE_PTR phKey              /* gets new handle */
)
{
    CK_RV rv = CKR_OK;
    P11_Object *object;
    P11_Attrib *attrib;
    MSCCryptInit cryptInit;
    P11_Session *session = (P11_Session *)hSession;
    P11_Object *key = (P11_Object *)hUnwrappingKey;
    CK_BYTE *output = 0, *final_output = 0;
    MSCULong32 output_len;
	CK_ULONG final_output_len;
    CK_ULONG i;

    P11_LOG_START("C_UnwrapKey");

    thread_MutexLock(st.async_lock);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (!pMechanism ||
             !hUnwrappingKey || !pWrappedKey || !pTemplate)
        rv = CKR_ARGUMENTS_BAD;
    else if (INVALID_SESSION)
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (READ_ONLY_SESSION)
        rv = CKR_SESSION_READ_ONLY;
    else if (pMechanism->mechanism == CKM_RSA_PKCS)
    {
        log_Log(LOG_LOW, "UnwrapKey:  %X", hUnwrappingKey);
        log_Log(LOG_LOW, "WrappedLen:  %lu", ulWrappedKeyLen);

        if (CKR_ERROR(object_AddObject(session->session.slotID, (CK_OBJECT_HANDLE *)&object)))
            rv = CKR_FUNCTION_FAILED;
        else
        {
            for (i = 0; i < ulAttributeCount; i++)
                if (CKR_ERROR(rv = object_AddAttribute(object, 
                                                       pTemplate[i].type, 
                                                       FALSE,
                                                       (CK_BYTE *)pTemplate[i].pValue, 
                                                       pTemplate[i].ulValueLen, 
                                                       &attrib)))
                    break;
    
            if (!CKR_ERROR(rv))
            {
                MSCULong32  ulValue, lenValue;
                if (MSC_ERROR(msc_GetCapabilities(
                                &st.slots[session->session.slotID - 1].conn,
                                MSC_TAG_CAPABLE_RSA,
                                (CK_BYTE_PTR) &ulValue,
                                &lenValue)))
                        rv = CKR_FUNCTION_FAILED;
                else if (ulValue & MSC_CAPABLE_RSA_NOPAD)
                {
                log_Log(LOG_LOW, "UnwrapKey num: %lu", key->msc_key->keyNum);
                cryptInit.keyNum = key->msc_key->keyNum;
                cryptInit.cipherMode = MSC_MODE_RSA_NOPAD;
                    cryptInit.cipherDirection = MSC_DIR_DECRYPT;
                cryptInit.optParams = 0;
                cryptInit.optParamsSize = 0;
        
                output_len = key->msc_key->keySize / 8;
                output = (CK_BYTE *)malloc(output_len);
                final_output_len = output_len;
                final_output = (CK_BYTE *)malloc(final_output_len);
        
                if (!output || !final_output)
                    rv = CKR_HOST_MEMORY;
                else if (CKR_ERROR(rv = slot_BeginTransaction(session->session.slotID)))
                    /* Intentionally blank */;
                    else if (MSC_ERROR(msc_ComputeCrypt(
                                    &st.slots[session->session.slotID - 1].conn,
                                                   &cryptInit,
                                                   pWrappedKey,
                                                   ulWrappedKeyLen,
                                                   output,
                                                   &output_len)))
                {
                    P11_ERR("MSCComputeCrypt failed");
                    rv = CKR_FUNCTION_FAILED;
                }
                else
                {
                    (void)CKR_ERROR(slot_EndTransaction(session->session.slotID, MSC_LEAVE_TOKEN));

                        if (!CKR_ERROR(rv = util_StripPKCS1(output, output_len, 
                                        final_output, &final_output_len)))
                        {
                            rv = object_AddAttribute(object, CKA_VALUE, FALSE, 
                                    final_output, final_output_len, &attrib);
                            *phKey = (CK_OBJECT_HANDLE)object;
                        }
                    }
                }
                else if (ulValue & MSC_CAPABLE_RSA_PKCS1)
                {
                    log_Log(LOG_LOW, "UnwrapKey num: %lu", key->msc_key->keyNum);
                    cryptInit.keyNum = key->msc_key->keyNum;
                    cryptInit.cipherDirection = MSC_DIR_DECRYPT;
                    cryptInit.cipherMode = MSC_MODE_RSA_PAD_PKCS1;
                    cryptInit.optParams = 0;
                    cryptInit.optParamsSize = 0;
                    
                    output_len = key->msc_key->keySize / 8;
                    output = (CK_BYTE *)malloc(output_len);
        
                    if (!output)
                        rv = CKR_HOST_MEMORY;
                    else if (CKR_ERROR(rv = slot_BeginTransaction(session->session.slotID)))
                        /* Intentionally blank */;
                    else if (MSC_ERROR(msc_ComputeCrypt(
                                    &st.slots[session->session.slotID - 1].conn,
                                    &cryptInit,
                                    pWrappedKey,
                                    ulWrappedKeyLen,
                                    output,
                                    &output_len)))
                    {
                        P11_ERR("MSCComputeCrypt failed");
                        rv = CKR_FUNCTION_FAILED;
                    }
                    else
                    {
                        (void)CKR_ERROR(slot_EndTransaction(session->session.slotID, MSC_LEAVE_TOKEN));

                        rv = object_AddAttribute(object, CKA_VALUE, FALSE,
                                output, output_len, &attrib);
                        *phKey = (CK_OBJECT_HANDLE)object;
                    }
                }
                else
                    rv = CKR_MECHANISM_PARAM_INVALID;
        
                if (output)
                    free(output);

                if (final_output)
                    free(final_output);
            }
        }
    }
    else
    {
        log_Log(LOG_LOW, "Invalid mechanism specified: 0x%lX", pMechanism->mechanism);
        rv = CKR_MECHANISM_INVALID;
    }

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_UnwrapKey");

    return rv;
}


/* C_DeriveKey derives a key from a base key, creating a new key
 * object. */
CK_DEFINE_FUNCTION(CK_RV, C_DeriveKey)
(
  CK_SESSION_HANDLE    hSession,          /* session's handle */
  CK_MECHANISM_PTR     pMechanism,        /* key deriv. mech. */
  CK_OBJECT_HANDLE     hBaseKey,          /* base key */
  CK_ATTRIBUTE_PTR     pTemplate,         /* new key template */
  CK_ULONG             ulAttributeCount,  /* template length */
  CK_OBJECT_HANDLE_PTR phKey              /* gets new handle */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_DeriveKey");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_DeriveKey");

    return rv;
}

