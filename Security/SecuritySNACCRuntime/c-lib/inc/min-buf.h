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
 * .../c-lib/inc/min-buf.h - trivial buffer routines.
 *             only use these for
 *                encoding - if you know you 'buffer' is big enough
 *                            to hold the encoded value
 *
 *                decoding - if you know that the encoding is error
 *                           free.
 *
 *
 * The minimal buffer is simply a block of mem referenced
 * by a char **(ie BUF_TYPE char**).  These are very efficient
 * but should only be used when it is safe (see above) or you're
 * willing to risk reading past the end of the buffer or writing
 * 'past' the beginning (segmentation faults/bus errors etc).
 *
 * No checks for reading or writing past the buffer are done.
 * every operation is assumed to succeed.
 * MS 92
 *
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
 * $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c-lib/inc/min-buf.h,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
 * $Log: min-buf.h,v $
 * Revision 1.1.1.1  2001/05/18 23:14:08  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:23  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:21  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.2  1995/07/24 21:01:21  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.1  1994/08/28  09:21:42  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#ifndef _min_buf_h_
#define _min_buf_h_

#define MinBufGetByte(b)\
    (unsigned char)(*((*(b))++))

#define MinBufGetSeg( b, lenPtr)\
    *(b);\
    (*b) += *lenPtr;

#define MinBufCopy( dst, b, len)\
    memcpy ((dst), *(b), (len));\
    (*(b)) += (len);

#define MinBufSkip( b, len) ((*(b)) += len)

#define MinBufPeekByte( b)  (**(b))


#define MinBufPutByteRvs( b,  byte)\
    (*(--(*(b))) = (byte))

#define MinBufPutSegRvs( b, data, len)\
    ((*(b)) = (*(b)) - (len));\
    memcpy (*(b), (data), (len));


#define MinBufReadError( b)	0  /* always false */
#define MinBufWriteError( b)	0  /* always false */

#endif /* conditional include */
