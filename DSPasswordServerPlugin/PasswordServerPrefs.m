/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#import <sys/param.h>
#import <sys/stat.h>
#import "PasswordServerPrefs.h"
#import "PSUtilitiesDefs.h"

// ----------------------------------------------------------------------------------------
//  pwsf_SetSASLPluginState
//
//	Returns: TRUE if the plugin list changed
// ----------------------------------------------------------------------------------------

bool pwsf_SetSASLPluginState( const char *inMechName, bool enable )
{
	PasswordServerPrefsObject *prefsObj = [[PasswordServerPrefsObject alloc] init];
	SASLPluginStatus mechState = kSASLPluginStateUnlisted;
	BOOL changeMade = NO;
	int index = -1;
	char *pluginFileNamePtr = NULL;
	PasswordServerPrefs prefs;
	char fromPath[PATH_MAX];
	char toPath[PATH_MAX];
	struct stat sb;
	int err;
	
	if ( prefsObj == nil )
		return NO;
    
	mechState = [prefsObj getSASLPluginStatus:inMechName foundAtIndex:&index];
	if ( enable && mechState != kSASLPluginStateAllowed )
	{
		// if the plug-in is disabled the old way (plug-in moved to the disabled
		// folder) then move the plug-in file.
		if ( pwsf_GetSASLMechInfo( inMechName, &pluginFileNamePtr, NULL ) )
		{
			sprintf( fromPath, "%s/%s", kSASLPluginDisabledPath, pluginFileNamePtr );
			err = lstat( fromPath, &sb );
			if ( err == 0 )
			{
				sprintf( toPath, "/usr/lib/sasl2/%s", pluginFileNamePtr );
				rename( fromPath, toPath );
			}
		
			if ( pluginFileNamePtr != NULL )
				free( pluginFileNamePtr );
		}
		
		// activate or add plug-in to active list
		[prefsObj getPrefs:&prefs];
		if ( index >= 0 )
		{
			prefs.saslPluginState[index].state = kSASLPluginStateAllowed;
		}
		else
		{
			// add to array
			for ( index = 0; index <= kMaxSASLPlugins; index++ )
				if ( prefs.saslPluginState[index].name[0] == '\0' )
					break;
			if ( index <= kMaxSASLPlugins )
			{
				strlcpy( prefs.saslPluginState[index].name, inMechName, SASL_MECHNAMEMAX + 1 );
				prefs.saslPluginState[index].state = kSASLPluginStateAllowed;
			}
		}
		[prefsObj setPrefs:&prefs];
		[prefsObj savePrefs];
		changeMade = YES;
	}
	else
	if ( !enable && mechState != kSASLPluginStateDisabled )
	{
		// disable or add plug-in to disable list
		[prefsObj getPrefs:&prefs];
		if ( index >= 0 )
		{
			prefs.saslPluginState[index].state = kSASLPluginStateDisabled;
		}
		else
		{
			// add to array
			for ( index = 0; index <= kMaxSASLPlugins; index++ )
				if ( prefs.saslPluginState[index].name[0] == '\0' )
					break;
			if ( index <= kMaxSASLPlugins )
			{
				strlcpy( prefs.saslPluginState[index].name, inMechName, SASL_MECHNAMEMAX + 1 );
				prefs.saslPluginState[index].state = kSASLPluginStateDisabled;
			}
		}
		[prefsObj setPrefs:&prefs];
		[prefsObj savePrefs];
		changeMade = YES;
	}
	
	[prefsObj release];
	
	return (bool)changeMade;
}


// ----------------------------------------------------------------------------------------
//  ConvertCFDictToFlatArray
// ----------------------------------------------------------------------------------------

void ConvertCFDictToFlatArray(const void *key, const void *value, void *context);
void ConvertCFDictToFlatArray(const void *key, const void *value, void *context)
{
	SASLPluginListConverterContext *text = NULL;
	CFStringRef keyRef;
	CFStringRef valueRef;
	char keyStr[256];
	
	if ( key == NULL || value == NULL || context == NULL )
		return;
	
	text = (SASLPluginListConverterContext *)context;
	if ( text->arrayIndex >= kMaxSASLPlugins )
		return;
	keyRef = (CFStringRef)key;
	valueRef = (CFStringRef)value;
	if ( ! CFStringGetCString(keyRef, keyStr, sizeof(keyStr), kCFStringEncodingUTF8) )
		strcpy( keyStr, "<none>" );
	
	strlcpy( (text->saslPluginState)[text->arrayIndex].name, keyStr, SASL_MECHNAMEMAX + 1 );
	if ( CFStringCompare(valueRef, CFSTR("ON"), kCFCompareCaseInsensitive) == kCFCompareEqualTo )
		text->saslPluginState[text->arrayIndex].state = kSASLPluginStateAllowed;
	else
		text->saslPluginState[text->arrayIndex].state = kSASLPluginStateDisabled;
		
	text->arrayIndex++;
}


inline void TransferItemToCFDictionary(CFMutableDictionaryRef inDict, CFStringRef inKey, CFTypeRef inValue )
{
	if ( inValue != NULL )
	{
		CFDictionaryAddValue( inDict, inKey, inValue );
		CFRelease( inValue );
	}
}


@implementation PasswordServerPrefsObject

-(id)init
{
	int idx = 0;
	
	if ( (self = [super init]) != nil )
	{
		// set defaults
		mPrefs.passiveReplicationOnly = NO;
		mPrefs.provideReplicationOnly = NO;
		mPrefs.badTrialDelay = 0;
		mPrefs.timeSkewMaxSeconds = 8 * 60;
		mPrefs.syncInterval = 60*60*24;
		mPrefs.listenerPort[0] = 106;
		mPrefs.listenerPort[1] = 3659;
		mPrefs.listenerTypeFlags = kPWPrefsAll;
		mPrefs.externalToolSet = NO;
		mPrefs.externalToolPath[0] = '\0';
		mPrefs.testSpillBucket = NO;
		mPrefs.realmSet = NO;
		mPrefs.realm[0] = '\0';
		mPrefs.kerberosCacheLimit = kKerberosCacheScaleLimit;
		mPrefs.syncSASLPluginList = YES;
		mPrefs.prefsVersion = kPWPrefsVersion;
		mPrefs.logOptions.changeList = NO;
		mPrefs.logOptions.quit = NO;
		mPrefs.deleteWait = 120;			// 2 minutes
		mPrefs.purgeWait = 20160 * 60;		// 14 days
		
		mPrefsDict = NULL;
		mPrefsFileModDate.tv_sec = 0;
		mPrefsFileModDate.tv_nsec = 0;
		mExternalToolIllegalChars = CFCharacterSetCreateWithCharactersInString( kCFAllocatorDefault, CFSTR("/:") );
		
		// leave a template behind if no preferences file present
		if ( [self loadPrefs] != 0 || mPrefs.prefsVersion < kPWPrefsVersion )
		{
			// for old prefs files, update the interface
			// list to include the UNIX domain socket
			mPrefs.listenerTypeFlags = kPWPrefsAll;
			
			// add new auth method
			for ( idx = 0; mPrefs.saslPluginState[idx].name[0] != '\0' && idx < kMaxSASLPlugins; idx++ );
			if ( idx < kMaxSASLPlugins )
			{
				strcpy( mPrefs.saslPluginState[idx].name, "PPS" );
				mPrefs.saslPluginState[idx].state = kSASLPluginStateAllowed;
			}
			
			// set the current version
			mPrefs.prefsVersion = kPWPrefsVersion;
			[self savePrefs];
		}
	}
	
	return self;
}


-(void)dealloc
{
	if ( mPrefsDict != NULL )
		CFRelease( mPrefsDict );
	if ( mExternalToolIllegalChars != NULL )
		CFRelease( mExternalToolIllegalChars );
	[super dealloc];
}


-free
{
    [self release];
    return 0;
}


// ---------------------------------------------------------------------------
//	getPrefs
// ---------------------------------------------------------------------------

-(void)getPrefs:(PasswordServerPrefs *)outPrefs
{
	if ( outPrefs != NULL )
		memcpy( outPrefs, &mPrefs, sizeof(PasswordServerPrefs) );
}


// ---------------------------------------------------------------------------
//	setPrefs
// ---------------------------------------------------------------------------

-(void)setPrefs:(PasswordServerPrefs *)inPrefs
{
	if ( inPrefs != NULL )
		memcpy( &mPrefs, inPrefs, sizeof(PasswordServerPrefs) );
}


// ---------------------------------------------------------------------------
//	refreshIfNeeded
//
//	Returns: void
// ---------------------------------------------------------------------------

-(void)refreshIfNeeded
{
	struct timespec modDate;
	bool refresh = NO;
	
	if ( [self statPrefsFileAndGetModDate:&modDate] == 0 )
	{
		if ( modDate.tv_sec > mPrefsFileModDate.tv_sec )
			refresh = true;
		
		if ( modDate.tv_sec == mPrefsFileModDate.tv_sec && modDate.tv_nsec > mPrefsFileModDate.tv_nsec )
			refresh = true;
		
		if ( refresh )
			[self loadPrefs];
	}
}


-(int)loadPrefs
{
	int					result					= -1;
	CFIndex				index					= 0;
	CFIndex				arrayCount				= 0;
	CFArrayRef			portArray				= NULL;
	CFTypeRef			valueRef				= NULL;
	CFArrayRef			interfaceArray			= NULL;
	uint16_t			aShortValue				= 0;
	uint32_t			aLongValue				= 0;
	
	result = [self loadXMLData];
	if ( result == 0 )
	{
		mPrefs.passiveReplicationOnly = [self longValueForKey:CFSTR(kPWPrefsKey_PassiveReplicationOnly) inDictionary:mPrefsDict];
		mPrefs.provideReplicationOnly = [self longValueForKey:CFSTR(kPWPrefsKey_ProvideReplicationOnly) inDictionary:mPrefsDict];
		mPrefs.badTrialDelay = [self longValueForKey:CFSTR(kPWPrefsKey_BadTrialDelay) inDictionary:mPrefsDict];
		mPrefs.timeSkewMaxSeconds = [self longValueForKey:CFSTR(kPWPrefsKey_TimeSkewMaxSeconds) inDictionary:mPrefsDict];
		mPrefs.syncInterval = [self longValueForKey:CFSTR(kPWPrefsKey_SyncInterval) inDictionary:mPrefsDict];
		mPrefs.prefsVersion = (int)[self longValueForKey:CFSTR(kPWPrefsKey_PrefsVersion) inDictionary:mPrefsDict];
		
		if ( CFDictionaryGetValueIfPresent( mPrefsDict, CFSTR(kPWPrefsKey_ListenerPorts), (const void **)&portArray ) &&
			 CFGetTypeID(portArray) == CFArrayGetTypeID() )
		{
			bzero( mPrefs.listenerPort, sizeof(mPrefs.listenerPort) );
			
			arrayCount = CFArrayGetCount( portArray );
			if ( arrayCount > kMaxListenerPorts )
				arrayCount = kMaxListenerPorts;
			for ( index = 0; index < arrayCount; index++ )
			{
				valueRef = CFArrayGetValueAtIndex( portArray, index );
				if ( valueRef == NULL )
					break;
				
				if ( CFGetTypeID(valueRef) != CFNumberGetTypeID() )
					break;
				
				if ( CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberShortType, &aShortValue) )
					mPrefs.listenerPort[index] = aShortValue;
			}
		}
		
		mPrefs.testSpillBucket = [self longValueForKey:CFSTR(kPWPrefsKey_TestSpillBucket) inDictionary:mPrefsDict];		
		if ( CFDictionaryGetValueIfPresent( mPrefsDict, CFSTR(kPWPrefsKey_SASLRealm), (const void **)&valueRef ) &&
			 CFGetTypeID(valueRef) == CFStringGetTypeID() )
		{
			mPrefs.realmSet = CFStringGetCString( (CFStringRef)valueRef, mPrefs.realm, sizeof(mPrefs.realm), kCFStringEncodingUTF8 );
		}

		// External Command
		mPrefs.externalToolSet = 0;
		if ( CFDictionaryGetValueIfPresent( mPrefsDict, CFSTR(kPWPrefsKey_ExternalTool), (const void **)&valueRef ) &&
			 CFGetTypeID(valueRef) == CFStringGetTypeID() &&
			 CFStringCompare((CFStringRef)valueRef, CFSTR(kPWPrefsValue_ExternalToolNone), kCFCompareCaseInsensitive) != kCFCompareEqualTo )
		{
			CFRange searchResult;
			char toolName[256] = {0,};
			char toolPath[256] = {0,};
			BOOL stillGood;
			int err;
			struct stat sb;
			
			stillGood = CFStringGetCString( (CFStringRef)valueRef, toolName, sizeof(toolName), kCFStringEncodingUTF8 );
			if ( stillGood )
				stillGood = ! CFStringFindCharacterFromSet( (CFStringRef)valueRef,
															mExternalToolIllegalChars,
															CFRangeMake(0,CFStringGetLength((CFStringRef)valueRef)-1),
															0, &searchResult );
			if ( stillGood )
			{
				snprintf( toolPath, sizeof(toolPath), kPWExternalToolPath"/%s", toolName );
				err = stat( toolPath, &sb );
				if ( err != 0 )
					stillGood = NO;
			}
			
			if ( stillGood )
			{
				strcpy( mPrefs.externalToolPath, toolPath );
				mPrefs.externalToolSet = 1;
			}
		}
		
		// listener interfaces
		if ( CFDictionaryGetValueIfPresent( mPrefsDict, CFSTR(kPWPrefsKey_ListenerInterfaces), (const void **)&interfaceArray ) &&
			 CFGetTypeID(interfaceArray) == CFArrayGetTypeID() )
		{
			mPrefs.listenerTypeFlags = kPWPrefsNoListeners;
			
			arrayCount = CFArrayGetCount( interfaceArray );
			for ( index = 0; index < arrayCount; index++ )
			{
				valueRef = CFArrayGetValueAtIndex( interfaceArray, index );
				if ( valueRef == NULL )
					break;
				
				if ( CFGetTypeID(valueRef) != CFStringGetTypeID() )
					break;
				
				if ( CFStringCompare((CFStringRef)valueRef, CFSTR(kPWPrefsValue_ListenerEnet), kCFCompareCaseInsensitive) == kCFCompareEqualTo )
					mPrefs.listenerTypeFlags = (ListenerTypes)((unsigned int)mPrefs.listenerTypeFlags | kPWPrefsEnet);
				else
				if ( CFStringCompare((CFStringRef)valueRef, CFSTR(kPWPrefsValue_ListenerLocal), kCFCompareCaseInsensitive) == kCFCompareEqualTo )
					mPrefs.listenerTypeFlags = (ListenerTypes)((unsigned int)mPrefs.listenerTypeFlags | kPWPrefsLocal);
				else
				if ( CFStringCompare((CFStringRef)valueRef, CFSTR(kPWPrefsValue_ListenerUDSocket), kCFCompareCaseInsensitive) == kCFCompareEqualTo )
					mPrefs.listenerTypeFlags = (ListenerTypes)((unsigned int)mPrefs.listenerTypeFlags | kPWPrefsUnixDomainSocket);
			}
		}
		
		// kerberos cache limit for replication
		if ( CFDictionaryGetValueIfPresent( mPrefsDict, CFSTR(kPWPrefsKey_KerberosCacheLimit), (const void **)&valueRef ) &&
			 CFGetTypeID(valueRef) == CFNumberGetTypeID() &&
			 CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberLongType, &aLongValue) )
		{
			mPrefs.kerberosCacheLimit = (unsigned long)aLongValue;
		}

		// sync SASL plug-in list
		mPrefs.syncSASLPluginList = [self longValueForKey:CFSTR(kPWPrefsKey_SyncSASLPluginList) inDictionary:mPrefsDict];
		
		// get the SASL plug-in state list
		if ( CFDictionaryGetValueIfPresent( mPrefsDict, CFSTR(kPWPrefsKey_SASLPluginList), (const void **)&valueRef ) &&
			 CFGetTypeID(valueRef) == CFDictionaryGetTypeID() )
		{
			SASLPluginListConverterContext context = { 0, &(mPrefs.saslPluginState[0]) };			
			CFDictionaryApplyFunction( (CFDictionaryRef)valueRef, ConvertCFDictToFlatArray, (void *)&context );
		}
		else
		{
			[self buildSASLMechPrefsFromCurrentSASLState];
		}
		
		// deleteWait
		if ( CFDictionaryGetValueIfPresent( mPrefsDict, CFSTR(kPWPrefsKey_DeleteWaitInMinutes), (const void **)&valueRef ) &&
			 CFGetTypeID(valueRef) == CFNumberGetTypeID() &&
			 CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberLongType, &aLongValue) )
		{
			mPrefs.deleteWait = MAX(aLongValue * 60, 60);
		}
		
		// purgeWait
		if ( CFDictionaryGetValueIfPresent( mPrefsDict, CFSTR(kPWPrefsKey_PurgeInMinutes), (const void **)&valueRef ) &&
			 CFGetTypeID(valueRef) == CFNumberGetTypeID() &&
			 CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberLongType, &aLongValue) )
		{
			mPrefs.purgeWait = MAX(aLongValue * 60, 120);
		}
		
		// debug log options
		if ( CFDictionaryGetValueIfPresent( mPrefsDict, CFSTR(kPWPrefsKey_DebugLogOptions), (const void **)&valueRef ) &&
			 CFGetTypeID(valueRef) == CFDictionaryGetTypeID() )
		{
			mPrefs.logOptions.changeList = (BOOL)([self longValueForKey:CFSTR(kPWPrefsValue_LogChangeList)
														inDictionary:(CFDictionaryRef)valueRef] != 0);
			mPrefs.logOptions.quit = (BOOL)([self longValueForKey:CFSTR(kPWPrefsValue_LogQuit)
														inDictionary:(CFDictionaryRef)valueRef] != 0);
		}
	}
	else
	{
		[self buildSASLMechPrefsFromCurrentSASLState];
	}
	
	return result;
}


-(int)savePrefs
{
	CFMutableDictionaryRef prefsDict;
	CFMutableArrayRef portArray;
	CFNumberRef listenerPortRef;
	CFMutableArrayRef interfaceArray;
	CFStringRef interfaceString;
	int idx;
	
	prefsDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	if ( prefsDict == NULL )
		return -1;
	
	CFBooleanRef passiveReplicationRef = mPrefs.passiveReplicationOnly ? kCFBooleanTrue : kCFBooleanFalse;
	CFBooleanRef provideReplicationRef = mPrefs.provideReplicationOnly ? kCFBooleanTrue : kCFBooleanFalse;
	CFNumberRef badTrialRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &mPrefs.badTrialDelay );
	CFNumberRef timeSkewRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &mPrefs.timeSkewMaxSeconds );
	CFNumberRef syncIntervalRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &mPrefs.syncInterval );
	CFNumberRef testSpillBucketRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &mPrefs.testSpillBucket );
	CFNumberRef kerberosCacheLimitRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &mPrefs.kerberosCacheLimit );
	CFNumberRef prefsVersionRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &mPrefs.prefsVersion );
	CFBooleanRef syncSASLPluginListRef = mPrefs.syncSASLPluginList ? kCFBooleanTrue : kCFBooleanFalse;
	CFStringRef realmRef = NULL;
	CFStringRef externalToolRef = NULL;
	
	long waitMins = mPrefs.deleteWait / 60;
	CFNumberRef deleteWaitInMinutesRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &waitMins );
	waitMins = mPrefs.purgeWait / 60;
	CFNumberRef purgeInMinutesRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberLongType, &waitMins );
	
	if ( passiveReplicationRef != NULL )
		CFDictionaryAddValue( prefsDict, CFSTR(kPWPrefsKey_PassiveReplicationOnly), passiveReplicationRef );
	
	if ( provideReplicationRef != NULL )
		CFDictionaryAddValue( prefsDict, CFSTR(kPWPrefsKey_ProvideReplicationOnly), provideReplicationRef );
	
	TransferItemToCFDictionary( prefsDict, CFSTR(kPWPrefsKey_BadTrialDelay), badTrialRef );
	TransferItemToCFDictionary( prefsDict, CFSTR(kPWPrefsKey_TimeSkewMaxSeconds), timeSkewRef );
	TransferItemToCFDictionary( prefsDict, CFSTR(kPWPrefsKey_SyncInterval), syncIntervalRef );

	portArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	if ( portArray != NULL )
	{
		for ( idx = 0; idx < kMaxListenerPorts; idx++ )
		{
			if ( mPrefs.listenerPort[idx] != 0 )
			{
				listenerPortRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberShortType, &mPrefs.listenerPort[idx] );
				if ( listenerPortRef != NULL )
				{
					CFArrayAppendValue( portArray, listenerPortRef );
					CFRelease( listenerPortRef );
				}
			}
		}
		
		CFDictionaryAddValue( prefsDict, CFSTR(kPWPrefsKey_ListenerPorts), portArray );
		CFRelease( portArray );
	}
	
	// interface list
	interfaceArray = CFArrayCreateMutable( kCFAllocatorDefault, 2, &kCFTypeArrayCallBacks );
	if ( interfaceArray != NULL )
	{
		if ( (unsigned)mPrefs.listenerTypeFlags & kPWPrefsEnet )
		{
			interfaceString = CFStringCreateWithCString( kCFAllocatorDefault, kPWPrefsValue_ListenerEnet, kCFStringEncodingUTF8 );
			if ( interfaceString != NULL )
			{
				CFArrayAppendValue( interfaceArray, interfaceString );
				CFRelease( interfaceString );
			}
		}
		if ( (unsigned)mPrefs.listenerTypeFlags & kPWPrefsLocal )
		{
			interfaceString = CFStringCreateWithCString( kCFAllocatorDefault, kPWPrefsValue_ListenerLocal, kCFStringEncodingUTF8 );
			if ( interfaceString != NULL )
			{
				CFArrayAppendValue( interfaceArray, interfaceString );
				CFRelease( interfaceString );
			}
		}
		if ( (unsigned)mPrefs.listenerTypeFlags & kPWPrefsUnixDomainSocket )
		{
			interfaceString = CFStringCreateWithCString( kCFAllocatorDefault, kPWPrefsValue_ListenerUDSocket, kCFStringEncodingUTF8 );
			if ( interfaceString != NULL )
			{
				CFArrayAppendValue( interfaceArray, interfaceString );
				CFRelease( interfaceString );
			}
		}
		
		CFDictionaryAddValue( prefsDict, CFSTR(kPWPrefsKey_ListenerInterfaces), interfaceArray );
		CFRelease( interfaceArray );
	}
	
	TransferItemToCFDictionary( prefsDict, CFSTR(kPWPrefsKey_TestSpillBucket), testSpillBucketRef );
	
	if ( mPrefs.realmSet )
	{
		realmRef = CFStringCreateWithCString( kCFAllocatorDefault, mPrefs.realm, kCFStringEncodingUTF8 );
		TransferItemToCFDictionary( prefsDict, CFSTR(kPWPrefsKey_SASLRealm), realmRef );
	}
	
	if ( mPrefs.externalToolPath[0] == '\0' )
		mPrefs.externalToolSet = NO;
	externalToolRef = CFStringCreateWithCString( kCFAllocatorDefault,
							mPrefs.externalToolSet ? mPrefs.externalToolPath : kPWPrefsValue_ExternalToolNone,
							kCFStringEncodingUTF8 );
	TransferItemToCFDictionary( prefsDict, CFSTR(kPWPrefsKey_ExternalTool), externalToolRef );
	
	TransferItemToCFDictionary( prefsDict, CFSTR(kPWPrefsKey_KerberosCacheLimit), kerberosCacheLimitRef );

	if ( syncSASLPluginListRef != NULL )
		CFDictionaryAddValue( prefsDict, CFSTR(kPWPrefsKey_SyncSASLPluginList), syncSASLPluginListRef );
	
	CFDictionaryRef saslListDict = [self saslMechArrayToCFDictionary];
	TransferItemToCFDictionary( prefsDict, CFSTR(kPWPrefsKey_SASLPluginList), saslListDict );
	
	TransferItemToCFDictionary( prefsDict, CFSTR(kPWPrefsKey_DeleteWaitInMinutes), deleteWaitInMinutesRef );
	TransferItemToCFDictionary( prefsDict, CFSTR(kPWPrefsKey_PurgeInMinutes), purgeInMinutesRef );
	TransferItemToCFDictionary( prefsDict, CFSTR(kPWPrefsKey_PrefsVersion), prefsVersionRef );
	
	// debug log options
	CFMutableDictionaryRef logOptionDictRef = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
													&kCFTypeDictionaryValueCallBacks );
	if ( logOptionDictRef != NULL )
	{
		CFDictionaryAddValue( logOptionDictRef, CFSTR(kPWPrefsValue_LogChangeList),
			mPrefs.logOptions.changeList ? kCFBooleanTrue : kCFBooleanFalse );
		CFDictionaryAddValue( logOptionDictRef, CFSTR(kPWPrefsValue_LogQuit),
			mPrefs.logOptions.quit ? kCFBooleanTrue : kCFBooleanFalse );
		TransferItemToCFDictionary( prefsDict, CFSTR(kPWPrefsKey_DebugLogOptions), logOptionDictRef );
	}
	
	if ( mPrefsDict != NULL )
		CFRelease( mPrefsDict );
	mPrefsDict = prefsDict;
	
	return [self saveXMLData];
}


-(void)setRealm:(const char *)inRealm
{
	if ( inRealm != NULL ) {
		mPrefs.realmSet = YES;
		strlcpy( mPrefs.realm, inRealm, sizeof(mPrefs.realm) );
	}
}


-(void)buildSASLMechPrefsFromCurrentSASLState
{
	const char *knownMechList[] =
		{	"APOP", "CRAM-MD5", "CRYPT", "DHX", "DIGEST-MD5", "GSSAPI", "KERBEROS_V4", "MS-CHAPv2", "NTLM", "OTP",
			"PPS", "SMB-LAN-MANAGER", "SMB-NT", "SMB-NTLMv2", "TWOWAYRANDOM", "WEBDAV-DIGEST", NULL };
	int idx;
	char *pluginFileName = NULL;
	BOOL requiresPlain = NO;
	struct stat sb;
	char path[PATH_MAX];
	
	for ( idx = 0; knownMechList[idx] != NULL && idx < kMaxSASLPlugins; idx++ )
	{
		strlcpy( mPrefs.saslPluginState[idx].name, knownMechList[idx], SASL_MECHNAMEMAX + 1 );
		mPrefs.saslPluginState[idx].state = kSASLPluginStateDisabled;
		if ( pwsf_GetSASLMechInfo(knownMechList[idx], &pluginFileName, (bool *)&requiresPlain) )
		{
			snprintf( path, sizeof(path), "/usr/lib/sasl2/%s", pluginFileName );
			if ( stat(path, &sb) == 0 )
				mPrefs.saslPluginState[idx].state = kSASLPluginStateAllowed;
		}
	}
}


-(CFDictionaryRef)saslMechArrayToCFDictionary
{
	CFMutableDictionaryRef cfDict = NULL;
	CFStringRef keyString = NULL;
	CFStringRef valueString = NULL;
	int idx;
	
	cfDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	if ( cfDict != NULL )
	{
		for ( idx = 0; idx < kMaxSASLPlugins; idx++ )
		{
			if ( mPrefs.saslPluginState[idx].name[0] == '\0' )
				break;
			
			keyString = CFStringCreateWithCString( kCFAllocatorDefault, mPrefs.saslPluginState[idx].name, kCFStringEncodingUTF8 );
			if ( keyString != NULL )
			{
				switch ( mPrefs.saslPluginState[idx].state )
				{
					case kSASLPluginStateAllowed:
						valueString = CFSTR("ON");
						break;
					
					default:
						valueString = CFSTR("OFF");
						break;
				}
				
				CFDictionaryAddValue( cfDict, keyString, valueString );
				CFRelease( keyString );
			}
		}
	}
	
	return cfDict;
}


-(SASLPluginStatus)getSASLPluginStatus:(const char *)inSASLPluginName foundAtIndex:(int *)outIndex
{
	SASLPluginStatus result = kSASLPluginStateUnlisted;
	int idx;
	
	if ( outIndex != NULL )
		*outIndex = -1;
	
	// array size is (kMaxSASLPlugins + 1)
	for ( idx = 0; idx <= kMaxSASLPlugins; idx++ )
	{
		if ( mPrefs.saslPluginState[idx].name[0] == '\0' )
			break;
		
		if ( strcmp(mPrefs.saslPluginState[idx].name, inSASLPluginName) == 0 )
		{
			result = mPrefs.saslPluginState[idx].state;
			if ( outIndex != NULL )
				*outIndex = idx;
			break;
		}
	}
	
	return result;
}

-(BOOL)methodExists:(const char *)method inArray:(CFArrayRef)inActivePluginArray
{
    CFIndex methodCount = 0;
    BOOL result = NO;
    CFIndex index;
    CFStringRef testString;
    CFStringRef aString;
    
	if ( inActivePluginArray == NULL )
		return NO;
		
	methodCount = CFArrayGetCount( inActivePluginArray );
    if ( methodCount <= 0 )
        return NO;
    
    testString = CFStringCreateWithCString( NULL, method, kCFStringEncodingUTF8 );
    if ( !testString )
        return NO;
    
    for ( index = 0; index < methodCount; index++ )
    {
        aString = (CFStringRef) CFArrayGetValueAtIndex( inActivePluginArray, index );
        if ( aString && CFStringCompare( aString, testString, 0 ) == kCFCompareEqualTo )
        {	
            result = YES;
            break;
        }
    }
    
    CFRelease( testString );
    
    return result; 
}


-(int)statPrefsFileAndGetModDate:(struct timespec *)outModDate
{
	struct stat sb;
	int result;
	
	if ( outModDate != NULL ) {
		outModDate->tv_sec = 0;
		outModDate->tv_nsec = 0;
	}
	
	result = lstat( kPWPrefsFile, &sb );
	if ( result == 0 && outModDate != NULL )
		*outModDate = sb.st_mtimespec;
		
	return result;
}

-(int)loadXMLData
{
	CFStringRef myReplicaDataFilePathRef;
	CFURLRef myReplicaDataFileRef;
	CFReadStreamRef myReadStreamRef;
	CFPropertyListRef myPropertyListRef;
	CFStringRef errorString;
	CFPropertyListFormat myPLFormat;
	struct timespec modDate;
	
	if ( [self statPrefsFileAndGetModDate:&modDate] != 0 )
		return -1;
	
	myReplicaDataFilePathRef = CFStringCreateWithCString( kCFAllocatorDefault, kPWPrefsFile, kCFStringEncodingUTF8 );
	if ( myReplicaDataFilePathRef == NULL )
		return -1;
	
	myReplicaDataFileRef = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, myReplicaDataFilePathRef, kCFURLPOSIXPathStyle, NO );
	
	CFRelease( myReplicaDataFilePathRef );
	
	if ( myReplicaDataFileRef == NULL )
		return -1;
	
	myReadStreamRef = CFReadStreamCreateWithFile( kCFAllocatorDefault, myReplicaDataFileRef );
	
	CFRelease( myReplicaDataFileRef );
	
	if ( myReadStreamRef == NULL )
		return -1;
	
	if ( ! CFReadStreamOpen( myReadStreamRef ) )
	{
		CFRelease( myReadStreamRef );
		return -1;
	}
	
	errorString = NULL;
	myPLFormat = kCFPropertyListXMLFormat_v1_0;
	myPropertyListRef = CFPropertyListCreateFromStream( kCFAllocatorDefault, myReadStreamRef, 0, kCFPropertyListMutableContainersAndLeaves, &myPLFormat, &errorString );
	
	CFReadStreamClose( myReadStreamRef );
	CFRelease( myReadStreamRef );
	
	if ( errorString != NULL )
		CFRelease( errorString );
	
	if ( myPropertyListRef == NULL )
		return -1;
	
	if ( CFGetTypeID(myPropertyListRef) != CFDictionaryGetTypeID() )
	{
		CFRelease( myPropertyListRef );
		return -1;
	}
	
	if ( mPrefsDict != NULL )
		CFRelease( mPrefsDict );
	mPrefsDict = (CFMutableDictionaryRef)myPropertyListRef;
	mPrefsFileModDate = modDate;
	
	return 0;
}


-(int)saveXMLData
{
	CFStringRef myReplicaDataFilePathRef;
	CFURLRef myReplicaDataFileRef;
	CFWriteStreamRef myWriteStreamRef;
	CFStringRef errorString;
	
	myReplicaDataFilePathRef = CFStringCreateWithCString( kCFAllocatorDefault, kPWPrefsFile, kCFStringEncodingUTF8 );
	if ( myReplicaDataFilePathRef == NULL )
		return -1;
	
	myReplicaDataFileRef = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, myReplicaDataFilePathRef, kCFURLPOSIXPathStyle, NO );
	
	CFRelease( myReplicaDataFilePathRef );
	
	if ( myReplicaDataFileRef == NULL )
		return -1;
	
	myWriteStreamRef = CFWriteStreamCreateWithFile( kCFAllocatorDefault, myReplicaDataFileRef );
	
	CFRelease( myReplicaDataFileRef );
	
	if ( myWriteStreamRef == NULL )
		return -1;
	
	if ( ! CFWriteStreamOpen( myWriteStreamRef ) )
	{
		CFRelease( myWriteStreamRef );
		return -1;
	}
	
	errorString = NULL;
	CFPropertyListWriteToStream( (CFPropertyListRef) mPrefsDict, myWriteStreamRef, kCFPropertyListXMLFormat_v1_0, &errorString );
	
	CFWriteStreamClose( myWriteStreamRef );
	CFRelease( myWriteStreamRef );
	
	if ( errorString != NULL )
		CFRelease( errorString );
	
	[self statPrefsFileAndGetModDate:&mPrefsFileModDate];
	return 0;
}


-(long)longValueForKey:(CFStringRef)key inDictionary:(CFDictionaryRef)dict
{
	CFTypeRef valueRef = NULL;
	long result = 0;
	
	if ( CFDictionaryGetValueIfPresent( dict, key, (const void **)&valueRef ) )
	{
		if ( CFGetTypeID(valueRef) == CFBooleanGetTypeID() )
			result = CFBooleanGetValue( (CFBooleanRef)valueRef );
		else if ( CFGetTypeID(valueRef) == CFNumberGetTypeID() )
			CFNumberGetValue( (CFNumberRef)valueRef, kCFNumberLongType, &result );
	}
	
	return result;
}

-(BOOL)passiveReplicationOnly { return mPrefs.passiveReplicationOnly; };
-(BOOL)provideReplicationOnly { return mPrefs.provideReplicationOnly; };
-(unsigned long)badTrialDelay { return mPrefs.badTrialDelay; };
-(unsigned long)maxTimeSkewForSync { return mPrefs.timeSkewMaxSeconds; };
-(unsigned long)syncInterval { return mPrefs.syncInterval; };
-(BOOL)localListenersOnly { return (mPrefs.listenerTypeFlags == kPWPrefsLocal); };
-(BOOL)testSpillBucket { return mPrefs.testSpillBucket; };
-(const char *)realm { return (mPrefs.realmSet ? mPrefs.realm : NULL); };
-(const char *)passwordToolPath { return (mPrefs.externalToolSet ? mPrefs.externalToolPath : NULL); };
-(unsigned long)kerberosCacheLimit { return mPrefs.kerberosCacheLimit; };
-(BOOL)syncSASLPluginList { return mPrefs.syncSASLPluginList; };
-(time_t)deleteWait { return mPrefs.deleteWait; };
-(time_t)purgeWait { return mPrefs.purgeWait; };
-(const PWSDebugLogOptions *)logOptions { return &mPrefs.logOptions; };

@end
