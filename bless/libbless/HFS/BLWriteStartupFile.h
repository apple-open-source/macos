/*
 * Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
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
 *  writeStartupFile.h
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Mon Jun 25 2001.
 *  Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: BLWriteStartupFile.h,v 1.8 2006/02/20 22:49:55 ssen Exp $
 *
 */

// Defines
#ifndef BlockMoveData
#define BlockMoveData(s,d,l)    memcpy(d,s,l)
#endif

// Typedefs
typedef struct {
  /* File header */
  UInt16 magic;
#define kFileMagic                      0x1DF
  UInt16 nSections;
  UInt32 timeAndDate;
  UInt32 symPtr;
  UInt32 nSyms;
  UInt16 optHeaderSize;
  UInt16 flags;
} XFileHeader;

typedef struct {
  /* Optional header */
  UInt16 magic;
#define kOptHeaderMagic         0x10B
  UInt16 version;
  UInt32 textSize;
  UInt32 dataSize;
  UInt32 BSSSize;
  UInt32 entryPoint;
  UInt32 textStart;
  UInt32 dataStart;
  UInt32 toc;
  UInt16 snEntry;
  UInt16 snText;
  UInt16 snData;
  UInt16 snTOC;
  UInt16 snLoader;
  UInt16 snBSS;
  UInt8 filler[28];
} XOptHeader;

typedef struct {
  char name[8];
  UInt32 pAddr;
  UInt32 vAddr;
  UInt32 size;
  UInt32 sectionFileOffset;
  UInt32 relocationsFileOffset;
  UInt32 lineNumbersFileOffset;
  UInt16 nRelocations;
  UInt16 nLineNumbers;
  UInt32 flags;
} XSection;

enum SectionNumbers {
  kTextSN = 1,
  kDataSN,
  kBSSSN
};

const char kTextName[] = ".text";
const char kDataName[] = ".data";
const char kBSSName[] = ".bss";

/* @MAC_OS_X_END@ */
