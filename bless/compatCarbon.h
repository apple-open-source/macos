/*
 *  compatCarbon.h
 *  bless
 *
 *  Created by ssen on Mon Apr 23 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

 
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

#define fsRdPerm 0x01


int loadCarbonCore();
OSErr _bless_NativePathNameToFSSpec(char * A, FSSpec * B, long C);
OSErr _bless_FSMakeFSSpec(short A, long B, signed char * C, FSSpec * D);
short _bless_FSpOpenResFile(const FSSpec *  A, SInt8 B);
Handle _bless_Get1Resource( FourCharCode A, short B) ;
void _bless_DetachResource(Handle A);
void _bless_DisposeHandle(Handle A);
void _bless_CloseResFile(short A);

