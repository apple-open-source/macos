/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifndef CRYPTOPP_SMARTPTR_H
#define CRYPTOPP_SMARTPTR_H

#include "cryptopp_config.h"
#include <algorithm>

NAMESPACE_BEGIN(CryptoPP)

template<class T> class member_ptr
{
public:
	explicit member_ptr(T *p = NULL) : m_p(p) {}

	~member_ptr();

	const T& operator*() const { return *m_p; }
	T& operator*() { return *m_p; }

	const T* operator->() const { return m_p; }
	T* operator->() { return m_p; }

	const T* get() const { return m_p; }
	T* get() { return m_p; }

	T* release()
	{
		T *old_p = m_p;
		m_p = 0;
		return old_p;
	} 

	void reset(T *p = 0);

protected:
	member_ptr(const member_ptr<T>& rhs);		// copy not allowed
	void operator=(const member_ptr<T>& rhs);	// assignment not allowed

	T *m_p;
};

template <class T> member_ptr<T>::~member_ptr() {delete m_p;}
template <class T> void member_ptr<T>::reset(T *p) {delete m_p; m_p = p;}

// ********************************************************

template<class T> class value_ptr : public member_ptr<T>
{
public:
	value_ptr(const T &obj) : member_ptr<T>(new T(obj)) {}
	value_ptr(T *p = NULL) : member_ptr<T>(p) {}
	value_ptr(const value_ptr<T>& rhs)
		: member_ptr<T>(rhs.m_p ? new T(*rhs.m_p) : NULL) {}

	value_ptr<T>& operator=(const value_ptr<T>& rhs);
	bool operator==(const value_ptr<T>& rhs)
	{
		return (!m_p && !rhs.m_p) || (m_p && rhs.m_p && *m_p == *rhs.m_p);
	}
};

template <class T> value_ptr<T>& value_ptr<T>::operator=(const value_ptr<T>& rhs)
{
	T *old_p = m_p;
	m_p = rhs.m_p ? new T(*rhs.m_p) : NULL;
	delete old_p;
	return *this;
}

// ********************************************************

template<class T> class clonable_ptr : public member_ptr<T>
{
public:
	clonable_ptr(const T &obj) : member_ptr<T>(obj.Clone()) {}
	clonable_ptr(T *p = NULL) : member_ptr<T>(p) {}
	clonable_ptr(const clonable_ptr<T>& rhs)
		: member_ptr<T>(rhs.m_p ? rhs.m_p->Clone() : NULL) {}

	clonable_ptr<T>& operator=(const clonable_ptr<T>& rhs);
};

template <class T> clonable_ptr<T>& clonable_ptr<T>::operator=(const clonable_ptr<T>& rhs)
{
	T *old_p = m_p;
	m_p = rhs.m_p ? rhs.m_p->Clone() : NULL;
	delete old_p;
	return *this;
}

// ********************************************************

template<class T> class counted_ptr
{
public:
	explicit counted_ptr(T *p = 0);
	counted_ptr(const counted_ptr<T>& rhs);

	~counted_ptr();

	const T& operator*() const { return *m_p; }
	T& operator*() { return *m_p; }

	const T* operator->() const { return m_p; }
	T* operator->() { return m_p; }

	const T* get() const { return m_p; }
	T* get() { return m_p; }

	counted_ptr<T> & operator=(const counted_ptr<T>& rhs);

private:
	T *m_p;
};

template <class T> counted_ptr<T>::counted_ptr(T *p)
	: m_p(p) 
{
	if (m_p)
		m_p->m_referenceCount = 1;
}

template <class T> counted_ptr<T>::counted_ptr(const counted_ptr<T>& rhs)
	: m_p(rhs.m_p)
{
	if (m_p)
		m_p->m_referenceCount++;
}

template <class T> counted_ptr<T>::~counted_ptr()
{
	if (m_p && --m_p->m_referenceCount == 0)
		delete m_p;
}

template <class T> counted_ptr<T> & counted_ptr<T>::operator=(const counted_ptr<T>& rhs)
{
	if (m_p && --m_p->m_referenceCount == 0)
		delete m_p;
	m_p = rhs.m_p;
	if (m_p)
		m_p->m_referenceCount++;
	return *this;
}

// ********************************************************

template <class T> class vector_member_ptrs
{
public:
	vector_member_ptrs(unsigned int size=0)
		: _size(size) {ptr = new member_ptr<T>[_size];}
	~vector_member_ptrs()
		{delete [] ptr;}

	member_ptr<T>& operator[](unsigned int index)
		{assert(index<_size); return ptr[index];}
	const member_ptr<T>& operator[](unsigned int index) const
		{assert(index<_size); return ptr[index];}

	unsigned int size() const {return _size;}
	void resize(unsigned int newSize)
	{
		member_ptr<T> *newPtr = new member_ptr<T>[newSize];
		for (unsigned int i=0; i<STDMIN(_size, newSize); i++)
			newPtr[i].reset(ptr[i].release());
		delete [] ptr;
		_size = newSize;
		ptr = newPtr;
	}

private:
	vector_member_ptrs(const vector_member_ptrs<T> &c);	// copy not allowed
	void operator=(const vector_member_ptrs<T> &x);		// assignment not allowed

	unsigned int _size;
	member_ptr<T> *ptr;
};

// ********************************************************

// derive from this class for a temporary variable
// that can be used during base/member initialization
template <class T>
class ConstructorTemp
{
protected:
	ConstructorTemp(const ConstructorTemp &copy) : m_temp(NULL) {}
	ConstructorTemp(T *t = NULL) : m_temp(t) {}
	ConstructorTemp(const T &t) : m_temp(new T(t)) {}
	member_ptr<T> m_temp;
};

NAMESPACE_END

#endif
