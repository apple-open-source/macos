/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 *  .../c-lib/src/sbuf.c
 *
 * Copyright (C) 1992 Michael Sample and the University of British Columbia
 *  MS
 * This library is free software; you can redistribute it and/or
 * modify it provided that this copyright/license information is retained
 * in original form.
 *
 * If you modify this file, you must clearly indicate your changes.
 *
 * This source code is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifdef USE_GEN_BUF

#include "asn-config.h"
#include "gen-buf.h"
#include "sbuf.h"

/*
 * casts are used to overcome void * - SBuf * conflict
 * be careful if you modify param lists etc.
 */
static struct GenBuf sBufOpsG =
{
  (BufGetByteFcn)    SBufGetByte,
  (BufGetSegFcn)     SBufGetSeg,
  (BufCopyFcn)       SBufCopy,
  (BufSkipFcn)       SBufSkip,
  (BufPeekByteFcn)   SBufPeekByte,
  (BufPeekSegFcn)    SBufPeekSeg,
  (BufPeekCopyFcn)   SBufPeekCopy,
  (BufPutByteRvsFcn) SBufPutByteRvs,
  (BufPutSegRvsFcn)  SBufPutSegRvs,
  (BufReadErrorFcn)  SBufReadError,
  (BufWriteErrorFcn) SBufWriteError,
  NULL,
  NULL
};

void
PutSBufInGenBuf PARAMS ((sb, gb),
    SBuf *sb _AND_
    GenBuf *gb)
{
    *gb = sBufOpsG; /* structure assignemnt */
    gb->bufInfo = sb;
}

/*
 * given an SBuf,b, and a block of data
 * and its length this initializes a the SBuf
 * to point to the data block.  The data
 * block is assumed to contain no valid data-
 * ie it is empty and ready for writing
 */
void
SBufInit PARAMS ((b, data, dataLen),
    SBuf *b _AND_
    char *data _AND_
    long int dataLen)
{
    b->readError = b->writeError = 1;
    b->blkStart = data;
    b->blkEnd = data + dataLen;
    b->dataStart = b->dataEnd = b->readLoc = b->blkEnd;
}  /* SBufInit */


/*
 * puts the given buffer in read mode and sets
 * the current read location to the beginning of
 * the buffer's data.
 * The read error flag is cleared.
 * The writeError flag is set so that attempted writes
 * will be fail and be detectable via a call to
 * SBufWriteError().
 */
void
SBufResetInReadMode PARAMS ((b),
    SBuf *b)
{
    b->readLoc = b->dataStart;
    b->readError = 0;
    b->writeError = 1;
} /* SBufResetInnReadMode */


/*
 * puts the given buffer in reverse writing mode and sets
 * the current write location to the end of the
 * buffer's data block.
 * The data start and end pointers are set to point to
 * the end of the block - ie no data.
 * The write error flag is cleared.
 * The readError flag is set so that attempted reads
 * will be fail and be detectable via a call to
 * SBufReadError().
 */
void
SBufResetInWriteRvsMode PARAMS ((b),
    SBuf *b)
{
    b->dataStart = b->dataEnd = b->blkEnd;
    b->writeError = 0;
    b->readError = 1;
} /* SBufResetInWriteRvsMode */

/*
 * installs given block of data into a buffer
 * and sets it up for reading
 */
void
SBufInstallData PARAMS ((b, data, dataLen),
    SBuf *b _AND_
    char *data _AND_
    long int dataLen)
{
    SBufInit (b, data, dataLen);
    b->dataStart = b->blkStart;
    SBufResetInReadMode (b);
} /* SBufInstallData */

/*
 * returns the number of bytes in the data portion
 */
long int
SBufDataLen PARAMS ((b),
    SBuf *b)
{
    return b->dataEnd - b->dataStart;
} /* SBufDataLen */

/*
 *  returns the pointer to the first data byte
 */
char*
SBufDataPtr PARAMS ((b),
    SBuf *b)
{
  b->dataStart;
} /* SBufDataPtr */

/*
 * returns the size of block, the maximum size for data
 * (does not look at how much data is present, just the
 * max size if the block were empty)
 */
long int
SBufBlkLen PARAMS ((b),
    SBuf *b)
{
    return b->blkEnd - b->blkStart;
} /* SBufBlkLen */

/*
 * returns a pointer to the first byte of the block
 */
char*
SBufBlkPtr PARAMS ((b),
    SBuf *b)
{
    return b->blkStart;
} /* SBufBlkPtr */

/*
 *  returns true if there is no more data
 *  to be read in the SBuf
 */
int
SBufEod PARAMS ((b),
    SBuf *b)
{
  return b->readLoc >= b->dataEnd;
} /* SBufEod */


/* returns true if you attempted to read past the end of data */
int
SBufReadError PARAMS ((b),
    SBuf *b)
{
    return b->readError;
} /* SBufReadError */

/*
 * returns true if you attempted to write past the end of the block
 * (remember SBufs do not expand like ExpBufs)
 */
int
SBufWriteError PARAMS ((b),
    SBuf *b)
{
    return b->writeError;
} /* SBufWriteError */

/*
 *  Skips the next skipLen bytes for reading
 */
void
SBufSkip PARAMS ((b, skipLen),
    SBuf *b _AND_
    long int skipLen)
{
    if (b->readLoc + skipLen > b->dataEnd)
    {
        b->readLoc = b->dataEnd;
        b->readError = 1;
    }
    else
        b->readLoc += skipLen;
} /* SBufSkip */


/*
 * copies copyLen bytes from buffer b into char *dst.
 * Advances the curr read loc by copyLen
 * Assumes dst is pre-allocated and is large enough.
 * Will set the read error flag is you attempt to copy
 * more than the number of unread bytes available.
 */
void
SBufCopy PARAMS ((dst, b, copyLen),
    char *dst _AND_
    SBuf *b _AND_
    long int copyLen)
{
    if (b->readLoc + copyLen > b->dataEnd)
    {
        memcpy (dst, b->readLoc, b->dataEnd - b->readLoc);
        b->readLoc = b->dataEnd;
        b->readError = 1;
    }
    else
    {
        memcpy (dst, b->readLoc, copyLen);
        b->readLoc += copyLen;
    }
} /* SBufCopy */


/*
 * returns the next byte from the buffer without advancing the
 * current read location.
 */
unsigned char
SBufPeekByte PARAMS ((b),
    SBuf *b)
{
    if (SBufEod (b))
    {
        b->readError = 1;
        return (unsigned char)0;
    }
    else
        return (unsigned char) *b->readLoc;
} /* SBufPeekByte */

#if TTBL

/*
 * returns a pointer into the buffer to the next bytes to be read.
 * If *lenPtr unread bytes are not available, *lenPtr will be set
 * to the number of bytes that are available.  The current read location
 * is *NOT* advanced at all.  The read error flag will NOT be set
 *  by this routine.
 */
char*
SBufPeekSeg PARAMS ((b, lenPtr),
    SBuf *b _AND_
    long int *lenPtr)
{
    if (b->readLoc + *lenPtr > b->dataEnd)
         *lenPtr = b->dataEnd - b->readLoc;

    return b->readLoc;
} /* SBufPeekSeg */



/*
 * copies copyLen bytes from buffer b into char *dst.
 * Does NOT advance the curr read location.
 * assumes dst is pre-allocated and is large enough.
 * Will set the read error flag is you attempt to copy
 * more than the number of unread bytes available.
 */
void
SBufPeekCopy PARAMS ((dst, b, copyLen),
    char *dst _AND_
    SBuf *b _AND_
    long int copyLen)
{
    if (b->readLoc + copyLen > b->dataEnd)
    {
        memcpy (dst, b->readLoc, b->dataEnd - b->readLoc);
        b->readError = 1;
    }
    else
        memcpy (dst, b->readLoc, copyLen);

} /* SBufCopy */

#endif /* TTBL */

/*
 * returns a pointer into the buffer to the next bytes to be read.
 * If *lenPtr unread bytes are not available, *lenPtr will be set
 * to the number of bytes that are available.  The current read location
 * is advance by the number of bytes returned in *lenPtr.  The read error
 * flag will NOT be set, ever, by this routine.
 */
char*
SBufGetSeg PARAMS ((b, lenPtr),
    SBuf *b _AND_
    long int *lenPtr)
{
    char *retVal;
    retVal = b->readLoc;

    if (b->readLoc + *lenPtr > b->dataEnd)
    {
         *lenPtr = b->dataEnd - b->readLoc;
         b->readLoc = b->dataEnd;
    }
    else
        b->readLoc += *lenPtr;

    return retVal;
} /* SBufGetSeg */

/*
 * Write in reverse the char *seg of segLen bytes to the buffer b.
 * A reverse write of segement really just prepends the given seg
 * (in original order) to the buffers existing data.
 * If the SBuf does not have enough room for the segment,
 * the writeError flag is set and *NO* copying is done at all.
 */
void
SBufPutSegRvs PARAMS ((b, seg, segLen),
    SBuf *b _AND_
    char *seg _AND_
    long int segLen)
{
    if (b->dataStart - segLen < b->blkStart)
        b->writeError = 1;
    else
    {
        b->dataStart -= segLen;
        memcpy (b->dataStart, seg, segLen);
    }
} /* SBufPutSegRvs */

/*
 * returns the next byte from buffer b's data and advances the
 * current read location by one byte.  This will set the read error
 * flag if you attempt to read past the end of the SBuf
 */
unsigned char
SBufGetByte PARAMS ((b),
    SBuf *b)
{
    if (SBufEod (b))
        b->readError = 1;
    else
        return (unsigned char)(*(b->readLoc++));
} /* SBufGetByte */


/*
 * writes (prepends) the given byte to buffer b's data
 */
void
SBufPutByteRvs PARAMS ((b, byte),
    SBuf *b _AND_
    unsigned char byte)
{
    if (b->dataStart <= b->blkStart)
        b->writeError = 1;
    else
        *--b->dataStart = byte;
} /* SBufPutByteRvs */

#endif /* USE_GEN_BUF */
