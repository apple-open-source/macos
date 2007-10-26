/*
 * Copyright (c) 2000-2004,2006 Apple Computer, Inc. All Rights Reserved.
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


//
// Miscellaneous CSSM PODWrappers
//
#ifndef _H_CSSMPODS
#define _H_CSSMPODS

#include <security_utilities/utilities.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <string>


namespace Security {


//
// User-friendly GUIDs
//
class Guid : public PodWrapper<Guid, CSSM_GUID> {
public:
    Guid() { /*IFDEBUG(*/ memset(this, 0, sizeof(*this)) /*)*/ ; }
    Guid(const CSSM_GUID &rGuid) { memcpy(this, &rGuid, sizeof(*this)); }
    Guid(const char *string);
	Guid(const std::string &s);

    Guid &operator = (const CSSM_GUID &rGuid)
    { memcpy(this, &rGuid, sizeof(CSSM_GUID)); return *this; }
   
    bool operator == (const CSSM_GUID &other) const
    { return (this == &other) || !memcmp(this, &other, sizeof(CSSM_GUID)); }
    bool operator != (const CSSM_GUID &other) const
    { return (this != &other) && memcmp(this, &other, sizeof(CSSM_GUID)); }
    bool operator < (const CSSM_GUID &other) const
    { return memcmp(this, &other, sizeof(CSSM_GUID)) < 0; }
    size_t hash() const {	//@@@ revisit this hash
        return Data1 + Data2 << 3 + Data3 << 11 + Data4[3] + Data4[6] << 22;
    }

    static const unsigned stringRepLength = 38;	// "{x8-x4-x4-x4-x12}"
    char *toString(char buffer[stringRepLength+1]) const;	// will append \0
	string toString() const;	// make std::string
	
private:
	void parseGuid(const char *string);
};

class CssmGuidData : public CssmData {
public:
	CssmGuidData(const CSSM_GUID &guid);
	
private:
	char buffer[Guid::stringRepLength + 1];
};


//
// User-friendly CSSM_SUBSERVICE_UIDs
//
class CssmSubserviceUid : public PodWrapper<CssmSubserviceUid, CSSM_SUBSERVICE_UID> {
public:
    CssmSubserviceUid() { clearPod(); }
    CssmSubserviceUid(const CSSM_SUBSERVICE_UID &rSSuid) { memcpy(this, &rSSuid, sizeof(*this)); }

    CssmSubserviceUid &operator = (const CSSM_SUBSERVICE_UID &rSSuid)
    { memcpy(this, &rSSuid, sizeof(CSSM_SUBSERVICE_UID)); return *this; }
   
    bool operator == (const CSSM_SUBSERVICE_UID &other) const;
    bool operator != (const CSSM_SUBSERVICE_UID &other) const { return !(*this == other); }
    bool operator < (const CSSM_SUBSERVICE_UID &other) const;

    CssmSubserviceUid(const CSSM_GUID &guid, const CSSM_VERSION *version = NULL,
		uint32 subserviceId = 0,
        CSSM_SERVICE_TYPE subserviceType = CSSM_SERVICE_DL);

	const ::Guid &guid() const { return ::Guid::overlay(Guid); }
	uint32 subserviceId() const { return SubserviceId; }
	CSSM_SERVICE_TYPE subserviceType() const { return SubserviceType; }
	CSSM_VERSION version() const { return Version; }
};


//
// User-friendler CSSM_CRYPTO_DATA objects
//
class CryptoCallback {
public:
	CryptoCallback(CSSM_CALLBACK func, void *ctx = NULL) : mFunction(func), mCtx(ctx) { }
	CSSM_CALLBACK function() const { return mFunction; }
	void *context() const { return mCtx; }

	CssmData operator () () const
	{
		CssmData output;
		if (CSSM_RETURN err = mFunction(&output, mCtx))
			CssmError::throwMe(err);
		return output;
	}

private:
	CSSM_CALLBACK mFunction;
	void *mCtx;
};

class CssmCryptoData : public PodWrapper<CssmCryptoData, CSSM_CRYPTO_DATA> {
public:
	CssmCryptoData() { }
	
	CssmCryptoData(const CssmData &param, CSSM_CALLBACK callback = NULL, void *ctx = NULL)
	{ Param = const_cast<CssmData &>(param); Callback = callback; CallerCtx = ctx; }

	CssmCryptoData(const CssmData &param, CryptoCallback &cb)
	{ Param = const_cast<CssmData &>(param); Callback = cb.function(); CallerCtx = cb.context(); }
	
	CssmCryptoData(CSSM_CALLBACK callback, void *ctx = NULL)
	{ /* ignore Param */ Callback = callback; CallerCtx = ctx; }
	
	explicit CssmCryptoData(CryptoCallback &cb)
	{ /* ignore Param */ Callback = cb.function(); CallerCtx = cb.context(); }

	// member access
	CssmData &param() { return CssmData::overlay(Param); }
	const CssmData &param() const { return CssmData::overlay(Param); }
	bool hasCallback() const { return Callback != NULL; }
	CryptoCallback callback() const { return CryptoCallback(Callback, CallerCtx); }

	// get the value, whichever way is appropriate
	CssmData operator () () const
	{ return hasCallback() ? callback() () : param(); }
};

// a CssmCryptoContext whose callback is a virtual class member
class CryptoDataClass : public CssmCryptoData {
public:
	CryptoDataClass() : CssmCryptoData(callbackShim, this) { }
	virtual ~CryptoDataClass();
	
protected:
	virtual CssmData yield() = 0;	// must subclass and implement this
	
private:
	static CSSM_RETURN callbackShim(CSSM_DATA *output, void *ctx);
};


//
// Other PodWrappers for stuff that is barely useful...
//
class CssmKeySize : public PodWrapper<CssmKeySize, CSSM_KEY_SIZE> {
public:
    CssmKeySize() { }
    CssmKeySize(uint32 nom, uint32 eff) { LogicalKeySizeInBits = nom; EffectiveKeySizeInBits = eff; }
    CssmKeySize(uint32 size) { LogicalKeySizeInBits = EffectiveKeySizeInBits = size; }
    
    uint32 logical() const		{ return LogicalKeySizeInBits; }
    uint32 effective() const	{ return EffectiveKeySizeInBits; }
    operator uint32 () const	{ return effective(); }
};

inline bool operator == (const CSSM_KEY_SIZE &s1, const CSSM_KEY_SIZE &s2)
{
    return s1.LogicalKeySizeInBits == s2.LogicalKeySizeInBits
        && s1.EffectiveKeySizeInBits == s2.EffectiveKeySizeInBits;
}

inline bool operator != (const CSSM_KEY_SIZE &s1, const CSSM_KEY_SIZE &s2)
{ return !(s1 == s2); }


class QuerySizeData : public PodWrapper<QuerySizeData, CSSM_QUERY_SIZE_DATA> {
public:
    QuerySizeData() { }
	QuerySizeData(uint32 in) { SizeInputBlock = in; SizeOutputBlock = 0; }
	
	uint32 inputSize() const { return SizeInputBlock; }
	uint32 inputSize(uint32 size) { return SizeInputBlock = size; }
	uint32 outputSize() const { return SizeOutputBlock; }
};

inline bool operator == (const CSSM_QUERY_SIZE_DATA &s1, const CSSM_QUERY_SIZE_DATA &s2)
{
    return s1.SizeInputBlock == s2.SizeInputBlock
        && s1.SizeOutputBlock == s2.SizeOutputBlock;
}

inline bool operator != (const CSSM_QUERY_SIZE_DATA &s1, const CSSM_QUERY_SIZE_DATA &s2)
{ return !(s1 == s2); }


class CSPOperationalStatistics : 
	public PodWrapper<CSPOperationalStatistics, CSSM_CSP_OPERATIONAL_STATISTICS> {
public:
};


} // end namespace Security


#endif //_H_CSSMPODS
