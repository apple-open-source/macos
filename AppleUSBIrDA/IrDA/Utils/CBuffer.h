/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
    File:       CBuffer.h

    Contains:   Interface to the CBuffer abstract class

*/

#ifndef __CBUFFER_H
#define __CBUFFER_H

#include "IrDATypes.h"

//--------------------------------------------------------------------------------
//      CBuffer
//--------------------------------------------------------------------------------
class CBuffer : public OSObject
{
    OSDeclareAbstractStructors(CBuffer);
    
public:

    // position and size

    virtual Long    Hide(Long count, int dir) = 0;
    virtual Size    Seek(Long off, int dir) = 0;
    virtual Size    Position(void) const = 0;
    virtual Size    GetSize(void) const = 0;
    virtual Boolean AtEOF(void) const = 0;
    
    
    // get primitives

    virtual int     Peek(void) = 0;
    virtual int     Next(void) = 0;
    virtual int     Skip(void) = 0;
    virtual int     Get(void) = 0;
    virtual Size    Getn(UByte* p, Size n) = 0;
    virtual int     CopyOut(UByte* p, Size& n) = 0;

    // put primitives

    virtual int     Put(int dataByte) = 0;
    virtual Size    Putn(const UByte* p, Size n) = 0;
    virtual int     CopyIn(const UByte* p, Size& n) = 0;

    // misc

    virtual void    Reset(void) = 0;


}; // CBuffer

#endif  /*  __CBUFFER_H */
