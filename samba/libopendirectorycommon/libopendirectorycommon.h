/*
 *  libopendirectorycommon.h
 *
 */

#include <sys/types.h>

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesConst.h>
#include <DirectoryService/DirServicesUtils.h>

/* Definitions */
#ifdef __cplusplus
extern "C" {
#endif

int set_opendirectory_authenticator(u_int32_t authenticatorlen, char *authenticator, u_int32_t secretlen, char *secret);

void  *get_opendirectory_authenticator();

u_int32_t get_opendirectory_authenticator_accountlen(void *authenticator);
void *get_opendirectory_authenticator_account(void *authenticator);

u_int32_t get_opendirectory_authenticator_secretlen(void *authenticator);
void *get_opendirectory_authenticator_secret(void *authenticator);

void delete_opendirectory_authenticator(void*authenticator);

//tDirStatus opendirectory_cred_session_key(char *client_challenge, char *server_challenge, char *machine_acct, char *session_key);
//tDirStatus opendirectory_user_session_key(char *account, char *session_key);

u_int32_t opendirectory_add_data_buffer_item(tDataBufferPtr dataBuffer, u_int32_t len, void *buffer);

tDirStatus opendirectory_authenticate_node(tDirReference	dirRef, tDirNodeReference nodeRef);

tDirStatus opendirectory_user_session_key(const char *account_name, char *session_key, char *slot_id);
tDirStatus opendirectory_ntlmv2user_session_key(const char *account_name, u_int32_t ntv2response_len, char* ntv2response, char* domain, u_int32_t *session_key_len, char *session_key, char 
*slot_id);
tDirStatus opendirectory_cred_session_key(char *client_challenge, char *server_challenge, char *machine_acct, char *session_key, char *slot_id);
tDirStatus opendirectory_set_workstation_nthash(char *account_name, char *nt_hash, char *slot_id);
tDirStatus opendirectory_lmchap2changepasswd(char *account_name, char *passwordData, char *passwordHash, u_int8_t passwordFormat, char *slot_id);

#ifdef __cplusplus
}
#endif
