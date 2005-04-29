/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOMBUFQUEUE_H
#define _IOMBUFQUEUE_H

extern "C" {
#include <sys/param.h>
#include <sys/mbuf.h>
}

struct IOMbufQueue {
    mbuf_t  head;
    mbuf_t  tail;
    UInt32         size;
    UInt32         capacity;
};

static __inline__
UInt32 IOMbufFree(mbuf_t m)
{
    UInt32 count = 0;
    mbuf_t tempmb = m;

	while (tempmb) // FIXME if 3730918 is implemented, mbuf_freem_list() will return the free count for us.
	{
		tempmb = mbuf_nextpkt( tempmb );
		count++;
	}
	mbuf_freem_list(m);
    return count;
}

static __inline__
void IOMbufQueueInit(IOMbufQueue * q, UInt32 capacity)
{
    q->head = q->tail = 0;
    q->size = 0;
    q->capacity = capacity;
}

static __inline__
bool IOMbufQueueEnqueue(IOMbufQueue * q, mbuf_t m)
{
    if (q->size >= q->capacity) return false;

    if (q->size++ > 0)
        mbuf_setnextpkt(q->tail , m );
    else
        q->head = m;

    for (q->tail = m;
         mbuf_nextpkt(q->tail);
         q->tail = mbuf_nextpkt(q->tail), q->size++)
        ;

    return true;
}

static __inline__
bool IOMbufQueueEnqueue(IOMbufQueue * q, IOMbufQueue * qe)
{
    if (qe->size)
    {
        if (q->size == 0)
            q->head = qe->head;
        else
            mbuf_setnextpkt(q->tail , qe->head);
        q->tail  = qe->tail;
        q->size += qe->size;

        qe->head = qe->tail = 0;
        qe->size = 0;
    }
    return true;
}

static __inline__
void IOMbufQueuePrepend(IOMbufQueue * q, mbuf_t m)
{
    mbuf_t tail;

    for (tail = m, q->size++;
         mbuf_nextpkt(tail);
         tail = mbuf_nextpkt(tail), q->size++)
        ;

    mbuf_setnextpkt(tail , q->head);
    if (q->tail == 0)
        q->tail = tail;
    q->head = m;
}

static __inline__
void IOMbufQueuePrepend(IOMbufQueue * q, IOMbufQueue * qp)
{
    if (qp->size)
    {
        mbuf_setnextpkt(qp->tail , q->head);
        if (q->tail == 0)
            q->tail = qp->tail;
        q->head  = qp->head;
        q->size += qp->size;

        qp->head = qp->tail = 0;
        qp->size = 0;
    }
}

static __inline__
mbuf_t IOMbufQueueDequeue(IOMbufQueue * q)
{   
    mbuf_t m = q->head;
    if (m)
    {
        if ((q->head = mbuf_nextpkt(m)) == 0)
            q->tail = 0;
        mbuf_setnextpkt(m , 0);
        q->size--;
    }
    return m;
}

static __inline__
mbuf_t IOMbufQueueDequeueAll(IOMbufQueue * q)
{
    mbuf_t m = q->head;
    q->head = q->tail = 0;
    q->size = 0;
    return m;
}

static __inline__
mbuf_t IOMbufQueuePeek(IOMbufQueue * q)
{
    return q->head;
}

static __inline__
UInt32 IOMbufQueueGetSize(IOMbufQueue * q)
{
    return q->size;
}

static __inline__
UInt32 IOMbufQueueGetCapacity(IOMbufQueue * q)
{
    return q->capacity;
}

static __inline__
void IOMbufQueueSetCapacity(IOMbufQueue * q, UInt32 capacity)
{
	q->capacity = capacity;
}

#endif /* !_IOMBUFQUEUE_H */
