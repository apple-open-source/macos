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
 *  ci.h - Headers for the OF Client Interface Library
 *
 *  Copyright (c) 1998-2000 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#ifndef _BOOTX_CI_H_
#define _BOOTX_CI_H_

#define kCINoError (0)
#define kCIError   (-1)
#define kCICatch   (-2)

typedef long CICell;

struct CIArgs {
  char *service;
  CICell nArgs;
  CICell nReturns;
  
  union {
    struct {			// nArgs=1, nReturns=1
      char *forth;
      CICell catchResult;
    } interpret_0_0;
	
    struct {			// nArgs=2, nReturns=1
      char *forth;
      CICell arg1;
      CICell catchResult;
    } interpret_1_0;
	
    struct {			// nArgs=2, nReturns=2
      char *forth;
      CICell arg1;
      CICell catchResult;
      CICell return1;
    } interpret_1_1;
	
    struct {			// nArgs=3, nReturns=2
      char *forth;
      CICell arg1;
      CICell arg2;
      CICell catchResult;
      CICell return1;
    } interpret_2_1;
	
    struct {			// nArgs=4, nReturns=2
      char *forth;
      CICell arg1;
      CICell arg2;
      CICell arg3;
      CICell catchResult;
      CICell return1;
    } interpret_3_1;
	
    struct {			// nArgs=4, nReturns=3
      char *forth;
      CICell arg1;
      CICell arg2;
      CICell arg3;
      CICell catchResult;
      CICell return1;
      CICell return2;
    } interpret_3_2;
	
    struct {            // nArgs=5, nReturns=1
      char *forth;
      CICell arg1;
      CICell arg2;
      CICell arg3;
      CICell arg4;
      CICell catchResult;
    } interpret_4_0;
	
    struct {			// nArgs=1, nReturns=2
      char *forth;
      CICell catchResult;
      CICell return1;
    } interpret_0_1;
	
    struct {			// nArgs=1, nReturns=3
      char *forth;
      CICell catchResult;
      CICell return1;
      CICell return2;
    } interpret_0_2;
	
    struct {			// nArgs=1, nReturns=4
      char *forth;
      CICell catchResult;
      CICell return1;
      CICell return2;
      CICell return3;
    } interpret_0_3;
	
    struct {			// nArgs=2, nReturns=4
      char *forth;
      CICell arg1;
      CICell catchResult;
      CICell return1;
      CICell return2;
      CICell return3;
    } interpret_1_3;
	
    struct {			// nArgs=3, nReturns=4
      char *forth;
      CICell arg1;
      CICell arg2;
      CICell catchResult;
      CICell return1;
      CICell return2;
      CICell return3;
    } interpret_2_3;
	
    struct {			// nArgs=3, nReturns=5
      char *forth;
      CICell arg1;
      CICell arg2;
      CICell catchResult;
      CICell return1;
      CICell return2;
      CICell return3;
      CICell return4;
    } interpret_2_4;
	
    struct {			// nArgs=3, nReturns=1
      char *forth;
      CICell arg1;
      CICell arg2;
      CICell catchResult;
    } interpret_2_0;
	
    struct {			// nArgs=3, nReturns=3
      char *forth;
      CICell arg1;
      CICell arg2;
      CICell catchResult;
      CICell return1;
      CICell return2;
    } interpret_2_2;
	
    struct {			// nArgs=4, nReturns=1
      char *forth;
      CICell arg1;
      CICell arg2;
      CICell arg3;
      CICell catchResult;
    } interpret_3_0;

    struct {			// nArgs=2, nReturns=1
      char *method;
      CICell iHandle;
      CICell catchResult;
    } callMethod_0_0;

    struct {			// nArgs=2, nReturns=2
      char *method;
      CICell iHandle;
      CICell catchResult;
      CICell return1;
    } callMethod_0_1;

    struct {			// nArgs=3, nReturns=1
      char *method;
      CICell iHandle;
      CICell arg1;
      CICell catchResult;
    } callMethod_1_0;

    struct {			// nArgs=3, nReturns=2
      char *method;
      CICell iHandle;
      CICell arg1;
      CICell catchResult;
      CICell return1;
    } callMethod_1_1;

    struct {			// nArgs=4, nReturns=1
      char *method;
      CICell iHandle;
      CICell arg2;
      CICell arg1;
      CICell catchResult;
    } callMethod_2_0;

    struct {			// nArgs=5, nReturns=1
      char *method;
      CICell iHandle;
      CICell arg3;
      CICell arg2;
      CICell arg1;
      CICell catchResult;
    } callMethod_3_0;

    struct {			// nArgs=5, nReturns=2
      char *method;
      CICell iHandle;
      CICell arg3;
      CICell arg2;
      CICell arg1;
      CICell catchResult;
      CICell return1;
    } callMethod_3_1;

    struct {			// nArgs=6, nReturns=1
      char *method;
      CICell iHandle;
      CICell arg4;
      CICell arg3;
      CICell arg2;
      CICell arg1;
      CICell catchResult;
    } callMethod_4_0;

    struct {			// nArgs=7, nReturns=1
      char *method;
      CICell iHandle;
      CICell arg5;
      CICell arg4;
      CICell arg3;
      CICell arg2;
      CICell arg1;
      CICell catchResult;
    } callMethod_5_0;

    struct {			// nArgs=8, nReturns=1
      char *method;
      CICell iHandle;
      CICell arg6;
      CICell arg5;
      CICell arg4;
      CICell arg3;
      CICell arg2;
      CICell arg1;
      CICell catchResult;
    } callMethod_6_0;

    struct {			// nArgs=1, nReturns=1	( device-specifier -- ihandle )
      char *devSpec;	        // IN parameter
      CICell ihandle;		// RETURN value
    } open;
	
    struct {			// nArgs=1, nReturns=0	( ihandle -- )
      CICell ihandle;		// IN parameter
    } close;

    struct {                    // nArgs=3, nReturns=1 ( ihandle addr length -- actual )
      CICell ihandle;
      CICell addr;
      CICell length;
      CICell actual;
    } read;
    
    struct {                    // nArgs=3, nReturns=1 ( ihandle addr length -- actual )
      CICell ihandle;
      CICell addr;
      CICell length;
      CICell actual;
    } write;
    
    struct {                    // nArgs=3, nReturns=1 ( ihandle pos.high pos.low -- result )
      CICell ihandle;
      CICell pos_high;
      CICell pos_low;
      CICell result;
    } seek;
    
    struct {			// nArgs=3, nReturns=1
      CICell virt;
      CICell size;
      CICell align;
      CICell baseaddr;
    } claim;
    
    struct {			// nArgs=2, nReturns=0
      CICell virt;
      CICell size;
    } release;
    
    struct {			// nArgs=1, nReturns=1	( phandle -- peer-phandle )
      CICell phandle;	        // IN parameter
      CICell peerPhandle;	// RETURN value
    } peer;
    
    struct {			// nArgs=1, nReturns=1	( phandle -- child-phandle )
      CICell phandle;		// IN parameter
      CICell childPhandle;	// RETURN value
    } child;
    
    struct {			// nArgs=1, nReturns=1	( phandle -- parent-phandle )
      CICell childPhandle;	// IN parameter
      CICell parentPhandle;	// RETURN value
    } parent;
    
    struct {			// nArgs=1, nReturns=1	( devSpec -- phandle )
      char *devSpec;	        // IN parameter
      CICell phandle;	        // RETURN value
    } finddevice;
    
    struct {                    // nArgs=3, nReturns=1 ( ihandle buf buflen -- length )
      CICell ihandle;           // IN ihandle
      char   *buf;              // IN buf
      CICell buflen;            // IN buflen
      CICell length;            // RETURN length
    } instanceToPath;
    
    struct {                    // nArgs=1, nReturns=1 ( ihandle -- phandle )
      CICell ihandle;           // IN ihandle
      CICell phandle;           // RETURN phandle
    } instanceToPackage;
    
    struct {                    // nArgs=3, nReturns=1 ( phandle buf buflen -- length )
      CICell phandle;           // IN phandle
      char   *buf;              // IN buf
      CICell buflen;            // IN buflen
      CICell length;            // RETURN length
    } packageToPath;
    
    struct {			// nArgs=2, nReturns=1	( phandle name -- size )
      CICell phandle;		// IN parameter
      char   *name;		// IN parameter
      CICell size;		// RETURN value
    } getproplen;
    
    struct {			// nArgs=4, nReturns=1	( phandle name buf buflen -- size )
      CICell phandle;		// IN parameter
      char   *name;		// IN parameter
      char   *buf;		// IN parameter
      CICell buflen;		// IN parameter
      CICell size;		// RETURN value
    } getprop;
    
    struct {			// nArgs=3, nReturns=1	( phandle previous buf -- flag )
      CICell phandle;		// IN parameter
      char *previous;		// IN parameter
      char *buf;		// IN parameter
      CICell flag;		// RETURN value
    } nextprop;
    
    struct {			// nArgs=4, nReturns=1	( phandle name buf buflen -- size )
      CICell phandle;		// IN parameter
      char *name;		// IN parameter
      char *buf;		// IN parameter
      CICell buflen;		// IN parameter
      CICell size;		// RETURN value
    } setprop;
    
    struct {			// nArgs=1, nReturns=0
      char *bootspec;
    } boot;
  } args;
};
typedef struct CIArgs CIArgs;

typedef long (*ClientInterfacePtr)(CIArgs *args);

// ci.c
long InitCI(ClientInterfacePtr ciPtr);

long CallCI(CIArgs *ciArgsPtr);

// Device Tree
CICell Peer(CICell phandle);
CICell Child(CICell phandle);
CICell Parent(CICell phandle);
CICell FindDevice(char *devSpec);
CICell InstanceToPath(CICell ihandle, char *buf, long buflen);
CICell InstanceToPackage(CICell ihandle);
CICell PackageToPath(CICell phandle, char *buf, long buflen);
CICell GetPropLen(CICell phandle, char *name);
CICell GetProp(CICell phandle, char *name, char *buf, long buflen);
CICell NextProp(CICell phandle, char *previous, char *buf);
CICell SetProp(CICell phandle, char *name, char *buf, long buflen);

// Device I/O
CICell Open(char *devSpec);
void   Close(CICell ihandle);
CICell Read(CICell ihandle, long addr, long length);
CICell Write(CICell ihandle, long addr, long length);
CICell Seek(CICell ihandle, long long position);

// Call Method
long CallMethod_0_0(CICell ihandle, char *method);
long CallMethod_0_1(CICell ihandle, char *method, CICell *ret1);
long CallMethod_1_0(CICell ihandle, char *method, CICell arg1);
long CallMethod_1_1(CICell iHandle, char *method, CICell arg1, CICell *ret1);
long CallMethod_2_0(CICell ihandle, char *method,
		    CICell arg1, CICell arg2);
long CallMethod_3_0(CICell ihandle, char *method,
		    CICell arg1, CICell arg2, CICell arg3);
long CallMethod_3_1(CICell ihandle, char *method,
		    CICell arg1, CICell arg2, CICell arg3, CICell *ret1);
long CallMethod_4_0(CICell ihandle, char *method,
		    CICell arg1, CICell arg2, CICell arg3, CICell arg4);
long CallMethod_5_0(CICell ihandle, char *method, CICell arg1, CICell arg2,
		    CICell arg3, CICell arg4, CICell arg5);
long CallMethod_6_0(CICell ihandle, char *method, CICell arg1, CICell arg2,
		    CICell arg3, CICell arg4, CICell arg5, CICell arg6);

// Memory
CICell Claim(CICell virt, CICell size, CICell align);
void   Release(CICell virt, CICell size);

// Control Transfer
void Boot(char *bootspec);
void Enter(void);
void Exit(void);
void Quiesce(void);

// Interpret_n_m
long Interpret_0_0(char *forthString);
long Interpret_1_0(char *forthString, CICell arg1);
long Interpret_1_1(char *forthString, CICell arg1, CICell *ret1);
long Interpret_2_1(char *forthString, CICell arg1, CICell arg2, CICell *ret1);
long Interpret_3_1(char *forthString, CICell arg1, CICell arg2, CICell arg3,
		  CICell *ret1);
long Interpret_3_2(char *forthString, CICell arg1, CICell arg2, CICell arg3,
		  CICell *ret1, CICell *ret2);
long Interpret_4_0(char *forthString, CICell arg1, CICell arg2, CICell arg3,
		  CICell arg4);
long Interpret_0_1(char *forthString, CICell *ret1);
long Interpret_0_2(char *forthString, CICell *ret1, CICell *ret2);
long Interpret_0_3(char *forthString, CICell *ret1,
		   CICell *ret2, CICell *ret3);
long Interpret_1_3(char *forthString, CICell arg1, CICell *ret1, CICell *ret2,
		  CICell *ret3);
long Interpret_2_3(char *forthString, CICell arg1, CICell arg2, CICell *ret1,
		  CICell *ret2, CICell *ret3);
long Interpret_2_4(char *forthString, CICell arg1, CICell arg2, CICell *ret1,
		  CICell *ret2, CICell *ret3, CICell *ret4);
long Interpret_2_0(char *forthString, CICell arg1, CICell arg2);
long Interpret_2_2(char *forthString, CICell arg1, CICell arg2, CICell *ret1,
		  CICell *ret2);
long Interpret_3_0(char *forthString, CICell arg1, CICell arg2, CICell arg3);

#endif /* ! _BOOTX_CI_H_ */
