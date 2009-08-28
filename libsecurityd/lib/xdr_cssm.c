/*
 * Copyright (c) 2006-2008 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <architecture/byte_order.h>
#include <string.h>		/* bzero() */
#include <stdlib.h>		/* exit() */
#include <assert.h>		/* assert() */
#include <stdio.h>		/* XXX/gh  because utilities/debugging.h doesn't */
#include <security_utilities/debugging.h>

#include "xdr_cssm.h"

// All functions with the "writes" comment write to memory without regard for size only operation.  This is okay as long as they aren't used "naked", ie. as toplevel encoders.  For our purposes they're always in a struct or array, or with a pointer pointing at them.

// XXX/cs writes
bool_t sec_xdr_clip_long(XDR *xdrs, long *objp)
{
	uint32_t clip = 0;
	
	if (objp && xdrs->x_op == XDR_ENCODE)
		clip = *objp & UINT32_MAX;
    if (!xdr_uint32(xdrs, &clip))
		return (FALSE);
	if (objp && xdrs->x_op == XDR_DECODE)
		*objp = clip;
    return (TRUE);
}

// XXX/cs writes
bool_t xdr_voidptr(XDR *xdrs, void **objp)
{
    long ptr = 0;

    if (*objp)
		ptr = (intptr_t)*objp;
    if (!sec_xdr_clip_long(xdrs, &ptr))
		return (FALSE);
	// not returned
	
    return (TRUE);
}

bool_t xdr_CSSM_DATA(XDR *xdrs, CSSM_DATA *objp)
{
    u_int valueLength; // objp->Length is a size_t
    if (xdrs->x_op == XDR_ENCODE) {
        if (objp->Length > (u_int)~0)
            return (FALSE);
        valueLength = objp->Length;
    }
    if (!sec_xdr_bytes(xdrs, &objp->Data, &valueLength, ~0))
        return (FALSE);
    if (xdrs->x_op == XDR_DECODE)
        objp->Length = valueLength;
    return (TRUE);
}

bool_t xdr_CSSM_GUID(XDR *xdrs, CSSM_GUID *objp)
{
    return xdr_opaque(xdrs, (char *)objp, sizeof(CSSM_GUID));
}

bool_t xdr_CSSM_VERSION(XDR *xdrs, CSSM_VERSION *objp)
{
    if (!xdr_uint32(xdrs, &objp->Major))
        return (FALSE);
    if (!xdr_uint32(xdrs, &objp->Minor))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_SUBSERVICE_UID(XDR *xdrs, CSSM_SUBSERVICE_UID *objp)
{
    if (!xdr_CSSM_GUID(xdrs, &objp->Guid))
        return (FALSE);
    if (!xdr_CSSM_VERSION(xdrs, &objp->Version))
        return (FALSE);
    if (!xdr_uint32(xdrs, &objp->SubserviceId))
        return (FALSE);
    if (!xdr_CSSM_SERVICE_TYPE(xdrs, &objp->SubserviceType))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_NET_ADDRESS(XDR *xdrs, CSSM_NET_ADDRESS *objp)
{
    if (!xdr_CSSM_NET_ADDRESS_TYPE(xdrs, &objp->AddressType))
        return (FALSE);
    if (!xdr_CSSM_DATA(xdrs, &objp->Address))
        return (FALSE);
    return (TRUE);
}

// XXX/cs crypto_data will automagically send callback data when necessary, on the pass out it will reappear in Param, which is also the alternative data sent.  So Callback!=NULL means Param is crypto callback data, otherwise it is param data.
bool_t xdr_CSSM_CRYPTO_DATA(XDR *xdrs, CSSM_CRYPTO_DATA *objp)
{
    void *cb = (void *)objp->Callback;
    if (!xdr_voidptr(xdrs, &cb))
        return (FALSE);
    if (!xdr_voidptr(xdrs, &objp->CallerCtx))
        return (FALSE);

    // Encode callback result if existing, otherwise just param
    // Result comes back in Param
    if (xdrs->x_op == XDR_ENCODE && objp->Callback)
    {
        CSSM_CALLBACK func = objp->Callback; 
        CSSM_DATA data;
        CSSM_RETURN err;
        if (err = func(&data, objp->CallerCtx))
            return (FALSE); // XXX/cs meaningfully return err
        if (!xdr_CSSM_DATA(xdrs, &data))
            return (FALSE);
    }
    else
    {
        if (!xdr_CSSM_DATA(xdrs, &objp->Param))
            return (FALSE);
    }
    return (TRUE);
}

bool_t inline xdr_CSSM_LIST_ELEMENT(XDR *xdrs, CSSM_LIST_ELEMENT *objp)
{
    if (!xdr_CSSM_WORDID_TYPE(xdrs, &objp->WordID))
        return (FALSE);
    if (!xdr_CSSM_LIST_ELEMENT_TYPE(xdrs, &objp->ElementType))
        return (FALSE);
    switch(objp->ElementType) {
    case CSSM_LIST_ELEMENT_DATUM:
        if (!xdr_CSSM_DATA(xdrs, &objp->Element.Word)) return (FALSE); break;
    case CSSM_LIST_ELEMENT_SUBLIST:
        if (!xdr_CSSM_LIST(xdrs, &objp->Element.Sublist)) return (FALSE); break;
    case CSSM_LIST_ELEMENT_WORDID:
        break;
    default:
        secdebug("secxdr", "Illegal CSSM_LIST_ELEMENT type: %u", objp->ElementType); return (FALSE);
    }

    if (!sec_xdr_pointer(xdrs, (uint8_t**)&objp->NextElement, sizeof(CSSM_LIST_ELEMENT), (xdrproc_t)xdr_CSSM_LIST_ELEMENT))
        return (FALSE);

    return (TRUE);
}

bool_t xdr_CSSM_LIST(XDR *xdrs, CSSM_LIST *objp)
{
    if (!xdr_CSSM_LIST_TYPE(xdrs, &objp->ListType))
        return (FALSE);
    if (!sec_xdr_pointer(xdrs, (uint8_t**)&objp->Head, sizeof(CSSM_LIST_ELEMENT), (xdrproc_t)xdr_CSSM_LIST_ELEMENT))
        return (FALSE);
    // if we're restoring things, make sure to fix up Tail to point
    // to the right place
    if (xdrs->x_op == XDR_DECODE)
    {
        bool_t size_alloc = sec_xdr_arena_size_allocator(xdrs);
        if (!size_alloc)
            for (objp->Tail = objp->Head; objp->Tail && objp->Tail->NextElement; objp->Tail = objp->Tail->NextElement);
    }
    return (TRUE);
}

bool_t xdr_CSSM_SAMPLE(XDR *xdrs, CSSM_SAMPLE *objp)
{
    if (!xdr_CSSM_LIST(xdrs, &objp->TypedSample))
        return (FALSE);
    if (!sec_xdr_pointer(xdrs, (uint8_t**)&objp->Verifier, sizeof(CSSM_SUBSERVICE_UID), (xdrproc_t)xdr_CSSM_SUBSERVICE_UID))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_SAMPLEGROUP(XDR *xdrs, CSSM_SAMPLEGROUP *objp)
{
    assert(sizeof(objp->NumberOfSamples) == sizeof(int));
    if (!sec_xdr_array(xdrs, (uint8_t**)&objp->Samples, (u_int *)&objp->NumberOfSamples, ~0, sizeof(CSSM_SAMPLE), (xdrproc_t)xdr_CSSM_SAMPLE))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_ENCODED_CERT(XDR *xdrs, CSSM_ENCODED_CERT *objp)
{

    if (!xdr_CSSM_CERT_TYPE(xdrs, &objp->CertType))
        return (FALSE);
    if (!xdr_CSSM_CERT_ENCODING(xdrs, &objp->CertEncoding))
        return (FALSE);
    if (!xdr_CSSM_DATA(xdrs, &objp->CertBlob))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_CERTGROUP(XDR *xdrs, CSSM_CERTGROUP *objp)
{
    if (!xdr_CSSM_CERT_TYPE(xdrs, &objp->CertType))
        return (FALSE);
    if (!xdr_CSSM_CERT_ENCODING(xdrs, &objp->CertEncoding))
        return (FALSE);

    // NumCerts encoded as part of sec_xdr_array below (we need it
    // before the switch on decode)
    if (!xdr_CSSM_CERTGROUP_TYPE(xdrs, &objp->CertGroupType))
        return (FALSE);

    switch (objp->CertGroupType) {
    case CSSM_CERTGROUP_DATA:
        if (!sec_xdr_array(xdrs, (uint8_t**)&objp->GroupList.CertList, &objp->NumCerts, ~0, sizeof(CSSM_DATA), (xdrproc_t)xdr_CSSM_DATA)) 
            return (FALSE); 
        break;
    case CSSM_CERTGROUP_ENCODED_CERT:
        if (!sec_xdr_array(xdrs, (uint8_t**)&objp->GroupList.EncodedCertList, 
                    &objp->NumCerts, ~0, 
                    sizeof(CSSM_ENCODED_CERT), (xdrproc_t)xdr_CSSM_ENCODED_CERT)) 
            return (FALSE); 
        break;
    case CSSM_CERTGROUP_PARSED_CERT: // unimplemented -> there are no walkers for it
    case CSSM_CERTGROUP_CERT_PAIR:   // unimplemented -> there are no walkers for it
        assert(FALSE);
    default:
        return (FALSE);
    }

    if (!xdr_voidptr(xdrs, &objp->Reserved))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_BASE_CERTS(XDR *xdrs, CSSM_BASE_CERTS *objp)
{
    if (!xdr_CSSM_TP_HANDLE(xdrs, &objp->TPHandle))
        return (FALSE);
    if (!xdr_CSSM_CL_HANDLE(xdrs, &objp->CLHandle))
        return (FALSE);
    if (!xdr_CSSM_CERTGROUP(xdrs, &objp->Certs))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_ACCESS_CREDENTIALS(XDR *xdrs, CSSM_ACCESS_CREDENTIALS *objp)
{
    // XXX/cs this was for executing the callback but we're not doing that apparently void *cb = (void *)objp->Callback;

    if (!xdr_CSSM_STRING(xdrs, objp->EntryTag))
        return (FALSE);
    if (!xdr_CSSM_BASE_CERTS(xdrs, &objp->BaseCerts))
        return (FALSE);
    if (!xdr_CSSM_SAMPLEGROUP(xdrs, &objp->Samples))
        return (FALSE);
    // @@@ treating both Callback and CallerCtx like intptr_t 
    //     in case it ever turns into a magic cookie
    if (!xdr_voidptr(xdrs, (void *)&objp->Callback))
        return (FALSE);
    if (!xdr_voidptr(xdrs, &objp->CallerCtx))
        return (FALSE);

    return (TRUE);
}

bool_t xdr_CSSM_ACCESS_CREDENTIALS_PTR(XDR *xdrs, CSSM_ACCESS_CREDENTIALS_PTR *objp)
{
    return sec_xdr_reference(xdrs, (uint8_t **)objp, sizeof(CSSM_ACCESS_CREDENTIALS), (xdrproc_t)xdr_CSSM_ACCESS_CREDENTIALS);
}

bool_t xdr_CSSM_AUTHORIZATIONGROUP(XDR *xdrs, CSSM_AUTHORIZATIONGROUP *objp)
{
    assert(sizeof(objp->NumberOfAuthTags) == sizeof(int));
    if (!sec_xdr_array(xdrs, (uint8_t **)&objp->AuthTags, (u_int *)&objp->NumberOfAuthTags, ~0, sizeof(CSSM_ACL_AUTHORIZATION_TAG), (xdrproc_t)xdr_CSSM_ACL_AUTHORIZATION_TAG))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_ACL_VALIDITY_PERIOD(XDR *xdrs, CSSM_ACL_VALIDITY_PERIOD *objp)
{
    if (!xdr_CSSM_DATA(xdrs, &objp->StartDate))
        return (FALSE);
    if (!xdr_CSSM_DATA(xdrs, &objp->EndDate))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_ACL_ENTRY_PROTOTYPE(XDR *xdrs, CSSM_ACL_ENTRY_PROTOTYPE *objp)
{
    if (!xdr_CSSM_LIST(xdrs, &objp->TypedSubject))
        return (FALSE);
    //	if (!xdr_CSSM_BOOL(xdrs, &objp->Delegate))
    //		return (FALSE);
    if (!xdr_CSSM_AUTHORIZATIONGROUP(xdrs, &objp->Authorization))
        return (FALSE);
    // XXX/cs enable once securityd stops leaving garbage in here
    //	if (!xdr_CSSM_ACL_VALIDITY_PERIOD(xdrs, &objp->TimeRange))
    //		return (FALSE);
    if (!xdr_CSSM_STRING(xdrs, objp->EntryTag))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_ACL_ENTRY_PROTOTYPE_PTR(XDR *xdrs, CSSM_ACL_ENTRY_PROTOTYPE_PTR *objp)
{
    return sec_xdr_reference(xdrs, (uint8_t **)objp, sizeof(CSSM_ACL_ENTRY_PROTOTYPE), (xdrproc_t)xdr_CSSM_ACL_ENTRY_PROTOTYPE);
}

bool_t xdr_CSSM_ACL_OWNER_PROTOTYPE(XDR *xdrs, CSSM_ACL_OWNER_PROTOTYPE *objp)
{
    if (!xdr_CSSM_LIST(xdrs, &objp->TypedSubject))
        return (FALSE);
    if (!xdr_CSSM_BOOL(xdrs, &objp->Delegate))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_ACL_OWNER_PROTOTYPE_PTR(XDR *xdrs, CSSM_ACL_OWNER_PROTOTYPE_PTR *objp)
{
    return sec_xdr_reference(xdrs, (uint8_t **)objp,sizeof(CSSM_ACL_OWNER_PROTOTYPE), (xdrproc_t)xdr_CSSM_ACL_OWNER_PROTOTYPE);
}

bool_t xdr_CSSM_ACL_ENTRY_INPUT(XDR *xdrs, CSSM_ACL_ENTRY_INPUT *objp)
{
    if (!xdr_CSSM_ACL_ENTRY_PROTOTYPE(xdrs, &objp->Prototype))
        return (FALSE);
    // XXX/cs not currently using this
    // @@@ treating both Callback and CallerCtx like intptr_t 
    //     in case it ever turns into a magic cookie
    //	if (!xdr_voidptr(xdrs, &cb))
    //		return (FALSE);
    //	if (!xdr_voidptr(xdrs, &objp->CallerContext))
    //		return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_ACL_ENTRY_INPUT_PTR(XDR *xdrs, CSSM_ACL_ENTRY_INPUT_PTR *objp)
{
    return sec_xdr_reference(xdrs, (uint8_t **)objp,sizeof(CSSM_ACL_ENTRY_INPUT), (xdrproc_t)xdr_CSSM_ACL_ENTRY_INPUT);
}

bool_t xdr_CSSM_ACL_ENTRY_INFO(XDR *xdrs, CSSM_ACL_ENTRY_INFO *objp)
{

    if (!xdr_CSSM_ACL_ENTRY_PROTOTYPE(xdrs, &objp->EntryPublicInfo))
        return (FALSE);
    if (!xdr_CSSM_ACL_HANDLE(xdrs, &objp->EntryHandle))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_ACL_ENTRY_INFO_ARRAY(XDR *xdrs, CSSM_ACL_ENTRY_INFO_ARRAY *objp) 
{
    return sec_xdr_array(xdrs, (uint8_t **)&objp->acls, (u_int *)&objp->count, ~0, sizeof(CSSM_ACL_ENTRY_INFO), (xdrproc_t)xdr_CSSM_ACL_ENTRY_INFO);
}

bool_t xdr_CSSM_ACL_ENTRY_INFO_ARRAY_PTR(XDR *xdrs, CSSM_ACL_ENTRY_INFO_ARRAY_PTR *objp) 
{
    return sec_xdr_reference(xdrs, (uint8_t **)objp, sizeof(CSSM_ACL_ENTRY_INFO_ARRAY), (xdrproc_t)xdr_CSSM_ACL_ENTRY_INFO_ARRAY);
}


bool_t xdr_CSSM_DATE(XDR *xdrs, CSSM_DATE *objp)
{
    return xdr_opaque(xdrs, (char *)objp, sizeof(CSSM_DATE));
}

bool_t xdr_CSSM_RANGE(XDR *xdrs, CSSM_RANGE *objp)
{

    if (!xdr_uint32(xdrs, &objp->Min))
        return (FALSE);
    if (!xdr_uint32(xdrs, &objp->Max))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_KEYHEADER(XDR *xdrs, CSSM_KEYHEADER *objp)
{

    if (!xdr_CSSM_HEADERVERSION(xdrs, &objp->HeaderVersion))
        return (FALSE);
    if (!xdr_CSSM_GUID(xdrs, &objp->CspId))
        return (FALSE);
    if (!xdr_CSSM_KEYBLOB_TYPE(xdrs, &objp->BlobType))
        return (FALSE);
    if (!xdr_CSSM_KEYBLOB_FORMAT(xdrs, &objp->Format))
        return (FALSE);
    if (!xdr_CSSM_ALGORITHMS(xdrs, &objp->AlgorithmId))
        return (FALSE);
    if (!xdr_CSSM_KEYCLASS(xdrs, &objp->KeyClass))
        return (FALSE);
    if (!xdr_uint32(xdrs, &objp->LogicalKeySizeInBits))
        return (FALSE);
    if (!xdr_CSSM_KEYATTR_FLAGS(xdrs, &objp->KeyAttr))
        return (FALSE);
    if (!xdr_CSSM_KEYUSE(xdrs, &objp->KeyUsage))
        return (FALSE);
    if (!xdr_CSSM_DATE(xdrs, &objp->StartDate))
        return (FALSE);
    if (!xdr_CSSM_DATE(xdrs, &objp->EndDate))
        return (FALSE);
    if (!xdr_CSSM_ALGORITHMS(xdrs, &objp->WrapAlgorithmId))
        return (FALSE);
    if (!xdr_CSSM_ENCRYPT_MODE(xdrs, &objp->WrapMode))
        return (FALSE);
    if (!xdr_uint32(xdrs, &objp->Reserved))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_KEYHEADER_PTR(XDR *xdrs, CSSM_KEYHEADER_PTR *objp)
{
    return sec_xdr_reference(xdrs, (uint8_t **)objp, sizeof(CSSM_KEYHEADER), (xdrproc_t)xdr_CSSM_KEYHEADER);
}

bool_t xdr_CSSM_KEY(XDR *xdrs, CSSM_KEY *objp)
{
    if (!xdr_CSSM_KEYHEADER(xdrs, &objp->KeyHeader))
        return (FALSE);
    if (!xdr_CSSM_DATA(xdrs, &objp->KeyData))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_KEY_PTR(XDR *xdrs, CSSM_KEY_PTR *objp)
{
    if (!sec_xdr_reference(xdrs, (uint8_t **)objp, sizeof(CSSM_KEY), (xdrproc_t)xdr_CSSM_KEY))
        return (FALSE);
    return (TRUE);
}

// CSSM_DATA passed through in the following calls: findFirst, findNext and
// findRecordHandle actually contains a CSSM_KEY if the item is a key.  
// Since a key has byte order sensitive bits it needs to be encoded.
// At this level we can only guess based on the length of the CSSM_DATA passed in
// during encode, whether it's a CSSM_KEY, so we're currently letting securityd
// call xdr_CSSM_KEY_IN_DATA or xdr_CSSM_NO_KEY_IN_DATA to let us know.
bool_t xdr_CSSM_POSSIBLY_KEY_IN_DATA_WITH_BOOL(XDR *xdrs, CSSM_DATA *objp, bool_t in_iskey)
{
    bool_t size_alloc = sec_xdr_arena_size_allocator(xdrs);
    bool_t is_key = FALSE; /* shut compiler up */
    if (xdrs->x_op == XDR_ENCODE)
	is_key = (in_iskey && objp->Length == sizeof(CSSM_KEY));
    if (!xdr_CSSM_BOOL(xdrs, &is_key))
        return (FALSE);
    if (is_key) {
        if (!xdr_CSSM_KEY_PTR(xdrs, (CSSM_KEY_PTR*)&objp->Data))
            return (FALSE);
        if (!size_alloc && (xdrs->x_op == XDR_DECODE))
            objp->Length = sizeof(CSSM_KEY);
    } else {
        if (!xdr_CSSM_DATA(xdrs, objp))
            return (FALSE);
    }
    return (TRUE);
}

bool_t xdr_CSSM_POSSIBLY_KEY_IN_DATA(XDR *xdrs, CSSM_DATA *objp)
{
    return xdr_CSSM_POSSIBLY_KEY_IN_DATA_WITH_BOOL(xdrs, objp, FALSE);
}

bool_t xdr_CSSM_POSSIBLY_KEY_IN_DATA_PTR(XDR *xdrs, CSSM_DATA_PTR *objp)
{
    if (!sec_xdr_reference(xdrs, (uint8_t **)objp, sizeof(CSSM_DATA), (xdrproc_t)xdr_CSSM_POSSIBLY_KEY_IN_DATA))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_KEY_IN_DATA(XDR *xdrs, CSSM_DATA *objp)
{
    return xdr_CSSM_POSSIBLY_KEY_IN_DATA_WITH_BOOL(xdrs, objp, TRUE);
}

bool_t xdr_CSSM_NO_KEY_IN_DATA(XDR *xdrs, CSSM_DATA *objp)
{
    return xdr_CSSM_POSSIBLY_KEY_IN_DATA_WITH_BOOL(xdrs, objp, FALSE);
}

bool_t xdr_CSSM_DB_ATTRIBUTE_INFO(XDR *xdrs, CSSM_DB_ATTRIBUTE_INFO *objp)
{
    if (!xdr_CSSM_DB_ATTRIBUTE_NAME_FORMAT(xdrs, &objp->AttributeNameFormat))
        return (FALSE);
    switch (objp->AttributeNameFormat)
    {
    case CSSM_DB_ATTRIBUTE_NAME_AS_STRING:
        if (!sec_xdr_charp(xdrs, &objp->Label.AttributeName, ~0))
            return (FALSE);
        break;
    case CSSM_DB_ATTRIBUTE_NAME_AS_OID:
        if (!xdr_CSSM_OID(xdrs, &objp->Label.AttributeOID))
            return (FALSE);
        break;
    case CSSM_DB_ATTRIBUTE_NAME_AS_INTEGER: // @@@ apparently unused
        if (!xdr_uint32(xdrs, &objp->Label.AttributeID))
            return (FALSE);
        break;
    default:
        return (FALSE);
    }
    if (!xdr_CSSM_DB_ATTRIBUTE_FORMAT(xdrs, &objp->AttributeFormat))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_DATA_FLIPPED(XDR *xdrs, CSSM_DATA *objp)
{
	bool_t size_alloc = sec_xdr_arena_size_allocator(xdrs);
    if ((xdrs->x_op == XDR_ENCODE) && !size_alloc) {
        switch (objp->Length) {
        case sizeof(uint32_t): *(uint32_t*)objp->Data = htonl(*(uint32_t*)objp->Data); break;
        case sizeof(uint64_t): *(uint64_t*)objp->Data = OSSwapHostToBigInt64(*(uint64_t*)objp->Data); break;
		case sizeof(uint8_t): break;
        default: assert(FALSE); break;
        }
    }
    if (!xdr_CSSM_DATA(xdrs, objp))
        return (FALSE);
    if ((xdrs->x_op == XDR_DECODE) && !size_alloc) {
        switch (objp->Length) {
        case sizeof(uint32_t): *(uint32_t*)objp->Data = ntohl(*(uint32_t*)objp->Data); break;
        case sizeof(uint64_t): *(uint64_t*)objp->Data = OSSwapBigToHostInt64(*(uint64_t*)objp->Data); break;
		case sizeof(uint8_t): break;
        default: assert(FALSE); break;
        }
    }
    return (TRUE);
}

bool_t xdr_CSSM_DB_ATTRIBUTE_DATA(XDR *xdrs, CSSM_DB_ATTRIBUTE_DATA *objp)
{
    if (!xdr_CSSM_DB_ATTRIBUTE_INFO(xdrs, &objp->Info))
        return (FALSE);
    assert(sizeof(objp->NumberOfValues) == sizeof(int));
    CSSM_DB_ATTRIBUTE_FORMAT format = objp->Info.AttributeFormat;
    xdrproc_t proc = (xdrproc_t)xdr_CSSM_DATA; // fallback
    switch(format) {
    case CSSM_DB_ATTRIBUTE_FORMAT_STRING:
    case CSSM_DB_ATTRIBUTE_FORMAT_BLOB:
    case CSSM_DB_ATTRIBUTE_FORMAT_TIME_DATE: // all byte strings
        break;
    case CSSM_DB_ATTRIBUTE_FORMAT_UINT32:
    case CSSM_DB_ATTRIBUTE_FORMAT_SINT32:
    case CSSM_DB_ATTRIBUTE_FORMAT_REAL:
        proc = (xdrproc_t)xdr_CSSM_DATA_FLIPPED;
        break;
		/* XXX/cs unhandled:
			Note that in case of values being passed from CopyIn, it will be normal
			for the format to be set to CSSM_DB_ATTRIBUTE_FORMAT_COMPLEX, as that 
			is the "not-yet-filled-in" value in the CssmDbAttributeInfo constructor 
			(see Record::addAttributes for where this is called).
		 */
	case CSSM_DB_ATTRIBUTE_FORMAT_BIG_NUM:
	case CSSM_DB_ATTRIBUTE_FORMAT_MULTI_UINT32:
	case CSSM_DB_ATTRIBUTE_FORMAT_COMPLEX:
		assert(objp->NumberOfValues == 0);
		break;
	default:
        assert(FALSE);
    }
    if (!sec_xdr_array(xdrs, (uint8_t **)&objp->Value, (u_int *)&objp->NumberOfValues, ~0, sizeof(CSSM_DATA), proc))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_DB_RECORD_ATTRIBUTE_DATA(XDR *xdrs, CSSM_DB_RECORD_ATTRIBUTE_DATA *objp)
{
    if (!xdr_CSSM_DB_RECORDTYPE(xdrs, &objp->DataRecordType))
        return (FALSE);
    if (!xdr_uint32(xdrs, &objp->SemanticInformation))
        return (FALSE);
    assert(sizeof(objp->NumberOfAttributes) == sizeof(int));
    if (!sec_xdr_array(xdrs, (uint8_t **)&objp->AttributeData, (u_int *)&objp->NumberOfAttributes, ~0, sizeof(CSSM_DB_ATTRIBUTE_DATA), (xdrproc_t)xdr_CSSM_DB_ATTRIBUTE_DATA))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR(XDR *xdrs, CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR *objp)
{
    return sec_xdr_reference(xdrs, (uint8_t **)objp, sizeof(CSSM_DB_RECORD_ATTRIBUTE_DATA), (xdrproc_t)xdr_CSSM_DB_RECORD_ATTRIBUTE_DATA);
}

bool_t xdr_CSSM_SELECTION_PREDICATE(XDR *xdrs, CSSM_SELECTION_PREDICATE *objp)
{

    if (!xdr_CSSM_DB_OPERATOR(xdrs, &objp->DbOperator))
        return (FALSE);
    if (!xdr_CSSM_DB_ATTRIBUTE_DATA(xdrs, &objp->Attribute))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_QUERY_LIMITS(XDR *xdrs, CSSM_QUERY_LIMITS *objp)
{

    if (!xdr_uint32(xdrs, &objp->TimeLimit))
        return (FALSE);
    if (!xdr_uint32(xdrs, &objp->SizeLimit))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_QUERY(XDR *xdrs, CSSM_QUERY *objp)
{

    if (!xdr_CSSM_DB_RECORDTYPE(xdrs, &objp->RecordType))
        return (FALSE);
    if (!xdr_CSSM_DB_CONJUNCTIVE(xdrs, &objp->Conjunctive))
        return (FALSE);
    assert(sizeof(objp->NumSelectionPredicates) == sizeof(int));
    if (!sec_xdr_array(xdrs, (uint8_t **)&objp->SelectionPredicate, (u_int *)&objp->NumSelectionPredicates, ~0, sizeof(CSSM_SELECTION_PREDICATE), (xdrproc_t)xdr_CSSM_SELECTION_PREDICATE))
        return (FALSE);
    if (!xdr_CSSM_QUERY_LIMITS(xdrs, &objp->QueryLimits))
        return (FALSE);
    if (!xdr_CSSM_QUERY_FLAGS(xdrs, &objp->QueryFlags))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_QUERY_PTR(XDR *xdrs, CSSM_QUERY_PTR *objp)
{
    return sec_xdr_reference(xdrs, (uint8_t **)objp, sizeof(CSSM_QUERY), (xdrproc_t)xdr_CSSM_QUERY);
}

bool_t xdr_CSSM_CONTEXT_ATTRIBUTE(XDR *xdrs, CSSM_CONTEXT_ATTRIBUTE *objp)
{
    if (!xdr_CSSM_ATTRIBUTE_TYPE(xdrs, &objp->AttributeType))
        return (FALSE);
    // @@@ original walkers skirt the issue: set to 0 on copyin, set to sizeof(attr) on copyout - all attrs do have internal size or null termination.
    if (!xdr_uint32(xdrs, &objp->AttributeLength)) 
        return (FALSE);
    switch(objp->AttributeType & CSSM_ATTRIBUTE_TYPE_MASK)
    {
    case CSSM_ATTRIBUTE_DATA_CSSM_DATA:
        if (!sec_xdr_reference(xdrs, (uint8_t **)&objp->Attribute.Data, sizeof(CSSM_DATA), (xdrproc_t)xdr_CSSM_DATA)) return (FALSE); break;
    case CSSM_ATTRIBUTE_DATA_CRYPTO_DATA:
        if (!sec_xdr_reference(xdrs, (uint8_t **)&objp->Attribute.CryptoData, sizeof(CSSM_CRYPTO_DATA), (xdrproc_t)xdr_CSSM_CRYPTO_DATA)) return (FALSE); break;
    case CSSM_ATTRIBUTE_DATA_KEY:
        if (!sec_xdr_reference(xdrs, (uint8_t **)&objp->Attribute.Key, sizeof(CSSM_KEY), (xdrproc_t)xdr_CSSM_KEY)) return (FALSE); break;
    case CSSM_ATTRIBUTE_DATA_STRING:
        if (!sec_xdr_charp(xdrs, &objp->Attribute.String, ~0)) return (FALSE); break;
    case CSSM_ATTRIBUTE_DATA_DATE:
        if (!sec_xdr_reference(xdrs, (uint8_t **)&objp->Attribute.Date, sizeof(CSSM_DATE), (xdrproc_t)xdr_CSSM_DATE)) return (FALSE); break;
    case CSSM_ATTRIBUTE_DATA_RANGE:
        if (!sec_xdr_reference(xdrs, (uint8_t **)&objp->Attribute.Range, sizeof(CSSM_RANGE), (xdrproc_t)xdr_CSSM_RANGE)) return (FALSE); break;
    case CSSM_ATTRIBUTE_DATA_ACCESS_CREDENTIALS:
        if (!sec_xdr_reference(xdrs, (uint8_t **)&objp->Attribute.AccessCredentials, sizeof(CSSM_ACCESS_CREDENTIALS), (xdrproc_t)xdr_CSSM_ACCESS_CREDENTIALS)) return (FALSE); break;
    case CSSM_ATTRIBUTE_DATA_VERSION:
        if (!sec_xdr_reference(xdrs, (uint8_t **)&objp->Attribute.Version, sizeof(CSSM_VERSION), (xdrproc_t)xdr_CSSM_VERSION)) return (FALSE); break;
    case CSSM_ATTRIBUTE_DATA_DL_DB_HANDLE:
        if (!sec_xdr_reference(xdrs, (uint8_t **)&objp->Attribute.DLDBHandle, sizeof(CSSM_DL_DB_HANDLE), (xdrproc_t)xdr_CSSM_DL_DB_HANDLE)) return (FALSE); break;
    case CSSM_ATTRIBUTE_NONE:
        break;
    case CSSM_ATTRIBUTE_DATA_UINT32:
        if (!xdr_uint32(xdrs, &objp->Attribute.Uint32))
            return (FALSE);
        break;
    default:
        return (FALSE);
    }

    return (TRUE);
}

bool_t xdr_CSSM_CONTEXT(XDR *xdrs, CSSM_CONTEXT *objp)
{
    if (!xdr_CSSM_CONTEXT_TYPE(xdrs, &objp->ContextType))
        return (FALSE);
    if (!xdr_CSSM_ALGORITHMS(xdrs, &objp->AlgorithmType))
        return (FALSE);
    if (!sec_xdr_array(xdrs, (uint8_t **)&objp->ContextAttributes, (u_int *)&objp->NumberOfAttributes, ~0, sizeof(CSSM_CONTEXT_ATTRIBUTE), (xdrproc_t)xdr_CSSM_CONTEXT_ATTRIBUTE))
        return (FALSE);
    if (!xdr_CSSM_CSP_HANDLE(xdrs, &objp->CSPHandle))
        return (FALSE);
    if (!xdr_CSSM_BOOL(xdrs, &objp->Privileged))
        return (FALSE);
    if (!xdr_uint32(xdrs, &objp->EncryptionProhibited))
        return (FALSE);
    if (!xdr_uint32(xdrs, &objp->WorkFactor))
        return (FALSE);
    if (!xdr_uint32(xdrs, &objp->Reserved))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_CONTEXT_PTR(XDR *xdrs, CSSM_CONTEXT_PTR *objp)
{
    return sec_xdr_reference(xdrs, (uint8_t **)objp, sizeof(CSSM_CONTEXT), (xdrproc_t)xdr_CSSM_CONTEXT);
}

// this is possibly not actually used in favor of the flatidentifier
bool_t xdr_CSSM_DL_DB_HANDLE(XDR *xdrs, CSSM_DL_DB_HANDLE *objp)
{
    if (!xdr_CSSM_DL_HANDLE(xdrs, &objp->DLHandle))
        return (FALSE);
    if (!xdr_CSSM_DB_HANDLE(xdrs, &objp->DBHandle))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_PKCS5_PBKDF2_PARAMS(XDR *xdrs, CSSM_PKCS5_PBKDF2_PARAMS *objp)
{
    if (!xdr_CSSM_DATA(xdrs, &objp->Passphrase))
        return (FALSE);
    if (!xdr_CSSM_PKCS5_PBKDF2_PRF(xdrs, &objp->PseudoRandomFunction))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_DERIVE_DATA(XDR *xdrs, CSSM_DERIVE_DATA *objp)
{
    if (!xdr_CSSM_ALGORITHMS(xdrs,&objp->algorithm))
        return (FALSE);
    switch (objp->algorithm) {
    case CSSM_ALGID_PKCS5_PBKDF2:
        if ((xdrs->x_op == XDR_ENCODE) &&
                (!objp->baseData.Data) &&
                (objp->baseData.Length != sizeof(CSSM_PKCS5_PBKDF2_PARAMS)))
            return (FALSE); //CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_ALG_PARAMS);
        if (!sec_xdr_reference(xdrs, &(objp->baseData.Data), sizeof(CSSM_PKCS5_PBKDF2_PARAMS), (xdrproc_t)xdr_CSSM_PKCS5_PBKDF2_PARAMS))
            return (FALSE);
        objp->baseData.Length = sizeof(CSSM_PKCS5_PBKDF2_PARAMS);
        break;
    default:
        if (!xdr_CSSM_DATA(xdrs, &objp->baseData))
            return (FALSE);
        break;
    }
    return (TRUE);
}

bool_t xdr_CSSM_DERIVE_DATA_PTR(XDR *xdrs, CSSM_DERIVE_DATA **objp)
{
    return sec_xdr_reference(xdrs, (uint8_t **)objp, sizeof(CSSM_DERIVE_DATA), (xdrproc_t)xdr_CSSM_DERIVE_DATA);
}

bool_t xdr_CSSM_ACL_OWNER_PROTOTYPE_ARRAY(XDR *xdrs, CSSM_ACL_OWNER_PROTOTYPE_ARRAY *objp)
{
    if (!sec_xdr_array(xdrs, (uint8_t **)&objp->acls, (u_int *)&objp->count, ~0, sizeof(CSSM_ACL_OWNER_PROTOTYPE), (xdrproc_t)xdr_CSSM_ACL_OWNER_PROTOTYPE))
        return (FALSE);
    return (TRUE);
}


#if 0 /* unimplemented in current stack */

bool_t xdr_CSSM_FIELD(XDR *xdrs, CSSM_FIELD *objp)
{

    if (!xdr_CSSM_OID(xdrs, &objp->FieldOid))
        return (FALSE);
    if (!xdr_CSSM_DATA(xdrs, &objp->FieldValue))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_FIELDGROUP(XDR *xdrs, CSSM_FIELDGROUP *objp)
{
    assert(sizeof(objp->NumberOfFields) == sizeof(int));
    if (!sec_xdr_array(xdrs, (uint8_t**)&objp->Fields, (u_int *)&objp->NumberOfFields, ~0, sizeof(CSSM_FIELD), (xdrproc_t)xdr_CSSM_FIELD))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_TUPLE(XDR *xdrs, CSSM_TUPLE *objp)
{
    if (!xdr_CSSM_LIST(xdrs, &objp->Issuer))
        return (FALSE);
    if (!xdr_CSSM_LIST(xdrs, &objp->Subject))
        return (FALSE);
    if (!xdr_CSSM_BOOL(xdrs, &objp->Delegate))
        return (FALSE);
    if (!xdr_CSSM_LIST(xdrs, &objp->AuthorizationTag))
        return (FALSE);
    if (!xdr_CSSM_LIST(xdrs, &objp->ValidityPeriod))
        return (FALSE);
    return (TRUE);
}

bool_t xdr_CSSM_PARSED_CERT(XDR *xdrs, CSSM_PARSED_CERT *objp)
{
    if (!xdr_CSSM_CERT_TYPE(xdrs, &objp->CertType))
        return (FALSE);
    switch (objp->ParsedCertFormat)
    {
    case CSSM_CERT_PARSE_FORMAT_NONE:
    case CSSM_CERT_PARSE_FORMAT_CUSTOM:		/* void* */
        /* XXX/gh  SOL? */
        break;
    case CSSM_CERT_PARSE_FORMAT_SEXPR:
        if (!xdr_CSSM_LIST(xdrs, (CSSM_LIST *)objp->ParsedCert))
            return (FALSE);
        break;
    case CSSM_CERT_PARSE_FORMAT_COMPLEX:	/* void* */
        /* XXX/gh  SOL? */
        break;
    case CSSM_CERT_PARSE_FORMAT_OID_NAMED:
        if (!xdr_CSSM_FIELDGROUP(xdrs, (CSSM_FIELDGROUP *)objp->ParsedCert))
            return (FALSE);
        break;
    case CSSM_CERT_PARSE_FORMAT_TUPLE:
        if (!xdr_CSSM_TUPLE(xdrs, (CSSM_TUPLE *)objp->ParsedCert))
            return (FALSE);
        break;
    case CSSM_CERT_PARSE_FORMAT_MULTIPLE:
        /* multiple forms; each cert carries a parse format indicator */
        /* XXX/gh  ??? */
        break;
    case CSSM_CERT_PARSE_FORMAT_LAST:
        /* XXX/gh  ??? */
        break;
    case CSSM_CL_CUSTOM_CERT_PARSE_FORMAT:
        /* XXX/gh  ??? */
        break;
    default:
        return (FALSE);
    }
    return (TRUE);
}

bool_t xdr_CSSM_CERT_PAIR(XDR *xdrs, CSSM_CERT_PAIR *objp)
{

    if (!xdr_CSSM_ENCODED_CERT(xdrs, &objp->EncodedCert))
        return (FALSE);
    if (!xdr_CSSM_PARSED_CERT(xdrs, &objp->ParsedCert))
        return (FALSE);
    return (TRUE);
}

#endif /* unimplemented in current stack */

