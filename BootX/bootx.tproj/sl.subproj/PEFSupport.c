/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
	File:		UnpackPiData.c
	Written by:	Jeffrey Robbin
	Copyright:	© 1994, 1996 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):
		 <3>	 9/20/96	MRN		Changed "" to <> on including Types.h, Errors.h and Memory.h
									that was breaking MW.
		 <2>	 9/20/94	TS		Dump the globals!
		 <1>	  9/6/94	TS		first checked in


	File:		GetSymbolFromPEF.c
	Written by:	Jeffrey Robbin
	Copyright:	© 1994, 1996 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		 <6>	 9/20/96	MRN		Changed <> to "" on including GetSymbolFromPEF.h that was
									breaking MW.
		 <5>	  3/7/96	MRN		Minor adjustments to build with the latest version of
									SuperMarioLatestBuild name revision of MasterInterfaces.
		 <4>	10/13/94	JF		Changed CFM error enums-- from 'cfragXXXErr' to 'fragXXX'
		 <3>	10/11/94	TS		Move to new CFM error codes
		 <2>	 9/16/94	TS		Switch to pascal strings in SymbolCompare function
		 <1>	  9/6/94	TS		first checked in

*/

typedef unsigned char Boolean;
typedef unsigned char *Ptr;
typedef char *StringPtr;

typedef long OSErr;

typedef unsigned long OSStatus;
typedef unsigned long ByteCount;
typedef unsigned long LogicalAddress;

#define nil (void *)0
#define false (0)
#define true (1)
#define noErr (0)

#include "GetSymbolFromPEF.h"
//#include <Types.h>
//#include <Errors.h>
//#include <Memory.h>


unsigned char PEFGetNextByte (unsigned char** rawBuffer, long* rawBufferRemaining)
{
	*rawBufferRemaining = *rawBufferRemaining - 1;
	return *(*rawBuffer)++;
}


unsigned long PEFGetCount(unsigned char** rawBuffer, long* rawBufferRemaining)
{
	register unsigned char b;
	register unsigned long value = 0UL;
  
	/* Scan the count value.  All required bytes MUST be present...										*/
	
	b = PEFGetNextByte(rawBuffer, rawBufferRemaining);
	if (!IS_LAST_PICNT_BYTE(b)) {							/* if 1st byte is not that last...			*/
		value = CONCAT_PICNT(value, b);						/*   ...init value using 1st byte			*/
      
		b = PEFGetNextByte(rawBuffer, rawBufferRemaining);
		if (!IS_LAST_PICNT_BYTE(b)) {						/* 	 if 2nd byte is not the last...			*/
			value = CONCAT_PICNT(value, b);					/*	 ...add in 2nd byte						*/
        
			b = PEFGetNextByte(rawBuffer, rawBufferRemaining);
			if (!IS_LAST_PICNT_BYTE(b)) {					/* 	   if 3rd byte is not the last... 		*/
				value = CONCAT_PICNT(value, b);				/*	   ...add in 3rd byte					*/
           
				b = PEFGetNextByte(rawBuffer, rawBufferRemaining);
				if (!IS_LAST_PICNT_BYTE(b)) {				/*		   if 4th byte is not the last...	*/
					value = CONCAT_PICNT(value, b);			/*		   ...add in 4th byte				*/
	
															/* 		 5th byte is definitly last!		*/
					b = PEFGetNextByte(rawBuffer, rawBufferRemaining);	
				}
			}
		}
	}

	value = CONCAT_PICNT(value, b);							/* add in "last" byte (whichever one)		*/

	return (value);
}


void MemSet (unsigned char* theBuffer, long theValue, long theCount)
{
	while (theCount-- > 0)
		*theBuffer++ = theValue;
}


// UnpackPiData expands a compressed section into memory.

OSErr UnpackPiData (LogicalAddress		thePEFPtr,
					SectionHeaderPtr	sectionHeaderPtr,
					LogicalAddress*		theData)
{
	long 		 	cntX, cnt, rpt, dcnt, delta;
	unsigned char	op, b;
	unsigned char*	unpackBuffer;
	unsigned char*	originalUnpackBuffer;
	unsigned char*	endUnpackBuffer;
	unsigned char*	oldRawBuffer;
	long			oldRawBufferRemaining;
	unsigned char*	rawBuffer;
	long			rawBufferRemaining;
	
	// Verify incoming section is packed.
	if (sectionHeaderPtr->regionKind != kPIDataSection)
		return (paramErr);
	

	// Allocate memory to unpack into
	originalUnpackBuffer = (unsigned char*)NewPtrSys(sectionHeaderPtr->initSize);
	if (originalUnpackBuffer == nil)
		return memFullErr;

	unpackBuffer		= originalUnpackBuffer;
	endUnpackBuffer 	= unpackBuffer + sectionHeaderPtr->initSize;
	rawBuffer			= (unsigned char*)((unsigned long)thePEFPtr + sectionHeaderPtr->containerOffset);
	rawBufferRemaining	= sectionHeaderPtr->rawSize;

	
	/* Expand the pidata instructions.  EOF will terminate processing through the setjmp	*/
	/* on pidData_jmpbuf above...															*/

	while (rawBufferRemaining > 0) {
		
		/* The first byte of each instruction contains the opcode and a count. If the count	*/
		/* is 0, the count starts in the next byte...										*/
		
		/* Pick up the opcode and first count operand...									*/
		
		b = PEFGetNextByte(&rawBuffer, &rawBufferRemaining);
		
		op  = PIOP(b);
		cnt = PICNT(b);
		
		if (cnt == 0)
			cnt = PEFGetCount(&rawBuffer, &rawBufferRemaining);
		
		/* Unpack the data as a function of the opcode...									*/
		
		switch (op) {
			case kZero:																/* zero out cnt bytes...*/
				if (unpackBuffer + cnt > endUnpackBuffer)
					goto Error;
				MemSet(unpackBuffer, 0, cnt);
				unpackBuffer += cnt;
				break;
			
			case kBlock:															/* copy cnt bytes...*/
				if (unpackBuffer + cnt > endUnpackBuffer)
					goto Error;
				while (--cnt >= 0)
					*unpackBuffer++ = PEFGetNextByte(&rawBuffer, &rawBufferRemaining);
				break;
				
			case kRepeat:															/* copy cnt bytes rpt times...*/
				rpt = PEFGetCount(&rawBuffer, &rawBufferRemaining) + 1;
				
				if (cnt == 1)
				{
					if (unpackBuffer + rpt > endUnpackBuffer)
						goto Error;
					b = PEFGetNextByte(&rawBuffer, &rawBufferRemaining);
					MemSet(unpackBuffer, b, rpt);
					unpackBuffer += rpt;
				}
				else
				{
					oldRawBufferRemaining	= rawBufferRemaining;
					oldRawBuffer			= rawBuffer;
					while (--rpt >= 0) {
						if (unpackBuffer + cnt > endUnpackBuffer)
							goto Error;
						rawBufferRemaining	= oldRawBufferRemaining;
						rawBuffer			= oldRawBuffer;
						cntX = cnt;
						while (--cntX >= 0)
							*unpackBuffer++ = PEFGetNextByte(&rawBuffer, &rawBufferRemaining);
					}
				}
				break;
				
			case kRepeatZero:														/* copy cnt 0's and dcnt bytes rpt times*/
				dcnt 			 = PEFGetCount(&rawBuffer, &rawBufferRemaining);	/* ...then copy cnt more 0's			*/
				rpt  			 = PEFGetCount(&rawBuffer, &rawBufferRemaining);
				
				goto rptPart1;										/* jump into loop to copy 0's first...	*/
				
				while (--rpt >= 0) {
					if (unpackBuffer + dcnt > endUnpackBuffer)
						goto Error;
					cntX = dcnt;									/* cnt repeating parts follow each other*/
					while (--cntX >= 0)
						*unpackBuffer++ = PEFGetNextByte(&rawBuffer, &rawBufferRemaining);
rptPart1:															/* non-repeating part is always 0's...	*/
					if (unpackBuffer + cnt > endUnpackBuffer)
						goto Error;
					MemSet(unpackBuffer, 0, cnt);
					unpackBuffer += cnt;
				}
				
				break;
				
			case kRepeatBlock:										/* copy cnt repeating bytes and dcnt 		*/
				dcnt 			 = PEFGetCount(&rawBuffer, &rawBufferRemaining);					/* non-repating bytes rcnt times...			*/
				rpt  			 = PEFGetCount(&rawBuffer, &rawBufferRemaining);					/* ...then copy cnt repeating bytes			*/
				
				oldRawBufferRemaining	= rawBufferRemaining;
				oldRawBuffer			= rawBuffer;
				delta					= 0;						/*  the repeating part and each non-rep	*/
				
				goto rptPart2;										/* jump into loop to copy rptng part 1st*/
				
				while (--rpt >= 0) {
					if (unpackBuffer + dcnt > endUnpackBuffer)
						goto Error;
					
					rawBuffer			= oldRawBuffer + cnt + delta;
					rawBufferRemaining	= oldRawBufferRemaining - (cnt + delta);
					cntX = dcnt;
					while (--cntX >= 0)
						*unpackBuffer++ = PEFGetNextByte(&rawBuffer, &rawBufferRemaining);
					delta += dcnt;
rptPart2:															
					if (unpackBuffer + cnt > endUnpackBuffer)
						goto Error;
					rawBuffer			= oldRawBuffer;
					rawBufferRemaining	= oldRawBufferRemaining;
					cntX = cnt;
					while (--cntX >= 0)
						*unpackBuffer++ = PEFGetNextByte(&rawBuffer, &rawBufferRemaining);
				}
				
				rawBuffer			= oldRawBuffer + cnt + delta;
				rawBufferRemaining	= oldRawBufferRemaining - (cnt + delta);
				break;
			
			default:
				goto Error;
		} /* switch */
	} /* for */
	
	*theData = originalUnpackBuffer;
		
	return noErr;

Error:
	if (unpackBuffer)
		DisposePtr((Ptr)originalUnpackBuffer);
	
	*theData = nil;

	return paramErr;
}




// GetSymbolFromPEF will extract from a PEF container the data associated
// with a given symbol name.  It requires that the PEF file have been previously
// loaded into memory.

OSStatus GetSymbolFromPEF (	StringPtr		theSymbolName,
							LogicalAddress	thePEFPtr,
							LogicalAddress	theSymbolPtr,
							ByteCount		theSymbolSize)
{
	ContainerHeaderPtr			containerHeaderPtr;	// Pointer to the Container Header
	SectionHeaderPtr			loaderSectionPtr;	// Pointer to the Loader Section Header
	SectionHeaderPtr			exportSectionPtr;	// Pointer to the Section Header with the symbol
	short						currentSection;
	Boolean						foundSection;
	Boolean						foundSymbol;
	long						numExportSymbols;
	LoaderHeaderPtr				loaderHeaderPtr;
	ExportSymbolEntryPtr		exportSymbolEntryPtr;
	LoaderExportChainEntryPtr	exportChainEntryPtr;
	StringPtr					exportSymbolName;
	LogicalAddress				expandedDataPtr;
	unsigned char*				sourceDataPtr;
	unsigned char*				destDataPtr;
	

	containerHeaderPtr = (ContainerHeaderPtr)thePEFPtr;
	
	// Does the magic cookie match?
	if (containerHeaderPtr->magicCookie != 'Joy!')
		return cfragFragmentFormatErr;
	
	// Is this a known PEF container format?
	if (containerHeaderPtr->containerID != 'peff')
		return cfragFragmentFormatErr;
	
	// Validate parameters
	if (theSymbolPtr == nil)
		return paramErr;
	
	
	// Find the loader section.
	foundSection = false;
	for (currentSection = 0; currentSection < containerHeaderPtr->nbrOfSections; currentSection++)
	{
		loaderSectionPtr = (SectionHeaderPtr)(	(unsigned long)containerHeaderPtr +
												sizeof(ContainerHeader) +
												(sizeof(SectionHeader) * currentSection));
		if (loaderSectionPtr->regionKind == kLoaderSection)
		{
			foundSection = true;
			break;
		}
	}
	
	if (foundSection == false)
		return cfragNoSectionErr;
	
	// Get the number of export symbols.
	loaderHeaderPtr		= (LoaderHeaderPtr)((unsigned long)thePEFPtr + loaderSectionPtr->containerOffset);
	numExportSymbols	= loaderHeaderPtr->nbrExportSyms;
	
	// Start at the first exported symbol.
	exportSymbolEntryPtr = (ExportSymbolEntryPtr)(	(unsigned long)loaderHeaderPtr +
													loaderHeaderPtr->slotTblOffset +
													(sizeof(LoaderHashSlotEntry) * (1<<loaderHeaderPtr->hashSlotTblSz)) +
													(sizeof(LoaderExportChainEntry) * numExportSymbols));
													
	exportChainEntryPtr = (LoaderExportChainEntryPtr)(	(unsigned long)loaderHeaderPtr +
													loaderHeaderPtr->slotTblOffset +
													(sizeof(LoaderHashSlotEntry) * (1<<loaderHeaderPtr->hashSlotTblSz)));
	
	foundSymbol = false;
	while (numExportSymbols-- > 0)
	{
		exportSymbolName = (StringPtr)(	(unsigned long)loaderHeaderPtr +
										loaderHeaderPtr->strTblOffset +
										(exportSymbolEntryPtr->class_and_name & 0x00FFFFFF));
		if (SymbolCompare(theSymbolName, exportSymbolName, exportChainEntryPtr->_h._h_h._nameLength))
		{
			foundSymbol = true;
			break;
		}
        exportSymbolEntryPtr = (ExportSymbolEntryPtr)(((int)exportSymbolEntryPtr) + 10);
		exportChainEntryPtr++;
	}
	
	if (foundSymbol == false)
		return cfragNoSymbolErr;
		
	
	// Found the symbol, so... let's go get the data!
		
	exportSectionPtr = (SectionHeaderPtr)(	(unsigned long)containerHeaderPtr +
											sizeof(ContainerHeader) +
											(sizeof(SectionHeader) * exportSymbolEntryPtr->sectionNumber));
	
	expandedDataPtr = nil;
	
	switch (exportSectionPtr -> regionKind)
	{
		case kPIDataSection:
		
			// Expand the data!  (Not yet... :)
			
			if (UnpackPiData (thePEFPtr, exportSectionPtr, &expandedDataPtr) != noErr)
				return cfragFragmentCorruptErr;
		
			sourceDataPtr = (unsigned char*)((unsigned long)expandedDataPtr +
							exportSymbolEntryPtr->address);
			break;
		
		default:
			sourceDataPtr = (unsigned char*)((unsigned long)thePEFPtr +
							exportSectionPtr->containerOffset +
							exportSymbolEntryPtr->address);
			break;
	}
	
	
	// Copy the data!
	
	destDataPtr = (unsigned char*)theSymbolPtr;
	
	
	while (theSymbolSize-- > 0)
		*destDataPtr++ = *sourceDataPtr++;
	
	
	// Cleanup any expanded data
	
	if (expandedDataPtr != nil)
		DisposePtr((Ptr)expandedDataPtr);

	return noErr;
}


//
// SymbolCompare
//
// theExportSymbol is NOT null-terminated, so use theExportSymbolLength.
//
Boolean	SymbolCompare (	StringPtr		theLookedForSymbol,
						StringPtr		theExportSymbol,
						unsigned long	theExportSymbolLength)
{
	unsigned char*	p1 = (unsigned char*)theLookedForSymbol;
	unsigned char*	p2 = (unsigned char*)theExportSymbol;
	
	// Same length?
	// (skip over p string len byte)
	if ( theExportSymbolLength != *p1++ )
		return false;
		
	while ( theExportSymbolLength-- != 0 )
	{
		if ( *p1++ != *p2++ )
			return false;
	}
	
	return true;
}
