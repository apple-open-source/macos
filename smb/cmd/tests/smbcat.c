/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

/* smbcat: Trivial cat(1) implementation for SMB.
 *
 * This is a very simple, trivial test client for the SMBClient framework.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <strings.h>
#include <sysexits.h>
#include <sys/cdefs.h>
#include <unistd.h>

#include <SMBClient/smbclient.h>
#include <libkern/OSByteOrder.h>
#include <netsmb/smb.h>

void nt_error(NTSTATUS status, const char * fmt, ...) __printflike(2, 3);

void nt_error(NTSTATUS status, const char * fmt, ...)
{
    va_list args;

    fprintf(stderr, "%s: NTSTATUS %#08X: ", getprogname(), status);

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}

static void protocol_error(const char * fmt, ...)
{
    va_list args;

    fprintf(stderr, "%s: ", getprogname());

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
    exit(EX_PROTOCOL);
}

static void usage(void)
{
    const char * usage_message =
    "smbcat smb://[domain;][user[:password]@]server/share FILE [FILE ...]\n";

    printf("%s", usage_message);

    exit(EX_USAGE);
}

#define ECHO_PAYLOAD "SMBClient echo test"

static void smbecho(SMBHANDLE handle)
{
    uint8_t request[128];
    uint8_t response[128];

    NTSTATUS status;
    off_t nbytes;
    uint8_t * ptr;

    memset(request, 0, sizeof(request));
    memset(response, 0, sizeof(response));

    ptr = request;

    OSWriteLittleInt32(ptr, 0, *((uint32_t *)SMB_SIGNATURE));
    ptr += sizeof(uint32_t); /* SMB magic */

    *ptr++ = SMB_COM_ECHO;
    ptr += sizeof(uint32_t); /* status */
    ptr += sizeof(uint8_t);  /* flags */
    ptr += sizeof(uint16_t); /* flags2 */
    ptr += sizeof(uint16_t); /* pid_high */
    ptr += 8;                /* signature */
    ptr += sizeof(uint16_t); /* reserved */
    ptr += sizeof(uint16_t); /* tid */
    ptr += sizeof(uint16_t); /* pid */
    ptr += sizeof(uint16_t); /* uid */

    OSWriteLittleInt16(ptr, 0, 0xff); /* mid marker */
    ptr += sizeof(uint16_t); /* mid */

    *ptr++ = 1; /* word count */
    OSWriteLittleInt16(ptr, 0, 1); /* echo count */
    ptr += sizeof(uint16_t);

    OSWriteLittleInt16(ptr, 0, sizeof(ECHO_PAYLOAD)); /* byte count */
    ptr += sizeof(uint16_t);

    memcpy(ptr, ECHO_PAYLOAD, sizeof(ECHO_PAYLOAD));
    ptr += sizeof(ECHO_PAYLOAD);

    status = SMBRawTransaction(handle,
                request, (ptrdiff_t)ptr - (ptrdiff_t)request,
                response, sizeof(response), &nbytes);

    if (!NT_SUCCESS(status)) {
        nt_error(status, "SMBRawTransaction()");
    }

    if (nbytes != (SMB_HDRLEN + 5 + sizeof(ECHO_PAYLOAD))) {
        protocol_error("short SMBEcho response, received %u bytes, expected %u",
                (unsigned)nbytes, SMB_HDRLEN + 5 + sizeof(ECHO_PAYLOAD));
    }

    ptr = response;
    ptr += sizeof(uint32_t); /* SMB magic */

    if (*ptr != SMB_COM_ECHO) {
        protocol_error("invalid SMBEcho command, received %u, expected %u",
                *ptr, SMB_COM_ECHO);
    }
    ptr += sizeof(uint8_t); /* SMB command */

    status = OSReadLittleInt32(ptr, 0);
    if (status != NT_STATUS_SUCCESS) {
        nt_error(status, "SMBEcho()");
        protocol_error("SMBEcho failed");
    }

    ptr += sizeof(uint32_t); /* status */
    ptr += sizeof(uint8_t);  /* flags */
    ptr += sizeof(uint16_t); /* flags2 */
    ptr += sizeof(uint16_t); /* pid_high */
    ptr += 8;                /* signature */
    ptr += sizeof(uint16_t); /* reserved */
    ptr += sizeof(uint16_t); /* tid */
    ptr += sizeof(uint16_t); /* pid */
    ptr += sizeof(uint16_t); /* uid */
    ptr += sizeof(uint16_t); /* mid */

    if (*ptr++ != 1) {
        protocol_error("invalid SMBEcho word count");
    }

    if (OSReadLittleInt16(ptr, 0) != 1) {
        protocol_error("invalid SMBEcho sequence number");
    }
    ptr += sizeof(uint16_t);

    if (OSReadLittleInt16(ptr, 0) != sizeof(ECHO_PAYLOAD)) {
        protocol_error("invalid SMBEcho byte count");
    }
    ptr += sizeof(uint16_t);

    if (memcmp(ptr, ECHO_PAYLOAD, sizeof(ECHO_PAYLOAD)) != 0) {
        protocol_error("SMBEcho payload mismatch");
    }
}

static void cat_file(SMBHANDLE handle, const char * path)
{
    SMBFID hFile;
    NTSTATUS status;

    off_t current = 0;
    void * buffer = malloc(getpagesize());

    status = SMBCreateFile(handle, path,
        0x0001, /* dwDesiredAccess: FILE_READ_DATA */
        0x0007, /* dwShareMode: FILE_SHARE_ALL */
        NULL,   /* lpSecurityAttributes */
        0x0001, /* dwCreateDisposition: OPEN_EXISTING */
        0x0000, /* dwFlagsAndAttributes */
        &hFile);
    if (!NT_SUCCESS(status)) {
        nt_error(status, "SMBCreateFile(%s)", path);
        goto done;
    }

    do {
        off_t count = 0;

        status = SMBReadFile(handle, hFile,
                buffer, current, getpagesize(), &count);
        if (!NT_SUCCESS(status)) {
            break;
        }

        if (count == 0) {
            break;
        }

        printf("%*.*s", (int)count, (int)count, (char *)buffer);
        current += count;
    } while (NT_SUCCESS(status));

    SMBCloseFile(handle, hFile);

done:
    free(buffer);
}

int main(int argc, const char ** argv)
{
    SMBHANDLE handle;
    NTSTATUS status;

    if (argc < 3) {
        usage();
    }

    status = SMBOpenServer(argv[1], &handle);
    if (!NT_SUCCESS(status)) {
        nt_error(status, "SMBOpenServer(%s)", argv[1]);
        return EX_PROTOCOL;
    }

    smbecho(handle);

    for (int i = 2; i < argc; ++i) {
        cat_file(handle, argv[i]);
    }

    SMBReleaseServer(handle);
    return EX_OK;
}

/* vim: set ts=4 et tw=79 : */
