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

/*
 *  ExportController.h
 *  DSTools
 */

#import <Foundation/Foundation.h>
#import <OpenDirectory/NSOpenDirectory.h>
#import <DirectoryService/DirServicesTypes.h>

#define kExportExceptionName			"ExportException"

@interface ExportController : NSObject
{
	NSString*			_filePath;
    NSString*           _tmpFilePath;
	NSString*			_nodePath;
	NSString*			_remoteNetAddress;
	NSString*			_userName;
	NSString*			_userPass;
	NSString*			_recordType;
    NSString*           _returnAttributes;

	NSArray*			_recordsToExport;
    NSMutableArray*     _attributesToBeExcluded;
	
    ODNode*             _node;
    
	NSFileHandle*		_outputFile;
    NSFileHandle*		_tmpFile;
    NSString*           _tmpDir;
	
	NSArray*			_attributesToExport;
	NSArray*			_attributesForHeader;
	
	BOOL				_showUsage;
    BOOL                _determineHeader;
    BOOL                _exportAll;
}

- (ExportController*)initWithArgs:(const char**)argv numArgs:(int)argc;
- (void)export;

- (NSString*)filePath;
- (NSString*)nodePath;
- (NSString*)remoteNetAddress;
- (NSString*)userName;
- (NSString*)userPass;
- (NSArray*)recordsToExport;


@end
