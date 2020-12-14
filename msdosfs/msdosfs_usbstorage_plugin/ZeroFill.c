/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 *  ZeroFill.c
 *  livefiles_msdos
 *
 *  Created by Yakov Ben Zaken on 28/11/2017.
 *
 */

#include "ZeroFill.h"
#include "Logger.h"
#include "RawFile_Access_M.h"

#define ZERO_BUF_SIZE   (1024*1024)

static void* gpvZeroBuf = NULL;


int
ZeroFill_Init()
{
    if ( gpvZeroBuf )
    {
        return 0;
    }

    gpvZeroBuf = mmap(NULL, ZERO_BUF_SIZE, PROT_READ, MAP_ANON | MAP_PRIVATE, -1, 0); //XXXab: followup on using off_t as tag?

    if ( gpvZeroBuf == MAP_FAILED )
    {
        gpvZeroBuf = NULL;
        return errno;
    }

    return 0;
}

void
ZeroFill_DeInit()
{
    if ( gpvZeroBuf )
    {
        if (munmap(gpvZeroBuf, ZERO_BUF_SIZE))
        {
            int error = errno;
            MSDOS_LOG(LEVEL_ERROR, "failed to unmap zero buffer: %d", error);
        }
    }

    gpvZeroBuf = NULL;
}

int
ZeroFill_Fill( int iFd, uint64_t uOffset, uint32_t uLength )
{
    int iErr                    = 0;
    long lWriteSize             = 0;
    uint64_t uCurWriteOffset    = 0;
    uint32_t uCurWriteLen       = 0;
    uint32_t uDataWriten        = 0;

    if ( gpvZeroBuf == NULL )
    {
        iErr = EINVAL;
        goto exit;
    }

    while ( uDataWriten < uLength )
    {
        uCurWriteOffset = uOffset+uDataWriten;
        uCurWriteLen    = MIN( (uLength - uDataWriten), ZERO_BUF_SIZE );

        lWriteSize = pwrite( iFd, gpvZeroBuf, uCurWriteLen, uCurWriteOffset );
        if ( lWriteSize != uCurWriteLen )
        {
            iErr = errno;
            MSDOS_LOG(LEVEL_ERROR, "ZeroFill_Fill: Failed to write. Error [%d]\n",iErr);
            goto exit;
        }

        uDataWriten += uCurWriteLen;
    }

exit:
    return iErr;
}

int
ZeroFill_FillClusterSuffixWithZeros( NodeRecord_s* psNodeRecord, uint64_t uFillFromOffset)
{
    MSDOS_LOG( LEVEL_DEBUG, "ZeroFill_FillClusterSuffixWithZeros = %llu\n", uFillFromOffset);

    int iErr = 0;
    FileSystemRecord_s* psFSRecord = GET_FSRECORD(psNodeRecord);
    uint64_t uBytesToFill = ROUND_UP(uFillFromOffset,CLUSTER_SIZE(psFSRecord)) - uFillFromOffset;

    RAWFILE_write(psNodeRecord, uFillFromOffset, uBytesToFill, gpvZeroBuf, &iErr);

    return iErr;
}
