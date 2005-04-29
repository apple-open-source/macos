/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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



#ifndef __MUTEX_H__
#define __MUTEX_H__



#include <pthread.h>



// base class for a mutex -- note that this can't be instantiated
class Mutex
{
protected:
	pthread_mutex_t *mMutexPtr;
	Mutex () {}

public:
	void Lock ();
	void Unlock ();
};




// Mutex which initializes its own mutex
class DynamicMutex : public Mutex
{
protected:
	pthread_mutex_t mMutex;

public:
	DynamicMutex ();
	~DynamicMutex ();
};



// Mutex which takes an externally initialized mutex
class StaticMutex : public Mutex
{
protected:
	pthread_mutex_t& mMutex;

public:
	StaticMutex (pthread_mutex_t &mutex);
};



// class which locks and unlocks a mutex when it goes in and out of scope
class MutexLocker
{
protected:
	Mutex& mMutex;

public:
	MutexLocker (Mutex &mutex);
	~MutexLocker ();
};



#endif
