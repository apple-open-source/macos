/*
 * Copyright (c) 2000-2002,2011,2014 Apple Inc. All Rights Reserved.
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


//
// clclient - client interface to CSSM CLs and their operations
//
#include <security_cdsa_client/clclient.h>

using namespace CssmClient;


//
// Manage CL attachments
//
CLImpl::CLImpl(const Guid &guid) : AttachmentImpl(guid, CSSM_SERVICE_CL)
{
}

CLImpl::CLImpl(const Module &module) : AttachmentImpl(module, CSSM_SERVICE_CL)
{
}

CLImpl::~CLImpl()
{
}


//
// A BuildCertGroup
//
BuildCertGroup::BuildCertGroup(CSSM_CERT_TYPE ctype, CSSM_CERT_ENCODING encoding,
    CSSM_CERTGROUP_TYPE type, Allocator &alloc)
    : certificates(NumCerts, GroupList.CertList)
{
    clearPod();
    CertType = ctype;
    CertEncoding = encoding;
    CertGroupType = type;
}
