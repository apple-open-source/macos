#ifdef __cplusplus
extern "C" {
#endif

#include <Kerberos/CredentialsCache.h>
#include <Kerberos/CredentialsCache2.h>

cc_int32    __cc_context_release_vector (
	cc_context_t context);

cc_int32    __cc_context_get_change_time_vector (
	cc_context_t context,
	cc_time_t* time);

cc_int32    __cc_context_get_default_ccache_name_vector (
	cc_context_t context,
	cc_string_t* name);

cc_int32    __cc_context_open_ccache_vector (
	cc_context_t context,
	const char* name,
	cc_ccache_t* ccache);

cc_int32    __cc_context_open_default_ccache_vector (
	cc_context_t context,
	cc_ccache_t* ccache);

cc_int32    __cc_context_create_ccache_vector (
	cc_context_t context,
	const char* name,
	cc_int32 cred_vers,
	const char* principal, 
	cc_ccache_t* ccache);

cc_int32    __cc_context_create_default_ccache_vector (
	cc_context_t context,
	cc_int32 cred_vers,
	const char* principal, 
	cc_ccache_t* ccache);

cc_int32    __cc_context_create_new_ccache_vector (
	cc_context_t context,
	cc_int32 cred_vers,
	const char* principal, 
	cc_ccache_t* ccache);

cc_int32    __cc_context_new_ccache_iterator_vector (
	cc_context_t context,
	cc_ccache_iterator_t* iterator);

cc_int32    __cc_context_lock_vector (
	cc_context_t context,
	cc_uint32 lock_type,
	cc_uint32 block);

cc_int32    __cc_context_unlock_vector (
	cc_context_t context);

cc_int32    __cc_context_compare_vector (
	cc_context_t context,
	cc_context_t compare_to,
	cc_uint32* equal);

cc_int32    __cc_ccache_release_vector (
   cc_ccache_t ccache);
   
cc_int32    __cc_ccache_destroy_vector (
   cc_ccache_t ccache);
   
cc_int32    __cc_ccache_set_default_vector (
   cc_ccache_t ccache);
   
cc_uint32    __cc_ccache_get_credentials_version_vector (
   cc_ccache_t ccache,
   cc_uint32* credentials_version);

cc_int32    __cc_ccache_get_name_vector (
   cc_ccache_t ccache,
   cc_string_t* name);

cc_int32    __cc_ccache_get_principal_vector (
   cc_ccache_t ccache,
   cc_uint32 credentials_version,
   cc_string_t* principal);

cc_int32    __cc_ccache_set_principal_vector (
   cc_ccache_t ccache,
   cc_uint32 credentials_version,
   const char* principal);

cc_int32    __cc_ccache_store_credentials_vector (
   cc_ccache_t ccache,
   const cc_credentials_union* credentials);

cc_int32    __cc_ccache_remove_credentials_vector (
   cc_ccache_t ccache,
   cc_credentials_t credentials);

cc_int32    __cc_ccache_new_credentials_iterator_vector (
   cc_ccache_t ccache,
   cc_credentials_iterator_t* iterator);

cc_int32    __cc_ccache_move_vector (
   cc_ccache_t source,
   cc_ccache_t destination);

cc_int32    __cc_ccache_lock_vector (
   cc_ccache_t ccache,
   cc_uint32 block,
   cc_uint32 lock_type);

cc_int32    __cc_ccache_unlock_vector (
   cc_ccache_t ccache);

cc_int32    __cc_ccache_get_last_default_time_vector (
   cc_ccache_t ccache,
   cc_time_t* time);

cc_int32    __cc_ccache_get_change_time_vector (
   cc_ccache_t ccache,
   cc_time_t* time);

cc_int32    __cc_ccache_compare_vector (
	cc_ccache_t ccache,
	cc_ccache_t compare_to,
	cc_uint32* equal);

cc_int32	__cc_string_release_vector (
	cc_string_t string);

cc_int32	__cc_credentials_release_vector (
	cc_credentials_t credentials);

cc_int32    __cc_credentials_compare_vector (
	cc_credentials_t credentials,
	cc_credentials_t compare_to,
	cc_uint32* equal);

cc_int32    __cc_ccache_iterator_release_vector (
	cc_ccache_iterator_t iter);

cc_int32    __cc_ccache_iterator_next_vector (
	cc_ccache_iterator_t iter,
	cc_ccache_t* ccache);

cc_int32    __cc_credentials_iterator_release_vector (
	cc_credentials_iterator_t iter);

cc_int32    __cc_credentials_iterator_next_vector (
	cc_credentials_iterator_t iter,
	cc_credentials_t* credentials);


cc_int32 __cc_initialize_vector (
	cc_context_t*		outContext,
	cc_int32			inVersion,
	cc_int32*			outSupportedVersion,
	char const**		outVendor);

cc_int32 __cc_shutdown_vector (
	apiCB**				ioContext);
	
cc_int32 __cc_get_NC_info_vector (
	apiCB*				inContext,
	infoNC***			outInfo);
	
cc_int32 __cc_get_change_time_vector (
	apiCB*				inContext,
	cc_time_t*			outTime);
	
cc_int32 __cc_open_vector (
	apiCB*				inContext,
	const char*			inName,
	cc_int32			inVersion,
	cc_uint32			inFlags,
	ccache_p**			outCCache);
	
cc_int32 __cc_create_vector (
	apiCB*				inContext,
	const char*			inName,
	const char*			inPrincipal,
	cc_int32			inVersion,
	cc_uint32			inFlags,
	ccache_p**			outCCache);
	
cc_int32 __cc_close_vector (
	apiCB*				inContext,
	ccache_p**			ioCCache);
	
cc_int32 __cc_destroy_vector (
	apiCB*				inContext,
	ccache_p**			ioCCache);
	
cc_int32 __cc_seq_fetch_NCs_begin_vector (
	apiCB*				inContext,
	ccache_cit**		outIterator);

cc_int32 __cc_seq_fetch_NCs_next_vector (
	apiCB*				inContext,
	ccache_p**			outCCache,
	ccache_cit*			inIterator);

cc_int32 __cc_seq_fetch_NCs_end_vector (
	apiCB*				inContext,
	ccache_cit**		ioIterator);

cc_int32 __cc_get_name_vector (
	apiCB*				inContext,
	ccache_p*			inCCache,
	char**				outName);
	
cc_int32 __cc_get_cred_version_vector (
	apiCB*				inContext,
	ccache_p*			inCCache,
	cc_int32*			outVersion);
	
cc_int32 __cc_set_principal_vector (
	apiCB*				inContext,
	ccache_p*			inCCache,
	cc_int32			inVersion,
	char*				inPrincipal);
	
cc_int32 __cc_get_principal_vector (
	apiCB*				inContext,
	ccache_p*			inCCache,
	char**				outPrincipal);
	
cc_int32 __cc_store_vector (
	apiCB*				inContext,
	ccache_p*			inCCache,
	cred_union			inCredentials);

cc_int32 __cc_remove_cred_vector (
	apiCB*				inContext,
	ccache_p*			inCCache,
	cred_union			inCredentials);

cc_int32 __cc_seq_fetch_creds_begin_vector (
	apiCB*				inContext,
	const ccache_p*		inCCache,
	ccache_cit**		outIterator);

cc_int32 __cc_seq_fetch_creds_next_vector (
	apiCB*				inContext,
	cred_union**		outCreds,
	ccache_cit*			inIterator);
	
cc_int32 __cc_seq_fetch_creds_end_vector (
	apiCB*				inContext,
	ccache_cit**		ioIterator);
	
cc_int32 __cc_free_principal_vector (
	apiCB*				inContext,
	char**				ioPrincipal);

cc_int32 __cc_free_name_vector (
	apiCB*				inContext,
	char**				ioName);

cc_int32 __cc_free_creds_vector (
	apiCB*				inContext,
	cred_union**		creds);

cc_int32 __cc_free_NC_info_vector (
	apiCB*				inContext,
	infoNC***			ioInfo);

#ifdef __cplusplus
}
#endif
