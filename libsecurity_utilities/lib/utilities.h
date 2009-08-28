/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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


/*
 * cssm utilities
 */
#ifndef _H_UTILITIES
#define _H_UTILITIES

#include <security_utilities/utility_config.h>
#include <exception>
#include <new>
#include <string>
#include <errno.h>
#include <string.h>

namespace Security
{

//
// Elementary debugging support.
// #include <debugging.h> for more debugging facilities.
//
#define IFDEBUG(it)		IFELSEDEBUG(it,)
#define IFNDEBUG(it)	IFELSEDEBUG(,it)

#if defined(NDEBUG)

# define safe_cast	static_cast
# define safer_cast	static_cast

# define IFELSEDEBUG(d,nd) nd

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

# define IFELSEDEBUG(d,nd) d

#endif //NDEBUG


//
// Place this into your class definition if you don't want it to be copyable
// or asignable. This will not prohibit allocation on the stack or in static
// memory, but it will make anything derived from it, and anything containing
// it, fixed-once-created. A proper object, I suppose.
//
#define NOCOPY(Type)	\
	private: Type(const Type &) DEPRECATED_IN_MAC_OS_X_VERSION_10_0_AND_LATER; \
	void operator = (const Type &) DEPRECATED_IN_MAC_OS_X_VERSION_10_0_AND_LATER;


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
	
	void assignPod(const POD &source)
	{ static_cast<POD &>(*this) = source; }
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

template <class U>
struct Nonconst<const U *> {
	typedef U *Type;
};

// cast away pointed-to constness
template <class T>
typename Nonconst<T>::Type unconst_cast(T obj)
{
	return const_cast<typename Nonconst<T>::Type>(obj);
}

template <class T>
typename Nonconst<T>::Type &unconst_ref_cast(T &obj)
{
	return const_cast<typename Nonconst<T>::Type &>(obj);
}


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

// versions of copy that project to pair elements
template <class InIterator, class OutIterator>
inline OutIterator copy_first(InIterator first, InIterator last, OutIterator out)
{
	while (first != last)
		*out++ = (first++)->first;
	return out;
}

template <class InIterator, class OutIterator>
inline OutIterator copy_second(InIterator first, InIterator last, OutIterator out)
{
	while (first != last)
		*out++ = (first++)->second;
	return out;
}


// simple safe re-entry blocker
class RecursionBlock {
public:
	RecursionBlock() : mActive(false) { }
	~RecursionBlock() { assert(!mActive); }

public:
	class Once {
	public:
		Once(RecursionBlock &rb) : block(rb), mActive(false) { }
		~Once() { block.mActive &= !mActive; }
		bool operator () ()
		{ if (block.mActive) return true; mActive = block.mActive = true; return false; }
		
		RecursionBlock &block;
	
	private:
		bool mActive;
	};
	friend class Once;
	
private:
	bool mActive;
};

// Quick and dirty template for a (temporary) array of something
// Usage example auto_array<UInt32> anArray(20);
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


#endif //_H_UTILITIES
