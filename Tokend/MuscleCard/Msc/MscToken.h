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

/*
 *  MscToken.h
 *  TokendMuscle
 */

#ifndef _MSCTOKEN_H_
#define _MSCTOKEN_H_

#include <PCSC/musclecard.h>
#include <map>
#include "MscWrappers.h"
#include "MscObject.h"
#include "MscKey.h"
#include "TokenContext.h"

/*
	Token contains:
	- <set> of keys
	- <set> of objects
*/
class MscToken: public Tokend::TokenContext
{
public:
	MscToken();
	MscToken(MSCTokenConnection *connection) : mConnection(MscTokenConnection::optional(connection)) {};
	virtual ~MscToken() {};

    typedef std::map<std::string, MscObject *> ObjectMap;
    typedef ObjectMap::iterator ObjIterator;
    typedef ObjectMap::const_iterator ConstObjIterator;

    typedef std::map<MSCUChar8, MscKey *> KeyMap;
    typedef KeyMap::iterator KeyIterator;
    typedef KeyMap::const_iterator ConstKeyIterator;

	void loadobjects();
	void dumpobjects();

	MscObject &getObject(const std::string &objID);
	MscKey &getKey(MSCUChar8 keyNum);

	friend std::ostream& operator << (std::ostream& strm, const MscToken& oa);

protected:
	MscTokenConnection *mConnection;
	ObjectMap mObjects;
	KeyMap mKeys;

public:
	// Iterators for moving through records
	
	ConstObjIterator begin() const { return ConstObjIterator(mObjects.begin()); }
	ConstObjIterator end()   const { return ConstObjIterator(mObjects.end()); }	

	ObjIterator begin() { return ObjIterator(mObjects.begin()); }
	ObjIterator end() { return ObjIterator(mObjects.end()); }	

	ConstKeyIterator kbegin() const { return ConstKeyIterator(mKeys.begin()); }
	ConstKeyIterator kend()   const { return ConstKeyIterator(mKeys.end()); }	
};

std::ostream& operator << (std::ostream& strm, const MscToken& oa);

#endif /* !_MSCTOKEN_H_ */

