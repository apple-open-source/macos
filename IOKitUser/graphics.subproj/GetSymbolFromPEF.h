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
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
    File:        GetSymbolFromPEF.h

    Contains:    xxx put contents here xxx

    Written by:    Jeffrey Robbin

    Copyright:    © 1994 by Apple Computer, Inc., all rights reserved.

    Change History (most recent first):

         <2>     9/20/94    TS        Dump the globals!
         <1>      9/6/94    TS        first checked in

*/

#ifndef __GETSYMBOLFROMPEF__
#define __GETSYMBOLFROMPEF__

//#include <Types.h>

#pragma options align=mac68k

#define NewPtrSys(a)    malloc(a)
#define DisposePtr(a)    free(a)

/*
    Container information
*/

struct ContainerHeader {               // File/container header layout:
    unsigned long    magicCookie;      // PEF container magic cookie
    unsigned long    containerID;      // 'peff'
    unsigned long    architectureID;   // 'pwpc' | 'm68k'
    unsigned long    versionNumber;    // format version number
    unsigned long    dateTimeStamp;    // date/time stamp (Mac format)
    unsigned long    oldDefVersion;    // old definition version number
    unsigned long    oldImpVersion;    // old implementation version number
    unsigned long    currentVersion;   // current version number
    short            nbrOfSections;    // nbr of sections (rel. 1)
    short            loadableSections; // nbr of loadable sectionsfor execution
                                       //   (= nbr of 1st non-loadable section)
    LogicalAddress    memoryAddress;   // location this container was last loaded
};
typedef struct ContainerHeader ContainerHeader, *ContainerHeaderPtr;

#define  kMagic1  'joy!'
#define  kMagic2  'peff'


/*
    Section information
*/

struct SectionHeader {               // Section header layout:
    long            sectionName;     // section name (str tbl container offset)
    unsigned long   sectionAddress;  // preferred base address for the section
    long            execSize;        // execution (byte) size including 0 init)
    long            initSize;        // init (byte) size before 0 init
    long            rawSize;         // raw data size (bytes)
    long            containerOffset; // container offest to section's raw data
    unsigned char   regionKind;      // section/region classification
    unsigned char   shareKind;       // sharing classification
    unsigned char   alignment;       // log 2 alignment
    unsigned char   reserved;        // <reserved>
};
typedef struct SectionHeader SectionHeader, *SectionHeaderPtr;

/* regionKind section classification: */
/* loadable sections */
#define kCodeSection      0U  /* code section */
#define kDataSection      1U  /* data section */
#define kPIDataSection    2U  /* "pattern" initialized data */
#define kConstantSection  3U  /* read-only data */
#define kExecDataSection  6U  /* "self modifying" code (!?) */
/* non-loadable sections */
#define kLoaderSection    4U  /* loader */
#define kDebugSection     5U  /* debugging info */
#define kExceptionSection 7U  /* exception data */
#define kTracebackSection 8U  /* traceback data */


/*
    Loader Information
*/

struct LoaderHeader {     // Loader raw data section header layout:
    long entrySection;    // entry point section number
    long entryOffset;     // entry point descr. ldr section offset
    long initSection;     // init routine section number
    long initOffset;      // init routine descr. ldr section offset
    long termSection;     // term routine section number
    long termOffset;      // term routine descr. ldr section offset
    long nbrImportIDs;    // nbr of import container id entries
    long nbrImportSyms;   // nbr of import symbol table entries
    long nbrRelocSects;   // nbr of relocation sections (headers)
    long relocsOffset;    // reloc. instructions ldr section offset
    long strTblOffset;    // string table ldr section offset
      long slotTblOffset; // hash slot table ldr section offset
    long hashSlotTblSz;   // log 2 of nbr of hash slot entries
    long nbrExportSyms;   // nbr of export symbol table entries
};
typedef struct LoaderHeader LoaderHeader, *LoaderHeaderPtr;

struct LoaderHashSlotEntry { // Loader export hash slot table entry layout:
    unsigned long slotEntry; // chain count (0:13), chain index (14:31)
};
typedef struct LoaderHashSlotEntry LoaderHashSlotEntry, *LoaderHashSlotEntryPtr;

struct LoaderExportChainEntry { // Loader export hash chain tbl entry layout:
    union {
        unsigned long _hashWord; // name length and hash value
        struct {
            unsigned short _nameLength; // name length is top half of hash word
            unsigned short doNotUseThis; // this field should never be accessed!
        } _h_h;
    } _h;
};
typedef struct LoaderExportChainEntry LoaderExportChainEntry, *LoaderExportChainEntryPtr;

struct ExportSymbolEntry {        // Loader export symbol table entry layout:
    unsigned long class_and_name; // symClass (0:7), nameOffset (8:31)
    long address;                 // ldr section offset to exported symbol
    short sectionNumber;          // section nbr that this export belongs to
};
typedef struct ExportSymbolEntry ExportSymbolEntry, *ExportSymbolEntryPtr;



/*
    Unpacking Information
*/

 /* "pattern" initialized data opcodes: */
#define kZero              0U     /* zero (clear) bytes */
#define kBlock             1U     /* block transfer bytes */
#define kRepeat            2U     /* repeat block xfer bytes */
#define kRepeatBlock       3U     /* repeat block xfer with contant prefix */
#define kRepeatZero        4U     /* repeat block xfer with contant prefix 0 */
#define kOpcodeShift       0x05U  /* shift to access opcode */
#define kFirstOperandMask  0x1FU  /* mask to access 1st operand (count) */

#define PIOP(x)  (unsigned char)((x) >> kOpcodeShift)  /* extract opcode */
#define PICNT(x)  (long)((x) & kFirstOperandMask) /* extract 1st operand (count) */

/* The following macros are used for extracting count value operands from pidata...            */

#define kCountShift  0x07UL  /* value shift to concat count bytes */
#define kCountMask   0x7FUL  /* mask to extract count bits from a byte */
#define IS_LAST_PICNT_BYTE(x) (((x) & 0x80U) == 0) /* true if last count byte */
#define CONCAT_PICNT(value, x) (((value)<<kCountShift) | ((x) & kCountMask))



/*
    Function Prototypes
*/

static OSStatus GetSymbolFromPEF(
    StringPtr      theSymbolName,
    LogicalAddress thePEFPtr,
    LogicalAddress theSymbolPtr,
    ByteCount      theSymbolSize);

static Boolean SymbolCompare(
    StringPtr     theLookedForSymbol,
    StringPtr     theExportSymbol,
    unsigned long theExportSymbolLength);
                        
static OSErr UnpackPiData(
    LogicalAddress   thePEFPtr,
    SectionHeaderPtr sectionHeaderPtr,
    LogicalAddress * theData);

static unsigned char PEFGetNextByte(
    unsigned char ** rawBuffer,
    long * rawBufferRemaining);

static unsigned long PEFGetCount(
    unsigned char ** rawBuffer,
    long * rawBufferRemaining);


#pragma options align=reset

#endif
