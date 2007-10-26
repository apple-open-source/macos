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
#include "transit.h"


namespace Security {
namespace Tokend {


//
// Relocation support
//
void relocate(Context &context, void *base, Context::Attr *attrs, uint32 attrSize)
{
	ReconstituteWalker relocator(attrs, base);
	context.ContextAttributes = attrs;	// fix context->attr vector link
	for (uint32 n = 0; n < context.attributesInUse(); n++)
		walk(relocator, context[n]);
}


//
// Data search and retrieval interface
//
DataRetrieval::DataRetrieval(CssmDbRecordAttributeData *inAttributes, bool getData)
{
	this->attributes = inAttributes;
	this->data = getData ? &mData : NULL;
	this->record = CSSM_INVALID_HANDLE;
	this->keyhandle = CSSM_INVALID_HANDLE;
}


DataRetrieval::~DataRetrieval()
{
}


void DataRetrieval::returnData(KeyHandle &hKey, RecordHandle &hRecord,
	void *&outData, mach_msg_type_number_t &outDataLength,
	CssmDbRecordAttributeData *&outAttributes, mach_msg_type_number_t &outAttributesLength,
	CssmDbRecordAttributeData *&outAttributesBase)
{
	Copier<CssmDbRecordAttributeData> outCopy(CssmDbRecordAttributeData::overlay(this->attributes));
	outAttributesLength = outCopy.length();
	outAttributes = outAttributesBase = outCopy.keep();
	if (this->data) {
		outData = this->data->Data;
		outDataLength = this->data->Length;
	} else {
		outData = NULL;
		outDataLength = 0;
	}
	hKey = this->keyhandle;
	hRecord = this->record;
	server->freeRetrievedData(this);
	//@@@ deferred-release outAttributes == Copier
}


}	// namespace Tokend
}	// namespace Security
