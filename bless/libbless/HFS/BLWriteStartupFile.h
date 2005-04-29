/*
 * Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
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
 *  Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLWriteStartupFile.h,v 1.7 2005/02/03 00:42:26 ssen Exp $
 *
 *  $Log: BLWriteStartupFile.h,v $
 *  Revision 1.7  2005/02/03 00:42:26  ssen
 *  Update copyrights to 2005
 *
 *  Revision 1.6  2004/04/20 21:40:42  ssen
 *  Update copyrights to 2004
 *
 *  Revision 1.5  2003/07/22 15:58:31  ssen
 *  APSL 2.0
 *
 *  Revision 1.4  2003/04/19 00:11:08  ssen
 *  Update to APSL 1.2
 *
 *  Revision 1.3  2003/04/16 23:57:31  ssen
 *  Update Copyrights
 *
 *  Revision 1.2  2003/03/20 03:40:57  ssen
 *  Merge in from PR-3202649
 *
 *  Revision 1.1.10.1  2003/03/20 03:11:42  ssen
 *  swap XCOFF data structures
 *
 *  Revision 1.1  2001/11/16 05:36:47  ssen
 *  Add libbless files
 *
 *  Revision 1.6  2001/10/26 04:19:41  ssen
 *  Add dollar Id and dollar Log
 *
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
