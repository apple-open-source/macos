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


#import <sys/stat.h>
#import "PasswordServerPrefs.h"
#import "CPSUtilities.h"

// ----------------------------------------------------------------------------------------
//  pwsf_SetSASLPluginState
//
//	Returns: TRUE if the plugin list changed
// ----------------------------------------------------------------------------------------

bool pwsf_SetSASLPluginState( const char *inMechName, bool enabled )
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
	if ( enabled && mechState != kSASLPluginStateAllowed )
	{
		// if the plug-in is disabled the old way (plug-in moved to the disabled
		// folder) then move the plug-in file.
		if ( pwsf_GetSASLMechInfo( inMechName, &pluginFileNamePtr, NULL ) )
		{
			sprintf( fromPath, "%s/%s", kSASLPluginDisabledPath, pluginFileNamePtr );
			err = stat( fromPath, &sb );
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
	if ( !enabled && mechState != kSASLPluginStateDisabled )
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


@implementation PasswordServerPrefsObject

-(id)init
{
	self = [super init];
	
	// set the defaults
	mPrefs.passiveReplicationOnly = false;
	mPrefs.provideReplicationOnly = false;
	mPrefs.badTrialDelay = 0;
	mPrefs.timeSkewMaxSeconds = 8 * 60;
	mPrefs.syncInterval = 60*60*24;
	mPrefs.listenerPort[0] = 106;
	mPrefs.listenerPort[1] = 3659;
	mPrefs.listenerTypeFlags = kPWPrefsEnetAndLocal;
	mPrefs.externalToolSet = false;
	mPrefs.externalToolPath[0] = '\0';
	mPrefs.testSpillBucket = false;
	mPrefs.realmSet = false;
	mPrefs.realm[0] = '\0';
	mPrefs.kerberosCacheLimit = kKerberosCacheScaleLimit;
	mPrefs.syncSASLPluginList = true;
	
	mPrefsDict = NULL;
	mPrefsFileModDate.tv_sec = 0;
	mPrefsFileModDate.tv_nsec = 0;
	
	//mExternalToolIllegalChars = CFCharacterSetCreateWithCharactersInString( kCFAllocatorDefault, CFSTR("/:") );
	mExternalToolIllegalChars = [[NSMutableCharacterSet alloc] init];
	[mExternalToolIllegalChars addCharactersInString:@"/:"];
	
	// leave a template behind if no preferences file present
	if ( [self loadPrefs] != 0 )
		[self savePrefs];
		
	return self;
}


-(void)dealloc
{
	[mPrefsDict release];
	[mExternalToolIllegalChars release];
	[super dealloc];
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
	bool refresh = false;
	
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
	NSArray				*portArray				= nil;
	NSArray				*interfaceArray			= nil;
	NSNumber			*aNumber				= nil;
	NSString			*aString				= nil;
	NSDictionary		*saslPlugInList			= nil;
	NSAutoreleasePool	*pool					= [NSAutoreleasePool new];
	
	result = [self loadXMLData];
	if ( result == 0 )
	{
		mPrefs.passiveReplicationOnly = [[mPrefsDict objectForKey:@kPWPrefsKey_PassiveReplicationOnly] boolValue];
		mPrefs.provideReplicationOnly = [[mPrefsDict objectForKey:@kPWPrefsKey_ProvideReplicationOnly] boolValue];
		mPrefs.badTrialDelay = [[mPrefsDict objectForKey:@kPWPrefsKey_BadTrialDelay] longValue];
		mPrefs.timeSkewMaxSeconds = [[mPrefsDict objectForKey:@kPWPrefsKey_TimeSkewMaxSeconds] longValue];
		mPrefs.syncInterval = [[mPrefsDict objectForKey:@kPWPrefsKey_SyncInterval] longValue];
		
		portArray = [mPrefsDict objectForKey:@kPWPrefsKey_ListenerPorts];
		if ( portArray != nil )
		{
			bzero( mPrefs.listenerPort, sizeof(mPrefs.listenerPort) );
			
			arrayCount = [portArray count];
			if ( arrayCount > kMaxListenerPorts )
				arrayCount = kMaxListenerPorts;
			for ( index = 0; index < arrayCount; index++ )
			{
				aNumber = [portArray objectAtIndex:index];
				if ( aNumber == NULL )
					break;
				
				mPrefs.listenerPort[index] = [aNumber intValue];
			}
		}
		
		mPrefs.testSpillBucket = [[mPrefsDict objectForKey:@kPWPrefsKey_TestSpillBucket] boolValue];
		NSString *realmString = [mPrefsDict objectForKey:@kPWPrefsKey_SASLRealm];
		if ( realmString != nil )
		{
			mPrefs.realmSet = YES;
			strlcpy( mPrefs.realm, [realmString UTF8String], sizeof(mPrefs.realm) );
		}
		else
		{
			mPrefs.realmSet = NO;
		}

		// External Command
		mPrefs.externalToolSet = NO;
		NSString *toolString = [mPrefsDict objectForKey:@kPWPrefsKey_ExternalTool];
		if ( toolString != nil && ![toolString isEqualToString:@kPWPrefsValue_ExternalToolNone] )
		{
			char toolName[256] = {0,};
			char toolPath[256] = {0,};
			BOOL stillGood = YES;
			int err;
			struct stat sb;
			
			strlcpy( toolName, [toolString UTF8String], sizeof(toolName) );
			
			NSRange aRange = [toolString rangeOfCharacterFromSet:mExternalToolIllegalChars];
			stillGood = (aRange.location != 0);
			if ( stillGood )
			{
				snprintf( toolPath, sizeof(toolPath), kPWExternalToolPath"/%s", toolName );
				err = stat( toolPath, &sb );
				if ( err != 0 )
					stillGood = false;
			}
			
			if ( stillGood )
			{
				strcpy( mPrefs.externalToolPath, toolPath );
				mPrefs.externalToolSet = YES;
				//srvmsg("Using external command: %s", toolName);
			}
			else
			{
				//errmsg("External password command rejected because it must be in "kPWExternalToolPath);
				//errmsg("External tool name: %s", toolName);
			}
		}

		// listener interfaces
		interfaceArray = [mPrefsDict objectForKey:@kPWPrefsKey_ListenerInterfaces];
		if ( interfaceArray != nil )
		{
			mPrefs.listenerTypeFlags = kPWPrefsNoListeners;
			
			arrayCount = [interfaceArray count];
			for ( index = 0; index < arrayCount; index++ )
			{
				aString = [interfaceArray objectAtIndex:index];
				if ( aString == NULL )
					break;
				
				if ( [aString caseInsensitiveCompare:@kPWPrefsValue_ListenerEnet] == NSOrderedSame )
					mPrefs.listenerTypeFlags = (ListenerTypes)((unsigned int)mPrefs.listenerTypeFlags | kPWPrefsEnet);
				else
				if ( [aString caseInsensitiveCompare:@kPWPrefsValue_ListenerLocal] == NSOrderedSame )
					mPrefs.listenerTypeFlags = (ListenerTypes)((unsigned int)mPrefs.listenerTypeFlags | kPWPrefsLocal);
			}
		}
		
		// kerberos cache limit for replication
		aNumber = [mPrefsDict objectForKey:@kPWPrefsKey_KerberosCacheLimit];
		if ( aNumber != nil )
			mPrefs.kerberosCacheLimit = [aNumber unsignedLongValue];
		
		// sync SASL plug-in list
		mPrefs.syncSASLPluginList = [[mPrefsDict objectForKey:@kPWPrefsKey_SyncSASLPluginList] boolValue];
		
		// get the SASL plug-in state list
		saslPlugInList = [mPrefsDict objectForKey:@kPWPrefsKey_SASLPluginList];
		if ( saslPlugInList != nil )
		{
			int flatArrayIndex = 0;
			NSString *keyString = nil;
			NSString *valueString = nil;
			NSEnumerator *enumerator = [saslPlugInList keyEnumerator];
			while ( (keyString = [enumerator nextObject]) != nil )
			{
				valueString = [saslPlugInList objectForKey:keyString];
				strlcpy( mPrefs.saslPluginState[flatArrayIndex].name, [keyString UTF8String], SASL_MECHNAMEMAX + 1 );
				if ( [valueString caseInsensitiveCompare:@"ON"] == NSOrderedSame )
					mPrefs.saslPluginState[flatArrayIndex].state = kSASLPluginStateAllowed;
				else
					mPrefs.saslPluginState[flatArrayIndex].state = kSASLPluginStateDisabled;
				flatArrayIndex++;
			}
		}
	}
	
	[pool release];
	
	return result;
}


-(int)savePrefs
{
	NSAutoreleasePool *pool = [NSAutoreleasePool new];
	NSMutableDictionary *prefsDict;
	NSMutableArray *portArray;
	NSMutableArray *interfaceArray;
	NSMutableDictionary *pluginDict;
	int idx;
	
	prefsDict = [[NSMutableDictionary alloc] initWithCapacity:0];
	if ( prefsDict == nil )
		return -1;
	
	[prefsDict setObject:[NSNumber numberWithBool:mPrefs.passiveReplicationOnly] forKey:@kPWPrefsKey_PassiveReplicationOnly];
	[prefsDict setObject:[NSNumber numberWithBool:mPrefs.provideReplicationOnly] forKey:@kPWPrefsKey_ProvideReplicationOnly];
	[prefsDict setObject:[NSNumber numberWithLong:mPrefs.badTrialDelay] forKey:@kPWPrefsKey_BadTrialDelay];
	[prefsDict setObject:[NSNumber numberWithLong:mPrefs.timeSkewMaxSeconds] forKey:@kPWPrefsKey_TimeSkewMaxSeconds];
	[prefsDict setObject:[NSNumber numberWithLong:mPrefs.syncInterval] forKey:@kPWPrefsKey_SyncInterval];
	[prefsDict setObject:[NSNumber numberWithInt:mPrefs.testSpillBucket] forKey:@kPWPrefsKey_TestSpillBucket];
	[prefsDict setObject:[NSNumber numberWithLong:mPrefs.kerberosCacheLimit] forKey:@kPWPrefsKey_KerberosCacheLimit];
	[prefsDict setObject:[NSNumber numberWithBool:mPrefs.syncSASLPluginList] forKey:@kPWPrefsKey_SyncSASLPluginList];
	
	portArray = [NSMutableArray arrayWithCapacity:0];
	if ( portArray != nil )
	{
		for ( idx = 0; idx < kMaxListenerPorts; idx++ )
		{
			if ( mPrefs.listenerPort[idx] != 0 )
				[portArray addObject:[NSNumber numberWithInt:mPrefs.listenerPort[idx]]];
		}
		
		[prefsDict setObject:portArray forKey:@kPWPrefsKey_ListenerPorts];
	}
	
	// interface list
	interfaceArray = [NSMutableArray arrayWithCapacity:0];
	if ( interfaceArray != nil )
	{
		if ( (unsigned)mPrefs.listenerTypeFlags & kPWPrefsEnet )
			[interfaceArray addObject:@kPWPrefsValue_ListenerEnet];

		if ( (unsigned)mPrefs.listenerTypeFlags & kPWPrefsLocal )
			[interfaceArray addObject:@kPWPrefsValue_ListenerLocal];
		
		[prefsDict setObject:interfaceArray forKey:@kPWPrefsKey_ListenerInterfaces];
	}
		
	if ( mPrefs.realmSet )
		[prefsDict setObject:[NSString stringWithUTF8String:mPrefs.realm] forKey:@kPWPrefsKey_SASLRealm];
	
	if ( mPrefs.externalToolPath[0] == '\0' )
		mPrefs.externalToolSet = NO;
	
	[prefsDict setObject:
		[NSString stringWithUTF8String:
			mPrefs.externalToolSet ? mPrefs.externalToolPath : kPWPrefsValue_ExternalToolNone]
		forKey:@kPWPrefsKey_ExternalTool];
	
	
	
	// get the SASL plug-in state list
	pluginDict = [NSMutableDictionary dictionaryWithCapacity:0];
	if ( pluginDict != nil )
	{
		int flatArrayIndex;
		NSString *keyString;
		
		for ( flatArrayIndex = 0; flatArrayIndex <= kMaxSASLPlugins; flatArrayIndex++ )
		{
			if ( mPrefs.saslPluginState[flatArrayIndex].name[0] == '\0' )
				break;
			keyString = [NSString stringWithUTF8String:mPrefs.saslPluginState[flatArrayIndex].name];
			switch( mPrefs.saslPluginState[flatArrayIndex].state )
			{
				case kSASLPluginStateAllowed:
					[pluginDict setObject:@"ON" forKey:keyString];
					break;
				
				case kSASLPluginStateDisabled:
					[pluginDict setObject:@"OFF" forKey:keyString];
					break;
				
				case kSASLPluginStateUnlisted:
					break;
			}
		}
		
		[prefsDict setObject:pluginDict forKey:@kPWPrefsKey_SASLPluginList];
	}
	
	[mPrefsDict release];
	mPrefsDict = prefsDict;
	
	[pool release];
	
	return [self saveXMLData];
}


-(void)setRealm:(const char *)inRealm
{
	if ( inRealm != NULL ) {
		mPrefs.realmSet = YES;
		strlcpy( mPrefs.realm, inRealm, sizeof(mPrefs.realm) );
	}
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

-(BOOL)methodExists:(const char *)method inArray:(NSArray *)inActivePluginArray
{
	NSAutoreleasePool *pool = [NSAutoreleasePool new];
	NSString *testMethodString = [NSString stringWithUTF8String:method];
	NSString *curMethodString = nil;
	NSEnumerator *enumerator = [inActivePluginArray objectEnumerator];
	BOOL result = NO;
	
	while ( (curMethodString = [enumerator nextObject]) != nil )
	{
		if ( [testMethodString caseInsensitiveCompare:curMethodString] == NSOrderedSame ) {
			result = YES;
			break;
		}
	}
	
	[pool release];
	
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
	
	result = stat( kPWPrefsFile, &sb );
	if ( result == 0 && outModDate != NULL )
		*outModDate = sb.st_mtimespec;
		
	return result;
}

-(int)loadXMLData
{
	NSMutableDictionary *prefsDict = nil;
	struct timespec modDate;
	int result = -1;
	
	if ( [self statPrefsFileAndGetModDate:&modDate] != 0 )
		return -1;
	
	prefsDict = [[NSMutableDictionary alloc] initWithCapacity:0];
	[prefsDict initWithContentsOfFile:@kPWPrefsFile];
	if ( [prefsDict count] > 0 )
	{
		[mPrefsDict release];
		mPrefsDict = prefsDict;
		mPrefsFileModDate = modDate;
		result = 0;
	}
	else
	{
		[prefsDict release];
	}
	
	return result;
}


-(int)saveXMLData
{
	NSAutoreleasePool *pool = [NSAutoreleasePool new];
	int result = -1;
	
	if ( [mPrefsDict writeToFile:@kPWPrefsFile atomically:YES] )
	{
		[self statPrefsFileAndGetModDate:&mPrefsFileModDate];
		result = 0;
	}
	
	[pool release];
	
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
-(unsigned long)getKerberosCacheLimit { return mPrefs.kerberosCacheLimit; };
-(BOOL)syncSASLPluginList { return mPrefs.syncSASLPluginList; };

@end
