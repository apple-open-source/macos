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
    virtual uintptr_t     Skip(void) = 0;
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
