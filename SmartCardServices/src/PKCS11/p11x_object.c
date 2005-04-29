/******************************************************************************
** 
**  $Id: p11x_object.c,v 1.3 2004/10/14 20:33:36 mb Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: Session & object management functions
** 
******************************************************************************/

#include "cryptoki.h"
#include <openssl/x509.h>

/******************************************************************************
** Function: object_FreeAllObjects
**
** Recursively releases all objects from the given slot.
**
** Parameters:
**  slotID - Slot number to release
**  list   - Object to free (used for recursion)
**
** Returns:
**  none
*******************************************************************************/
void object_FreeAllObjects(CK_SLOT_ID slotID, P11_Object *list)
{
    if (list)
    {
        if (list->next) 
            object_FreeAllObjects(slotID, list->next);
        
        object_FreeObject(slotID, list);
    }
}

/******************************************************************************
** Function: object_FreeObject
**
** Deletes/frees a specific object from a specific slot.  The slot ID is needed
** because it contains the list of all objects.
**
** Parameters:
**  slotID - Slot number that object relates to
**  object - Pointer to object to free
**
** Returns:
**  none
*******************************************************************************/
void object_FreeObject(CK_SLOT_ID slotID, P11_Object *object)
{
    slotID--;

    if (object)
    {
        log_Log(LOG_LOW, "Removing object: %lX", object);

        if (object->prev)
        {
            object->prev->next = object->next;

            if (st.slots[slotID].objects == object)
                st.slots[slotID].objects = object->prev;
        }

        if (object->next)
        {
            object->next->prev = object->prev;

            if (st.slots[slotID].objects == object)
                st.slots[slotID].objects = object->next;
        }

        if (!object->prev && !object->next)
            st.slots[slotID].objects = 0x00;

        if (object->msc_obj)
            free(object->msc_obj);

        if (object->msc_key)
            free(object->msc_key);

        object_FreeAllAttributes(object->attrib);

        memset(object, 0x00, sizeof(P11_Object));

        free(object);
    }
}

/******************************************************************************
** Function: object_FreeAllAttributes
**
** Deletes/frees all attributes relating to a specific object
**
** Parameters:
**  list - attribute to free (used for recursion)
**
** Returns:
**  none
*******************************************************************************/
void object_FreeAllAttributes(P11_Attrib *list)
{
    if (list)
    {
        log_Log(LOG_LOW, "Removing attribute: %lX", list);

        if (list->next) 
            object_FreeAllAttributes(list->next);

        if (list->attrib.pValue)
            free(list->attrib.pValue);

        memset(list, 0x00, sizeof(P11_Attrib));

        free(list);
    }
}

/******************************************************************************
** Function: object_AddObject
**
** Adds an object to specified slot
**
** Parameters:
**  slotID   - Slot number to add object to
**  phObject - Return handle new object
**
** Returns:
**  CKR_HOST_MEMORY on memory alloc error
**  CKR_OK
*******************************************************************************/
CK_RV object_AddObject(CK_SLOT_ID slotID, CK_OBJECT_HANDLE *phObject)
{
    CK_RV rv = CKR_OK;

    *phObject = 0;
    slotID--;

    if (st.slots[slotID].objects)
    {
        if (st.prefs.obj_sort_order == P11_OBJ_SORT_NEWEST_LAST)
        {
            P11_Object *object_l;

            object_l = st.slots[slotID].objects;
            while(object_l->next)
                object_l = object_l->next;
    
            object_l->next = (P11_Object *)calloc(1, sizeof(P11_Object));
    
            if (!object_l->next)
                rv = CKR_HOST_MEMORY;
            else
            {
                object_l->next->prev = object_l;
                object_l->next->check = object_l->next;
                *phObject = (CK_OBJECT_HANDLE)object_l->next;
            }
        }
        else
        {
            st.slots[slotID].objects->prev = (P11_Object *)calloc(1, sizeof(P11_Object));
            if (!st.slots[slotID].objects->prev)
                rv = CKR_HOST_MEMORY;
            else
            {
                st.slots[slotID].objects->prev->next = st.slots[slotID].objects;
                st.slots[slotID].objects = st.slots[slotID].objects->prev;
                st.slots[slotID].objects->check = st.slots[slotID].objects;
                *phObject = (CK_OBJECT_HANDLE)st.slots[slotID].objects;
            }
        }
    }
    else
    {
        st.slots[slotID].objects = (P11_Object *)calloc(1, sizeof(P11_Object));
        if (!st.slots[slotID].objects)
            rv = CKR_HOST_MEMORY;
        else
        {
            st.slots[slotID].objects->check = st.slots[slotID].objects;
            *phObject = (CK_OBJECT_HANDLE)st.slots[slotID].objects;
        }
    }

    return rv;
}

/******************************************************************************
** Function: object_UpdateKeyInfo
**
** Add a new token keyinfo to this session's object list.  Creates a new
** object if neccessary, otherwise updates the existing object.
**
** Parameters:
**  hSession - Session handle
**  hObject  - Returns new or existing object handle
**  pKeyInfo - Sets the object's msc_key property to this
**
** Returns:
**  CKR_HOST_MEMORY on memory alloc error
**  CKR_OK
*******************************************************************************/
CK_RV object_UpdateKeyInfo(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE *hObject, MSCKeyInfo *pKeyInfo)
{
    CK_RV rv = CKR_OK;
    P11_Session *session = (P11_Session *)hSession;
    P11_Object *object_l, *new_obj;

    if (!pKeyInfo)
    {
        new_obj = (P11_Object *)*hObject;
        pKeyInfo = new_obj->msc_key;
    }
    else
    {
        object_l = st.slots[session->session.slotID - 1].objects;
        while (object_l)
        {
            if (object_l->msc_key && (object_l->msc_key->keyNum == pKeyInfo->keyNum))
            {
                log_Log(LOG_LOW, "Found existing key num: %lu", pKeyInfo->keyNum);
                memcpy(object_l->msc_key, pKeyInfo, sizeof(MSCKeyInfo));
                if (hObject) *hObject = (CK_OBJECT_HANDLE)object_l;
                return rv;
            }
    
            object_l = object_l->next;
        }
        
        if (!CKR_ERROR(rv = object_AddObject(session->session.slotID, (CK_OBJECT_HANDLE *)&new_obj)))
        {
            log_Log(LOG_LOW, "New key handle: %X", new_obj);
            new_obj->msc_key = (MSCKeyInfo *)calloc(1, sizeof(MSCKeyInfo));
            if (!new_obj->msc_key)
                rv = CKR_HOST_MEMORY;
            else
            {
                memcpy(new_obj->msc_key, pKeyInfo, sizeof(MSCKeyInfo));
                if (hObject) *hObject = (CK_OBJECT_HANDLE)new_obj;
            }
        }
    }

    if (!CKR_ERROR(rv))
    {
        CK_BYTE key_obj_id[3] = "k ";

        key_obj_id[1] = pKeyInfo->keyNum > 9 ? 65 + pKeyInfo->keyNum : 48 + pKeyInfo->keyNum;  /* Vinnie 1749 A-F (55-65) */

        object_FreeAllAttributes(new_obj->attrib);
        new_obj->attrib = 0;

        (void)CKR_ERROR(rv = object_ReadAttributes(hSession, key_obj_id, new_obj));
    }

    P11_ERR("Done UpdateKeyInfo");

    return rv;
}

/******************************************************************************
** Function: object_UpdateObjectInfo
**
** Add a new token objectinfo to this session's object list.  Creates a new
** object if neccessary, otherwise updates the existing object.
**
** If the object is a certificate then this function does some extra work
** to make sure the public/private match up and have all the attributes they
** need.
**
** Parameters:
**  hSession    - Session handle
**  pObjectInfo - Returns new or existing object handle
**
** Returns:
**  CKR_HOST_MEMORY on memory alloc error
**  CKR_FUNCTION_FAILED on general error (could be failed MSC call)
**  CKR_OK
*******************************************************************************/
CK_RV object_UpdateObjectInfo(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE *hObject, MSCObjectInfo *pObjectInfo)
{
    CK_RV rv = CKR_OK;
    P11_Session *session = (P11_Session *)hSession;
    P11_Slot *slot = &st.slots[session->session.slotID - 1];
    P11_Object *object_l, *new_obj;
    P11_Attrib *attrib, *modulus, *cka_id, *pub_exp;
    CK_ATTRIBUTE ck_attrib;
    CK_OBJECT_CLASS obj_class;
    CK_BYTE t_obj_id[MSC_MAXSIZE_OBJID];
    CK_BYTE *data;

    if (!pObjectInfo)
    {
        new_obj = (P11_Object *)*hObject;
        pObjectInfo = new_obj->msc_obj;
    }
    else
    {
        object_l = slot->objects;
        while (object_l)
        {
            /* Fixme: object ID's are always strings?! */
            if (object_l->msc_obj && (!strncmp(object_l->msc_obj->objectID, pObjectInfo->objectID, MSC_MAXSIZE_OBJID)))
            {
                log_Log(LOG_LOW, "Found existing object ID: %.*s", MSC_MAXSIZE_OBJID, pObjectInfo->objectID);
                memcpy(object_l->msc_obj, pObjectInfo, sizeof(MSCObjectInfo));
                if (hObject) *hObject = (CK_OBJECT_HANDLE)object_l;
                return rv;
            }
    
            object_l = object_l->next;
        }
        
        if (!CKR_ERROR(rv = object_AddObject(session->session.slotID, (CK_OBJECT_HANDLE *)&new_obj)))
        {
            log_Log(LOG_LOW, "New object handle: %X", new_obj);
            new_obj->msc_obj = (MSCObjectInfo *)calloc(1, sizeof(MSCObjectInfo));
            if (!new_obj->msc_obj)
                rv = CKR_HOST_MEMORY;
            else
            {
                memcpy(new_obj->msc_obj, pObjectInfo, sizeof(MSCObjectInfo));
                if (hObject) *hObject = (CK_OBJECT_HANDLE)new_obj;
            }
        }
    }

    if (!CKR_ERROR(rv))
    {
        object_FreeAllAttributes(new_obj->attrib);
        new_obj->attrib = 0;

        data = (CK_BYTE *)malloc(pObjectInfo->objectSize);

        if (!data)
            rv = CKR_HOST_MEMORY;
        else if (MSC_ERROR(msc_ReadObject(&slot->conn, 
                                     pObjectInfo->objectID,
                                     0, 
                                     data, 
                                     pObjectInfo->objectSize)))
            rv = CKR_FUNCTION_FAILED;
        else
            (void)CKR_ERROR(rv = object_AddAttribute(new_obj, CKA_VALUE, FALSE, data, pObjectInfo->objectSize, &attrib));

        if (data)
            free(data);

        strncpy((char *)t_obj_id, pObjectInfo->objectID, sizeof(t_obj_id));
        t_obj_id[0] = tolower(t_obj_id[0]);

        if (!CKR_ERROR(rv = object_ReadAttributes(hSession, t_obj_id, new_obj)))
        {
            rv = object_InferClassAttributes(hSession, new_obj);
        }

        obj_class = CKO_CERTIFICATE;
        ck_attrib.type = CKA_CLASS;
        ck_attrib.pValue = &obj_class;
        ck_attrib.ulValueLen = 4;

        if (object_MatchAttrib(&ck_attrib, new_obj))
        {
            if (!CKR_ERROR_NOLOG(object_GetAttrib(CKA_MODULUS, new_obj, &modulus)) &&
                !CKR_ERROR_NOLOG(object_GetAttrib(CKA_PUBLIC_EXPONENT, new_obj, &pub_exp)) &&
                !CKR_ERROR_NOLOG(object_GetAttrib(CKA_ID, new_obj, &cka_id)))
            {
                obj_class = CKO_PUBLIC_KEY;
    
                object_l = slot->objects;
                while (object_l)
                {
                    if (object_MatchAttrib(&cka_id->attrib, object_l) &&
                        object_MatchAttrib(&ck_attrib, object_l))
                    {
                        log_Log(LOG_LOW, "Found existing public key key match.");
                        break;
                    }
    
                    object_l = object_l->next;
                }

                /* object_l will be null if we didn't find an existing public key */
                if (!object_l &&
                    !CKR_ERROR(rv = object_AddObject(session->session.slotID, (CK_OBJECT_HANDLE *)&object_l)))
                {
                    log_Log(LOG_LOW, "Added non-token public key object");

                    obj_class = CKO_PUBLIC_KEY;
                    (void)CKR_ERROR(object_AddAttribute(object_l,
                                           CKA_CLASS,
                                           FALSE,
                                           (CK_BYTE *)ck_attrib.pValue,
                                           ck_attrib.ulValueLen, 0));

                    (void)CKR_ERROR(object_AddAttribute(object_l,
                                           CKA_MODULUS,
                                           FALSE,
                                           (CK_BYTE *)modulus->attrib.pValue,
                                           modulus->attrib.ulValueLen, 0));
        
                    (void)CKR_ERROR(object_AddAttribute(object_l,
                                           CKA_PUBLIC_EXPONENT,
                                           FALSE,
                                           (CK_BYTE *)pub_exp->attrib.pValue,
                                           pub_exp->attrib.ulValueLen, 0));

                    (void)CKR_ERROR(object_AddAttribute(object_l,
                                           CKA_ID,
                                           FALSE,
                                           (CK_BYTE *)cka_id->attrib.pValue,
                                           cka_id->attrib.ulValueLen, 0));
                }
            }
        }

        /* Fixme: move this up into the previous if's code block? */
        obj_class = CKO_CERTIFICATE;
        ck_attrib.type = CKA_CLASS;
        ck_attrib.pValue = &obj_class;
        ck_attrib.ulValueLen = 4;

        if (object_MatchAttrib(&ck_attrib, new_obj))
        {
            if (!CKR_ERROR_NOLOG(object_GetAttrib(CKA_MODULUS, new_obj, &modulus)))
            {
                if (!CKR_ERROR_NOLOG(object_GetAttrib(CKA_ID, new_obj, &cka_id)))
                {
                    obj_class = CKO_PRIVATE_KEY;

                    object_l = slot->objects;
                    while (object_l)
                    {
                        if (object_MatchAttrib(&cka_id->attrib, object_l) &&
                            object_MatchAttrib(&ck_attrib, object_l))
                        {
                            log_Log(LOG_LOW, "Found private key match.  Added modulus to key.");
                            (void)CKR_ERROR(object_AddAttribute(object_l,
                                                                CKA_MODULUS, 
                                                                FALSE, 
                                                                (CK_BYTE *)modulus->attrib.pValue,
                                                                modulus->attrib.ulValueLen, 0));
                            break;
                        }

                        object_l = object_l->next;
                    }
                }
            }
        }
    }

    return rv;
}

/******************************************************************************
** Function: object_FreeTokenObjects
**
** This only deletes/frees objects that are token objects (objects stored on
** the token).
**
** Fixme: This function is not used?
**
** Parameters:
**  slotID - Slot number to free objects on
**
** Returns:
**  CKR_OK
*******************************************************************************/
CK_RV object_FreeTokenObjects(CK_SLOT_ID slotID)
{
    CK_RV rv = CKR_OK;
    P11_Object *object_l;

    object_l = st.slots[slotID - 1].objects;

    while (object_l)
    {
        if (object_l->msc_obj || object_l->msc_key)
            object_FreeObject(slotID, object_l);

        object_l = st.slots[slotID - 1].objects;
    }

    return rv;
}

/******************************************************************************
** Function: object_GetAttrib
**
** Gets an attribute from an object
**
** Parameters:
**  type   - Attribute type to get (like CKA_ID)
**  object - Object to search for attribute
**  attrib - Returns the found attribute 
**
** Returns:
**  CKR_ATTRIBUTE_TYPE_INVALID if attribute was not found
**  CKR_OK if attribute was found
*******************************************************************************/
CK_RV object_GetAttrib(CK_ATTRIBUTE_TYPE type, P11_Object *object, P11_Attrib **attrib)
{
    CK_RV rv = CKR_ATTRIBUTE_TYPE_INVALID;
    P11_Attrib *attrib_l;

    attrib_l = object->attrib;
    while (attrib_l)
    {
        if (type == attrib_l->attrib.type)
        {
            rv = CKR_OK;
            if (attrib)
                *attrib = attrib_l;
            break;
        }

        attrib_l = attrib_l->next;
    }

    return rv;
}

/******************************************************************************
** Function: object_SetAttrib
**
** Set an attribute on an object (or adds a new one if it doesn't exist).
** This function is the same as object_AddAttribute.
**
** Fixme: this function is not used?
**
** Parameters:
**  object - Object to add attribute to
**  attrib - Attribute to add
**
** Returns:
**  Error from object_AddAttribute
**  CKR_OK
*******************************************************************************/
CK_RV object_SetAttrib(P11_Object *object, CK_ATTRIBUTE *attrib)
{
    CK_RV rv = CKR_OK;
    P11_Attrib *attrib_new;

    (void)CKR_ERROR(rv = object_AddAttribute(object, 
                                             attrib->type, 
                                             TRUE,
                                             (CK_BYTE *)attrib->pValue, 
                                             attrib->ulValueLen, 
                                             &attrib_new));

    return rv;
}

/******************************************************************************
** Function: object_AddAttribute
**
** Adds a new attribute to an object or replaces an existing attribute if it
** already exists.
**
** Parameters:
**  object    - Object to add attribute to
**  type      - Attribute type (CKA_ID, etc.)
**  token     - TRUE/FALSE stating whether this attribute will be stored on the
**              token
**  value     - Value of attribute (pValue)
**  value_len - Length of value (ulValueLen)
**  attrib    - Pointer to new attribute object
**
** Returns:
**  none
*******************************************************************************/
CK_RV object_AddAttribute(P11_Object *object, CK_ATTRIBUTE_TYPE type, CK_BBOOL token, CK_BYTE *value, CK_ULONG value_len, P11_Attrib **attrib)
{
    CK_RV rv = CKR_OK;
    P11_Attrib *t_attrib = 0;

    if (object->attrib)
    {
        if (object_GetAttrib(type, object, &t_attrib) != CKR_OK)
        {
            t_attrib = (P11_Attrib *)calloc(1, sizeof(P11_Attrib));

            if (!t_attrib)
                rv = CKR_HOST_MEMORY;
            else
            {
                object->attrib->prev = t_attrib;
                t_attrib->next = object->attrib;
                object->attrib = t_attrib;
            }
        }
    }
    else
    {
        t_attrib = (P11_Attrib *)calloc(1, sizeof(P11_Attrib));

        if (!t_attrib)
            rv = CKR_HOST_MEMORY;
        else
            object->attrib = t_attrib;
    }

    if (t_attrib)
    {
        if (t_attrib->attrib.pValue)
            free(t_attrib->attrib.pValue);

        t_attrib->token = token;
        t_attrib->attrib.type = type;
        t_attrib->attrib.ulValueLen = value_len;

        if (value_len)
            t_attrib->attrib.pValue = calloc(1, value_len);
        else
            t_attrib->attrib.pValue = 0;

        if (value_len && !t_attrib->attrib.pValue)
            rv = CKR_HOST_MEMORY; // Fixme: returns error, but has already added the attribute
        else if (value)
            memcpy(t_attrib->attrib.pValue, value, value_len);
    }

    if (attrib)
        *attrib = t_attrib;

    return rv;
}

/******************************************************************************
** Function: object_AddBoolAttribute
**
** Shortcut to add a simple true/false bool attribute to an object.
**
** Fixme: do the parameters need to be fixed? "object" should come first
**
** Parameters:
**  type   - Attribute type (CKA_ID, etc.)
**  value  - True or false
**  object - Object to add attribute to
**
** Returns:
**  Error from object_AddAttribute
**  CKR_OK;
*******************************************************************************/
CK_RV object_AddBoolAttribute(CK_ATTRIBUTE_TYPE type, CK_BBOOL value, P11_Object *object)
{
    CK_RV rv = CKR_OK;
    P11_Attrib *attrib;

    /* Fixme: all bool attributes are stored on the token? */
    (void)CKR_ERROR(rv = object_AddAttribute(object, type, TRUE, (CK_BYTE *)&value, sizeof(CK_BBOOL), &attrib));

    return rv;
}

/******************************************************************************
** Function: object_MatchAttrib
**
** Finds an attribute (by value) of an object
**
** Parameters:
**  attrib - Attrib to match
**  object - Object to use
**
** Returns:
**  CKR_HOST_MEMORY on memory alloc error
**  True/False if attribute matched or not
*******************************************************************************/
CK_RV object_MatchAttrib(CK_ATTRIBUTE *attrib, P11_Object *object)
{
    CK_RV rv;
    P11_Attrib *obj_attrib;
    CK_BYTE *reverse, *forward, temp;
    CK_ULONG i;

    object_LogAttribute(attrib);

    reverse = (CK_BYTE *)malloc(attrib->ulValueLen);

    if (!reverse)
        rv = CKR_HOST_MEMORY;
    else
    {
        forward = (CK_BYTE *)attrib->pValue;

        memcpy(reverse, forward, attrib->ulValueLen);

        for (i = 0; i < (CK_ULONG)(attrib->ulValueLen / 2); i++)
        {
            temp = reverse[i];
            reverse[i] = reverse[attrib->ulValueLen - i - 1];
            reverse[attrib->ulValueLen - i - 1] = temp;
        }

        log_Log(LOG_LOW, "Match attribute type: 0x%lX", attrib->type);

        if (!CKR_ERROR(rv = object_GetAttrib(attrib->type, object, &obj_attrib)) &&
            (!memcmp(forward, obj_attrib->attrib.pValue, attrib->ulValueLen) ||
             !memcmp(reverse, obj_attrib->attrib.pValue, attrib->ulValueLen)))
        {
            rv = 1;
        }
        else
        {
            { CK_BYTE buf[4096]; 
              object_BinToHex((CK_BYTE *)forward, attrib->ulValueLen, buf); 
              log_Log(LOG_LOW, "Orig:%s", buf);
              object_BinToHex((CK_BYTE *)reverse, attrib->ulValueLen, buf); 
              log_Log(LOG_LOW, " Rev:%s", buf);
    
              if (rv == CKR_OK)
                { object_BinToHex((CK_BYTE *)obj_attrib->attrib.pValue, obj_attrib->attrib.ulValueLen, buf); 
                  log_Log(LOG_LOW, " Obj:%s", buf); } }
    
            rv = 0;
        }

        free(reverse);
    }

    return rv;
}

/******************************************************************************
** Function: object_TemplateGetAttrib
**
** Finds an attribute inside a template attribute list.  This works with
** CK_ATTRIBUTE instead of a P11_Attrib and does not match by value, only
** attribute type.
**
** Parameters:
**  type         - Attribute type (CKA_ID, etc.)
**  attrib       - Attribute list
**  attrib_count - Number of elements in attribute list
**  attrib_out   - Returns matched attribute
**
** Returns:
**  CKR_ATTRIBUTE_TYPE_INVALID if attribute was not found
**  CKR_OK
*******************************************************************************/
CK_RV object_TemplateGetAttrib(CK_ATTRIBUTE_TYPE type, 
                               CK_ATTRIBUTE *attrib, 
                               CK_ULONG attrib_count, 
                               CK_ATTRIBUTE **attrib_out)
{
    CK_RV rv = CKR_ATTRIBUTE_TYPE_INVALID;
    CK_ULONG i;

    for (i = 0; i < attrib_count; i++)
    {
        if (attrib[i].type == type)
        {
            if (attrib_out)
                *attrib_out = &attrib[i];

            rv = CKR_OK;
            break;
        }
    }

    return rv;
}

/******************************************************************************
** Function: object_RSAGenKeyPair
**
** Generates an RSA key pair on the token
**
** Parameters:
**  hSession - Session handle
**  pPublicKeyTemplate - Public key template
**  ulPublicKeyAttributeCount - Public key template attribute count
**  pPrivateKeyTemplate - Private key template
**  ulPrivateKeyAttributeCount - Private key template attribute count
**  phPublicKey - Pointer to new public key object
**  phPrivateKey - Pointer to new private key object
**
** Returns:
**  CKR_FUNCTION_FAILED on general error
**  CKR_OK
*******************************************************************************/
CK_RV object_RSAGenKeyPair(CK_SESSION_HANDLE hSession,
                           CK_ATTRIBUTE *pPublicKeyTemplate,
                           CK_ULONG ulPublicKeyAttributeCount,
                           CK_ATTRIBUTE *pPrivateKeyTemplate,
                           CK_ULONG ulPrivateKeyAttributeCount,
                           CK_OBJECT_HANDLE *phPublicKey,
                           CK_OBJECT_HANDLE *phPrivateKey)
{
    CK_RV rv = CKR_OK;
    MSC_RV msc_rv;
    P11_Session *session = (P11_Session *)hSession;
    P11_Slot *slot = &st.slots[session->session.slotID - 1];
    P11_Object *objectPub;
    P11_Object *objectPrv;
    CK_ATTRIBUTE *attrib;
    P11_Attrib *p11_attrib;
    MSCKeyInfo keyInfo;
    MSCKeyInfo keyInfoPub;
    MSCKeyInfo keyInfoPrv;
    CK_BYTE keyNum = 0;
    CK_ULONG keySize = 0;
    MSCGenKeyParams params;

    (void)CKR_ERROR(object_TemplateGetAttrib(CKA_MODULUS_BITS, 
                                             pPublicKeyTemplate, 
                                             ulPublicKeyAttributeCount, 
                                             &attrib));
    memcpy(&keySize, attrib->pValue, attrib->ulValueLen);

    msc_rv = msc_ListKeys(&slot->conn, MSC_SEQUENCE_RESET, &keyInfo);
    if (!MSC_ERROR(msc_rv))
        keyNum = keyInfo.keyNum + 1;
    while (!MSC_ERROR(msc_rv) && !CKR_ERROR(rv))
    {
        msc_rv = msc_ListKeys(&slot->conn, MSC_SEQUENCE_NEXT, &keyInfo);

        if (!MSC_ERROR(msc_rv))
            keyNum = keyInfo.keyNum + 1;
    }

    log_Log(LOG_LOW, "KeySize: %lu", keySize);
    log_Log(LOG_LOW, "KeyNum: %lu", keyNum);

    /* Fixme: check capabilities for CRT */

    params.algoType = MSC_GEN_ALG_RSA_CRT;
    params.keySize = (MSCUShort16)keySize;
    params.privateKeyACL.readPermission = MSC_AUT_NONE;
    params.privateKeyACL.writePermission = (MSCUShort16)object_MapPIN(st.prefs.user_pin_num);
    params.privateKeyACL.usePermission = (MSCUShort16)object_MapPIN(st.prefs.user_pin_num);
    params.publicKeyACL.readPermission = MSC_AUT_ALL;
    params.publicKeyACL.writePermission = (MSCUShort16)object_MapPIN(st.prefs.user_pin_num);
    params.publicKeyACL.usePermission = MSC_AUT_ALL;
    params.privateKeyPolicy.cipherDirection = MSC_KEYPOLICY_DIR_SIGN | MSC_KEYPOLICY_DIR_DECRYPT;
    params.privateKeyPolicy.cipherMode = MSC_KEYPOLICY_MODE_RSA_NOPAD;
    params.publicKeyPolicy.cipherDirection = MSC_KEYPOLICY_DIR_VERIFY | MSC_KEYPOLICY_DIR_ENCRYPT;
    params.publicKeyPolicy.cipherMode = MSC_KEYPOLICY_MODE_RSA_NOPAD;
    params.keyGenOptions = MSC_OPT_DEFAULT;
    params.pOptParams = 0;
    params.optParamsSize = 0;

    if (MSC_ERROR(msc_GenerateKeys(&slot->conn, keyNum, (MSCUChar8)(keyNum + 1), &params)))
        rv = CKR_FUNCTION_FAILED;
    else
    {

        (void)MSC_ERROR(msc_GetKeyAttributes(&slot->conn, (MSCUChar8)(keyNum + 1), &keyInfoPub));
        (void)MSC_ERROR(msc_GetKeyAttributes(&slot->conn, keyNum, &keyInfoPrv));

        log_Log(LOG_LOW, "keyNums: %lu and %lu", keyNum, keyNum + 1);

        (void)CKR_ERROR(object_UpdateKeyInfo(hSession, (CK_OBJECT_HANDLE *)&objectPub, &keyInfoPub));
        (void)CKR_ERROR(object_UpdateKeyInfo(hSession, (CK_OBJECT_HANDLE *)&objectPrv, &keyInfoPrv));

        *phPublicKey = (CK_OBJECT_HANDLE)objectPub;
        *phPrivateKey = (CK_OBJECT_HANDLE)objectPrv;

        (void)CKR_ERROR(object_GetAttrib(CKA_MODULUS, objectPub, &p11_attrib));
        (void)CKR_ERROR(object_AddAttribute(objectPrv,
                                            p11_attrib->attrib.type,
                                            TRUE, /* Fixme: Always a token attribute? */
                                            (CK_BYTE *)p11_attrib->attrib.pValue,
                                            p11_attrib->attrib.ulValueLen, 0));

    }

    return rv;
}

/******************************************************************************
** Function: object_LogObjects
**
** Logs all objects and attributes for specified slot
**
** Parameters:
**  slotID - Slot number
**
** Returns:
**  none
*******************************************************************************/
void object_LogObjects(CK_SLOT_ID slotID)
{
    P11_Object *list;

    list = st.slots[slotID - 1].objects;

    while(list)
    {

        object_LogObject(list);

        list = list->next;
    }
}

/******************************************************************************
** Function: object_LogObject
**
** Logs a single object and its attributes
**
** Parameters:
**  object - Object to log
**
** Returns:
**  none
*******************************************************************************/
void object_LogObject(P11_Object *object)
{
    P11_Attrib *attrib;

    log_Log(LOG_LOW, "=== Object handle: %X", object);

    if (object->msc_obj)
        log_Log(LOG_LOW, "--- Object Object: %s  Size: %lu", 
                        object->msc_obj->objectID, object->msc_obj->objectSize);
    else if (object->msc_key)
        log_Log(LOG_LOW, "--- Key Object: %lu  Size: %lu", 
                        object->msc_key->keyNum, object->msc_key->keySize);

    attrib = object->attrib;

    while(attrib)
    {
        object_LogAttribute(&attrib->attrib);
        attrib = attrib->next;
    }
}

/******************************************************************************
** Function: object_LogAttribute
**
** Write out a single attribute to the log file
**
** Parameters:
**  attrib - Attribute to log
**
** Returns:
**  none
*******************************************************************************/
void object_LogAttribute(CK_ATTRIBUTE *attrib)
{
    CK_ULONG ulValue = 0;
    CK_BYTE bValue = 0;
    CK_BYTE *buf;

    buf = (CK_BYTE *)malloc((attrib->ulValueLen * 3) + 1);
    if (!buf)
    {
        log_Log(LOG_HIGH, "Memory alloc failed");
        return;
    }

    if (attrib->pValue)
    {
        memcpy(&ulValue, attrib->pValue, 4);
        memcpy(&bValue, attrib->pValue, 1);
        object_BinToHex((CK_BYTE *)attrib->pValue, attrib->ulValueLen, buf);

        switch (attrib->type)
        {
        case CKA_ID:
            log_Log(LOG_LOW, "CKA_ID:%s", buf);
            break;
        case CKA_SERIAL_NUMBER:
            log_Log(LOG_LOW, "CKA_SERIAL_NUMBER:%s", buf);
            break;
        case CKA_SUBJECT:
            log_Log(LOG_LOW, "CKA_SUBJECT:%s", buf);
            break;
        case CKA_ISSUER:
            log_Log(LOG_LOW, "CKA_ISSUER:%s", buf);
            break;
        case CKA_MODULUS:
            log_Log(LOG_LOW, "CKA_MODULUS:%s", buf);
            break;
        case CKA_CLASS:
            log_Log(LOG_LOW, "CKA_CLASS: %lu 0x%X", ulValue, ulValue);
            break;
        case CKA_KEY_TYPE:
            log_Log(LOG_LOW, "CKA_KEY_TYPE: %lu 0x%X", ulValue, ulValue);
            break;
        case CKA_TOKEN: 
            log_Log(LOG_LOW, "CKA_TOKEN: %lu 0x%X", bValue, bValue);
            break;
        case CKA_LABEL:
            log_Log(LOG_LOW, "CKA_LABEL:%s", buf);
            log_Log(LOG_LOW, "CKA_LABEL (string): %.*s", attrib->ulValueLen, attrib->pValue);
            break;
        case CKA_VALUE:
            log_Log(LOG_LOW, "CKA_VALUE:%s", buf);
            break;
        case CKA_PUBLIC_EXPONENT:
            log_Log(LOG_LOW, "CKA_PUBLIC_EXPONENT:%s", buf);
            break;
        case CKA_CERTIFICATE_TYPE:
            log_Log(LOG_LOW, "CKA_CERTIFICATE_TYPE:%s", buf);
            break;
        case CKA_EXTRACTABLE:
            log_Log(LOG_LOW, "CKA_EXTRACTABLE:%s", buf);
            break;
        case CKA_SIGN_RECOVER:
            log_Log(LOG_LOW, "CKA_SIGN_RECOVER:%s", buf);
            break;
        case CKA_DERIVE:
            log_Log(LOG_LOW, "CKA_DERIVE:%s", buf);
            break;
        case CKA_MODIFIABLE:
            log_Log(LOG_LOW, "CKA_MODIFIABLE:%s", buf);
            break;
        case CKA_UNWRAP:
            log_Log(LOG_LOW, "CKA_UNWRAP:%s", buf);
            break;
        case CKA_DECRYPT:
            log_Log(LOG_LOW, "CKA_DECRYPT:%s", buf);
            break;
        case CKA_PRIVATE:
            log_Log(LOG_LOW, "CKA_PRIVATE:%s", buf);
            break;
        case CKA_SIGN:
            log_Log(LOG_LOW, "CKA_SIGN:%s", buf);
            break;
        case CKA_NEVER_EXTRACTABLE:
            log_Log(LOG_LOW, "CKA_NEVER_EXTRACTABLE:%s", buf);
            break;
        case CKA_ALWAYS_SENSITIVE:
            log_Log(LOG_LOW, "CKA_CKA_ALWAYS_SENSITIVE:%s", buf);
            break;
        case CKA_SENSITIVE:
            log_Log(LOG_LOW, "CKA_SENSITIVE:%s", buf);
            break;
        default:
            log_Log(LOG_LOW, "CKA_UNKNOWN (0x%lX):%s", attrib->type, buf);
            break;
        }
    }

    free(buf);
}

/******************************************************************************
** Function: object_BinToHex
**
** Converts a binary array to hex text
**
** Parameters:
**  data     - Input data
**  data_len - Input data len
**  out      - Output text data
**
** Returns:
**  none
*******************************************************************************/
void object_BinToHex(CK_BYTE *data, CK_ULONG data_len, CK_BYTE *out)
{
    CK_ULONG i;

    for (i = 0; i < data_len; i++)
        sprintf((char *)&out[i * 3], " %.2X", data[i]);

    out[data_len * 3] = 0;
}

/******************************************************************************
** Function: object_AddAttributes
**
** Used by object_ReadAttributes when reading an object's attribute file.
** Handles endian conversion of types.
**
** Parameters:
**  object - Object to add attributes to
**  data   - Raw data to be processed
**  len    - Length of raw data
**
** Returns:
**  Result of object_AddAttribute
**  CKR_OK
*******************************************************************************/
CK_RV object_AddAttributes(P11_Object *object, CK_BYTE *data, CK_ULONG len)
{
    CK_RV rv = CKR_OK;
    CK_ULONG i;
    P11_Attrib *attrib;
    CK_ATTRIBUTE_TYPE type;
    CK_ULONG data_len;

    for (i = 0; (i < len) && !CKR_ERROR(rv);)
    {
        type = (data[i] * 0x1000000) + 
               (data[i+1] * 0x10000) + 
               (data[i+2] * 0x100) + 
               data[i+3];

        data_len = (data[i+4] * 0x100) + data[i+5];

        (void)CKR_ERROR(rv = object_AddAttribute(object, type, TRUE, &data[i + 6], data_len, &attrib));

        log_Log(LOG_LOW, "object_AddAttributes:");
        object_LogAttribute(&attrib->attrib);
   
        i += 6 + attrib->attrib.ulValueLen;
    }

    return rv;
}

/******************************************************************************
** Function: object_ReadAttributes
**
** Reads the attributes (from attribute file) of an object
**
** Parameters:
**  hSession - Session handle
**  obj_id   - Object ID of attribute file (string)
**  object   - Object to add attributes to
**
** Returns:
**  CKR_HOST_MEMORY on memory alloc error
**  Error from object_AddAttributes
**  CKR_OK
*******************************************************************************/
CK_RV object_ReadAttributes(CK_SESSION_HANDLE hSession, CK_BYTE *obj_id, P11_Object *object)
{
    CK_RV rv = CKR_OK;
    CK_RV msc_rv;
    P11_Session *session = (P11_Session *)hSession;
    P11_Slot *slot = &st.slots[session->session.slotID - 1];
    CK_ULONG data_len;
    CK_BYTE *data;
    CK_BYTE data_hdr[7];
        
    (void)CKR_ERROR(rv = object_AddBoolAttribute(CKA_TOKEN, 1, object));

    msc_rv = msc_ReadObject(&slot->conn, (char *)obj_id, 0, data_hdr, sizeof(data_hdr));

    if (MSC_ERROR(msc_rv) && (msc_rv != MSC_UNAUTHORIZED))
    {
InferAttributes:
        if (CKR_ERROR(object_InferAttributes(hSession, object)))
            rv = CKR_FUNCTION_FAILED;
        else
            (void)CKR_ERROR(object_WriteAttributes(hSession, object));
    }
    else if (msc_rv == MSC_UNAUTHORIZED)
    {
        object->sensitive = 1;
    }
    else
    {
        { CK_BYTE buf[(sizeof(data_hdr) * 3) + 1];
          object_BinToHex(data_hdr, sizeof(data_hdr), buf);
          log_Log(LOG_LOW, "Raw attribute header (file %s): %s", obj_id, buf); }

        data_len = (data_hdr[5] * 0x100) + data_hdr[6];
        data = (CK_BYTE *)malloc(data_len);

        log_Log(LOG_LOW, "Going to read %lu (0x%lX) bytes of data at offset %lX", data_len, data_len, sizeof(data_hdr));

        if (!data)
            rv = CKR_HOST_MEMORY;
        else if (MSC_ERROR(msc_ReadObject(&slot->conn,
                                          (char *)obj_id, 
                                          sizeof(data_hdr), 
                                          data, 
                                          data_len)))
        {
            P11_ERR("Reading of attribute object failed");
            goto InferAttributes; /* Fixme: may want to remove the goto; this is fairly clean, if hard to follow */
        }
        else
        {
            { CK_BYTE *buf;
              buf = (CK_BYTE *)malloc((data_len * 3) + 1);
              object_BinToHex(data, data_len, buf);
              log_Log(LOG_LOW, "Raw attribute file: %s", buf);
              free(buf); }

            (void)CKR_ERROR(rv = object_AddAttributes(object, data, data_len));
         }

        if (data)
            free(data);
    }

    object_LogObject(object);

    return rv;
}

/******************************************************************************
** Function: object_InferAttributes
**
** Depending on the type of object this adds some default values of attributes
** or pulls out values that it can and adds them as attributes.
**
** Parameters:
**  hSession - Session handle
**  object   - Object to infer attributes on
**
** Returns:
**  CKR_FUNCTION_FAILED if a non-token object is specified
**  CKR_OK
*******************************************************************************/
CK_RV object_InferAttributes(CK_SESSION_HANDLE hSession, P11_Object *object)
{
    CK_RV rv = CKR_OK;

    if (object->msc_key)
        object_InferKeyAttributes(hSession, object);
    else if (object->msc_obj)
        object_InferObjAttributes(hSession, object);
    else
    {
        P11_ERR("Invalid non-token object");
        rv = CKR_FUNCTION_FAILED;
    }

    return rv;
}

/******************************************************************************
** Function: object_InferKeyAttributes
**
** Adds some default values of attributes or pulls out values that it can and
** adds them as attributes for this key.
**
** Parameters:
**  hSession - Session handle
**  object   - Object to infer attributes on
**
** Returns:
**  Error from object_AddAttribute
**  CKR_OK
*******************************************************************************/
CK_RV object_InferKeyAttributes(CK_SESSION_HANDLE hSession, P11_Object *object)
{
    CK_RV rv = CKR_OK;
    P11_Session *session = (P11_Session *)hSession;
    P11_Slot *slot = &st.slots[session->session.slotID - 1];
    P11_Attrib *attrib, *modulus;
    CK_OBJECT_CLASS obj_class;
    CK_KEY_TYPE key_type;

    switch (object->msc_key->keyType)
    {
    case MSC_KEY_RSA_PUBLIC:
    case MSC_KEY_DSA_PUBLIC:
        obj_class = CKO_PUBLIC_KEY;
        break;
    case MSC_KEY_RSA_PRIVATE:
    case MSC_KEY_RSA_PRIVATE_CRT:
    case MSC_KEY_DSA_PRIVATE:
        obj_class = CKO_PRIVATE_KEY;
        break;
    case MSC_KEY_DES:
    case MSC_KEY_3DES:
    case MSC_KEY_3DES3:
        obj_class = CKO_SECRET_KEY; /* Fixme: Secret key??? */
        break;
    default:
        obj_class = CKO_DATA;
        break;
    }

    switch (object->msc_key->keyType)
    {
    case MSC_KEY_DSA_PUBLIC:
    case MSC_KEY_DSA_PRIVATE:
        key_type = CKK_DSA;
        break;
    case MSC_KEY_RSA_PUBLIC:
    case MSC_KEY_RSA_PRIVATE:
    case MSC_KEY_RSA_PRIVATE_CRT:
        key_type = CKK_RSA;
        break;
    case MSC_KEY_DES:
        key_type = CKK_DES;
        break;
    case MSC_KEY_3DES:
        key_type = CKK_DES2;
        break;
    case MSC_KEY_3DES3:
        key_type = CKK_DES3;
        break;
    default:
        key_type = CKK_VENDOR_DEFINED;
        break;
    }

    /* Fixme: some of these are ULONG values?  Need to convert to big-endian? */
    (void)CKR_ERROR(rv = object_AddAttribute(object, CKA_CLASS, TRUE, (CK_BYTE *)&obj_class, sizeof(CK_OBJECT_CLASS), &attrib));
    (void)CKR_ERROR(rv = object_AddAttribute(object, CKA_KEY_TYPE, TRUE, (CK_BYTE *)&key_type, sizeof(CK_KEY_TYPE), &attrib));

    if ((object->msc_key->keyType == MSC_KEY_RSA_PRIVATE) ||
        (object->msc_key->keyType == MSC_KEY_RSA_PRIVATE_CRT) ||
        (object->msc_key->keyType == MSC_KEY_DSA_PRIVATE))
    {
        (void)CKR_ERROR(rv = object_AddBoolAttribute(CKA_SENSITIVE, 1, object));
        (void)CKR_ERROR(rv = object_AddBoolAttribute(CKA_ALWAYS_SENSITIVE, 1, object));
        (void)CKR_ERROR(rv = object_AddBoolAttribute(CKA_NEVER_EXTRACTABLE, 1, object));
        (void)CKR_ERROR(rv = object_AddBoolAttribute(CKA_SIGN, 1, object));
        (void)CKR_ERROR(rv = object_AddBoolAttribute(CKA_PRIVATE, 1, object));
        (void)CKR_ERROR(rv = object_AddBoolAttribute(CKA_DECRYPT, 1, object));
        (void)CKR_ERROR(rv = object_AddBoolAttribute(CKA_UNWRAP, 1, object));

        (void)CKR_ERROR(rv = object_AddBoolAttribute(CKA_MODIFIABLE, 0, object));
        (void)CKR_ERROR(rv = object_AddBoolAttribute(CKA_DERIVE, 0, object));
        (void)CKR_ERROR(rv = object_AddBoolAttribute(CKA_SIGN_RECOVER, 0, object));
        (void)CKR_ERROR(rv = object_AddBoolAttribute(CKA_EXTRACTABLE, 0, object));
    }

    if ((object->msc_key->keyType == MSC_KEY_RSA_PUBLIC) ||
        (object->msc_key->keyType == MSC_KEY_DSA_PUBLIC))
    {
        MSCUChar8 keyBlob[4096]; // Fixme: don't hardcode this
        MSCULong32 keyBlobSize = sizeof(keyBlob);
        CK_RV msc_rv;

        if (!MSC_ERROR(msc_rv = msc_ExportKey(&slot->conn,
                                              object->msc_key->keyNum,
                                              keyBlob,
                                              &keyBlobSize,
                                              0, 0)))
        {
            (void)CKR_ERROR(rv = object_AddAttribute(object, 
                                                     CKA_MODULUS, 
                                                     TRUE, 
                                                     &keyBlob[6], 
                                                     (keyBlob[4] * 0x100) + keyBlob[5], &modulus));

            (void)CKR_ERROR(rv = object_AddAttribute(object, 
                                   CKA_PUBLIC_EXPONENT, 
                                   TRUE,
                                   &keyBlob[modulus->attrib.ulValueLen + 8], 
                                   (keyBlob[modulus->attrib.ulValueLen + 6] * 255) + keyBlob[modulus->attrib.ulValueLen + 7], 
                                   &attrib));
        }
    }

    if (!CKR_ERROR(rv = object_AddAttribute(object, CKA_ID, TRUE, 0, 20, &attrib)))
    {
        char t_id[21];

        sprintf(t_id, "KEY%.17u", object->msc_key->keyNum);
        memcpy(attrib->attrib.pValue, t_id, 20);
    }

    return rv;
}

/******************************************************************************
** Function: object_InferObjAttributes
**
** Adds some default values of attributes or pulls out values that it can and
** adds them as attributes for this object.
**
** Parameters:
**  hSession - Session handle
**  object   - Object to infer attributes on
**
** Returns:
**  CKR_OK
*******************************************************************************/
CK_RV object_InferObjAttributes(CK_SESSION_HANDLE hSession, P11_Object *object)
{
    CK_RV rv = CKR_OK;

    return rv;
}

/******************************************************************************
** Function: object_WriteAttributes
**
** Writes attributes of object out to attribute file.  This function is big
** because it handles keys, certs, and other objects along with creating them
** if necessary.  This could be split into smaller pieces.
**
** Parameters:
**  hSession - Session handle
**  object   - Object to get attributes from
**
** Returns:
**  CKR_HOST_MEMORY on memory alloc erro
**  CKR_FUNCTION_FAILED on general error
**  CKR_OK
*******************************************************************************/
CK_RV object_WriteAttributes(CK_SESSION_HANDLE hSession, P11_Object *object)
{
    CK_RV rv = CKR_OK;
    MSC_RV msc_rv;
    P11_Session *session = (P11_Session *)hSession;
    P11_Slot *slot = &st.slots[session->session.slotID - 1];
    P11_Attrib *attrib;
    char obj_id[MSC_MAXSIZE_OBJID];
    MSCObjectACL objACL;
    CK_ULONG obj_size;
    CK_BYTE data_hdr[7];
    CK_BYTE *data;
    CK_ULONG data_pos;
    CK_ATTRIBUTE ck_attrib;
    CK_BBOOL priv;
    CK_OBJECT_CLASS obj_class;

    memset(obj_id, 0x00, sizeof(obj_id));
    memset(data_hdr, 0x00, sizeof(data_hdr));

    attrib = object->attrib;
    obj_size = 0;

    while (attrib)
    {
        if (attrib->token)
            obj_size += attrib->attrib.ulValueLen + 6; // Fixme: what if the obj_id is more than 2 bytes??

        log_Log(LOG_LOW, "%lX Objsize: %lu Len: %lu", attrib->attrib.type, obj_size, attrib->attrib.ulValueLen);

        attrib = attrib->next;
    }

    if (object->msc_key)
    {
        obj_id[0] = 'k';
        obj_id[1] = object->msc_key->keyNum + 1 > 9 ? 65 + object->msc_key->keyNum : 48 + object->msc_key->keyNum;

        data_hdr[0] = 0; // Fixme: Key object type

        priv = 0x01;
        ck_attrib.type = CKA_PRIVATE;
        ck_attrib.pValue = &priv;
        ck_attrib.ulValueLen = sizeof(priv);

/*
        if (object_MatchAttrib(&ck_attrib, object))
            objACL.readPermission = (MSCUShort16)object_MapPIN(st.prefs.user_pin_num);
        else
*/
        objACL.readPermission = MSC_AUT_ALL;

        objACL.writePermission = (MSCUShort16)object_MapPIN(st.prefs.user_pin_num);
        objACL.deletePermission = (MSCUShort16)object_MapPIN(st.prefs.user_pin_num);

        obj_class = CKO_PRIVATE_KEY;
        ck_attrib.type = CKA_CLASS;
        ck_attrib.pValue = &obj_class;
        ck_attrib.ulValueLen = sizeof(obj_class);

        if (object_MatchAttrib(&ck_attrib, object))
            msc_rv = msc_CreateObject(&slot->conn, (char *)obj_id, st.prefs.prvkey_attrib_size, &objACL);
        else
            msc_rv = msc_CreateObject(&slot->conn, (char *)obj_id, st.prefs.pubkey_attrib_size, &objACL);

        if (MSC_ERROR(msc_rv) && (msc_rv != MSC_OBJECT_EXISTS))
            rv = CKR_FUNCTION_FAILED;
    }
    else if (object->msc_obj)
    {
        data_hdr[0] = 0; // Fixme: object type

        strncpy((char *)obj_id, object->msc_obj->objectID, sizeof(obj_id));
        objACL.readPermission = object->msc_obj->objectACL.readPermission;
        objACL.writePermission = object->msc_obj->objectACL.writePermission;
        objACL.deletePermission = object->msc_obj->objectACL.deletePermission;

        log_Log(LOG_LOW, "Object ID is: %s", obj_id);

        if (!isupper(obj_id[0]))
        {
            P11_ERR("Can't create object; first character in OID is not upper-case");
            rv = CKR_FUNCTION_FAILED;
        }
        else if (CKR_ERROR(object_GetAttrib(CKA_VALUE, object, &attrib)))
        {
            P11_ERR("Can't create object; no CKA_VALUE attribute");
            rv = CKR_FUNCTION_FAILED;
        }
        else
        {
            msc_rv = msc_CreateObject(&slot->conn, (char *)obj_id, attrib->attrib.ulValueLen, &objACL);

            if (msc_rv == MSC_OBJECT_EXISTS)
            {
                if (MSC_ERROR(msc_DeleteObject(&slot->conn, obj_id, 0)))
                    rv = CKR_FUNCTION_FAILED;
                else if (MSC_ERROR(msc_CreateObject(&slot->conn, (char *)obj_id, attrib->attrib.ulValueLen, &objACL)))
                    rv = CKR_FUNCTION_FAILED;
                else if (MSC_ERROR(msc_WriteObject(&slot->conn, (char *)obj_id, 0, 
                                                   (CK_BYTE *)attrib->attrib.pValue, attrib->attrib.ulValueLen)))
                    rv = CKR_FUNCTION_FAILED;
            }
            else if (MSC_ERROR(msc_rv))
                rv = CKR_FUNCTION_FAILED;
            else if (MSC_ERROR(msc_WriteObject(&slot->conn, (char *)obj_id, 0, 
                                               (CK_BYTE *)attrib->attrib.pValue, attrib->attrib.ulValueLen)))
                rv = CKR_FUNCTION_FAILED;

            obj_id[0] = tolower(obj_id[0]);

            priv = 0x01;
            ck_attrib.type = CKA_PRIVATE;
            ck_attrib.pValue = &priv;
            ck_attrib.ulValueLen = sizeof(priv);
    
            if (object_MatchAttrib(&ck_attrib, object))
                objACL.readPermission = (MSCUShort16)object_MapPIN(st.prefs.user_pin_num);
            else
                objACL.readPermission = MSC_AUT_ALL;

            objACL.writePermission = (MSCUShort16)object_MapPIN(st.prefs.user_pin_num);
            objACL.deletePermission = (MSCUShort16)object_MapPIN(st.prefs.user_pin_num);
    
            obj_class = CKO_CERTIFICATE;
            ck_attrib.type = CKA_CLASS;
            ck_attrib.pValue = &obj_class;
            ck_attrib.ulValueLen = sizeof(obj_class);
    
            if (object_MatchAttrib(&ck_attrib, object))
                msc_rv = msc_CreateObject(&slot->conn, (char *)obj_id, st.prefs.cert_attrib_size, &objACL);
            else
                msc_rv = msc_CreateObject(&slot->conn, (char *)obj_id, st.prefs.data_attrib_size, &objACL);
    
            if (MSC_ERROR(msc_rv) && (msc_rv != MSC_OBJECT_EXISTS))
                rv = CKR_FUNCTION_FAILED;
        }
    }
    else
    {
        P11_ERR("Invalid non-token object");
        rv = CKR_FUNCTION_FAILED;
    }

    if (!CKR_ERROR(rv))
    {
        data = (CK_BYTE *)malloc(obj_size + sizeof(data_hdr));

        if (!data)
            rv = CKR_HOST_MEMORY;
        else
        {
            log_Log(LOG_LOW, "Obj size is: %lu (0x%lX)", obj_size, obj_size);

            data_hdr[1] = obj_id[0];
            data_hdr[2] = obj_id[1];
            data_hdr[5] = (CK_BYTE)((obj_size & 0xFF00) >> 8);
            data_hdr[6] = (CK_BYTE)(obj_size & 0xFF);
            memcpy(data, data_hdr, sizeof(data_hdr));

            attrib = object->attrib;
            data_pos = sizeof(data_hdr);
    
            while (attrib)
            {
                if (!attrib->token)
                {
                    log_Log(LOG_LOW, "Skipping attribute:");
                    object_LogAttribute(&attrib->attrib);
                }
                else
                {
                    log_Log(LOG_LOW, "Writing attribute:");
                    object_LogAttribute(&attrib->attrib);

                    data[data_pos] = (CK_BYTE)((attrib->attrib.type & 0xFF000000) >> 24);
                    data[data_pos + 1] = (CK_BYTE)((attrib->attrib.type & 0xFF0000) >> 16);
                    data[data_pos + 2] = (CK_BYTE)((attrib->attrib.type & 0xFF00) >> 8);
                    data[data_pos + 3] = (CK_BYTE)(attrib->attrib.type & 0xFF);
                    data[data_pos + 4] = (CK_BYTE)((attrib->attrib.ulValueLen & 0xFF00) >> 8);
                    data[data_pos + 5] = (CK_BYTE)(attrib->attrib.ulValueLen & 0xFF);
                    memcpy(&data[data_pos + 6], attrib->attrib.pValue, attrib->attrib.ulValueLen);

                    data_pos += attrib->attrib.ulValueLen + 6; // Fixme: 2 byte obj_id??
                }
    
                attrib = attrib->next;
            }

            log_Log(LOG_LOW, "ID: %s  SIZE: %lu", obj_id, obj_size);
        
            { CK_BYTE buf[4096]; 
              object_BinToHex(data, obj_size + sizeof(data_hdr), buf); 
              log_Log(LOG_LOW, "Data:%s", buf); }

            if (MSC_ERROR(msc_WriteObject(&slot->conn, obj_id, 0, data, obj_size + sizeof(data_hdr))))
                rv = CKR_FUNCTION_FAILED;
        }

        if (data)
            free(data);
    }

    return rv;
}

/******************************************************************************
** Function: object_InferClassAttributes
**
** Infer attribute about a specific class of object (a certificate for example)
**
** Parameters:
**  hSession - Session handle
**  object   - Object to infer attributes
**
** Returns:
**  none
*******************************************************************************/
CK_RV object_InferClassAttributes(CK_SESSION_HANDLE hSession, P11_Object *object)
{
    CK_RV rv = CKR_OK;
    CK_ATTRIBUTE attrib;
    P11_Attrib *obj_attrib, *t_attrib;
    CK_OBJECT_CLASS obj_class;
    CK_BYTE buf[4096]; /* Fixme: don't hardcode this */
    CK_ULONG len;

    log_Log(LOG_LOW, "object_InferClassAttributes");

    if (!CKR_ERROR(object_GetAttrib(CKA_VALUE, object, &obj_attrib)))
    {
        obj_class = CKO_CERTIFICATE;
        attrib.type = CKA_CLASS;
        attrib.pValue = &obj_class; // Fixme: endian issue
        attrib.ulValueLen = 4;

        log_Log(LOG_LOW, "object_InferClassAttributes: got CKA_VALUE");
    
        if (object_MatchAttrib(&attrib, object))
        {
            log_Log(LOG_LOW, "object_InferClassAttributes: got CKO_CERTIFICATE");
    
            if (CKR_ERROR_NOLOG(object_GetAttrib(CKA_MODULUS, object, &t_attrib)) &&
                !CKR_ERROR(object_GetCertModulus((CK_BYTE *)obj_attrib->attrib.pValue,
                                                 obj_attrib->attrib.ulValueLen,
                                                 buf,
                                                 &len)))
            {
                log_Log(LOG_LOW, "object_InferClassAttributes: got CKA_MODULUS");
                (void)CKR_ERROR(object_AddAttribute(object, CKA_MODULUS, FALSE, buf, len, &t_attrib));
            }

            if (CKR_ERROR_NOLOG(object_GetAttrib(CKA_PUBLIC_EXPONENT, object, &t_attrib)) &&
                !CKR_ERROR(object_GetCertPubExponent((CK_BYTE *)obj_attrib->attrib.pValue,
                                                     obj_attrib->attrib.ulValueLen,
                                                     buf,
                                                     &len)))
            {
                log_Log(LOG_LOW, "object_InferClassAttributes: got CKA_PUBLIC_EXPONENT");
                (void)CKR_ERROR(object_AddAttribute(object, CKA_PUBLIC_EXPONENT, FALSE, buf, len, &t_attrib));
            }

            if (CKR_ERROR_NOLOG(object_GetAttrib(CKA_SUBJECT, object, &t_attrib)) &&
                !CKR_ERROR(object_GetCertSubject((CK_BYTE *)obj_attrib->attrib.pValue,
                                                 obj_attrib->attrib.ulValueLen,
                                                 buf,
                                                 &len)))
            {
                log_Log(LOG_LOW, "object_InferClassAttributes: got CKA_SUBJECT");
                (void)CKR_ERROR(object_AddAttribute(object, CKA_SUBJECT, FALSE, buf, len, &t_attrib));
            }

            if (CKR_ERROR_NOLOG(object_GetAttrib(CKA_ISSUER, object, &t_attrib)) &&
                !CKR_ERROR(object_GetCertIssuer((CK_BYTE *)obj_attrib->attrib.pValue,
                                                obj_attrib->attrib.ulValueLen,
                                                buf,
                                                &len)))
            {
                log_Log(LOG_LOW, "object_InferClassAttributes: got CKA_ISSUER");
                (void)CKR_ERROR(object_AddAttribute(object, CKA_ISSUER, FALSE, buf, len, &t_attrib));
            }

            if (CKR_ERROR_NOLOG(object_GetAttrib(CKA_SERIAL_NUMBER, object, &t_attrib)) &&
                !CKR_ERROR(object_GetCertSerial((CK_BYTE *)obj_attrib->attrib.pValue,
                                                obj_attrib->attrib.ulValueLen,
                                                buf,
                                                &len)))
            {
                log_Log(LOG_LOW, "object_InferClassAttributes: got CKA_SERIAL_NUMBER");
                (void)CKR_ERROR(object_AddAttribute(object, CKA_SERIAL_NUMBER, FALSE, buf, len, &t_attrib));
            }
        }
    }

    return rv;
}

/******************************************************************************
** Function: object_GetCertSerial
**
** Pulls the serial number from raw certificate data using OpenSSL.
**
** Fixme: add error checking for OpenSSL functions?
**
** Parameters:
**  cert      - Input certificate
**  cert_size - Size of input data
**  out       - Output data (serial number)
**  out_len   - Length of output data
**
** Returns:
**  CKR_OK
*******************************************************************************/
CK_RV object_GetCertSerial(CK_BYTE *cert, CK_ULONG cert_size, CK_BYTE *out, CK_ULONG *out_len)
{
    CK_RV rv = CKR_OK;
    BIO *inObject=NULL;
    X509 *certObject=NULL;
    ASN1_INTEGER *serial;
    BUF_MEM *bptr;

    inObject   = BIO_new_mem_buf(cert, cert_size);
    certObject = d2i_X509_bio(inObject, NULL);

    serial = X509_get_serialNumber(certObject);

    *out_len = serial->length;

    if (out)
        memcpy(out, serial->data, serial->length);

    { CK_BYTE *buf;
      buf = (CK_BYTE *)malloc(((*out_len) * 3) + 1);
      object_BinToHex(out, *out_len, buf);
      log_Log(LOG_LOW, "GetCertSerial:%s", buf);
      free(buf); }

    ASN1_INTEGER_free(serial);
    //Fixme: X509_free(certObject);
    BIO_get_mem_ptr(inObject, &bptr);
    BIO_set_close(inObject, BIO_NOCLOSE);
    BIO_free(inObject);

    return rv;
}

/******************************************************************************
** Function: object_GetCertSubject
**
** Pulls the subject from raw certificate data using OpenSSL.
**
** Fixme: add error checking for OpenSSL functions?
**
** Parameters:
**  cert      - Input certificate
**  cert_size - Size of input data
**  out       - Output data (subject)
**  out_len   - Length of output data
**
** Returns:
**  CKR_OK
*******************************************************************************/
CK_RV object_GetCertSubject(CK_BYTE *cert, CK_ULONG cert_size, CK_BYTE *out, CK_ULONG *out_len)
{
    CK_RV rv = CKR_OK;
    BIO *inObject=NULL;
    X509 *certObject=NULL;
    X509_NAME *name;
    BUF_MEM *bptr;

    inObject   = BIO_new_mem_buf(cert, cert_size);
    certObject = d2i_X509_bio(inObject, NULL);

    name = X509_get_subject_name(certObject);

    if (!out)
        *out_len = i2d_X509_NAME(name, 0);
    else
        *out_len = i2d_X509_NAME(name, &out);

    X509_NAME_free(name);
    //Fixme: X509_free(certObject);
    BIO_get_mem_ptr(inObject, &bptr);
    BIO_set_close(inObject, BIO_NOCLOSE);
    BIO_free(inObject);

    return rv;
}

/******************************************************************************
** Function: object_GetCertIssuer
**
** Pulls the issuer from raw certificate data using OpenSSL.
**
** Fixme: add error checking for OpenSSL functions?
**
** Parameters:
**  cert      - Input certificate
**  cert_size - Size of input data
**  out       - Output data (issuer)
**  out_len   - Length of output data
**
** Returns:
**  CKR_OK
*******************************************************************************/
CK_RV object_GetCertIssuer(CK_BYTE *cert, CK_ULONG cert_size, CK_BYTE *out, CK_ULONG *out_len)
{
    CK_RV rv = CKR_OK;
    BIO *inObject=NULL;
    X509 *certObject=NULL;
    X509_NAME *name;
    BUF_MEM *bptr;

    inObject   = BIO_new_mem_buf(cert, cert_size);
    certObject = d2i_X509_bio(inObject, NULL);

    name = X509_get_issuer_name(certObject);

    if (!out)
        *out_len = i2d_X509_NAME(name, 0);
    else
        *out_len = i2d_X509_NAME(name, &out);

    X509_NAME_free(name);
    //Fixme: X509_free(certObject);
    BIO_get_mem_ptr(inObject, &bptr);
    BIO_set_close(inObject, BIO_NOCLOSE);
    BIO_free(inObject);

    return rv;
}

/******************************************************************************
** Function: object_GetCertModulus
**
** Pulls the modulus from raw certificate data using OpenSSL.
**
** Fixme: add error checking for OpenSSL functions?
**
** Parameters:
**  cert      - Input certificate
**  cert_size - Size of input data
**  out       - Output data (modulus)
**  out_len   - Length of output data
**
** Returns:
**  CKR_OK
*******************************************************************************/
CK_RV object_GetCertModulus(CK_BYTE *cert, CK_ULONG cert_size, CK_BYTE *out, CK_ULONG *out_len)
{
    CK_RV rv = CKR_OK;
    BIO *inObject=NULL;
    X509 *certObject=NULL;
    EVP_PKEY *pkey;
    BUF_MEM *bptr;

    inObject   = BIO_new_mem_buf(cert, cert_size);

    certObject = d2i_X509_bio(inObject, NULL);

    pkey = X509_get_pubkey(certObject);

    if (!out)
        *out_len = BN_num_bytes(pkey->pkey.rsa->n);
    else
    {
        *out_len = BN_num_bytes(pkey->pkey.rsa->n);
        BN_bn2bin(pkey->pkey.rsa->n, out);
    }

    //Fixme: X509_free(certObject);
    BIO_get_mem_ptr(inObject, &bptr);
    BIO_set_close(inObject, BIO_NOCLOSE);
    BIO_free(inObject);

    return rv;
}

/******************************************************************************
** Function: object_GetCertPubExponent
**
** Pulls the public exponent from raw certificate data using OpenSSL.
**
** Fixme: add error checking for OpenSSL functions?
**
** Parameters:
**  cert      - Input certificate
**  cert_size - Size of input data
**  out       - Output data (public exponent)
**  out_len   - Length of output data
**
** Returns:
**  CKR_OK
*******************************************************************************/
CK_RV object_GetCertPubExponent(CK_BYTE *cert, CK_ULONG cert_size, CK_BYTE *out, CK_ULONG *out_len)
{
    CK_RV rv = CKR_OK;
    BIO *inObject=NULL;
    X509 *certObject=NULL;
    EVP_PKEY *pkey;
    BUF_MEM *bptr;

    inObject   = BIO_new_mem_buf(cert, cert_size);

    certObject = d2i_X509_bio(inObject, NULL);

    pkey = X509_get_pubkey(certObject);

    if (!out)
        *out_len = BN_num_bytes(pkey->pkey.rsa->e);
    else
    {
        *out_len = BN_num_bytes(pkey->pkey.rsa->e);
        BN_bn2bin(pkey->pkey.rsa->e, out);
    }

    //Fixme: X509_free(certObject);
    BIO_get_mem_ptr(inObject, &bptr);
    BIO_set_close(inObject, BIO_NOCLOSE);
    BIO_free(inObject);

    return rv;
}

/******************************************************************************
** Function: object_CreateCertificate
**
** Creates a new certificate object on the token
**
** Parameters:
**  hSession - Session handle
**  object   - Object to add to token
**
** Returns:
**  CKR_OK
*******************************************************************************/
CK_RV object_CreateCertificate(CK_SESSION_HANDLE hSession, P11_Object *object)
{
    CK_RV rv = CKR_OK;
    P11_Session *session = (P11_Session *)hSession;
    P11_Slot *slot = &st.slots[session->session.slotID - 1];
    P11_Object *object_l;
    P11_Attrib *attrib;
    CK_BYTE tag, last;

    if (!(object->msc_obj = (MSCObjectInfo *)calloc(1, sizeof(MSCObjectInfo))))
        rv = CKR_HOST_MEMORY;
    else
    {
        object_l = slot->objects;
        tag = 'C';
        last = 47;

        while (object_l)
        {
            if ((object_l != object) &&
                object_l->msc_obj &&
                !CKR_ERROR(object_GetAttrib(CKA_CLASS, object_l, &attrib)))
            {
                if (*((CK_ULONG *)attrib->attrib.pValue) == CKO_CERTIFICATE)
                {
                    tag = object_l->msc_obj->objectID[0];
                    last = object_l->msc_obj->objectID[1];
                }
            }

            object_l = object_l->next;
        }

        object->msc_obj->objectID[0] = tag;
        object->msc_obj->objectID[1] = last + 1;
        object->msc_obj->objectID[2] = 0;

        object->msc_obj->objectSize = 0; /* WriteAttributes will figure out the size */
        object->msc_obj->objectACL.readPermission = MSC_AUT_ALL;
        object->msc_obj->objectACL.writePermission = (MSCUShort16)object_MapPIN(st.prefs.user_pin_num);
        object->msc_obj->objectACL.deletePermission = (MSCUShort16)object_MapPIN(st.prefs.user_pin_num);

        rv = object_WriteAttributes(hSession, object);
    }

    return rv;
}

/******************************************************************************
** Function: object_CreatePublicKey
**
** Creates a new public key object on the token (imports RSA key)
**
** Parameters:
**  hSession - Session handle
**  object   - Object to add to token
**
** Returns:
**  CKR_OK
*******************************************************************************/
CK_RV object_CreatePublicKey(CK_SESSION_HANDLE hSession, P11_Object *object)
{
    CK_RV rv = CKR_OK;
    MSC_RV msc_rv;
    P11_Session *session = (P11_Session *)hSession;
    P11_Slot *slot = &st.slots[session->session.slotID - 1];
    CK_BYTE keyNum = 0;
    MSCKeyInfo keyInfo;
    P11_Attrib *modulus;
    P11_Attrib *public_exponent;
    CK_ULONG keySize = 1024;
    CK_BYTE *keyBlob;
    CK_ULONG keyBlobSize;

    if (!(object->msc_key = (MSCKeyInfo *)calloc(1, sizeof(MSCKeyInfo))))
        rv = CKR_HOST_MEMORY;
    else
    {
        msc_rv = msc_ListKeys(&slot->conn, MSC_SEQUENCE_RESET, &keyInfo);
        if (!MSC_ERROR(msc_rv))
            keyNum = keyInfo.keyNum + 1;
        while (!MSC_ERROR(msc_rv) && !CKR_ERROR(rv))
        {
            msc_rv = msc_ListKeys(&slot->conn, MSC_SEQUENCE_NEXT, &keyInfo);
    
            if (!MSC_ERROR(msc_rv))
                keyNum = keyInfo.keyNum + 1;
        }

        if (!CKR_ERROR(object_GetAttrib(CKA_MODULUS, object, &modulus)))
            keySize = modulus->attrib.ulValueLen * 8;
        else
            return CKR_FUNCTION_FAILED;

        (void)CKR_ERROR(object_GetAttrib(CKA_PUBLIC_EXPONENT, object, &public_exponent));

        keyBlobSize = modulus->attrib.ulValueLen +
                      public_exponent->attrib.ulValueLen;

        keyBlob = (CK_BYTE *)malloc(keyBlobSize + 14);

        if (!keyBlob)
            rv = CKR_HOST_MEMORY;
        else
        {
            object->msc_key->keyNum = keyNum;
            object->msc_key->keyType = MSC_KEY_RSA_PUBLIC;
            object->msc_key->keySize = (MSCUShort16)keySize;
            object->msc_key->keyPolicy.cipherDirection = MSC_KEYPOLICY_DIR_VERIFY | MSC_KEYPOLICY_DIR_ENCRYPT;
            object->msc_key->keyPolicy.cipherMode = MSC_KEYPOLICY_MODE_RSA_NOPAD;
            object->msc_key->keyACL.readPermission = MSC_AUT_ALL;
            object->msc_key->keyACL.writePermission = (MSCUShort16)object_MapPIN(st.prefs.user_pin_num);
            object->msc_key->keyACL.usePermission = (MSCUShort16)object_MapPIN(st.prefs.user_pin_num);
    
            keyBlobSize = 0;
            keyBlob[keyBlobSize++] = MSC_BLOB_ENC_PLAIN;
            keyBlob[keyBlobSize++] = object->msc_key->keyType;
            keyBlob[keyBlobSize++] = (CK_BYTE)(object->msc_key->keySize >> 8);
            keyBlob[keyBlobSize++] = (CK_BYTE)object->msc_key->keySize;
            keyBlob[keyBlobSize++] = (CK_BYTE)(modulus->attrib.ulValueLen >> 8);
            keyBlob[keyBlobSize++] = (CK_BYTE)modulus->attrib.ulValueLen;
            memcpy(&keyBlob[keyBlobSize], modulus->attrib.pValue, modulus->attrib.ulValueLen);
            keyBlobSize += modulus->attrib.ulValueLen;
            keyBlob[keyBlobSize++] = (CK_BYTE)(public_exponent->attrib.ulValueLen >> 8);
            keyBlob[keyBlobSize++] = (CK_BYTE)public_exponent->attrib.ulValueLen;
            memcpy(&keyBlob[keyBlobSize], public_exponent->attrib.pValue, public_exponent->attrib.ulValueLen);
            keyBlobSize += public_exponent->attrib.ulValueLen;

            { CK_BYTE *buf;
              buf = (CK_BYTE *)malloc((keyBlobSize * 3) + 1);
              object_BinToHex(keyBlob, keyBlobSize, buf);
              log_Log(LOG_LOW, "Raw keyBlob: %s", buf);
              free(buf); }

            if (MSC_ERROR(msc_ImportKey(&slot->conn, 
                                        object->msc_key->keyNum, 
                                        &object->msc_key->keyACL, 
                                        keyBlob, 
                                        keyBlobSize,
                                        &object->msc_key->keyPolicy, 0, 0)))
                rv = CKR_FUNCTION_FAILED;
            else
                rv = object_WriteAttributes(hSession, object);
        }
    }

    return rv;
}

/******************************************************************************
** Function: object_CreatePrivateKey
**
** Creates a new private key object on the token (imports RSA key)
**
** Parameters:
**  hSession - Session handle
**  object   - Object to add to token
**
** Returns:
**  CKR_OK
*******************************************************************************/
CK_RV object_CreatePrivateKey(CK_SESSION_HANDLE hSession, P11_Object *object)
{
    CK_RV rv = CKR_OK;
    MSC_RV msc_rv;
    P11_Session *session = (P11_Session *)hSession;
    P11_Slot *slot = &st.slots[session->session.slotID - 1];
    CK_BYTE keyNum = 0;
    MSCKeyInfo keyInfo;
    P11_Attrib *modulus;
    P11_Attrib *private_exponent;
    P11_Attrib *public_exponent;
    P11_Attrib *prime_1;
    P11_Attrib *prime_2;
    P11_Attrib *exponent_1;
    P11_Attrib *exponent_2;
    P11_Attrib *coefficient;
    CK_ULONG keySize = 1024;
    CK_BYTE *keyBlob;
    CK_ULONG keyBlobSize;

    if (!(object->msc_key = (MSCKeyInfo *)calloc(1, sizeof(MSCKeyInfo))))
        rv = CKR_HOST_MEMORY;
    else
    {
        msc_rv = msc_ListKeys(&slot->conn, MSC_SEQUENCE_RESET, &keyInfo);
        if (!MSC_ERROR(msc_rv))
            keyNum = keyInfo.keyNum + 1;
        while (!MSC_ERROR(msc_rv) && !CKR_ERROR(rv))
        {
            msc_rv = msc_ListKeys(&slot->conn, MSC_SEQUENCE_NEXT, &keyInfo);
    
            if (!MSC_ERROR(msc_rv))
                keyNum = keyInfo.keyNum + 1;
        }

        if (!CKR_ERROR(object_GetAttrib(CKA_MODULUS, object, &modulus)))
            keySize = modulus->attrib.ulValueLen * 8;

        (void)CKR_ERROR(object_GetAttrib(CKA_PRIVATE_EXPONENT, object, &private_exponent));
        (void)CKR_ERROR(object_GetAttrib(CKA_PUBLIC_EXPONENT, object, &public_exponent));
        (void)CKR_ERROR(object_GetAttrib(CKA_PRIME_1, object, &prime_1));
        (void)CKR_ERROR(object_GetAttrib(CKA_PRIME_2, object, &prime_2));
        (void)CKR_ERROR(object_GetAttrib(CKA_EXPONENT_1, object, &exponent_1));
        (void)CKR_ERROR(object_GetAttrib(CKA_EXPONENT_2, object, &exponent_2));
        (void)CKR_ERROR(object_GetAttrib(CKA_COEFFICIENT, object, &coefficient));

        keyBlobSize = prime_1->attrib.ulValueLen +
                      prime_2->attrib.ulValueLen +
                      coefficient->attrib.ulValueLen +
                      exponent_1->attrib.ulValueLen +
                      exponent_2->attrib.ulValueLen;

        keyBlob = (CK_BYTE *)malloc(keyBlobSize + 14);

        if (!keyBlob)
            rv = CKR_HOST_MEMORY;
        else
        {
            object->msc_key->keyNum = keyNum;
            object->msc_key->keyType = MSC_KEY_RSA_PRIVATE_CRT;
            object->msc_key->keySize = (MSCUShort16)keySize;
            object->msc_key->keyPolicy.cipherDirection = MSC_KEYPOLICY_DIR_SIGN | MSC_KEYPOLICY_DIR_DECRYPT;
            object->msc_key->keyPolicy.cipherMode = MSC_KEYPOLICY_MODE_RSA_NOPAD;
            object->msc_key->keyACL.readPermission = MSC_AUT_NONE;
            object->msc_key->keyACL.writePermission = (MSCUShort16)object_MapPIN(st.prefs.user_pin_num);
            object->msc_key->keyACL.usePermission = (MSCUShort16)object_MapPIN(st.prefs.user_pin_num);
    
            keyBlobSize = 0;
            keyBlob[keyBlobSize++] = MSC_BLOB_ENC_PLAIN;
            keyBlob[keyBlobSize++] = object->msc_key->keyType;
            keyBlob[keyBlobSize++] = (CK_BYTE)(object->msc_key->keySize >> 8);
            keyBlob[keyBlobSize++] = (CK_BYTE)object->msc_key->keySize;
            keyBlob[keyBlobSize++] = (CK_BYTE)(prime_1->attrib.ulValueLen >> 8);
            keyBlob[keyBlobSize++] = (CK_BYTE)prime_1->attrib.ulValueLen;
            memcpy(&keyBlob[keyBlobSize], prime_1->attrib.pValue, prime_1->attrib.ulValueLen);
            keyBlobSize += prime_1->attrib.ulValueLen;
            keyBlob[keyBlobSize++] = (CK_BYTE)(prime_2->attrib.ulValueLen >> 8);
            keyBlob[keyBlobSize++] = (CK_BYTE)prime_2->attrib.ulValueLen;
            memcpy(&keyBlob[keyBlobSize], prime_2->attrib.pValue, prime_2->attrib.ulValueLen);
            keyBlobSize += prime_2->attrib.ulValueLen;
            keyBlob[keyBlobSize++] = (CK_BYTE)(coefficient->attrib.ulValueLen >> 8);
            keyBlob[keyBlobSize++] = (CK_BYTE)coefficient->attrib.ulValueLen;
            memcpy(&keyBlob[keyBlobSize], coefficient->attrib.pValue, coefficient->attrib.ulValueLen);
            keyBlobSize += coefficient->attrib.ulValueLen;
            keyBlob[keyBlobSize++] = (CK_BYTE)(exponent_1->attrib.ulValueLen >> 8);
            keyBlob[keyBlobSize++] = (CK_BYTE)exponent_1->attrib.ulValueLen;
            memcpy(&keyBlob[keyBlobSize], exponent_1->attrib.pValue, exponent_1->attrib.ulValueLen);
            keyBlobSize += exponent_1->attrib.ulValueLen;
            keyBlob[keyBlobSize++] = (CK_BYTE)(exponent_2->attrib.ulValueLen >> 8);
            keyBlob[keyBlobSize++] = (CK_BYTE)exponent_2->attrib.ulValueLen;
            memcpy(&keyBlob[keyBlobSize], exponent_2->attrib.pValue, exponent_2->attrib.ulValueLen);
            keyBlobSize += exponent_2->attrib.ulValueLen;

            { CK_BYTE *buf;
              buf = (CK_BYTE *)malloc((keyBlobSize * 3) + 1);
              object_BinToHex(keyBlob, keyBlobSize, buf);
              log_Log(LOG_LOW, "Raw keyBlob: %s", buf);
              free(buf); }

            if (MSC_ERROR(msc_ImportKey(&slot->conn, 
                                        object->msc_key->keyNum, 
                                        &object->msc_key->keyACL, 
                                        keyBlob, 
                                        keyBlobSize,
                                        &object->msc_key->keyPolicy, 0, 0)))
                rv = CKR_FUNCTION_FAILED;
            else
                rv = object_WriteAttributes(hSession, object);
        }
    }

    return rv;
}

/******************************************************************************
** Function: object_CreateObject
**
** Creates a new data object on the token
**
** Parameters:
**  hSession - Session handle
**  object   - Object to add to token
**
** Returns:
**  CKR_OK
*******************************************************************************/
CK_RV object_CreateObject(CK_SESSION_HANDLE hSession, P11_Object *object)
{
    CK_RV rv = CKR_OK;
    P11_Session *session = (P11_Session *)hSession;
    P11_Slot *slot = &st.slots[session->session.slotID - 1];
    P11_Object *object_l;
    P11_Attrib *attrib;
    CK_BYTE tag, last;

    if (!(object->msc_obj = (MSCObjectInfo *)calloc(1, sizeof(MSCObjectInfo))))
        rv = CKR_HOST_MEMORY;
    else
    {
        object_l = slot->objects;
        tag = 'O';
        last = 47;

        while (object_l)
        {
            if ((object_l != object) &&
                object_l->msc_obj &&
                !CKR_ERROR(object_GetAttrib(CKA_CLASS, object_l, &attrib)))
            {
                if (*((CK_ULONG *)attrib->attrib.pValue) == CKO_CERTIFICATE)
                {
                    tag = object_l->msc_obj->objectID[0];
                    last = object_l->msc_obj->objectID[1];
                }
            }

            object_l = object_l->next;
        }

        object->msc_obj->objectID[0] = tag;
        object->msc_obj->objectID[1] = last + 1;
        object->msc_obj->objectID[2] = 0;

        object->msc_obj->objectSize = 0; /* WriteAttributes will figure out the size */

        /* Data objects don't have any attributes that specify permissions so we just */
        /* default to these.                                                          */
        object->msc_obj->objectACL.readPermission = (MSCUShort16)object_MapPIN(st.prefs.user_pin_num);
        object->msc_obj->objectACL.writePermission = (MSCUShort16)object_MapPIN(st.prefs.user_pin_num);
        object->msc_obj->objectACL.deletePermission = (MSCUShort16)object_MapPIN(st.prefs.user_pin_num);

        rv = object_WriteAttributes(hSession, object);
    }

    return rv;
}

/******************************************************************************
** Function: object_MapPIN
**
** Maps a PIN number to a Musclecard Framework AUT PIN
**
** Parameters:
**  pinNum - P11 PIN number
**
** Returns:
**  MSC_AUT_XXX
*******************************************************************************/
CK_ULONG object_MapPIN(CK_ULONG pinNum)
{
    CK_RV rv = CKR_OK;

    if (st.prefs.disable_security)
        return MSC_AUT_ALL;
    else if (pinNum == 0)
        return MSC_AUT_PIN_0;
    else if (pinNum == 1)
        return MSC_AUT_PIN_1;
    else if (pinNum == 2)
        return MSC_AUT_PIN_2;
    else if (pinNum == 3)
        return MSC_AUT_PIN_3;
    else if (pinNum == 4)
        return MSC_AUT_PIN_4;
    else
        return P11_MAX_ULONG;

    return rv;
}

/******************************************************************************
** Function: object_UserMode
**
** Tries to update the information on "sensitive" objects
**
** Parameters:
**  hSession - session handle
**
** Returns:
**  CKR_XXX
*******************************************************************************/
CK_ULONG object_UserMode(CK_SESSION_HANDLE hSession)
{
    CK_RV rv = CKR_OK;
    P11_Session *session = (P11_Session *)hSession;
    P11_Object *object_l;

    if (!session)
        rv = CKR_FUNCTION_FAILED;
    else
    {
        object_l = st.slots[session->session.slotID - 1].objects;
        while(object_l && object_l->next)
        {
            if (object_l->sensitive)
            {
                if (object_l->msc_key)
                    rv = CKR_ERROR(object_UpdateKeyInfo(hSession, (CK_OBJECT_HANDLE *)&object_l, 0));
                else if (object_l->msc_obj)
                    rv = CKR_ERROR(object_UpdateObjectInfo(hSession, (CK_OBJECT_HANDLE *)&object_l, 0));
            }
    
            object_l = object_l->next;
        }
    }

    return rv;
}
