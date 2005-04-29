/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 
/*!
 *  @header CSMBPlugin
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/termios.h>
#include <sys/signal.h>
#include <sys/sysctl.h>

#include <Security/Authorization.h>

#include "CNSLDirNodeRep.h"
#include "CSMBPlugin.h"
#include "CSMBNodeLookupThread.h"
#include "CSMBServiceLookupThread.h"
#include "LMBDiscoverer.h"
#include "CommandLineUtilities.h"
#include "CNSLTimingUtils.h"

#define kDefaultGroupName			"Workgroup"
#define kDefaultAllCapsGroupName	"WORKGROUP"
#define	kNMBDProcessName			"nmbd"
#define	kMaxWorkgroupLen			16			// 15 characters, one null terminator
#define	kMaxNetBIOSNameLen			16			// 15 characters, one null terminator

#define	kNativeSMBUseComputerName	"dsAttrTypeNative:UseComputerNameForNetBIOSName"
#define	kUseComputerNameComment		"; Using the Computer Name to compute the NetBIOS name.  Remove this comment to override"

#define	kMaxNumLMBsForAggressiveSearch	0		// more than 10 LMBs in your subnet, we take it easy.
#define	kInitialTimeBetweenNodeLookups	60		// look again in a minute if we haven't found any LMBs

#pragma mark -
boolean_t SMBHandleSystemConfigChangedCallBack(SCDynamicStoreRef session, void *callback_argument);
CFStringRef	CreateNetBIOSNameFromComputerName( void );
int signalProcessByName(const char *name, int signal);

#pragma mark -

static Boolean			sNMBLookupToolIsAvailable = false;
static CSMBPlugin*		gPluginInstance = NULL;
const CFStringRef	gBundleIdentifier = CFSTR("com.apple.DirectoryService.SMB");
const char*			gProtocolPrefixString = "SMB";
Boolean	gUseCapitalization = true;


#pragma warning "Need to get our default Node String from our resource"

extern "C" {
CFUUIDRef ModuleFactoryUUID = CFUUIDGetConstantUUIDWithBytes ( NULL, \
								0xFB, 0x9E, 0xDB, 0xD3, 0x9B, 0x3A, 0x11, 0xD5, \
								0xA0, 0x50, 0x00, 0x30, 0x65, 0x3D, 0x61, 0xE4 );

}

static CDSServerModule* _Creator ( void )
{
	DBGLOG( "Creating new SMB Plugin\n" );
    gPluginInstance = new CSMBPlugin;
	return( gPluginInstance );
}

CDSServerModule::tCreator CDSServerModule::sCreator = _Creator;

Boolean UseCapitalization( void )
{
	return gUseCapitalization;
}

void AddWorkgroup( CFStringRef nodeNameRef )
{
	if ( gUseCapitalization )
	{
		CFMutableStringRef		modNode = CFStringCreateMutableCopy( NULL, 0, nodeNameRef );

		CFStringCapitalize( modNode, 0 );
		gPluginInstance->AddNode( modNode );
		CFRelease( modNode );
	}
	else
		gPluginInstance->AddNode( nodeNameRef );
}

CSMBPlugin::CSMBPlugin( void )
    : CNSLPlugin()
{
	DBGLOG( "CSMBPlugin::CSMBPlugin\n" );
    mNodeListIsCurrent = false;
	mNodeSearchInProgress = false;
	mLocalNodeString = NULL;
	mWINSServer = NULL;
	mNetBIOSName = NULL;
	mCommentFieldString = NULL;
	mInitialSearch = true;
	mNeedFreshLookup = true;
	mCurrentSearchCanceled = false;
	mTimeBetweenLookups = kInitialTimeBetweenNodeLookups;
	mConfFileModTime = 0;
	mSCRef = NULL;
	mComputerNameChangeKey = NULL;
	mRunningOnXServer = false;
	mUseWINSURL = false;
	mBroadcastThrottler = NULL;
}

CSMBPlugin::~CSMBPlugin( void )
{
	DBGLOG( "CSMBPlugin::~CSMBPlugin\n" );
    
    if ( mLocalNodeString )
        free( mLocalNodeString );    
    mLocalNodeString = NULL;
	
	if ( mWINSServer )
		free( mWINSServer );
	mWINSServer = NULL;
	
	if ( mSCRef )
		CFRelease( mSCRef );
	mSCRef = NULL;
	
	if ( mComputerNameChangeKey )
		CFRelease( mComputerNameChangeKey );
	mComputerNameChangeKey = NULL;
}

sInt32 CSMBPlugin::InitPlugin( void )
{
    sInt32				siResult	= eDSNoErr;
    // need to see if this is installed!
    struct stat			data;
    int 				result = eDSNoErr;
	
	gUseCapitalization = strcmp( GetCodePageStringForCurrentSystem(), "CP437" ) == 0;		// ok to do capitalization	

	mLMBDiscoverer	= new LMBDiscoverer();
	mLMBDiscoverer->Initialize();
	
	pthread_mutex_init( &mNodeStateLock, NULL );
	pthread_mutex_init( &mBroadcastThrottlerLock, NULL );
	
	DBGLOG( "CSMBPlugin::InitPlugin\n" );
	
    if ( siResult == eDSNoErr )
    {
        result = stat( kNMBLookupToolPath, &data );
        if ( result < 0 )
        {
            DBGLOG( "SMB couldn't find nmblookup tool: %s (should be at:%s?)\n", strerror(errno), kNMBLookupToolPath );
        }
        else
            sNMBLookupToolIsAvailable = true;
    }
    
	if (stat( "/System/Library/CoreServices/ServerVersion.plist", &data ) == eDSNoErr)
		mRunningOnXServer = true;
		
    if ( siResult == eDSNoErr )
		ReadConfigFile();
	
	struct stat				statResult;

	// ok, let's stat the file
	if ( stat( kConfFilePath, &statResult ) == 0 )
		mConfFileModTime = statResult.st_mtimespec.tv_sec;
	
    if ( siResult == eDSNoErr )
	{
		if ( !mLocalNodeString )
		{
			if ( UseCapitalization() )
				mLocalNodeString = (char *) malloc( strlen(kDefaultGroupName) + 1 );
			else
				mLocalNodeString = (char *) malloc( strlen(kDefaultAllCapsGroupName) + 1 );

			if ( mLocalNodeString )
			{
				if ( UseCapitalization() )
					strcpy( mLocalNodeString, kDefaultGroupName );
				else
					strcpy( mLocalNodeString, kDefaultAllCapsGroupName );
			}
		}
    }
	
	mUseWINSURL = (stat( kUseWINSURLFilePath, &statResult ) == 0 );
	
    return siResult;
}

void CSMBPlugin::ActivateSelf( void )
{
    DBGLOG( "CSMBPlugin::ActivateSelf called\n" );

	OurLMBDiscoverer()->EnableSearches();
	if ( !AreWeRunningOnXServer() )
		RegisterForComputerNameChanges();
	
	CNSLPlugin::ActivateSelf();
}

void CSMBPlugin::DeActivateSelf( void )
{
	// we are getting deactivated.  We want to tell the slpd daemon to shutdown as
	// well as deactivate our DA Listener
    DBGLOG( "CSMBPlugin::DeActivateSelf called\n" );
	
	OurLMBDiscoverer()->DisableSearches();
	OurLMBDiscoverer()->ClearLMBCache();				// no longer valid
	OurLMBDiscoverer()->ResetOurBroadcastAddress();	
	OurLMBDiscoverer()->ClearBadLMBList();
	
	if ( !AreWeRunningOnXServer() )
		DeregisterForComputerNameChanges();
	
	mInitialSearch = true;	// starting from scratch
	mNeedFreshLookup = true;
	mCurrentSearchCanceled = true;
	ZeroLastNodeLookupStartTime();		// force this to start again
	mNodeSearchInProgress = false;

	CNSLPlugin::DeActivateSelf();
}

Boolean CSMBPlugin::PluginSupportsServiceType( const char* serviceType )
{
	Boolean		serviceTypeSupported = false;		// only support smb or cifs
	
	if ( serviceType && strcmp( serviceType, kDSStdRecordTypeSMBServer ) == 0 )
		serviceTypeSupported = true;
		
	return serviceTypeSupported;
}

void CSMBPlugin::RegisterForComputerNameChanges( void )
{
    CFMutableArrayRef	notifyKeys			= CFArrayCreateMutable(	kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    CFMutableArrayRef	notifyPatterns		= CFArrayCreateMutable(	kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	Boolean				setStatus			= FALSE;
	SInt32				scdStatus			= 0;

    DBGLOG( "CSMBPlugin::RegisterForComputerNameChanges for SCDynamicStoreKeyCreateComputerName:\n" );
    if ( !mComputerNameChangeKey )
		mComputerNameChangeKey = SCDynamicStoreKeyCreateComputerName(NULL);

    CFArrayAppendValue(notifyKeys, mComputerNameChangeKey);

    if ( !mSCRef )
		mSCRef = ::SCDynamicStoreCreate(NULL, gBundleIdentifier, NULL, NULL);

    setStatus = SCDynamicStoreSetNotificationKeys(mSCRef, notifyKeys, notifyPatterns);

    CFRelease(notifyKeys);
    CFRelease(notifyPatterns);

    if ( GetRunLoopRef() && mSCRef && setStatus )
    {
        ::CFRunLoopAddCommonMode( GetRunLoopRef(), kCFRunLoopDefaultMode );
        scdStatus = ::SCDynamicStoreNotifyCallback( mSCRef, GetRunLoopRef(), SMBHandleSystemConfigChangedCallBack, this );
        DBGLOG( "CSMBPlugin::RegisterForComputerNameChanges, SCDynamicStoreNotifyCallback returned %ld\n", scdStatus );
    }
    else
        DBGLOG( "CSMBPlugin::RegisterForComputerNameChanges, No Current Run Loop or setStatus returned false, couldn't store Notify callback\n" );
}

void CSMBPlugin::DeregisterForComputerNameChanges( void )
{
	if ( GetRunLoopRef() && mSCRef )
		SCDynamicStoreNotifyCancel( mSCRef );
}

void CSMBPlugin::HandleComputerNameChange( void )
{
	DBGLOG( "CSMBPlugin::HandleComputerNameChange\n" );
	
	if ( mUseComputerNameTracking )
	{
		// if no NetBIOS name is provided, we will utilize the zero conf host name.  We will want to keep track any changes
		// to the Computer Name so that we can update this value.  If a value has been manually entered into the config
		// file, we want to honor that and ignore any system wide changes to the Computer Name
		if ( mNetBIOSName )
		{
			free( mNetBIOSName );
			mNetBIOSName = NULL;
		}					

		if ( mCommentFieldString )
		{
			free( mCommentFieldString );
			mCommentFieldString = NULL;
		}					
		
		CFStringRef netBIOSNameRef = CreateNetBIOSNameFromComputerName();
	
		if ( netBIOSNameRef )
		{
			mNetBIOSName = (char*)malloc(kMaxNetBIOSNameLen);
			
			CFStringGetCString( netBIOSNameRef, mNetBIOSName, kMaxNetBIOSNameLen, kCFStringEncodingUTF8 );
			
			CFRelease( netBIOSNameRef );
		}

		CFStringEncoding	encoding;
		CFStringRef			computerNameRef = SCDynamicStoreCopyComputerName(NULL, &encoding);
	
		if ( computerNameRef )
		{
			char*		comment = (char*)malloc(CFStringGetMaximumSizeForEncoding(CFStringGetLength(computerNameRef), encoding)+1);
			 
			CFStringGetCString( computerNameRef, comment, CFStringGetMaximumSizeForEncoding(CFStringGetLength(computerNameRef), encoding)+1, encoding );
			
			CFRelease( computerNameRef );
			computerNameRef = NULL;

			mCommentFieldString = comment;
		}
		
		WriteToConfigFile( kConfFilePath );
		CheckAndHandleIfConfigFileChanged();			// this will update our mod file time and hup nmbd
	}
	else
		DBGLOG( "CSMBPlugin::HandleComputerNameChange, we are not tracking computer name, ignore\n" );
	
}

boolean_t SMBHandleSystemConfigChangedCallBack(SCDynamicStoreRef session, void *callback_argument)
{                       
	DBGLOG( "***** SMB Detecting System Configuration Change ******\n" );

	CSMBPlugin*					pluginPtr = (CSMBPlugin*)callback_argument;
	CFArrayRef					changedKeys = NULL;
	CFStringRef					currentKey = NULL;
	CFIndex 					i;
	CFIndex						numChangedKeys = 0;
	
	changedKeys = SCDynamicStoreCopyNotifiedKeys(session);

	if ( changedKeys )
	{
		numChangedKeys = CFArrayGetCount(changedKeys);
		
		for ( i = 0; i < numChangedKeys; i++ )
		{
			currentKey = (CFStringRef)CFArrayGetValueAtIndex( changedKeys, i );
			
			if ( currentKey && CFGetTypeID(currentKey) == CFStringGetTypeID() && CFStringHasSuffix( currentKey, pluginPtr->GetComputerNameChangeKey() ) )
			{
				pluginPtr->HandleComputerNameChange();
			}
		}
	}
	
	return true;				// return whether there everything went ok
}

#pragma mark -
#define kMaxSizeOfParam 1024
void CSMBPlugin::ReadConfigFile( void )
{
	// we can see if there is a config file, if so then see if they have a WINS server specified
	DBGLOG( "CSMBPlugin::ReadConfigFile\n" );
    FILE *		fp;
    char 		buf[kMaxSizeOfParam];
	Boolean		needToUpdateConfFile = false;
	
	fp = fopen(kConfFilePath,"r");
	
    if (fp == NULL) 
	{
        DBGLOG( "CSMBPlugin::ReadConfigFile, couldn't open conf file, copy temp to conf\n" );

		char		command[256];
		
		snprintf( command, sizeof(command), "/bin/cp %s %s\n", kTemplateConfFilePath, kConfFilePath );
		executecommand( command );

		fp = fopen(kConfFilePath,"r");
		
		if (fp == NULL) 
			return;
    }
    
	if ( mLocalNodeString )
		free( mLocalNodeString );
	mLocalNodeString = NULL;
	
	if ( mWINSServer )
		free( mWINSServer );
	mWINSServer = NULL;
	
	mUseComputerNameTracking = false;
	
	if ( mNetBIOSName )
		free( mNetBIOSName );
	mNetBIOSName = NULL;
	
	if ( mCommentFieldString )
		free( mCommentFieldString );
	mCommentFieldString = NULL;
	
    while (fgets(buf,kMaxSizeOfParam,fp) != NULL) 
	{
        char *pcKey;
        
        if (buf[0] == '\n' || buf[0] == '\0' || buf[0] == '#')
			continue;
			
		if (buf[0] == ';' && strncmp( kUseComputerNameComment, buf, strlen(kUseComputerNameComment) ) != 0 )
			continue;
    
		if ( buf[strlen(buf)-1] == '\n' )
			buf[strlen(buf)-1] = '\0';
					
        pcKey = strstr( buf, "wins server" );

        if ( pcKey )
		{
			pcKey = strstr( pcKey, "=" );
			
			if ( pcKey )
			{
				pcKey++;
				
				while ( isspace( *pcKey ) )
					pcKey++;
			
				long	ignore;
				if ( IsIPAddress( pcKey, &ignore ) || IsDNSName( pcKey ) )
				{
					mWINSServer = (char*)malloc(strlen(pcKey)+1);
					strcpy( mWINSServer, pcKey );
					DBGLOG( "CSMBPlugin::ReadConfigFile, WINS Server is %s\n", mWINSServer );
					mLMBDiscoverer->SetWinsServer(mWINSServer);
				}
			}
			continue;
		}

        pcKey = strstr( buf, "workgroup" );

        if ( pcKey )
		{
			pcKey = strstr( pcKey, "=" );

			if ( pcKey )
			{
				pcKey++;
				
				while ( isspace( *pcKey ) )
					pcKey++;
			
				mLocalNodeString = (char*)malloc(strlen(pcKey)+1);
				strcpy( mLocalNodeString, pcKey );

				DBGLOG( "CSMBPlugin::ReadConfigFile, Workgroup is %s\n", mLocalNodeString );
			}
			
			continue;
		}

        pcKey = strstr( buf, kUseComputerNameComment );		// we will base whether we are tieing the netBIOS name to the
															// computer name dependant on whether this comment is in the conf file
        if ( pcKey )
		{
			DBGLOG( "CSMBPlugin::ReadConfigFile, we are going to tie the netbios name to the Computer Name\n" );
			mUseComputerNameTracking = true;
			
			continue;
		}

        pcKey = strstr( buf, "netbios name" );

        if ( pcKey )
		{
			pcKey = strstr( pcKey, "=" );

			if ( pcKey )
			{
				pcKey++;
				
				while ( isspace( *pcKey ) )
					pcKey++;
			
				mNetBIOSName = (char*)malloc(strlen(pcKey)+1);
				strcpy( mNetBIOSName, pcKey );

				DBGLOG( "CSMBPlugin::ReadConfigFile, netbios name is %s\n", mNetBIOSName );
			}
			
			continue;
		}

        pcKey = strstr( buf, "server string" );

        if ( pcKey )
		{
			pcKey = strstr( pcKey, "=" );

			if ( pcKey )
			{
				pcKey++;
				
				while ( isspace( *pcKey ) )
					pcKey++;
			
				mCommentFieldString = (char*)malloc(strlen(pcKey)+1);
				strcpy( mCommentFieldString, pcKey );

				DBGLOG( "CSMBPlugin::ReadConfigFile, comment is %s\n", mCommentFieldString );
			}
			
			continue;
		}
    }

    fclose(fp);

	if ( mUseComputerNameTracking || !mNetBIOSName )
	{
		// if no NetBIOS name is provided, we will utilize the zero conf host name.  We will want to keep track any changes
		// to the Computer Name so that we can update this value.  If a value has been manually entered into the config
		// file, we want to honor that and ignore any system wide changes to the Computer Name
		
		CFStringRef netBIOSNameRef = CreateNetBIOSNameFromComputerName();
	
		if ( netBIOSNameRef )
		{
			char*		netBIOSName = (char*)malloc(kMaxNetBIOSNameLen);
			 
			CFStringGetCString( netBIOSNameRef, netBIOSName, kMaxNetBIOSNameLen, kCFStringEncodingUTF8 );
			
			CFRelease( netBIOSNameRef );
			netBIOSNameRef = NULL;

			if ( mNetBIOSName && strcmp( netBIOSName, mNetBIOSName ) != 0 )
			{
				// the computed name is different from what we've read from the file, we want to update it
				free( mNetBIOSName );
				needToUpdateConfFile = true;
			}
			else if ( mNetBIOSName )
			{
				free( mNetBIOSName );
			}
			else
				needToUpdateConfFile = true;			// if there is no net bios name, we need to add it
			
			mNetBIOSName = netBIOSName;
			mUseComputerNameTracking = true;			// if we have to create the netBIOS name the first time, set tracking to true
		}
	}

	if ( mUseComputerNameTracking || !mCommentFieldString )
	{
		// if no comment is provided, we will set this as the computer's name.  We will want to keep track any changes
		// to the Computer Name so that we can update this value.  If a value has been manually entered into the config
		// file, we want to honor that and ignore any system wide changes to the Computer Name
		
		CFStringEncoding	encoding;
		CFStringRef			computerNameRef = SCDynamicStoreCopyComputerName(NULL, &encoding);
	
		if ( computerNameRef )
		{
			char*		comment = (char*)malloc(CFStringGetMaximumSizeForEncoding(CFStringGetLength(computerNameRef), encoding)+1);
			 
			CFStringGetCString( computerNameRef, comment, CFStringGetMaximumSizeForEncoding(CFStringGetLength(computerNameRef), encoding)+1, encoding );
			
			CFRelease( computerNameRef );
			computerNameRef = NULL;

			if ( mCommentFieldString && strcmp( comment, mCommentFieldString ) != 0 )
			{
				// the computed name is different from what we've read from the file, we want to update it
				free( mCommentFieldString );
				needToUpdateConfFile = true;
			}
			else if ( mCommentFieldString )
			{
				free( mCommentFieldString );
			}
			else
				needToUpdateConfFile = true;			// if there is no comment, we need to add it
			
			mCommentFieldString = comment;
			mUseComputerNameTracking = true;			// if we have to create the comment the first time, set tracking to true
		}
	}

	if ( !mLocalNodeString )
	{
		if ( UseCapitalization() )
			mLocalNodeString = (char *) malloc( strlen(kDefaultGroupName) + 1 );
		else
			mLocalNodeString = (char *) malloc( strlen(kDefaultAllCapsGroupName) + 1 );
			
		if ( mLocalNodeString )
		{
			if ( UseCapitalization() )
				strcpy( mLocalNodeString, kDefaultGroupName );
			else
				strcpy( mLocalNodeString, kDefaultAllCapsGroupName );
			needToUpdateConfFile = true;				// need to write this back out
		}
	}

	if ( needToUpdateConfFile )
	{
		WriteToConfigFile( kConfFilePath );
		CheckAndHandleIfConfigFileChanged();			// this will update our mod file time and hup nmbd
	}
	
	WriteToConfigFile(kBrowsingConfFilePath);			// need to update the browsing config file with the appropriate codepage
}

void CSMBPlugin::WriteWorkgroupToFile( FILE* fp )
{
	if ( mLocalNodeString )
	{
		char	workgroupLine[kMaxSizeOfParam];
		
		snprintf( workgroupLine, sizeof(workgroupLine), "  workgroup = %s\n", mLocalNodeString );
		
		DBGLOG( "CSMBPlugin::WriteWorkgroupToFile writing line: [%s]\n", workgroupLine );
		fputs( workgroupLine, fp );
	}
}

void CSMBPlugin::WriteNetBIOSTrackingCommentToFile( FILE* fp )
{
	char	netBIOSCommentLine[kMaxSizeOfParam];
	snprintf( netBIOSCommentLine, sizeof(netBIOSCommentLine), "%s\n", kUseComputerNameComment );

	DBGLOG( "CSMBPlugin::WriteNetBIOSTrackingCommentToFile writing line: [%s]\n", netBIOSCommentLine );
	fputs( netBIOSCommentLine, fp );
}

void CSMBPlugin::WriteNetBIOSNameToFile( FILE* fp )
{
	if ( mNetBIOSName )
	{
		char	netBIOSNameLine[kMaxSizeOfParam];
		
		snprintf( netBIOSNameLine, sizeof(netBIOSNameLine), "  netbios name = %s\n", mNetBIOSName );
		
		DBGLOG( "CSMBPlugin::WriteNetBIOSNameToFile writing line: [%s]\n", netBIOSNameLine );
		fputs( netBIOSNameLine, fp );
	}
}

void CSMBPlugin::WriteCommentToFile( FILE* fp )
{
	if ( mCommentFieldString )
	{
		char	comment[kMaxSizeOfParam];
		
		snprintf( comment, sizeof(comment), "  server string = %s\n", mCommentFieldString );
		
		DBGLOG( "CSMBPlugin::WriteCommentToFile writing line: [%s]\n", comment );
		fputs( comment, fp );
	}
}

void CSMBPlugin::WriteWINSToFile( FILE* fp )
{
	if ( mWINSServer )
	{
		char	winsLine[kMaxSizeOfParam];
		
		snprintf( winsLine, sizeof(winsLine), "  wins server = %s\n", mWINSServer );
		
		DBGLOG( "CSMBPlugin::WriteWINSToFile writing line: [%s]\n", winsLine );
		fputs( winsLine, fp );
	}
}

void CSMBPlugin::WriteCodePageToFile( FILE* fp )
{
	char	winsLine[kMaxSizeOfParam];
	
	snprintf( winsLine, sizeof(winsLine), "  dos charset = %s\n", GetCodePageStringForCurrentSystem() );
	
	DBGLOG( "CSMBPlugin::WriteCodePageToFile writing line: [%s]\n", winsLine );
	fputs( winsLine, fp );
}

void CSMBPlugin::WriteUnixCharsetToFile( FILE* fp )
{
	char	winsLine[kMaxSizeOfParam];
		
	snprintf( winsLine, sizeof(winsLine), "  unix charset = %s\n", GetCodePageStringForCurrentSystem() );
	
	DBGLOG( "CSMBPlugin::WriteUnixCharsetToFile writing line: [%s]\n", winsLine );
	fputs( winsLine, fp );
}

void CSMBPlugin::WriteDisplayCharsetToFile( FILE* fp )
{
	char	winsLine[kMaxSizeOfParam];
	
	snprintf( winsLine, sizeof(winsLine), "  display charset = %s\n", GetCodePageStringForCurrentSystem() );
	
	DBGLOG( "CSMBPlugin::WriteDisplayCharsetToFile writing line: [%s]\n", winsLine );
	fputs( winsLine, fp );
}

void CSMBPlugin::WriteToConfigFile( const char* pathToConfigFile )
{
	// we can see if there is a config file, if so then see if they have a WINS server specified
	DBGLOG( "CSMBPlugin::WriteToConfigFile [%s]\n", pathToConfigFile );
	
    FILE		*sourceFP = NULL, *destFP = NULL;
    char		buf[kMaxSizeOfParam];
    Boolean		writtenWINS = false;
	Boolean		writtenWorkgroup = false;
	Boolean		writtenNetBIOSName = false;
	Boolean		writtenComment = false;
	Boolean		writtenNetBIOSTrackingComment = !mUseComputerNameTracking;
	Boolean		writeCodePage = (strcmp( kBrowsingConfFilePath, pathToConfigFile ) == 0);	// only update the browsing config's code page
	Boolean		writeUnixCharset = writeCodePage;
	Boolean		writeDisplayCharset = writeCodePage;
	
	sourceFP = fopen(kConfFilePath,"r+");
	
    if (sourceFP == NULL) 
	{
        DBGLOG( "CSMBPlugin::WriteToConfigFile, couldn't open conf file, copy temp to conf\n" );

		char		command[256];
		
		snprintf( command, sizeof(command), "/bin/cp %s %s\n", kTemplateConfFilePath, kConfFilePath );
		executecommand( command );

		sourceFP = fopen(kConfFilePath,"r+");
		
		if (sourceFP == NULL) 
			return;
    }
    
	if ( strcmp( kConfFilePath, pathToConfigFile ) == 0 )
		destFP = fopen( kTempConfFilePath, "w" );		// if we are writing to the config file, need to modify the temp and copy over
	else
	{
		destFP = fopen( pathToConfigFile, "w" );			// otherwise we'll just copy and modify straight into the new file
	}
	
	if ( !destFP )
	{
		fclose( sourceFP );
		return;
	}
	
	if ( !mLocalNodeString )
		writtenWorkgroup = true;
		
	while (fgets(buf,kMaxSizeOfParam,sourceFP) != NULL) 
	{
        char *pcKey = NULL;
        
        if (buf[0] == '\n' || buf[0] == '\0' || buf[0] == '#' || (writtenWorkgroup && writtenWINS && writtenNetBIOSName && writtenNetBIOSTrackingComment && !writeCodePage && !writeUnixCharset && !writeDisplayCharset) )
		{
			fputs( buf, destFP );
			continue;
		}
		
		if ( buf[0] == ';' )
		{
			if ( !writtenNetBIOSTrackingComment && strncmp( kUseComputerNameComment, buf, strlen(kUseComputerNameComment) ) == 0 )
				writtenNetBIOSTrackingComment = true;
			
			fputs( buf, destFP );
			continue;
		}
		
		if ( strstr( buf, "[homes]" ) || strstr( buf, "[public]" ) || strstr( buf, "[printers]" )  )
		{
			// ok, we've passed where this data should go, write out whatever we have left
			if ( writeUnixCharset )
			{
				WriteUnixCharsetToFile( destFP );
				writeUnixCharset = false;
			}
				
			if ( writeDisplayCharset )
			{
				WriteDisplayCharsetToFile( destFP );
				writeDisplayCharset = false;
			}
				
			if ( writeCodePage )
			{
				WriteCodePageToFile( destFP );
				writeCodePage = false;
			}
				
			if ( !writtenWorkgroup )
			{
				WriteWorkgroupToFile( destFP );
				writtenWorkgroup = true;
			}
			
			if ( !writtenWINS )
			{
				WriteWINSToFile( destFP );
				writtenWINS = true;
			}
			
			if ( !writtenNetBIOSTrackingComment )
			{
				WriteNetBIOSTrackingCommentToFile( destFP );
				writtenNetBIOSTrackingComment = true;
			}
			
			if ( !writtenNetBIOSName )
			{
				WriteNetBIOSNameToFile( destFP );
				writtenNetBIOSName = true;
			}
			
			if ( !writtenComment )
			{
				WriteCommentToFile( destFP );
				writtenComment = true;			
			}
			
			fputs( buf, destFP );			// now add the line we read
			
			continue;
		}
		
		if ( !writtenWINS )
			pcKey = strstr( buf, "wins server" );

        if ( pcKey )
		{
			WriteWINSToFile( destFP );
			writtenWINS = true;

			continue;
		}
		else if ( !writtenWorkgroup && (pcKey = strstr( buf, "workgroup" )) != NULL )
        {
			WriteWorkgroupToFile( destFP );
			writtenWorkgroup = true;

			continue;
		}
		else if ( !writtenNetBIOSTrackingComment && (pcKey = strstr( buf, kUseComputerNameComment )) != NULL )
        {
			WriteNetBIOSTrackingCommentToFile( destFP );
			writtenNetBIOSTrackingComment = true;

			continue;
		}
		else if ( !writtenNetBIOSName && (pcKey = strstr( buf, "netbios name" )) != NULL )
        {
			WriteNetBIOSNameToFile( destFP );
			writtenNetBIOSName = true;

			continue;
		}
		else if ( !writtenComment && (pcKey = strstr( buf, "server string" )) != NULL )
        {
			WriteCommentToFile( destFP );
			writtenComment = true;

			continue;
		}
		else if ( writeCodePage && (pcKey = strstr( buf, "dos charset" )) != NULL )
		{
			WriteCodePageToFile( destFP );
			writeCodePage = false;
		}
		else if ( writeUnixCharset && (pcKey = strstr( buf, "unix charset" )) != NULL )
		{
			WriteUnixCharsetToFile( destFP );
			writeUnixCharset = false;
		}
		else if ( writeDisplayCharset && (pcKey = strstr( buf, "display charset" )) != NULL )
		{
			WriteDisplayCharsetToFile( destFP );
			writeDisplayCharset = false;
		}
		else
			fputs( buf, destFP );			// now add the line we read		
    }

    fclose(sourceFP);
    fclose(destFP);

	if ( strcmp( kConfFilePath, pathToConfigFile ) == 0 )	// if we are writing to the config file, need to modify the temp and copy over
	{
		char		command[256];
		
		snprintf( command, sizeof(command), "/bin/mv %s %s\n", kTempConfFilePath, kConfFilePath );

		executecommand( command );
	}
}

void CSMBPlugin::CheckAndHandleIfConfigFileChanged( void )
{
	time_t					modTimeOfFile = 0;
	struct stat				statResult;
	int						statErr = stat( kConfFilePath, &statResult );
	
	if ( statErr != 0 )
	{
		ReadConfigFile();		// call this and it will copy the file from the template if it isn't there and try again.
		statErr = stat( kConfFilePath, &statResult );
	}
	
	// ok, let's stat the file		
	if ( statErr == 0 )
	{
		modTimeOfFile = statResult.st_mtimespec.tv_sec;
		if ( modTimeOfFile > mConfFileModTime )
		{
			// what we would like to do is have nmbd respond to a SIGHUP.  But until it does, we'll terminate it
			// and let it get relaunced via xinetd

			if ( mConfFileModTime > 0 )
			{
				ReadConfigFile();									// re-read the config file as it has changed on us
				signalProcessByName( kNMBDProcessName, SIGTERM );	// if this has changed since the last time we checked, signal nmbd
			}
			
			mConfFileModTime = modTimeOfFile;
		}
	}
}

#pragma mark -
Boolean CSMBPlugin::AreWeRunningOnXServer( void )
{
	return mRunningOnXServer;
}

sInt32 CSMBPlugin::DoPlugInCustomCall ( sDoPlugInCustomCall *inData )
{
	sInt32					siResult	= eDSNoErr;
	unsigned long			aRequest	= 0;
	unsigned long			bufLen			= 0;
	AuthorizationRef		authRef			= 0;
	AuthorizationItemSet   *resultRightSet = NULL;

	DBGLOG( "CSMBPlugin::DoPlugInCustomCall called\n" );
//seems that the client needs to have a tDirNodeReference 
//to make the custom call even though it will likely be non-dirnode specific related

	try
	{
		if ( inData == nil ) throw( (sInt32)eDSNullParameter );
		if ( mOpenRefTable == nil ) throw ( (sInt32)eDSNodeNotFound );
		
		const void*				dictionaryResult	= NULL;
		CNSLDirNodeRep*			nodeDirRep			= NULL;
		
		dictionaryResult = ::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );
		if( !dictionaryResult )
			DBGLOG( "CSMBPlugin::DoPlugInCustomCall called but we couldn't find the nodeDirRep!\n" );
	
		nodeDirRep = (CNSLDirNodeRep*)dictionaryResult;
	
		if ( nodeDirRep )
		{
			aRequest = inData->fInRequestCode;

			if ( aRequest != kReadSMBConfigData && aRequest != kReadSMBConfigXMLData && aRequest != kReadSMBConfigXMLDataSize )
			{
				if ( inData->fInRequestData == nil ) throw( (sInt32)eDSNullDataBuff );
				if ( inData->fInRequestData->fBufferData == nil ) throw( (sInt32)eDSEmptyBuffer );
		
				bufLen = inData->fInRequestData->fBufferLength;
				if ( bufLen < sizeof( AuthorizationExternalForm ) ) throw( (sInt32)eDSInvalidBuffFormat );

				siResult = AuthorizationCreateFromExternalForm((AuthorizationExternalForm *)inData->fInRequestData->fBufferData,
					&authRef);
				if (siResult != errAuthorizationSuccess)
				{
					throw( (sInt32)eDSPermissionError );
				}
	
				AuthorizationItem rights[] = { {"system.services.directory.configure", 0, 0, 0} };
				AuthorizationItemSet rightSet = { sizeof(rights)/ sizeof(*rights), rights };
			
				siResult = AuthorizationCopyRights(authRef, &rightSet, NULL,
					kAuthorizationFlagExtendRights, &resultRightSet);
				if (resultRightSet != NULL)
				{
					AuthorizationFreeItemSet(resultRightSet);
					resultRightSet = NULL;
				}
				if (siResult != errAuthorizationSuccess)
				{
					throw( (sInt32)eDSPermissionError );
				}
			}
			
			switch( aRequest )
			{
				case kReadSMBConfigXMLDataSize:
				case kReadSMBConfigXMLData:
				{
					// using the more extensible xml based config data
					// check config
					CheckAndHandleIfConfigFileChanged();
						
					siResult = FillOutCurrentStateWithXML( inData, (aRequest == kReadSMBConfigXMLDataSize) );

					if ( !(mState & kActive) || (mState & kInactive) )
					{
						ClearOutAllNodes();					// clear these out
						mNodeListIsCurrent = false;			// this is no longer current
						DBGLOG( "CSMBPlugin::DoPlugInCustomCall cleared out all our registered nodes as we are inactive\n" );
					}
				}
				break;
				
				case kWriteSMBConfigXMLData:
				{
					DBGLOG( "CSMBPlugin::DoPlugInCustomCall kWriteSMBConfigXMLData\n" );
					// using the more extensible xml based config data
					siResult = SaveNewStateFromXML( inData );
				}
				break;
				
				case kReadSMBConfigData:
				{
					DBGLOG( "CSMBPlugin::DoPlugInCustomCall kReadSMBConfigData\n" );

					// check config
					CheckAndHandleIfConfigFileChanged();
						
					siResult = FillOutCurrentState( inData );

					if ( !(mState & kActive) || (mState & kInactive) )
					{
						ClearOutAllNodes();					// clear these out
						mNodeListIsCurrent = false;			// this is no longer current
						DBGLOG( "CSMBPlugin::DoPlugInCustomCall cleared out all our registered nodes as we are inactive\n" );
					}
				}
				break;
					 
				case kWriteSMBConfigData:
				{
					DBGLOG( "CSMBPlugin::DoPlugInCustomCall kWriteSMBConfigData\n" );

					if ( AreWeRunningOnXServer() )
						siResult = eDSReadOnly;			// can't modify server
					else
						siResult = SaveNewState( inData );
				}
				break;
					
				default:
					break;
			}
		}
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}
	
	if (authRef != 0)
	{
		AuthorizationFree(authRef, 0);
		authRef = 0;
	}

	return( siResult );

}

sInt32 CSMBPlugin::HandleNetworkTransition( sHeader *inData )
{
    sInt32					siResult			= eDSNoErr;

	DBGLOG( "CSMBPlugin::HandleNetworkTransition called\n" );

	if ( mActivatedByNSL && IsActive() )
	{
		DBGLOG( "CSMBPlugin::HandleNetworkTransition cleaning up data and calling CNSLPlugin::HandleNetworkTransition\n" );

		OurLMBDiscoverer()->ClearLMBCache();				// no longer valid
		OurLMBDiscoverer()->ResetOurBroadcastAddress();	
		OurLMBDiscoverer()->ClearBadLMBList();
		
		ResetBroadcastThrottle();
		
		mInitialSearch = true;	// starting from scratch
		mNeedFreshLookup = true;
		mCurrentSearchCanceled = true;
		ZeroLastNodeLookupStartTime();		// force this to start again
		mTimeBetweenLookups = kInitialTimeBetweenNodeLookups;
		
		siResult = CNSLPlugin::HandleNetworkTransition( inData );
	}
	else
		DBGLOG( "CSMBPlugin::HandleNetworkTransition skipped, mActivatedByNSL: %d, IsActive: %d\n", mActivatedByNSL, IsActive() );

    return ( siResult );
}

#pragma mark -
#define kMaxTimeToWait	10	// in seconds
sInt32 CSMBPlugin::FillOutCurrentStateWithXML( sDoPlugInCustomCall *inData, Boolean sizeOnly )
{
	sInt32					siResult					= eDSNoErr;
	CFMutableDictionaryRef	currentStateRef				= CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	CFStringRef				winsServerRef				= NULL;
	CFStringRef				workgroupRef				= NULL;
	CFStringRef				newBIOSNameRef				= NULL;
	CFStringRef				newCommentRef				= NULL;
	CFRange					aRange;
	CFDataRef   			xmlData						= NULL;
	
	try
	{
		if ( !currentStateRef )
			throw( eMemoryError );
		
		if ( mLocalNodeString )
		{
			DBGLOG( "CSMBPlugin::FillOutCurrentStateWithXML: mLocalNodeString is %s\n", mLocalNodeString );
			workgroupRef = CFStringCreateWithCString( NULL, mLocalNodeString, kCFStringEncodingUTF8 );
			
			if ( workgroupRef )
				CFDictionaryAddValue( currentStateRef, CFSTR(kDS1AttrLocation), workgroupRef );
		}
		
		if ( mWINSServer )
		{
			DBGLOG( "CSMBPlugin::FillOutCurrentStateWithXML: mWINSServer is %s\n", mWINSServer );
			winsServerRef = CFStringCreateWithCString( NULL, mWINSServer, kCFStringEncodingUTF8 );
			
			if ( winsServerRef )
				CFDictionaryAddValue( currentStateRef, CFSTR(kDSStdRecordTypeServer), winsServerRef );
		}
		
		if ( mNetBIOSName )
		{
			DBGLOG( "CSMBPlugin::FillOutCurrentStateWithXML: mNetBIOSName is %s\n", mNetBIOSName );
			newBIOSNameRef = CFStringCreateWithCString( NULL, mNetBIOSName, kCFStringEncodingUTF8 );
			
			if ( newBIOSNameRef )
				CFDictionaryAddValue( currentStateRef, CFSTR(kDS1AttrLocation), newBIOSNameRef );
		}
		
		if ( mCommentFieldString )
		{
			DBGLOG( "CSMBPlugin::FillOutCurrentStateWithXML: mCommentFieldString is %s\n", mCommentFieldString );
			newCommentRef = CFStringCreateWithCString( NULL, mCommentFieldString, kCFStringEncodingUTF8 );
			
			if ( newCommentRef )
				CFDictionaryAddValue( currentStateRef, CFSTR(kDS1AttrComment), newCommentRef );
		}
		
		CFDictionaryAddValue( currentStateRef, CFSTR(kNativeSMBUseComputerName), (mUseComputerNameTracking)?CFSTR("YES"):CFSTR("NO") );			
		
		// are we running as a server?
		if ( AreWeRunningOnXServer() )
		{
			CFDictionaryAddValue( currentStateRef, CFSTR(kDS1AttrReadOnlyNode), CFSTR("YES") );
		}
		else
		{
			CFArrayRef		listOfWorkgroups = CreateListOfWorkgroups();
			
			if ( listOfWorkgroups )
			{
				CFDictionaryAddValue( currentStateRef, CFSTR(kDSNAttrSubNodes), listOfWorkgroups );
				CFRelease( listOfWorkgroups );
			}
		}
		
		//convert the dict into a XML blob
		xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, currentStateRef );

		if (xmlData != 0)
		{
			if ( sizeOnly )		// they just want to know how much to allocate
			{
				CFIndex		xmlSize = CFDataGetLength(xmlData);
				
				inData->fOutRequestResponse->fBufferLength = sizeof(xmlSize);
				memcpy( inData->fOutRequestResponse->fBufferData, &xmlSize, sizeof(xmlSize) );
			}
			else
			{
				aRange.location = 0;
				aRange.length = CFDataGetLength(xmlData);
				if ( inData->fOutRequestResponse->fBufferSize < (unsigned int)aRange.length ) throw( (sInt32)eDSBufferTooSmall );
				CFDataGetBytes( xmlData, aRange, (UInt8*)(inData->fOutRequestResponse->fBufferData) );
				inData->fOutRequestResponse->fBufferLength = aRange.length;
			}
		}
	}

	catch ( sInt32 err )
	{
		DBGLOG( "CSMBPlugin::FillOutCurrentState: Caught error: %ld\n", err );
		siResult = err;
	}
	
	if ( currentStateRef )
		CFRelease( currentStateRef );
	
	if ( winsServerRef )
		CFRelease( winsServerRef );
	
	if ( workgroupRef )
		CFRelease( workgroupRef );
		
	if ( newBIOSNameRef )
		CFRelease( newBIOSNameRef );
		
	if ( newCommentRef )
		CFRelease( newCommentRef );
		
	if ( xmlData )
		CFRelease( xmlData );

	return siResult;
}

sInt32 CSMBPlugin::SaveNewStateFromXML( sDoPlugInCustomCall *inData )
{
	sInt32					siResult					= eDSNoErr;
	sInt32					xmlDataLength				= 0;
	CFDataRef   			xmlData						= NULL;
	CFDictionaryRef			newStateRef					= NULL;
	CFStringRef				workgroupRef				= NULL;
	CFStringRef				winsServerRef				= NULL;
	CFStringRef				netBIOSNameRef				= NULL;
	CFStringRef				useComputerNameTrackingRef	= NULL;
	CFStringRef				errorString					= NULL;
	Boolean					configChanged				= false;
	char*					newWorkgroupString			= NULL;
	char*					newWINSServer				= NULL;
	char*					newNetBIOSName				= NULL;
	char*					newComment					= NULL;
	
	DBGLOG( "CSMBPlugin::SaveNewStateFromXML called\n" );
	
	xmlDataLength = (sInt32) inData->fInRequestData->fBufferLength - sizeof( AuthorizationExternalForm );
	
	if ( xmlDataLength <= 0 )
		return (sInt32)eDSInvalidBuffFormat;
	
	xmlData = CFDataCreate(NULL,(UInt8 *)(inData->fInRequestData->fBufferData + sizeof( AuthorizationExternalForm )), xmlDataLength);
	
	if ( !xmlData )
	{
		DBGLOG( "CSMBPlugin::SaveNewStateFromXML, couldn't create xmlData from buffer!!\n" );
		siResult = (sInt32)eDSInvalidBuffFormat;
	}
	else
		newStateRef = (CFDictionaryRef)CFPropertyListCreateFromXMLData(	NULL,
																		xmlData,
																		kCFPropertyListImmutable,
																		&errorString);
																		
	if ( newStateRef && CFGetTypeID( newStateRef ) != CFDictionaryGetTypeID() )
	{
		DBGLOG( "CSMBPlugin::SaveNewStateFromXML, XML Data wasn't a CFDictionary!\n" );
		siResult = (sInt32)eDSInvalidBuffFormat;
	}
	else
	{
		workgroupRef = (CFStringRef)CFDictionaryGetValue( newStateRef, CFSTR(kDS1AttrLocation) );
		
		if ( workgroupRef && CFGetTypeID( workgroupRef ) == CFStringGetTypeID() )
		{
			newWorkgroupString = (char*)malloc(kMaxWorkgroupLen);
			CFStringGetCString( workgroupRef, newWorkgroupString, kMaxWorkgroupLen, kCFStringEncodingUTF8 );
		}
		else if ( !workgroupRef )
		{
			DBGLOG( "CSMBPlugin::SaveNewStateFromXML, we received no domain name so we are basically being turned off\n" );
			if ( mLocalNodeString )
				configChanged = true;
		}
		else
		{
			DBGLOG( "CSMBPlugin::SaveNewStateFromXML, the domain name is of the wrong type! (%ld)\n", CFGetTypeID( workgroupRef ) );
			siResult = (sInt32)eDSInvalidBuffFormat;
		}
		
		winsServerRef = (CFStringRef)CFDictionaryGetValue( newStateRef, CFSTR(kDSStdRecordTypeServer) );

		if ( winsServerRef && CFGetTypeID( winsServerRef ) != CFStringGetTypeID() )
		{
			DBGLOG( "CSMBPlugin::SaveNewStateFromXML, the wins server is of the wrong type! (%ld)\n", CFGetTypeID( winsServerRef ) );
			siResult = (sInt32)eDSInvalidBuffFormat;
		}
		else if ( winsServerRef )
		{
			long	winsServerLen = CFStringGetLength(winsServerRef)+1;
			
			newWINSServer = (char*)malloc(winsServerLen);
			CFStringGetCString( winsServerRef, newWINSServer, winsServerLen, kCFStringEncodingASCII );	// this is an IPAddress or hostname
		}
		
		useComputerNameTrackingRef = (CFStringRef)CFDictionaryGetValue( newStateRef, CFSTR(kNativeSMBUseComputerName) );

		if ( useComputerNameTrackingRef && CFGetTypeID( netBIOSNameRef ) != CFStringGetTypeID() )
		{
			DBGLOG( "CSMBPlugin::SaveNewStateFromXML, kNativeSMBUseComputerName is of the wrong type! (%ld)\n", CFGetTypeID( useComputerNameTrackingRef ) );
			siResult = (sInt32)eDSInvalidBuffFormat;
		}
		else if ( useComputerNameTrackingRef )
		{
			Boolean useComputerNameTracking = CFStringCompare( useComputerNameTrackingRef, CFSTR("YES"), 0 ) == kCFCompareEqualTo;
			
			if ( useComputerNameTracking != mUseComputerNameTracking )
				configChanged = true;
				
			mUseComputerNameTracking = useComputerNameTracking;
		}
		
		if ( mUseComputerNameTracking )
		{
			netBIOSNameRef = CreateNetBIOSNameFromComputerName();

			if ( netBIOSNameRef )
			{
				newNetBIOSName = (char*)malloc(kMaxNetBIOSNameLen);
				CFStringGetCString( netBIOSNameRef, newNetBIOSName, kMaxNetBIOSNameLen, kCFStringEncodingUTF8 );
				
				CFRelease( netBIOSNameRef );
				netBIOSNameRef = NULL;
			}

			CFStringEncoding	encoding;
			CFStringRef			computerNameRef = SCDynamicStoreCopyComputerName(NULL, &encoding);
		
			if ( computerNameRef )
			{
				newComment = (char*)malloc(CFStringGetMaximumSizeForEncoding(CFStringGetLength(computerNameRef), encoding)+1);
				 
				CFStringGetCString( computerNameRef, newComment, CFStringGetMaximumSizeForEncoding(CFStringGetLength(computerNameRef), encoding)+1, encoding );
				
				CFRelease( computerNameRef );
				computerNameRef = NULL;
			}
		}
		else
		{
			netBIOSNameRef = (CFStringRef)CFDictionaryGetValue( newStateRef, CFSTR(kDSStdRecordTypeServer) );
	
			if ( netBIOSNameRef && CFGetTypeID( netBIOSNameRef ) != CFStringGetTypeID() )
			{
				DBGLOG( "CSMBPlugin::SaveNewStateFromXML, the NetBIOS name is of the wrong type! (%ld)\n", CFGetTypeID( netBIOSNameRef ) );
				siResult = (sInt32)eDSInvalidBuffFormat;
			}
			else if ( netBIOSNameRef )
			{
				newNetBIOSName = (char*)malloc(kMaxNetBIOSNameLen);
				CFStringGetCString( netBIOSNameRef, newNetBIOSName, kMaxNetBIOSNameLen, kCFStringEncodingUTF8 );
			}
		}
	}
	
	if ( mLocalNodeString && newWorkgroupString )
	{
		if ( strcmp( mLocalNodeString, newWorkgroupString ) != 0 )
		{
			free( mLocalNodeString );
			mLocalNodeString = newWorkgroupString;
			
			configChanged = true;
		}
		else
		{
			free( newWorkgroupString );
			newWorkgroupString = NULL;
		}
	}
	else if ( newWorkgroupString )
	{
		mLocalNodeString = newWorkgroupString;
			
		configChanged = true;
	}
		
	if ( mWINSServer && newWINSServer )
	{
		if ( strcmp( mWINSServer, newWINSServer ) != 0 )
		{
			free( mWINSServer );
			mWINSServer = newWINSServer;
			
			configChanged = true;
		}
		else
		{
			free( newWINSServer );
			newWINSServer = NULL;
		}
	}
	else if ( newWINSServer )
	{
		mWINSServer = newWINSServer;
			
		configChanged = true;
	}
		
	if ( mNetBIOSName && newNetBIOSName )
	{
		if ( strcmp( mNetBIOSName, newNetBIOSName ) != 0 )
		{
			free( mNetBIOSName );
			mNetBIOSName = newNetBIOSName;
			
			configChanged = true;
		}
		else
		{
			free( newNetBIOSName );
			newNetBIOSName = NULL;
		}
	}
	else if ( newNetBIOSName )
	{
		mNetBIOSName = newNetBIOSName;
			
		configChanged = true;
	}
		
	if ( mCommentFieldString && newComment )
	{
		if ( strcmp( mCommentFieldString, newComment ) != 0 )
		{
			free( mCommentFieldString );
			mCommentFieldString = newComment;
			
			configChanged = true;
		}
		else
		{
			free( newComment );
			newComment = NULL;
		}
	}
	else if ( newComment )
	{
		mCommentFieldString = newComment;
			
		configChanged = true;
	}
		
	if ( xmlData )
		CFRelease( xmlData );
		
	if ( newStateRef )
		CFRelease( newStateRef );

	if ( configChanged )
	{
		WriteToConfigFile(kConfFilePath);
		WriteToConfigFile(kBrowsingConfFilePath);

		if ( !(mState & kActive) || (mState & kInactive) )
		{
			ClearOutAllNodes();					// clear these out
			mNodeListIsCurrent = false;			// this is no longer current
			DBGLOG( "CSMBPlugin::DoPlugInCustomCall cleared out all our registered nodes after writing config changes as we are inactive\n" );
		}
		
		if ( (mState & kActive) && mActivatedByNSL )
		{
			DBGLOG( "CSMBPlugin::DoPlugInCustomCall (mState & kActive) && mActivatedByNSL so starting a new fresh node lookup.\n" );
			mNeedFreshLookup = true;
			OurLMBDiscoverer()->ClearLMBCache();
			StartNodeLookup();					// and then start all over
		}
		
		CheckAndHandleIfConfigFileChanged();
	}

	return siResult;
}

sInt32 CSMBPlugin::FillOutCurrentState( sDoPlugInCustomCall *inData )
{
	sInt32					siResult	= eDSNoErr;
	UInt32					workgroupDataLen = 0;
	void*					workgroupData = NULL;

//seems that the client needs to have a tDirNodeReference 
//to make the custom call even though it will likely be non-dirnode specific related

	try
	{
		if ( !mNodeListIsCurrent && !mNodeSearchInProgress )
		{
			StartNodeLookup();
			
			int	i = 0;
			while ( !mNodeListIsCurrent && i++ < kMaxTimeToWait )
				SmartSleep(1*USEC_PER_SEC);
		}
			
		workgroupDataLen = 0;
		workgroupData = MakeDataBufferOfWorkgroups( &workgroupDataLen );
		
		char*		curPtr = inData->fOutRequestResponse->fBufferData;
		UInt32		dataLen = 4;
		
		 if (mLocalNodeString)
			dataLen += strlen(mLocalNodeString);
			
		dataLen += 4;
		
		 if (mWINSServer)
			dataLen += strlen(mWINSServer);
		
		dataLen += workgroupDataLen;
		
		if ( inData->fOutRequestResponse == nil ) throw( (sInt32)eDSNullDataBuff );
		if ( inData->fOutRequestResponse->fBufferData == nil ) throw( (sInt32)eDSEmptyBuffer );

		if ( inData->fOutRequestResponse->fBufferSize < (unsigned int)dataLen ) 
			throw( (sInt32)eDSBufferTooSmall );

		if ( mLocalNodeString )
		{
			*((UInt32*)curPtr) = strlen(mLocalNodeString);
			curPtr += 4;
			memcpy( curPtr, mLocalNodeString, strlen(mLocalNodeString) );
			curPtr += strlen(mLocalNodeString);
		}
		else
		{
			*((UInt32*)curPtr) = 0;
			curPtr += 4;
		}
		
		if ( mWINSServer )
		{
			*((UInt32*)curPtr) = strlen(mWINSServer);
			curPtr += 4;
			memcpy( curPtr, mWINSServer, strlen(mWINSServer) );
			curPtr += strlen(mWINSServer);
		}
		else
		{
			*((UInt32*)curPtr) = 0;
			curPtr += 4;
		}
		
		memcpy( curPtr, workgroupData, workgroupDataLen );
		inData->fOutRequestResponse->fBufferLength = dataLen;
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	if ( workgroupData )
	{
		free( workgroupData );
		workgroupData = NULL;
	}
	
	return siResult;
}

sInt32 CSMBPlugin::SaveNewState( sDoPlugInCustomCall *inData )
{
	sInt32					siResult					= eDSNoErr;
	sInt32					dataLength					= (sInt32) inData->fInRequestData->fBufferLength - sizeof( AuthorizationExternalForm );
	Boolean					configChanged				= false;
	
	if ( dataLength <= 0 ) throw( (sInt32)eDSInvalidBuffFormat );
	
	UInt8*		curPtr =(UInt8 *)(inData->fInRequestData->fBufferData + sizeof( AuthorizationExternalForm ));
	UInt32		curDataLen;
	char*		newWorkgroupString = NULL;
	char*		newWINSServer = NULL;
	
	curDataLen = *((UInt32*)curPtr);
	curPtr += 4;
	
	if ( curDataLen > 0 )
	{
		newWorkgroupString = (char*)malloc(curDataLen+1);
		memcpy( newWorkgroupString, curPtr, curDataLen );
		newWorkgroupString[curDataLen] = '\0';

		curPtr += curDataLen;
	}
	
	curDataLen = *((UInt32*)curPtr);
	curPtr += 4;

	if ( curDataLen > 0 )
	{
		newWINSServer = (char*)malloc(curDataLen+1);
		memcpy( newWINSServer, curPtr, curDataLen );
		newWINSServer[curDataLen] = '\0';
	}

	if ( mLocalNodeString && newWorkgroupString )
	{
		if ( strcmp( mLocalNodeString, newWorkgroupString ) != 0 )
		{
			free( mLocalNodeString );
			mLocalNodeString = newWorkgroupString;
			
			configChanged = true;
		} else {
			// if we are the same string, we need to free it
			free( newWorkgroupString );
		}
	}
	else if ( newWorkgroupString )
	{
		// we shouldn't be called if we don't have a mLocalNodeString!
		mLocalNodeString = newWorkgroupString;
			
		configChanged = true;
	}
	
	if ( mWINSServer && newWINSServer )
	{
		if ( strcmp( mWINSServer, newWINSServer ) != 0 )
		{
			free( mWINSServer );
			mWINSServer = newWINSServer;
			mLMBDiscoverer->SetWinsServer(mWINSServer);
			
			configChanged = true;
		}
	}
	else if ( newWINSServer )
	{
		mWINSServer = newWINSServer;
		mLMBDiscoverer->SetWinsServer(mWINSServer);
			
		configChanged = true;
	}
	else if ( mWINSServer )
	{
		free( mWINSServer );
		mWINSServer = NULL;
		mLMBDiscoverer->SetWinsServer(mWINSServer);

		configChanged = true;
	}

	if ( configChanged )
	{
		WriteToConfigFile(kConfFilePath);
		WriteToConfigFile(kBrowsingConfFilePath);

		if ( !(mState & kActive) || (mState & kInactive) )
		{
			ClearOutAllNodes();					// clear these out
			mNodeListIsCurrent = false;			// this is no longer current
			DBGLOG( "CSMBPlugin::DoPlugInCustomCall cleared out all our registered nodes after writing config changes as we are inactive\n" );
		}
		
		if ( (mState & kActive) && mActivatedByNSL )
		{
			DBGLOG( "CSMBPlugin::DoPlugInCustomCall (mState & kActive) && mActivatedByNSL so starting a new fresh node lookup.\n" );
			mNeedFreshLookup = true;
			OurLMBDiscoverer()->ClearLMBCache();
			StartNodeLookup();					// and then start all over
		}
		
		CheckAndHandleIfConfigFileChanged();
	}
	
	return siResult;
}

typedef char	SMBNodeName[16];
typedef struct NSLPackedNodeList {
	UInt32						fBufLen;
	UInt32						fNumNodes;
	SMBNodeName					fNodeData[];
} NSLPackedNodeList;

void SMBNodeHandlerFunction(const void *inKey, const void *inValue, void *inContext);
void SMBNodeHandlerFunction(const void *inKey, const void *inValue, void *inContext)
{
	DBGLOG( "SMBNodeHandlerFunction\n" );
	CFShow( inKey );

    NodeData*					curNodeData = (NodeData*)inValue;
    NSLNodeHandlerContext*		context = (NSLNodeHandlerContext*)inContext;

	NSLPackedNodeList*	nodeListData = (NSLPackedNodeList*)context->fDataPtr;
	SMBNodeName	curNodeName = {0};
	
	CFStringGetCString( curNodeData->fNodeName, curNodeName, sizeof(curNodeName), kCFStringEncodingUTF8 );
	
	if ( !nodeListData )
	{
		DBGLOG( "SMBNodeHandlerFunction first time called, creating nodeListData from scratch\n" );
		nodeListData = (NSLPackedNodeList*)malloc( sizeof(NSLPackedNodeList) + 4 + sizeof(curNodeName) );
		nodeListData->fBufLen = sizeof(NSLPackedNodeList) + 4 + sizeof(curNodeName);
		nodeListData->fNumNodes = 1;
		memcpy( nodeListData->fNodeData, curNodeName, sizeof(curNodeName) );
	}
	else
	{
		DBGLOG( "SMBNodeHandlerFunction first time called, need to append nodeListData\n" );
		NSLPackedNodeList*	oldNodeListData = nodeListData;
		
		nodeListData = (NSLPackedNodeList*)malloc( oldNodeListData->fBufLen + 4 + sizeof(curNodeName) );
		nodeListData->fBufLen = oldNodeListData->fBufLen + 4 + sizeof(curNodeName);
		nodeListData->fNumNodes = oldNodeListData->fNumNodes + 1;
		memcpy( nodeListData->fNodeData, oldNodeListData->fNodeData, oldNodeListData->fNumNodes * sizeof(curNodeName) );
		memcpy( &nodeListData->fNodeData[nodeListData->fNumNodes - 1], curNodeName, sizeof(curNodeName) );
		
		free( oldNodeListData );
	}
	
	context->fDataPtr = nodeListData;
}

void SMBNodeListCreationFunction(const void *inKey, const void *inValue, void *inContext);
void SMBNodeListCreationFunction(const void *inKey, const void *inValue, void *inContext)
{
	DBGLOG( "SMBNodeListCreationFunction\n" );
	CFShow( inKey );

    NodeData*					curNodeData = (NodeData*)inValue;
    CFMutableArrayRef			context = (CFMutableArrayRef)inContext;
	CFStringRef					workgroupName = CFStringCreateWithSubstring( NULL, curNodeData->fNodeName, CFRangeMake(4,CFStringGetLength(curNodeData->fNodeName)-4) );

	CFArrayAppendValue( context, workgroupName );
	
	CFRelease( workgroupName );
}

CFArrayRef	CSMBPlugin::CreateListOfWorkgroups( void )
{
	DBGLOG( "CSMBPlugin::CreateListOfWorkgroups called\n" );
	// need to fill out all the currently found workgroups
	CFMutableArrayRef		listToReturn = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	
    LockPublishedNodes();
	CFDictionaryApplyFunction( mPublishedNodes, SMBNodeListCreationFunction, listToReturn );
    UnlockPublishedNodes();
	
	return listToReturn;
}

void* CSMBPlugin::MakeDataBufferOfWorkgroups( UInt32* dataLen )
{
	DBGLOG( "CSMBPlugin::MakeDataBufferOfWorkgroups called\n" );
	// need to fill out all the currently found workgroup data
	NSLNodeHandlerContext	context;
	
	context.fDictionary = mPublishedNodes;
	context.fDataPtr = NULL;
	
    LockPublishedNodes();
	CFDictionaryApplyFunction( mPublishedNodes, SMBNodeHandlerFunction, &context );
    UnlockPublishedNodes();
	
	if ( context.fDataPtr )
		*dataLen = ((NSLPackedNodeList*)context.fDataPtr)->fBufLen;
	
	return context.fDataPtr;
}

CFStringRef CSMBPlugin::GetBundleIdentifier( void )
{
    return gBundleIdentifier;
}

// this is used for top of the node's path "SMB"
const char*	CSMBPlugin::GetProtocolPrefixString( void )
{		
    return gProtocolPrefixString;
}

// this maps to the group we belong to  (i.e. WORKGROUP)
const char*	CSMBPlugin::GetLocalNodeString( void )
{		
    return mLocalNodeString;
}


Boolean CSMBPlugin::IsLocalNode( const char *inNode )
{
    Boolean result = false;
    
    if ( mLocalNodeString )
    {
        result = ( strcmp( inNode, mLocalNodeString ) == 0 );
    }
    
    return result;
}

void CSMBPlugin::NodeLookupIsCurrent( void )
{
	LockNodeState();
	mNodeListIsCurrent = true;
	mNodeSearchInProgress = false;

	if ( mCurrentSearchCanceled )
		mInitialSearch = true;		// if this search was canceled, we want to start again
	else
		mInitialSearch = false;

	UnLockNodeState();
}

UInt32 CSMBPlugin::GetTimeBetweenNodeLookups( void )
{
	if ( mTimeBetweenLookups < kOncePerDay )	// are we still in a situation where we haven't found any LMBs?
	{
		CFDictionaryRef		knownLMBDictionary = OurLMBDiscoverer()->GetAllKnownLMBs();
		
		if ( knownLMBDictionary && CFDictionaryGetCount( knownLMBDictionary ) > 0 )
			mTimeBetweenLookups = kOncePerDay;
		else
			mTimeBetweenLookups *= 2;			// we still haven't found any LMBs, back off node discovery
	}
	else if ( mTimeBetweenLookups > kOncePerDay )
		mTimeBetweenLookups = kOncePerDay;
	
	return mTimeBetweenLookups;
}

void CSMBPlugin::NewNodeLookup( void )
{
	DBGLOG( "CSMBPlugin::NewNodeLookup\n" );
	
	LockNodeState();
	if ( !mNodeSearchInProgress )
	{
		CheckAndHandleIfConfigFileChanged();
		
		mNodeListIsCurrent = false;
		mNodeSearchInProgress = true;
		mNeedFreshLookup = false;
		mCurrentSearchCanceled = false;
		// First add our local scope
		CFStringRef		nodeString = CFStringCreateWithCString( NULL, GetLocalNodeString(), NSLGetSystemEncoding() );
		
		if ( nodeString )
		{
			AddWorkgroup( nodeString );		// always register the default registration node
			CFRelease( nodeString );
		}
		
		if ( sNMBLookupToolIsAvailable )
		{
			CSMBNodeLookupThread* newLookup = new CSMBNodeLookupThread( this );
			
			newLookup->Resume();
		}
		
		if ( !sNMBLookupToolIsAvailable )
			DBGLOG( "CSMBPlugin::NewNodeLookup, ignoring as the SMB library isn't available\n" );
	}
	else if ( mNeedFreshLookup )
	{
		DBGLOG( "CSMBPlugin::NewNodeLookup, we need a fresh lookup, but one is still in progress\n" );
		ZeroLastNodeLookupStartTime();		// force this to start again
	}
	
	UnLockNodeState();
}

void CSMBPlugin::NewServiceLookup( char* serviceType, CNSLDirNodeRep* nodeDirRep )
{
	DBGLOG( "CSMBPlugin::NewServiceLookup\n" );

    if ( sNMBLookupToolIsAvailable )
    {
        if ( serviceType && strcmp( serviceType, kServiceTypeString ) == 0 )
        {
			CheckAndHandleIfConfigFileChanged();
			
			CFStringRef	workgroupRef = nodeDirRep->GetNodeName();
			
			if ( !workgroupRef )
			{
				DBGLOG( "CSMBPlugin::NewServiceLookup failed to get workgroup ref from node name!\n" );
				return;
			}
			
			if ( !OKToDoServiceLookupInWorkgroup( workgroupRef ) )
			{
				DBGLOG( "CSMBPlugin::NewServiceLookup skipping lookup as we are throttling lookups\n" );
				return;
			}
			CFArrayRef listOfLMBs = OurLMBDiscoverer()->CopyBroadcastResultsForLMB( workgroupRef );
			
			if ( listOfLMBs || !GetWinsServer() )
			{
				char	workgroup[256];
				CFStringGetCString( workgroupRef, workgroup, sizeof(workgroup), kCFStringEncodingUTF8 );
				DBGLOG( "CSMBPlugin::NewServiceLookup doing lookup on %ld LMBs responsible for %s\n", (listOfLMBs)?CFArrayGetCount(listOfLMBs):0, workgroup );

				CSMBServiceLookupThread* newLookup = new CSMBServiceLookupThread( this, serviceType, nodeDirRep, listOfLMBs, (mUseWINSURL)?GetWinsServer():NULL );
				
						// if we have too many threads running, just queue this search object and run it later
						if ( OKToStartNewSearch() )
							newLookup->Resume();
						else
							QueueNewSearch( newLookup );
				
				if ( listOfLMBs )
					CFRelease( listOfLMBs ); // release the list now that we're done with it
			}
			else if ( GetWinsServer() )	// if we have a WINS server, go ahead and try with a cached name, this may get resolved properly and not be on our subnet
			{
				CFStringRef	cachedLMB = OurLMBDiscoverer()->CopyOfCachedLMBForWorkgroup( nodeDirRep->GetNodeName() );

				if ( cachedLMB )
				{
					CFArrayRef listOfLMBs = CFArrayCreate( NULL, (const void**)(&cachedLMB), 1, &kCFTypeArrayCallBacks );
					
					if ( listOfLMBs )
					{
						CSMBServiceLookupThread* newLookup = new CSMBServiceLookupThread( this, serviceType, nodeDirRep, listOfLMBs, (mUseWINSURL)?GetWinsServer():NULL );
			
					// if we have too many threads running, just queue this search object and run it later
					if ( OKToStartNewSearch() )
						newLookup->Resume();
					else
						QueueNewSearch( newLookup );
						
						CFRelease( listOfLMBs );
					}
					
					CFRelease( cachedLMB );
				}
				else
					DBGLOG( "CSMBPlugin::NewServiceLookup, no cached LMB\n" );
			}
        }
        else if ( serviceType )
            DBGLOG( "CSMBPlugin::NewServiceLookup skipping as we don't support lookups on type:%s\n", serviceType );
    }
}

Boolean CSMBPlugin::OKToOpenUnPublishedNode( const char* parentNodeName )
{
	return false;
}

#define kTimeToWaitBetweenFailedBroadcasts	1*60		// one minute

void CSMBPlugin::ResetBroadcastThrottle( void )
{
	LockBroadcastThrottler();

	if ( mBroadcastThrottler )
		CFDictionaryRemoveAllValues( mBroadcastThrottler );
	
	UnLockBroadcastThrottler();
}

Boolean CSMBPlugin::OKToDoServiceLookupInWorkgroup( CFStringRef workgroupRef )
{
	Boolean		okToDoServiceLookup = true;
	
	LockBroadcastThrottler();

	if ( mBroadcastThrottler )
	{
		CFAbsoluteTime	timeToWaitForNextBroadcastCF = 0;
		CFNumberRef		timeToWaitForNextBroadcast = (CFNumberRef)CFDictionaryGetValue( mBroadcastThrottler, workgroupRef );
		
		if ( timeToWaitForNextBroadcast && CFNumberGetValue( timeToWaitForNextBroadcast, kCFNumberDoubleType, &timeToWaitForNextBroadcastCF ) )
		{
			if ( timeToWaitForNextBroadcastCF < CFAbsoluteTimeGetCurrent() )
			{
				// we've past the time limit, go ahead and remove this
				CFDictionaryRemoveValue( mBroadcastThrottler, workgroupRef );
			}
			else
			{
				DBGLOG( "CSMBPlugin::OKToDoServiceLookupInWorkgroup returning false for lookup as it isn't time yet\n" );
				okToDoServiceLookup = false;
			}
		}
	}
	
	UnLockBroadcastThrottler();
	
	return okToDoServiceLookup;
}

void CSMBPlugin::BroadcastServiceLookupFailedInWorkgroup( CFStringRef workgroupRef )
{
	// if we failed to find any servers in this workgroup, don't try again for at least a minute to prevent spamming
	
	LockBroadcastThrottler();
	
	if ( !mBroadcastThrottler )
		mBroadcastThrottler = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

	CFAbsoluteTime	timeToWaitTill = CFAbsoluteTimeGetCurrent()+kTimeToWaitBetweenFailedBroadcasts;
	CFNumberRef		timeToWaitForNextBroadcast = CFNumberCreate( NULL, kCFNumberDoubleType, &timeToWaitTill );
	
	if ( timeToWaitForNextBroadcast )
	{
		CFDictionarySetValue( mBroadcastThrottler, workgroupRef, timeToWaitForNextBroadcast );
		CFRelease( timeToWaitForNextBroadcast );
	}
	
	UnLockBroadcastThrottler();
}

void CSMBPlugin::BroadcastServiceLookupSucceededInWorkgroup( CFStringRef workgroupRef )
{
	LockBroadcastThrottler();

	if ( mBroadcastThrottler && CFDictionaryContainsValue( mBroadcastThrottler, workgroupRef ) )
		CFDictionaryRemoveValue( mBroadcastThrottler, workgroupRef );
	
	UnLockBroadcastThrottler();
}


#pragma mark -
void CSMBPlugin::ClearLMBForWorkgroup( CFStringRef workgroupRef, CFStringRef lmbNameRef )
{
	OurLMBDiscoverer()->ClearLMBForWorkgroup( workgroupRef, lmbNameRef );
}

#pragma mark -
CFStringRef	CreateNetBIOSNameFromComputerName( void )
{
	CFStringEncoding	encoding;
	CFStringRef			netBIOSName = NULL;
	CFStringRef			computerNameRef = SCDynamicStoreCopyComputerName(NULL, &encoding);

	if ( computerNameRef )
	{
		netBIOSName = CreateRFC1034HostLabelFromUTF8Name( computerNameRef, kMaxNetBIOSNameLen-1 );
		
		CFRelease( computerNameRef );
	}
	
	return netBIOSName;
}

/*-----------------------------------------------------------------------------
 *	signalProcessByName
 *	Signal asked process.
 *  Taken from slapconfig's utilities.c
 *---------------------------------------------------------------------------*/
int signalProcessByName(const char *name, int signal) 
{
	int			mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
	size_t		buf_size;
	int			result;

	errno = 0;
	result = sysctl(mib, 4, NULL, &buf_size, NULL, 0);
	if (result >= 0) 
	{
		struct kinfo_proc	*processes = NULL;
		int					i,nb_entries;
		nb_entries = buf_size / sizeof(struct kinfo_proc);
		processes = (struct kinfo_proc*) malloc(buf_size);
		if (processes != NULL) 
		{
			result = sysctl(mib, 4, processes, &buf_size, NULL, 0);
			if (result >= 0) 
			{
				for (i = 0; i < nb_entries; i++) 
				{
					if (processes[i].kp_proc.p_comm == NULL ||
						strcmp(processes[i].kp_proc.p_comm, name) != 0) continue;
					(void) kill(processes[i].kp_proc.p_pid, signal);
				}
			}
			free(processes);
		}
	}
	return errno;
}

