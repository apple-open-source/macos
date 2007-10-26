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

/*!
 * @header PathManager
 */


#import <Foundation/Foundation.h>
#import <DirectoryService/DirServicesTypes.h>

@interface PathManager : NSObject
{
    NSMutableArray	*_stack;
    NSMutableArray	*_stackBackup;
	NSMutableArray	*_pushdPopdStack;
}

// Open a connection to the local machine.
- initWithLocal;

// Open a conection to a remote machine using DS Proxy.
- initWithHost:(NSString*)hostName user:(NSString*)user password:(NSString*)password;

// Open only a specific node via the local DS service
- initWithNodeEnum:(int)inNodeEnum;
- initWithNodeName:(NSString*)inNodeName;
- initWithNodeName:(NSString*)inNodeName user:(NSString*)inUsername password:(NSString*)inPassword;

// Open the local node via the local DS service (a specialized case of initWithNodeName)
- initWithLocalNode;
- initWithLocalNodeAuthUser:(NSString*)inUsername password:(NSString*)inPassword;

//open the slim version of the DS daemon with only the local node at the file path specified
- initWithLocalPath:(NSString*)filePath;

- (tDirStatus)authenticateUser:(NSString*)inUsername password:(NSString*)inPassword authOnly:(BOOL)inAuthOnly;
- (tDirStatus) setPasswordForUser:(NSString*)inRecordPath withParams:(NSArray*)inParams;

- (void)list:(NSString*)inPath key:(NSString*)inKey;
- (void)cd:(NSString*)dest;
- (void)pushd:(NSString*)dest;
- (void)popd;
- (NSString*)cwd;
- (NSArray*)getCurrentList:(NSString*)inPath;
- (NSArray*)getPossibleCompletionsFor:(NSString*)inPathAndPrefix;

- (tDirStatus)read:(NSString*)inPath keys:(NSArray*)inKeys;
- (tDirStatus)readAll:(NSString*)inPath keys:(NSArray*)inKeys;
- (tDirStatus)read:(NSString*)inPath key:(NSString*)inKey plistPath:(NSString*)inPlistPath;
- (tDirStatus)read:(NSString*)inPath key:(NSString*)inKey atIndex:(int)index plistPath:(NSString*)inPlistPath;
- (tDirStatus)create:(NSString*)inPath key:(NSString*)inKey plistPath:(NSString*)inPlistPath values:(NSArray*)inValues;
- (tDirStatus)create:(NSString*)inPath key:(NSString*)inKey atIndex:(int)index plistPath:(NSString*)inPlistPath values:(NSArray*)inValues;
- (tDirStatus)delete:(NSString*)inPath key:(NSString*)inKey plistPath:(NSString*)inPlistPath values:(NSArray*)inValues;
- (tDirStatus)delete:(NSString*)inPath key:(NSString*)inKey atIndex:(int)index plistPath:(NSString*)inPlistPath values:(NSArray*)inValues;
- (void)diff:(NSString*)inPath1 otherPath:(NSString*)inPath2 keys:(NSArray*)inKeys;

- (tDirStatus)createRecord:(NSString*)inRecordPath key:(NSString*)inKey values:(NSArray*)inValues;
- (tDirStatus)appendToRecord:(NSString*)inRecordPath key:(NSString*)inKey values:(NSArray*)inValues;
- (tDirStatus)mergeToRecord:(NSString*)inRecordPath key:(NSString*)inKey values:(NSArray*)inValues;
- (tDirStatus)deleteInRecord:(NSString*)inRecordPath key:(NSString*)inKey values:(NSArray*)inValues;
- (tDirStatus)deleteRecord:(NSString*)inRecordPath;
- (tDirStatus)changeInRecord:(NSString*)inRecordPath key:(NSString*)inKey values:(NSArray*)inValues;
- (tDirStatus)changeInRecordByIndex:(NSString*)inRecordPath key:(NSString*)inKey values:(NSArray*)inValues;

- (void)searchInPath:(NSString*)inPath forKey:(NSString*)inKey withValue:(NSString*)inValue matchType:(NSString*)inType;

// -----------------------
// Utilities:

// Create a trivial path (a single component, no path delimiter)
// And then cd into that path.
- (tDirStatus)createAndCd:(NSString*)inPath;

// Backup the path stack.
- (void)backupStack;

// Restore the path stack from the backup.
- (void)restoreStack;

- (void)printPushdPopdStack;

// -----------------------
// ATM - PlugInManager needs direct access to the stack
-(NSArray*) stack;
@end
