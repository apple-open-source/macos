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


// file: .../c++-lib/inc/asn-buf.h - buffer class
//
// MS 92
// Copyright (C) 1992 Michael Sample and the University of British Columbia
//
// This library is free software; you can redistribute it and/or
// modify it provided that this copyright/license information is retained
// in original form.
//
// If you modify this file, you must clearly indicate your changes.
//
// This source code is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/c++-lib/inc/asn-buf.h,v 1.1.1.1 2001/05/18 23:14:06 mb Exp $
// $Log: asn-buf.h,v $
// Revision 1.1.1.1  2001/05/18 23:14:06  mb
// Move from private repository to open source repository
//
// Revision 1.3  2001/05/05 00:59:17  rmurphy
// Adding darwin license headers
//
// Revision 1.2  2000/06/15 18:48:23  dmitch
// Snacc-generated source files, now part of CVS tree to allow for cross-platform build of snaccRuntime.
//
// Revision 1.1.1.1  2000/03/09 01:00:05  rmurphy
// Base Fortissimo Tree
//
// Revision 1.4  1999/08/06 16:13:18  mb
// Set readError when doing a GetSeg past the end of the buffer.  This fixes many potential bugs and hangs when doing  streaming decodes with embedded data.
//
// Revision 1.3  1999/07/14 23:53:55  aram
// Made const correct so things build with CW 5.0
//
// Revision 1.2  1999/03/04 00:43:20  mb
// Made buffer full check work for NULL buffer in an unsigned int context
//
// Revision 1.1  1999/02/25 05:21:41  mb
// Added snacc c++ library
//
// Revision 1.5  1997/02/16 20:25:35  rj
// check-in of a few cosmetic changes
//
// Revision 1.4  1995/07/25  20:18:58  rj
// changed `_' to `-' in file names.
//
// Revision 1.3  1994/10/08  04:15:38  rj
// fixed both Copy()'s name and implementation to CopyOut() that always returns the number of bytes copied out instead of 0 in case less than the requested amount is available.
//
// several `unsigned long int' turned into `size_t'.
//
// Revision 1.2  1994/08/28  10:00:46  rj
// comment leader fixed.
//
// Revision 1.1  1994/08/28  09:20:28  rj
// first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.

#ifndef _asn_buf_h_
#define _asn_buf_h_

class AsnBuf
{
protected:
    char *dataStart;
    char *dataEnd;
    char *blkStart;
    char *blkEnd;
    char *readLoc;
    bool writeError;
    bool readError;

public:
    // install data for reading or blank blk for writing in buffer
    // must be followed by 'mode' setting method call
    void Init (char *data, size_t dataLen)
    {
	readError = writeError = 1;
	blkStart = data;
	blkEnd = data + dataLen;
	dataStart = dataEnd = readLoc = blkEnd;
    }

    void ResetInReadMode()
    {
	readLoc = dataStart;
	readError = false;
	writeError = true;
    }

    void ResetInWriteRvsMode()
    {
	dataStart = dataEnd = blkEnd;
	writeError = false;
	readError = true;
    }

    void InstallData (const char *data, size_t dataLen)
    {
	Init (const_cast<char *>(data), dataLen);
	dataStart = blkStart;
	ResetInReadMode();
    }

    size_t			DataLen()		{ return dataEnd - dataStart; }
    char			*DataPtr()		{ return dataStart; }
    size_t			BlkLen()		{ return blkEnd - blkStart; }
    char			*BlkPtr()		{ return blkStart; }
    bool			Eod()			{ return readLoc >= dataEnd; }
    bool			ReadError()		{ return readError; }
    bool			WriteError()		{ return writeError; }

    void Skip (size_t skipLen)
    {
	if ((readLoc + skipLen) > dataEnd)
	{
	    readLoc = dataEnd;
	    readError = true;
	}
	else
	    readLoc += skipLen;
    }

    size_t CopyOut (char *dst, size_t copyLen)
    {
	if (readLoc + copyLen > dataEnd)
	{
	    copyLen = dataEnd - readLoc;
	    readError = true;
	}
	memcpy (dst, readLoc, copyLen);
	readLoc += copyLen;
	return copyLen;
    }

    unsigned char PeekByte()
    {
	if (Eod())
	{
	    readError = true;
	    return 0;
	}
	else
	    return *readLoc;
    }

    char *GetSeg (size_t *lenPtr)
    {
	char *retVal = readLoc;
	if ((readLoc + *lenPtr) > dataEnd)
	{
	    *lenPtr = dataEnd - readLoc;
	    readLoc = dataEnd;

	    /* Attempting to read more bytes than left in the buffer is a read error --Michael. */
	    readError = true;

	    return retVal;
	}
	else
	{
	    readLoc += *lenPtr;
	    return retVal;
	}
    }

    void PutSegRvs (char *seg, size_t segLen)
    {
	if (dataStart < (blkStart + segLen))
	    writeError = true;
	else
	{
	    dataStart -= segLen;
	    memcpy (dataStart, seg, segLen);
	}
    }

    unsigned char GetByte()
    {
	if (Eod())
	{
	    readError = true;
	    return 0;
	}
	else
	    return *(readLoc++);
    }

    void PutByteRvs (unsigned char byte)
    {
	if (dataStart <= blkStart)
	    writeError = true;
	else
	    *(--dataStart) = byte;
    }
};

#endif /* conditional include */
