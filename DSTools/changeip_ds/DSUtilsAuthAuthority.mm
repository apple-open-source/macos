/*
 *  DSUtilsAuthAuthority.cpp
 *  NeST
 *
 *  Created by admin on Mon Oct 06 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#include <unistd.h>
#include <PasswordServer/AuthFile.h>

#include "DSUtilsAuthAuthority.h"
#include "Common.h"

DSUtilsAuthAuthority::DSUtilsAuthAuthority(
	const char *inUsername,
	const char *inServerAddress,
	const char *inPassword,
	const char *inUserID,
  	bool inVerifyOnly,
	bool inSetToBasic )
	: DSUtils()
{
	mUserID = NULL;
	mUsername[0] = '\0';
	mServerAddress[0] = '\0';
	mPassword[0] = '\0';
	mKerberosDomain[0] = '\0';
	
	if ( inUsername != NULL )
		strlcpy( mUsername, inUsername, sizeof(mUsername) );
	if ( inServerAddress != NULL )
		strlcpy( mServerAddress, inServerAddress, sizeof(mServerAddress) );
	if ( inPassword != NULL )
		strlcpy( mPassword, inPassword, sizeof(mPassword) );
	if ( inUserID != NULL )
		mUserID = strdup( inUserID );
	
	mVerifyOnly = inVerifyOnly;
	mSetToBasic = inSetToBasic;
	mSetKerberosAA = false;
	
	strcpy( mAuthType, "ApplePasswordServer" );
}


DSUtilsAuthAuthority::~DSUtilsAuthAuthority()
{
	if ( mUserID != NULL )
		free( mUserID );
}


tDirStatus
DSUtilsAuthAuthority::DoActionOnCurrentNode( void )
{
    tDirStatus					status				= eDSNoErr;
    tRecordReference			recordRef			= 0;
	
	status = this->OpenRecord( kDSStdRecordTypeUsers, mUsername, &recordRef );
	if (status != eDSNoErr)
		return status;
	
	this->HandleAuthAuthorityForRecord( recordRef );
	
	if ( recordRef != 0 )
	{
		dsCloseRecord( recordRef );
		recordRef = 0;
	}
	
	return status;
}


//-----------------------------------------------------------------------------
//	 HandleAuthAuthorityForRecord
//-----------------------------------------------------------------------------

void
DSUtilsAuthAuthority::HandleAuthAuthorityForRecord( tRecordReference inRecordRef )
{
    tDirReference				dsRef				= 0;
    long						status				= eDSNoErr;
	tAttributeValueEntry	   *pExistingAttrValue	= NULL;
	unsigned long				attrValIndex		= 0;
    unsigned long				attrValCount		= 0;
    tDataNode				   *attrTypeNode		= nil;
    tAttributeEntryPtr			pAttrEntry			= nil;
    char						*aaVersion			= nil;
    char						*aaTag				= nil;
    char						*aaData				= nil;
    bool						hasServerID			= false;
    bool						hasServerIDOnThisServer	= false;
    unsigned long				attrValueIDToReplace = 0;
	
    do
    {
		// initialize state
		hasServerID = false;
		hasServerIDOnThisServer = false;
		pExistingAttrValue = nil;
		attrValueIDToReplace = 0;
                        
		// get info about this attribute
		attrTypeNode = dsDataNodeAllocateString( 0, kDSNAttrAuthenticationAuthority );
		status = dsGetRecordAttributeInfo( inRecordRef, attrTypeNode, &pAttrEntry );
		debugerr(status, "dsGetRecordAttributeInfo = %ld\n", status);
		if ( status == eDSNoErr )
		{
			// run through the values and replace the target authority if it exists
			attrValCount = pAttrEntry->fAttributeValueCount;
			for ( attrValIndex = 1; attrValIndex <= attrValCount; attrValIndex++ )
			{
				status = dsGetRecordAttributeValueByIndex( inRecordRef, attrTypeNode, attrValIndex, &pExistingAttrValue );
				debugerr(status, "dsGetRecordAttributeValueByIndex = %ld\n", status);
				if (status != eDSNoErr) continue;
				
				status = dsParseAuthAuthority( pExistingAttrValue->fAttributeValueData.fBufferData, &aaVersion, &aaTag, &aaData );
				if (status != eDSNoErr) continue;
				
				hasServerID = ( strcasecmp( aaTag, mAuthType ) == 0 );
				if ( hasServerID )
				{
					attrValueIDToReplace = pExistingAttrValue->fAttributeValueID;
					
					char *endPtr = strchr( aaData, ':' );
					if ( endPtr )
					{
						*endPtr++ = '\0';
						if ( strcmp( mServerAddress, endPtr ) == 0 )
						{
							if ( mVerifyOnly )
							{
								if ( mUserID != NULL )
									free( mUserID );
								mUserID = strdup( aaData );
							}
							hasServerIDOnThisServer = true;
						}
					}
					
					break;
				}
				else
				{
					if ( attrValCount == 1 &&
						 (strcasecmp(aaTag, "basic") == 0 || strcasecmp(aaTag, "ShadowHash") == 0) )
					{
						attrValueIDToReplace = pExistingAttrValue->fAttributeValueID;
					}
					else
					{
						pExistingAttrValue = nil;
					}
				}
			}
		}
		
		if ( /*!hasServerIDOnThisServer && */ !mVerifyOnly &&
				(status == eDSNoErr || status == eDSAttributeNotFound || status == eDSAttributeDoesNotExist ) )
		{
			tDataNodePtr aaNode;
			char aaNewData[2048];
			tDataNode *pwAttrTypeNode = nil;
			tAttributeValueEntry *pExistingPWAttrValue = NULL;
			tAttributeValueEntry *pNewPWAttrValue = NULL;
			unsigned long attributeValueID;
			bool attributeExists = ( status == eDSNoErr );
			
			// set the auth authority
			if ( mSetToBasic )
				strcpy( aaNewData, ";basic;" );
			else
			if ( mSetKerberosAA )
			{
	//		;Kerberosv5;0x40a16e695e4699aa0000000400000004;steve@SIMOST4.APPLE.COM;SIMOST4.APPLE.COM;
	//		1024 35 164692999217882480120010181985339211388047639839616187360976365575530073744777771701552637536890555905013528783554248142660969160507356813214281057897972383713616368593028764113166043426838691582996328640572552610270859282539365270980737661781203823315290805865338656227505745975321882937715664665247820049277 root@simost4.apple.com:17.221.40.76
				long len;
				
				len = sprintf( aaNewData, ";%s;", mAuthType );
				strncat( aaNewData, mUserID, 34 );
				len += 34;
				aaNewData[len] = '\0';
				len += snprintf( aaNewData + len, sizeof(aaNewData) - len, ";%s@%s;%s;%s:%s", mUsername, mKerberosDomain, mKerberosDomain, mUserID + 35, mServerAddress );
			}
			else
			{
				snprintf( aaNewData, sizeof(aaNewData), ";%s;%s:%s", mAuthType, mUserID, mServerAddress );
			}
			
			if ( pExistingAttrValue != nil )
			{
				pNewPWAttrValue = dsAllocAttributeValueEntry(dsRef, attrValueIDToReplace, aaNewData, strlen(aaNewData));
				if ( pNewPWAttrValue != nil )
				{
					debug("using dsSetAttributeValue for authentication_authority\n");
					status = dsSetAttributeValue( inRecordRef, attrTypeNode, pNewPWAttrValue );
					dsDeallocAttributeValueEntry( dsRef, pNewPWAttrValue );
					pNewPWAttrValue = nil;
				}
			}
			else
			if ( attributeExists )
			{
				aaNode = dsDataNodeAllocateString( dsRef, aaNewData );
				if ( aaNode )
				{
					debug("using dsAddAttributeValue for authentication_authority\n");
					status = dsAddAttributeValue( inRecordRef, attrTypeNode, aaNode );
					dsDataNodeDeAllocate( dsRef, aaNode );
				}
			}
			else
			{
				// no authority
				aaNode = dsDataNodeAllocateString( dsRef, aaNewData );
				if ( aaNode )
				{
					debug("using dsAddAttribute for authentication_authority\n");
					status = dsAddAttribute( inRecordRef, attrTypeNode, NULL, aaNode );
					dsDataNodeDeAllocate( dsRef, aaNode );
				}
			}
			
			debugerr( status, "status(1) = %ld.\n", status );
			
			// get the passwd attribute to change
			pwAttrTypeNode = dsDataNodeAllocateString( 0, kDS1AttrPassword );
			status = dsGetRecordAttributeValueByIndex( inRecordRef, pwAttrTypeNode, 1, &pExistingPWAttrValue );
			
			// [3053515] not a fatal error
			if (status != eDSNoErr) {
				pExistingPWAttrValue = NULL;
				status = eDSNoErr;
			}
			
			if ( mSetToBasic )
			{
				char saltTable[64];
				char salt[3] = { 0, 0, 0 };
				time_t now;
				long lnow;
				int idx;
				
				idx = 0;
				for ( int ii = 'A'; ii <= 'Z'; ii++ )
					saltTable[idx++] = ii;
				for ( int ii = 'a'; ii <= 'z'; ii++ )
					saltTable[idx++] = ii;
				for ( int ii = '0'; ii <= '9'; ii++ )
					saltTable[idx++] = ii;
				saltTable[idx++] = '+';
				saltTable[idx++] = '/';
				
				time( &now );
				lnow = (long) now;
				salt[0] = saltTable[ (lnow & 0x0000003F) ];
				salt[1] = saltTable[ ((lnow & 0x00003F00) >> 8) ];
				
				char *staticDataPtr = crypt( mPassword, salt );
				strcpy( aaNewData, staticDataPtr );
			}
			else
			{
				strcpy(aaNewData, "********");
			}
			
			if ( pExistingPWAttrValue != NULL )
			{
				// change the passwd attribute to "********"
				attributeValueID = pExistingPWAttrValue->fAttributeValueID;
				
				pNewPWAttrValue = dsAllocAttributeValueEntry(dsRef, attributeValueID, aaNewData, strlen(aaNewData));
				if ( pNewPWAttrValue == nil ) continue;
				
				status = dsSetAttributeValue(inRecordRef, pwAttrTypeNode, pNewPWAttrValue);
				debugerr( status, "dsSetAttributeValue(#2) = %ld\n", status );
				
				dsDeallocAttributeValueEntry(dsRef,pNewPWAttrValue);
			}
			/*
			else
			if ( pwAttributeExists )
			{
				aaNode = dsDataNodeAllocateString( dsRef, aaNewData );
				if ( aaNode )
				{
					debug("using dsAddAttributeValue for passwd\n");
					status = dsAddAttributeValue( inRecordRef, attrTypeNode, aaNode );
					dsDataNodeDeAllocate( dsRef, aaNode );
				}
			}
			*/
			else
			{
				// no passwd
				aaNode = dsDataNodeAllocateString( dsRef, aaNewData );
				if ( aaNode )
				{
					debug("using dsAddAttribute for passwd\n");
					status = dsAddAttribute( inRecordRef, pwAttrTypeNode, NULL, aaNode );
					debugerr( status, "dsAddAttribute(#2) = %ld\n", status );
					
					dsDataNodeDeAllocate( dsRef, aaNode );
				}
			}
		}
		else
		{
			debugerr(status, "ds error = %ld\n", status);
		}
    }
    while(false);
}


void
DSUtilsAuthAuthority::CopyUserID( char *outUserID )
{
	if ( mUserID != NULL )
		strcpy( outUserID, mUserID );
	else
		*outUserID = '\0';
}


void
DSUtilsAuthAuthority::SetAuthTypeToKerberos( const char *inKerberosDomain )
{
	strcpy( mAuthType, "Kerberosv5" );
	if ( inKerberosDomain != NULL )
		strcpy( mKerberosDomain, inKerberosDomain );
	mSetKerberosAA = true;
}


void
DSUtilsAuthAuthority::SetUserID( const char *inUserID )
{
	if ( inUserID != NULL )
	{
		free( mUserID );
		mUserID = strdup( inUserID );
	}
}


void
DSUtilsAuthAuthority::SetPassword( const char *inPassword )
{
	if ( inPassword != NULL )
		strlcpy( mPassword, inPassword, sizeof(mPassword) );
}





		
