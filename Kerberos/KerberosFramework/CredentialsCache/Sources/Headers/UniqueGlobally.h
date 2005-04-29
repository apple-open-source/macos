#pragma once

#include "SharedStaticData.h"
#include "Allocators.h"
#include "Implementations.h"

// CCIGloballyUnique is a template that is used to assign unique
// IDs among processes to objects of type T. The IDs are not going
// to be unique among different types, but they will be unique
// among different processes.
// The unique IDs have to be mapped back to an object,
// so we keep a global array of pointers to all objects of a type,
// and use that to map a unique ID to an object. 


template <class T>
class CCIUniqueGlobally:
	public Implementations::SharedData {
	public:
	
		typedef	CCIUniqueID		UniqueID;
			
		CCIUniqueGlobally ():
			mGlobalsProxy (sGlobals) {
			mGlobalsProxy.Get ();
			mUniqueID = UniqueID (sGlobalsProxy.Get () -> mNextObjectID++);
			sGlobalsProxy.Get () -> mObjects.insert (
				typename ObjectMap::value_type (mUniqueID, static_cast <T*> (this)));
		}

		~CCIUniqueGlobally () {
			sGlobalsProxy.Get () -> mObjects.erase (mUniqueID);
		}
		
		const UniqueID& GetGloballyUniqueID () const { return mUniqueID; }
                
		static UniqueID GetNextGloballyUniqueID ();

		static void SetNextGloballyUniqueID (
			const UniqueID&		inNextID);
		
        static size_t CountGloballyUniqueIDs ();

		bool operator == (
			const CCIUniqueGlobally&		inRhs) const {
			return inRhs.mUniqueID == mUniqueID;
		}
		
		bool Valid () const;
		
		static T*	Resolve (
			const UniqueID&	inID);
		
	private:
		UniqueID		mUniqueID;

		// I would have preferred to use hash_map, but CWP4 doesn't have
		// a standards compliant hash_map (no allocator template argument),
		// so I can't (the allocator is needed for sysheap storage when we don't
		// use AEs).
		typedef	typename	Implementations::Map <UniqueID, T*>::Shared		ObjectMap;

		struct Globals: public CCISharedData {
			Globals () { mNextObjectID = 0; }
			CCIObjectID			mNextObjectID;
			ObjectMap			mObjects;
		};
		

		static CCISharedStaticData <Globals>		sGlobals;
		static CCISharedStaticDataProxy <Globals>	sGlobalsProxy;

		CCISharedStaticDataProxy <Globals>	mGlobalsProxy;

		// Disallowed
		CCIUniqueGlobally (const CCIUniqueGlobally&);
		CCIUniqueGlobally& operator = (const CCIUniqueGlobally&);

};

template <class T>
T*
CCIUniqueGlobally <T>::Resolve (
	const 	CCIUniqueGlobally <T>::UniqueID&	inID) {
	
	StReadLock __attribute__ ((unused)) lock (sGlobalsProxy.Lock ());
//	CCISharedStaticDataProxy <Globals>	globalsProxy = sGlobals;
	typename ObjectMap::iterator	location =
		sGlobalsProxy.Get () -> mObjects.find (inID);
		
	if (location == sGlobalsProxy.Get () -> mObjects.end ()) {
		throw CCIException (T::objectNotFound);
	}
	
	return location -> second;
}

template <class T>
typename CCIUniqueGlobally <T>::UniqueID
CCIUniqueGlobally <T>::GetNextGloballyUniqueID () {
	
	StReadLock	__attribute__ ((unused)) lock (sGlobalsProxy.Lock ());
//	CCISharedStaticDataProxy <Globals>	globalsProxy = sGlobals;
	return sGlobalsProxy.Get () -> mNextObjectID;
}

template <class T>
void
CCIUniqueGlobally <T>::SetNextGloballyUniqueID (
	const CCIUniqueGlobally <T>::UniqueID&	inNextID) {
	
	StReadLock lock = sGlobalsProxy.Lock ();
//	CCISharedStaticDataProxy <Globals>	globalsProxy = sGlobals;
	sGlobalsProxy.Get () -> mNextObjectID = inNextID.object;
}

template <class T>
size_t
CCIUniqueGlobally <T>::CountGloballyUniqueIDs ()
{
    return sGlobalsProxy.Get () -> mObjects.size ();
}
