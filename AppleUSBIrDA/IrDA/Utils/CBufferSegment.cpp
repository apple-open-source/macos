/*
    File:       CBufferSegment.c

    Contains:   Implementation of the CBufferSegment class


*/


#include "CBufferSegment.h"
#include "IrDALog.h"

#if (hasTracing > 0 && hasCBufferSegTracing > 0)

enum
{
    kLogNew = 1,
    kLogNewWith,
    kLogDelete,
    kLogFree1,
    kLogFree2,
    kLogCBInit1,
    kLogCBInit2,
    
    kLogIOAllocBuf,
    kLogIOAllocSize,
    kLogIOFreeBuf,
    kLogIOFreeSize
};

static
EventTraceCauseDesc TraceEvents[] = {
    {kLogNew,           "CBufferSegment:  New, obj="},
    {kLogNewWith,       "CBufferSegment:  New hdr, obj=, buffer="},
    {kLogDelete,        "CBufferSegment:  Delete"},
    {kLogFree1,         "CBufferSegment:  Free, obj="},
    {kLogFree2,         "CBufferSegment:  Free, buffer="},
    {kLogCBInit1,       "CBufferSegment:  Init, length="},
    {kLogCBInit2,       "CBufferSegment:  Init, buffer="},
    
    {kLogIOAllocBuf,    "CBufferSegment: ioalloc, buffer="},
    {kLogIOAllocSize,   "CBufferSegment: ioalloc, size="},
    {kLogIOFreeBuf,     "CBufferSegment: iofree, buffer="},
    {kLogIOFreeSize,    "CBufferSegment: iofree, size="}
};
#define XTRACE(x, y, z) IrDALogAdd( x, y, (uintptr_t)z & 0xffff, TraceEvents, true )
#else
#define XTRACE(x, y, z) ((void) 0)
#endif

#define super CBuffer
    OSDefineMetaClassAndStructors(CBufferSegment, CBuffer);


//--------------------------------------------------------------------------------
//      CBufferSegment::New
//          This is the preferred method (and easiest) of getting a statically
//          allocated CbufferSegment and its associated buffer.  Initializes
//          the object also.
//
//--------------------------------------------------------------------------------
CBufferSegment * CBufferSegment::New(Size len)
{
#pragma unused(len)
    //int Fix_Unused_Length;

    CBufferSegment *obj = new CBufferSegment;
    
    XTRACE(kLogNew, 0, obj);
    
    if (obj && !obj->Init(nil, kDefaultCBufferSize)) {  // alloc a big buffer
	obj->release();
	obj = nil;
    }

    return obj;
}

CBufferSegment * CBufferSegment::New(UByte *buffer, Size len)
{
    CBufferSegment *obj = new CBufferSegment;
    
    XTRACE(kLogNewWith, 0, obj);
    XTRACE(kLogNewWith, 0, buffer);
    
    if (obj && !obj->Init(buffer, len)) {
	obj->release();
	obj = nil;
    }

    return obj;
}

//--------------------------------------------------------------------------------
//      CBufferSegment::Delete
//          Old naming style, just calls release
//--------------------------------------------------------------------------------
/*void
CBufferSegment::Delete()
{
    XTRACE( kLogDelete, (int)this >> 16, (int)this);
    
    this->release();
}*/

//--------------------------------------------------------------------------------
//      CBufferSegment::free
//--------------------------------------------------------------------------------
void
CBufferSegment::free(void)
{
    XTRACE(kLogFree1, 0, this);
    XTRACE(kLogFree2, (uintptr_t)fBufBase>>16, fBufBase);
    
    if (fFreeMe && fBufBase) {
	check(fSize);
	XTRACE(kLogIOFreeBuf, 0, fBufBase);
	XTRACE(kLogIOFreeSize, fSize >> 16, fSize);
	IOFree(fBufBase, fSize);
	fBufBase = nil;
	fSize = 0;
    }
    
    super::free();
}


//--------------------------------------------------------------------------------
//      CBufferSegment::init
//--------------------------------------------------------------------------------
Boolean CBufferSegment::Init(UByte *buffer, Size len)
{
    XTRACE(kLogCBInit1, 0, len);
    
    fBufBase = nil;
    
    if (!super::init()) return false;
    
    if (buffer == nil) {            // callers wants us to alloc and free
	fBufBase = (UByte*)IOMalloc(len);
	XTRACE(kLogIOAllocBuf, 0, fBufBase);
	XTRACE(kLogIOAllocSize, len >> 16, len);
	require(fBufBase, Fail);
	fFreeMe = true;
    } else {
	fBufBase = buffer;
	fFreeMe = false;
    }
    
    XTRACE(kLogCBInit2, (uintptr_t)fBufBase>>16, fBufBase);

    fSize = len;
    fBufEnd = fBufBase + fSize;

    // the whole buffer is available

    fBase = fMark = fBufBase;
    fEnd = fBufEnd;

    return true;
    
Fail:
    return false;

} // CBufferSegment::init


//--------------------------------------------------------------------------------
//      CBufferSegment::Peek
//--------------------------------------------------------------------------------
int CBufferSegment::Peek()
{
    return (fMark >= fEnd) ? EOF : *fMark;

} // CBufferSegment::Peek


//--------------------------------------------------------------------------------
//      CBufferSegment::Next
//--------------------------------------------------------------------------------
int CBufferSegment::Next()
{
    return (fMark >= fEnd) ? EOF : *(++fMark);

} // CBufferSegment::Next


//--------------------------------------------------------------------------------
//      CBufferSegment::Skip
//--------------------------------------------------------------------------------
uintptr_t CBufferSegment::Skip()
{
    return (fMark >= fEnd) ? EOF : (uintptr_t)fMark++;

} // CBufferSegment::Skip


//--------------------------------------------------------------------------------
//      CBufferSegment::Get
//--------------------------------------------------------------------------------
int CBufferSegment::Get()
{
    return (fMark >= fEnd) ? EOF : *fMark++;

} // CBufferSegment::Get


//--------------------------------------------------------------------------------
//      CBufferSegment::Getn
//--------------------------------------------------------------------------------
Size CBufferSegment::Getn(UByte* p, Size n)
{
    if (n > 0)
	{
	n = Min(n, fEnd - fMark);
	if (n > 0)
	    {
	    BlockMove(fMark, p, n);
	    fMark += n;
	    }
	}

    return n;

} // CBufferSegment::Getn


//--------------------------------------------------------------------------------
//      CBufferSegment::CopyOut
//--------------------------------------------------------------------------------
int CBufferSegment::CopyOut(UByte* p, Size& n)
{
    int result = 0;

    if (n > 0)
	{
	n -= Getn(p, n);
	if (fMark == fEnd)
	    result = EOF;
	}

    return result;

} // CBufferSegment::CopyOut


//--------------------------------------------------------------------------------
//      CBufferSegment::Put
//--------------------------------------------------------------------------------
int CBufferSegment::Put(int b)
{
    return (fMark >= fEnd) ? EOF : (*fMark++ = b);

} // CBufferSegment::Put


//--------------------------------------------------------------------------------
//      CBufferSegment::Putn
//--------------------------------------------------------------------------------
Size CBufferSegment::Putn(const UByte* p, Size n)
{
    if (n > 0)
	{
	n = Min(n, fEnd - fMark);
	if (n > 0)
	    {
	    BlockMove(p, fMark, n);
	    fMark += n;
	    }
	}

    return n;

} // CBufferSegment::Putn


//--------------------------------------------------------------------------------
//      CBufferSegment::CopyIn
//--------------------------------------------------------------------------------
int CBufferSegment::CopyIn(const UByte* p, Size& n)
{
    int result = 0;

    if (n > 0)
	{
	n -= Putn(p, n);
	if (fMark == fEnd)
	    return EOF;
	}

    return result;

} // CBufferSegment::CopyIn


//--------------------------------------------------------------------------------
//      CBufferSegment::Reset
//--------------------------------------------------------------------------------
void CBufferSegment::Reset()
{
    fBase = fMark = fBufBase;
    fEnd = fBufEnd;

} // CBufferSegment::Reset


//--------------------------------------------------------------------------------
//      CBufferSegment::Hide
//--------------------------------------------------------------------------------
Size CBufferSegment::Hide(Long count, int dir)
{
    if (count == 0)
	return 0;

    switch (dir)
	{
	case kPosBeg:
	    fBase += count;
	    if (fBase < fBufBase)
		{
		count += (Long) (fBufBase - fBase);
		fBase = fBufBase;
		}
	    else if (fBase > fBufEnd)
		{
		count -= (Long) (fBase - fBufEnd);
		fBase = fBufEnd;
		}
	    fMark = fBase;
	    break;

	case kPosEnd:
	    fEnd -= count;
	    if (fEnd < fBufBase)
		{
		count -= (Long) (fBufBase - fEnd);
		fEnd = fBufBase;
		}
	    else if (fEnd > fBufEnd)
		{
		count += (Long) (fEnd - fBufEnd);
		fEnd = fBufEnd;
		}
	    fMark = fEnd;
	    break;

	default:
	    count = 0;
	    break;
	}

    return count;

} // CBufferSegment::Hide


//--------------------------------------------------------------------------------
//      CBufferSegment::Seek
//--------------------------------------------------------------------------------
Size CBufferSegment::Seek(Long off, int dir)
{
    UByte* newPos;

    // quick check for simple cases

    if (off == 0)
	{
	if (dir == kPosBeg)
	    fMark = fBase;

	else if (dir == kPosEnd)
	    fMark = fEnd;

	else if (dir == kPosCur)
	    /* no change */;
	}

    else
	{
	switch (dir)
	    {
	    case kPosBeg:
		newPos = fBase + off;
		break;

	    case kPosCur:
		newPos = fMark + off;
		break;

	    case kPosEnd:
		newPos = fEnd - off;
		break;

	    default:
		newPos = fMark;
		break;
	    }

	fMark = (UByte*) UMinMax((uintptr_t) fBase, (uintptr_t) newPos, (uintptr_t) fEnd);
	}

    return (Size) (fMark - fBase);

} // CBufferSegment::Seek


//--------------------------------------------------------------------------------
//      CBufferSegment::Position
//--------------------------------------------------------------------------------
Size CBufferSegment::Position() const
{
    return (Size) (fMark ? (fMark - fBase) : 0);

} // CBufferSegment::Position


//--------------------------------------------------------------------------------
//      CBufferSegment::GetSize
//--------------------------------------------------------------------------------
Size CBufferSegment::GetSize() const
{
    return (Size) (fEnd - fBase);

} // CBufferSegment::GetSize


//--------------------------------------------------------------------------------
//      CBufferSegment::AtEOF
//--------------------------------------------------------------------------------
Boolean CBufferSegment::AtEOF() const
{
    return (fMark == fEnd);

} // CBufferSegment::AtEOF

