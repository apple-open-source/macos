/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

/*!
 * @header CNiNodeList
 */

#ifndef __CNiNodeList_h__
#define __CNiNodeList_h__	1

#include <netinfo/ni.h>
#include <map>
#include <string>

#include "PrivateTypes.h"
#include "DirServicesTypes.h"
#include "DSMutexSemaphore.h"

using namespace std;

// Typedefs --------------------------------------------------------------------

typedef struct sNode
{
	tDataList	*listPtr;		//opaque struct for node name
	void		*fDomain;		//NetInfo handle
	char		*fDomainName;	//NetInfo name
	ni_id		fDirID;			//NetInfo index
	uInt32		refCount;		//many consumers
	bool		bisDirty;		//network transition marking
	bool		bRegistered;	//is this node registered and can we add/delete it ourselves
	uInt32		localOrParent;	//indicates whether to use local or parent NI shortcuts "." and ".."
} sNode;

typedef map<string, sNode*>	NiNodeMap;
typedef NiNodeMap::iterator	NiNodeMapI;

class CNiNodeList {

public:
				CNiNodeList			( void );
	virtual	   ~CNiNodeList			( void );

	void	 	Lock				( void );
	void	 	UnLock				( void );
	bool	 	IsPresent			( const char *inStr );
	bool	 	IsPresent			( const char *inStr, tDataList **inListPtr );
	bool	 	IsOpen				( const char *inStr, void **outDomain, char **outDomName, ni_id *outDirID );

	sInt32	   	AddNode				( const char *inStr, tDataList *inListPtr, bool inRegistered, uInt32 inLocalOrParent = 0 );
	void	   *DeleteNode			( const char *inStr );

	bool		SetDomainInfo		( const char *inStr, void *inDomain, char *inDomName, ni_id *inDirID );
	void		SetAllDirty			( void );
	void		CleanAllDirty		( const uInt32 inSignature );
	uInt32		CheckForLocalOrParent
									( const char *inName );

protected:

private:
	NiNodeMap			fNiNodeMap;
	DSMutexSemaphore	fMutex;
};

#endif
