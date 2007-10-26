#include <stdio.h>
#include <unistd.h>
#include <CoreFoundation/CFString.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dns.h>
#include <dns_util.h>
#include <sysexits.h>

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesConst.h>
#include <DirectoryService/DirServicesTypes.h>
#include <DirectoryService/DirServicesUtils.h>

#include "Common.h"

#include <PasswordServer/ReplicaFile.h>

#define kPWConfigDefaultRecordName		"passwordserver"
#define kPWConfigRecordPrefix			"passwordserver_"

extern "C" {
char* ReplaceKerberosAddrs( const char *inValue, const char *inOldIP, const char *inNewIP, 
					const char *inOldHostName, const char *inNewHostName );
					
int	UpdateKerberosPrincipals(const char* node, const char *inNewHostName);

FILE *gLogFileDesc = NULL;
};


tDirStatus DoNodeAuth(  const tDirReference inDirRef, const tDirNodeReference inDirNodeRef,
	const char* inUserName, const char* inPass )
{
	tDirStatus			dirStatus = eDSNoErr;
	tDataBufferPtr		authRequestBuffer = NULL;
	tDataBufferPtr		authResponseBuffer = NULL;
	tContextData		continueData = NULL;
	tDataNodePtr		authType = NULL;
	long				authRequestBufferSize = 0;
	long				curOffset = 0;
	long				userNameLength = 0;
	long				passwordLength = 0;

	authType = dsDataNodeAllocateString( inDirRef, kDSStdAuthNodeNativeClearTextOK );
	if( authType == NULL )
		return eMemoryAllocError;

	userNameLength = strlen( inUserName );
	passwordLength = strlen( inPass );
	
	authResponseBuffer = dsDataBufferAllocate( inDirRef, 512 ); // Enough for the response.
	if( authResponseBuffer == NULL )
	{
		dsDataNodeDeAllocate( inDirRef, authType );
		return eMemoryAllocError;
	}
	
	//how big should the request buffer be?
	authRequestBufferSize += sizeof( userNameLength ) + userNameLength;
	authRequestBufferSize += sizeof( passwordLength ) + passwordLength;

	//allocate the request buffer
	authRequestBuffer = dsDataBufferAllocate( inDirRef, authRequestBufferSize );
	if( authRequestBuffer == NULL )
	{
		dsDataBufferDeAllocate( inDirRef, authResponseBuffer );
		dsDataNodeDeAllocate( inDirRef, authType );
		return eMemoryAllocError;
	}

	//populate the request buffer
	//user name first
	memcpy( &(authRequestBuffer->fBufferData[curOffset]), &userNameLength, sizeof( userNameLength ) );
	curOffset += sizeof( userNameLength );
	memcpy( &(authRequestBuffer->fBufferData[curOffset]), inUserName, userNameLength );
	curOffset += userNameLength;

	//now the password
	memcpy( &(authRequestBuffer->fBufferData[curOffset]), &passwordLength, sizeof( passwordLength ) );
	curOffset += sizeof( passwordLength );
	memcpy( &(authRequestBuffer->fBufferData[curOffset]), inPass, passwordLength );
	curOffset += passwordLength;

	authRequestBuffer->fBufferLength = curOffset;

	dirStatus = dsDoDirNodeAuth( inDirNodeRef, authType, FALSE, authRequestBuffer, authResponseBuffer, &continueData );
	
	dsDataBufferDeAllocate( inDirRef, authRequestBuffer );
	dsDataBufferDeAllocate( inDirRef, authResponseBuffer );
	dsDataNodeDeAllocate( inDirRef, authType );
	return dirStatus;
}

tDirStatus OpenRecordFromEntry( const tDirReference inDirRef, const tDirNodeReference inDirNodeRef, 
								tRecordEntry* recEntry, tRecordReference* outRecordRef )
{
	tDirStatus dirStatus = eDSNoErr;
	char* recNameStr = NULL;
	char* recTypeStr = NULL;
	tDataNodePtr recName = NULL;
	tDataNodePtr recType = NULL;

	dirStatus = dsGetRecordNameFromEntry( recEntry, &recNameStr );
	if (dirStatus == eDSNoErr)
		dirStatus = dsGetRecordTypeFromEntry( recEntry, &recTypeStr );
	
	if (dirStatus == eDSNoErr)
		recName = dsDataNodeAllocateString( inDirRef, recNameStr );
	if ( recName != NULL )
	{
		recType = dsDataNodeAllocateString( inDirRef, recTypeStr );
		if ( recType != NULL )
		{
			dirStatus = dsOpenRecord( inDirNodeRef, recType, recName, outRecordRef );
			dsDataNodeDeAllocate( inDirRef, recType );
		}
		dsDataNodeDeAllocate( inDirRef, recName );
	}
	//printf("Opening record %s returned %d\n", recNameStr, dirStatus);
	if (recNameStr != NULL)
		free(recNameStr);
	if (recTypeStr != NULL)
		free(recTypeStr);

	return dirStatus;
}

bool IsPartOfIP(char c)
{
	return (isdigit(c) || (c == '.'));
}

bool IsPartOfHostName(char c)
{
	return (isdigit(c) || isalpha(c) || (c == '-'));
}

char* ReplaceAddrs( const char *inValue, const char *inOldIP, const char *inNewIP, 
					const char *inOldHostName, const char *inNewHostName )
{
	char *inValueCopy = NULL;
	char* returnValue = NULL;
	char* ipStr = NULL;
	char* hostStr = NULL;
	const char* tptr = NULL;
	int newHostLen = 0;
	int newFileLen = 0;
	int index = 0;
	int ipCount = 0;
	int hostCount = 0;
	
	// get count of inOldIP
	for (tptr = inValue; tptr != NULL;)
	{
		tptr = strstr(tptr, inOldIP);
		if (tptr != NULL) {
			tptr++;
			ipCount++;
		}
	}
	
	// get count of inOldHostName
	if (inOldHostName != NULL)
	{
		for (tptr = inValue; tptr != NULL;)
		{
			tptr = strstr(tptr, inOldHostName);
			if (tptr != NULL) {
				tptr++;
				hostCount++;
			}
		}
	}
	
	if (ipCount > 0 || hostCount > 0)
	{
		// calculate new size
		if (inNewHostName != NULL)
			newHostLen = strlen(inNewHostName);
	
		newFileLen = strlen(inValue) + strlen(inNewIP) * ipCount + newHostLen * hostCount;
		
		inValueCopy = (char *) malloc(newFileLen);
		if (inValueCopy == NULL)
			return NULL;
		
		returnValue = (char *) malloc(newFileLen);
		if (returnValue == NULL)
			return NULL;
		
		strcpy(inValueCopy, inValue);
		
		// replace instances of the IP address
		for (index = 0; index < ipCount; index++)
		{
			ipStr = strstr(inValueCopy, inOldIP);
	
			// check that found strings are not substrings
			if (ipStr != NULL)
			{
				if ((ipStr != inValueCopy) && IsPartOfIP(*(ipStr - 1)))
					ipStr = NULL;
				else if (IsPartOfIP(*(ipStr + strlen(inOldIP))))
					ipStr = NULL;
			}
	
			if (ipStr != NULL)
			{
				strcpy(returnValue, inValueCopy);
		
				ipStr = strstr(returnValue, inOldIP);
				if (ipStr != NULL)
				{
					char* ipEnd = ipStr + strlen(inOldIP);
					memmove(ipEnd + strlen(inNewIP) - strlen(inOldIP), ipEnd, strlen(ipEnd) + 1);
					memcpy(ipStr, inNewIP, strlen(inNewIP));
				}
			}
			
			strcpy(inValueCopy, returnValue);
		}
		
		// replace instances of the host name
		for (index = 0; index < hostCount; index++)
		{
			hostStr = strstr(inValueCopy, inOldHostName);
		
			// check that found strings are not substrings		
			if (hostStr != NULL)
			{
				if ((hostStr != inValueCopy) && IsPartOfHostName(*(hostStr - 1)))
					hostStr = NULL;
				else if (IsPartOfHostName(*(hostStr + strlen(inOldHostName))))
					hostStr = NULL;
			}
		
			if (hostStr != NULL)
			{
				strcpy(returnValue, inValueCopy);
					
				hostStr = strstr(returnValue, inOldHostName);
				if (hostStr != NULL)
				{
					char* hostEnd = hostStr + strlen(inOldHostName);
					memmove(hostEnd + strlen(inNewHostName) - strlen(inOldHostName), hostEnd, strlen(hostEnd) + 1);
					memcpy(hostStr, inNewHostName, strlen(inNewHostName));
				}
			}
			
			strcpy(inValueCopy, returnValue);
		}
	}
	
	if (inValueCopy != NULL)
		free(inValueCopy);
	
	//if (returnValue != NULL)
	//	printf("Swtiched %s to %s\n", inValue, returnValue);
	return returnValue;
}

void ConvertRecordAttributes( const tDirReference inDirRef, tDirNodeReference inDirNodeRef,
					const char *inOldIP, const char *inNewIP, 
					const char *inOldHostName, const char *inNewHostName,
					char* inRecordType, char* in1stAttributeName, ...)
{
    tDataBuffer				   *tDataBuff						= NULL;
    long						status							= eDSNoErr;
    tContextData				context							= nil;
	tDataList					recordTypeList;
	tDataList					recordNameList;
	tDataList					attributeList;
	unsigned long				j								= 0;
	unsigned long				k								= 0;
	unsigned long				recIndex						= 0;
	unsigned long				recCount						= 0;
	tRecordEntry		  		*recEntry						= nil;
	tAttributeListRef			attrListRef						= 0;
	tAttributeValueListRef		valueRef						= 0;
	tAttributeEntry		   		*pAttrEntry						= nil;
	tAttributeValueEntry   		*pValueEntry					= nil;
	tAttributeValueEntry   		*pNewPWAttrValue				= nil;
	tRecordReference			recRef							= 0;
	unsigned long				buffSize						= 4096;
	
	va_list args;
	va_start( args, in1stAttributeName );
	
	memset(&attributeList, 0, sizeof(attributeList));
	memset(&recordTypeList, 0, sizeof(recordTypeList));
	memset(&recordNameList, 0, sizeof(recordNameList));

    do
    {
		tDataBuff = dsDataBufferAllocate( inDirRef, buffSize );
		if ( tDataBuff == NULL )
			break;
		
		status = dsBuildListFromStringsAllocV( inDirRef, &attributeList, in1stAttributeName, args );
		if( status != eDSNoErr )
			break;
		status = dsBuildListFromStringsAlloc ( inDirRef, &recordTypeList, inRecordType, NULL );
		if( status != eDSNoErr )
			break;
		status = dsBuildListFromStringsAlloc ( inDirRef, &recordNameList, kDSRecordsAll, NULL );
		if( status != eDSNoErr )
			break;

		do
		{
			do
			{
				status = dsGetRecordList( inDirNodeRef, tDataBuff, &recordNameList, eDSExact,
											&recordTypeList, &attributeList, false,
											&recCount, &context );
				
				if ( status == eDSBufferTooSmall )
				{
					buffSize *= 2;
					
					// a safety for a runaway condition
					if ( buffSize > 1024 * 1024 )
						break;
					
					dsDataBufferDeAllocate( inDirRef, tDataBuff );
					tDataBuff = dsDataBufferAllocate( inDirRef, buffSize );
					if ( tDataBuff == NULL ) {
						status = eMemoryError;
						break;
					}
				}
			}
			while ( status == eDSBufferTooSmall );
			
			if (status != eDSNoErr) break;
			
			for ( recIndex = 1; recIndex <= recCount; recIndex++ )
			{
				status = dsGetRecordEntry( inDirNodeRef, tDataBuff, recIndex, &attrListRef, &recEntry );
				if ( status != eDSNoErr && recEntry == NULL )
					continue;
				
				for ( j = 1; (j <= recEntry->fRecordAttributeCount) && (status == eDSNoErr); j++ )
				{
					status = dsGetAttributeEntry( inDirNodeRef, tDataBuff, attrListRef, j, &valueRef, &pAttrEntry );
					if ( status == eDSNoErr && pAttrEntry != NULL )
					{
						for ( k = 1; (k <= pAttrEntry->fAttributeValueCount) && (status == eDSNoErr); k++ )
						{
							status = dsGetAttributeValue( inDirNodeRef, tDataBuff, k, valueRef, &pValueEntry );
							if ( status == eDSNoErr && pValueEntry != NULL )
							{
								char* valueStr = (char*)(pValueEntry->fAttributeValueData.fBufferData);
								char* newValue;
								if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrXMLPlist ) == 0 )
									newValue = ReplaceKerberosAddrs(valueStr, inOldIP, inNewIP, inOldHostName, inNewHostName);
								else if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrAuthenticationAuthority ) == 0 )
									newValue = ReplaceAddrs(valueStr, inOldIP, inNewIP, nil, nil); // don't convert hostnames, only IP addresses
								else
									newValue = ReplaceAddrs(valueStr, inOldIP, inNewIP, inOldHostName, inNewHostName);
								if (newValue != NULL)
								{
									OpenRecordFromEntry(inDirRef, inDirNodeRef, recEntry, &recRef);
									if (recRef != 0)
										pNewPWAttrValue = dsAllocAttributeValueEntry(inDirRef, pValueEntry->fAttributeValueID, newValue, strlen(newValue));
									if ( pNewPWAttrValue != nil )
									{
										status = dsSetAttributeValue( recRef, &pAttrEntry->fAttributeSignature, pNewPWAttrValue );
										//printf("dsSetAttributeValue returned %ld\n", status);
										dsDeallocAttributeValueEntry( inDirRef, pNewPWAttrValue );
										pNewPWAttrValue = nil;
									}
									free(newValue);
								}
								dsDeallocAttributeValueEntry( inDirRef, pValueEntry );
							}
						}
						dsDeallocAttributeEntry( inDirRef, pAttrEntry );
						dsCloseAttributeValueList(valueRef);
					}
				}
				dsDeallocRecordEntry( inDirRef, recEntry );
				dsCloseAttributeList(attrListRef);
			}
		}
		while ( status == eDSNoErr && context != NULL );
    }
    while(false);

	dsDataListDeallocate( inDirRef, &attributeList );
	dsDataListDeallocate( inDirRef, &recordTypeList );
	dsDataListDeallocate( inDirRef, &recordNameList );
	if (tDataBuff != nil)
		dsDataBufferDeAllocate(inDirRef, tDataBuff);
}

void UpdatePSReplicas(const char *inOldIP, const char* inNewIP)
{
	CFMutableDictionaryRef replicaDict = NULL;
	ReplicaFile *replicaFile = [ReplicaFile new];
	char myAddresses[256] = {0};
	CFStringRef replicaNameString = NULL;
	char newIP[256];
	
	strlcpy(newIP,inNewIP, sizeof(newIP));

	replicaNameString = [replicaFile getNameFromIPAddress:inOldIP];
	replicaDict = [replicaFile getReplicaByName:replicaNameString];
	
	GetPWServerAddresses( myAddresses );
	if ( strstr( myAddresses, inOldIP ) != NULL )
		SetPWServerAddress( NULL, NULL, newIP, false, NULL, NULL );
	
	if ( replicaDict != NULL )
	{
		[replicaFile addIPAddress:inNewIP orReplaceIP:inOldIP inReplica:replicaDict];
		[replicaFile saveXMLData];
		CFRelease( replicaDict );
	}
	
	UpdateReplicaList( inOldIP, inNewIP );
}

#define SLAPPATH "/usr/sbin/slapconfig"

void CallSlapConfig( const char *inOldIP, const char *inNewIP, 
					const char *inOldHostName, const char *inNewHostName )
{
	pid_t pid;
	int status;
	struct stat sb;
	const char* argv[7];

	status = stat( SLAPPATH, &sb );
	if ( status != 0 )
		return;
	
	argv[0] = "slapconfig";
	argv[1] = "-changeip";
	argv[2] = inOldIP;
	argv[3] = inNewIP;
	argv[4] = inOldHostName;
	argv[5] = inNewHostName;
	argv[6] = nil;
	
	pid = fork();
	if (pid == -1) return;

	/* Handle the child */
	if (pid == 0) {
		int result = -1;
	
		result = execv(SLAPPATH, (char* const*)argv);
		if (result == -1) {
			_exit(1);
		}
		
		/* This should never be reached */
		_exit(1);
	}
	
	waitpid(pid, &status, 0);

	return;
}

void ConvertRecords( const char *inDirPath, const char *inOldIP, const char *inNewIP, 
					const char *inOldHostName, const char *inNewHostName )
{
	tDirReference				dsRef							= 0;
    tDirNodeReference			nodeRef							= 0;
    long						status							= eDSNoErr;
	tDataList					*nodeName						= nil;
	bool						isLocalNode						= false;
	bool						isLDAPINode						= false;

	isLocalNode = (strncmp(inDirPath, "/Local", sizeof("/Local") - 1) == 0);
	isLDAPINode = (strstr(inDirPath, "ldapi") != NULL);
	
	status = dsOpenDirService( &dsRef );
	if ( status != eDSNoErr ) {
		fprintf( stderr, "ERROR: could not open Directory Services (error = %ld)\n", status );
		exit( EX_UNAVAILABLE );
	}
	
	if ( isLDAPINode )
		inDirPath = "/LDAPv3/ldapi://%2Fvar%2Frun%2Fldapi";
	
	nodeName = dsBuildFromPath( dsRef, inDirPath, "/" );
	status = dsOpenDirNode( dsRef, nodeName, &nodeRef );
	dsDataListDeallocate( dsRef, nodeName );
	if ( status != eDSNoErr ) {
		fprintf(stderr, "Error %ld opening node %s\n", status, inDirPath);
		exit( EX_UNAVAILABLE );
	}
	
	// authenticate if needed
	if ( !isLocalNode && !isLDAPINode )
	{
		int tries = 0;
		do
		{
			char userName[256];
			char* password;
			printf("Enter admin name for node %s: ", inDirPath);
			fgets(userName, 255, stdin);
			if (strlen(userName) > 0 && userName[strlen(userName)-1] == '\n')
				userName[strlen(userName)-1] = '\0';
			password = getpass("Password:");
			status = DoNodeAuth(dsRef, nodeRef, userName, password);
			if (status != eDSNoErr)
			{
				fprintf(stderr, "Error %ld authenticating to node %s\n", status, inDirPath);
				if (++tries == 3)
				{
					fprintf(stderr, "Too many tries, exiting\n");
					exit(1);
				}
			}
		}
		while (status != eDSNoErr);
	}
	
	// First do User Records
	ConvertRecordAttributes(dsRef, nodeRef, inOldIP, inNewIP, inOldHostName, inNewHostName, kDSStdRecordTypeUsers,
							kDSNAttrAuthenticationAuthority, kDS1AttrNFSHomeDirectory, kDSNAttrHomeDirectory,
                                                        kDSNAttrRecordName, nil);
	// Then machine records
	if (!isLocalNode)
		ConvertRecordAttributes(dsRef, nodeRef, inOldIP, inNewIP, inOldHostName, 
								inNewHostName, kDSStdRecordTypeMachines,
								kDSNAttrIPAddress, kDSNAttrRecordName, kDSNAttrDNSName, nil);
	else
		ConvertRecordAttributes(dsRef, nodeRef, inOldIP, inNewIP, inOldHostName, 
								inNewHostName, kDSStdRecordTypeMachines,
								kDSNAttrIPAddress, kDSNAttrRecordName, kDSNAttrDNSName,
								kDSNativeAttrTypePrefix "serves", nil);
	// need serves attribute too for hostname changes
	
	// Then computer records
	ConvertRecordAttributes(dsRef, nodeRef, inOldIP, inNewIP, inOldHostName, inNewHostName, kDSStdRecordTypeComputers,
							kDSNAttrAuthenticationAuthority,kDSNAttrIPAddress, kDSNAttrRecordName, nil);
							
	// Then mount records
	ConvertRecordAttributes(dsRef, nodeRef, inOldIP, inNewIP, inOldHostName, inNewHostName, kDSStdRecordTypeMounts,
							kDSNAttrVFSOpts, kDSNAttrRecordName, nil);
							
	// Then config records
	ConvertRecordAttributes(dsRef, nodeRef, inOldIP, inNewIP, inOldHostName, inNewHostName, kDSStdRecordTypeConfig,
							kDSNAttrLDAPReadReplicas, kDSNAttrLDAPWriteReplicas, kDS1AttrPasswordServerLocation,
							kDS1AttrPasswordServerList, kDS1AttrXMLPlist, kDSNAttrRecordName, nil);
							
	// update master property in top level
	if (isLocalNode) 
	{
		tRecordReference recRef = 0;
		tDataNodePtr recName = dsDataNodeAllocateString( dsRef, "/" );
		if ( recName != NULL )
		{
			tDataNodePtr recType = dsDataNodeAllocateString( dsRef, kDSNativeRecordTypePrefix "/" );
			if ( recType != NULL )
			{
				status = dsOpenRecord( nodeRef, recType, recName, &recRef );
				dsDataNodeDeAllocate( dsRef, recType );
			}
			dsDataNodeDeAllocate( dsRef, recName );
		}
		if (status == eDSNoErr)
		{
			tAttributeValueEntryPtr masterAttr = nil;
			tAttributeValueEntryPtr newValue = nil;
			char* newValueStr = nil;
			tDataNodePtr attrType = dsDataNodeAllocateString(dsRef, kDSNativeAttrTypePrefix "master");
			status = dsGetRecordAttributeValueByIndex(recRef,attrType,1,&masterAttr);
			if (status == eDSNoErr) 
			{
				if ((strncmp(masterAttr->fAttributeValueData.fBufferData,inOldIP,strlen(inOldIP)) == 0)
					&& (masterAttr->fAttributeValueData.fBufferData[strlen(inOldIP)] == '/'))
				{
					// found a matching IP
					newValueStr = (char*)calloc(strlen(masterAttr->fAttributeValueData.fBufferData)+strlen(inNewIP)+1,1);
					strcpy(newValueStr,inNewIP);
					strcat(newValueStr,masterAttr->fAttributeValueData.fBufferData + strlen(inOldIP));
					newValue = dsAllocAttributeValueEntry(dsRef,masterAttr->fAttributeValueID,
								newValueStr,strlen(newValueStr));
					status = dsSetAttributeValue(recRef,attrType,newValue);
				}
				else if (inOldHostName != nil
					&& (strncmp(masterAttr->fAttributeValueData.fBufferData,inOldHostName,strlen(inOldHostName)) == 0)
					&& (masterAttr->fAttributeValueData.fBufferData[strlen(inOldHostName)] == '/'))
				{
					// found a matching hostname
					newValueStr = (char*)calloc(strlen(masterAttr->fAttributeValueData.fBufferData)+strlen(inNewHostName)+1,1);
					strcpy(newValueStr,inNewHostName);
					strcat(newValueStr,masterAttr->fAttributeValueData.fBufferData + strlen(inOldHostName));
					newValue = dsAllocAttributeValueEntry(dsRef,masterAttr->fAttributeValueID,
								newValueStr,strlen(newValueStr));
					status = dsSetAttributeValue(recRef,attrType,newValue);
				}
			}
		}
	}
	
	dsCloseDirNode(nodeRef);
}

void UpdateIPAddrAndHostNameInFile( const char* filePath, const char *inOldIP,
	const char *inNewIP,  const char *inOldHostName, const char *inNewHostName)
{
	char* hostConfigBuffer;
	char* newValue;
	int fd;
	struct stat sb;
	int result;
	
	result = stat(filePath, &sb);
	if (result != 0)
		return;
		
	hostConfigBuffer = (char*)malloc(sb.st_size + 1);
	fd = open(filePath, O_RDWR, 0);
	if (fd == -1)
		return;
	result = read(fd, hostConfigBuffer, sb.st_size);
	hostConfigBuffer[result] = 0;
	newValue = ReplaceAddrs(hostConfigBuffer, inOldIP, inNewIP, inOldHostName, inNewHostName);
	if (newValue != NULL)
	{
		lseek(fd, 0, SEEK_SET);
		ftruncate(fd, 0);
		result = write(fd, newValue, strlen(newValue));
		free(newValue);
	}
	free(hostConfigBuffer);
	close(fd);
}
						
void UpdateHostConfig(const char *inOldIP, const char *inNewIP, 
						const char *inOldHostName, const char *inNewHostName)
{
	UpdateIPAddrAndHostNameInFile( "/etc/hostconfig", inOldIP, inNewIP, inOldHostName, inNewHostName );
}

void UpdateSMBConf(const char *inOldIP, const char *inNewIP, 
						const char *inOldHostName, const char *inNewHostName)
{
     char oldHostName[MAXHOSTNAMELEN]    = { 0, };
     char newHostName[MAXHOSTNAMELEN]    = { 0, };
 
     // smb uses unqualified name, so let's unqualify the name just in case
     if( NULL != inOldHostName && NULL != inNewHostName )
     {
         strcpy( oldHostName, inOldHostName );
         strcpy( newHostName, inNewHostName );
 
         char *pTemp = strchr( oldHostName, '.' );
         if( NULL != pTemp )
             *pTemp = '\0';
 
         pTemp = strchr( newHostName, '.' );
         if( NULL != pTemp )
             *pTemp = '\0';
 
         inOldHostName = oldHostName;
         inNewHostName = newHostName;
     }
 

	UpdateIPAddrAndHostNameInFile( "/etc/smb.conf", inOldIP, inNewIP, inOldHostName, inNewHostName );
}

void Usage()
{
	fprintf(stderr, "Usage: changeip_ds <dir node path> <oldIP> <newIP> [<oldHost> <newHost>]\n");
	fprintf(stderr, "\teg: changeip_ds /LDAPv3/127.0.0.1 11.0.1.10 11.0.1.12 server10 server12\n\n");
	fprintf(stderr, "Use - for node path to update the local node only, \n\teg. changeip_ds - 11.0.1.10 11.0.1.12\n");
}

char *getReverseIPString( const char *inAddress )
{
    unsigned char   addr[INET_ADDRSTRLEN]   = { 0, };
    Boolean         bIPv4                   = true;
    char            *pReturnString          = NULL;

    if( inet_net_pton(AF_INET, inAddress, &addr, INET_ADDRSTRLEN) <= 0 )
    {
        bIPv4 = false;
        if( inet_net_pton(AF_INET6, inAddress, &addr, INET_ADDRSTRLEN) <= 0 )
        {
            return nil;
        }
    }

    if( bIPv4 )
    {
        pReturnString = (char *) calloc( 64, sizeof(char) );
        snprintf( pReturnString, 32, "%u.%u.%u.%u.in-addr.arpa.", addr[3], addr[2], addr[1], addr[0] );
    }
    else
    {
        int                     s       = 0;
        char                    ptr_name[128];
        int                     x       = sizeof(ptr_name);
        int                     n;
        int                     i;

        /*
         * build IPv6 "nibble" PTR query name (RFC 1886, RFC 3152)
         *   N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.ip6.arpa.
         */
        for (i = sizeof(addr) - 1; i >= 0; i--)
        {
            n = snprintf( &ptr_name[s], x, "%x.%x.",
                          ( addr[i]       & 0xf),
                          ((addr[i] >> 4) & 0xf));
            if ((n == -1) || (n >= x))
            {
                goto done;
            }

            s += n;
            x -= n;
        }

        n = snprintf(&ptr_name[s], x, "ip6.arpa.");
        if (n != -1 && n < x)
        {
            goto done;
        }
        pReturnString = strdup( ptr_name );
    }

done:
    return pReturnString;
}

char *_GetCStringFromCFString( CFStringRef cfString )
{
    char    *pReturn    = NULL;

    if( NULL != cfString )
    {
        CFIndex iBufferSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfString), kCFStringEncodingUTF8) + 1;

        pReturn = (char *) malloc( iBufferSize );
        if( CFStringGetCString( cfString, pReturn, iBufferSize, kCFStringEncodingUTF8 ) == 0 )
        {
            free( pReturn );
            pReturn = NULL;
        }        
    }

    return pReturn;
}

char *lookupReverseInDNS( CFStringRef inAddress )
{           
    char        *pAddress       = _GetCStringFromCFString( inAddress );
    char        *reversePTR     = NULL;
    char        *reverseName    = NULL;

    reversePTR = getReverseIPString( pAddress );
            
    free( pAddress );
    pAddress = NULL;
        
    if( NULL != reversePTR )
    {   
        // let's try to lookup our own hostname via DNS
        dns_handle_t   dns = dns_open( NULL );
        
        if( NULL != dns )
        {
            uint16_t           type            = 0;
            uint16_t           classtype   = 0;
            dns_reply_t    *reply       = NULL;

            dns_type_number( "PTR", &type );
            dns_class_number( "IN", &classtype );
            
            reply = dns_lookup( dns, reversePTR, classtype, type );
            if( NULL != reply )
            {
                if ( (reply->header != NULL) && (reply->header->ancount > 0) )
                {
                    if( reply->header->ancount == 1 )
                    {
                        dns_domain_name_record_t *record = reply->answer[0]->data.PTR;

                        if( NULL != record && NULL != record->name )
                        {
                            if( strcmp(record->name, "localhost") != 0 )
                            {
                                reverseName = strdup( record->name );
                            }
                        }
                    }
                }
                dns_free_reply( reply );
                reply = NULL;
            }
            dns_free( dns );
            dns = NULL;
        }

        free( reversePTR );
        reversePTR = NULL;
    }

    return reverseName;
}

void checkHostName( void )
{
    SCDynamicStoreRef   cfStore                 = NULL;
    CFStringRef         cfKey                   = NULL;
    CFStringRef         cfGlobalKey             = NULL;
    CFDictionaryRef     cfGlobalIPv4            = NULL;
    CFStringRef         cfPrimaryService        = NULL;
    CFDictionaryRef     cfServiceIPv4           = NULL;
    CFArrayRef          cfPrimaryAddressList    = NULL;
    CFStringRef         cfPrimaryAddress        = NULL;

    // let's get the primary IP address according to SC
    cfStore = SCDynamicStoreCreate(NULL, CFSTR("changeip_ds"), NULL, NULL);
    if( NULL != cfStore )
    {
        cfGlobalKey = SCDynamicStoreKeyCreateNetworkGlobalEntity( kCFAllocatorDefault, kSCDynamicStoreDomainState,
                                                                  kSCEntNetIPv4 );
        if( NULL != cfGlobalKey )
        {                       
            cfGlobalIPv4 = (CFDictionaryRef) SCDynamicStoreCopyValue( cfStore, cfGlobalKey );
            if( NULL != cfGlobalIPv4 )
            {       
                cfPrimaryService = (CFStringRef) CFDictionaryGetValue( cfGlobalIPv4, kSCDynamicStorePropNetPrimaryService );
                if( NULL != cfPrimaryService )
                {
                    cfKey = SCDynamicStoreKeyCreateNetworkServiceEntity( kCFAllocatorDefault, kSCDynamicStoreDomainState,
                                                                         cfPrimaryService, kSCEntNetIPv4 );
                    if( NULL != cfKey )
                    {
                        cfServiceIPv4 = (CFDictionaryRef) SCDynamicStoreCopyValue( cfStore, cfKey );
                        if( NULL != cfServiceIPv4 )
                        {
                            cfPrimaryAddressList = (CFArrayRef) CFDictionaryGetValue( cfServiceIPv4, kSCPropNetIPv4Addresses );

                            if( NULL != cfPrimaryAddressList && CFArrayGetCount(cfPrimaryAddressList) != 0 )
                            {
                                cfPrimaryAddress = (CFStringRef) CFArrayGetValueAtIndex( cfPrimaryAddressList, 0 );
                                
                                if( NULL != cfPrimaryAddress )
                                {
                                    CFRetain( cfPrimaryAddress );
                                }
                            }
                            
                            CFRelease( cfServiceIPv4 );
                            cfServiceIPv4 = NULL; 
                        }
                        
                        CFRelease( cfKey );     
                        cfKey = NULL;
                    }   
                }
                 
                CFRelease( cfGlobalIPv4 );
                cfGlobalIPv4 = NULL;
            }
            
            CFRelease( cfGlobalKey );                             
            cfGlobalKey = NULL; 
        }
    }

    // only a need to continue if we have a valid IP address
    if (NULL != cfPrimaryAddress)
    {
        char    *reverseName                = lookupReverseInDNS( cfPrimaryAddress );
        char    *primaryAddress             = _GetCStringFromCFString( cfPrimaryAddress );
        char    oldHostName[MAXHOSTNAMELEN] = { 0, };

        (void) gethostname( oldHostName, MAXHOSTNAMELEN );

        printf( "\n" );
        printf( "Primary address     = %s\n\n", primaryAddress );
        printf( "Current HostName    = %s\n", oldHostName );

        if ( NULL != reverseName)
        {
            printf( "DNS HostName        = %s\n", reverseName );

            if( strcmp(reverseName, oldHostName) == 0 )
            {
                printf( "\nThe names match. There is nothing to change.\n" );
            }
            else
            {
				// test for /LDAPv3/127.0.0.1 node
				tDirReference			dsRef					= 0;
				tDirNodeReference		nodeRef					= 0;
				tDirStatus				status					= eDSNoErr;
				tDataList				*nodeName				= NULL;
				bool					hasLocalLDAPNode		= false;
				
				status = dsOpenDirService( &dsRef );
				if ( status == eDSNoErr )
				{
					nodeName = dsBuildListFromStrings( dsRef, "LDAPv3", "127.0.0.1", NULL );
					if ( nodeName != NULL )
					{
						status = dsOpenDirNode( dsRef, nodeName, &nodeRef );
						if ( status == eDSNoErr ) {
							hasLocalLDAPNode = true;
							dsCloseDirNode( nodeRef );
						}
						dsDataListDeallocate( dsRef, nodeName );
					}
					dsCloseDirService( dsRef );
				}
				
                printf( "\nTo fix the hostname please run /usr/sbin/changeip for your system with the\n" );
                printf( "appropriate directory with the following values\n\n" );
                printf( "   /usr/sbin/changeip <node> %s %s %s %s\n\n", primaryAddress, primaryAddress, oldHostName, reverseName );
                printf( "example:\n\n   /usr/sbin/changeip %s %s %s %s %s\n\n",
						hasLocalLDAPNode ? "/LDAPv3/127.0.0.1" : "-",
						primaryAddress, primaryAddress,
                        oldHostName, reverseName );
            }
        }
        else
        {
            printf( "\nThe DNS hostname is not available, please repair DNS and re-run this tool.\n\n" );
        }
        
        CFRelease( cfPrimaryAddress );
        cfPrimaryAddress = NULL;
    }
    else
    {
        fprintf( stderr, "Unable to determine Primary IP address.\n\n" );
    }   
}       

int main (int argc, const char * argv[])
{
	const char* oldHostName = nil;
	const char* newHostName = nil;
	
	if (getuid() != 0)
	{
		fprintf(stderr, "%s must be run as root\n", argv[0]);
		exit(1);
	}
	
    if (argc == 2 && strcasecmp(argv[1], "-checkhostname") == 0)
    {       
        checkHostName();
        exit(0);
    }       
                
    // this is used by the changeIP script to preflight check the IP address name, not exposed
    // in the usage
    if (argc == 3 && strcasecmp(argv[1], "-nameforaddress") == 0)
    {           
        CFStringRef cfAddress   = CFStringCreateWithCString( kCFAllocatorDefault, argv[2], kCFStringEncodingUTF8 );
        char        *pName      = lookupReverseInDNS( cfAddress );
                
        if( NULL != pName )
        {   
            printf( "%s\n", pName );
            free( pName );
            pName = NULL;
            exit( 0 );
        }
        exit( ENOENT );
    }

	if (argc != 4 && argc != 6)
	{
		Usage();
		exit(1);
	}
	
	if (argc == 6)
	{
		oldHostName =  argv[4];
		newHostName =  argv[5];
	}

	printf("Updating local node\n");
	ConvertRecords("/Local/Default", argv[2], argv[3], oldHostName, newHostName);
	
	if (strcmp(argv[1], "-") != 0)
	{
		printf("Updating node %s\n", argv[1]);
		ConvertRecords(argv[1], argv[2], argv[3], oldHostName, newHostName);
	}
	
	printf("Updating Password Server config\n");
	UpdatePSReplicas(argv[2], argv[3]);
	printf("Updating Open Directory config\n");
	CallSlapConfig(argv[2], argv[3], oldHostName, newHostName);
	printf("Updating hostconfig file\n");
	UpdateHostConfig(argv[2], argv[3], oldHostName, newHostName);
	printf("Updating smb.conf file\n");
	UpdateSMBConf(argv[2], argv[3], oldHostName, newHostName);
	printf("Updating Kerberos Service Principals and keytabs\n");
	UpdateKerberosPrincipals(argv[1], newHostName);
	printf("Finished updating Kerberos\n");

    return 0;
}
