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


#import <Foundation/Foundation.h>
#import <PasswordServer/AuthFile.h>
#import <PasswordServer/AuthDBFileDefs.h>

#define kOverflowFilePrefix				"authserveroverflow"

@interface AuthOverflowFile : NSObject {
	BOOL mReadOnlyFileSystem;
	char *mOverflowPath;
}

-(id)initWithUTF8Path:(const char *)inOverflowPath;
-(void)dealloc;
-free DEPRECATED_ATTRIBUTE ;

// traps for overrides
-(void)pwWait;
-(void)pwSignal;
-(FILE *)fopenOverflow:(const char *)path mode:(const char *)mode;
-(int)fcloseOverflow:(FILE *)filePtr;

-(int)getPasswordRecFromSpillBucket:(PWFileEntry *)inOutRec unObfuscate:(BOOL)unObfuscate;	// default YES
-(int)saveOverflowRecord:(PWFileEntry *)inPasswordRec obfuscate:(BOOL)obfuscate setModDate:(BOOL)setModDate; // default YES,YES
-(int)deleteSlot:(PWFileEntry *)inPasswordRec;
-(void)addOverflowToSyncFile:(FILE *)inSyncFile
	afterDate:(time_t)inAfterDate
	timeSkew:(long)inTimeSkew
	numUpdated:(long *)outNumRecordsUpdated;
-(int)getUserRecord:(PWFileEntry *)inOutUserRec fromName:(const char *)inName;
-(int)getUserRecord:(PWFileEntry *)inOutUserRec fromPrincipal:(const char *)inPrincipal;
-(void)requireNewPasswordForAllAccounts:(BOOL)inRequired;
-(void)kerberizeOrNewPassword;
-(int)doActionForAllOverflowFiles:(OverflowAction)inAction
		principal:(const char *)inPrincipal
		userRec:(PWFileEntry *)inOutUserRec
		purgeBefore:(time_t)beforeSecs;

-(BOOL)getOverflowFileList:(CFMutableArrayRef *)outFileArray customPath:(const char *)inCustomPath;	// default NULL

// protected-ish
-(int)openOverflowFile:(PWFileEntry *)inPasswordRec create:(BOOL)create fileDesc:(FILE **)outFP filePath:(char **)outFilePath;
-(int)getFileNumberFromPath:(const char *)inFilePath;
-(void)getOverflowFileName:(char *)outFileName forRec:(PWFileEntry *)inPasswordRec;
-(long)simpleHash:(PWFileEntry *)inPasswordRec;

@end
