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
 *  Copyright (c) 1998-2002 Apple Computer, Inc.
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
    struct {			// nArgs=1 + args, nReturns=1 + rets
      const char *forth;
      CICell     cells[6 + 1 + 6];
    } interpret;
    
    struct {			// nArgs=2 + args, nReturns=1 + rets
      const char *method;
      CICell     iHandle;
      CICell     cells[6 + 1 + 6];
    } callMethod;
    
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
long CallMethod(long args, long rets, CICell iHandle, const char *method, ...);

// Memory
CICell Claim(CICell virt, CICell size, CICell align);
void   Release(CICell virt, CICell size);

// Control Transfer
void Boot(char *bootspec);
void Enter(void);
void Exit(void);
void Quiesce(void);

// Interpret
long Interpret(long args, long rets, const char *forthString, ...);

#endif /* ! _BOOTX_CI_H_ */
