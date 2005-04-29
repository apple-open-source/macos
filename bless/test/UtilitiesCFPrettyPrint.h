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
