/******************************************************************************
** 
**  $Id: p11_object.c,v 1.2 2003/02/13 20:06:38 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: Object management
** 
******************************************************************************/

#include "cryptoki.h"

/* C_CreateObject creates a new object. */
CK_DEFINE_FUNCTION(CK_RV, C_CreateObject)
(
  CK_SESSION_HANDLE hSession,    /* the session's handle */
  CK_ATTRIBUTE_PTR  pTemplate,   /* the object's template */
  CK_ULONG          ulCount,     /* attributes in template */
  CK_OBJECT_HANDLE_PTR phObject  /* gets new object's handle. */
)
{
    CK_RV rv = CKR_OK;
    P11_Session *session = (P11_Session *)hSession;
    P11_Object *object;
    P11_Attrib *attrib;
    CK_BBOOL is_token = 0, token_attrib;
    CK_ULONG i;
    CK_ULONG obj_class;
    CK_ATTRIBUTE t_attrib;

    P11_LOG_START("C_CreateObject");

    thread_MutexLock(st.async_lock);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (!pTemplate)
        rv = CKR_ARGUMENTS_BAD;
    else if (INVALID_SESSION)
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (READ_ONLY_SESSION)
        rv = CKR_SESSION_READ_ONLY;
    else if (!CKR_ERROR(object_TemplateGetAttrib(CKA_CLASS, pTemplate, ulCount, 0)))
    {
        if (!CKR_ERROR(rv = object_AddObject(session->session.slotID, (CK_OBJECT_HANDLE *)&object)))
        {
            for (i = 0; i < ulCount; i++)
            {
                if (pTemplate[i].type == CKA_TOKEN)
                    is_token = ((CK_BYTE *)(pTemplate[i].pValue))[0];

                if ((pTemplate[i].type == CKA_VALUE) ||
                    (pTemplate[i].type == CKA_SUBJECT) ||
                    (pTemplate[i].type == CKA_ISSUER) ||
                    (pTemplate[i].type == CKA_SERIAL_NUMBER) ||
                    (pTemplate[i].type == CKA_PUBLIC_EXPONENT) ||
                    (pTemplate[i].type == CKA_PRIVATE_EXPONENT) ||
                    (pTemplate[i].type == CKA_PRIME_1) ||
                    (pTemplate[i].type == CKA_PRIME_2) ||
                    (pTemplate[i].type == CKA_EXPONENT_1) ||
                    (pTemplate[i].type == CKA_EXPONENT_2) ||
                    (pTemplate[i].type == CKA_COEFFICIENT))
                {
                    token_attrib = FALSE;
                }
                else
                    token_attrib = TRUE;

                if (CKR_ERROR(rv = object_AddAttribute(object, 
                                                       pTemplate[i].type, 
                                                       token_attrib,
                                                       (CK_BYTE *)pTemplate[i].pValue, 
                                                       pTemplate[i].ulValueLen, 
                                                       &attrib)))
                    break;

                object_LogAttribute(&pTemplate[i]);
            }

            if (is_token && !CKR_ERROR(rv))
            {
                t_attrib.type = CKA_CLASS;
                t_attrib.pValue = &obj_class;
                t_attrib.ulValueLen = sizeof(obj_class);

                obj_class = CKO_CERTIFICATE;

                if (object_MatchAttrib(&t_attrib, object))
                {
                    /* Pull the serial from the cert because Netscape/Mozilla sets it incorrectly */
                    CK_BYTE buf[4096]; /* Fixme: don't hardcode this */
                    P11_Attrib *obj_attrib;
                    CK_ULONG len;

                    if (!CKR_ERROR(object_GetAttrib(CKA_VALUE, object, &obj_attrib)) &&
                        !CKR_ERROR(object_GetCertSerial((CK_BYTE *)obj_attrib->attrib.pValue,
                                                        obj_attrib->attrib.ulValueLen,
                                                        buf,
                                                        &len)))
                    {
                        log_Log(LOG_LOW, "Overwriting certificate serial number with infered value");
                        (void)CKR_ERROR(object_AddAttribute(object, CKA_SERIAL_NUMBER, FALSE, buf, len, 0));
                    }

                    /* Write the cert & attributes to the card */
                    log_Log(LOG_LOW, "Creating certificate");
                    rv = object_CreateCertificate(hSession, object);
                }
                else
                {
                    obj_class = CKO_PUBLIC_KEY;
                    if (object_MatchAttrib(&t_attrib, object))
                    {
                        log_Log(LOG_LOW, "Creating public key");
                        rv = object_CreatePublicKey(hSession, object);
                    }
                    else
                    {
                        obj_class = CKO_PRIVATE_KEY;
                        if (object_MatchAttrib(&t_attrib, object))
                        {
                            log_Log(LOG_LOW, "Creating private key");
                            rv = object_CreatePrivateKey(hSession, object);

                            { P11_Object *object2;
                            // Fixme: hack to make sure the public key is on the token since some
                            // Fixme: cards require the public key.  Missing error checking.
                            log_Log(LOG_LOW, "Creating public key (THIS MAY BE REMOVED IN THE FUTURE)");
                            rv = object_AddObject(session->session.slotID, (CK_OBJECT_HANDLE *)&object2);
                            (void)CKR_ERROR(rv);

                            obj_class = CKO_PUBLIC_KEY;
                            rv = object_AddAttribute(object2, 
                                                     CKA_CLASS, 
                                                     TRUE,
                                                     (CK_BYTE *)&obj_class, 
                                                     sizeof(obj_class), 0);

                            rv = object_GetAttrib(CKA_ID, object, &attrib);
                            (void)CKR_ERROR(rv);
                            rv = object_AddAttribute(object2, 
                                                     attrib->attrib.type, 
                                                     TRUE,
                                                     (CK_BYTE *)attrib->attrib.pValue, 
                                                     attrib->attrib.ulValueLen, 0);
                            (void)CKR_ERROR(rv);
                            rv = object_GetAttrib(CKA_MODULUS, object, &attrib);
                            (void)CKR_ERROR(rv);
                            rv = object_AddAttribute(object2, 
                                                     attrib->attrib.type, 
                                                     TRUE,
                                                     (CK_BYTE *)attrib->attrib.pValue, 
                                                     attrib->attrib.ulValueLen, 0);
                            (void)CKR_ERROR(rv);
                            rv = object_GetAttrib(CKA_PUBLIC_EXPONENT, object, &attrib);
                            (void)CKR_ERROR(rv);
                            rv = object_AddAttribute(object2, 
                                                     attrib->attrib.type, 
                                                     TRUE,
                                                     (CK_BYTE *)attrib->attrib.pValue, 
                                                     attrib->attrib.ulValueLen, 0);
                            (void)CKR_ERROR(rv);

                            rv = object_CreatePublicKey(hSession, object2);
                            (void)CKR_ERROR(rv); }
                        }
                        else
                        {
                            log_Log(LOG_LOW, "Creating object");
                            rv = object_CreateObject(hSession, object);
                        }
                    }
                }
            }

            *phObject = (CK_OBJECT_HANDLE)object;
            log_Log(LOG_LOW, "New object handle: %lX", *phObject);
        }
    }
    else
        rv = CKR_TEMPLATE_INCOMPLETE;

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_CreateObject");
    return rv;
}


/* C_CopyObject copies an object, creating a new object for the
 * copy. */
CK_DEFINE_FUNCTION(CK_RV, C_CopyObject)
(
  CK_SESSION_HANDLE    hSession,    /* the session's handle */
  CK_OBJECT_HANDLE     hObject,     /* the object's handle */
  CK_ATTRIBUTE_PTR     pTemplate,   /* template for new object */
  CK_ULONG             ulCount,     /* attributes in template */
  CK_OBJECT_HANDLE_PTR phNewObject  /* receives handle of copy */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_CopyObject");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_CopyObject");

    return rv;
}


/* C_DestroyObject destroys an object. */
CK_DEFINE_FUNCTION(CK_RV, C_DestroyObject)
(
  CK_SESSION_HANDLE hSession,  /* the session's handle */
  CK_OBJECT_HANDLE  hObject    /* the object's handle */
)
{
    CK_RV rv = CKR_OK;
    P11_Session *session = (P11_Session *)hSession;

    P11_LOG_START("C_DestroyObject");

    thread_MutexLock(st.async_lock);

    log_Log(LOG_LOW, "Object handle: %lX", hObject);
    object_FreeObject(session->session.slotID, (P11_Object *)hObject);

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_DestroyObject");

    return rv;
}


/* C_GetObjectSize gets the size of an object in bytes. */
CK_DEFINE_FUNCTION(CK_RV, C_GetObjectSize)
(
  CK_SESSION_HANDLE hSession,  /* the session's handle */
  CK_OBJECT_HANDLE  hObject,   /* the object's handle */
  CK_ULONG_PTR      pulSize    /* receives size of object */
)
{
    CK_RV rv = CKR_OK;

    P11_LOG_START("C_GetObjectSize");

    thread_MutexLock(st.async_lock);

    rv = CKR_FUNCTION_NOT_SUPPORTED;
    log_Log(LOG_MED, "Function not supported");

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_GetObjectSize");

    return rv;
}


/* C_GetAttributeValue obtains the value of one or more object
 * attributes. */
CK_DEFINE_FUNCTION(CK_RV, C_GetAttributeValue)
(
  CK_SESSION_HANDLE hSession,   /* the session's handle */
  CK_OBJECT_HANDLE  hObject,    /* the object's handle */
  CK_ATTRIBUTE_PTR  pTemplate,  /* specifies attrs; gets vals */
  CK_ULONG          ulCount     /* attributes in template */
)
{
    CK_RV rv = CKR_OK;
    CK_RV perm_rv = CKR_OK;
    P11_Object *object = (P11_Object *)hObject;
    P11_Attrib *attrib;
    CK_ULONG i;
    CK_CHAR *obj_type;

    P11_LOG_START("C_GetAttributeValue");

    thread_MutexLock(st.async_lock);

    log_Log(LOG_LOW, "Object handle: %lX", hObject);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (!pTemplate)
        rv = CKR_ARGUMENTS_BAD;
    else if (INVALID_SESSION)
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (INVALID_OBJECT)
        rv = CKR_OBJECT_HANDLE_INVALID;
    else
    {
        if (object->msc_key)
            obj_type = (CK_CHAR *)"Key";
        else if (object->msc_obj)
            obj_type = (CK_CHAR *)"Object";
        else
            obj_type = (CK_CHAR *)"Non-token object";

        log_Log(LOG_LOW, "Get attribute object handle: %X  type: %s", hObject, obj_type);
        for (i = 0; i < ulCount; i++)
        {
            log_Log(LOG_LOW, "Trying to get attribute: 0x%X", pTemplate[i].type);

            if (!CKR_ERROR(rv = object_GetAttrib(pTemplate[i].type, object, &attrib)))
            {
                if (pTemplate[i].pValue == 0)
                {
                    log_Log(LOG_LOW, "pValue is NULL returning length: %lu", attrib->attrib.ulValueLen);
                    pTemplate[i].ulValueLen = attrib->attrib.ulValueLen;
                }
                else if (pTemplate[i].ulValueLen < attrib->attrib.ulValueLen)
                {
                    log_Log(LOG_LOW, "Output buffer too small: %lu < %lu", 
                                        pTemplate[i].ulValueLen, attrib->attrib.ulValueLen);
                    rv = CKR_BUFFER_TOO_SMALL;
                    pTemplate[i].ulValueLen = (CK_ULONG)-1;
                }
                else
                {
                    log_Log(LOG_LOW, "Returning attribute");
                    object_LogAttribute(&attrib->attrib);

                    memcpy(pTemplate[i].pValue, attrib->attrib.pValue, attrib->attrib.ulValueLen);
                    pTemplate[i].ulValueLen = attrib->attrib.ulValueLen;

                    /* FIXME: hack for Mozilla 1.1b endian issue */
                    if (pTemplate[i].type == CKA_CLASS)
                    {
                        if (util_IsLittleEndian())
                        {
                            if ((((CK_BYTE *)pTemplate[i].pValue)[0] == 0x00) || 
                                (((CK_BYTE *)pTemplate[i].pValue)[0] == 0x80))
                            {
                                log_Log(LOG_LOW, "Reversing CKA_CLASS for little endian");
                                util_byterev(pTemplate[i].pValue, pTemplate[i].ulValueLen);
                            }
                        }
                        else
                        {
                            if ((((CK_BYTE *)pTemplate[i].pValue)[0] != 0x00) &&
                                (((CK_BYTE *)pTemplate[i].pValue)[0] != 0x80))
                            {
                                log_Log(LOG_LOW, "Reversing CKA_CLASS for big endian");
                                util_byterev(pTemplate[i].pValue, pTemplate[i].ulValueLen);
                            }
                        }
                    }
                }
            }
            else
            {
                pTemplate[i].ulValueLen = (CK_ULONG)-1;
                perm_rv = rv;
                rv = CKR_OK;
            }
        }

        if ((rv == CKR_OK) && (perm_rv != CKR_OK))
            rv = perm_rv;
    }
    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_GetAttributeValue");

    return rv;
}


/* C_SetAttributeValue modifies the value of one or more object
 * attributes */
CK_DEFINE_FUNCTION(CK_RV, C_SetAttributeValue)
(
  CK_SESSION_HANDLE hSession,   /* the session's handle */
  CK_OBJECT_HANDLE  hObject,    /* the object's handle */
  CK_ATTRIBUTE_PTR  pTemplate,  /* specifies attrs and values */
  CK_ULONG          ulCount     /* attributes in template */
)
{
    CK_RV rv = CKR_OK;
    P11_Session *session = (P11_Session *)hSession;
    P11_Object *object = (P11_Object *)hObject;
    ULONG i;

    P11_LOG_START("C_SetAttributeValue");

    thread_MutexLock(st.async_lock);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (!pTemplate)
        rv = CKR_ARGUMENTS_BAD;
    else if (INVALID_SESSION)
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (INVALID_OBJECT)
        rv = CKR_OBJECT_HANDLE_INVALID;
    else if (CKR_ERROR(rv = slot_BeginTransaction(session->session.slotID)))
        /* Intentionally blank */;
    else
    {
        for (i = 0; i < ulCount; i++)
        {
            log_Log(LOG_LOW, "SetAttributeValue:");
            object_LogAttribute(&pTemplate[i]);

            if (CKR_ERROR(rv = object_AddAttribute(object,
                                                   pTemplate[i].type,
                                                   TRUE, /* Fixme: Always a token attribute? */
                                                   (CK_BYTE *)pTemplate[i].pValue,
                                                   pTemplate[i].ulValueLen, 0)))
                break;
        }
    
        (void)CKR_ERROR(rv = object_WriteAttributes(hSession, object));

        (void)CKR_ERROR(rv = slot_EndTransaction(session->session.slotID, MSC_LEAVE_TOKEN));
    }

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_SetAttributeValue");

    return rv;
}


/* C_FindObjectsInit initializes a search for token and session
 * objects that match a template. */
CK_DEFINE_FUNCTION(CK_RV, C_FindObjectsInit)
(
  CK_SESSION_HANDLE hSession,   /* the session's handle */
  CK_ATTRIBUTE_PTR  pTemplate,  /* attribute values to match */
  CK_ULONG          ulCount     /* attrs in search template */
)
{
    CK_RV rv = CKR_OK;
    CK_RV msc_rv;
    P11_Slot *slot;
    P11_Session *session = (P11_Session *)hSession;
    MSCKeyInfo keyInfo;
    MSCObjectInfo objectInfo;

    P11_LOG_START("C_FindObjectsInit");

    thread_MutexLock(st.async_lock);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (ulCount && !pTemplate)
        rv = CKR_ARGUMENTS_BAD;
    else if (INVALID_SESSION)
        rv = CKR_SESSION_HANDLE_INVALID;
    else
    {
        slot = &st.slots[session->session.slotID - 1];
        session->search_attrib_count = 0;

        if (ulCount)
        {
            /* Fixme: Big problem? May need to malloc the pValue for the local search attributes. */
            /*        Otherwise it uses the memory pointer back into the calling application. */
            session->search_attrib = (CK_ATTRIBUTE *)calloc(ulCount, sizeof(CK_ATTRIBUTE));
            if (!session->search_attrib)
                rv = CKR_HOST_MEMORY;
            else
            {
                memcpy(session->search_attrib, pTemplate, ulCount * sizeof(CK_ATTRIBUTE));
                session->search_attrib_count = ulCount;
            }
        }

        if (st.prefs.multi_app || !slot->objects)
        {
            if (!CKR_ERROR(rv = slot_BeginTransaction(session->session.slotID)))
            {
                msc_rv = msc_ListKeys(&slot->conn, MSC_SEQUENCE_RESET, &keyInfo);
                while (!MSC_ERROR(msc_rv) && !CKR_ERROR(rv))
                {
                    rv = object_UpdateKeyInfo(hSession, 0, &keyInfo);
                    msc_rv = msc_ListKeys(&slot->conn, MSC_SEQUENCE_NEXT, &keyInfo); 
                }
        
                msc_rv = msc_ListObjects(&slot->conn, MSC_SEQUENCE_RESET, &objectInfo);
                while (!MSC_ERROR(msc_rv) && !CKR_ERROR(rv))
                {
                    if (!islower(objectInfo.objectID[0]))
                        rv = object_UpdateObjectInfo(hSession, 0, &objectInfo);
        
                    msc_rv = msc_ListObjects(&slot->conn, MSC_SEQUENCE_NEXT, &objectInfo); 
                }
    
                /* Fixme: Need to delete objects that are no longer on the token */

                if (!CKR_ERROR(rv))
                    (void)CKR_ERROR(rv = slot_EndTransaction(session->session.slotID, MSC_LEAVE_TOKEN));
                else
                    (void)CKR_ERROR(slot_EndTransaction(session->session.slotID, MSC_LEAVE_TOKEN));
            }
        }

        session->search_object = st.slots[session->session.slotID - 1].objects;
    }

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_FindObjectsInit");
    return rv;
}


/* C_FindObjects continues a search for token and session
 * objects that match a template, obtaining additional object
 * handles. */
CK_DEFINE_FUNCTION(CK_RV, C_FindObjects)
(
 CK_SESSION_HANDLE    hSession,          /* session's handle */
 CK_OBJECT_HANDLE_PTR phObject,          /* gets obj. handles */
 CK_ULONG             ulMaxObjectCount,  /* max handles to get */
 CK_ULONG_PTR         pulObjectCount     /* actual # returned */
)
{
    CK_RV rv = CKR_OK;
    P11_Session *session = (P11_Session *)hSession;
    P11_Slot *slot = &st.slots[session->session.slotID - 1];
    CK_ULONG j, objnum;
    CK_BYTE match;

    P11_LOG_START("C_FindObjects");

    thread_MutexLock(st.async_lock);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (INVALID_SESSION)
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (!phObject || !ulMaxObjectCount || !pulObjectCount)
        rv= CKR_ARGUMENTS_BAD;
    else
    {
        objnum = 0;

        log_Log(LOG_LOW, "Find max object count: %lu", ulMaxObjectCount);
        while ((objnum < ulMaxObjectCount) && session->search_object)
        {
            if (!session->search_object->sensitive || (slot->pin_state > 0))
            {
                match = 1;
    
                for (j = 0; j < session->search_attrib_count; j++)
                {
                    if (!object_MatchAttrib(&session->search_attrib[j], session->search_object))
                    {
                        match = 0;
                        break;
                    }
                }
    
                if (match)
                {
                    log_Log(LOG_LOW, "Object matched: %lX", session->search_object);
                    phObject[objnum] = (CK_OBJECT_HANDLE)session->search_object;
                    objnum++;
                }
            }

            session->search_object = session->search_object->next;
        }

        log_Log(LOG_LOW, "Matched %lu objects", objnum);
        *pulObjectCount = objnum;
    }

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_FindObjects");

    return rv;
}


/* C_FindObjectsFinal finishes a search for token and session
 * objects. */
CK_DEFINE_FUNCTION(CK_RV, C_FindObjectsFinal)
(
  CK_SESSION_HANDLE hSession  /* the session's handle */
)
{
    CK_RV rv = CKR_OK;
    P11_Session *session = (P11_Session *)hSession;

    P11_LOG_START("C_FindObjectsFinal");

    thread_MutexLock(st.async_lock);

    if (CKR_ERROR(rv = slot_TokenChanged()))
        rv = CKR_SESSION_HANDLE_INVALID;
    else if (INVALID_SESSION)
        rv = CKR_SESSION_HANDLE_INVALID;
    else
    {
        session->search_object = 0x00;

        if (session->search_attrib)
        {
            free(session->search_attrib);
            session->search_attrib = 0x00;
        }

        session->search_attrib_count = 0x00;
    }

    thread_MutexUnlock(st.async_lock);

    P11_LOG_END("C_FindObjectsFinal");

    return rv;
}

