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
 * sbuf.h  - a buffer consisting of one contiguous block
 *               that checks for read and write range errors.
 * MS 92
 * Copyright (C) 1992 Michael Sample and the University of British Columbia
 *
 * This library is free software; you can redistribute it and/or
 * modify it provided that this copyright/license information is retained
 * in original form.
 *
 * If you modify this file, you must clearly indicate your changes.
 *
 * This source code is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/c-lib/inc/sbuf.h,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
 * $Log: sbuf.h,v $
 * Revision 1.1.1.1  2001/05/18 23:14:08  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:23  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:21  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.2  1995/07/27 08:54:46  rj
 * functions used by gen-bufs or type tables merged.
 *
 * changed `_' to `-' in file names.
 *
 * Revision 1.1  1994/08/28  09:45:39  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#ifndef _asn_buf_h_
#define _asn_buf_h_

typedef struct SBuf
{
    char *dataStart;  /* byte last written (or end) */
    char *dataEnd;    /* ptr to first byte after last valid data byte */
    char *blkStart;   /* ptr to first byte of the buffer */
    char *blkEnd;     /* ptr to first byte past end of the buffer */
    char *readLoc;    /* next byte to read (or end) */
    int writeError;   /* whether write error occurred */
    int readError;    /* whether read error occurred */
} SBuf;

#ifdef USE_GEN_BUF

/* use functions (-> src/sbuf.c) instead of cpp macros */

void		PutSBufInGenBuf PROTO ((SBuf *sb, GenBuf *gb));
void		SBufInit PROTO ((SBuf *b, char *data, long int dataLen));
void		SBufResetInReadMode PROTO ((SBuf *b));
void		SBufResetInWriteRvsMode PROTO ((SBuf *b));
void		SBufInstallData PROTO ((SBuf *b, char *data, long int dataLen));
long int	SBufDataLen PROTO ((SBuf *b));
char		*SBufDataPtr PROTO ((SBuf *b));
long int	SBufBlkLen PROTO ((SBuf *b));
char		*SBufBlkPtr PROTO ((SBuf *b));
int		SBufEod PROTO ((SBuf *b));
int		SBufReadError PROTO ((SBuf *b));
int		SBufWriteError PROTO ((SBuf *b));
void		SBufSkip PROTO ((SBuf *b, long int skipLen));
void		SBufCopy PROTO ((char *dst, SBuf *b, long int copyLen));
unsigned char	SBufPeekByte PROTO ((SBuf *b));
#if TTBL
char		*SBufPeekSeg PROTO ((SBuf *b, long int *lenPtr));
void		SBufPeekCopy PROTO ((char *dst, SBuf *b, long int copyLen));
#endif
char		*SBufGetSeg PROTO ((SBuf *b,long int *lenPtr));
void		SBufPutSegRvs PROTO ((SBuf *b, char *seg, long int segLen));
unsigned char	SBufGetByte PROTO ((SBuf *b));
void		SBufPutByteRvs PROTO ((SBuf *b, unsigned char byte));

#else

/* initializes a buffer into an 'empty' state */
#define SBufInit(b, data, dataLen)\
{ (b)->readError = (b)->writeError = 1;\
  (b)->blkStart = data;\
  (b)->blkEnd = data + dataLen;\
  (b)->dataStart = (b)->dataEnd = (b)->readLoc = (b)->blkEnd;\
}

#define SBufResetInReadMode(b)\
{ (b)->readLoc = (b)->dataStart;\
  (b)->readError = 0;\
  (b)->writeError = 1;\
}

#define SBufResetInWriteRvsMode(b)\
{ (b)->dataStart = (b)->dataEnd = (b)->blkEnd;\
  (b)->writeError = 0;\
  (b)->readError = 1;\
}

/* installs given block of data into a buffer and sets it up for reading */
#define SBufInstallData(b, data, dataLen)\
   SBufInit (b, data, dataLen);\
   (b)->dataStart = (b)->blkStart;\
   SBufResetInReadMode (b);

/* returns the number of bytes in the data portion */
#define SBufDataLen(b)\
  ((b)->dataEnd - (b)->dataStart)

/* returns the pointer to the first data byte */
#define SBufDataPtr(b)\
  ((b)->dataStart)

/* returns the size of block, the maximum size for data */
#define SBufBlkLen(b)\
  ((b)->blkEnd - (b)->blkStart)

/* returns a pointer to the first byte of the block */
#define SBufBlkPtr(b)\
  ((b)->blkStart)

/* returns true if there is no more data to be read in the SBuf */
#define SBufEod(b)\
  ((b)->readLoc >= (b)->dataEnd)

/* returns true if you attempted to read past the end of data */
#define SBufReadError(b)\
  ((b)->readError)

/*
 * returns true if you attempted to write past the end of the block
 * (remember SBufs do not expand like ExpBufs)
 */
#define SBufWriteError(b)\
  ((b)->writeError)

/* Skips the next skipLen bytes for reading */
#define SBufSkip(b, skipLen)\
{ if ( ((b)->readLoc + skipLen) > (b)->dataEnd)\
  {\
      (b)->readLoc = (b)->dataEnd;\
      (b)->readError = 1;\
  }\
  else\
      (b)->readLoc += skipLen;\
}


/*
 * copies copyLen bytes from buffer b into char *dst.
 * assumes dst is pre-allocated and is large enough.
 * Will set the read error flag is you attempt to copy
 * more than the number of unread bytes available.
 */
#define SBufCopy(dst, b, copyLen)\
{ if (((b)->readLoc + copyLen) > (b)->dataEnd)\
  {\
      memcpy (dst, (b)->readLoc, (b)->dataEnd - (b)->readLoc);\
      (b)->readLoc = (b)->dataEnd;\
      (b)->readError = 1;\
  }\
  else\
  {\
      memcpy (dst, (b)->readLoc, copyLen);\
      (b)->readLoc += copyLen;\
  }\
}

/*
 * returns the next byte from the buffer without advancing the
 * current read location.
 */
#define SBufPeekByte(b)\
    ((SBufEod (b))? ((b)->readError = 1):(unsigned char) *((b)->readLoc))

/*
 * WARNING: this is a fragile macro. be careful where you use it.
 * return a pointer into the buffer for the next bytes to be read
 * if *lenPtr uread bytes are not available, *lenPtr will be set
 * to the number of byte that are available.  The current read location
 * is advance by the number of bytes returned in *lenPtr.  The read error
 * flag will NOT set, ever, by this routine.
 */
#define SBufGetSeg( b, lenPtr)\
    ((b)->readLoc);\
    if (((b)->readLoc + *lenPtr) > (b)->dataEnd)\
    {\
         *lenPtr = (b)->dataEnd - (b)->readLoc;\
         (b)->readLoc = (b)->dataEnd;\
    }\
    else\
        (b)->readLoc += *lenPtr;

/*
 * Write in reverse the char *seg of segLen bytes to the buffer b.
 * A reverse write of segement really just prepends the given seg
 * (in original order) to the buffers existing data
 */
#define SBufPutSegRvs(b, seg, segLen)\
{ if (((b)->dataStart - segLen) < (b)->blkStart)\
      (b)->writeError = 1;\
  else\
  {\
     (b)->dataStart -= segLen;\
     memcpy ((b)->dataStart, seg, segLen);\
  }\
}

/*
 * returns the next byte from buffer b's data and advances the
 * current read location by one byte.  This will set the read error
 * flag if you attempt to read past the end of the SBuf
 */
#define SBufGetByte(b)\
   (unsigned char)((SBufEod (b))? ((b)->readError = 1):*((b)->readLoc++))

/*
 * writes (prepends) the given byte to buffer b's data
 */
#define SBufPutByteRvs(b, byte)\
{ if ((b)->dataStart <= (b)->blkStart)\
      (b)->writeError = 1;\
  else\
      *(--(b)->dataStart) = byte;\
}

#endif /* USE_GEN_BUF */

#endif /* conditional include */
