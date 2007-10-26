/*
 *  Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */
#ifndef _H_TRANSIT
#define _H_TRANSIT

#include "tdclient.h"
#include "tokend.h"
#include "server.h"
#include <security_cdsa_utilities/cssmwalkers.h>


namespace Security {
namespace Tokend {


using namespace Security::Tokend;
using namespace Security::DataWalkers;


#define TOKEND_ARGS \
	mach_port_t servicePort, mach_port_t replyPort, CSSM_RETURN *rcode

#define CONTEXT_ARGS Context context, Pointer contextBase, Context::Attr *attributes, mach_msg_type_number_t attrSize

#define BEGIN_IPC	*rcode = CSSM_OK; try {
#define END_IPC(base) } \
	catch (const CommonError &err) { *rcode = CssmError::cssmError(err, CSSM_ ## base ## _BASE_ERROR); } \
	catch (const std::bad_alloc &) { *rcode = CssmError::merge(CSSM_ERRCODE_MEMORY_ERROR, CSSM_ ## base ## _BASE_ERROR); } \
	catch (...) { *rcode = CssmError::cssmError(CSSM_ERRCODE_INTERNAL_ERROR, CSSM_ ## base ## _BASE_ERROR); } \
	return KERN_SUCCESS;

#define DATA_IN(base)	void *base, mach_msg_type_number_t base##Length
#define DATA_OUT(base)	void **base, mach_msg_type_number_t *base##Length
#define DATA(base)		CssmData(base, base##Length)

#define COPY_IN(type,name)	type *name, mach_msg_type_number_t name##Length, type *name##Base
#define COPY_OUT(type,name)	\
	type **name, mach_msg_type_number_t *name##Length, type **name##Base


#define CALL(func,args)	{ \
	if (server->func) \
		{ if (*rcode = server->func args) return KERN_SUCCESS; /* and rcode */ } \
	else \
		CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED); }


using LowLevelMemoryUtilities::increment;
using LowLevelMemoryUtilities::difference;


//
// An OutputData object will take memory allocated within the SecurityServer,
// hand it to the MIG return-output parameters, and schedule it to be released
// after the MIG reply has been sent. It will also get rid of it in case of
// error.
//
class OutputData : public CssmData {
public:
	OutputData(void **outP, mach_msg_type_number_t *outLength)
		: mData(*outP), mLength(*outLength) { }
	~OutputData()
	{ mData = data(); mLength = length(); server->releaseWhenDone(mData); }
    
    void operator = (const CssmData &source)
    { CssmData::operator = (source); }
	
private:
	void * &mData;
	mach_msg_type_number_t &mLength;
};


//
// A local copy of a structured return value (COPY_OUT form), self-managing
//
template <class T>
class Return : public T {
public:
	Return(T **p, mach_msg_type_number_t *l, T **b)
		: ptr(*p), len(*l), base(*b) { ptr = NULL; }
	~Return();

private:
	T *&ptr;
	mach_msg_type_number_t &len;
	T *&base;
};

template <class T>
Return<T>::~Return()
{
	Copier<T> copier(static_cast<T*>(this), Allocator::standard());
	ptr = base = copier;
	len = copier.length();
	server->releaseWhenDone(copier.keep());
}


//
// Relocation support
//
void relocate(Context &context, void *base, Context::Attr *attrs, uint32 attrSize);


//
// Data search and retrieval interface
//
class DataRetrieval : public TOKEND_RETURN_DATA {
public:
	DataRetrieval(CssmDbRecordAttributeData *inAttributes, bool getData);
	~DataRetrieval();
	void returnData(KeyHandle &hKey, RecordHandle &hRecord,
		void *&outData, mach_msg_type_number_t &outDataLength,
		CssmDbRecordAttributeData *&outAttributes, mach_msg_type_number_t &outAttrLength,
		CssmDbRecordAttributeData *&outAttributesBase);
	
private:
	CssmData mData;
};

}	// namespace Tokend
}	// namespace Security

#endif //_H_TRANSIT
