/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <arpa/inet.h>
#include "CPSPlugInUtils.h"
#include <PasswordServer/AuthDBFileDefs.h>
#include <CoreServices/CoreServices.h>

#define DEBUGLOG(A,args...)		CShared::LogIt( 0x0F, (A), ##args )

//------------------------------------------------------------------------------------
//	* PWSErrToDirServiceError
//------------------------------------------------------------------------------------

SInt32 PWSErrToDirServiceError( PWServerError inError )
{
    SInt32 result = 0;
    
    if ( inError.err == 0 )
        return 0;
    
    switch ( inError.type )
    {
        case kPolicyError:
            result = PolicyErrToDirServiceError( inError.err );
            break;
        
        case kSASLError:
            result = SASLErrToDirServiceError( inError.err );
            break;
		
        case kConnectionError:
            result = eDSAuthServerError;
            break;

		case kGeneralError:
			result = eDSAuthFailed;
			break;
    }
    
    return result;
}


//------------------------------------------------------------------------------------
//	* SASLErrToDirServiceError
//------------------------------------------------------------------------------------

SInt32 PolicyErrToDirServiceError( int inPolicyError )
{
    SInt32 dirServiceErr = eDSAuthFailed;
    
    switch( inPolicyError )
    {
		case kAuthOK:							dirServiceErr = eDSNoErr;							break;
		case kAuthFail:							dirServiceErr = eDSAuthFailed;						break;
		case kAuthUserDisabled:					dirServiceErr = eDSAuthAccountDisabled;				break;
		case kAuthNeedAdminPrivs:				dirServiceErr = eDSPermissionError;					break;
		case kAuthUserNotSet:					dirServiceErr = eDSAuthUnknownUser;					break;
		case kAuthUserNotAuthenticated:			dirServiceErr = eDSAuthFailed;						break;
		case kAuthPasswordExpired:				dirServiceErr = eDSAuthAccountExpired;				break;
		case kAuthPasswordNeedsChange:			dirServiceErr = eDSAuthNewPasswordRequired;			break;
		case kAuthPasswordNotChangeable:		dirServiceErr = eDSAuthFailed;						break;
		case kAuthPasswordTooShort:				dirServiceErr = eDSAuthPasswordTooShort;			break;
		case kAuthPasswordTooLong:				dirServiceErr = eDSAuthPasswordTooLong;				break;
		case kAuthPasswordNeedsAlpha:			dirServiceErr = eDSAuthPasswordNeedsLetter;			break;
		case kAuthPasswordNeedsDecimal:			dirServiceErr = eDSAuthPasswordNeedsDigit;			break;
		case kAuthMethodTooWeak:				dirServiceErr = eDSAuthMethodNotSupported;			break;
		case kAuthPasswordNeedsMixedCase:		dirServiceErr = eDSAuthPasswordQualityCheckFailed;  break;
		case kAuthPasswordHasGuessablePattern:  dirServiceErr = eDSAuthPasswordQualityCheckFailed;  break;
		case kAuthPasswordCannotBeUsername:		dirServiceErr = eDSAuthPasswordQualityCheckFailed;  break;
		case kAuthPasswordNeedsSymbol:			dirServiceErr = eDSAuthPasswordQualityCheckFailed;  break;
    }
    
    return dirServiceErr;
}


//------------------------------------------------------------------------------------
//	* SASLErrToDirServiceError
//------------------------------------------------------------------------------------

SInt32 SASLErrToDirServiceError( int inSASLError )
{
    SInt32 dirServiceErr = eDSAuthFailed;

    switch (inSASLError)
    {
        case SASL_CONTINUE:		dirServiceErr = eDSNoErr;					break;
        case SASL_OK:			dirServiceErr = eDSNoErr;					break;
        case SASL_FAIL:			dirServiceErr = eDSAuthFailed;				break;
        case SASL_NOMEM:		dirServiceErr = eMemoryError;				break;
        case SASL_BUFOVER:		dirServiceErr = eDSBufferTooSmall;			break;
        case SASL_NOMECH:		dirServiceErr = eDSAuthMethodNotSupported;	break;
        case SASL_BADPROT:		dirServiceErr = eDSAuthParameterError;		break;
        case SASL_NOTDONE:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_BADPARAM:		dirServiceErr = eDSAuthParameterError;		break;
        case SASL_TRYAGAIN:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_BADMAC:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_NOTINIT:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_INTERACT:		dirServiceErr = eDSAuthParameterError;		break;
        case SASL_BADSERV:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_WRONGMECH:	dirServiceErr = eDSAuthParameterError;		break;
        case SASL_BADAUTH:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_NOAUTHZ:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_TOOWEAK:		dirServiceErr = eDSAuthMethodNotSupported;	break;
        case SASL_ENCRYPT:		dirServiceErr = eDSAuthInBuffFormatError;	break;
        case SASL_TRANS:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_EXPIRED:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_DISABLED:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_NOUSER:		dirServiceErr = eDSAuthUnknownUser;			break;
        case SASL_BADVERS:		dirServiceErr = eDSAuthServerError;			break;
        case SASL_UNAVAIL:		dirServiceErr = eDSAuthNoAuthServerFound;	break;
        case SASL_NOVERIFY:		dirServiceErr = eDSAuthNoAuthServerFound;	break;
        case SASL_PWLOCK:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_NOCHANGE:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_WEAKPASS:		dirServiceErr = eDSAuthBadPassword;			break;
        case SASL_NOUSERPASS:	dirServiceErr = eDSAuthFailed;				break;
    }
    
    return dirServiceErr;
}


// ---------------------------------------------------------------------------
//	* CheckServerVersionMin
// ---------------------------------------------------------------------------

bool CheckServerVersionMin( int serverVers[], int reqMajor, int reqMinor, int reqBugFix, int reqTiny )
{
	if ( serverVers[0] > reqMajor )
		return true;
	if ( serverVers[0] < reqMajor )
		return false;
	
	if ( serverVers[1] > reqMinor )
		return true;
	if ( serverVers[1] < reqMinor )
		return false;
	
	if ( serverVers[2] > reqBugFix )
		return true;
	if ( serverVers[2] < reqBugFix )
		return false;
	
	if ( serverVers[3] > reqTiny )
		return true;
	if ( serverVers[3] < reqTiny )
		return false;
	
	// equal
	return true;
}


// ---------------------------------------------------------------------------
//	* RSAPublicKeysEqual
// ---------------------------------------------------------------------------

bool RSAPublicKeysEqual ( const char *rsaKeyStr1, const char *rsaKeyStr2 )
{
	const char *end1 = rsaKeyStr1;
	const char *end2 = rsaKeyStr2;
	int index;
	bool result = false;
	
	if ( rsaKeyStr1 == NULL && rsaKeyStr2 == NULL )
		return true;
	else
	if ( rsaKeyStr1 == NULL || rsaKeyStr2 == NULL )
		return false;
	
	// locate the comment section (3rd space char)
	for ( index = 0; index < 3 && end1 != NULL; index++ )
	{
		end1 = strchr( end1, ' ' );
		if ( end1 != NULL )
			end1++;
	}
	for ( index = 0; index < 3 && end2 != NULL; index++ )
	{
		end2 = strchr( end2, ' ' );
		if ( end2 != NULL )
			end2++;
	}
	
	if ( end1 != NULL && end2 != NULL )
	{
		// the lengths of the keys (sans comment) should be the same
		if ( (end1-rsaKeyStr1) != (end2-rsaKeyStr2) )
			return false;
		result = ( strncmp( rsaKeyStr1, rsaKeyStr2, (end1-rsaKeyStr1) ) == 0 );
	}
	else
	{
		// no comment on either key
		if ( end1 == NULL && end2 == NULL )
		{
			result = ( strcmp( rsaKeyStr1, rsaKeyStr2 ) == 0 );
		}
		else
		{
			// comment on one key, get the length of the data section from that one
			if ( end1 != NULL )
				result = ( strncmp( rsaKeyStr1, rsaKeyStr2, (end1-rsaKeyStr1) ) == 0 );
			else
			if ( end2 != NULL )
				result = ( strncmp( rsaKeyStr1, rsaKeyStr2, (end2-rsaKeyStr2) ) == 0 );
		}
	}
	
	return result;
}


// ---------------------------------------------------------------------------
//	* ServiceClientCallBack
// ---------------------------------------------------------------------------

static void ServiceClientCallBack(CFNetServiceRef theService, CFStreamError *error, void *info)
{
	BonjourServiceCBData *bjContext = (BonjourServiceCBData *)info;
	
	if ( bjContext != NULL )
	{
		if ( theService != NULL )
		{
			CFNetServiceUnscheduleFromRunLoop( theService, bjContext->runLoop, kCFRunLoopCommonModes );
		}
		bjContext->checking = false;
		bjContext->errorCode = error->error;
	}
}


// ---------------------------------------------------------------------------
//	* GetServerListFromBonjourForKeyHash
// ---------------------------------------------------------------------------

long GetServerListFromBonjourForKeyHash( const char *inKeyHash, CFRunLoopRef inRunLoop, CFMutableArrayRef *outServerList )
{
	long status = 0;
	CFNetServiceRef serviceRef = NULL;
	CFStringRef keyHashString = NULL;
	CFNetServiceClientContext scContext = {0};
	CFStreamError error = {(CFStreamErrorDomain)kCFStreamErrorDomainMacOSStatus, 0};
	BonjourServiceCBData bjContext = {0};
	sPSServerEntry serverEntry = {0};
	
	*outServerList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	if ( *outServerList == NULL )
		return eMemoryError;
	
	keyHashString = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%s+"), inKeyHash );
	if ( keyHashString != NULL )
	{
		serviceRef = CFNetServiceCreate( kCFAllocatorDefault, CFSTR("local."), CFSTR("_apple-sasl._tcp."), keyHashString, 3659 );
		if ( serviceRef == NULL )
			return 0;
		
		bjContext.runLoop = inRunLoop;
		bjContext.checking = true;
		scContext.info = &bjContext;
		if ( CFNetServiceSetClient(serviceRef, ServiceClientCallBack, &scContext) )
		{
			CFNetServiceScheduleWithRunLoop( serviceRef, inRunLoop, kCFRunLoopCommonModes );
			if ( CFNetServiceResolveWithTimeout(serviceRef, 5, &error) )
			{
				for ( int checkRetry = 5; bjContext.checking && checkRetry > 0; checkRetry-- )
					sleep( 1 );
				
				if ( bjContext.errorCode == 0 )
				{
					CFIndex idx;
					struct sockaddr_in address;
					
					CFStringRef nameRef = CFNetServiceGetName((CFNetServiceRef)serviceRef);
					CFArrayRef addressResults = CFNetServiceGetAddressing( (CFNetServiceRef)serviceRef );
					
					if ( nameRef != NULL && addressResults != NULL )
					{
						CFIndex numAddressResults = CFArrayGetCount( addressResults );
						CFDataRef sockAddrRef = NULL;
						struct sockaddr sockHdr;
						
						for ( idx = 0; idx < numAddressResults; idx++ )
						{
							sockAddrRef = (CFDataRef)CFArrayGetValueAtIndex( addressResults, idx );
							if ( sockAddrRef != NULL )
							{
								CFDataGetBytes( sockAddrRef, CFRangeMake(0, sizeof(sockHdr)), (UInt8*)&sockHdr );
								switch ( sockHdr.sa_family )
								{
									case AF_INET:
										CFDataGetBytes( sockAddrRef, CFRangeMake(0, sizeof(address)), (UInt8*)&address );
										if ( inet_ntop(sockHdr.sa_family, &address.sin_addr, serverEntry.ip, (socklen_t)sizeof(serverEntry.ip)) != NULL )
										{
											strlcpy( serverEntry.id, inKeyHash, sizeof(serverEntry.id) );
											strcpy( serverEntry.port, kPasswordServerPortStr );
											AppendToArrayIfUnique( *outServerList, &serverEntry );
										}
										break;
									
									case AF_INET6:
									default:
										break;
								}
							}
						}
					}
				}
			}
			else
			{
				CFNetServiceUnscheduleFromRunLoop( serviceRef, inRunLoop, kCFRunLoopCommonModes );
			}
		}
	}
	
	return status;
}


// ---------------------------------------------------------------------------
//	* ServiceBrowserCallBack
// ---------------------------------------------------------------------------

static void ServiceBrowserCallBack(CFNetServiceBrowserRef browser, CFOptionFlags flags, CFTypeRef domainOrService,
	CFStreamError* error, void* info) 
{
	CFIndex idx;
	char buffer[256];
	struct sockaddr_in address;
	char addressString[16];
	Boolean success;
	sPSServerEntry serverEntry = {0};
	
	if (error->error == 0)
	{
		success = CFNetServiceSetClient((CFNetServiceRef)domainOrService, ServiceClientCallBack, NULL);
		if ( !success ) {
			fprintf(stderr, "CFNetServiceSetClient error\n");
		}
		CFNetServiceScheduleWithRunLoop((CFNetServiceRef)domainOrService, ((BonjourServiceCBData *)info)->runLoop, kCFRunLoopDefaultMode);

		success = CFNetServiceResolveWithTimeout( (CFNetServiceRef)domainOrService, 20000, error );
		if ( !success ) {
			fprintf(stderr, "CFNetServiceResolveWithTimeout error == %ld\n", (long)error->error);
		}
		
		CFStringRef nameRef = CFNetServiceGetName((CFNetServiceRef)domainOrService);
		CFStringRef typeRef = CFNetServiceGetType((CFNetServiceRef)domainOrService);
		CFStringRef domainRef = CFNetServiceGetDomain((CFNetServiceRef)domainOrService);

		CFArrayRef addressResults = CFNetServiceGetAddressing( (CFNetServiceRef)domainOrService );
		
		addressString[0] = '\0';
		if ( nameRef != NULL && addressResults != NULL )
		{
			CFIndex numAddressResults = CFArrayGetCount(addressResults);
			
			for ( idx = 0; idx < numAddressResults; idx++ )
			{
				CFDataRef sockAddrRef = (CFDataRef)CFArrayGetValueAtIndex( addressResults, idx );
				struct sockaddr sockHdr;
				
				if ( sockAddrRef )
				{
					CFDataGetBytes( sockAddrRef, CFRangeMake(0, sizeof(sockHdr)), (UInt8*)&sockHdr );
					// now get the appropriate data...
					switch ( sockHdr.sa_family )
					{
						case AF_INET:
							CFDataGetBytes( sockAddrRef, CFRangeMake(0, sizeof(address)), (UInt8*)&address );
							if ( inet_ntop(sockHdr.sa_family, &address.sin_addr, serverEntry.ip, (socklen_t)sizeof(serverEntry.ip)) != NULL )
							{
								char nameBuffer[256];
								
								if ( CFStringGetCString(nameRef, nameBuffer, sizeof(nameBuffer), kCFStringEncodingUTF8) )
									strlcpy( serverEntry.id, nameBuffer, 33 );
								strcpy( serverEntry.port, kPasswordServerPortStr );
								//AppendToArrayIfUnique( *outServerList, &serverEntry );
							}
							break;
						
						case AF_INET6:
							break;
						
						default:
							break;
					}
				}
			}
		}
		
		if ( nameRef != NULL && CFStringGetCString( nameRef, buffer, sizeof(buffer), kCFStringEncodingUTF8 ) ) {
			printf( "name = %s, address = %s\n", buffer, addressString );
			if ( domainRef != NULL ) {
				CFStringGetCString( domainRef, buffer, sizeof(buffer), kCFStringEncodingUTF8 );
				printf( "domain = %s\n", buffer);
			}
			if ( typeRef != NULL ) {
				CFStringGetCString( typeRef, buffer, sizeof(buffer), kCFStringEncodingUTF8 );
				printf( "domain = %s\n", buffer);
			}
			
		}
		else
			printf( "no string\n" );
		
	}
}


// ---------------------------------------------------------------------------
//	* CopyServiceBrowserDescription
// ---------------------------------------------------------------------------

CFStringRef CopyServiceBrowserDescription( const void* info )
{
	return CFSTR("");
}


// ---------------------------------------------------------------------------
//	* BrowseForPasswordServers
// ---------------------------------------------------------------------------

void BrowseForPasswordServers( CFRunLoopRef inRunLoop )
{
	BonjourBrowserCBData cbContext = {0};
	CFStreamError error = { (CFStreamErrorDomain)kCFStreamErrorDomainMacOSStatus, 0 };
    CFNetServiceClientContext context = { 0, &cbContext, NULL, NULL, CopyServiceBrowserDescription };
	CFNetServiceBrowserRef searchingBrowser = CFNetServiceBrowserCreate( NULL, ServiceBrowserCallBack, &context );
	
	if ( searchingBrowser != NULL )
	{
		cbContext.runLoop = inRunLoop;
		cbContext.checking = true;
		cbContext.errorCode = 0;
		cbContext.serverArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		if ( cbContext.serverArray != NULL )
		{
			CFNetServiceBrowserScheduleWithRunLoop( searchingBrowser, inRunLoop, kCFRunLoopDefaultMode );
			CFNetServiceBrowserSearchForServices( searchingBrowser, CFSTR(""), CFSTR("_apple-sasl._tcp."), &error );
		}
	}
}

