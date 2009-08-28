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
	
	/*
		"record" and "keyhandle" below come from the underlying TOKEND_RETURN_DATA,
		and are native-sized quantities. These need to be converted to a client 
		handle (32 bit)
	 */
	
	hKey = (this->keyhandle)?TokendHandleObject::make(this->keyhandle)->ipcHandle():0;
	hRecord = (this->record)?TokendHandleObject::make(this->record)->ipcHandle():0;
	
	server->freeRetrievedData(this);
	//@@@ deferred-release outAttributes == Copier
}

/*
	Cheat sheet
 
	CSSM_HANDLE
		- native size, 64 bit or 32 bit
		- passed along to Tokend e.g. Tokend::Token::_findRecordHandle
		- passed e.g. as CALL(findNext, ...)
 
	RecordHandle & SearchHandle
		- always 32 bit
		- returned to callers through SecurityTokend interface
*/

TokendHandleObject::TokendHandleObject(CSSM_HANDLE cssmh) : mTokendHandle(cssmh)
{ 
    thmstate().add(ipcHandle(), this);
}

TokendHandleObject::~TokendHandleObject()
{
    thmstate().erase(ipcHandle());
}


ModuleNexus<TokendHandleObject::TokendHandleMapState> TokendHandleObject::thmstate;

TokendHandleObject::TokendHandleMapState::TokendHandleMapState()
{
}

// static factory method
TokendHandleObject *TokendHandleObject::make(CSSM_HANDLE cssmh)
{
	// This will create a new TokendHandleObject, add it to the map, and return
	// the value of the newly created object
	if (!cssmh)
		CssmError::throwMe(CSSMERR_CSSM_INVALID_ADDIN_HANDLE);
	return new TokendHandleObject(cssmh);
}
	
// static method
CSSM_HANDLE TokendHandleObject::findTDHandle(TokendHandleObject::TransitHandle thdl)
{
	/*
		Find a handle that we can pass along to tokend. We do this so that Tokend
		can return different errors for different calls -- even if we don't find
		it in our map, we will still call out to Tokend. It might be better to
		use a distinctive non-zero value for the bad handle for tracking
	*/
	const CSSM_HANDLE knownBadHandle = 0;	// for now
	
	if (!thdl)
		return knownBadHandle; 
	
	TokendHandleMap::const_iterator it = thmstate().find(thdl);
	return (it == thmstate().end()) ? knownBadHandle : it->second->tokendHandle();
}

void TokendHandleObject::TokendHandleMapState::add(TransitHandle thdl, TokendHandleObject *tho)
{
	StLock<Mutex> _(*this);
	thmstate().insert(thmstate().end(), make_pair(thdl,tho));
}

void TokendHandleObject::TokendHandleMapState::erase(TransitHandle thdl)
{
	StLock<Mutex> _(*this);
	// @@@  check cssmh, don't erase if not valid
	TokendHandleMap::erase(thdl);
}
	
}	// namespace Tokend
}	// namespace Security

