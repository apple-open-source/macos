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
 * @header CNodeRegister
 */

#ifndef __CNodeRegister_h__
#define __CNodeRegister_h__		1

#include <netinfo/ni.h>

#include "DSCThread.h"
#include "DSMutexSemaphore.h"
#include "DSEventSemaphore.h"
#include "PrivateTypes.h"

extern DSMutexSemaphore		*gNetInfoMutex;

class	CNiNodeList;
class	CNetInfoPI;

class CNodeRegister : public DSCThread
{
public:
						CNodeRegister					( uInt32 inToken, CNiNodeList *inNodeList, bool bReInit, CNetInfoPI *parentClass);
	virtual			   ~CNodeRegister					( void );
	
	virtual	long		ThreadMain						( void );		// we manage our own thread top level
	virtual	void		StartThread						( void );
	virtual	void		StopThread						( void );
			void		Restart							( void );

private:
	sInt32				RegisterNodes					( char *inDomainName );
	sInt32				RegisterLocalNetInfoHierarchy	( bool inSetLocallyHosted );
	bool				IsValidNameList					( ni_namelist *inNameList );
	bool				IsValidName						( char *inName );
	bool				IsLocalDomain					( char *inName );

	uInt32				fToken;
	uInt32				fCount;
	uInt32				fTotal;
	CNiNodeList		   *fNiNodeList;
	DSEventSemaphore   	fMutex;
	bool				bReInit;
	bool				bRestart;
	CNetInfoPI		   *fParentClass;
};


#endif // CNodeRegister
