/*
 * UNIXReadWrite.h
 *
 * $Header$
 *
 * Copyright 2004 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include <Kerberos/KerberosDebug.h>
#include "UNIXReadWrite.h"

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

static int __UNIXReadWriteError (int err, const char *function, const char *file, int line);
#define URWError_(err) __UNIXReadWriteError(err, __FUNCTION__, __FILE__, __LINE__)

// ---------------------------------------------------------------------------
// Error reporting
// ---------------------------------------------------------------------------

static int __UNIXReadWriteError (int err, const char *function, const char *file, int line)
{
    if (err && (ddebuglevel () > 0)) {
        dprintf ("%s() error %d ('%s') at %s: %d", function, err, strerror (err), file, line);
    }
    return err;
}

// ---------------------------------------------------------------------------
// Standard read loop, accounting for EINTR, EOF and incomplete reads
// ---------------------------------------------------------------------------

int ReadBuffer (int inDescriptor, size_t inBufferLength, char *ioBuffer)
{
    int err = 0;
    size_t bytesRead = 0;
    
    if (!ioBuffer) { err = EINVAL; }
    
    if (!err) {
        char *ptr = ioBuffer;
        do {
            ssize_t count = read (inDescriptor, ptr, inBufferLength - bytesRead);
            if (count < 0) {
                // Try again on EINTR
                if (errno != EINTR) { err = errno; }
            } else if (count == 0) {
                err = ECONNRESET; // EOF and we expected data
            } else {
                ptr += count;
                bytesRead += count;
            }
        } while (!err && (bytesRead < inBufferLength));
    } 
    
    return URWError_ (err);
}

// ---------------------------------------------------------------------------
// Standard write loop, accounting for EINTR and incomplete writes
// ---------------------------------------------------------------------------

int WriteBuffer (int inDescriptor, const char *inBuffer, size_t inBufferLength)
{
    int err = 0;
    size_t bytesWritten = 0;
    
    if (!inBuffer) { err = EINVAL; }
    
    if (!err) {
        const char *ptr = inBuffer;
        do {
            ssize_t count = write (inDescriptor, ptr, inBufferLength - bytesWritten);
            if (count < 0) {
                // Try again on EINTR
                if (errno != EINTR) { err = errno; }
            } else {
                ptr += count;
                bytesWritten += count;
            }
        } while (!err && (bytesWritten < inBufferLength));
    } 
    
    return URWError_ (err);
}

// ---------------------------------------------------------------------------
// Read a dynamic length buffer (length + data) off the descriptor
// ---------------------------------------------------------------------------

int ReadDynamicLengthBuffer (int inDescriptor, char **outData, size_t *outLength)
{
    int err = 0;
    char *data = NULL;
    u_int32_t length = 0;
    
    if (!outData  ) { err = EINVAL; }
    if (!outLength) { err = EINVAL; }
    
    if (!err) {
        err = ReadBuffer (inDescriptor, 4, (char *) &length);
    }

    if (!err) {
        length = ntohl (length);
    }
    
    if (!err) {
	data = malloc (length);
        if (!data) { err = ENOMEM; }
    }
    
    if (!err) {
	memset (data, 0, length); 
	err = ReadBuffer (inDescriptor, length, data);
    }
    
    if (!err) {
	*outLength = length;
        *outData = data;        
        data = NULL; // only free on error
    }
    
    if (data) { free (data); }
    
    return URWError_ (err);
}

// ---------------------------------------------------------------------------
// Write a dynamic length buffer (length + data) to the descriptor
// ---------------------------------------------------------------------------

int WriteDynamicLengthBuffer (int inDescriptor, const char *inData, size_t inLength)
{
    int err = 0;
    u_int32_t length = htonl (inLength);
    
    if (!inData) { err = EINVAL; }
    
    if (!err) {
	err = WriteBuffer (inDescriptor, (char *) &length, 4);
    }
    
    if (!err) { 
        err = WriteBuffer (inDescriptor, inData, inLength);
    }
    
    return URWError_ (err);
}

