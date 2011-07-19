/*
 * Copyright (c) 1998-2008 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* IOMbufMemoryCursor.cpp created by gvdl on 1999-1-20 */

#include <sys/cdefs.h>

__BEGIN_DECLS
#include <IOKit/assert.h>

#include <sys/param.h>
#include <sys/mbuf.h>
struct mbuf * m_getpackets(int num_needed, int num_with_pkthdrs, int how);
__END_DECLS

#include <IOKit/network/IOMbufMemoryCursor.h>
#include <IOKit/IOLib.h>

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif /* MIN */

#define next_page(x) trunc_page(x + PAGE_SIZE)

#if 0 
#define ERROR_LOG(args...)  IOLog(args)
#else
#define ERROR_LOG(args...)
#endif

/* Define the meta class stuff for the entire file here */
OSDefineMetaClassAndStructors(IOMbufMemoryCursor, IOMemoryCursor)
OSMetaClassDefineReservedUnused( IOMbufMemoryCursor,  0);
OSMetaClassDefineReservedUnused( IOMbufMemoryCursor,  1);
OSMetaClassDefineReservedUnused( IOMbufMemoryCursor,  2);
OSMetaClassDefineReservedUnused( IOMbufMemoryCursor,  3);

OSDefineMetaClassAndStructors(IOMbufNaturalMemoryCursor, IOMbufMemoryCursor)
OSDefineMetaClassAndStructors(IOMbufBigMemoryCursor, IOMbufMemoryCursor)
OSDefineMetaClassAndStructors(IOMbufLittleMemoryCursor, IOMbufMemoryCursor)

#ifdef __ppc__
OSDefineMetaClassAndStructors(IOMbufDBDMAMemoryCursor, IOMbufMemoryCursor)
#endif /* __ppc__ */

/*********************** class IOMbufMemoryCursor ***********************/
#define super IOMemoryCursor

bool IOMbufMemoryCursor::initWithSpecification(OutputSegmentFunc outSeg,
                                               UInt32 maxSegmentSize,
                                               UInt32 maxTransferSize,
                                               UInt32 align)
{
    return false;
}

bool IOMbufMemoryCursor::initWithSpecification(OutputSegmentFunc inOutSeg,
                                               UInt32 inMaxSegmentSize,
                                               UInt32 inMaxNumSegments)
{
    if (!super::initWithSpecification(inOutSeg, inMaxSegmentSize, 0, 1))
        return false;

#if 0
    // It is too confusing to force the max segment size to be at least
    // as large as a page. Most Enet devices only have 11-12 bit fields,
    // enough for a full size frame, and also the PAGE_SIZE parameter
    // may be architecture dependent.

    assert(inMaxSegmentSize >= PAGE_SIZE);
    if (inMaxSegmentSize < PAGE_SIZE)
        return false;
#else
    if (!inMaxSegmentSize)
        return false;
#endif

    maxSegmentSize = MIN(maxSegmentSize, PAGE_SIZE);
    maxNumSegments = inMaxNumSegments;
    coalesceCount = 0;

    return true;
}

//
// Copy the src packet into the destination packet. The amount to copy is
// determined by the dstm->m_len, which is setup by analyseSegments, see below.
// Mbufs(except for the head) from the source are freed as they are copied and the
// remaining uncopied mbufs are attached to the end of the dest chain.
#define BCOPY(s, d, l) do { bcopy((void *) s, (void *) d, l); } while(0)

static inline void coalesceSegments(mbuf_t srcm, mbuf_t dstm)
{
    uintptr_t src, dst;
    SInt32 srcLen, dstLen;
    mbuf_t temp;
    mbuf_t head = srcm;
	
    srcLen = mbuf_len( srcm );
    src = (uintptr_t) mbuf_data(srcm);

    dstLen = mbuf_len( dstm );
    dst = (uintptr_t) mbuf_data( dstm );
	
    for (;;) {
        if (srcLen < dstLen) {

            // Copy remainder of src mbuf to current dst.
            BCOPY(src, dst, srcLen);
            dst += srcLen;
            dstLen -= srcLen;

            // Move on to the next source mbuf.
            temp = mbuf_next( srcm ); assert(temp);
			if(srcm != head)
				mbuf_free(srcm);
            srcm = temp;

            srcLen = mbuf_len( srcm );
            src = (uintptr_t)mbuf_data(srcm);
        }
        else if (srcLen > dstLen) {

            // Copy some of src mbuf to remaining space in dst mbuf.
            BCOPY(src, dst, dstLen);
            src += dstLen;
            srcLen -= dstLen;
            
            // Move on to the next destination mbuf.
            temp = mbuf_next( dstm ); assert(temp);
            dstm = temp;

            dstLen = mbuf_len( dstm );
            dst = (uintptr_t)mbuf_data( dstm );
        }
        else {  /* (srcLen == dstLen) */

            // copy remainder of src into remaining space of current dst
            BCOPY(src, dst, srcLen);

            // Free current mbuf and move the current onto the next
            temp = mbuf_next( srcm );
			if(srcm != head)
				mbuf_free(srcm);
			srcm = temp;
			
            // Do we have any more dest buffers to copy to?
            if (! mbuf_next ( dstm ))
			{
				// nope- hook the remainder of source chain to end of dest chain
				mbuf_setnext(dstm, srcm);
                break;
			}
            dstm = mbuf_next ( dstm );

            assert(srcm);
            dstLen = mbuf_len ( dstm );
            dst = (uintptr_t)mbuf_data( dstm );
            srcLen = mbuf_len( srcm );
            src = (uintptr_t)mbuf_data( srcm );
        }
    }
}

static const UInt32 kMBufDataCacheSize = 16;

static inline bool analyseSegments(
    mbuf_t packet,        /* input packet mbuf */
    const UInt32 mbufsInCache,  /* number of entries in segsPerMBuf[] */
    const UInt32 segsPerMBuf[], /* segments required per mbuf */
    SInt32 numSegs,               /* total number of segments */
    const UInt32 maxSegs)       /* max controller segments per mbuf */
{
    mbuf_t newPacket;     // output mbuf chain.
    mbuf_t out;           // current output mbuf link.
    SInt32 outSize;               // size of current output mbuf link.
    SInt32 outSegs;               // segments for current output mbuf link.
    SInt32 doneSegs;              // segments for output mbuf chain.
    SInt32 outLen;                // remaining length of input buffer.

    mbuf_t in = packet;   // save the original input packet pointer.
    UInt32 inIndex = 0;
    const uint32_t c_mlen = mbuf_get_mlen();

    // Allocate a mbuf (non header mbuf) to begin the output mbuf chain.
    
    if(mbuf_get(MBUF_DONTWAIT, MT_DATA, &newPacket))
    {
        ERROR_LOG("analyseSegments: MGET() 1 error\n");
        return false;
    }

    /* Initialise outgoing packet controls */
    out = newPacket;
    outSize = c_mlen;
    doneSegs = outSegs = outLen = 0;

    // numSegs stores the delta between the total and the max. For each
    // input mbuf consumed, we decrement numSegs.
    //
    numSegs -= maxSegs;

    // Loop through the input packet mbuf 'in' and construct a new mbuf chain
    // large enough to make (numSegs + doneSegs + outSegs) less than or
    // equal to zero.
    //  
    do {
        uintptr_t vmo;
        
        outLen += mbuf_len(in);

        while (outLen > outSize) {
            // Oh dear the current outgoing length is too big.
            if (outSize != MCLBYTES) {
                // Current mbuf is not yet a cluster so promote, then
                // check for error.

                if(mbuf_mclget(MBUF_DONTWAIT, MT_DATA, &out) || !(mbuf_flags(out) & MBUF_EXT) ) 
				{
                    ERROR_LOG("analyseSegments: MCLGET() error\n");
                    goto bombAnalysis;
                }
                
                outSize = MCLBYTES;
                
                continue;
            }
            
            vmo = (uintptr_t)mbuf_data(out);
            mbuf_setlen(out, MCLBYTES);  /* Fill in target copy size */
            doneSegs += (round_page(vmo + MCLBYTES) - trunc_page(vmo))
                     /   PAGE_SIZE;

            // If the number of segments of the output chain, plus
            // the segment for the mbuf we are about to allocate is greater
            // than maxSegs, then abort.
            //
            if (doneSegs + 1 > (int) maxSegs) {
                ERROR_LOG("analyseSegments: maxSegs limit 1 reached! %ld %ld\n",
                          doneSegs, maxSegs);
                goto bombAnalysis;
            }

            mbuf_t tempmbuf;
			if(mbuf_get(MBUF_DONTWAIT, MT_DATA, &tempmbuf))
            {
                ERROR_LOG("analyseSegments: MGET() error\n");
                goto bombAnalysis;
            }
            mbuf_setnext(out, tempmbuf);
            out = tempmbuf;
            outSize = c_mlen;
            outLen -= MCLBYTES;
        }

        // Compute number of segment in current outgoing mbuf.
        vmo = (uintptr_t)mbuf_data(out);
        outSegs = (round_page(vmo + outLen) - trunc_page(vmo)) / PAGE_SIZE;
        if (doneSegs + outSegs > (int) maxSegs) {
            ERROR_LOG("analyseSegments: maxSegs limit 2 reached! %ld %ld %ld\n",
                      doneSegs, outSegs, maxSegs);
            goto bombAnalysis;
        }

        // Get the number of segments in the current inbuf
        if (inIndex < mbufsInCache)
            numSegs -= segsPerMBuf[inIndex];    // Yeah, in cache
        else {
            // Hmm, we have to recompute from scratch. Copy code from genPhys.
            int thisLen = 0, mbufLen;

            vmo = (uintptr_t)mbuf_data(in);
            for (mbufLen = mbuf_len(in); mbufLen; mbufLen -= thisLen) {
                thisLen = MIN(next_page(vmo), vmo + mbufLen) - vmo;
                vmo += thisLen;
                numSegs--;
            }
        }

        // Walk the incoming buffer on one.
        in = mbuf_next(in);
        inIndex++;

        // continue looping until the total number of segments has dropped
        // to an acceptable level, or if we ran out of mbuf links.

    } while (in && ((numSegs + doneSegs + outSegs) > 0));

    if ( (int) (numSegs + doneSegs + outSegs) <= 0) {   // success

        mbuf_setlen(out, outLen);    // Set last mbuf with the remaining length.
        
        // The amount to copy is determine by the segment length in each
        // mbuf linked to newPacket. The sum can be smaller than
        // packet->pkthdr.len;
        //
        coalesceSegments(packet, newPacket);
        
        // The initial header mbuf is preserved, its length set to zero, and
        // linked to the new packet chain.
        // coalesceSegments() has already freed the mbufs that it coalesced into the newPacket chain.
		// It also hooked the remaining chain pointed to by "in" to the end of the newPacket chain.
		// All that remains is to set packet's len to 0 (to "free" the contents that coalesceSegments copied out)
		// and make it the head of the new chain.
        
        mbuf_setlen(packet , 0 );
        mbuf_setnext(packet, newPacket);
        
        return true;
    }

bombAnalysis:

    mbuf_freem(newPacket);
    return false;
}
                               
UInt32 IOMbufMemoryCursor::genPhysicalSegments(mbuf_t packet, void *vector,
                                               UInt32 maxSegs, bool doCoalesce)
{
    bool doneCoalesce = false;

    if (!packet || !(mbuf_flags(packet) & MBUF_PKTHDR))
        return 0;

    if (!maxSegs)
    {
        maxSegs = maxNumSegments;
        if (!maxSegs) return 0;
    }

    if ( mbuf_next(packet) == 0 )
    {
        uintptr_t               src;
        struct IOPhysicalSegment  physSeg;

        /*
         * the packet consists of only 1 mbuf
         * so if the data buffer doesn't span a page boundary
         * we can take the simple way out
         */
        src = (uintptr_t)mbuf_data(packet);

        if ( trunc_page(src) == trunc_page(src + mbuf_len(packet) - 1) )
        {
            physSeg.location = (IOPhysicalAddress) mbuf_data_to_physical((char *)src);
            if ( physSeg.location )
            {
                physSeg.length = mbuf_len(packet);
                (*outSeg)(physSeg, vector, 0);
                return 1;
            }
            
            maxSegs = 1;
            if ( doCoalesce == false ) return 0;
        }
    }

    if ( doCoalesce == true && maxSegs == 1 )
    {
        uintptr_t               src;
        uintptr_t               dst;
        mbuf_t               m;
        mbuf_t               mnext;
        mbuf_t               out;
        UInt32                    len = 0;
        struct IOPhysicalSegment  physSeg;

        if ( mbuf_pkthdr_len(packet) > MCLBYTES ) return 0;

        m = packet;

        // Allocate a non-header mbuf + cluster.
        if (mbuf_getpacket( MBUF_DONTWAIT, &out ))
			return 0;
		mbuf_setflags( out, mbuf_flags( out ) & ~MBUF_PKTHDR );
        dst = (uintptr_t)mbuf_data(out);

        do
        {
            src = (uintptr_t)mbuf_data(m);
            BCOPY( src, dst, mbuf_len(m) );
            dst += mbuf_len(m);
            len += mbuf_len(m);
        } while ( (m = mbuf_next(m)) != 0 );

        mbuf_setlen(out , len);

        dst = (uintptr_t)mbuf_data(out);
        physSeg.location = (IOPhysicalAddress) mbuf_data_to_physical((char *)dst);
        if (!physSeg.location)
        {
            mbuf_free(out);
            return 0;
        }
        physSeg.length = mbuf_len(out);
        (*outSeg)(physSeg, vector, 0);

        m = mbuf_next(packet);
        while (m != 0)
        {
            mnext = mbuf_next(m);
            mbuf_free(m);
            m = mnext;
        }

        // The initial header mbuf is preserved, its length set to zero,
        // and linked to the new packet chain.

        mbuf_setlen(packet , 0);
        mbuf_setnext(packet , out);
        mbuf_setnext(out , 0);

        return 1;
    }

    //
    // Iterate over the mbuf, translating segments were allowed.  When we
    // are not allowed to translate segments then accumulate segment
    // statistics up to kMBufDataCacheSize of mbufs.  Finally
    // if we overflow our cache just count how many segments this
    // packet represents.
    //
    UInt32 segsPerMBuf[kMBufDataCacheSize];

tryAgain:
    UInt32 curMBufIndex = 0;
    UInt32 curSegIndex  = 0;
    UInt32 lastSegCount = 0;
    mbuf_t m = packet;

    // For each mbuf in incoming packet.
    do {
        vm_size_t   mbufLen, thisLen = 0;
        uintptr_t src;

        // Step through each segment in the current mbuf
        for (mbufLen = mbuf_len(m), src = (uintptr_t)mbuf_data(m);
             mbufLen;
             src += thisLen, mbufLen -= thisLen)
        {
            // If maxSegmentSize is atleast PAGE_SIZE, then
            // thisLen = MIN(next_page(src), src + mbufLen) - src;

            thisLen = MIN(mbufLen, maxSegmentSize);
            thisLen = MIN(next_page(src), src + thisLen) - src;

            // If room left then find the current segment addr and output
            if (curSegIndex < maxSegs) {
                struct IOPhysicalSegment physSeg;

                physSeg.location = (IOPhysicalAddress) mbuf_data_to_physical((char *)src);
                if ( physSeg.location == 0 )
                {
                    return doCoalesce ?
                           genPhysicalSegments(packet, vector, 1, true) : 0;
                }
                physSeg.length = thisLen;
                (*outSeg)(physSeg, vector, curSegIndex);
            }

            // Count segments if we are coalescing. 
            curSegIndex++;
        }

        // Cache the segment count data if room is available.
        if (curMBufIndex < kMBufDataCacheSize) {
            segsPerMBuf[curMBufIndex] = curSegIndex - lastSegCount;
            lastSegCount = curSegIndex;
        }

        // Move on to next imcoming mbuf
        curMBufIndex++;
        m = mbuf_next(m);
    } while (m);

    // If we finished cleanly return number of segments found
    if (curSegIndex <= maxSegs)
        return curSegIndex;
    if (!doCoalesce)
        return 0;   // if !coalescing we've got a problem.

    // If we are coalescing and it is possible then attempt coalesce, 
    if (!doneCoalesce
    &&  (UInt) mbuf_pkthdr_len(packet) <= maxSegs * maxSegmentSize) {
        // Hmm, we have to do some coalescing.
        bool analysisRet;
            
        analysisRet = analyseSegments(packet,
                                      MIN(curMBufIndex, kMBufDataCacheSize),
                                      segsPerMBuf,
                                      curSegIndex, maxSegs);
        if (analysisRet) {
            doneCoalesce = true;
            coalesceCount++;
            goto tryAgain;
        }
    }

    assert(!doneCoalesce);  // Problem in Coalesce code.
    packetTooBigErrors++;
    return 0;
}

UInt32 IOMbufMemoryCursor::getAndResetCoalesceCount()
{
    UInt32 cnt = coalesceCount; coalesceCount = 0; return cnt;
}

/* the extern "C" wrappers that follow, are used for binary compatability since
changing the prototypes from "struct mbuf *" to "mbuf_t" causes the symbol name to change. */

/********************* class IOMbufBigMemoryCursor **********************/
IOMbufBigMemoryCursor *
IOMbufBigMemoryCursor::withSpecification(UInt32 maxSegSize, UInt32 maxNumSegs)
{
    IOMbufBigMemoryCursor *me = new IOMbufBigMemoryCursor;

    if (me && !me->initWithSpecification(&bigOutputSegment,
                                         maxSegSize, maxNumSegs)) {
        me->release();
        return 0;
    }

    return me;
}

extern "C" UInt32 _ZN21IOMbufBigMemoryCursor19getPhysicalSegmentsEP4mbufPN14IOMemoryCursor15PhysicalSegmentEm(
      IOMbufBigMemoryCursor *self, void *packet, struct IOPhysicalSegment *vector, UInt32 numVectorSegments)
{
	return self->getPhysicalSegments((mbuf_t)packet, vector,numVectorSegments);
}
																								   
UInt32
IOMbufBigMemoryCursor::getPhysicalSegments(mbuf_t packet,
                       struct IOPhysicalSegment *vector,
                       UInt32 numVectorSegments)
{
    return genPhysicalSegments(packet, vector, numVectorSegments, false);
}

extern "C" UInt32 _ZN21IOMbufBigMemoryCursor31getPhysicalSegmentsWithCoalesceEP4mbufPN14IOMemoryCursor15PhysicalSegmentEm(
 IOMbufBigMemoryCursor *self, void *packet, struct IOPhysicalSegment *vector, UInt32 numVectorSegments)
{
	return self->getPhysicalSegmentsWithCoalesce((mbuf_t)packet, vector,numVectorSegments);
}

UInt32
IOMbufBigMemoryCursor::getPhysicalSegmentsWithCoalesce(mbuf_t packet,
                                   struct IOPhysicalSegment *vector,
                                   UInt32 numVectorSegments)
{
    return genPhysicalSegments(packet, vector, numVectorSegments, true);
}


/******************* class IOMbufNaturalMemoryCursor ********************/
IOMbufNaturalMemoryCursor *
IOMbufNaturalMemoryCursor::withSpecification(UInt32 maxSegSize, UInt32 maxNumSegs)
{
    IOMbufNaturalMemoryCursor *me = new IOMbufNaturalMemoryCursor;

    if (me && !me->initWithSpecification(&naturalOutputSegment,
                                         maxSegSize, maxNumSegs)) {
        me->release();
        return 0;
    }

    return me;
}

extern "C" UInt32 _ZN25IOMbufNaturalMemoryCursor19getPhysicalSegmentsEP4mbufPN14IOMemoryCursor15PhysicalSegmentEm(
 IOMbufNaturalMemoryCursor *self, void *packet, struct IOPhysicalSegment *vector, UInt32 numVectorSegments)
{
	return self->getPhysicalSegments((mbuf_t)packet, vector,numVectorSegments);
}

UInt32
IOMbufNaturalMemoryCursor::getPhysicalSegments(mbuf_t packet,
                       struct IOPhysicalSegment *vector,
                       UInt32 numVectorSegments)
{
    return genPhysicalSegments(packet, vector, numVectorSegments, false);
}

extern "C" UInt32 _ZN25IOMbufNaturalMemoryCursor31getPhysicalSegmentsWithCoalesceEP4mbufPN14IOMemoryCursor15PhysicalSegmentEm(
IOMbufNaturalMemoryCursor *self, void *packet, struct IOPhysicalSegment *vector, UInt32 numVectorSegments)
{
	return self->getPhysicalSegmentsWithCoalesce((mbuf_t)packet, vector,numVectorSegments);
}


UInt32
IOMbufNaturalMemoryCursor::getPhysicalSegmentsWithCoalesce(mbuf_t packet,
                                   struct IOPhysicalSegment *vector,
                                   UInt32 numVectorSegments)
{
    return genPhysicalSegments(packet, vector, numVectorSegments, true);
}


/******************** class IOMbufLittleMemoryCursor ********************/
IOMbufLittleMemoryCursor *
IOMbufLittleMemoryCursor::withSpecification(UInt32 maxSegSize, UInt32 maxNumSegs)
{
    IOMbufLittleMemoryCursor *me = new IOMbufLittleMemoryCursor;

    if (me && !me->initWithSpecification(&littleOutputSegment,
                                         maxSegSize, maxNumSegs)) {
        me->release();
        return 0;
    }

    return me;
}

extern "C" UInt32 _ZN24IOMbufLittleMemoryCursor19getPhysicalSegmentsEP4mbufPN14IOMemoryCursor15PhysicalSegmentEm(
 IOMbufLittleMemoryCursor *self, void *packet, struct IOPhysicalSegment *vector, UInt32 numVectorSegments)
{
	return self->getPhysicalSegments((mbuf_t)packet, vector,numVectorSegments);
}

UInt32
IOMbufLittleMemoryCursor::getPhysicalSegments(mbuf_t packet,
                       struct IOPhysicalSegment *vector,
                       UInt32 numVectorSegments)
{
    return genPhysicalSegments(packet, vector, numVectorSegments, false);
}

extern "C" UInt32 _ZN24IOMbufLittleMemoryCursor31getPhysicalSegmentsWithCoalesceEP4mbufPN14IOMemoryCursor15PhysicalSegmentEm(
  IOMbufLittleMemoryCursor *self, void *packet, struct IOPhysicalSegment *vector, UInt32 numVectorSegments)
{
	return self->getPhysicalSegmentsWithCoalesce((mbuf_t)packet, vector,numVectorSegments);
}


UInt32
IOMbufLittleMemoryCursor::getPhysicalSegmentsWithCoalesce(mbuf_t packet,
                                   struct IOPhysicalSegment *vector,
                                   UInt32 numVectorSegments)
{
    return genPhysicalSegments(packet, vector, numVectorSegments, true);
}


/******************** class IOMbufDBDMAMemoryCursor *********************/
#ifdef __ppc__

IOMbufDBDMAMemoryCursor *
IOMbufDBDMAMemoryCursor::withSpecification(UInt32 maxSegSize, UInt32 maxNumSegs)
{
    IOMbufDBDMAMemoryCursor *me = new IOMbufDBDMAMemoryCursor;

    if (me && !me->initWithSpecification(&dbdmaOutputSegment,
                                         maxSegSize, maxNumSegs)) {
        me->release();
        return 0;
    }

    return me;
}

extern "C" UInt32 _ZN23IOMbufDBDMAMemoryCursor19getPhysicalSegmentsEP4mbufP17IODBDMADescriptorm(
IOMbufDBDMAMemoryCursor *self, void *packet, struct IODBDMADescriptor *vector, UInt32 numVectorSegments)
{
	return self->getPhysicalSegments((mbuf_t)packet, vector, numVectorSegments);
}

UInt32
IOMbufDBDMAMemoryCursor::getPhysicalSegments(mbuf_t packet,
                                   struct IODBDMADescriptor *vector,
                                   UInt32 numVectorSegments)
{
    return genPhysicalSegments(packet, vector, numVectorSegments, false);
}

extern "C" UInt32 _ZN23IOMbufDBDMAMemoryCursor31getPhysicalSegmentsWithCoalesceEP4mbufP17IODBDMADescriptorm(
IOMbufDBDMAMemoryCursor *self, void *packet, struct IODBDMADescriptor *vector, UInt32 numVectorSegments)
{
	return self->getPhysicalSegmentsWithCoalesce((mbuf_t)packet, vector, numVectorSegments);
}


UInt32
IOMbufDBDMAMemoryCursor::getPhysicalSegmentsWithCoalesce(mbuf_t packet,
                                   struct IODBDMADescriptor *vector,
                                   UInt32 numVectorSegments)
{
    return genPhysicalSegments(packet, vector, numVectorSegments, true);
}

#endif /* __ppc__ */
