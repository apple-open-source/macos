//
//  GTrace.cpp
//
//  Created by bparke on 3/17/17.
//


#include "GTrace.hpp"


#if defined(_KERNEL_) || defined (KERNEL)
#pragma mark - OSObject...
OSDefineMetaClassAndStructors(GTrace, OSObject);

bool GTrace::init()
{
    bool    bRet = super::init();

    if (true == bRet)
    {
        fInUseLock = IOLockAlloc();
        if (NULL == fInUseLock)
        {
            bRet  = false;
        }
        else
        {
            GTRACE_RAII_LOCK(fInUseLock);

            fInitialized = false;
            fProvider = NULL;
            fBuffer = NULL;
            fBufferSize = 0;
            fLineCount = 0;
            fLineMask = 0;
            fNextLine = 0;
            fComponentID = 0;
            fObjectID = 0;
        }
    }

    return (bRet);
}

void GTrace::free()
{
    // RAII scope
    if (fInitialized)
    {
        GTRACE_RAII_LOCK(fInUseLock);

        fInitialized = false;

        if (NULL != fBuffer)
        {
            GTRACE_SAFE_FREE(fBuffer, fBufferSize);
            fBufferSize = 0;
        }

        if (NULL != fProvider)
        {
            fProvider->removeProperty(kGTracePropertyTokens);
            fProvider->removeProperty(kGTracePropertyTokenMarker);
            fProvider->removeProperty(kGTracePropertyTokenCount);
            OSSafeReleaseNULL(fProvider);
        }
    }

    if (NULL != fInUseLock)
    {
        IOLockFree(fInUseLock);
        fInUseLock = NULL;
    }

    super::free();
}
#endif /*defined(_KERNEL_) || defined (KERNEL)*/


#pragma mark - GTrace...
void GTrace::delayFor(uint8_t seconds)
{
    while (seconds)
    {
        seconds--;
        GTRACE_SLEEP(1);
    }
}

void GTrace::freeGTraceResources(void)
{
#if defined(_KERNEL_) || defined (KERNEL)
    // GTrace::free does this for kernel clients.
#else /*defined(_KERNEL_) || defined (KERNEL)*/
    if (fInitialized)
    {
        GTRACE_RAII_LOCK(fInUseLock);

        fInitialized = false;

        if (NULL != fBuffer)
        {
            GTRACE_SAFE_FREE(fBuffer, fBufferSize);
            fBufferSize = 0;
        }
    }
#endif /*defined(_KERNEL_) || defined (KERNEL)*/
}

IOReturn GTrace::initWithDeviceCountAndID( GTRACE_SERVICE * provider, const uint32_t lineCount, const uint64_t componentID )
{
    uint32_t    bufSize = 0;

    if (fInitialized)
    {
        return (kIOReturnNotPermitted);
    }

#if defined(_KERNEL_) || defined (KERNEL)
    if (!init())
    {
        return (kIOReturnNotOpen);
    }
#endif /*defined(_KERNEL_) || defined (KERNEL)*/

    GTRACE_RAII_LOCK(fInUseLock);

#if defined(_KERNEL_) || defined (KERNEL)
    if (NULL == provider)
    {
        return (kIOReturnBadArgument);
    }
#endif /*defined(_KERNEL_) || defined (KERNEL)*/

    if (0 == lineCount)
    {
        return (kIOReturnBadArgument);
    }

    bufSize = linesToSize(lineCount);
    if (0 == bufSize)
    {
        return (kIOReturnBadArgument);
    }

    fBuffer = reinterpret_cast<sGTrace *>(GTRACE_MALLOC(bufSize));
    if (NULL == fBuffer)
    {
        return (kIOReturnNoMemory);
    }

    fProvider = provider;

#if defined(_KERNEL_) || defined (KERNEL)
    fProvider->retain();

    fObjectID = static_cast<uint32_t>(fProvider->getRegistryEntryID() & kGTRACE_REGISTRYID_MASK);
#else /*defined(_KERNEL_) || defined (KERNEL)*/
    fObjectID = static_cast<uint64_t>(static_cast<uint32_t>((mach_absolute_time() & 0x0000000000FFFFFF) |
                                                            ((componentID & kGTRACE_COMPONENT_MASK) << 24)));
#endif /*defined(_KERNEL_) || defined (KERNEL)*/

    fBufferSize = bufSize;
    bzero(fBuffer,fBufferSize);

    fLineCount = sizeToLines(fBufferSize);
    fLineMask = fLineCount - 1;
    fNextLine = 0;

    fComponentID = (componentID & kGTRACE_COMPONENT_MASK);

    fInitialized = true;

    return (kIOReturnSuccess);
}

void GTrace::recordToken(const uint16_t line,
                         const uint16_t tag1, const uint64_t arg1,
                         const uint16_t tag2, const uint64_t arg2,
                         const uint16_t tag3, const uint64_t arg3,
                         const uint16_t tag4, const uint64_t arg4)
{
    if (fInitialized)
    {
        GTRACE_RETAIN;

        sGTrace * const buffer = &(fBuffer[getLine()]);
        buffer->traceEntry.timestamp = mach_continuous_time();
        buffer->traceEntry.traceID.ID.line = line;
        buffer->traceEntry.traceID.ID.component = fComponentID;
#if defined(_KERNEL_) || defined (KERNEL)
        buffer->traceEntry.threadInfo.TI.cpu = static_cast<uint8_t>(cpu_number());
        buffer->traceEntry.threadInfo.TI.threadID = (thread_tid(current_thread()) & kTHREAD_ID_MASK);
#else /*defined(_KERNEL_) || defined (KERNEL)*/
        buffer->traceEntry.threadInfo.TI.cpu = 0;
        buffer->traceEntry.threadInfo.TI.threadID = (((uintptr_t)pthread_self()) & kTHREAD_ID_MASK);
#endif /*defined(_KERNEL_) || defined (KERNEL)*/
        buffer->traceEntry.threadInfo.TI.registryID = fObjectID;
        buffer->traceEntry.argsTag.TAG.targ[0] = tag1;
        buffer->traceEntry.argsTag.TAG.targ[1] = tag2;
        buffer->traceEntry.argsTag.TAG.targ[2] = tag3;
        buffer->traceEntry.argsTag.TAG.targ[3] = tag4;
        buffer->traceEntry.args.ARGS.u64s[0] = arg1;
        buffer->traceEntry.args.ARGS.u64s[1] = arg2;
        buffer->traceEntry.args.ARGS.u64s[2] = arg3;
        buffer->traceEntry.args.ARGS.u64s[3] = arg4;

        GTRACE_RELEASE;
    }
}

IOReturn GTrace::publishTokens( void )
{
#if defined(_KERNEL_) || defined (KERNEL)
    IOReturn    kr = kIOReturnSuccess;

    if (NULL == fProvider)
    {
        return (kIOReturnNotAttached);
    }

    if (NULL != fBuffer)
    {
        return (kIOReturnNotAttached);
    }

    GTRACE_RAII_LOCK(fInUseLock);

    if (false == fProvider->setProperty(kGTracePropertyTokens, fBuffer, fBufferSize))
    {
        kr = kIOReturnIOError;
    }

    if (kIOReturnSuccess == kr)
    {
        if (false == fProvider->setProperty(kGTracePropertyTokenMarker, fNextLine, 16))
        {
            kr = kIOReturnIOError;
        }
    }

    if (kIOReturnSuccess == kr)
    {
        if (false == fProvider->setProperty(kGTracePropertyTokenCount, fLineCount, 16))
        {
            kr = kIOReturnIOError;
        }
    }

    return (kr);
#else /*defined(_KERNEL_) || defined (KERNEL)*/
    return (kIOReturnUnsupported);
#endif /*defined(_KERNEL_) || defined (KERNEL)*/

}

IOReturn GTrace::copyTokens( uintptr_t ** buffer, uint32_t * length, uint32_t * tokenMarker, uint32_t * tokenCount )
{
    IOReturn    ret = kIOReturnNotPermitted;

    if (fInitialized)
    {
        GTRACE_RAII_LOCK(fInUseLock);

        if ((NULL != buffer) && (NULL != length))
        {
            if (NULL != fBuffer)
            {
                sGTrace * const     traceBuffer = fBuffer;
                uintptr_t *         dstBuffer = reinterpret_cast<uintptr_t *>(GTRACE_MALLOC(fBufferSize));
                if (NULL != dstBuffer)
                {
                    *length = fBufferSize;
                    *buffer = dstBuffer;
                    memcpy(dstBuffer, traceBuffer, *length);

                    // Post process and obfuscate any kernel pointers.
                    sGTrace     * gtBuffer = reinterpret_cast<sGTrace *>(dstBuffer);
                    if (gtBuffer)
                    {
                        uint32_t    lineCount = 0;
                        while (lineCount < fLineCount)
                        {
                            for (uint8_t tagIndex = 0; tagIndex < 4; tagIndex++)
                            {
                                gtBuffer[lineCount].traceEntry.args.ARGS.u64s[tagIndex] = makeExternalReference(gtBuffer[lineCount], tagIndex);
                            }
                            lineCount++;
                        }
                    }

                    if (NULL != tokenMarker)
                    {
                        *tokenMarker = (fNextLine & fLineMask);
                    }

                    if (NULL != tokenCount)
                    {
                        *tokenCount = fLineCount;
                    }

                    ret = kIOReturnSuccess;
                }
                else
                {
                    ret = kIOReturnNoMemory;
                }
            }
            else
            {
                ret = kIOReturnNotReadable;
            }
        }
        else
        {
            ret = kIOReturnBadArgument;
        }
    }

    return (ret);
}

IOReturn GTrace::setTokens( const uintptr_t * buffer, const uint32_t bufferSize )
{
    IOReturn    ret = kIOReturnNotPermitted;
    uint32_t    lineCount = 0;

    if (fInitialized)
    {
        if ((NULL != buffer) && (0 != bufferSize))
        {
            lineCount = sizeToLines(bufferSize);

            GTRACE_RAII_LOCK(fInUseLock);

            if ((lineCount) && (lineCount <= fLineCount))
            {
                if (NULL != fBuffer)
                {
                    memcpy(fBuffer, buffer, linesToSize(lineCount));
                    ret = kIOReturnSuccess;
                }
                else
                {
                    ret = kIOReturnNotWritable;
                }
            }
            else
            {
                ret = kIOReturnOverrun;
            }
        }
        else
        {
            ret = kIOReturnBadArgument;
        }
    }
    
    return (ret);
}

IOReturn GTrace::printToken( uint32_t line )
{
    IOReturn            ret = kIOReturnNotPermitted;
    uint64_t            args[4] = {0};

    if (fInitialized)
    {
        GTRACE_RAII_LOCK(fInUseLock);

        line &= fLineMask;

        // Obfuscate any kernel pointers.
        for (uint8_t tagIndex = 0; tagIndex < 4; tagIndex++)
        {
            args[tagIndex] = makeExternalReference(fBuffer[line], tagIndex);
        }

        GTRACE_PRINTF("%#llx:%#llx:%#llx:%#llx:%#llx:%#llx:%#llx:%#llx\n",
                      fBuffer[line].traceEntry.timestamp,
                      fBuffer[line].traceEntry.traceID.ID.u64,
                      fBuffer[line].traceEntry.threadInfo.TI.u64,
                      fBuffer[line].traceEntry.argsTag.TAG.u64,
                      args[0],
                      args[1],
                      args[2],
                      args[3]
                      );

        ret = kIOReturnSuccess;
    }

    return (ret);
}

