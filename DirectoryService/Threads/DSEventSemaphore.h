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

/*!
 * @header DSEventSemaphore
 * Interface for the DSEventSemaphore (gating lock) class.
 */

#ifndef _DSEventSemaphore_H_
#define _DSEventSemaphore_H_

#include "DSSemaphore.h"

// --------------------------------------------------------------------------------
// DSEventSemaphore class definition
//
// --------------------------------------------------------------------------------

class DSEventSemaphore : public DSSemaphore
{
public:
	/**** Instance methods. ****/
	// ctor and dtor.
			DSEventSemaphore		( bool posted = false ) ;
	virtual ~DSEventSemaphore	( void ) ;

	// Superclass overrides.
	virtual void		Signal	( void ) ;
	virtual sInt32		Wait	( sInt32 milliSecs = kForever ) ;
	virtual uInt32		Reset	( void ) ;
};

#endif /* _DSEventSemaphore_H_ */
