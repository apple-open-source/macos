/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
 * cssm utilities
 */
#ifndef _H_UTILITIES
#define _H_UTILITIES

#include <Security/cssm.h>
#include <Security/utility_config.h>
#include <exception>
#include <new>
#include <string>
#include <errno.h>
#include <string.h>

#ifdef _CPP_UTILITIES
#pragma export on
#endif

namespace Security
{

//
// Elementary debugging support.
// #include <debugging.h> for more debugging facilities.
//
#if defined(NDEBUG)

# define safe_cast	static_cast
# define safer_cast	static_cast

# define IFDEBUG(it)	/* do nothing */
# define IFNDEBUG(it)	it

#else

template <class Derived, class Base>
inline Derived safer_cast(Base &base)
{
    return dynamic_cast<Derived>(base);
}

template <class Derived, class Base>
inline Derived safe_cast(Base *base)
{
    if (base == NULL)
        return NULL;	// okay to cast NULL to NULL
    Derived p = dynamic_cast<Derived>(base);
    assert(p);
    return p;
}

# define IFDEBUG(it)	it
# define IFNDEBUG(it)	/* do nothing */

#endif //NDEBUG


//
// Place this into your class definition if you don't want it to be copyable
// or asignable. This will not prohibit allocation on the stack or in static
// memory, but it will make anything derived from it, and anything containing
// it, fixed-once-created. A proper object, I suppose.
//
#define NOCOPY(Type)	private: Type(const Type &); void operator = (const Type &);


//
// Exception hierarchy
//
class CssmCommonError : public std::exception {
protected:
    CssmCommonError();
    CssmCommonError(const CssmCommonError &source);
public:
    virtual ~CssmCommonError() throw ();

    virtual CSSM_RETURN cssmError() const = 0;
    virtual CSSM_RETURN cssmError(CSSM_RETURN base) const;
    virtual OSStatus osStatus() const;
	virtual int unixError() const;
	
protected:
	virtual void debugDiagnose(const void *id) const;	// used internally for debug logging
	
private:
	IFDEBUG(mutable bool mCarrier);	// primary carrier of exception flow
};

class CssmError : public CssmCommonError {
protected:
    CssmError(CSSM_RETURN err);
public:
    const CSSM_RETURN error;
    virtual CSSM_RETURN cssmError() const;
    virtual OSStatus osStatus() const;
    virtual const char *what () const throw ();

    static CSSM_RETURN merge(CSSM_RETURN error, CSSM_RETURN base);
    
	static void check(CSSM_RETURN error)	{ if (error != CSSM_OK) throwMe(error); }
    static void throwMe(CSSM_RETURN error) __attribute__((noreturn));
};

class UnixError : public CssmCommonError {
protected:
    UnixError();
    UnixError(int err);
public:
    const int error;
    virtual CSSM_RETURN cssmError() const;
    virtual OSStatus osStatus() const;
	virtual int unixError() const;
    virtual const char *what () const throw ();
    
    static void check(int result)		{ if (result == -1) throwMe(); }
    static void throwMe(int err = errno) __attribute__((noreturn));

    // @@@ This is a hack for the Network protocol state machine
    static UnixError make(int err = errno);

private:
	IFDEBUG(void debugDiagnose(const void *id) const);
};

class MacOSError : public CssmCommonError {
protected:
    MacOSError(int err);
public:
    const int error;
    virtual CSSM_RETURN cssmError() const;
    virtual OSStatus osStatus() const;
    virtual const char *what () const throw ();
    
    static void check(OSStatus status)	{ if (status != noErr) throwMe(status); }
    static void throwMe(int err) __attribute__((noreturn));
};


//
// API boilerplate macros. These provide a frame for C++ code that is impermeable to exceptions.
// Usage:
//	BEGIN_API
//		... your C++ code here ...
//  END_API(base)	// returns CSSM_RETURN on exception; complete it to 'base' (DL, etc.) class;
//					// returns CSSM_OK on fall-through
//	END_API0		// completely ignores exceptions; falls through in all cases
//	END_API1(bad)	// return (bad) on exception; fall through on success
//
#define BEGIN_API	try {
#define END_API(base) 	} \
catch (const CssmCommonError &err) { return err.cssmError(CSSM_ ## base ## _BASE_ERROR); } \
catch (const std::bad_alloc &) { return CssmError::merge(CSSM_ERRCODE_MEMORY_ERROR, CSSM_ ## base ## _BASE_ERROR); } \
catch (...) { return CssmError::merge(CSSM_ERRCODE_INTERNAL_ERROR, CSSM_ ## base ## _BASE_ERROR); } \
    return CSSM_OK;
#define END_API0		} catch (...) { return; }
#define END_API1(bad)	} catch (...) { return bad; }


//
// Helpers for memory pointer validation
//
template <class T>
inline T &Required(T *ptr, CSSM_RETURN err = CSSM_ERRCODE_INVALID_POINTER)
{
    if (ptr == NULL)
        CssmError::throwMe(err);
    return *ptr;
}

// specialization for void * (just check for non-null; don't return a void & :-)
inline void Required(void *ptr, CSSM_RETURN err = CSSM_ERRCODE_INVALID_POINTER)
{
	if (ptr == NULL)
		CssmError::throwMe(err);
}


//
// Tools to build POD wrapper classes
//
template <class Wrapper, class POD>
class PodWrapper : public POD {
public:
    // pure typecasts
    static Wrapper * &overlayVar(POD * &data)
    { return reinterpret_cast<Wrapper * &>(data); }
    static const Wrapper * &overlayVar(const POD * &data)
    { return reinterpret_cast<const Wrapper * &>(data); }
	
    static Wrapper *overlay(POD *data)
    { return static_cast<Wrapper *>(data); }
    static const Wrapper *overlay(const POD *data)
    { return static_cast<const Wrapper *>(data); }
    static Wrapper &overlay(POD &data)
    { return static_cast<Wrapper &>(data); }
    static const Wrapper &overlay(const POD &data)
    { return static_cast<const Wrapper &>(data); }

    // optional/required forms
    static Wrapper &required(POD *data)
    { return overlay(Required(data)); }
    static const Wrapper &required(const POD *data)
    { return overlay(Required(data)); }
    static Wrapper *optional(POD *data)
    { return overlay(data); }
    static const Wrapper *optional(const POD *data)
    { return overlay(data); }
    
    // general helpers for all PodWrappers
    void clearPod()
    { memset(static_cast<POD *>(this), 0, sizeof(POD)); }
};


//
// Template builder support
//
template <class T>
struct Nonconst {
	typedef T Type;
};

template <class U>
struct Nonconst<const U> {
	typedef U Type;
};


//
// User-friendly GUIDs
//
class Guid : public PodWrapper<Guid, CSSM_GUID> {
public:
    Guid() { /*IFDEBUG(*/ memset(this, 0, sizeof(*this)) /*)*/ ; }
    Guid(const CSSM_GUID &rGuid) { memcpy(this, &rGuid, sizeof(*this)); }

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
    Guid(const char *string);
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
// User-friendlier CSSM_DATA thingies.
// CssmData is a PODWrapper and has no memory allocation features.
//
class CssmData : public PodWrapper<CssmData, CSSM_DATA> {
public:
    CssmData() { Data = 0; Length = 0; }

    size_t length() const { return Length; }
    void *data() const { return Data; }
    void *end() const { return Data + Length; }
	
	//
	// Create a CssmData from any pointer-to-byte-sized-object and length.
	//
	CssmData(void *data, size_t length)
	{ Data = reinterpret_cast<UInt8 *>(data); Length = length; }
	CssmData(char *data, size_t length)
	{ Data = reinterpret_cast<UInt8 *>(data); Length = length; }
	CssmData(unsigned char *data, size_t length)
	{ Data = reinterpret_cast<UInt8 *>(data); Length = length; }
	CssmData(signed char *data, size_t length)
	{ Data = reinterpret_cast<UInt8 *>(data); Length = length; }
		
	// the void * form accepts too much; explicitly deny all other types
	private: template <class T> CssmData(T *, size_t); public:
	
	// explicitly construct from a data-oid source
	template <class T>
	explicit CssmData(const T &obj)
	{ Data = (UInt8 *)obj.data(); Length = obj.length(); }
	
	//
	// Do allow generic "wrapping" of any data structure, but make it conspicuous
	// since it's not necessarily the Right Thing (alignment and byte order wise).
	// Also note that the T & form removes const-ness, since there is no ConstCssmData.
	//
	template <class T>
	static CssmData wrap(const T &it)
	{ return CssmData(const_cast<void *>(reinterpret_cast<const void *>(&it)), sizeof(it)); }
	
	template <class T>
	static CssmData wrap(T *data, size_t length)
	{ return CssmData(static_cast<void *>(data), length); }

	//
	// Automatically convert a CssmData to any pointer-to-byte-sized-type.
	//
	operator signed char * () const { return reinterpret_cast<signed char *>(Data); }
	operator unsigned char * () const { return reinterpret_cast<unsigned char *>(Data); }
	operator char * () const { return reinterpret_cast<char *>(Data); }
	operator void * () const { return reinterpret_cast<void *>(Data); }
	
	//
	// If you want to interpret the contents of a CssmData blob as a particular
	// type, you have to be more explicit to show that you know what you're doing.
	// See wrap() above.
	//
	template <class T>
	T *interpretedAs() const		{ return reinterpret_cast<T *>(Data); }

	template <class T>
	T *interpretedAs(CSSM_RETURN error) const
	{ return interpretedAs<T>(sizeof(T), error); }
	
	template <class T>
	T *interpretedAs(size_t len, CSSM_RETURN error) const
	{
		if (data() == NULL || length() != len) CssmError::throwMe(error);
		return interpretedAs<T>();
	}
	
public:
    void length(size_t newLength)	// shorten only
    { assert(newLength <= Length); Length = newLength; }

	void *at(off_t offset) const
	{ assert(offset >= 0 && offset <= Length); return Data + offset; }
	void *at(off_t offset, size_t size) const	// length-checking version
	{ assert(offset >= 0 && offset + size <= Length); return Data + offset; }
	
    unsigned char operator [] (size_t pos) const
    { assert(pos < Length); return Data[pos]; }
    void *use(size_t taken)			// logically remove some bytes
    { assert(taken <= Length); void *r = Data; Length -= taken; Data += taken; return r; }
	
	void clear()
	{ Data = NULL; Length = 0; }

    string toString () const;	// convert to string type (no trailing null)

    operator bool () const { return Data != NULL; }
    bool operator ! () const { return Data == NULL; }
    bool operator < (const CssmData &other) const;
	bool operator == (const CssmData &other) const
	{ return length() == other.length() && !memcmp(data(), other.data(), length()); }
	bool operator != (const CssmData &other) const
	{ return !(*this == other); }
    
    // Extract fixed-format data from a CssmData. Fixes any alignment trouble for you.
    template <class T>
    void extract(T &destination, CSSM_RETURN error = CSSM_ERRCODE_INVALID_DATA) const
    {
        if (length() != sizeof(destination) || data() == NULL)
            CssmError::throwMe(error);
        memcpy(&destination, data(), sizeof(destination));
    }
};


inline bool CssmData::operator < (const CssmData &other) const
{
    if (Length != other.Length) // If lengths are not equal the shorter data is smaller.
        return Length < other.Length;
    if (Length == 0) // If lengths are both zero ignore the Data.
        return false;
    if (Data == NULL || other.Data == NULL)	// arbitrary (but consistent) ordering
        return Data < other.Data;
    return memcmp(Data, other.Data, Length) < 0; // Do a lexicographic compare on equal sized Data.
}


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
	static CSSM_RETURN callbackShim(CSSM_DATA *output, void *ctx)
	{
		BEGIN_API
		*output = reinterpret_cast<CryptoDataClass *>(ctx)->yield();
		END_API(CSSM)
	}
};


//
// CSSM_OIDs are CSSM_DATAs but will probably have different wrapping characteristics.
//
typedef CssmData CssmOid;


//
// User-friendlier CSSM_KEY objects
//
class CssmKey : public PodWrapper<CssmKey, CSSM_KEY> {
public:
    CssmKey() { clearPod(); KeyHeader.HeaderVersion = CSSM_KEYHEADER_VERSION; }
	// all of the following constructors take over ownership of the key data
    CssmKey(const CSSM_KEY &key);
	CssmKey(const CSSM_DATA &keyData);
    CssmKey(uint32 length, void *data);

public:
    class Header : public PodWrapper<Header, CSSM_KEYHEADER> {
    public:
		// access to components of the key header
		CSSM_KEYBLOB_TYPE blobType() const { return BlobType; }
		void blobType(CSSM_KEYBLOB_TYPE blobType) { BlobType = blobType; }
		
		CSSM_KEYBLOB_FORMAT blobFormat() const { return Format; }
		void blobFormat(CSSM_KEYBLOB_FORMAT blobFormat) { Format = blobFormat; }
		
		CSSM_KEYCLASS keyClass() const { return KeyClass; }
		void keyClass(CSSM_KEYCLASS keyClass) { KeyClass = keyClass; }
		
		CSSM_KEY_TYPE algorithm() const { return AlgorithmId; }
		void algorithm(CSSM_KEY_TYPE algorithm) { AlgorithmId = algorithm; }
		
		CSSM_KEY_TYPE wrapAlgorithm() const { return WrapAlgorithmId; }
		void wrapAlgorithm(CSSM_KEY_TYPE wrapAlgorithm) { WrapAlgorithmId = wrapAlgorithm; }
        
        CSSM_ENCRYPT_MODE wrapMode() const { return WrapMode; }
        void wrapMode(CSSM_ENCRYPT_MODE mode) { WrapMode = mode; }
		
		bool isWrapped() const { return WrapAlgorithmId != CSSM_ALGID_NONE; }

		const Guid &cspGuid() const { return Guid::overlay(CspId); }
		void cspGuid(const Guid &guid) { Guid::overlay(CspId) = guid; }
		
		uint32 attributes() const { return KeyAttr; }
		bool attribute(uint32 attr) const { return KeyAttr & attr; }
		void setAttribute(uint32 attr) { KeyAttr |= attr; }
		void clearAttribute(uint32 attr) { KeyAttr &= ~attr; }
		
		uint32 usage() const { return KeyUsage; }
		bool useFor(uint32 u) const { return KeyUsage & u; }

		void usage(uint32 u) { KeyUsage |= u; }
		void clearUsage(uint32 u) { u &= ~u; }

    };

	// access to the key header
	Header &header() { return Header::overlay(KeyHeader); }
	const Header &header() const { return Header::overlay(KeyHeader); }
	
	CSSM_KEYBLOB_TYPE blobType() const	{ return header().blobType(); }
	void blobType(CSSM_KEYBLOB_TYPE blobType) { header().blobType(blobType); }

	CSSM_KEYBLOB_FORMAT blobFormat() const { return header().blobFormat(); }
	void blobFormat(CSSM_KEYBLOB_FORMAT blobFormat) { header().blobFormat(blobFormat); }

	CSSM_KEYCLASS keyClass() const		{ return header().keyClass(); }
	void keyClass(CSSM_KEYCLASS keyClass) { header().keyClass(keyClass); }

	CSSM_KEY_TYPE algorithm() const		{ return header().algorithm(); }
	void algorithm(CSSM_KEY_TYPE algorithm) { header().algorithm(algorithm); }

	CSSM_KEY_TYPE wrapAlgorithm() const	{ return header().wrapAlgorithm(); }
	void wrapAlgorithm(CSSM_KEY_TYPE wrapAlgorithm) { header().wrapAlgorithm(wrapAlgorithm); }
    
    CSSM_ENCRYPT_MODE wrapMode() const	{ return header().wrapMode(); }
    void wrapMode(CSSM_ENCRYPT_MODE mode) { header().wrapMode(mode); }
	
	bool isWrapped() const				{ return header().isWrapped(); }
	const Guid &cspGuid() const			{ return header().cspGuid(); }
	
	uint32 attributes() const			{ return header().attributes(); }
	bool attribute(uint32 a) const		{ return header().attribute(a); }
	void setAttribute(uint32 attr) { header().setAttribute(attr); }
	void clearAttribute(uint32 attr) { header().clearAttribute(attr); }

	uint32 usage() const				{ return header().usage(); }
	bool useFor(uint32 u) const			{ return header().useFor(u); }

	void usage(uint32 u) { header().usage(u); }
	void clearUsage(uint32 u) { header().clearUsage(u); }
	
public:
	// access to the key data
	size_t length() const { return KeyData.Length; }
	void *data() const { return KeyData.Data; }
	operator void * () const { return data(); }
	CssmData &keyData()		{ return CssmData::overlay(KeyData); }
	const CssmData &keyData() const { return CssmData::overlay(KeyData); }
	operator CssmData & () { return keyData(); }
	operator const CssmData & () const { return keyData(); }
	operator bool () const { return KeyData.Data != NULL; }
	void operator = (const CssmData &data) { KeyData = data; }
};


//
// Wrapped keys are currently identically structured to normal keys.
// But perhaps in the future...
//
typedef CssmKey CssmWrappedKey;


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


//
// User-friendli(er) DL queries.
// @@@ Preliminary (flesh out as needed)
//
class DLQuery : public PodWrapper<DLQuery, CSSM_QUERY> {
public:
    DLQuery() { /*IFDEBUG(*/ memset(this, 0, sizeof(*this)) /*)*/ ; }
    DLQuery(const CSSM_QUERY &q) { memcpy(this, &q, sizeof(*this)); }

    DLQuery &operator = (const CSSM_QUERY &q)
    { memcpy(this, &q, sizeof(*this)); return *this; }
};


// Help with container of something->pointer cleanup
template <class In>
static inline void for_each_delete(In first, In last)
{
    while (first != last)
        delete *(first++);
}

// Help with map of something->pointer cleanup
template <class In>
static inline void for_each_map_delete(In first, In last)
{
    while (first != last)
        delete (first++)->second;
}

// Quick and dirty template for a (temporary) array of something
// Usage example auto_array<uint32> anArray(20);
template <class T>
class auto_array
{
public:
	auto_array() : mArray(NULL) {}
	auto_array(size_t inSize) : mArray(new T[inSize]) {}
	~auto_array() { if (mArray) delete[] mArray; }
    T &operator[](size_t inIndex) { return mArray[inIndex]; }
	void allocate(size_t inSize) { if (mArray) delete[] mArray; mArray = new T[inSize]; }
	T *get() { return mArray; }
	T *release() { T *anArray = mArray; mArray = NULL; return anArray; }
private:
	T *mArray;
};

// Template for a vector-like class that takes a c-array as it's
// underlying storage without making a copy.
template <class _Tp>
class constVector
{
    NOCOPY(constVector<_Tp>)
public:
    typedef _Tp value_type;
    typedef const value_type* const_pointer;
    typedef const value_type* const_iterator;
    typedef const value_type& const_reference;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;

    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
public:
    const_iterator begin() const { return _M_start; }
    const_iterator end() const { return _M_finish; }

    const_reverse_iterator rbegin() const
    { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const
    { return const_reverse_iterator(begin()); }

    size_type size() const
    { return size_type(end() - begin()); }
    bool empty() const
    { return begin() == end(); }

    const_reference operator[](size_type __n) const { return *(begin() + __n); }

    // "at" will eventually have range checking, once we have the
    // infrastructure to be able to throw stl range errors.
    const_reference at(size_type n) const { return (*this)[n]; }

    constVector(size_type __n, const _Tp* __value)
    : _M_start(__value), _M_finish(__value + __n)
    {}
	
	constVector() : _M_start(NULL), _M_finish(NULL) {}
	
	void overlay(size_type __n, const _Tp* __value) {
		_M_start = __value;
		_M_finish = __value + __n;
	}

    const_reference front() const { return *begin(); }
    const_reference back() const { return *(end() - 1); }
private:
    const _Tp *_M_start;
    const _Tp *_M_finish;
};

} // end namespace Security


//
// Strictly as a transition measure, include cfutilities.h here
//
#include "cfutilities.h"


#endif //_H_UTILITIES
