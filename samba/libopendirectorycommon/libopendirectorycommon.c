/*
 *  libopendirectorycommon.c
 *
 */

#include "libopendirectorycommon.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

/* Private */

#define credentialfile "/var/db/samba/opendirectorysam"

#define sig 'odsa'

typedef struct opendirectory_secret_header {
    u_int32_t signature;
    u_int32_t authenticator_len;
    u_int32_t secret_len;
    u_int32_t authenticatorid_len;
} opendirectory_secret_header;

opendirectory_secret_header *get_opendirectory_secret_header(void *authenticator)
{
	opendirectory_secret_header *hdr;
	
	if (authenticator) {
		hdr = (opendirectory_secret_header *)authenticator;
		if (hdr->signature == sig) {
			return hdr;
		}
	}
	return NULL;
}

/* Public */

int set_opendirectory_authenticator(u_int32_t authenticatorlen, char *authenticator, u_int32_t secretlen, char *secret)
{
	int fd = 0;
	int result = -1;
	struct flock lock;
	opendirectory_secret_header hdr;
	int initialized = 0;
	
	if (authenticatorlen && authenticator && secretlen && secret) {
		fd = open(credentialfile, O_CREAT | O_WRONLY | O_TRUNC,0600);
		if (fd == -1) {
			fprintf(stderr,"unable to open file (%s)\n",strerror(errno));
		} else {
			lock.l_type = F_WRLCK;
			lock.l_start = 0;
			lock.l_whence = SEEK_CUR;
			lock.l_len = 0;
			if (fcntl(fd, F_SETFL,&lock) == -1)
				result = errno;
			else {
				memset(&hdr, 0, sizeof(opendirectory_secret_header));
				hdr.signature = sig;
				hdr.authenticator_len = authenticatorlen;
				hdr.secret_len = secretlen;
				if (write(fd, &hdr, sizeof(opendirectory_secret_header)) != sizeof(opendirectory_secret_header))
					goto cleanup;
				if (write(fd, authenticator, authenticatorlen) != authenticatorlen)
					goto cleanup;
				if (write(fd, secret, secretlen) != secretlen)
					goto cleanup;
				initialized = 1;
				result = 0;
			}
				
		}
	}
cleanup:
	if (fd)
		close(fd);
	if (!initialized)
		unlink(credentialfile);
	return result;
}

void  *get_opendirectory_authenticator()
{
	int fd = 0;
	void *authenticator = NULL;
	opendirectory_secret_header hdr;
	int authentriessize;
	int initialized = 0;
	
	fd = open(credentialfile, O_RDONLY,0);
	if (fd != -1) {
		printf("get_opendirectory_authenticator: opened file\n");
		
//		if(pread(fd, &hdr, sizeof(opendirectory_secret_header), 0) != sizeof(opendirectory_secret_header)) {
		if(read(fd, &hdr, sizeof(opendirectory_secret_header)) != sizeof(opendirectory_secret_header)) {
			printf("get_opendirectory_authenticator: bad hdr(%ld)\n", sizeof(opendirectory_secret_header));
			goto cleanup;
		}
		if (hdr.signature != sig) {
			printf("get_opendirectory_authenticator: bad signature(%X)\n", hdr.signature);
			goto cleanup;
		}
		authentriessize = hdr.authenticator_len + hdr.secret_len;
		authenticator = malloc(sizeof(opendirectory_secret_header) + authentriessize);
		memset(authenticator, 0, sizeof(opendirectory_secret_header) + authentriessize);
		memcpy(authenticator, &hdr, sizeof(opendirectory_secret_header));
//		if(pread(fd, authenticator + sizeof(hdr), authentriessize, sizeof(hdr)) != authentriessize) {
		if(read(fd, authenticator + sizeof(opendirectory_secret_header), authentriessize) != authentriessize) {
			printf("get_opendirectory_authenticator: bad authentriessize(%d)\n", authentriessize);
			goto cleanup;
		}
		initialized = 1;
	} else {
		printf("unable to open file (%s)\n",strerror(errno));
	}
cleanup:
	if (fd)
		close(fd);
	if (!initialized) {
		if (authenticator)
			free(authenticator);
		return NULL;
	} else 
		return authenticator;
}


u_int32_t get_opendirectory_authenticator_accountlen(void *authenticator)
{
	opendirectory_secret_header *hdr = get_opendirectory_secret_header(authenticator);
	u_int32_t len = 0;
	
	if (hdr)
		len = hdr->authenticator_len;
		
	return len;
}

void *get_opendirectory_authenticator_account(void *authenticator)
{
	opendirectory_secret_header *hdr = get_opendirectory_secret_header(authenticator);
	void *result = NULL;

	if (hdr)
		result = authenticator + sizeof(opendirectory_secret_header);
		
	return result;
}

u_int32_t get_opendirectory_authenticator_secretlen(void *authenticator)
{
	opendirectory_secret_header *hdr = get_opendirectory_secret_header(authenticator);
	u_int32_t len = 0;
	
	if (hdr)
		len = hdr->secret_len;
		
	return len;

}
void *get_opendirectory_authenticator_secret(void *authenticator)
{
	opendirectory_secret_header *hdr = get_opendirectory_secret_header(authenticator);
	void *result = NULL;
	
	if (hdr)
		result = authenticator + sizeof(opendirectory_secret_header) + hdr->authenticator_len;

	return result;
}

void delete_opendirectory_authenticator(void*authenticator)
{
	opendirectory_secret_header *hdr = get_opendirectory_secret_header(authenticator);
	
	if (hdr) {
		bzero(authenticator, sizeof(opendirectory_secret_header) + hdr->authenticator_len + hdr->secret_len);
		free(authenticator);
	}
}

tDirStatus get_node_ref_and_name(tDirReference dirRef, char *name, char *recordType, tDirNodeReference *nodeRef, char *outRecordName)
{
    tDirStatus			status			= eDSNoErr;
	unsigned long	bufferSize		= 1024 * 10;
    long			returnCount		= 0;
    tDataBufferPtr		nodeBuffer		= NULL;
    tDirNodeReference		searchNodeRef		= NULL;
    tDataListPtr		searchNodeName		= NULL;
    tDataListPtr		NodePath		= NULL;
    char			NodePathStr[256]	= {0};
//    char			nameBuffer[128]	= {0};

    tDataListPtr		recName			= NULL;
    tDataListPtr		recType			= NULL;
    tDataListPtr		attrType		= NULL;

    tAttributeListRef		attributeListRef	= 0;
    tRecordEntryPtr		outRecordEntryPtr	= NULL;
    tAttributeEntryPtr		attributeInfo		= NULL;
    tAttributeValueListRef	attributeValueListRef	= 0;
    tAttributeValueEntryPtr	attrValue		= NULL;
    long			i			= 0;
    
    nodeBuffer = dsDataBufferAllocate(dirRef, bufferSize);
    if (nodeBuffer == NULL) goto cleanup;
    status = dsFindDirNodes(dirRef, nodeBuffer, NULL, eDSSearchNodeName, &returnCount, NULL);
    if ((status != eDSNoErr) || (returnCount <= 0)) goto cleanup;

    status = dsGetDirNodeName(dirRef, nodeBuffer, 1, &searchNodeName);
    if (status != eDSNoErr) goto cleanup;
    status = dsOpenDirNode(dirRef, searchNodeName, &searchNodeRef);
    if (status != eDSNoErr) goto cleanup;

    recName = dsBuildListFromStrings(dirRef, name, NULL);
    recType = dsBuildListFromStrings(dirRef, recordType, NULL);	
    attrType = dsBuildListFromStrings(dirRef, kDSNAttrMetaNodeLocation, kDSNAttrRecordName, NULL);

    status = dsGetRecordList(searchNodeRef, nodeBuffer, recName, eDSiExact, recType, attrType, 0, &returnCount, NULL);
    if (status != eDSNoErr) goto cleanup;

    status = dsGetRecordEntry(searchNodeRef, nodeBuffer, 1, &attributeListRef, &outRecordEntryPtr);
    if (status == eDSNoErr)
    {
        for (i = 1 ; i <= outRecordEntryPtr->fRecordAttributeCount; i++)
        {
            status = dsGetAttributeEntry(searchNodeRef, nodeBuffer, attributeListRef, i, &attributeValueListRef, &attributeInfo);
            status = dsGetAttributeValue(searchNodeRef, nodeBuffer, 1, attributeValueListRef, &attrValue);
            if (attributeValueListRef != 0)
            {
                    dsCloseAttributeValueList(attributeValueListRef);
                    attributeValueListRef = 0;
            }	
            if (status == eDSNoErr)
            {
                if (strncmp(attributeInfo->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation, strlen(kDSNAttrMetaNodeLocation)) == 0)
                    strncpy(NodePathStr, attrValue->fAttributeValueData.fBufferData, attrValue->fAttributeValueData.fBufferSize);
                else if (strncmp(attributeInfo->fAttributeSignature.fBufferData, kDSNAttrRecordName, strlen(kDSNAttrRecordName)) == 0)
                    strncpy(outRecordName, attrValue->fAttributeValueData.fBufferData, attrValue->fAttributeValueData.fBufferSize);
            }
            if (attrValue != NULL) {
                    dsDeallocAttributeValueEntry(dirRef, attrValue);
                    attrValue = NULL;
            }				
            if (attributeInfo != NULL) {
                    dsDeallocAttributeEntry(dirRef, attributeInfo);
                    attributeInfo = NULL;
            }
        }
        if (outRecordEntryPtr != NULL) {
            dsDeallocRecordEntry(dirRef, outRecordEntryPtr);
            outRecordEntryPtr = NULL;
        }
        if (strlen(NodePathStr) != 0 && strlen(outRecordName) != 0)
        {
 			//DEBUG(0,("getnode: kDSNAttrMetaNodeLocation(%s)\n", NodePathStr));
           NodePath = dsBuildFromPath(dirRef, NodePathStr, "/");
            status = dsOpenDirNode(dirRef, NodePath, nodeRef);
            dsDataListDeallocate( dirRef, NodePath);
            free(NodePath);
        }
    }
cleanup:
    if (nodeBuffer != NULL)
        dsDataBufferDeAllocate(dirRef, nodeBuffer);
    if (searchNodeName != NULL)
    {
        dsDataListDeallocate(dirRef, searchNodeName);
        free(searchNodeName);
    }
    if (searchNodeRef != NULL)
        dsCloseDirNode(searchNodeRef);
    if (recName != NULL)
    {
        dsDataListDeallocate(dirRef, recName);
        free(recName);
    }
    if (recType != NULL)
    {
        dsDataListDeAllocate(dirRef, recType, 0);
        free(recType);
    }
    if (attrType != NULL)
    {
        dsDataListDeAllocate(dirRef, attrType, 0);
        free(attrType);
    }

    return status;
}

u_int32_t opendirectory_add_data_buffer_item(tDataBufferPtr dataBuffer, u_int32_t len, void *buffer)
{
	u_int32_t result = 0;
	
	if (dataBuffer->fBufferLength + len + 4 <= dataBuffer->fBufferSize) {
		memcpy( &(dataBuffer->fBufferData[ dataBuffer->fBufferLength ]), &len, 4);
		dataBuffer->fBufferLength += 4;
		if (len != 0) {
			memcpy( &(dataBuffer->fBufferData[ dataBuffer->fBufferLength ]), buffer, len);
			dataBuffer->fBufferLength += len;
		}
	}
	
	return result;	
}

tDirStatus opendirectory_cred_session_key(char *client_challenge, char *server_challenge, char *machine_acct, char *session_key, char *slot_id)
{
    tDirReference	dirRef = NULL;
	tDirStatus 		status			= eDSNoErr;
	tDirStatus 		bufferStatus	= eDSNoErr;
	unsigned long	bufferSize		= 1024 * 10;
	unsigned long		curr			= 0;
	unsigned long		len			= 0;
	tDataBufferPtr		authBuff  		= NULL;
	tDataBufferPtr		stepBuff  		= NULL;
	tDataNodePtr		authType		= NULL;
	tDataNodePtr		recordType		= NULL;
	tDirNodeReference nodeRef = NULL;
	char *targetaccount = NULL;
	char recordName[255] = {0};
	
    status = dsOpenDirService(&dirRef);
	
	if (status != eDSNoErr)
		return status;
	
	status = get_node_ref_and_name(dirRef, machine_acct, kDSStdRecordTypeComputers, &nodeRef, recordName);

	if (status != eDSNoErr)
		goto cleanup;

	status = opendirectory_authenticate_node(dirRef, nodeRef);

	if (status != eDSNoErr)
		goto cleanup;

	if (slot_id && strlen(slot_id))
		targetaccount = slot_id;
	else
		targetaccount = recordName;

	authBuff = dsDataBufferAllocate( dirRef, bufferSize );
	if ( authBuff != NULL )
	{
			authBuff->fBufferLength = 0;
			stepBuff = dsDataBufferAllocate( dirRef, bufferSize );
			if ( stepBuff != NULL )
			{
					authType = dsDataNodeAllocateString( dirRef,  kDSStdAuthSMBWorkstationCredentialSessionKey);
					recordType = dsDataNodeAllocateString( dirRef,  kDSStdRecordTypeComputers);
					if ( authType != NULL )
					{
							// Target account
							len = strlen( targetaccount );
							memcpy( &(authBuff->fBufferData[ curr ]), &len, 4 );
							curr += sizeof( long );
							memcpy( &(authBuff->fBufferData[ curr ]), targetaccount, len );
							curr += len;
							// Client Challenge and Server Challenge
							len = 16;
							memcpy( &(authBuff->fBufferData[ curr ]), &len, 4 );
							curr += sizeof( long );
							memcpy( &(authBuff->fBufferData[ curr ]), server_challenge, 8 );
							curr += 8;
							memcpy( &(authBuff->fBufferData[ curr ]), client_challenge, 8 );
							curr += 8;

							authBuff->fBufferLength = curr;
							status = dsDoDirNodeAuthOnRecordType( nodeRef, authType, 1, authBuff, stepBuff, NULL,  recordType);
							if ( status == eDSNoErr )
							{
									//DEBUG(1,("kDSStdAuthSMBWorkstationCredentialSessionKey was successful for  \"%s\" :)\n", machine_acct));
									memcpy(&len, stepBuff->fBufferData, 4);
									stepBuff->fBufferData[len+4] = '\0';
									memcpy(session_key,stepBuff->fBufferData+4, 8);
									printf("session key (%s) len (%ld)\n",session_key, len);
									//DEBUG(1,("session key  \"%s\" :)\n", policy));

							}
							else
							{
									// DEBUG(1,("kDSStdAuthGetPolicy FAILED for user \"%s\" (%d) :(\n", userid, status) );
							}
					}
					bufferStatus = dsDataBufferDeAllocate( dirRef, stepBuff );
					if ( bufferStatus != eDSNoErr )
					{
							// DEBUG(1,("get_password_policy: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", bufferStatus) );
					}
			}
			else
			{
					// DEBUG(1,("get_password_policy: *** dsDataBufferAllocate(2) faild with \n" ));
			}
			bufferStatus = dsDataBufferDeAllocate( dirRef, authBuff );
			if ( bufferStatus != eDSNoErr )
			{
					// DEBUG(1,( "get_password_policy: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", status ));
			}
	}
	else
	{
			// DEBUG(1,("get_password_policy: *** dsDataBufferAllocate(1) faild with \n" ));
	}
cleanup:
    if (authType != NULL)
		dsDataNodeDeAllocate(dirRef, authType);
    if (recordType != NULL)
		dsDataNodeDeAllocate(dirRef, recordType);
	if (dirRef)
		dsCloseDirService(dirRef);
	if (nodeRef)
		dsCloseDirNode(nodeRef);
	return status;
}

tDirStatus opendirectory_user_session_key(const char *account_name, char *session_key, char *slot_id)
{
    tDirReference	dirRef = NULL;
	tDirStatus 		status			= eDSNoErr;
	tDirStatus 		bufferStatus	= eDSNoErr;
	unsigned long	bufferSize		= 1024 * 10;
	unsigned long		len			= 0;
	tDataBufferPtr		authBuff  		= NULL;
	tDataBufferPtr		stepBuff  		= NULL;
	tDataNodePtr		authType		= NULL;
	tDataNodePtr		recordType		= NULL;
	tDirNodeReference nodeRef = NULL;
	char *targetaccount = NULL;
	char recordName[255] = {0};
		
    status = dsOpenDirService(&dirRef);
	
	if (status != eDSNoErr)
		return status;
	
	status = get_node_ref_and_name(dirRef, account_name, kDSStdRecordTypeUsers, &nodeRef, recordName);

	if (status != eDSNoErr)
		goto cleanup;

	status = opendirectory_authenticate_node(dirRef, nodeRef);

	if (status != eDSNoErr)
		goto cleanup;

	if (slot_id && strlen(slot_id))
		targetaccount = slot_id;
	else
		targetaccount = recordName;

	authBuff = dsDataBufferAllocate( dirRef, bufferSize );
	if ( authBuff != NULL )
	{
			authBuff->fBufferLength = 0;
			stepBuff = dsDataBufferAllocate( dirRef, bufferSize );
			if ( stepBuff != NULL )
			{
					authType = dsDataNodeAllocateString( dirRef,  kDSStdAuthSMB_NT_UserSessionKey);
					recordType = dsDataNodeAllocateString( dirRef,  kDSStdRecordTypeUsers);
					if ( authType != NULL )
					{
							// Target account
							opendirectory_add_data_buffer_item(authBuff, strlen( targetaccount ), targetaccount);
							opendirectory_add_data_buffer_item(authBuff, 1, ""); // null terminate ??
							
							status = dsDoDirNodeAuthOnRecordType( nodeRef, authType, 1, authBuff, stepBuff, NULL,  recordType);
							if ( status == eDSNoErr )
							{
									//DEBUG(4,("kDSStdAuthSMB_NT_UserSessionKey was successful for  \"%s\" :)\n", targetaccount));
									memcpy(&len, stepBuff->fBufferData, 4);
									stepBuff->fBufferData[len+4] = '\0';
									memcpy(session_key,stepBuff->fBufferData+4, len);
									//DEBUG(4,("session key  \"%s\" :)\n", policy));

							}
							else
							{
									//DEBUG(4,("kDSStdAuthSMB_NT_UserSessionKey FAILED for user \"%s\" (%d) :(\n", targetaccount, status) );
							}
					}
					bufferStatus = dsDataBufferDeAllocate( dirRef, stepBuff );
					if ( bufferStatus != eDSNoErr )
					{
							//DEBUG(0,("kDSStdAuthSMB_NT_UserSessionKey: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", bufferStatus) );
					}
			}
			else
			{
					//DEBUG(0,("kDSStdAuthSMB_NT_UserSessionKey: *** dsDataBufferAllocate(2) faild with \n" ));
			}
			bufferStatus = dsDataBufferDeAllocate( dirRef, authBuff );
			if ( bufferStatus != eDSNoErr )
			{
					//DEBUG(0,( "kDSStdAuthSMB_NT_UserSessionKey: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", status ));
			}
	}
	else
	{
			//DEBUG(0,("kDSStdAuthSMB_NT_UserSessionKey: *** dsDataBufferAllocate(1) faild with \n" ));
	}
cleanup:
    if (authType != NULL)
		dsDataNodeDeAllocate(dirRef, authType);
    if (recordType != NULL)
		dsDataNodeDeAllocate(dirRef, recordType);
	if (dirRef)
		dsCloseDirService(dirRef);
	if (nodeRef)
		dsCloseDirNode(nodeRef);
	return status;
}

tDirStatus opendirectory_ntlmv2user_session_key(const char *account_name, char *serverchallenge, char* ntv2response, char *session_key, char *slot_id)
{
    tDirReference	dirRef = NULL;
	tDirStatus 		status			= eDSNoErr;
	tDirStatus 		bufferStatus	= eDSNoErr;
	unsigned long	bufferSize		= 1024 * 10;
	unsigned long		len			= 0;
	tDataBufferPtr		authBuff  		= NULL;
	tDataBufferPtr		stepBuff  		= NULL;
	tDataNodePtr		authType		= NULL;
	tDataNodePtr		recordType		= NULL;
	tDirNodeReference nodeRef = NULL;
	char *targetaccount = NULL;
	char recordName[255] = {0};
		
    status = dsOpenDirService(&dirRef);
	
	if (status != eDSNoErr)
		return status;
	
	status = get_node_ref_and_name(dirRef, account_name, kDSStdRecordTypeUsers, &nodeRef, recordName);

	if (status != eDSNoErr)
		goto cleanup;

	status = opendirectory_authenticate_node(dirRef, nodeRef);

	if (status != eDSNoErr)
		goto cleanup;

	if (slot_id && strlen(slot_id))
		targetaccount = slot_id;
	else
		targetaccount = recordName;

	authBuff = dsDataBufferAllocate( dirRef, bufferSize );
	if ( authBuff != NULL )
	{
			authBuff->fBufferLength = 0;
			stepBuff = dsDataBufferAllocate( dirRef, bufferSize );
			if ( stepBuff != NULL )
			{
					authType = dsDataNodeAllocateString( dirRef, "dsAuthMethodStandard:dsAuthNodeNTLMv2SessionKey"); /* kDSStdAuthSMBNTv2UserSessionKey */
					recordType = dsDataNodeAllocateString( dirRef,  kDSStdRecordTypeUsers);
					if ( authType != NULL )
					{
							// Target account
							opendirectory_add_data_buffer_item(authBuff, strlen( targetaccount ), targetaccount);
							opendirectory_add_data_buffer_item(authBuff, 8, serverchallenge);
							opendirectory_add_data_buffer_item(authBuff, 16, ntv2response);
							
							status = dsDoDirNodeAuthOnRecordType( nodeRef, authType, 1, authBuff, stepBuff, NULL,  recordType);
							if ( status == eDSNoErr )
							{
									//DEBUG(4,("kDSStdAuthSMBNTv2UserSessionKey was successful for  \"%s\" :)\n", targetaccount));
									memcpy(&len, stepBuff->fBufferData, 4);
									stepBuff->fBufferData[len+4] = '\0';
									memcpy(session_key,stepBuff->fBufferData+4, len);
									//DEBUG(4,("session key  \"%s\" :)\n", policy));

							}
							else
							{
									//DEBUG(4,("kDSStdAuthSMB_NT_UserSessionKey FAILED for user \"%s\" (%d) :(\n", targetaccount, status) );
							}
					}
					bufferStatus = dsDataBufferDeAllocate( dirRef, stepBuff );
					if ( bufferStatus != eDSNoErr )
					{
							//DEBUG(0,("kDSStdAuthSMB_NT_UserSessionKey: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", bufferStatus) );
					}
			}
			else
			{
					//DEBUG(0,("kDSStdAuthSMB_NT_UserSessionKey: *** dsDataBufferAllocate(2) faild with \n" ));
			}
			bufferStatus = dsDataBufferDeAllocate( dirRef, authBuff );
			if ( bufferStatus != eDSNoErr )
			{
					//DEBUG(0,( "kDSStdAuthSMB_NT_UserSessionKey: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", status ));
			}
	}
	else
	{
			//DEBUG(0,("kDSStdAuthSMB_NT_UserSessionKey: *** dsDataBufferAllocate(1) faild with \n" ));
	}
cleanup:
    if (authType != NULL)
		dsDataNodeDeAllocate(dirRef, authType);
    if (recordType != NULL)
		dsDataNodeDeAllocate(dirRef, recordType);
	if (dirRef)
		dsCloseDirService(dirRef);
	if (nodeRef)
		dsCloseDirNode(nodeRef);
	return status;
}

tDirStatus opendirectory_set_workstation_nthash(char *account_name, char *nt_hash, char *slot_id)
{
    tDirReference	dirRef = NULL;
	tDirStatus 		status			= eDSNoErr;
	tDirStatus 		bufferStatus	= eDSNoErr;
	unsigned long	bufferSize		= 1024 * 10;
	unsigned long		curr			= 0;
	unsigned long		len			= 0;
	tDataBufferPtr		authBuff  		= NULL;
	tDataBufferPtr		stepBuff  		= NULL;
	tDataNodePtr		authType		= NULL;
	tDataNodePtr		recordType		= NULL;
	tDirNodeReference nodeRef = NULL;
	char *targetaccount = NULL;
	char recordName[255] = {0};
	
    status = dsOpenDirService(&dirRef);
	
	if (status != eDSNoErr)
		return status;
	
	status = get_node_ref_and_name(dirRef, account_name, kDSStdRecordTypeUsers, &nodeRef, recordName);

	if (status != eDSNoErr)
		goto cleanup;

	status = opendirectory_authenticate_node(dirRef, nodeRef);

	if (status != eDSNoErr)
		goto cleanup;

	if (slot_id && strlen(slot_id))
		targetaccount = slot_id;
	else
		targetaccount = recordName;

	authBuff = dsDataBufferAllocate( dirRef, bufferSize );
	if ( authBuff != NULL )
	{
			authBuff->fBufferLength = 0;
			stepBuff = dsDataBufferAllocate( dirRef, bufferSize );
			if ( stepBuff != NULL )
			{
					authType = dsDataNodeAllocateString( dirRef,  kDSStdAuthSetWorkstationPasswd);
					recordType = dsDataNodeAllocateString( dirRef,  kDSStdRecordTypeUsers);
					if ( authType != NULL )
					{
							// Target account
							len = strlen( targetaccount );
							memcpy( &(authBuff->fBufferData[ curr ]), &len, 4 );
							curr += sizeof( long );
							memcpy( &(authBuff->fBufferData[ curr ]), targetaccount, len );
							curr += len;
							// NT Hash
							len = 16;
							memcpy( &(authBuff->fBufferData[ curr ]), &len, 4 );
							curr += sizeof( long );
							memcpy( &(authBuff->fBufferData[ curr ]), nt_hash, len );
							curr += len;

							authBuff->fBufferLength = curr;
							status = dsDoDirNodeAuthOnRecordType( nodeRef, authType, 1, authBuff, stepBuff, NULL,  recordType);
							if ( status == eDSNoErr )
							{
									//DEBUG(1,("kDSStdAuthSetWorkstationPasswd was successful for  \"%s\" :)\n", machine_acct));
							}
							else
							{
									// DEBUG(1,("kDSStdAuthSetWorkstationPasswd FAILED for user \"%s\" (%d) :(\n", userid, status) );
							}
					}
					bufferStatus = dsDataBufferDeAllocate( dirRef, stepBuff );
					if ( bufferStatus != eDSNoErr )
					{
							// DEBUG(1,("kDSStdAuthSetWorkstationPasswd: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", bufferStatus) );
					}
			}
			else
			{
					// DEBUG(1,("kDSStdAuthSetWorkstationPasswd: *** dsDataBufferAllocate(2) faild with \n" ));
			}
			bufferStatus = dsDataBufferDeAllocate( dirRef, authBuff );
			if ( bufferStatus != eDSNoErr )
			{
					// DEBUG(1,( "kDSStdAuthSetWorkstationPasswd: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", status ));
			}
	}
	else
	{
			// DEBUG(1,("kDSStdAuthSetWorkstationPasswd: *** dsDataBufferAllocate(1) faild with \n" ));
	}
cleanup:
    if (authType != NULL)
		dsDataNodeDeAllocate(dirRef, authType);
    if (recordType != NULL)
		dsDataNodeDeAllocate(dirRef, recordType);
	if (dirRef)
		dsCloseDirService(dirRef);
	if (nodeRef)
		dsCloseDirNode(nodeRef);
	return status;
}

tDirStatus opendirectory_lmchap2changepasswd(char *account_name, char *passwordData, char *passwordHash, u_int8_t passwordFormat, char *slot_id)
{
    tDirReference	dirRef = NULL;
	tDirStatus 		status			= eDSNoErr;
	tDirStatus 		bufferStatus	= eDSNoErr;
	unsigned long	bufferSize		= 1024 * 10;
	unsigned long		len			= 0;
	tDataBufferPtr		authBuff  		= NULL;
	tDataBufferPtr		stepBuff  		= NULL;
	tDataNodePtr		authType		= NULL;
	tDataNodePtr		recordType		= NULL;
	tDirNodeReference nodeRef = NULL;
	char *targetaccount = NULL;
	char recordName[255] = {0};
		
    status = dsOpenDirService(&dirRef);
	
	if (status != eDSNoErr)
		return status;
	
	status = get_node_ref_and_name(dirRef, account_name, kDSStdRecordTypeUsers, &nodeRef, recordName);

	if (status != eDSNoErr)
		goto cleanup;

	status = opendirectory_authenticate_node(dirRef, nodeRef);

	if (status != eDSNoErr)
		goto cleanup;

	if (slot_id && strlen(slot_id))
		targetaccount = slot_id;
	else
		targetaccount = recordName;

	authBuff = dsDataBufferAllocate( dirRef, bufferSize );
	if ( authBuff != NULL )
	{
			authBuff->fBufferLength = 0;
			stepBuff = dsDataBufferAllocate( dirRef, bufferSize );
			if ( stepBuff != NULL )
			{
					authType = dsDataNodeAllocateString( dirRef,  "dsAuthMethodStandard:dsAuthMSLMCHAP2ChangePasswd" /*kDSStdAuthMSLMCHAP2ChangePasswd*/);
					recordType = dsDataNodeAllocateString( dirRef,  kDSStdRecordTypeUsers);
					if ( authType != NULL )
					{
							// Target account
							printf("account_name(%s)\n",targetaccount);
							opendirectory_add_data_buffer_item(authBuff, strlen( targetaccount ), targetaccount);
							opendirectory_add_data_buffer_item(authBuff, 1, &passwordFormat);
							opendirectory_add_data_buffer_item(authBuff, 516, passwordData);
							
							status = dsDoDirNodeAuthOnRecordType( nodeRef, authType, 1, authBuff, stepBuff, NULL,  recordType);
							if ( status == eDSNoErr )
							{
									//DEBUG(1,("kDSStdAuthMSLMCHAP2ChangePasswd was successful for  \"%s\" :)\n", machine_acct));

							}
							else
							{
									// DEBUG(1,("kDSStdAuthMSLMCHAP2ChangePasswd FAILED for user \"%s\" (%d) :(\n", userid, status) );
							}
					}
					bufferStatus = dsDataBufferDeAllocate( dirRef, stepBuff );
					if ( bufferStatus != eDSNoErr )
					{
							// DEBUG(1,("kDSStdAuthMSLMCHAP2ChangePasswd: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", bufferStatus) );
					}
			}
			else
			{
					// DEBUG(1,("kDSStdAuthMSLMCHAP2ChangePasswd: *** dsDataBufferAllocate(2) faild with \n" ));
			}
			bufferStatus = dsDataBufferDeAllocate( dirRef, authBuff );
			if ( bufferStatus != eDSNoErr )
			{
					// DEBUG(1,( "kDSStdAuthMSLMCHAP2ChangePasswd: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", status ));
			}
	}
	else
	{
			// DEBUG(1,("kDSStdAuthMSLMCHAP2ChangePasswd: *** dsDataBufferAllocate(1) faild with \n" ));
	}
cleanup:
    if (authType != NULL)
		dsDataNodeDeAllocate(dirRef, authType);
    if (recordType != NULL)
		dsDataNodeDeAllocate(dirRef, recordType);
	if (dirRef)
		dsCloseDirService(dirRef);
	if (nodeRef)
		dsCloseDirNode(nodeRef);
	return status;
}

tDirStatus opendirectory_authenticate_node(tDirReference	dirRef, tDirNodeReference nodeRef)
{
	tDirStatus 		status			= eDSNoErr;
	tDirStatus 		bufferStatus	= eDSNoErr;
	unsigned long	bufferSize		= 1024 * 10;
	tDataBufferPtr		authBuff  		= NULL;
	tDataBufferPtr		stepBuff  		= NULL;
	tDataNodePtr		authType		= NULL;
	tDataNodePtr		recordType		= NULL;
	void				*authenticator = NULL;
	
	authenticator = get_opendirectory_authenticator();
	
	if (authenticator == NULL || 
		get_opendirectory_authenticator_accountlen(authenticator) == 0 || 
		get_opendirectory_authenticator_secretlen(authenticator) == 0) {
		return eDSNullParameter;
	}
		
	authBuff = dsDataBufferAllocate( dirRef, bufferSize );
	if ( authBuff != NULL )
	{
			authBuff->fBufferLength = 0;
			stepBuff = dsDataBufferAllocate( dirRef, bufferSize );
			if ( stepBuff != NULL )
			{
					authType = dsDataNodeAllocateString( dirRef,  kDSStdAuthNodeNativeClearTextOK);
					recordType = dsDataNodeAllocateString( dirRef,  kDSStdRecordTypeUsers);
					if ( authType != NULL )
					{
							// Account Name (authenticator)
							opendirectory_add_data_buffer_item(authBuff, get_opendirectory_authenticator_accountlen(authenticator), get_opendirectory_authenticator_account(authenticator));
							// Password (authenticator password)
							opendirectory_add_data_buffer_item(authBuff, get_opendirectory_authenticator_secretlen(authenticator), get_opendirectory_authenticator_secret(authenticator));

							status = dsDoDirNodeAuthOnRecordType( nodeRef, authType, 0, authBuff, stepBuff, NULL,  recordType);
							if ( status == eDSNoErr )
							{
									printf("kDSStdAuthNodeNativeClearTextOK was successful\n");
									//DEBUG(1,("kDSStdAuthNodeNativeClearTextOK was successful for  \"%s\" :)\n", machine_acct));
							}
							else
							{
									printf("kDSStdAuthNodeNativeClearTextOK was FAILED (%d)\n", status);
									// DEBUG(1,("kDSStdAuthNodeNativeClearTextOK FAILED for user \"%s\" (%d) :(\n", userid, status) );
							}
					}
					bufferStatus = dsDataBufferDeAllocate( dirRef, stepBuff );
					if ( bufferStatus != eDSNoErr )
					{
							// DEBUG(1,("kDSStdAuthNodeNativeClearTextOK: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", bufferStatus) );
					}
			}
			else
			{
					// DEBUG(1,("kDSStdAuthNodeNativeClearTextOK: *** dsDataBufferAllocate(2) faild with \n" ));
			}
			bufferStatus = dsDataBufferDeAllocate( dirRef, authBuff );
			if ( bufferStatus != eDSNoErr )
			{
					// DEBUG(1,( "kDSStdAuthNodeNativeClearTextOK: *** dsDataBufferDeAllocate(2) faild with error = %d: \n", status ));
			}
	}
	else
	{
			// DEBUG(1,("kDSStdAuthNodeNativeClearTextOK: *** dsDataBufferAllocate(1) faild with \n" ));
	}
	
    if (authType != NULL)
		dsDataNodeDeAllocate(dirRef, authType);
    if (recordType != NULL)
		dsDataNodeDeAllocate(dirRef, recordType);
	delete_opendirectory_authenticator(authenticator);
	return status;
}
