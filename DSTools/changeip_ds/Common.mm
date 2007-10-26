
#include "Common.h"
#include <PasswordServer/ReplicaFile.h>
#include <Foundation/Foundation.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/sysctl.h>
#include <unistd.h>

//---------------------------------------------------------------------------------------------------------------
//	nest_log_time
//---------------------------------------------------------------------------------------------------------------

void nest_log_time( FILE *inFile )
{
	time_t now;
	struct tm nowStruct;
	char timeStr[256];
	
	if ( inFile == NULL )
		return;
	
	time(&now);
	localtime_r( &now, &nowStruct );
	strftime( timeStr, sizeof(timeStr),"%F %T ", &nowStruct );
	fprintf( inFile, "%s", timeStr );
}


//---------------------------------------------------------------------------------------------------------------
//	UpdateReplicaList
//---------------------------------------------------------------------------------------------------------------

void UpdateReplicaList(const char *inOldIP, const char* inNewIP)
{
	long						status				= eDSNoErr;
	tAttributeValueEntry	   *pAttrValueEntry		= NULL;
	tAttributeValueEntry	   *pNewAttrValue		= NULL;
	unsigned long				index				= 0;
	unsigned long				nodeCount			= 0;
	tRecordReference			recordRef			= 0;
	tDirReference				dsRef				= 0;
	tDataNode				   *attrTypeNode		= NULL;
	CFMutableDictionaryRef		replicaDict			= NULL;
	DSUtils						dsUtils;
	CFStringRef					replicaNameString	= NULL;
	NSString					*replicaListString	= nil;
	char						fullRecordName[256];
	NSString					*replicaIDString	= nil;
	
	status = dsUtils.GetLocallyHostedNodeList();
	if ( status != eDSNoErr )
		return;
	
	dsRef = dsUtils.GetDSRef();
	
	do
	{
		nodeCount = dsUtils.GetLocallyHostedNodeCount();
		for ( index = 1; index <= nodeCount; index++ )
		{
			status = dsUtils.OpenLocallyHostedNode( index );
			if ( status != eDSNoErr )
				continue;
			
			status = dsUtils.OpenRecord( kDSStdRecordTypeConfig, kPWConfigDefaultRecordName, &recordRef );
			if ( status != eDSNoErr )
				continue;
			
			attrTypeNode = dsDataNodeAllocateString( 0, kDS1AttrPasswordServerList );
			status = dsGetRecordAttributeValueByIndex( recordRef, attrTypeNode, 1, &pAttrValueEntry );
			if ( status != eDSNoErr )
				break;
			
			ReplicaFile *repList = [[ReplicaFile alloc] initWithXMLStr:(char *)&(pAttrValueEntry->fAttributeValueData.fBufferData)];
			replicaNameString = [repList getNameFromIPAddress:inOldIP];
			if ( replicaNameString == NULL )
				continue;
			
			replicaDict = [repList getReplicaByName:replicaNameString];
			if ( replicaDict != NULL )
			{
				[repList addIPAddress:inNewIP orReplaceIP:inOldIP inReplica:replicaDict];
				CFRelease( replicaDict );
			}
			
			replicaListString = (NSString *)[repList xmlString];
			if ( replicaListString != nil )
			{
				// set the attribute
				pNewAttrValue = dsAllocAttributeValueEntry(dsRef, pAttrValueEntry->fAttributeValueID, (char *)[replicaListString UTF8String], strlen([replicaListString UTF8String]) );
				[replicaListString release];
				replicaListString = nil;
			}
			if ( pNewAttrValue == NULL )
				continue;
			
			status = dsSetAttributeValue( recordRef, attrTypeNode, pNewAttrValue );
			debugerr( status, "dsSetAttributeValue(#3) = %ld\n", status );
			
			dsDeallocAttributeValueEntry( dsRef, pNewAttrValue );
			pNewAttrValue = NULL;
			
			if (recordRef != 0) {
				dsCloseRecord( recordRef );
				recordRef = 0;
			}
			
			replicaIDString = (NSString *)[repList getUniqueID];
			if ( replicaIDString != nil )
			{
				// update record passwordserver_HASH
				strcpy( fullRecordName, kPWConfigRecordPrefix );
				strcat( fullRecordName, [replicaIDString UTF8String] );
				
				status = dsUtils.OpenRecord( kDSStdRecordTypeConfig, fullRecordName, &recordRef );
				if ( status != eDSNoErr )
					continue;
				
				status = dsGetRecordAttributeValueByIndex( recordRef, attrTypeNode, 1, &pAttrValueEntry );
				if ( status != eDSNoErr )
					break;
				
				ReplicaFile *repList2 = [[ReplicaFile alloc] initWithXMLString:[NSString stringWithUTF8String:(char *)&(pAttrValueEntry->fAttributeValueData.fBufferData)]];
				replicaNameString = [repList2 getNameFromIPAddress:inOldIP];
				replicaDict = [repList2 getReplicaByName:replicaNameString];
				if ( replicaDict != NULL )
				{
					[repList2 addIPAddress:inNewIP orReplaceIP:inOldIP inReplica:replicaDict];
					CFRelease( replicaDict );
				}
				
				replicaListString = (NSString *)[repList2 xmlString];
				if ( replicaListString != NULL )
				{
					// set the attribute
					pNewAttrValue = dsAllocAttributeValueEntry( dsRef, pAttrValueEntry->fAttributeValueID, (char *)[replicaListString UTF8String], strlen([replicaListString UTF8String]) );
					[replicaListString release];
					replicaListString = nil;
				}
				
				if ( pNewAttrValue != NULL )
				{
					status = dsSetAttributeValue( recordRef, attrTypeNode, pNewAttrValue );
					debugerr( status, "dsSetAttributeValue(#4) = %ld\n", status );
					
					dsDeallocAttributeValueEntry( dsRef, pNewAttrValue );
					pNewAttrValue = NULL;
				}
				
				if (recordRef != 0) {
					dsCloseRecord( recordRef );
					recordRef = 0;
				}
			}
		}
	}
	while(false);
	
	if (recordRef != 0) {
		dsCloseRecord( recordRef );
		recordRef = 0;
	}
}


//---------------------------------------------------------------------------------------------------------------
//	GetPWServerAddresses
//
//	Returns: a comma-delimited list of IP addresses in each locally hosted node.
//---------------------------------------------------------------------------------------------------------------

void GetPWServerAddresses(char *outAddressStr)
{
    long						status				= eDSNoErr;
	tAttributeValueEntry	   *pAttrValueEntry		= NULL;
    unsigned long				index				= 0;
    unsigned long				nodeCount			= 0;
    tRecordReference			recordRef			= 0;
    tDataNode				   *attrTypeNode		= nil;
    DSUtils						dsUtils;
	
    *outAddressStr = '\0';
    
	status = dsUtils.GetLocallyHostedNodeList();
	if ( status != eDSNoErr )
		return;
	
    do
    {
		nodeCount = dsUtils.GetLocallyHostedNodeCount();
        for ( index = 1; index <= nodeCount; index++ )
        {
			status = dsUtils.OpenLocallyHostedNode( index );
			if ( status != eDSNoErr )
				continue;
            
			status = dsUtils.OpenRecord( kDSStdRecordTypeConfig, kPWConfigDefaultRecordName, &recordRef );
			if ( status != eDSNoErr )
				continue;
            
            attrTypeNode = dsDataNodeAllocateString( 0, kDS1AttrPasswordServerLocation );
            status = dsGetRecordAttributeValueByIndex( recordRef, attrTypeNode, 1, &pAttrValueEntry );
            if ( status != eDSNoErr )
				break;
            
            if ( outAddressStr[0] != '\0' )
                strcat( outAddressStr, "," );
            strcat( outAddressStr, (char *)&(pAttrValueEntry->fAttributeValueData.fBufferData) );
            
            if (recordRef != 0) {
                dsCloseRecord( recordRef );
                recordRef = 0;
            }
        }
    }
    while(false);
	
	if ( outAddressStr[0] == '\0' )
	{
		do
		{
			status = dsUtils.OpenLocalLDAPNode( NULL, NULL );
			if ( status != eDSNoErr )
				break;
			
			status = dsUtils.OpenRecord( kDSStdRecordTypeConfig, kPWConfigDefaultRecordName, &recordRef );
			if ( status != eDSNoErr )
				break;
			
			attrTypeNode = dsDataNodeAllocateString( 0, kDS1AttrPasswordServerLocation );
			status = dsGetRecordAttributeValueByIndex( recordRef, attrTypeNode, 1, &pAttrValueEntry );
			if ( status != eDSNoErr )
				break;
			
			if ( outAddressStr[0] != '\0' )
				strcat( outAddressStr, "," );
			strcat( outAddressStr, (char *)&(pAttrValueEntry->fAttributeValueData.fBufferData) );
			
			if (recordRef != 0) {
				dsCloseRecord( recordRef );
				recordRef = 0;
			}
		}
		while(false);
	}
	
    if (recordRef != 0) {
        dsCloseRecord( recordRef );
        recordRef = 0;
    }
}


//---------------------------------------------------------------------------------------------------------------
//	SetPWServerAddress
//
//	inOutAddressStr - an address is required on input if <inHost> is false.
//					- an address is returned on output if <inHost> is true and <inOutAddressStr> is non-null.
//---------------------------------------------------------------------------------------------------------------

void SetPWServerAddress(const char *inUsername, const char *inPassword, char *inOutAddressStr, bool inHost, const char *inReplicaList, const char *inReplicaRecordName, SetupActionType action )
{
	long				status				= eDSNoErr;
	long				status2				= eDSNoErr;
	char				*addressStr			= nil;
	DSUtils				ldapNode;
	char				fullRecordName[256];
	
	if ( action == kSetupActionGeneral )
	{
		// for -hostpasswordserver, <inAddressStr> will be nil, and we should
		// use this machine's IP address
		if ( inHost )
		{
			addressStr = (char *)malloc(30);
			GetMyAddressAsString( addressStr );
			//printf("%s\n",addressStr);
			
			if ( inOutAddressStr )
				strcpy( inOutAddressStr, addressStr );
		}
		else
		if ( inOutAddressStr != NULL )
		{
			long addrLen;
			
			addrLen = strlen(inOutAddressStr);
			addressStr = (char *)malloc(addrLen+1);
			strcpy(addressStr, inOutAddressStr);
		}
		
		if ( addressStr != NULL )
		{
			// Note: if setting up a replica, do not update the old Jaguar attribute in LDAP. It should point at the LDAP master, not the replica.
			if ( action != kSetupActionSetupReplica )
			{
				status = SetPWConfigAttribute( kPWConfigDefaultRecordName, kDS1AttrPasswordServerLocation, addressStr, inUsername, inPassword, &ldapNode );
				status2 = SetPWConfigAttributeLocal( kPWConfigDefaultRecordName, kDS1AttrPasswordServerLocation, addressStr, true, true );
			}
		}
	}
	
	if ( inReplicaList != NULL )
	{
		status = SetPWConfigAttribute( kPWConfigDefaultRecordName, kDS1AttrPasswordServerList, inReplicaList, inUsername, inPassword, &ldapNode );
		if ( inReplicaRecordName != NULL )
		{
			strcpy( fullRecordName, kPWConfigRecordPrefix );
			strcat( fullRecordName, inReplicaRecordName );
			status = SetPWConfigAttribute( fullRecordName, kDS1AttrPasswordServerList, inReplicaList, inUsername, inPassword, &ldapNode );
		}
	}
	
	if (addressStr != nil) {
        free(addressStr);
    }
}

long RemovePWSListAttributeAndPWSReplicaRecord( const char *inUsername, const char *inPassword )
{
	DSUtils						ldapNode;
	long						status				= eDSNoErr;
	tRecordReference			pwConfigRecordRef	= 0;
	tRecordReference			recordRef			= 0;
	tDataNodePtr				attrName			= NULL;
	tDirReference				dsRef				= 0;
	tAttributeValueEntryPtr		attrValueEntry		= NULL;
	NSString*					replicaIDString		= nil;
	char*						pwsListAttrStr		= NULL;
	char						pwsReplicaRecordName[96];

	status = ldapNode.OpenLocalLDAPNode( inUsername, inPassword );

	if( status == eDSNoErr ) {
		dsRef = ldapNode.GetDSRef();
		status = ldapNode.OpenRecord( kDSStdRecordTypeConfig, kPWConfigDefaultRecordName, &pwConfigRecordRef, true );
	}
	
	// get the  replica ID string from the ID key in the kDS1AttrPasswordServerList attribute
	if( status == eDSNoErr )
		attrName = dsDataNodeAllocateString( dsRef, kDS1AttrPasswordServerList );

	if( status == eDSNoErr && attrName != NULL )
		status = dsGetRecordAttributeValueByIndex( pwConfigRecordRef, attrName, 1, &attrValueEntry );
	
	if( status == eDSNoErr ) {
		pwsListAttrStr = attrValueEntry->fAttributeValueData.fBufferData;
		if( pwsListAttrStr != NULL ) {
			ReplicaFile *replicaFile = [[ReplicaFile alloc] initWithXMLStr:pwsListAttrStr];
			replicaIDString = (NSString *)[replicaFile getUniqueID];
		}
	}
	
	if( [replicaIDString length] > 0 ) {
		sprintf( pwsReplicaRecordName, "%s_%s", kPWConfigDefaultRecordName, [replicaIDString UTF8String] );
		status = ldapNode.OpenRecord( kDSStdRecordTypeConfig, pwsReplicaRecordName, &recordRef, false );
		if( status == eDSNoErr )
			status = dsDeleteRecord( recordRef );
		if( status == eDSRecordNotFound )
			status = eDSNoErr;
		if( status != eDSNoErr )
			dsCloseRecord( recordRef );
		recordRef = 0;
	}

	if( attrName != NULL )
		status = dsRemoveAttribute( pwConfigRecordRef, attrName);

	if( attrName != NULL ) {
		dsDataNodeDeAllocate( dsRef, attrName );
		attrName = NULL;
	}

	if( attrValueEntry != NULL ) {
		dsDeallocAttributeValueEntry( dsRef, attrValueEntry );
		attrValueEntry = NULL;
	}

	if (recordRef != 0) {
		dsCloseRecord( recordRef );
		recordRef = 0;
	}

	if (pwConfigRecordRef != 0) {
		dsCloseRecord( pwConfigRecordRef );
		pwConfigRecordRef = 0;
	}

	return status;
}

//-----------------------------------------------------------------------------
//	 GetMyAddressAsString
//-----------------------------------------------------------------------------

void GetMyAddressAsString( char *outAddressStr )
{
	unsigned char ip[4];
    struct sockaddr_in server_addr;
	
	get_myaddress(&server_addr);
    memcpy(ip, &server_addr.sin_addr.s_addr, 4);
    sprintf(outAddressStr, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}



//---------------------------------------------------------------------------------------------------------------
//	SetPWConfigAttribute
//---------------------------------------------------------------------------------------------------------------

long SetPWConfigAttribute( const char *inRecordName, const char *inAttributeName, const char *inValue, const char *inUser, const char *inPassword, DSUtils *ldapNode )
{
	long status = eDSNoErr;
	
	// As of Slate, we're not using password server for the local node anymore
	//status = SetPWConfigAttributeLocal( inRecordName, inAttributeName, inValue );
	status = SetPWConfigAttributeLDAP( inRecordName, inAttributeName, inValue, inUser, inPassword, ldapNode );
	
	return status;
}


long SetPWConfigAttributeLocal( const char *inRecordName, const char *inAttributeName, const char *inValue, bool inCreateRecord, bool inParentOnly )
{
    long						status				= eDSNoErr;
    tAttributeValueEntry	   *pExistingAttrValue	= NULL;
    tAttributeValueEntry	   *pNewAttrValue		= NULL;
    unsigned long				index				= 0;
    unsigned long				nodeCount			= 0;
	tRecordReference			recordRef			= 0;
    tDataNode				   *attrTypeNode		= nil;
    tDirReference				dsRef				= 0;
	DSUtils						dsUtils;
	
	status = dsUtils.GetLocallyHostedNodeList();
	if ( status != eDSNoErr ) {
		debug ( "GetLocallyHostedNodeList = %ld.\n", status );
		return status;
	}
	
	dsRef = dsUtils.GetDSRef();
	
    do
    {
		nodeCount = dsUtils.GetLocallyHostedNodeCount();
        for ( index = 1; index <= nodeCount; index++ )
        {
			if ( inParentOnly )
			{
				status = dsUtils.OpenNodeByName( "/Local/..", NULL, NULL );
				if ( status != eDSNoErr )
					break;
			}
			else
			{
				status = dsUtils.OpenLocallyHostedNode( index );
			}
			if ( status != eDSNoErr )
				continue;
            
			status = dsUtils.OpenRecord( kDSStdRecordTypeConfig, inRecordName, &recordRef, inCreateRecord );
            if ( status != eDSNoErr )
				continue;
            
            attrTypeNode = dsDataNodeAllocateString( 0, inAttributeName );
            status = dsGetRecordAttributeValueByIndex( recordRef, attrTypeNode, 1, &pExistingAttrValue );
            if ( status == eDSNoErr )
            {
				long valueLen = strlen(inValue);
                char *valueStr = (char *) malloc( valueLen + 1 );
                unsigned long attributeValueID = 1;
                
                strcpy( valueStr, inValue );
                if ( pExistingAttrValue ) {
                    attributeValueID = pExistingAttrValue->fAttributeValueID;
                }
                pNewAttrValue = dsAllocAttributeValueEntry(dsRef, attributeValueID, valueStr, valueLen );
                if ( pNewAttrValue == nil ) continue;
				
                status = dsSetAttributeValue(recordRef, attrTypeNode, pNewAttrValue);
				debugerr( status, "dsSetAttributeValue(#1) = %ld\n", status );
            }
            else
            if ( status == eDSAttributeNotFound || status == eDSAttributeDoesNotExist )
            {
                tDataNodePtr attrValue;
                
                attrValue = dsDataNodeAllocateString(dsRef, inValue);
                if ( attrValue ) {
                    status = dsAddAttribute( recordRef, attrTypeNode, NULL, attrValue );
					debugerr( status, "dsAddAttribute(#1) = %ld\n", status );
                }
            }
            // need to handle case of attribute with no values
				
            if (recordRef != 0) {
                dsCloseRecord( recordRef );
                recordRef = 0;
            }
			
			if ( inParentOnly )
				break;
        }
    }
    while( false );
    
    if (recordRef != 0) {
        dsCloseRecord( recordRef );
        recordRef = 0;
    }
	
	return status;
}


long SetPWConfigAttributeLDAP( const char *inRecordName, const char *inAttributeName, const char *inValue, const char *inUser, const char *inPassword, DSUtils *ldapNode )
{
    long						status				= eDSNoErr;
    tAttributeValueEntry	   *pExistingAttrValue	= NULL;
    tAttributeValueEntry	   *pNewAttrValue		= NULL;
	tRecordReference			recordRef			= 0;
    tDataNode				   *attrTypeNode		= nil;
	tDirReference				dsRef				= 0;
	
	if ( inUser != NULL && inPassword != NULL && ldapNode != NULL )
	{
		do
		{
			status = ldapNode->OpenLocalLDAPNode( inUser, inPassword );
			if ( status != eDSNoErr )
				break;
				
			status = ldapNode->OpenRecord( kDSStdRecordTypeConfig, inRecordName, &recordRef, true );
			if ( status != eDSNoErr )
				break;
			
			dsRef = ldapNode->GetDSRef();
			
			attrTypeNode = dsDataNodeAllocateString( 0, inAttributeName );
			status = dsGetRecordAttributeValueByIndex( recordRef, attrTypeNode, 1, &pExistingAttrValue );
			if ( status == eDSNoErr )
			{
				long valueLen = strlen(inValue);
				char *valueStr = (char *) malloc( valueLen + 1 );
				unsigned long attributeValueID = 1;
				
				strcpy( valueStr, inValue );
				if ( pExistingAttrValue ) {
					attributeValueID = pExistingAttrValue->fAttributeValueID;
				}
				pNewAttrValue = dsAllocAttributeValueEntry( dsRef, attributeValueID, valueStr, valueLen );
				if ( pNewAttrValue == nil ) break;
				
				status = dsSetAttributeValue( recordRef, attrTypeNode, pNewAttrValue );
				debugerr( status, "dsSetAttributeValue(#1.2) = %ld\n", status );
			}
			else
			if ( status == eDSAttributeNotFound || status == eDSAttributeDoesNotExist )
			{
				tDataNodePtr attrValue;
				
				attrValue = dsDataNodeAllocateString( dsRef, inValue );
				if ( attrValue ) {
					status = dsAddAttribute( recordRef, attrTypeNode, NULL, attrValue );
					debugerr( status, "dsAddAttribute(#1.2) = %ld\n", status );
				}
			}
			// need to handle case of attribute with no values
			else
			if ( status == eDSIndexOutOfRange )
			{
				tDataNodePtr attrValue;
				
				attrValue = dsDataNodeAllocateString( dsRef, inValue );
				if ( attrValue ) {
					status = dsAddAttributeValue( recordRef, attrTypeNode, attrValue );
					dsDataNodeDeAllocate( dsRef, attrValue );
					debugerr( status, "dsAddAttributeValue(#1.2) = %ld\n", status );
				}
			}
			
			if (recordRef != 0) {
				dsCloseRecord( recordRef );
				recordRef = 0;
			}
		}
		while( false );
		
		if (recordRef != 0) {
			dsCloseRecord( recordRef );
			recordRef = 0;
		}
	}
	
	return status;
}


// ---------------------------------------------------------------------------
//	* ProcessRunning
//
//  Returns: -1 = not running, or pid
// ---------------------------------------------------------------------------

pid_t ProcessRunning( const char *inProcName )
{
	register size_t		i ;
	register pid_t 		pidLast		= -1 ;
	int					mib[]		= { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
	size_t				ulSize		= 0;

	// Allocate space for complete process list
	if ( 0 > sysctl( mib, 4, NULL, &ulSize, NULL, 0) )
		return( pidLast );
	
	i = ulSize / sizeof( struct kinfo_proc );
	struct kinfo_proc	*kpspArray = new kinfo_proc[ i ];
	if ( !kpspArray )
		return( pidLast );
	
	// Get the proc list
	ulSize = i * sizeof( struct kinfo_proc );
	if ( 0 > sysctl( mib, 4, kpspArray, &ulSize, NULL, 0 ) )
	{
		delete [] kpspArray;
		return( pidLast );
	}

	register struct kinfo_proc	*kpsp = kpspArray;
	
	for ( ; i-- ; kpsp++ )
	{
		// match the name
		if ( strcmp( kpsp->kp_proc.p_comm, inProcName ) == 0 )
		{
			// skip zombies
			if ( kpsp->kp_proc.p_stat != SZOMB )
			{
				pidLast = kpsp->kp_proc.p_pid;
				continue;
			}
		}
	}
	
	delete [] kpspArray;
	
	return( pidLast );
}

// ---------------------------------------------------------------------------
//	* ProcessName
// ---------------------------------------------------------------------------

char *ProcessName( pid_t inPID )
{
	register size_t		i ;
	int					mib[]		= { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
	size_t				ulSize		= 0;
	char				*retVal		= NULL;
	
	// Allocate space for complete process list
	if ( 0 > sysctl( mib, 4, NULL, &ulSize, NULL, 0) )
		return NULL;
	
	i = ulSize / sizeof( struct kinfo_proc );
	struct kinfo_proc	*kpspArray = new kinfo_proc[ i ];
	if ( !kpspArray )
		return NULL;
	
	// Get the proc list
	ulSize = i * sizeof( struct kinfo_proc );
	if ( 0 > sysctl( mib, 4, kpspArray, &ulSize, NULL, 0 ) )
	{
		delete [] kpspArray;
		return NULL;
	}

	register struct kinfo_proc	*kpsp = kpspArray;
	
	for ( ; i-- ; kpsp++ )
	{
		// match the name
		if ( kpsp->kp_proc.p_pid == inPID )
		{			
			retVal = strdup( kpsp->kp_proc.p_comm );
			break;
		}
	}
	
	delete [] kpspArray;
	
	return retVal;
}


