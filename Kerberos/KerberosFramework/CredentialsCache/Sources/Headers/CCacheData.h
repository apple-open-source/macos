#pragma once

#include "Allocators.h"
#include "UniqueGlobally.h"
#include "Implementations.h"

#include "CredentialsData.h"

class CCIContextData;
class CCICCacheData;

class CCICCacheData:
	public CCIUniqueGlobally <CCICCacheData> {
	public:
	
		enum {
			objectNotFound = ccErrCCacheNotFound
		};
		
		typedef	Implementations::List <CCICCacheData>::Shared		SharedCCacheDataList;
			
	
		CCICCacheData ();
		CCICCacheData (
					CCIContextData*		inContext,
			const	std::string&		inName,
			CCIUInt32					inVersion,
			const	std::string&		inPrincipal);
		~CCICCacheData ();
		
		void		Destroy ();

		void		SetDefault ();

		CCIUInt32	GetCredentialsVersion () const;
		
		std::string	GetPrincipal (
			CCIUInt32				inVersion) const;
		
		std::string	GetName () const;
		
		void		SetPrincipal (
			CCIUInt32				inVersion,
			const std::string&		inPrincipal);
		
#if CCache_v2_compat		
		void		CompatSetPrincipal (
			CCIUInt32				inVersion,
			const std::string&		inPrincipal);
#endif
		
		void		StoreCredentials (
			const cc_credentials_union*				inCredentials);

		void		StoreCredentials (
			CCICredentialsData*					inCredentials);

#if CCache_v2_compat		
		void		CompatStoreCredentials (
			const cred_union&						inCredentials);
#endif
		
		void		RemoveCredentials (
			const CCICredentialsData::UniqueID&		inCredentials);
			
		CCITime		GetLastDefaultTime () const;

		CCITime		GetChangeTime () const;
		
		void		Move (
			const UniqueID&			inDestination);
			
		CCILockID	Lock () const;

		void		Unlock (
			CCILockID			inLock);
			
		void			GetCredentialsIDs (
			std::vector <CCIObjectID>&	outCredentialsIDs) const;
			
		bool			Compare (
			const CCICCacheData::UniqueID&		inCompareTo);

        CCITime		GetKDCTimeOffset (
            CCIUInt32				inVersion) const;
            
        void		SetKDCTimeOffset (
            CCIUInt32				inVersion,
            CCITime					inTimeOffset);

        void		ClearKDCTimeOffset (
            CCIUInt32				inVersion);
        
	private:
	
		typedef	Implementations::String::Shared							SharedString;
		typedef	Implementations::Vector <CCICredentialsData*>::Shared	SharedCredentialsVector;
	
		CCIContextData*							mContext;
		CCITime									mChangeTime;
		bool									mHasBeenDefault;
		CCITime									mLastDefaultTime;
		SharedString							mName;
		bool									mHavePrincipalV4;
		SharedString							mPrincipalV4;
		bool									mHavePrincipalV5;
		SharedString							mPrincipalV5;
		SharedCredentialsVector					mCredentials;
        bool									mHaveKDCTimeOffsetV4;
        CCITime									mKDCTimeOffsetV4;
        bool									mHaveKDCTimeOffsetV5;
        CCITime									mKDCTimeOffsetV5;
		
		void Changed ();

		// Disallowed
		CCICCacheData (const CCICCacheData&);
		CCICCacheData& operator = (const CCICCacheData&);
};

class CCICCacheDataInterface {
	public:
		CCICCacheDataInterface (
			const CCICCacheData::UniqueID&		inCCache):
                        mCCache (CCICCacheData::Resolve (inCCache)) {
                }
			
		CCICCacheData* operator -> () { return mCCache; }
		
	private:
		CCICCacheData*	mCCache;

		// Disallowed
		CCICCacheDataInterface (const CCICCacheDataInterface&);
		CCICCacheDataInterface& operator = (const CCICCacheDataInterface&);
};

