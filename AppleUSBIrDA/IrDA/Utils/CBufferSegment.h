/*
    File:       CBufferSegment.h

    Contains:   Interface to the CBufferSegment class

*/

#ifndef __CBUFFERSEGMENT_H
#define __CBUFFERSEGMENT_H

#include "CBuffer.h"

enum {                                      // move this out of here!
    kDefaultCBufferSize = (2048 + 20)       // max lap packet and some overhead and some slop
};

//--------------------------------------------------------------------------------
//      CBufferSegment
//--------------------------------------------------------------------------------
class CBufferSegment : public CBuffer
{
    OSDeclareDefaultStructors(CBufferSegment);
    
public:

    static CBufferSegment * New(Size len = kDefaultCBufferSize);    // allocate and init a buffer of size len
    static CBufferSegment * New(UByte *buffer, Size len);           // use existing buffer, don't alloc or free it
    void free();
    void Delete();                  // old style, same as release() for now ...

    // get primitives

    virtual int     Peek(void);
    virtual int     Next(void);
    virtual uintptr_t     Skip(void);
    virtual int     Get(void);
    virtual Size    Getn(UByte* p, Size n);
    virtual int     CopyOut(UByte* p, Size& n);

    // put primitives

    virtual int     Put(int dataByte);
    virtual Size    Putn(const UByte* p, Size n);
    virtual int     CopyIn(const UByte* p, Size& n);

    // misc

    virtual void    Reset(void);

    // position and size

    virtual Long    Hide(Long count, int dir);
    virtual Size    Seek(Long off, int dir);
    virtual Size    Position(void) const;
    virtual Size    GetSize(void) const;
    virtual Boolean AtEOF(void) const;

    // direct access for ... (needed anymore?)

    UByte*      GetBufferPtr(void);
    Size        GetBufferSize(void);
    UInt8   *   GetBufferBase( void );

private:

    Boolean Init(UByte *buffer, Size len);

    Boolean         fFreeMe;        // true if we allocated the buffer

    UByte*          fBufBase;
    UByte*          fBufEnd;
    Size            fSize;

    UByte*          fBase;
    UByte*          fMark;
    UByte*          fEnd;

}; // CBufferSegment


//--------------------------------------------------------------------------------
//      CBufferSegment inlines
//--------------------------------------------------------------------------------

inline UByte* CBufferSegment::GetBufferPtr()
    { return fBase; }

inline Size CBufferSegment::GetBufferSize()
    { return (Size) (fEnd - fBase); }
    
inline UInt8 *  CBufferSegment::GetBufferBase() { return fBufBase; }
inline void     CBufferSegment::Delete(void)    { this->release(); return; }

#endif  /*  __BUFFERSEGMENT_H   */
