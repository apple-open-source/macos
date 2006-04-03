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
/*
 *  UtilitiesCFPrettyPrint.h
 *  DiskImages
 *
 *  Moved here from UtilitiesCoreFoundation.[ch]
 *
 *  Created by Shantonu Sen on 8/11/04.
 *  Copyright 2004 Apple Computer, Inc. All rights reserved.
 *
 *  Revision History
 *  
 *  $Log: UtilitiesCFPrettyPrint.h,v $
 *  Revision 1.2  2005/12/04 05:27:52  ssen
 *  add APSL to remaining source files
 *
 *  Revision 1.1  2005/02/07 22:32:51  ssen
 *  dump DiskArb info
 *
 *  Revision 1.1  2004/08/11 18:52:10  ssen
 *  Move TAOCFPrettyPrint code to a separate source file, so that
 *  clients don't need to drag in lots of dependencies on other
 *  parts of TAOcommon or CoreServices
 *  Bug #:
 *  Submitted by:
 *  Reviewed by:
 *
 */

#ifndef _UTILITIES_CFPRETTYPRINT_H_
#define _UTILITIES_CFPRETTYPRINT_H_

#ifdef __cplusplus
extern "C" {
#endif


#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>

/*
 *	@function TAOCFPrettyPrint
 * 	@abstract Prints a CFType in a human-readable format
 *	@param inRef A CFTypeRef to be printed. Scalars (CFString, CFNumber, etc.)
 are printed on a line by themselves. CFArray's are printed one element
 per line. CFDictionaries are printed as "key: value", one line per pair.
 In the event that the value is a collection (CFArray or CFDictionary),
 just "key: " is printed, and the value is printed on the next line,
 indented by one level.
 * 	@result The CFType summary is printed to standard out.
 *	@discussion This function calls through to TAOCFPrettyPrintToFile()
 *   with stdout as the file argument.
 */

void TAOCFPrettyPrint(CFTypeRef inRef);

/*
 *	@function TAOCFPrettyPrintWithIndenter
 * 	@abstract Prints a CFType in a human-readable format
 *	@param inRef A CFTypeRef to be printed. Scalars (CFString, CFNumber, etc.)
 are printed on a line by themselves. CFArray's are printed one element
 per line. CFDictionaries are printed as "key: value", one line per pair.
 In the event that the value is a collection (CFArray or CFDictionary),
 just "key: " is printed, and the value is printed on the next line,
 indented by one level.
 *  @param indent A string to be used for each level of indentation
 * 	@result The CFType summary is printed to standard out.
 *	@discussion This function calls through to TAOCFPrettyPrintToFileWithIndenter()
 *   with stdout as the file argument and indent as the indenter
 */
void TAOCFPrettyPrintWithIndenter(CFTypeRef inRef, char *indent);

/*
 *	@function TAOCFPrettyPrintToFile
 * 	@abstract Prints a CFType in a human-readable format to a file stream
 *	@param inRef See description under TAOCFPrettyPrint()
 *  @param out The output stream to print to.
 * 	@result The CFType summary is printed to the stream out.
 */

void TAOCFPrettyPrintToFile(CFTypeRef inRef, FILE *out);

/*
 *	@function TAOCFPrettyPrintToFileWithIndenter
 * 	@abstract Prints a CFType in a human-readable format to a file stream
 *	@param inRef See description under TAOCFPrettyPrint() and
 TAOCFPrettyPrintWithIdentifier
 *  @param out The output stream to print to.
 *  @param indent A string to be used for each level of indentation
 * 	@result The CFType summary is printed to the stream out.
 */

void TAOCFPrettyPrintToFileWithIndenter(CFTypeRef inRef, FILE *out, char *indent);

#ifdef __cplusplus
};
#endif

#endif
