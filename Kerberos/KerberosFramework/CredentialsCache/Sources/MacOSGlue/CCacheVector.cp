#include "CCache.config.h"
#include "CCacheVectorPrivate.h"
#include "Context.h"
#include "CCache.h"
#include "Credentials.h"
#include "CredentialsIterator.h"
#include "CCacheIterator.h"
#include "CCacheString.h"

template <class T>
class StFixFunctions {
    public:
        StFixFunctions (T* fix, bool restore = true):
            mFix (fix),
            mRestore (restore) {
            std::swap (mFix -> functions, mFix -> otherFunctions);
        }
        
        ~StFixFunctions () {
            if (mRestore)
                std::swap (mFix -> functions, mFix -> otherFunctions);
        }
    
    private:
        T*	mFix;
        bool mRestore;
};

typedef StFixFunctions <cc_ccache_d>	StFixCCacheFunctions;
typedef StFixFunctions <cc_context_d>	StFixContextFunctions;
typedef StFixFunctions <cc_ccache_iterator_d>	StFixCCacheIteratorFunctions;
typedef StFixFunctions <cc_credentials_d>	StFixCredentialsFunctions;
typedef StFixFunctions <cc_credentials_iterator_d> StFixCredentialsIteratorFunctions;
typedef StFixFunctions <cc_string_d> StFixStringFunctions;

cc_int32    __cc_context_release_vector (
	cc_context_t context)
{
    StFixContextFunctions 	fix (context, false);
    return CCEContext::Release (context);
}

cc_int32    __cc_context_get_change_time_vector (
	cc_context_t context,
	cc_time_t* time)
{
    StFixContextFunctions 	fix (context);
    return CCEContext::GetChangeTime (context, time);
}

cc_int32    __cc_context_get_default_ccache_name_vector (
	cc_context_t context,
	cc_string_t* name)
{
    StFixContextFunctions 	fix (context);
    return CCEContext::GetDefaultCCacheName (context, name);
}

cc_int32    __cc_context_open_ccache_vector (
	cc_context_t context,
	const char* name,
	cc_ccache_t* ccache)
{
    StFixContextFunctions 	fix (context);
    return CCEContext::OpenCCache (context, name, ccache);
}

cc_int32    __cc_context_open_default_ccache_vector (
	cc_context_t context,
	cc_ccache_t* ccache)
{
    StFixContextFunctions 	fix (context);
    return CCEContext::OpenDefaultCCache (context, ccache);
}

cc_int32    __cc_context_create_ccache_vector (
	cc_context_t context,
	const char* name,
	cc_int32 cred_vers,
	const char* principal, 
	cc_ccache_t* ccache)
{
    StFixContextFunctions 	fix (context);
    return CCEContext::CreateCCache (context, name, cred_vers, principal, ccache);
}


cc_int32    __cc_context_create_default_ccache_vector (
	cc_context_t context,
	cc_int32 cred_vers,
	const char* principal, 
	cc_ccache_t* ccache)
{
    StFixContextFunctions 	fix (context);
    return CCEContext::CreateDefaultCCache (context, cred_vers, principal, ccache);
}

cc_int32    __cc_context_create_new_ccache_vector (
	cc_context_t context,
	cc_int32 cred_vers,
	const char* principal, 
	cc_ccache_t* ccache)
{
    StFixContextFunctions 	fix (context);
    return CCEContext::CreateNewCCache (context, cred_vers, principal, ccache);
}

cc_int32    __cc_context_new_ccache_iterator_vector (
	cc_context_t context,
	cc_ccache_iterator_t* iterator)
{
    StFixContextFunctions 	fix (context);
    return CCEContext::NewCCacheIterator (context, iterator);
}

cc_int32    __cc_context_lock_vector (
	cc_context_t context,
	cc_uint32 lock_type,
	cc_uint32 block)
{
    StFixContextFunctions 	fix (context);
    return CCEContext::Lock (context, lock_type, block);
}

cc_int32    __cc_context_unlock_vector (
	cc_context_t context)
{
    StFixContextFunctions 	fix (context);
    return CCEContext::Unlock (context);
}

cc_int32    __cc_context_compare_vector (
	cc_context_t context,
	cc_context_t compare_to,
	cc_uint32* equal)
{
    StFixContextFunctions 	fix (context);
    return CCEContext::Compare (context, compare_to, equal);
}

cc_int32    __cc_ccache_release_vector (
   cc_ccache_t ccache)
{
    StFixCCacheFunctions fix (ccache, false);
    return CCECCache::Release (ccache);
}
   
cc_int32    __cc_ccache_destroy_vector (
   cc_ccache_t ccache)
{
    StFixCCacheFunctions fix (ccache, false);
    return CCECCache::Destroy (ccache);
}
   
cc_int32    __cc_ccache_set_default_vector (
   cc_ccache_t ccache)
{
    StFixCCacheFunctions fix (ccache);
    return CCECCache::SetDefault (ccache);
}
   
cc_uint32    __cc_ccache_get_credentials_version_vector (
   cc_ccache_t ccache,
   cc_uint32* credentials_version)
{
    StFixCCacheFunctions fix (ccache);
    return CCECCache::GetCredentialsVersion (ccache, credentials_version);
}

cc_int32    __cc_ccache_get_name_vector (
   cc_ccache_t ccache,
   cc_string_t* name)
{
    StFixCCacheFunctions fix (ccache);
    return CCECCache::GetName (ccache, name);
}

cc_int32    __cc_ccache_get_principal_vector (
   cc_ccache_t ccache,
   cc_uint32 credentials_version,
   cc_string_t* principal)
{
    StFixCCacheFunctions fix (ccache);
    return CCECCache::GetPrincipal (ccache, credentials_version, principal);
}
cc_int32    __cc_ccache_set_principal_vector (
   cc_ccache_t ccache,
   cc_uint32 credentials_version,
   const char* principal)
{
    StFixCCacheFunctions fix (ccache);
    return CCECCache::SetPrincipal (ccache, credentials_version, principal);
}

cc_int32    __cc_ccache_store_credentials_vector (
   cc_ccache_t ccache,
   const cc_credentials_union* credentials)
{
    StFixCCacheFunctions fix (ccache);
    return CCECCache::StoreCredentials (ccache, credentials);
}

cc_int32    __cc_ccache_remove_credentials_vector (
   cc_ccache_t ccache,
   cc_credentials_t credentials)
{
    StFixCCacheFunctions fix (ccache);
    StFixCredentialsFunctions fix2 (credentials);
    return CCECCache::RemoveCredentials (ccache, credentials);
}

cc_int32    __cc_ccache_new_credentials_iterator_vector (
   cc_ccache_t ccache,
   cc_credentials_iterator_t* iterator)
{
    StFixCCacheFunctions fix (ccache);
    return CCECCache::NewCredentialsIterator (ccache, iterator);
}

cc_int32    __cc_ccache_move_vector (
   cc_ccache_t source,
   cc_ccache_t destination)
{
    StFixCCacheFunctions fix (source, false);
    StFixCCacheFunctions fix2 (destination);
    return CCECCache::Move (source, destination);
}

cc_int32    __cc_ccache_lock_vector (
   cc_ccache_t ccache,
   cc_uint32 block,
   cc_uint32 lock_type)
{
    StFixCCacheFunctions fix (ccache);
    return CCECCache::Lock (ccache, block, lock_type);
}

cc_int32    __cc_ccache_unlock_vector (
   cc_ccache_t ccache)
{
    StFixCCacheFunctions fix (ccache);
    return CCECCache::Unlock (ccache);
}


cc_int32    __cc_ccache_get_last_default_time_vector (
   cc_ccache_t ccache,
   cc_time_t* time)
{
    StFixCCacheFunctions fix (ccache);
    return CCECCache::GetLastDefaultTime (ccache, time);
}

cc_int32    __cc_ccache_get_change_time_vector (
   cc_ccache_t ccache,
   cc_time_t* time)
{
    StFixCCacheFunctions fix (ccache);
    return CCECCache::GetChangeTime (ccache, time);
}

cc_int32    __cc_ccache_compare_vector (
  cc_ccache_t ccache,
  cc_ccache_t compare_to,
  cc_uint32* equal)
{
    StFixCCacheFunctions fix (ccache);
    StFixCCacheFunctions fix2 (compare_to);
    return CCECCache::Compare (ccache, compare_to, equal);
}

cc_int32	__cc_string_release_vector (
	cc_string_t string)
{
    StFixStringFunctions fix (string, false);
    return CCEString::Release (string);
}

cc_int32	__cc_credentials_release_vector (
	cc_credentials_t credentials)
{
    StFixCredentialsFunctions fix (credentials, false);
    return CCECredentials::Release (credentials);
}

cc_int32    __cc_credentials_compare_vector (
	cc_credentials_t credentials,
	cc_credentials_t compare_to,
	cc_uint32* equal)
{
    StFixCredentialsFunctions fix (credentials);
    StFixCredentialsFunctions fix2 (compare_to);
    return CCECredentials::Compare (credentials, compare_to, equal);
}

cc_int32    __cc_ccache_iterator_release_vector (
	cc_ccache_iterator_t iter)
{
    StFixCCacheIteratorFunctions fix (iter, false);
    return CCECCacheIterator::Release (iter);
}

cc_int32    __cc_ccache_iterator_next_vector (
	cc_ccache_iterator_t iter,
	cc_ccache_t* ccache)
{
    StFixCCacheIteratorFunctions fix (iter);
    return CCECCacheIterator::Next (iter, ccache);
}

cc_int32    __cc_credentials_iterator_release_vector (
	cc_credentials_iterator_t iter)
{
    StFixCredentialsIteratorFunctions fix (iter, false);
    return CCECredentialsIterator::Release (iter);
}

cc_int32    __cc_credentials_iterator_next_vector (
	cc_credentials_iterator_t iter,
	cc_credentials_t* credentials)
{
    StFixCredentialsIteratorFunctions fix (iter);
    return CCECredentialsIterator::Next (iter, credentials);
}

cc_int32 __cc_initialize_vector (
	cc_context_t*		outContext,
	cc_int32			inVersion,
	cc_int32*			outSupportedVersion,
	char const**		outVendor)
{
	return cc_initialize (outContext, inVersion, outSupportedVersion, outVendor);
}

cc_int32 __cc_shutdown_vector (
	apiCB**				ioContext)
{
        StFixContextFunctions fix (*ioContext, false);
	return cc_shutdown (ioContext);
}
	
cc_int32 __cc_get_NC_info_vector (
	apiCB*				inContext,
	infoNC***			outInfo)
{
        StFixContextFunctions fix (inContext);
	return cc_get_NC_info (inContext, outInfo);
}
	
cc_int32 __cc_get_change_time_vector (
	apiCB*				inContext,
	cc_time_t*			outTime)
{
        StFixContextFunctions fix (inContext);
	return cc_get_change_time (inContext, outTime);
}
	
cc_int32 __cc_open_vector (
	apiCB*				inContext,
	const char*			inName,
	cc_int32			inVersion,
	cc_uint32			inFlags,
	ccache_p**			outCCache)
{
        StFixContextFunctions fix (inContext);
	return cc_open (inContext, inName, inVersion, inFlags, outCCache);
}
	
cc_int32 __cc_create_vector (
	apiCB*				inContext,
	const char*			inName,
	const char*			inPrincipal,
	cc_int32			inVersion,
	cc_uint32			inFlags,
	ccache_p**			outCCache)
{
        StFixContextFunctions fix (inContext);
	return cc_create (inContext, inName, inPrincipal, inVersion, inFlags, outCCache);
}
	
cc_int32 __cc_close_vector (
	apiCB*				inContext,
	ccache_p**			ioCCache)
{
        StFixContextFunctions fix (inContext);
        StFixCCacheFunctions fix2 (*ioCCache, false);
	return cc_close (inContext, ioCCache);
}
	
cc_int32 __cc_destroy_vector (
	apiCB*				inContext,
	ccache_p**			ioCCache)
{
        StFixContextFunctions fix (inContext);
        StFixCCacheFunctions fix2 (*ioCCache, false);
	return cc_destroy (inContext, ioCCache);
}
	
cc_int32 __cc_seq_fetch_NCs_begin_vector (
	apiCB*				inContext,
	ccache_cit**		outIterator)
{
        StFixContextFunctions fix (inContext);
	return cc_seq_fetch_NCs_begin (inContext, outIterator);
}
	

cc_int32 __cc_seq_fetch_NCs_next_vector (
	apiCB*				inContext,
	ccache_p**			outCCache,
	ccache_cit*			inIterator)
{
        StFixContextFunctions fix (inContext);
	StFixCCacheIteratorFunctions fix2 (reinterpret_cast <ccache_cit_ccache*> (inIterator));
	return cc_seq_fetch_NCs_next (inContext, outCCache, inIterator);
}
	
cc_int32 __cc_seq_fetch_NCs_end_vector (
	apiCB*				inContext,
	ccache_cit**		ioIterator)
{
	StFixContextFunctions fix (inContext);
	StFixCCacheIteratorFunctions fix2 (reinterpret_cast <ccache_cit_ccache*> (*ioIterator), false);
	return cc_seq_fetch_NCs_end (inContext, ioIterator);
}
	
cc_int32 __cc_get_name_vector (
	apiCB*				inContext,
	ccache_p*			inCCache,
	char**				outName)
{
	StFixContextFunctions fix (inContext);
	StFixCCacheFunctions fix2 (inCCache);
	return cc_get_name (inContext, inCCache, outName);
}
	
cc_int32 __cc_get_cred_version_vector (
	apiCB*				inContext,
	ccache_p*			inCCache,
	cc_int32*			outVersion)
{
	StFixContextFunctions fix (inContext);
	StFixCCacheFunctions fix2 (inCCache);
	return cc_get_cred_version (inContext, inCCache, outVersion);
}

cc_int32 __cc_set_principal_vector (
	apiCB*				inContext,
	ccache_p*			inCCache,
	cc_int32			inVersion,
	char*				inPrincipal)
{
	StFixContextFunctions fix (inContext);
	StFixCCacheFunctions fix2 (inCCache);
	return cc_set_principal (inContext, inCCache, inVersion, inPrincipal);
}
	
cc_int32 __cc_get_principal_vector (
	apiCB*				inContext,
	ccache_p*			inCCache,
	char**				outPrincipal)
{
	StFixContextFunctions fix (inContext);
	StFixCCacheFunctions fix2 (inCCache);
	return cc_get_principal (inContext, inCCache, outPrincipal);
}
	
cc_int32 __cc_store_vector (
	apiCB*				inContext,
	ccache_p*			inCCache,
	cred_union			inCredentials)
{
	StFixContextFunctions fix (inContext);
	StFixCCacheFunctions fix2 (inCCache);
	return cc_store (inContext, inCCache, inCredentials);
}

cc_int32 __cc_remove_cred_vector (
	apiCB*				inContext,
	ccache_p*			inCCache,
	cred_union			inCredentials)
{
	StFixContextFunctions fix (inContext);
	StFixCCacheFunctions fix2 (inCCache);
	return cc_remove_cred (inContext, inCCache, inCredentials);
}

cc_int32 __cc_seq_fetch_creds_begin_vector (
	apiCB*				inContext,
	const ccache_p*		inCCache,
	ccache_cit**		outIterator)
{
	StFixContextFunctions fix (inContext);
	StFixCCacheFunctions fix2 (const_cast <ccache_p*> (inCCache));
	return cc_seq_fetch_creds_begin (inContext, inCCache, outIterator);
}

cc_int32 __cc_seq_fetch_creds_next_vector (
	apiCB*				inContext,
	cred_union**		outCreds,
	ccache_cit*			inIterator)
{
	StFixContextFunctions fix (inContext);
	StFixCredentialsIteratorFunctions fix2 (reinterpret_cast <ccache_cit_creds*> (inIterator));
	return cc_seq_fetch_creds_next (inContext, outCreds, inIterator);
}
	
cc_int32 __cc_seq_fetch_creds_end_vector (
	apiCB*				inContext,
	ccache_cit**		ioIterator)
{
	StFixContextFunctions fix (inContext);
	StFixCredentialsIteratorFunctions fix2 (reinterpret_cast <ccache_cit_creds*> (*ioIterator), false);
	return cc_seq_fetch_creds_end (inContext, ioIterator);
}
	
cc_int32 __cc_free_principal_vector (
	apiCB*				inContext,
	char**				ioPrincipal)
{
	StFixContextFunctions fix (inContext);
	return cc_free_principal (inContext, ioPrincipal);
}
	
cc_int32 __cc_free_name_vector (
	apiCB*				inContext,
	char**				ioName)
{
	StFixContextFunctions fix (inContext);
	return cc_free_name (inContext, ioName);
}

cc_int32 __cc_free_creds_vector (
	apiCB*				inContext,
	cred_union**		creds)
{
	StFixContextFunctions fix (inContext);
	return cc_free_creds (inContext, creds);
}

cc_int32 __cc_free_NC_info_vector (
	apiCB*				inContext,
	infoNC***			ioInfo)
{
	StFixContextFunctions fix (inContext);
	return cc_free_NC_info (inContext, ioInfo);
}
