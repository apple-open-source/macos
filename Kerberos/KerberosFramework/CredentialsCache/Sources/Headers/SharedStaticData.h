/*
 * CCISharedStaticData.h
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/Headers/SharedStaticData.h,v 1.15 2004/10/22 20:48:32 lxs Exp $
 */
 
/*
 * This class is used to initialize globally shared data
 *
 * When we are using data-only shared libraries and the system heap,
 * we can't get a constructor called when the data library is loaded,
 * because the data library can't contain any code.
 *
 * So, any class that has globally shared static data has to inherit
 * from CCISharedStaticData and provide InitializeStaticData to initialize
 * the static data
 */
 
#pragma once

template <class Data> class CCISharedStaticDataProxy;

template <class Data>
class CCISharedStaticData {
	public:
		CCISharedStaticData (): mRefCount (0), mData (NULL) {}
		~CCISharedStaticData () {}
		
	private:
		CCIUInt32		mRefCount;
		Data*			mData;
		
		friend class CCISharedStaticDataProxy <Data>;
};

template <class Data> 
class CCISharedStaticDataProxy {
	public:
		CCISharedStaticDataProxy (
			CCISharedStaticData <Data>&	inData):
			mData (inData),
			mInitialized (false) {
                }

		Data* Get () {
			if (!mInitialized) {
				if (mData.mRefCount == 0) {
					mData.mData = new Data ();
				}
				mData.mRefCount++;
				mInitialized = true;
			}

			return mData.mData;
		}
		
		~CCISharedStaticDataProxy () {
			if (mInitialized) {
				mInitialized = false;
				mData.mRefCount--;
				if (mData.mRefCount == 0) {
					delete mData.mData;
				}
			}
		}
		
		CCILockID	Lock () const {
			#pragma message (CCIMessage_Warning_ "CCISharedStaticDataProxy::Lock unimplemented"
			return 0;
		}
		
		void Unlock (CCILockID /* inLock */) {
			#pragma message (CCIMessage_Warning_ "CCISharedStaticDataProxy::Unlock unimplemented"
		}
	
	private:
		CCISharedStaticData <Data>&		mData;
		bool							mInitialized;
};
