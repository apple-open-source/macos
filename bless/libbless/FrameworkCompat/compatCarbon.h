/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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
 *  compatCarbon.h
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Mon Apr 23 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: compatCarbon.h,v 1.1 2001/11/16 05:36:46 ssen Exp $
 *
 *  $Log: compatCarbon.h,v $
 *  Revision 1.1  2001/11/16 05:36:46  ssen
 *  Add libbless files
 *
 *  Revision 1.8  2001/11/11 06:20:59  ssen
 *  readding files
 *
 *  Revision 1.6  2001/10/26 04:19:40  ssen
 *  Add dollar Id and dollar Log
 *
 *
 */

#define SInt16 int16_t
#define UInt16 u_int16_t
#define UInt32 u_int32_t
#define SInt8 int8_t
 
/* Lifted unceremoniously from MacTypes.h and CarbonCore */
typedef SInt16                          OSErr;
typedef unsigned char                   Str63[64];
typedef Str63                           StrFileName;
struct FSSpec {
  short               vRefNum;
  long                parID;
  StrFileName         name;                   /* a Str63 on MacOS*/
};
typedef struct FSSpec                   FSSpec;


typedef char *                          Ptr;
typedef Ptr *                           Handle;
typedef unsigned long                   FourCharCode;
typedef FourCharCode                    OSType;

typedef UInt16                          UniChar;

struct HFSUniStr255 {
  UInt16              length;                 /* number of unicode characters */
  UniChar             unicode[255];           /* unicode characters */
};
typedef struct HFSUniStr255             HFSUniStr255;

struct Point {
  short               v;
  short               h;
};
typedef struct Point                    Point;

struct FInfo {
  OSType              fdType;
  OSType              fdCreator;
  UInt16              fdFlags;
  Point               fdLocation;
  SInt16              fdFldr;
};
typedef struct FInfo                    FInfo;
typedef UInt32                          ByteCount;


#define fsRdPerm 0x01
#define fsRdWrPerm 0x03
#define fsRtDirID 2
#define smSystemScript -1
#define fsFromStart 1

/* From Finder.h */
enum {
  kIsOnDesk                     = 0x0001, /* Files and folders (System 6) */
  kColor                        = 0x000E, /* Files and folders */
  kIsShared                     = 0x0040, /* Files only (Applications only) */
  kHasNoINITs                   = 0x0080, /* Files only (Extensions/Control Panels only) */
  kHasBeenInited                = 0x0100, /* Files only */
  kHasCustomIcon                = 0x0400, /* Files and folders */
  kIsStationery                 = 0x0800, /* Files only */
  kNameLocked                   = 0x1000, /* Files and folders */
  kHasBundle                    = 0x2000, /* Files only */
  kIsInvisible                  = 0x4000, /* Files and folders */
  kIsAlias                      = 0x8000  /* Files only */
};

OSErr _BLNativePathNameToFSSpec(char * A, FSSpec * B, long C);
OSErr _BLFSMakeFSSpec(short A, long B, signed char * C, FSSpec * D);
short _BLFSpOpenResFile(const FSSpec *  A, SInt8 B);
Handle _BLGet1Resource( FourCharCode A, short B) ;
void _BLDetachResource(Handle A);
void _BLDisposeHandle(Handle A);
void _BLCloseResFile(short A);

