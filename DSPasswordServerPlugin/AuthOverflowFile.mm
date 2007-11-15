/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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

#import <TargetConditionals.h>
#import <unistd.h>
#import <sys/stat.h>
#import "AuthOverflowFile.h"
#import "PSUtilitiesDefs.h"
#import "CKerberosPrincipal.h"

#define kFixedDESKey					"1POTATO2potato3PotatoFOUR"

@implementation AuthOverflowFile

-(id)init
{
	self = [super init];
	
	mOverflowPath = strdup( kPWDirPath );

	return self;
}

-(id)initWithUTF8Path:(const char *)inOverflowPath
{
	self = [super init];
	
	mOverflowPath = strdup( inOverflowPath );
	
	return self;
}


-free
{
	if ( mOverflowPath != NULL )
		free( mOverflowPath );
	
	return [super free];
}


-(void)pwWait
{
}


-(void)pwSignal
{
}


-(FILE *)fopenOverflow:(const char *)path mode:(const char *)mode
{
	return fopen(path, mode);
}


-(int)fcloseOverflow:(FILE *)filePtr
{
	return fclose(filePtr);
}


-(int)getPasswordRecFromSpillBucket:(PWFileEntry *)inOutRec unObfuscate:(BOOL)unObfuscate
{
	PWFileEntry recBuff;
	off_t offset = 0;
	off_t byteCount;
	FILE *fp;
	int err = -1;
	char uidStr[35] = {0,};
	char buff[35] = {0,};
	
	if ( inOutRec == NULL )
		return -1;
	
	err = [self openOverflowFile:inOutRec create:NO fileDesc:&fp filePath:NULL];
	if ( err != 0 )
		return err;
	
	// use text-based matching to avoid endian problems
	pwsf_passwordRecRefToString( inOutRec, uidStr );
	
	do
	{
		byteCount = pread( fileno(fp), buff, sizeof(buff), offset );
		if ( byteCount >= 34 && strncmp( uidStr, buff, 34 ) == 0 )
		{
			// found it
			byteCount = pread( fileno(fp), (char *)&recBuff, sizeof(recBuff), offset+34 );
			if ( byteCount > 0 )
			{
				pwsf_EndianAdjustPWFileEntry( &recBuff, 1 );
				
				// recover the password
				if ( unObfuscate )
					pwsf_DESAutoDecode( recBuff.passwordStr );
				
				// copy the record
				memcpy( inOutRec, &recBuff, sizeof(PWFileEntry) );
				
				// zero our copy
				bzero( &recBuff, sizeof(recBuff) );
				err = 0;
			}
			break;
		}
		
		offset += sizeof(PWFileEntry) + 34;
	}
	while ( byteCount == sizeof(uidStr) );
	
	[self fcloseOverflow:fp];
	
	return err;
}


//------------------------------------------------------------------------------------------------
//	saveOverflowRecord
//
//	Returns: -1, errno, or 0 for success
//
//	Updates and existing record in the overflow bucket
//------------------------------------------------------------------------------------------------

-(int)saveOverflowRecord:(PWFileEntry *)inPasswordRec obfuscate:(BOOL)obfuscate setModDate:(BOOL)setModDate
{
	off_t offset = 0;
	off_t byteCount;
	FILE *fp = NULL;
	int err = -1;
	char uidStr[35] = {0,};
	char buff[35] = {0,};
	unsigned int encodeLen;
    int writeCount;
    char *filePath = NULL;
	bool bad = false;
	PWFileEntry passRec;

	if ( inPasswordRec == NULL )
		return -1;
	
	if ( inPasswordRec->slot <= 0 )
    	return -1;	
	
	if ( mReadOnlyFileSystem )
		return -1;
	
	err = [self openOverflowFile:inPasswordRec create:YES fileDesc:&fp filePath:&filePath];
	if ( err != 0 )
		return err;
	if ( fp == NULL || filePath == NULL ) {
		err = -1;
		goto done;
	}
	
	int fileNumber = [self getFileNumberFromPath:filePath];
	
	// use text-based matching to avoid endian problems
	pwsf_passwordRecRefToString( inPasswordRec, uidStr );
	
	if ( setModDate )
		pwsf_getGMTime( (struct tm *)&inPasswordRec->modificationDate );
	
	encodeLen = strlen(inPasswordRec->passwordStr);
	encodeLen += (kFixedDESChunk - (encodeLen % kFixedDESChunk));	
	if ( encodeLen > sizeof(inPasswordRec->passwordStr) )
		encodeLen = sizeof(inPasswordRec->passwordStr);
	
	if ( obfuscate )
		pwsf_DESEncode(inPasswordRec->passwordStr, encodeLen);
	
	err = -1;
	do
	{
		byteCount = pread( fileno(fp), buff, sizeof(buff), offset );
		if ( byteCount == 0 )
			break;
		bad = (byteCount != sizeof(buff));
		if ( !bad )
		{
			pwsf_stringToPasswordRecRef( buff, &passRec );
			bad = (fileNumber >= 0 && [self simpleHash:&passRec] != fileNumber);
		}
		
		if ( bad )
		{
			[self pwWait];
			if ( offset > 0 ) {
				ftruncate( fileno(fp), offset );
			}
			else {
				[self fcloseOverflow:fp];
				fp = NULL;
				unlink( filePath );
				free( filePath );
				filePath = NULL;
			}
			[self pwSignal];
			
			err = -1;
			break;
		}
		
		if ( byteCount >= 34 && strncmp( uidStr, buff, 34 ) == 0 )
		{
			// found it
#if TARGET_RT_LITTLE_ENDIAN
			memcpy( &passRec, inPasswordRec, sizeof(PWFileEntry) );
			pwsf_EndianAdjustPWFileEntry( &passRec, 0 );
			byteCount = pwrite( fileno(fp), &passRec, sizeof(PWFileEntry), offset+34 );
			bzero( &passRec, sizeof(PWFileEntry) );
#else
			byteCount = pwrite( fileno(fp), inPasswordRec, sizeof(PWFileEntry), offset+34 );
#endif
			err = 0;
			break;
		}
		
		offset += 34+sizeof(PWFileEntry);
	}
	while ( byteCount == sizeof(buff) );

	// if not found, append the new record
	if ( err == -1 )
	{
		// if the file was damaged, it may be gone
		if ( fp == NULL )
		{
			err = [self openOverflowFile:inPasswordRec create:YES fileDesc:&fp filePath:&filePath];
			if ( err != 0 )
				goto done;
			if ( fp == NULL || filePath == NULL ) {
				err = -1;
				goto done;
			}
		}
		
		[self pwWait];
		
		err = fseek( fp, 0, SEEK_END );
		if ( err == 0 )
		{
			offset = ftell( fp );
			byteCount = offset % (34 + sizeof(PWFileEntry));
			if ( byteCount != 0 )
			{
				#if DEBUG
				errmsg( kOverflowInvalidLengthRepairMsg, filePath );
				#endif
				ftruncate( fileno(fp), offset - byteCount );
			}
			
			writeCount = fwrite( uidStr, 34, 1, fp );
			if ( writeCount != 1 )
				err = -1;
		
			if ( err == 0 )
			{
#if TARGET_RT_LITTLE_ENDIAN
				memcpy( &passRec, inPasswordRec, sizeof(PWFileEntry) );
				pwsf_EndianAdjustPWFileEntry( &passRec, 0 );
				writeCount = fwrite( &passRec, sizeof(PWFileEntry), 1, fp );
				bzero( &passRec, sizeof(PWFileEntry) );
#else
				writeCount = fwrite( inPasswordRec, sizeof(PWFileEntry), 1, fp );
#endif
				if ( writeCount != 1 ) {
					err = -1;
					
					// We don't know what happened (disk full error?)
					// However, we must make sure the file is the correct length
					offset = ftell( fp );
					byteCount = offset % (34 + sizeof(PWFileEntry));
					if ( byteCount != 0 )
					{
						#if DEBUG
						errmsg( kOverflowInvalidLengthRepairMsg, filePath );
						#endif
						ftruncate( fileno(fp), offset - byteCount );
					}
				}
			}
			fflush( fp );
		}
		[self pwSignal];
	}
	
	if ( obfuscate )
		pwsf_DESDecode(inPasswordRec->passwordStr, encodeLen);
	
done:
	if ( fp != NULL )
		[self fcloseOverflow:fp];
	
	if ( filePath != NULL )
		free( filePath );
	
	return err;
}


//------------------------------------------------------------------------------------------------
//	DeleteSlot
//------------------------------------------------------------------------------------------------

-(int)deleteSlot:(PWFileEntry *)inPasswordRec
{
	off_t offset = 0;
	off_t byteCount;
	FILE *fp;
	int err = -1;
	char uidStr[35];
	char idBuff[35];
	struct stat sb;
	ssize_t copyLen;
	unsigned char *buff = NULL;
	
	if ( inPasswordRec == NULL || inPasswordRec->slot <= 0 || mReadOnlyFileSystem )
		return -1;
	
	err = [self openOverflowFile:inPasswordRec create:NO fileDesc:&fp filePath:NULL];
	if ( err != 0 )
		return err;
	
	[self pwWait];
	err = fstat( fileno(fp), &sb );
	if ( err != 0 ) {
		[self fcloseOverflow:fp];
		#if DEBUG
		errmsg( "fstat() failed reading overflow file (err = %d).", err );
		#endif
		[self pwSignal];
		return err;
	}
	
	// use text-based matching to avoid endian problems
	pwsf_passwordRecRefToString( inPasswordRec, uidStr );
	
	err = -1;
	do
	{
		byteCount = pread( fileno(fp), idBuff, sizeof(idBuff), offset );
		if ( byteCount >= 34 && strncmp(uidStr, idBuff, 34) == 0 )
		{
			// found it
			#if DEBUG
			srvmsg( "deleting id: %s", uidStr);
			#endif
			copyLen = sb.st_size - offset - 34 - sizeof(PWFileEntry);
			if ( copyLen > 0 ) 
			{
				buff = (unsigned char *) malloc( copyLen );
				if ( buff != NULL )
				{
					byteCount = pread( fileno(fp), buff, copyLen, offset + 34 + sizeof(PWFileEntry) );
					if ( byteCount == copyLen )
					{
						byteCount = pwrite( fileno(fp), buff, copyLen, offset );
						if ( byteCount == copyLen )
							ftruncate( fileno(fp), sb.st_size - 34 - sizeof(PWFileEntry) );
					}
					free( buff );
					err = 0;
				}
			}
			else
			{
				if ( sb.st_size == 34 + sizeof(PWFileEntry) )
				{
					char overflowFileName[50];
					char overflowPath[1024];
					
					[self getOverflowFileName:overflowFileName forRec:inPasswordRec];
					sprintf( overflowPath, "%s/%s", kPWDirPath, overflowFileName );
					[self fcloseOverflow:fp];
					fp = NULL;
					unlink( overflowPath );
				}
				else
				{
					ftruncate( fileno(fp), sb.st_size - 34 - sizeof(PWFileEntry) );
				}
			}
			break;
		}
		
		offset += 34 + sizeof(PWFileEntry);
	}
	while ( byteCount == sizeof(idBuff) );
	
	[self pwSignal];
	
	[self fcloseOverflow:fp];
	
	return err;
}


//------------------------------------------------------------------------------------------------
//	AddOverflowToSyncFile
//------------------------------------------------------------------------------------------------

-(void)addOverflowToSyncFile:(FILE *)inSyncFile
	afterDate:(time_t)inAfterDate
	timeSkew:(long)inTimeSkew
	numUpdated:(long *)outNumRecordsUpdated
{
	CFMutableArrayRef fileListArray = NULL;
	CFIndex index, flCount;
	CFStringRef filePathString = NULL;
	off_t offset = 0;
	off_t byteCount;
	FILE *fp;
	time_t theTime;
	int writeCount;
	PWFileEntry recBuff;
	const char *filePathPtr = NULL;
	PWSFKerberosPrincipal* kerberosRec;
	int zeroLen = 0;
	
	if ( [self getOverflowFileList:&fileListArray customPath:NULL] )
	{
		flCount = CFArrayGetCount( fileListArray );
		for (index = 0; index < flCount; index++)
		{
			filePathString = (CFStringRef) CFArrayGetValueAtIndex( fileListArray, index );
			filePathPtr = CFStringGetCStringPtr( filePathString, kCFStringEncodingUTF8 );
			if ( filePathPtr == NULL )
				continue;
			
			fp = [self fopenOverflow:filePathPtr mode:(mReadOnlyFileSystem ? "r" : "r+")];
			if ( fp == NULL && errno == EROFS )
			{
				mReadOnlyFileSystem = YES;
				fp = [self fopenOverflow:filePathPtr mode:"r"];
			}
			if ( fp == NULL )
				continue;
			
			offset = 0;
			do
			{
				// Because every record gets evaluated for sync, there is no reason to
				// check the string form of the ID (first 34 chars).
				byteCount = pread( fileno(fp), (char *)&recBuff, sizeof(recBuff), offset+34 );
				
				// adjust time skew for comparison purposes. The record itself is
				// adjusted on the processing side.
				theTime = timegm( (struct tm *)&recBuff.modificationDate ) + inTimeSkew;
				
				if ( theTime >= inAfterDate )
				{
					writeCount = fwrite( &recBuff, sizeof(recBuff), 1, inSyncFile );
					if ( writeCount != 1 )
						break;
					
					if ( recBuff.digest[4].digest[0] != '\0' )
					{
						char principalName[600] = {0,};
						strcpy(principalName, recBuff.usernameStr);
						strcat(principalName, "@");
						strcat(principalName, recBuff.digest[4].digest);
						PWSFKerberosPrincipal::ReadPrincipalFromDB(principalName, &kerberosRec);
					}
					else
						kerberosRec = NULL;
			
					if (kerberosRec != NULL)
						writeCount = kerberosRec->WritePrincipalToFile(inSyncFile);
					else
						writeCount = fwrite( &zeroLen, sizeof(zeroLen), 1, inSyncFile );

					if ( outNumRecordsUpdated != NULL )
						(*outNumRecordsUpdated)++;
				}
				
				offset += 34 + sizeof(PWFileEntry);
			}
			while ( byteCount == sizeof(recBuff) );
			
			[self fcloseOverflow:fp];
		}
		
		CFRelease( fileListArray );
	}
}


//------------------------------------------------------------------------------------------------
//	GetUserRecordFromName
//
//	Returns: 1 if found, 0 if not.
//
//	Only looks at the usernameStr for the user's short name.
//	Use GetUserRecordFromPrincipal to get the correct name for Kerberos.
//------------------------------------------------------------------------------------------------

-(int)getUserRecord:(PWFileEntry *)inOutUserRec fromName:(const char *)inName
{
	return [self doActionForAllOverflowFiles:kOverflowActionGetFromName principal:inName userRec:inOutUserRec purgeBefore:0];
}


//------------------------------------------------------------------------------------------------
//	GetUserRecordFromPrincipal
//
//	Returns: 1 if found, 0 if not.
//------------------------------------------------------------------------------------------------

-(int)getUserRecord:(PWFileEntry *)inOutUserRec fromPrincipal:(const char *)inPrincipal
{
	return [self doActionForAllOverflowFiles:kOverflowActionGetFromPrincipal principal:inPrincipal userRec:inOutUserRec purgeBefore:0];
}


-(void)requireNewPasswordForAllAccounts:(BOOL)inRequired
{
	[self doActionForAllOverflowFiles:inRequired ? kOverflowActionRequireNewPassword : kOverflowActionDoNotRequireNewPassword
		principal:NULL userRec:NULL purgeBefore:0];
}


-(void)kerberizeOrNewPassword
{
	[self doActionForAllOverflowFiles:kOverflowActionKerberizeOrNewPassword principal:NULL userRec:NULL purgeBefore:0];
}


//------------------------------------------------------------------------------------------------
//	DoActionForAllOverflowFiles
//
//	RETURNS: 1 if (inAction==kOverflowActionGetFromPrincipal && userRecord==found), else 0
//------------------------------------------------------------------------------------------------

-(int)doActionForAllOverflowFiles:(OverflowAction)inAction
	principal:(const char *)inPrincipal
	userRec:(PWFileEntry *)inOutUserRec
	purgeBefore:(time_t)beforeSecs
{
	CFMutableArrayRef fileListArray = NULL;
	CFStringRef filePathString = NULL;
	CFIndex index, flCount;
	off_t offset = 0;
	off_t byteCount = 0;
	FILE *fp = NULL;
	PWFileEntry recBuff = {0};
	int returnValue = 0;
	int fileNumber = -1;
	bool repairedAFile = false;
	char *thePrincDomain = NULL;
	long len = 0;
	bool bad = false;
	//unsigned long encodeLen = 0;
	char filePathStr[PATH_MAX] = {0};
	char thePrincName[256] = {0,};
	
	if ( mReadOnlyFileSystem )
	{
		if ( inAction == kOverflowActionRequireNewPassword ||
			 inAction == kOverflowActionDoNotRequireNewPassword ||
			 inAction == kOverflowActionKerberizeOrNewPassword ||
			 inAction == kOverflowActionPurgeDeadSlots )
		{
			return 0;
		}
	}
	
	// prep work
	if ( inAction == kOverflowActionGetFromPrincipal )
	{
		if ( inPrincipal == NULL || inOutUserRec == NULL )
			return 0;
		
		// break principal into name and domain
		thePrincDomain = strchr( inPrincipal, '@' );
		if ( thePrincDomain == NULL )
			return 0;
		
		// must have a principal name
		len = thePrincDomain - inPrincipal;
		if ( len == 0 )
			return 0;
		
		// advance past the '@'
		thePrincDomain++;
		
		// save the name as a c-str
		strlcpy( thePrincName, inPrincipal, len + 1 );
	}
	
	if ( [self getOverflowFileList:&fileListArray customPath:NULL] )
	{
		flCount = CFArrayGetCount( fileListArray );
		for ( index = 0; index < flCount; index++ )
		{
			filePathString = (CFStringRef) CFArrayGetValueAtIndex( fileListArray, index );
			if ( !CFStringGetCString(filePathString, filePathStr, sizeof(filePathStr), kCFStringEncodingUTF8) )
				continue;
			
			fp = [self fopenOverflow:filePathStr mode:(mReadOnlyFileSystem ? "r" : "r+")];
			if ( fp == NULL && errno == EROFS )
			{
				mReadOnlyFileSystem = true;
				
				if ( inAction == kOverflowActionRequireNewPassword ||
					 inAction == kOverflowActionDoNotRequireNewPassword ||
					 inAction == kOverflowActionKerberizeOrNewPassword )
					break;
				
				fp = [self fopenOverflow:filePathStr mode:"r"];
			}
			if ( fp == NULL )
				continue;
			
			fileNumber = [self getFileNumberFromPath:filePathStr];
			
			offset = 0;
			do
			{
				byteCount = pread( fileno(fp), (char *)&recBuff, sizeof(recBuff), offset+34 );
				if ( byteCount == 0 )
					break;
				
				// file integrity check
				bad = (byteCount != sizeof(recBuff));
				if ( !bad )
				{
					pwsf_EndianAdjustPWFileEntry( &recBuff, 1 );
					bad = (fileNumber >= 0 && [self simpleHash:&recBuff] != fileNumber);
				}
				if ( bad )
				{
					if ( offset > 0 ) {
						#if DEBUG
						errmsg( kOverflowInvalidRepairMsg, filePathStr );
						#endif
						[self pwWait];
						ftruncate( fileno(fp), offset );
						[self pwSignal];
					}
					else {
						[self fcloseOverflow:fp];
						fp = NULL;
						#if DEBUG
						errmsg( kOverflowInvalidRemoveMsg, filePathStr );
						#endif
						unlink( filePathStr );
					}
					repairedAFile = true;
					break;
				}
				
				if ( byteCount == sizeof(recBuff) )
				{
					switch( inAction )
					{
						case kOverflowActionRequireNewPassword:
							recBuff.access.newPasswordRequired = 1;
							pwrite( fileno(fp), (char *)&recBuff, sizeof(recBuff), offset+34 );
							break;
							
						case kOverflowActionDoNotRequireNewPassword:
							recBuff.access.newPasswordRequired = 0;
							pwrite( fileno(fp), (char *)&recBuff, sizeof(recBuff), offset+34 );
							break;
						
						case kOverflowActionGetFromName:
							if ( strcmp( inPrincipal, recBuff.usernameStr ) == 0 )
							{
								memcpy( inOutUserRec, &recBuff, sizeof(PWFileEntry) );
								returnValue = 1;
								break;
							}
							break;
						
						case kOverflowActionGetFromPrincipal:
							if ( strcmp( thePrincName, pwsf_GetPrincName(&recBuff) ) == 0 )
							{
								memcpy( inOutUserRec, &recBuff, sizeof(PWFileEntry) );
								returnValue = 1;
								break;
							}
							break;
						
						case kOverflowActionKerberizeOrNewPassword:
							if ( recBuff.access.passwordIsHash )
							{
								// new password required
								recBuff.access.newPasswordRequired = 1;
								pwrite( fileno(fp), (char *)&recBuff, sizeof(recBuff), offset+34 );
							}
							else
							{
								#warning Kerberize not implemented
								#if 0
								// un-obfuscate
								pwsf_DESAutoDecode( recBuff.passwordStr );
								if ( AddPrincipal(pwsf_GetPrincName(&recBuff), recBuff.passwordStr, recBuff.digest[kPWHashSlotKERBEROS].digest, sizeof(recBuff.digest[kPWHashSlotKERBEROS].digest)) )
								{
									if ( recBuff.digest[kPWHashSlotKERBEROS].digest[0] != '\0' )
									{
										strcpy( recBuff.digest[kPWHashSlotKERBEROS].method, "KerberosRealmName" );
										strcpy( recBuff.digest[kPWHashSlotKERBEROS_NAME].method, "KerberosPrincName" );
										strcpy( recBuff.digest[kPWHashSlotKERBEROS_NAME].digest, pwsf_GetPrincName(&recBuff) );
										
										// re-obfuscate
										encodeLen = strlen( recBuff.passwordStr );
										encodeLen += (kFixedDESChunk - (encodeLen % kFixedDESChunk));	
										if ( encodeLen > sizeof(recBuff.passwordStr) )
											encodeLen = sizeof(recBuff.passwordStr);
										pwsf_DESEncode( recBuff.passwordStr, encodeLen );
										
										// write
										pwrite( fileno(fp), (char *)&recBuff, sizeof(recBuff), offset+34 );
									}
									else
									{
										recBuff.digest[kPWHashSlotKERBEROS].method[0] = 0;
									}
								}
								else
								{
									srvmsg( "Could not add kerberos principal %s.", pwsf_GetPrincName(&recBuff) );
								}
								#endif
							}
							break;
						
						case kOverflowActionDumpRecords:
							{
								char idStr[35] = {0,};
								char modDateStr[256] = {0,};
								time_t secs;
								struct tm localTime;
								
								pwsf_passwordRecRefToString( &recBuff, idStr );
								secs = timegm( (struct tm *)&recBuff.modificationDate );
								localtime_r( &secs, &localTime );
								strftime( modDateStr, sizeof(modDateStr), "%m/%d/%Y %r", &localTime );
								printf( "overflow %.2ld: %s %15s\t%s\n", [self simpleHash:&recBuff], idStr, recBuff.usernameStr, modDateStr );
							}
							break;
						
						case kOverflowActionPurgeDeadSlots:
							if ( recBuff.extraAccess.recordIsDead )
							{
								time_t deleteSecs = timegm( (struct tm *)&recBuff.modDateOfPassword );
								if ( difftime(deleteSecs, beforeSecs) < 0 )
									[self deleteSlot:&recBuff];
							}
							break;
					}
					bzero( &recBuff, sizeof(recBuff) );
				}
				
				offset += 34 + sizeof(PWFileEntry);
			}
			while ( byteCount == sizeof(recBuff) );
			
			[self fcloseOverflow:fp];
		}
		
		CFRelease( fileListArray );
	}
	
	return returnValue;
}


//------------------------------------------------------------------------------------------------
//	GetOverflowFileList
//
//	RETURNS: TRUE if the list is retrieved and contains at least one overflow file
//------------------------------------------------------------------------------------------------

-(BOOL)getOverflowFileList:(CFMutableArrayRef *)outFileArray customPath:(const char *)inCustomPath
{
	int result;
	CFMutableArrayRef fileArray = NULL;
	const char *pathPtr = mOverflowPath;
	
	if ( outFileArray == nil )
		return -1;
	*outFileArray = nil;
	
	if ( inCustomPath != NULL )
		pathPtr = inCustomPath;
	
	result = pwsf_EnumerateDirectory( pathPtr, kOverflowFilePrefix, &fileArray );
	if ( result == 0 )
	{
		if ( CFArrayGetCount( fileArray ) == 0 )
		{
			CFRelease( fileArray );
			result = -1;
		}
	}
	
	if ( result == 0 )
		*outFileArray = fileArray;
	return (result == 0);
}


//------------------------------------------------------------------------------------------------
//	openOverflowFile
//------------------------------------------------------------------------------------------------

-(int)openOverflowFile:(PWFileEntry *)inPasswordRec create:(BOOL)create fileDesc:(FILE **)outFP filePath:(char **)outFilePath
{
	char overflowFileName[50] = {0,};
	char overflowPath[1024] = {0,};
	FILE *fp;
	int err = -1;
	struct stat sb;
	
	if ( inPasswordRec == NULL || outFP == NULL )
		return -1;
	
	*outFP = NULL;
	if ( outFilePath != NULL )
		*outFilePath = NULL;
	
	[self getOverflowFileName:overflowFileName forRec:inPasswordRec];
	snprintf( overflowPath, sizeof(overflowPath), "%s/%s", mOverflowPath, overflowFileName );
	
	[self pwWait];
	if ( create && lstat(overflowPath, &sb) != 0 )
	{
		// For file creation, don't use the wrapper methods.
		fp = fopen( overflowPath, "w" );
		if ( fp != NULL )
			fclose( fp );
		else
		{
			[self pwSignal];
			return -1;
		}
	}
	
	fp = [self fopenOverflow:overflowPath mode:(mReadOnlyFileSystem ? "r" : "r+")];
	if ( fp == NULL && errno == EROFS )
	{
		mReadOnlyFileSystem = YES;
		fp = [self fopenOverflow:overflowPath mode:"r"];
	}
	[self pwSignal];
	
	if ( fp == NULL )
	{
		err = errno;
		if ( err == 0 )
			err = -1;
		return err;
	}
	
	*outFP = fp;
	if ( outFilePath != NULL )
		*outFilePath = strdup( overflowPath );
	
	return 0;
}


//------------------------------------------------------------------------------------------------
//	GetFileNumberFromPath
//------------------------------------------------------------------------------------------------

-(int)getFileNumberFromPath:(const char *)inFilePath
{
	int fileNumber = -1;
	const char *numberInPathPtr = NULL;
	
	if ( inFilePath != NULL ) {
		numberInPathPtr = strstr( inFilePath, kOverflowFilePrefix );
		if ( numberInPathPtr != NULL ) {
			numberInPathPtr += sizeof( kOverflowFilePrefix );
			sscanf( numberInPathPtr, "%d", &fileNumber );
		}
	}
	
	return fileNumber;
}


//------------------------------------------------------------------------------------------------
//	getOverflowFileName
//
//	Returns: void
//	outFileName		<- 		The name of the file that contains the overflow for a given slot
//
//	The user ID overflows are put into multiple files as an optimization because we are doing
//	sequential searching.
//------------------------------------------------------------------------------------------------

-(void)getOverflowFileName:(char *)outFileName forRec:(PWFileEntry *)inPasswordRec;
{
	if ( outFileName == NULL )
		return;
	
	if ( inPasswordRec == NULL ) {
		strcpy( outFileName, kOverflowFilePrefix".exception" );
		return;
	}
	
	sprintf( outFileName, "%s.%lu", kOverflowFilePrefix, [self simpleHash:inPasswordRec] );
}


//------------------------------------------------------------------------------------------------
//	simpleHash
//------------------------------------------------------------------------------------------------

-(long)simpleHash:(PWFileEntry *)inPasswordRec
{
	return ( (inPasswordRec->time ^ inPasswordRec->rnd) * (inPasswordRec->sequenceNumber + inPasswordRec->slot) ) % 100;
}



@end
