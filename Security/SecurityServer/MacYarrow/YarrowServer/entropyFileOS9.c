/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
	File:		entropyFile.c

	Contains:	Module to maintain MacYarrow's entropy file.

	Written by:	Doug Mitchell

	Copyright: (c) 2000 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		02/29/00	dpm		Created.
 
*/

#include "entropyFile.h"
#include "debug.h"
#include <Files.h>
#include <Folders.h>
#include <Errors.h>
#include <Script.h>		// for smSystemScript

/*
 * FIXME - for debugging, we put the entropy file the current user's 
 * preferences folder.  For the real thing, we should either put it in
 * System preferences or use UNIX I/O to specify some other path. 
 */
#ifdef	DEBUG
#define ENTROPY_FOLDER			kPreferencesFolderType
#else
#define ENTROPY_FOLDER			kSystemPreferencesFolderType
#endif
#define ENTROPY_FILE_NAME		"\pSystem Entropy"
#define ENTROPY_FILE_CREATOR	'yarw'
#define ENTROPY_FILE_TYPE		'ENTR'

/*
 * Open/create entropy file. fnfErr returned if doCreate is false and
 * the file doesn't exist.  
 */
static OSErr openEntropyFile(
	Boolean doCreate,
	Boolean writeAccess,		// required if doCreate true
	short	*refNum)			// RETURNED
{
	FSSpec		fsp;
	OSErr 		ortn;
	short 		vRefNum;
	long 		dirID;
	SInt8 		perm;
	
	if(doCreate && !writeAccess) {
		return paramErr;
	}
	*refNum = 0;
	ortn = FindFolder(kOnSystemDisk, 
		ENTROPY_FOLDER, 
		kDontCreateFolder, 
		&vRefNum, 
		&dirID);
	if(ortn) {
		errorLog1("openEntropyFile: FindFolder returned %d\n", (int)ortn);
		return ioErr;
	}
	ortn = FSMakeFSSpec(vRefNum, dirID, ENTROPY_FILE_NAME, &fsp);
	switch(ortn) {
		case noErr:
			break;
		case fnfErr:
			if(!doCreate) {
				return fnfErr;
			}
			else {
				break;
			}
		default:
			errorLog1("openEntropyFile: FSMakeFSSpec returned %d\n", (int)ortn);
			return ioErr;
	}

	if(doCreate && (ortn == fnfErr)) {
		/* create it */
		ortn = FSpCreate(&fsp, 
			ENTROPY_FILE_CREATOR, 
			ENTROPY_FILE_TYPE, 
			smSystemScript);
		if(ortn) {
			errorLog1("openEntropyFile: FSpCreate returned %d\n", (int)ortn);
			return ortn;
		}
		
		/* fixme - set FInfo.fdFlags.kIsInvisible? */
	}

	/* open it in any case */
	perm = (writeAccess ? fsRdWrPerm : fsRdPerm);
	ortn = FSpOpenDF(&fsp, perm, refNum);
	if(ortn) {
		errorLog1("openEntropyFile: FSpOpenDF returned %d\n", (int)ortn);
	}
	return ortn;
}

/*
 * Write specified data to entropy file. A new file will be created
 * if none exists. Data will be appended to possible existing data
 * if append is true, otherwise the file's data is replaced with 
 * caller's data.
 */
OSErr writeEntropyFile(
	UInt8		*bytes,
	UInt32		numBytes,
	Boolean		append)
{
	OSErr ortn;
	short refNum;
	long  eof;
	long  actLength = numBytes;
	
	ortn = openEntropyFile(true, true, &refNum);
	if(ortn) {
		return ortn;
	}
	if(append) {
		ortn = GetEOF(refNum, &eof);
		if(ortn) {
			goto done;
		}
	}
	else {
		/* truncate to 0 */
		ortn = SetEOF(refNum, 0);
		if(ortn) {
			goto done;
		}
		eof = 0;
	}
	ortn = SetFPos(refNum, fsFromStart, eof);
	if(ortn) {
		goto done;
	}
	ortn = FSWrite(refNum, &actLength, bytes);
	if((ortn == noErr) && (actLength != numBytes)) {
		errorLog0("writeEntropyFile: short write\n");
	}
done:
	FSClose(refNum);
	return ortn;
}
	
/*
 * Read data from entropy file.
 */
OSErr readEntropyFile(
	UInt8		*bytes,
	UInt32		numBytes,		// max # of bytes to read
	UInt32		*actualBytes)	// RETURNED - number of bytes actually read
{
	OSErr ortn;
	short refNum;
	long  actLength = numBytes;
	
	ortn = openEntropyFile(false, false, &refNum);
	if(ortn) {
		return ortn;
	}
	ortn = FSRead(refNum, &actLength, bytes);
	*actualBytes = actLength;
	FSClose(refNum);
	return ortn;
}
