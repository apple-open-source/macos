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
 * LockedMap.h - STL-style map with attached Mutex
 *
 * Created 9/1/2000 by Doug Mitchell. 
 * Copyright (c) 2000 by Apple Computer. 
 */
 
#ifndef	_LOCKED_MAP_H_
#define _LOCKED_MAP_H_

#include <map>
#include <security_utilities/threading.h>

template <class KeyType, class ValueType>
class LockedMap
{
private:
	typedef std::map<KeyType, ValueType *> MapType;
	MapType					mMap;
	Mutex					mMapLock;
	
	/* low-level lookup, cacheMapLock held on entry and exit */
	ValueType 				
	*lookupEntryLocked(KeyType key) 
		{
			// don't create new entry if desired entry isn't there
			typename MapType::iterator it = mMap.find(key);
			if(it == mMap.end()) {
				return NULL;
			}
			return it->second;
		}

public:
	/* high level maintenance */
	void 
	addEntry(ValueType &value, KeyType key)
		{
			StLock<Mutex> _(mMapLock);
			mMap[key] = &value;
		}
		
	ValueType				
	*lookupEntry(KeyType key)
		{
			StLock<Mutex> _(mMapLock);
			return lookupEntryLocked(key);
		}
		
	void	
	removeEntry(KeyType key)
		{
			StLock<Mutex> _(mMapLock);

			ValueType *value = lookupEntryLocked(key);
			if(value != NULL) {
				mMap.erase(key);
			}
		}
		
	ValueType	
	*removeFirstEntry()
		{
			StLock<Mutex> _(mMapLock);
			typename MapType::iterator it = mMap.begin();
			if(it == mMap.end()) {
				return NULL;
			}
			ValueType *rtn = it->second;
			mMap.erase(it->first);
			return rtn;
		}
};

#endif	/* _LOCKED_MAP_H_ */
