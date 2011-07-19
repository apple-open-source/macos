/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _NTTYPES_H
#define _NTTYPES_H

/* for now until 7245052 gets fixed */
#if !defined(STATUS_SUCCESS)
#define STATUS_SUCCESS                  0x00000000
#endif
#if !defined(STATUS_UNEXPECTED_IO_ERROR)
#define STATUS_UNEXPECTED_IO_ERROR      0xC00000E9
#endif
#if !defined(STATUS_CONNECTION_REFUSED)
#define STATUS_CONNECTION_REFUSED       0xC0000236
#endif
#if !defined(STATUS_NO_SUCH_DEVICE)
#define STATUS_NO_SUCH_DEVICE           0xC000000E
#endif
#if !defined(STATUS_BUFFER_OVERFLOW)
#define STATUS_BUFFER_OVERFLOW          0x80000005
#endif
#if !defined(STATUS_NO_MEMORY)
#define STATUS_NO_MEMORY                0xC0000017
#endif
#if !defined(STATUS_OBJECT_PATH_SYNTAX_BAD)
#define STATUS_OBJECT_PATH_SYNTAX_BAD   0xC000003B
#endif
#if !defined(STATUS_INVALID_HANDLE)
#define STATUS_INVALID_HANDLE           0xC0000008
#endif
#if !defined(STATUS_INVALID_PARAMETER)
#define STATUS_INVALID_PARAMETER        0xC000000D
#endif
#if !defined(STATUS_UNSUCCESSFUL)
#define STATUS_UNSUCCESSFUL             0xC0000001
#endif
#if !defined(STATUS_LOGON_FAILURE)
#define STATUS_LOGON_FAILURE            0xC000006D
#endif
#if !defined(STATUS_BAD_NETWORK_NAME)
#define STATUS_BAD_NETWORK_NAME         0xC00000CC
#endif
#if !defined(STATUS_END_OF_FILE)
#define STATUS_END_OF_FILE              0xC0000011
#endif

typedef enum _CREATE_DISPOSITION
{
    FILE_SUPERSEDE      = 0,
    FILE_OPEN           = 1,
    FILE_CREATE         = 2,
    FILE_OPEN_IF        = 3,
    FILE_OVERWRITE      = 4,
    FILE_OVERWRITE_IF   = 5
} CREATE_DISPOSITION;

typedef enum _CREATE_ACTION
{
    FILE_SUPERSEDED     = 0,
    FILE_OPENED         = 1,
    FILE_CREATED        = 2,
    FILE_OVERWRITTEN    = 3
} CREATE_ACTION;

/* MS-SMB2 2.2.13. CreateOptions. */
typedef enum _CREATE_OPTIONS
{
    FILE_DIRECTORY_FILE             = 0x00000001,
    FILE_WRITE_THROUGH              = 0x00000002,
    FILE_SEQUENTIAL_ONLY            = 0x00000004,
    FILE_NO_INTERMEDIATE_BUFFERING  = 0x00000008,
    FILE_SYNCHRONOUS_IO_ALERT       = 0x00000010,
    FILE_SYNCHRONOUS_IO_NONALERT    = 0x00000020,
    FILE_NON_DIRECTORY_FILE         = 0x00000040,
    FILE_CREATE_TREE_CONNECTION     = 0x00000080,
    FILE_COMPLETE_IF_OPLOCKED       = 0x00000100,
    FILE_NO_EA_KNOWLEDGE            = 0x00000200,
    FILE_OPEN_FOR_RECOVERY          = 0x00000400,
    FILE_RANDOM_ACCESS              = 0x00000800,
    FILE_DELETE_ON_CLOSE            = 0x00001000,
    FILE_OPEN_BY_FILE_ID            = 0x00002000,
    FILE_OPEN_FOR_BACKUP_INTENT     = 0x00004000,
    FILE_NO_COMPRESSION             = 0x00008000,
    FILE_RESERVE_OPFILTER           = 0x00100000,
    FILE_OPEN_REPARSE_POINT         = 0x00200000,
    FILE_OPEN_NO_RECALL             = 0x00400000,
    FILE_OPEN_FOR_FREE_SPACE_QUERY  = 0x00800000
} CREATE_OPTIONS;

typedef enum _SHARE_ACCESS
{
    FILE_SHARE_READ     = 0x0001,
    FILE_SHARE_WRITE    = 0x0002,
    FILE_SHARE_DELETE   = 0x0004
} SHARE_ACCESS;

/*
 * Access mask encoding:
 *
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | | | | | | | | | | |1| | | | | | | | | |2| | | | | | | | | |3| |
 * |0|1|2|3|4|5|6|7|8|9|0|1|2|3|4|5|6|7|8|9|0|1|2|3|4|5|6|7|8|9|0|1|
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |R|W|E|A|   |M|S|  standard     |  specific                     |
 * +-------+-------+---------------+-------------------------------+
 *
 * R => generic read
 * W => generic write
 * E => generic execute
 * A => generic all
 * S => SACL access (ACCESS_SYSTEM_SECURITY)
 * M => maximal access
 */

/* Generic rights. */
#define GENERIC_ALL             0x10000000
#define GENERIC_EXECUTE         0x20000000
#define GENERIC_WRITE           0x40000000
#define GENERIC_READ            0x80000000
#define ACCESS_SYSTEM_SECURITY  0x01000000
#define MAXIMUM_ALLOWED         0x02000000

/* Standard rights. */
#define DELETE                  0x00010000
#define READ_CONTROL            0x00020000
#define WRITE_DAC               0x00040000
#define WRITE_OWNER             0x00080000
#define SYNCHRONIZE             0x00100000

#define STANDARD_RIGHTS_REQUIRED ( \
DELETE | READ_CONTROL | WRITE_DAC | WRITE_OWNER \
)

#define STANDARD_RIGHTS_ALL ( \
DELETE | READ_CONTROL | WRITE_DAC | WRITE_OWNER | SYNCHRONIZE \
)

/* File-specific rights. */
#define FILE_LIST_DIRECTORY         0x00000001
#define FILE_READ_DATA              0x00000001
#define FILE_ADD_FILE               0x00000002
#define FILE_WRITE_DATA             0x00000002
#define FILE_ADD_SUBDIRECTORY       0x00000004
#define FILE_APPEND_DATA            0x00000004
#define FILE_CREATE_PIPE_INSTANCE   0x00000004
#define FILE_READ_EA                0x00000008
#define FILE_READ_PROPERTIES        0x00000008
#define FILE_WRITE_EA               0x00000010
#define FILE_WRITE_PROPERTIES       0x00000010
#define FILE_EXECUTE                0x00000020
#define FILE_TRAVERSE               0x00000020
#define FILE_DELETE_CHILD           0x00000040
#define FILE_READ_ATTRIBUTES        0x00000080
#define FILE_WRITE_ATTRIBUTES       0x00000100

#define FILE_ALL_ACCESS (STANDARD_RIGHTS_ALL | 0x000001FF)

#define FILE_GENERIC_EXECUTE ( \
READ_CONTROL | FILE_READ_ATTRIBUTES | FILE_EXECUTE | SYNCHRONIZE )

#define FILE_GENERIC_READ ( \
READ_CONTROL | FILE_READ_ATTRIBUTES | FILE_READ_DATA | \
FILE_READ_EA | SYNCHRONIZE \
)

#define FILE_GENERIC_WRITE ( \
READ_CONTROL | FILE_WRITE_ATTRIBUTES | FILE_WRITE_DATA | \
FILE_WRITE_EA | FILE_APPEND_DATA | SYNCHRONIZE \
)

#endif /* _NTTYPES_H */
/* vim: set sw=4 ts=4 tw=79 et: */
