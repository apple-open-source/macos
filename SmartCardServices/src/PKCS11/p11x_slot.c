/******************************************************************************
** 
**  $Id: p11x_slot.c,v 1.2 2003/02/13 20:06:41 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: Slot management functions
** 
******************************************************************************/

#include "cryptoki.h"

/******************************************************************************
** Function: slot_BeginTransaction
**
** Description
**
** Parameters:
**  none
**
** Returns:
**  none
*******************************************************************************/
CK_RV slot_BeginTransaction(CK_ULONG slotID)
{
    CK_RV rv = CKR_OK;
    MSC_RV msc_rv;

    log_Log(LOG_LOW, "Begin transaction: %lu", slotID);

    if (INVALID_SLOT)
        rv = CKR_SLOT_ID_INVALID;
    else if (CKR_ERROR(rv = slot_EstablishConnection(slotID)))
        rv = rv;
    else if (MSC_ERROR(msc_rv = msc_BeginTransaction(&st.slots[slotID - 1].conn)))
    {
        slot_ReleaseConnection(slotID);

        if ((msc_rv == MSC_TOKEN_RESET) || (msc_rv == MSC_TOKEN_REMOVED))
            rv = CKR_DEVICE_REMOVED;
        else
            rv = CKR_FUNCTION_FAILED;
    }

    return rv;
}

/******************************************************************************
** Function: slot_EndTransaction
**
** Description
**
** Parameters:
**  none
**
** Returns:
**  none
*******************************************************************************/
CK_RV slot_EndTransaction(CK_ULONG slotID, CK_ULONG action)
{
    CK_RV rv = CKR_OK;

    log_Log(LOG_LOW, "End transaction: %lu", slotID);

    if (INVALID_SLOT)
        rv = CKR_SLOT_ID_INVALID;
    else if (MSC_ERROR(msc_EndTransaction(&st.slots[slotID - 1].conn, action)))
        rv = CKR_FUNCTION_FAILED;
    else if (st.prefs.threaded)
        (void)CKR_ERROR(rv = slot_ReleaseConnection(slotID));

    return rv;
}

/*
** Checks for an open RW SO session
*/
/******************************************************************************
** Function: slot_CheckRWSOsession
**
** Description
**
** Parameters:
**  none
**
** Returns:
**  none
*******************************************************************************/
CK_BBOOL slot_CheckRWSOsession(CK_ULONG slotID)
{
    CK_BBOOL rv = FALSE;
    P11_Session *session_l;

    session_l = st.sessions;
    while (session_l)
    {
        if ((session_l->session.slotID == slotID) && 
            (session_l->session.state == CKS_RW_SO_FUNCTIONS))
        {
            rv = TRUE;
            break;
        }

        session_l = session_l->next;
    }

    return rv;
}

/******************************************************************************
** Function: slot_EstablishConnection
**
** Description
**
** Parameters:
**  none
**
** Returns:
**  none
*******************************************************************************/
CK_RV slot_EstablishConnection(CK_ULONG slotID)
{
    CK_RV rv = CKR_OK;
    MSC_RV msc_rv;
    MSCTokenInfo token_info;
    P11_Slot *slot;

    if (INVALID_SLOT)
        rv = CKR_SLOT_ID_INVALID;
    else if (CKR_ERROR(rv = slot_TokenPresent(slotID)))
        /* Return error */;
    else 
    {
        slot = &st.slots[slotID - 1];

        log_Log(LOG_LOW, "Attempting establish");

        if (!slot->conn.hCard)
        {
            log_Log(LOG_LOW, "Establish connection");

            memcpy(&token_info, &slot->conn.tokenInfo, sizeof(MSCTokenInfo));
            if (MSC_ERROR(msc_rv = msc_EstablishConnection(&token_info, 
                                                 MSC_SHARE_SHARED,
                                                 NULL, 0,
                                                 &slot->conn)))
            {
                /* memset(&slot->conn, 0x00, sizeof(slot->conn)); */
                /* memcpy(&slot->conn.tokenInfo, &token_info, sizeof(MSCTokenInfo)); */
                st.slot_status[slotID - 1] = 0x01;
                log_Log(LOG_MED, "MSCEstablishConnection failed");

                if ((msc_rv == MSC_TOKEN_RESET) || (msc_rv == MSC_TOKEN_REMOVED))
                    rv = CKR_DEVICE_REMOVED;
                else
                    rv = CKR_FUNCTION_FAILED;
            }
        }
    }

    return rv;
}

/******************************************************************************
** Function: slot_ReleaseConnection
**
** Description
**
** Parameters:
**  none
**
** Returns:
**  none
*******************************************************************************/
CK_RV slot_ReleaseConnection(CK_ULONG slotID)
{
    CK_RV rv = CKR_OK;
    P11_Session *session_l;
    P11_Slot *slot;

    if (INVALID_SLOT)
        rv = CKR_SLOT_ID_INVALID;
    else 
    {
        slot = &st.slots[slotID - 1];
        /* Don't close the connection if a session is using it */
        /* Fixme: this could be sped up by using reference counting instead */
        log_Log(LOG_LOW, "Attempting release");
        session_l = st.sessions;

        while (session_l)
        {
            if (session_l->session.slotID == slotID)
                return rv;
            session_l = session_l->next;
        }

        if (slot->conn.hCard)
        {
            log_Log(LOG_LOW, "Releasing connection (slot_ReleaseConnection)");
            (void)MSC_ERROR(msc_ReleaseConnection(&slot->conn, MSC_LEAVE_TOKEN));
            log_Log(LOG_LOW, "Done releasing (slot_ReleaseConnection)");
        }

        slot->conn.hCard = 0;
    }

    return rv;
}

/******************************************************************************
** Function: slot_UpdateSlot
**
** Description
**
** Parameters:
**  none
**
** Returns:
**  none
*******************************************************************************/
CK_RV slot_UpdateSlot(CK_ULONG slotID)
{
    CK_RV rv = CKR_OK;
    P11_Slot *slot;
    MSCTokenInfo *token_info;
    CK_SLOT_INFO *slot_info;

    if (INVALID_SLOT)
        rv = CKR_SLOT_ID_INVALID;
    else
    {
        slot = &st.slots[slotID - 1];
        token_info = &slot->conn.tokenInfo;
        slot_info = &slot->slot_info;
    
        token_info->tokenState = MSC_STATE_UNAWARE;

        if (MSC_ERROR(msc_WaitForTokenEvent(token_info, 1, 10000)))
            rv = CKR_FUNCTION_FAILED;
        else
        {
            util_PadStrSet(slot_info->slotDescription, (CK_CHAR *)token_info->slotName, sizeof(slot_info->slotDescription));
            util_PadStrSet(slot_info->manufacturerID, (CK_CHAR *)"Unknown MFR", sizeof(slot_info->manufacturerID));
            /* Fixme: If Netscape does not see a token present, it may mark the slot as bad and never use it */
            slot_info->flags = CKF_REMOVABLE_DEVICE | CKF_HW_SLOT |
                               (CKR_ERROR(slot_TokenPresent(slotID)) ? 0x00 : CKF_TOKEN_PRESENT);
            slot_info->hardwareVersion.major = 0x01; /* Fixme: unsupported */
            slot_info->hardwareVersion.minor = 0x00; /* Fixme: unsupported */
            slot_info->firmwareVersion.major = 0x01; /* Fixme: unsupported */
            slot_info->firmwareVersion.minor = 0x00; /* Fixme: unsupported */
        }
    }

    return rv;
}

/******************************************************************************
** Function: slot_VerifyPIN
**
** Description
**
** Parameters:
**  none
**
** Returns:
**  none
*******************************************************************************/
CK_RV slot_VerifyPIN(CK_SLOT_ID slotID, CK_USER_TYPE userType, CK_UTF8CHAR_PTR pPin, CK_ULONG ulPinLen)
{
    CK_RV rv = CKR_OK;
    CK_RV msc_rv = MSC_SUCCESS;
    P11_Slot *slot;

    if (INVALID_SLOT)
        rv = CKR_SLOT_ID_INVALID;
    else
    {
        slot = &st.slots[slotID - 1];

        if (userType == CKU_SO)
            (void)MSC_ERROR(msc_rv = msc_VerifyPIN(&slot->conn, (MSCUChar8)st.prefs.so_user_pin_num, pPin, ulPinLen));
        else if (userType == CKU_USER)
            (void)MSC_ERROR(msc_rv = msc_VerifyPIN(&slot->conn, (MSCUChar8)st.prefs.user_pin_num, pPin, ulPinLen));
        else
            rv = CKR_USER_TYPE_INVALID;
    
        if (msc_rv == MSC_AUTH_FAILED)
        {
            P11_ERR("PIN INCORRECT");
            rv = CKR_PIN_INCORRECT;
        }
        else if (msc_rv == MSC_IDENTITY_BLOCKED)
            rv = CKR_PIN_LOCKED;
        else if (msc_rv != MSC_SUCCESS)
            rv = CKR_FUNCTION_FAILED;
        else if (st.prefs.cache_pin && (userType == CKU_SO))
        {
            // Fixme: do padding // encryption is disabled
            // des_ecb3_encrypt(pPin, slot->pins[CKU_SO].pin, st.pin_key[0], st.pin_key[1], st.pin_key[2], 1);
            memcpy(slot->pins[CKU_SO].pin, pPin, ulPinLen);
            slot->pins[0].pin_size = ulPinLen;
        }
        else if (st.prefs.cache_pin && (userType == CKU_USER))
        {
            // Fixme: do padding // encryption is disabled
            // des_ecb3_encrypt(pPin, slot->pins[CKU_USER].pin, st.pin_key[0], st.pin_key[1], st.pin_key[2], 1);
            memcpy(slot->pins[CKU_USER].pin, pPin, ulPinLen);
            slot->pins[1].pin_size = ulPinLen;
        }
    }

    return rv;
}

/******************************************************************************
** Function: slot_MinRSAKeySize
**
** Returns minimum RSA key size
**
** Fixme: does Netscape use # bits or # bytes for RSA mechanisms?? 
**
** Parameters:
**  cap - MSC capabilities
**
** Returns:
**  Length
*******************************************************************************/
CK_ULONG slot_MinRSAKeySize(MSCULong32 cap)
{
    CK_ULONG rv = 0;

    if (cap & MSC_CAPABLE_RSA_512)
        rv = 64;
    else if (cap & MSC_CAPABLE_RSA_768)
        rv = 96;
    else if (cap & MSC_CAPABLE_RSA_1024)
        rv = 128;
    else if (cap & MSC_CAPABLE_RSA_2048)
        rv = 256;
    else if (cap & MSC_CAPABLE_RSA_4096)
        rv = 512;

    return rv;
}

/******************************************************************************
** Function: slot_MaxRSAKeySize
**
** Returns maximum RSA key size
**
** Fixme: does Netscape use # bits or # bytes for RSA mechanisms?? 
**
** Parameters:
**  cap - MSC capabilities
**
** Returns:
**  Length
*******************************************************************************/
CK_ULONG slot_MaxRSAKeySize(MSCULong32 cap)
{
    CK_ULONG rv = 0;

    if (cap & MSC_CAPABLE_RSA_4096)
        rv = 512;
    else if (cap & MSC_CAPABLE_RSA_2048)
        rv = 256;
    else if (cap & MSC_CAPABLE_RSA_1024)
        rv = 128;
    else if (cap & MSC_CAPABLE_RSA_768)
        rv = 96;
    else if (cap & MSC_CAPABLE_RSA_512)
        rv = 64;

    return rv;
}

/******************************************************************************
** Function: slot_UpdateMechanisms
**
** Updates information about the token in a specific slot.  Many of the symmetric
** algorithms are commented out because they are too slow to use on smartcards
**
** Parameters:
**  slotID - Slot number to update
**
** Returns:
**  CKR_SLOT_ID_INVALID if the slot number is invalid
**  CKR_OK
*******************************************************************************/
CK_RV slot_UpdateMechanisms(CK_ULONG slotID)
{
    CK_RV rv = CKR_OK;
    P11_Slot *slot;
    MSCULong32 len = 0;
    MSCULong32 crypto_alg = 0;
    MSCULong32 temp_cap = 0;
    P11_MechInfo *mech;

    if (INVALID_SLOT)
        rv = CKR_SLOT_ID_INVALID;
    else
    {
        slot = &st.slots[slotID - 1];

        slot_FreeAllMechanisms(slot->mechanisms);
        slot->mechanisms = 0;

        if (MSC_ERROR(msc_GetCapabilities(&slot->conn, 
                                         MSC_TAG_SUPPORT_CRYPTOALG, 
                                         (MSCUChar8 *)&crypto_alg, 
                                         &len)))
            P11_ERR("MSCGetCapabilities failed");

        if (crypto_alg & MSC_SUPPORT_RSA)
        {
            log_Log(LOG_LOW, "Card supports RSA");

            slot_AddMechanism(slot, CKM_SHA1_RSA_PKCS, &mech);
            mech->info.ulMinKeySize = slot_MinRSAKeySize(temp_cap);
            mech->info.ulMaxKeySize = slot_MaxRSAKeySize(temp_cap);
            /* Fixme: these flags may be wrong */
            mech->info.flags = CKF_ENCRYPT |
                               CKF_DECRYPT |
                               CKF_SIGN |
                               CKF_SIGN_RECOVER |
                               CKF_VERIFY |
                               CKF_VERIFY_RECOVER |
                               CKF_GENERATE |
                               CKF_GENERATE_KEY_PAIR |
                               CKF_WRAP |
                               CKF_UNWRAP;

            if (!MSC_ERROR(msc_GetCapabilities(&slot->conn, MSC_TAG_CAPABLE_RSA, 
                            (MSCUChar8 *)&temp_cap, &len)))
            {
                if (temp_cap & MSC_CAPABLE_RSA_KEYGEN)
                {
                    log_Log(LOG_LOW, "Card supports RSA key gen");
                    slot_AddMechanism(slot, CKM_RSA_PKCS_KEY_PAIR_GEN, &mech);

                    mech->info.ulMinKeySize = slot_MinRSAKeySize(temp_cap);
                    mech->info.ulMaxKeySize = slot_MaxRSAKeySize(temp_cap);
                    /* Fixme: these flags may be wrong */
                    mech->info.flags = CKF_GENERATE |
                                       CKF_GENERATE_KEY_PAIR;
                }

                if (temp_cap & (MSC_CAPABLE_RSA_PKCS1 | MSC_CAPABLE_RSA_NOPAD))
                {
                    if (temp_cap & MSC_CAPABLE_RSA_NOPAD)
                        log_Log(LOG_LOW, "Card supports RSA NOPAD");
                    
                    if (temp_cap & MSC_CAPABLE_RSA_PKCS1)
                        log_Log(LOG_LOW, "Card supports RSA PKCS#1");
                    
                    slot_AddMechanism(slot, CKM_RSA_PKCS, &mech);

                    mech->info.ulMinKeySize = slot_MinRSAKeySize(temp_cap);
                    mech->info.ulMaxKeySize = slot_MaxRSAKeySize(temp_cap);
                    /* Fixme: these flags may be wrong */
                    mech->info.flags = CKF_ENCRYPT |
                                       CKF_DECRYPT | 
                                       CKF_SIGN | 
                                       CKF_SIGN_RECOVER | 
                                       CKF_VERIFY | 
                                       CKF_VERIFY_RECOVER;
                    
                    if (temp_cap & MSC_CAPABLE_RSA_NOPAD)
                         mech->info.flags =  CKF_WRAP |                                                                                CKF_UNWRAP;
                }
            }
        }

        if (crypto_alg & MSC_SUPPORT_DSA)
        {
            log_Log(LOG_LOW, "Card supports DSA");
            slot_AddMechanism(slot, CKM_DSA, &mech);

            if (!MSC_ERROR(msc_GetCapabilities(&slot->conn, MSC_TAG_CAPABLE_DSA, (MSCUChar8 *)&temp_cap, &len)))
            {
                if (temp_cap & MSC_CAPABLE_DSA_KEYGEN)
                    slot_AddMechanism(slot, CKM_DSA_KEY_PAIR_GEN, &mech);
            }
        }

        if (crypto_alg & MSC_SUPPORT_ELGAMAL)
        {
            log_Log(LOG_LOW, "Card supports ElGamal");
            /* Fixme: unsupported */
        }

        if (crypto_alg & MSC_SUPPORT_DES)
        {
            log_Log(LOG_LOW, "Card supports DES but not returning mechanism");

            /*
            ** if (!MSC_ERROR(msc_GetCapabilities(&slot->conn, MSC_TAG_CAPABLE_DES, (MSCUChar8 *)&des_cap, &len)))
            ** {
            **     if (des_cap & MSC_CAPABLE_DES_KEYGEN)
            **         slot_AddMechanism(slot, CKM_DES_KEY_GEN, &mech);
            **     if (des_cap & MSC_CAPABLE_DES_CBC)
            **         slot_AddMechanism(slot, CKM_DES_CBC, &mech);
            **     if (des_cap & MSC_CAPABLE_DES_ECB)
            **         slot_AddMechanism(slot, CKM_DES_ECB, &mech);
            ** }
            */
        }

        if (crypto_alg & MSC_SUPPORT_3DES)
        {
            log_Log(LOG_LOW, "Card supports 3DES but not returning mechanism");

            /*
            ** if (!MSC_ERROR(msc_GetCapabilities(&slot->conn, MSC_TAG_CAPABLE_3DES, (MSCUChar8 *)&temp_cap, &len)))
            ** {
            **    if (temp_cap & MSC_CAPABLE_3DES_KEYGEN)
            **        slot_AddMechanism(slot, CKM_DES_KEY_GEN, &mech);
            **    if (des_cap & MSC_CAPABLE_DES_CBC)
            **        slot_AddMechanism(slot, CKM_DES3_CBC, &mech);
            **    if (des_cap & MSC_CAPABLE_DES_ECB)
            **        slot_AddMechanism(slot, CKM_DES3_ECB, &mech);
            ** }
            */
        }

        if (crypto_alg & MSC_SUPPORT_IDEA)
        {
            log_Log(LOG_LOW, "Card supports IDEA");
            /* Fixme: unsupported */
        }

        if (crypto_alg & MSC_SUPPORT_AES)
        {
            log_Log(LOG_LOW, "Card supports AES");
            /* Fixme: unsupported */
        }

        if (crypto_alg & MSC_SUPPORT_BLOWFISH)
        {
            log_Log(LOG_LOW, "Card supports BLOWFISH");
            /* Fixme: unsupported */
        }

        if (crypto_alg & MSC_SUPPORT_TWOFISH)
        {
            log_Log(LOG_LOW, "Card supports TWOFISH");
            /* Fixme: unsupported */
        }
    }

    return rv;
}

/******************************************************************************
** Function: slot_MechanismCount
**
** Counts the number of mechanisms in the provided linked list
**
** Parameters:
**  mech - Mechanism list
**
** Returns:
**  Count
*******************************************************************************/
CK_ULONG slot_MechanismCount(P11_MechInfo *mech)
{
    CK_ULONG count = 0;
    
    while (mech)
    {
        count++;
        mech = mech->next;
    }

    return count;
}

/******************************************************************************
** Function: slot_FreeAllMechanisms
**
** Recursively frees/deletes all mechanisms in list
**
** Parameters:
**  list - List of mechanisms to free
**
** Returns:
**  none
*******************************************************************************/
void slot_FreeAllMechanisms(P11_MechInfo *list)
{
    if (list)
    {
        if (list->next)
            slot_FreeAllMechanisms(list->next);

        free(list);
    }
}

/******************************************************************************
** Function: slot_AddMechanism
**
** Adds a mechanism to a specific slot
**
** Parameters:
**  slot      - Slot to add mechanism to
**  type      - Mechanism type
**  mech_info - Returns new mechanism info
**
** Returns:
**  CKR_HOST_MEMORY if memory alloc fails
**  CKR_OK
*******************************************************************************/
CK_RV slot_AddMechanism(P11_Slot *slot, CK_MECHANISM_TYPE type, P11_MechInfo **mech_info)
{
    CK_RV rv = CKR_OK;

    if (slot->mechanisms)
    {
        slot->mechanisms->prev = (P11_MechInfo *)calloc(1, sizeof(P11_MechInfo));
        if (!slot->mechanisms->prev)
            rv = CKR_HOST_MEMORY;
        else
        {
            slot->mechanisms->prev->next = slot->mechanisms;
            slot->mechanisms = slot->mechanisms->prev;
            slot->mechanisms->type = type;

            if (mech_info)
                *mech_info = slot->mechanisms;
        }
    }
    else
    {
        slot->mechanisms = (P11_MechInfo *)calloc(1, sizeof(P11_MechInfo));
        if (!slot->mechanisms)
            rv = CKR_HOST_MEMORY;
        else
        {
            slot->mechanisms->type = type;

            if (mech_info)
                *mech_info = slot->mechanisms;
        }
    }

    return rv;
}

/******************************************************************************
** Function: slot_UpdateToken
**
** Updates all information about a token in a slot.
**
** Parameters:
**  slotID - Slot number to update
**
** Returns:
**  CKR_SLOT_ID_INVALID if slotID is invalid
**  CKR_OK
*******************************************************************************/
CK_RV slot_UpdateToken(CK_ULONG slotID)
{
    CK_RV rv = CKR_OK;
    P11_Slot *slot;
    P11_Session *session_l;
    CK_ULONG sess_count, rw_sess_count;
    MSCUShort16 pin_bit_mask = 0;
    MSCULong32 support_func = 0;
    MSCUChar8 cap_pin_maxsize = 0;
    MSCUChar8 cap_pin_minsize = 0;
    MSCULong32 len = 0;

    if (INVALID_SLOT)
        rv = CKR_SLOT_ID_INVALID;
    else if (CKR_ERROR((rv = slot_TokenPresent(slotID))))
        (void)CKR_ERROR(slot_DisconnectSlot(slotID, MSC_RESET_TOKEN));
    else
    {
        slot = &st.slots[slotID - 1];

        if (!MSC_ERROR(msc_GetStatus(&slot->conn, &slot->status_info)) &&
            !MSC_ERROR(msc_GetCapabilities(&slot->conn, MSC_TAG_SUPPORT_FUNCTIONS, (MSCUChar8 *)&support_func, &len)) &&
            !MSC_ERROR(msc_GetCapabilities(&slot->conn, MSC_TAG_CAPABLE_PIN_MAXSIZE, &cap_pin_maxsize, &len)) &&
            !MSC_ERROR(msc_GetCapabilities(&slot->conn, MSC_TAG_CAPABLE_PIN_MINSIZE, &cap_pin_minsize, &len)) &&
            !MSC_ERROR(msc_ListPINs(&slot->conn, &pin_bit_mask))
            )
        {
            session_l = st.sessions;
            sess_count = rw_sess_count = 0;

            while (session_l)
            {
                if (session_l->session.slotID == slotID)
                {
                    sess_count++;
                    if (session_l->session.flags & CKF_RW_SESSION)
                        rw_sess_count++;
                }

                session_l = session_l->next;
            }

            util_PadStrSet(slot->token_info.label, (CK_CHAR *)slot->conn.tokenInfo.tokenName, sizeof(slot->token_info.label));
            util_PadStrSet(slot->token_info.manufacturerID, (CK_CHAR *)"Unknown MFR", sizeof(slot->token_info.manufacturerID));
            util_PadStrSet(slot->token_info.model, (CK_CHAR *)"Unknown Model", sizeof(slot->token_info.model));
            util_PadStrSet(slot->token_info.serialNumber, (CK_CHAR *)"1", sizeof(slot->token_info.serialNumber)); /* Fixme: */
            slot->token_info.flags = (support_func & MSC_SUPPORT_GETCHALLENGE ? CKF_RNG : 0x00) |
                                     CKF_LOGIN_REQUIRED |
                                     (pin_bit_mask & (1 << st.prefs.user_pin_num) ? CKF_USER_PIN_INITIALIZED : 0x00) |
                                     CKF_TOKEN_INITIALIZED;

            /* None of these  */  /* CKF_WRITE_PROTECTED | */
            /* are used (yet) */  /* CKR_RESTORE_KEY_NOT_NEEDED | */ /* Not supported */
                                  /* CKF_CLOCK_ON_TOKEN | */
                                  /* CKF_PROTECTED_AUTHENICATION_PATH | */
                                  /* CKF_DUAL_CRYPTO_OPERATIONS | */
                                  /* CKF_SECONDARY_AUTHENTICATION | */
                                  /* CKF_USER_PIN_COUNT_LOW | */
                                  /* CKF_USER_PIN_FINAL_TRY | */
                                  /* CKF_USER_PIN_LOCKED | */
                                  /* CKF_USER_PIN_TO_BE_CHANGED | */
                                  /* CKF_SO_PIN_COUNT_LOW | */
                                  /* CKF_SO_PIN_FINAL_TRY | */
                                  /* CKF_SO_PIN_LOCKED | */
                                  /* CKF_SO_PIN_TO_BE_CHANGED | */

            slot->token_info.ulMaxSessionCount = CK_EFFECTIVELY_INFINITE;
            slot->token_info.ulSessionCount = sess_count;
            slot->token_info.ulMaxRwSessionCount = CK_EFFECTIVELY_INFINITE;
            slot->token_info.ulRwSessionCount = rw_sess_count;
            slot->token_info.ulMaxPinLen = cap_pin_maxsize;
            slot->token_info.ulMinPinLen = cap_pin_minsize;
            slot->token_info.ulTotalPublicMemory = slot->status_info.totalMemory;  /* Fixme: Does this conflict */
            slot->token_info.ulFreePublicMemory = slot->status_info.freeMemory;    /* with the private memory? */
            slot->token_info.ulTotalPrivateMemory = slot->status_info.totalMemory; /* Fixme: Does this conflict */
            slot->token_info.ulFreePrivateMemory = slot->status_info.freeMemory;   /* with the public memory? */
            slot->token_info.hardwareVersion.major = ((char *)(&slot->status_info.swVersion))[0];  /* Fixme: endian?? */;
            slot->token_info.hardwareVersion.minor = ((char *)(&slot->status_info.swVersion))[1];
            slot->token_info.firmwareVersion.major = ((char *)(&slot->status_info.appVersion))[0]; /* Fixme: endian?? */
            slot->token_info.firmwareVersion.minor = ((char *)(&slot->status_info.appVersion))[1];
            memset(slot->token_info.utcTime, '0', sizeof(slot->token_info.utcTime));

            log_Log(LOG_LOW, "Token: %s", slot->conn.tokenInfo.tokenName);
        }
    }

    return rv;
}

/******************************************************************************
** Function: slot_UpdateSlotList
**
** Updates information about all connected slots
**
** Note: this wipes out all current sessions and objects
**
** Parameters:
**  none
**
** Returns:
**  CKR_FUNCTION_FAILED on general error
**  CKR_OK
*******************************************************************************/
CK_RV slot_UpdateSlotList()
{
    CK_RV rv = CKR_OK;
    CK_ULONG i;
    MSCTokenInfo *tokenArray = 0;
    MSCULong32 arrayLength = 0;

    /* Fixme: What happens if there are no readers connected? */
    if (MSC_ERROR(msc_ListTokens(MSC_LIST_SLOTS, NULL, &arrayLength)))
    {
        rv = CKR_FUNCTION_FAILED;
        P11_ERR("MSCListTokens failed");
    }
    else
    {
        tokenArray = (MSCTokenInfo *)calloc(arrayLength, sizeof(MSCTokenInfo));
    
        if (!tokenArray)
            rv = CKR_HOST_MEMORY;
        else if (MSC_ERROR(msc_ListTokens(MSC_LIST_SLOTS, tokenArray, &arrayLength)))
        {
            rv = CKR_FUNCTION_FAILED;
            P11_ERR("MSCListTokens failed");
        }
        else
        {
            slot_FreeAllSlots();
        
            st.slots = (P11_Slot *)calloc(arrayLength, sizeof(P11_Slot));
        
            if (!st.slots)
            {
                rv = CKR_HOST_MEMORY;
                P11_ERR("calloc failed");
            }
            else for (i = 0; i < arrayLength; i++)
            {
                memcpy(&st.slots[i].conn.tokenInfo, &tokenArray[i], sizeof(MSCTokenInfo));
                st.slot_count = i + 1;
                log_Log(LOG_LOW, "Added reader: %s", tokenArray[i].slotName);
            }
        
            if (CKR_ERROR(rv) && st.slots)
            {
                free(st.slots);
                st.slots = 0;
            }
        }
    }

    if (tokenArray)
        free(tokenArray);
        
    return rv;
}

/******************************************************************************
** Function: slot_FreeAllSlots
**
** Releases all slots and all tokens.  This assumes you are shutting down and
** therefore cannot fail.
**
** Parameters:
**  none
**
** Returns:
**  CKR_OK
*******************************************************************************/
CK_RV slot_FreeAllSlots()
{
    CK_RV rv = CKR_OK;
    CK_ULONG i;

    if (st.slots)
    {
        for (i = 1; i <= st.slot_count; i++)
            slot_DisconnectSlot(i, MSC_RESET_TOKEN); /* Fixme: Don't care if this fails? */

        free(st.slots);
        log_Log(LOG_LOW, "Freed st.slots");

        st.slots = 0;
        st.slot_count = 0;
    }

    return rv;
}

/******************************************************************************
** Function: slot_DisconnectSlot
**
** Releases a slot and all information about any tokens in the slot.
**
** Parameters:
**  slotID - Slot number to release
**  action - A valid MSC card action: MSC_RESET_TOKEN or MSC_LEAVE_TOKEN
**
** Returns:
**  CKR_SLOT_ID_INVALID if slotID is invalid
**  CKR_OK
*******************************************************************************/
CK_RV slot_DisconnectSlot(CK_ULONG slotID, CK_ULONG action)
{
    CK_RV rv = CKR_OK;
    P11_Session *session_l;
    P11_Slot *slot;

    if (INVALID_SLOT)
        rv = CKR_SLOT_ID_INVALID;
    else
    {
        slot = &st.slots[slotID - 1];
        session_l = st.sessions;

        while (session_l)
        {
            if (session_l->session.slotID == slotID)
                session_FreeSession((CK_SESSION_HANDLE)session_l);

            session_l = st.sessions;
        }

        object_FreeAllObjects(slotID, st.slots[slotID - 1].objects);
        slot_FreeAllMechanisms(slot->mechanisms);
        slot->mechanisms = 0;
        memset(slot->pins, 0x00, sizeof(slot->pins));
        slot->pin_state = 0;
        slot_BlankTokenInfo(&slot->token_info);

        memset(&slot->status_info, 0x00, sizeof(slot->status_info));

        if (slot->conn.hCard)
        {
            log_Log(LOG_LOW, "Releasing connection (slot_DisconnectSlot)");
            (void)MSC_ERROR(msc_ReleaseConnection(&slot->conn, action));
        }

        slot->conn.hCard = 0;
        slot->slot_info.flags = (slot->slot_info.flags & ~CKF_TOKEN_PRESENT);
    }

    return rv;
}

/******************************************************************************
** Function: slot_PublicMode
**
** Switchs all sessions of a slot to PUBLIC mode (versus USER or SO mode)
**
** Parameters:
**  slotID - Slot number
**
** Returns:
**  CKR_SLOT_ID_INVALID if slotID is invalid
**  CKR_OK
*******************************************************************************/
CK_RV slot_PublicMode(CK_ULONG slotID)
{
    CK_RV rv = CKR_OK;
    P11_Session *session_l;

    if (INVALID_SLOT)
        rv = CKR_SLOT_ID_INVALID;
    else
    {
        st.slots[slotID - 1].pin_state = 0; /* Fixme: create #define for this */

        session_l = st.sessions;
        while (session_l)
        {
            if (session_l->session.slotID == slotID)
            {
                switch (session_l->session.state)
                {
                case CKS_RO_USER_FUNCTIONS:
                    session_l->session.state = CKS_RO_PUBLIC_SESSION;
                    break;
                case CKS_RW_USER_FUNCTIONS:
                    session_l->session.state = CKS_RW_PUBLIC_SESSION;
                    break;
                case CKS_RW_SO_FUNCTIONS: /* Fixme: can't really handle this one well */
                    session_l->session.state = CKS_RO_PUBLIC_SESSION;
                    break;
                default:
                    break;
                }

                session_l->session.flags = (session_l->session.flags & ~CKF_RW_SESSION);
            }

            session_l = session_l->next;
        }
    }

    return rv;
}

/******************************************************************************
** Function: slot_UserMode
**
** Switchs all sessions of a slot to USER mode (versus PUBLIC or SO mode)
**
** Parameters:
**  slotID - Slot number
**
** Returns:
**  CKR_SLOT_ID_INVALID if slotID is invalid
**  CKR_OK
*******************************************************************************/
CK_RV slot_UserMode(CK_ULONG slotID)
{
    CK_RV rv = CKR_OK;
    P11_Session *session_l;

    if (INVALID_SLOT)
        rv = CKR_SLOT_ID_INVALID;
    else
    {
        st.slots[slotID - 1].pin_state = 1; /* Fixme: create #define for this */

        session_l = st.sessions;
        while (session_l)
        {
            if (session_l->session.slotID == slotID)
            {
                switch (session_l->session.state)
                {
                case CKS_RO_PUBLIC_SESSION:
                    object_UserMode((CK_SESSION_HANDLE)session_l);
                    session_l->session.state = CKS_RO_USER_FUNCTIONS;
                    break;
                case CKS_RW_PUBLIC_SESSION:
                    object_UserMode((CK_SESSION_HANDLE)session_l);
                    session_l->session.state = CKS_RW_USER_FUNCTIONS;
                    break;
                default:
                    break;
                }

                session_l->session.flags = (session_l->session.flags | CKF_RW_SESSION);
            }

            session_l = session_l->next;
        }
    }

    return rv;
}

/******************************************************************************
** Function: slot_TokenPresent
**
** Checks if a token is present in a slot
**
** Parameters:
**  slotID - Slot number to check
**
** Returns:
**  CKR_SLOT_ID_INVALID if slotID is invalid
**  CKR_TOKEN_NOT_PRESENT or CKR_TOKEN_NOT_RECOGNIZED
**  CKR_OK
*******************************************************************************/
CK_RV slot_TokenPresent(CK_ULONG slotID)
{
    CK_RV rv;

    /* Fixme: Use the MSC tests */
    if (INVALID_SLOT)
        rv = CKR_SLOT_ID_INVALID;
    else if (strcmp(st.slots[slotID - 1].conn.tokenInfo.tokenName, MSC_TOKEN_EMPTY_STR) == 0)
        rv = CKR_TOKEN_NOT_PRESENT;
    else if (strcmp(st.slots[slotID - 1].conn.tokenInfo.tokenName, MSC_TOKEN_UNKNOWN_STR) == 0)
        rv = CKR_TOKEN_NOT_RECOGNIZED;
    else
        rv = CKR_OK;

    return rv;
}

/******************************************************************************
** Function: slot_TokenChanged
**
** Master routine that checks the status of a token.  This calls
** slot_UpdateToken and slot_UpdateMechanisms to update information about the
** current token.  If a token status has not changed then this function does
** not do anything.
**
** Parameters:
**  none
**
** Returns:
**  CKR_CRYPTOKI_NOT_INITIALIZED
**  CKR_OK
*******************************************************************************/
CK_RV slot_TokenChanged()
{
    CK_RV rv = CKR_OK;
    CK_ULONG i;
    P11_Session *sess;

    sess = st.sessions;
    log_Log(LOG_LOW, "Active session list:");
    while (sess)
    {
        log_Log(LOG_LOW, "Session ID: %X", sess);
        sess = sess->next;
    }

    if (!st.initialized)
        rv = CKR_CRYPTOKI_NOT_INITIALIZED;
    else if (!st.slot_status || !st.slots)
        rv = CKR_FUNCTION_FAILED;
    else
        for (i = 0; i < st.slot_count; i++)
        {
            if (!st.prefs.threaded && !(st.slots[i].slot_info.flags & CKF_TOKEN_PRESENT))
            {
                slot_UpdateSlot(i + 1);
                if ((st.slots[i].slot_info.flags & CKF_TOKEN_PRESENT))
                    st.slot_status[i] = 0x01;
            }

            /* Old code, may still be useful; handles reset differently
            if (st.slot_status[i] || (st.slots[i].conn.hCard &&
                (msc_IsTokenReset(&st.slots[i].conn) || msc_IsTokenMoved(&st.slots[i].conn))))
            */

            if (st.slot_status[i] || (st.slots[i].conn.hCard &&
                msc_IsTokenMoved(&st.slots[i].conn)))
            {
                if (st.slots[i].conn.hCard)
                    msc_ClearReset(&st.slots[i].conn);

                log_Log(LOG_LOW, "Slot %d changed", i + 1);

                slot_DisconnectSlot(i + 1, MSC_RESET_TOKEN);
                slot_UpdateSlot(i + 1);

                if (!CKR_ERROR(slot_BeginTransaction(i + 1)))
                {
                    (void)CKR_ERROR(slot_UpdateToken(i + 1));
                    (void)CKR_ERROR(slot_UpdateMechanisms(i + 1));
                    (void)CKR_ERROR(slot_EndTransaction(i + 1, MSC_LEAVE_TOKEN));
                }
    
                st.slot_status[i] = 0x00;

                if (st.slots[i].conn.hCard)
                    rv = CKR_DEVICE_REMOVED;
            }
        }

    return rv;
}

/******************************************************************************
** Function: slot_BlankTokenInfo
**
** Sets a CK_TOKEN_INFO structure to blank data
**
** Parameters:
**  token_info - Structure to blank
**
** Returns:
**  none
*******************************************************************************/
void slot_BlankTokenInfo(CK_TOKEN_INFO *token_info)
{
    memset(token_info, 0x00, sizeof(MSCTokenInfo));

    util_PadStrSet(token_info->label, (CK_CHAR *)"", sizeof(token_info->label));
    util_PadStrSet(token_info->manufacturerID, (CK_CHAR *)"", sizeof(token_info->manufacturerID));
    util_PadStrSet(token_info->model, (CK_CHAR *)"", sizeof(token_info->model));
    util_PadStrSet(token_info->serialNumber, (CK_CHAR *)"", sizeof(token_info->serialNumber));
    memset(token_info->utcTime, '0', sizeof(token_info->utcTime));
}

/******************************************************************************
** Function: slot_ReverifyPins()
**
** Reverifies cached PIN's.  If any cached PIN fails to verify then this will
** kill that PIN so that it won't be used again.  This is to prevent the
** caching mechanism from inadvertantly locking a token.
**
** Parameters:
**  none
**
** Returns:
**  Error from slot_VerifyPIN
**  CKR_OK
*******************************************************************************/
CK_RV slot_ReverifyPins()
{
    CK_RV rv = CKR_OK;
    CK_ULONG i;

    log_Log(LOG_LOW, "Reverifying all cached PIN's");

    for (i = 0; i < st.slot_count; i++)
    {
        if (st.slots[i].conn.hCard && msc_IsTokenReset(&st.slots[i].conn))
        {
            msc_ClearReset(&st.slots[i].conn);

            if (st.slots[i].pins[CKU_SO].pin_size)
                rv = slot_VerifyPIN(i + 1, 
                                    CKU_SO, 
                                    st.slots[i].pins[CKU_SO].pin, 
                                    st.slots[i].pins[CKU_SO].pin_size);
        
            if (CKR_ERROR(rv))
            {
                st.slots[i].pins[CKU_SO].pin_size = 0;
                memset(st.slots[i].pins[CKU_SO].pin, 0x00, sizeof(st.slots[i].pins[CKU_SO].pin));
            }
            else if (st.slots[i].pins[CKU_USER].pin_size)
            {
                rv = slot_VerifyPIN(i + 1, 
                                    CKU_USER, 
                                    st.slots[i].pins[CKU_USER].pin, 
                                    st.slots[i].pins[CKU_USER].pin_size);
        
                if (CKR_ERROR(rv))
                {
                    st.slots[i].pins[CKU_USER].pin_size = 0;
                    memset(st.slots[i].pins[CKU_USER].pin, 0x00, sizeof(st.slots[i].pins[CKU_USER].pin));
                }
                else
                    slot_UserMode(i + 1);
            }
        }
    }

    return rv;
}

