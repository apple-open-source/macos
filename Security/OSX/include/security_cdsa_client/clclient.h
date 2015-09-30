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
#ifndef _H_CDSA_CLIENT_CLCLIENT
#define _H_CDSA_CLIENT_CLCLIENT  1

#include <security_cdsa_client/cssmclient.h>
#include <security_cdsa_utilities/cssmcert.h>


namespace Security {
namespace CssmClient {


//
// A CL attachment
//
class CLImpl : public AttachmentImpl
{
public:
	CLImpl(const Guid &guid);
	CLImpl(const Module &module);
	virtual ~CLImpl();
    
};

class CL : public Attachment
{
public:
	typedef CLImpl Impl;

	explicit CL(Impl *impl) : Attachment(impl) {}
	CL(const Guid &guid) : Attachment(new Impl(guid)) {}
	CL(const Module &module) : Attachment(new Impl(module)) {}

	Impl *operator ->() const { return &impl<Impl>(); }
	Impl &operator *() const { return impl<Impl>(); }
};


//
// A self-building CertGroup.
// This is a CertGroup, but it's NOT A PODWRAPPER (it's larger).
//
class BuildCertGroup : public CertGroup {
public:
    BuildCertGroup(CSSM_CERT_TYPE ctype, CSSM_CERT_ENCODING encoding,
        CSSM_CERTGROUP_TYPE type, Allocator &alloc = Allocator::standard());
    
    CssmVector<CSSM_DATA, CssmData> certificates;
};


} // end namespace CssmClient
} // end namespace Security

#endif // _H_CDSA_CLIENT_CLCLIENT
