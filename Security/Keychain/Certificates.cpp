/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
	File:		Certificates.cpp

	Contains:	Working with Certificates

	Copyright:	2002 by Apple Computer, Inc., all rights reserved.

	To Do:
*/

#include <Security/Certificates.h>
#include <Security/CertLibRef.h>//%%%should be included in Certificates.h

using namespace KeychainCore;

void CertificateImpl::CertificateImplCommonInit(CSSM_CERT_TYPE type)
{
    mType = type;
    mCLReference = NULL;
    //
    // Create a CL reference for this certificate type.
    // %%%find us the 1st CL reference we can find for this cert type (this can change)
    //
	CertLibCursorImpl* cursor = NULL;
    cursor = new CertLibCursorImpl(type);
    if (!cursor)
        MacOSError::throwMe(errSecItemNotFound/*%%%*/);
    
    CertLib certLib;//%%%allocated on the stack?!
    if (!cursor->next(certLib))
    {
        delete cursor;
        MacOSError::throwMe(errSecItemNotFound/*%%%*/);
    }
    delete cursor;
    
	mCLReference = CertLibRef::handle(certLib);	// 'tis a SecCertificateLibraryRef
}

CertificateImpl::CertificateImpl(const CSSM_DATA* data, CSSM_CERT_TYPE type):
    mItem(NULL)
{
    CertificateImplCommonInit(type);
    (void*)mData.Data = malloc(data->Length);
    memcpy(mData.Data, data->Data, data->Length);
    mData.Length = data->Length;
}

CertificateImpl::CertificateImpl(SecKeychainItemRef item, CSSM_CERT_TYPE type)
{
    CertificateImplCommonInit(type);
    mItem = item;
    SecRetain(item);
    mData.Data = NULL;
    mData.Length = 0;
}

CertificateImpl::~CertificateImpl()
{
    if (mData.Data)
    {
        if (mItem)
            SecKeychainItemFreeContent(NULL, mData.Data);	// free if copied via SecKeychainItemCopyContent.
        else
            free(mData.Data);	// free if copied from the caller when cert ref was created.
    }
    if (mItem)
        SecRelease(mItem);
    
    if (mCLReference)
        SecRelease(mCLReference);
}

CSSM_DATA* CertificateImpl::getData()
{
    if (mItem)
    {
        if (mData.Data)
            SecKeychainItemFreeContent(NULL, mData.Data);
        
        OSStatus result = SecKeychainItemCopyContent(mItem, NULL, NULL, &mData.Length, (void**)&(mData.Data));
        if (result)
            MacOSError::throwMe(result);
    }	// otherwise, return the data originally specified when the cert ref was created.
    return &mData; 
}

CSSM_X509_NAME* CertificateImpl::getSubject()
{
    return NULL;//%%%use mCLReference to get subject
}

CSSM_X509_NAME* CertificateImpl::getIssuer()
{
    return NULL;//%%%use mCLReference to get issuer
}
