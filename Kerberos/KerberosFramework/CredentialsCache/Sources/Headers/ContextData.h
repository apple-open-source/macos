#pragma once

#include "CCacheData.h"
#include "UniqueGlobally.h"

class CCIContextData:
	public CCIUniqueGlobally <CCIContextData> {
	public:
		enum {
			objectNotFound = ccErrContextNotFound
		};
        
		CCIContextData ();
		~CCIContextData ();
		
		const CCICCacheData::UniqueID&	GetCCacheID (
			const std::string&		inName) const;
			
		const CCICCacheData::UniqueID& GetDefaultCCacheID () const;
		
		std::string		GetDefaultCCacheName () const;
		
		const CCICCacheData::UniqueID&	CreateCCache (
			const std::string&		inName,
			CCIUInt32				inVersion,
			const std::string&		inPrincipal);

		const CCICCacheData::UniqueID&	CreateDefaultCCache (
			CCIUInt32				inVersion,
			const std::string&		inPrincipal);

		const CCICCacheData::UniqueID&	CreateNewCCache (
			CCIUInt32				inVersion,
			const std::string&		inPrincipal);
			
		CCITime			GetChangeTime () const;
		
		void			GetCCacheIDs (
			std::vector <CCIObjectID>&	outCCacheIDs) const;
			
		CCILockID		Lock () const;
		void			Unlock (
			const CCILockID&		inLock);

		bool			Compare (
			const CCIContextData::UniqueID&		inCompareTo);

	private:
		// IDs for the ccaches are kept in defaultness order in the list
		// so that the first ccache is always the default one etc
		typedef	Implementations::Deque <CCICCacheData::UniqueID>::Shared		SharedCCacheCollection;
		SharedCCacheCollection						mCCaches;
		CCITime										mChangeTime;
		CCITime										mLastTimeStamp;
	
		void 			Changed ();
		bool			FindCCache (
			const std::string&		inName,
			CCICCacheData*&			outCCache) const;
		void			RemoveCCache (
			const CCICCacheData&	inCCache);
		void			SetDefault (
			const CCICCacheData&	inCCache);
			
		CCITime			NewTimeStamp ();
			
		friend class CCICCacheData;

		static const char sInitialDefaultCCacheName[];
		
		// Disallowed
		CCIContextData (const CCIContextData&);
		CCIContextData& operator = (const CCIContextData&);
};

class CCIContextDataInterface {
	public:
		CCIContextDataInterface (
			const CCIContextData::UniqueID&		inContext):
                        mContext (CCIContextData::Resolve (inContext)) {
                }
			
		CCIContextData* operator -> () { return mContext; }
		
		static CCIContextData* GetGlobalContext () {
			return sGlobalContextProxy.Get ();
		};
			
	private:
		CCIContextData*		mContext;
		
		static	CCISharedStaticData <CCIContextData>		sGlobalContext;
                static	CCISharedStaticDataProxy <CCIContextData>	sGlobalContextProxy;

		
		// Disallowed
		CCIContextDataInterface (const CCIContextDataInterface&);
		CCIContextDataInterface& operator = (const CCIContextDataInterface&);
};

