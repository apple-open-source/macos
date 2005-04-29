/*
 * bufQueue.c --
 *
 *	Implementation of a queue out of buffers.
 *
 * Copyright (c) 2000 by Andreas Kupries <a.kupries@westend.com>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: bufQueue.c,v 1.2 2002/04/25 06:29:48 andreas_kupries Exp $
 */

#include "buf.h"

/*
 * Internal structures used to hold the buffers in the queue.
 */

/*
 * Structure of a node in the queue.
 */

typedef struct QNode_ {
  Buf_Buffer     buf;     /* The buffer managed by the node */
  struct QNode_* nextPtr; /* Reference to the next node/buffer */
} QNode;

/*
 * Structure of the whole queue.
 */

typedef struct Queue_ {
  QNode*    firstNode;  /* Head of the queue */
  QNode*    lastNode;   /* Last node/buffer in the queue */
  int       size;       /* Number of bytes stored in the queue */
#if GT81
  Tcl_Mutex lock;       /* mutex to serialize access to the
			 * queue when more than one thread
			 * is trying to access it. */
#endif
} Queue;

/*
 * Declaration of size to use for new buffers when
 * extending the queue
 */

#define BUF_SIZE (1024)


/*
 *------------------------------------------------------*
 *
 *	Buf_NewQueue --
 *
 *	Creates a new, empty queue.
 *
 *	Sideeffects:
 *		Allocates and initializes memory.
 *
 *	Result:
 *		A queue token.
 *
 *------------------------------------------------------*
 */

Buf_BufferQueue
Buf_NewQueue ()
{
  Queue* q = (Queue*) Tcl_Alloc (sizeof (Queue));

  q->firstNode = (QNode*) NULL;
  q->lastNode  = (QNode*) NULL;
  q->size      = 0;
#if GT81
  q->lock      = (Tcl_Mutex) NULL;
#endif
  return (Buf_BufferQueue) q;
}

/*
 *------------------------------------------------------*
 *
 *	Buf_FreeQueue --
 *
 *	Deletes the specified queue.
 *
 *	Sideeffects:
 *		Deallocates the memory which was
 *		allocated in Buf_NewQueue.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

void
Buf_FreeQueue (queue)
     Buf_BufferQueue queue;
{
  Queue* q = (Queue*) queue;
  QNode* n = q->firstNode;
  QNode* tmp;

#if GT81
  Tcl_MutexLock (&q->lock);
#endif

  while (n != (QNode*) NULL) {
    Buf_DecrRefcount (n->buf);
    tmp = n->nextPtr;
    Tcl_Free ((char*) n);
    n = tmp;
  }

#if GT81
  Tcl_MutexUnlock   (&q->lock);
  Tcl_MutexFinalize (&q->lock);
#endif
  Tcl_Free((char*) q);
  return;
}

/*
 *------------------------------------------------------*
 *
 *	Buf_QueueRead --
 *
 *	Reads information from the queue. The read data
 *	is deleted from the queue.
 *
 *	Sideeffects:
 *		May deallocate memory. Moves the access
 *		pointer in the queue buffers.
 *
 *	Result:
 *		Returns the number of bytes actually read.
 *
 *------------------------------------------------------*
 */

int
Buf_QueueRead (queue, outbuf, size)
     Buf_BufferQueue queue;
     char*           outbuf;
     int             size;
{
  Queue* q = (Queue*) queue;
  QNode* n;
  int    got, read;

#if GT81
  Tcl_MutexLock (&q->lock);
#endif

  n = q->firstNode;

  if ((size <= 0) || (n == (QNode*) NULL)) {
#if GT81
    Tcl_MutexUnlock (&q->lock);
#endif
    return 0;
  }

  read = 0;
  while ((size > 0) && (n != (QNode*) NULL)) {
    got = Buf_Read (n->buf, outbuf, size);

    if (got > 0) {
      read   += got;
      outbuf += got;
      size   -= got;
    }

    if (size > 0) {
      Buf_DecrRefcount (n->buf);
      q->firstNode = n->nextPtr;
      Tcl_Free ((char*) n);
      n = q->firstNode;
    }
  }

  if (n == (QNode*) NULL) {
    q->lastNode = (QNode*) NULL;
  }

  q->size -= read;

#if GT81
  Tcl_MutexUnlock (&q->lock);
#endif

  return read;
}

/*
 *------------------------------------------------------*
 *
 *	Buf_QueueWrite --
 *
 *	Writes information to the queue. The written data
 *	is appended at the end of the queue.
 *
 *	Sideeffects:
 *		May allocate memory. Moves the access
 *		pointer in the queue buffers.
 *
 *	Result:
 *		Returns the number of bytes actually written.
 *
 *------------------------------------------------------*
 */

int
Buf_QueueWrite (queue, inbuf, size)
Buf_BufferQueue queue;
CONST char*     inbuf;
int             size;
{
  Queue* q = (Queue*) queue;
  QNode* n;
  int    done, written;

  if ((size <= 0)) {
    return 0;
  }

#if GT81
  Tcl_MutexLock (&q->lock);
#endif

  n       = q->firstNode;
  written = 0;

  while (size > 0) {
    if (n == (QNode*) NULL) {
      n = (QNode*) Tcl_Alloc (sizeof (QNode));
      n->nextPtr = (QNode*) NULL;
      n->buf     = Buf_CreateFixedBuffer (BUF_SIZE);

      if (q->lastNode == (QNode*) NULL) {
	q->firstNode = n;
      } else {
	q->lastNode->nextPtr = n;
      }

      q->lastNode = n;
    }

    done = Buf_Write (n->buf, inbuf, size);

    if (done > 0) {
      written += done;
      inbuf   += done;
      size    -= done;
    }
    if (size > 0) {
      n = (QNode*) NULL;
    }
  }

  q->size += written;

#if GT81
  Tcl_MutexUnlock (&q->lock);
#endif

  return written;
}

/*
 *------------------------------------------------------*
 *
 *	BufQueue_Append --
 *
 *	Appends a range containing the information
 *	not yet read from the specified buffer to the queue.
 *
 *	Sideeffects:
 *		Creates a range buffer, allocates memory.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

void
Buf_QueueAppend (queue, buf)
     Buf_BufferQueue queue;
     Buf_Buffer      buf;
{
  /* Not the buffer is appended, but a range containing
   * the rest of the data to read from it.
   *
   * Allows external usage of the buffer without affecting
   * the queue. Writing (s.a.) is no problem, as ranges
   * always return that nothing was written and thus force
   * the system to append a new fixed-size buffer behind them.
   */

  Queue* q = (Queue*) queue;
  QNode* n;

#if GT81
  Tcl_MutexLock (&q->lock);
#endif

  buf = Buf_CreateRange (buf, Buf_Size (buf));

  n = (QNode*) Tcl_Alloc (sizeof (QNode));
  n->nextPtr = (QNode*) NULL;
  n->buf     = buf;

  if (q->lastNode == (QNode*) NULL) {
    q->firstNode = n;
  } else {
    q->lastNode->nextPtr = n;
  }

  q->lastNode = n;

  q->size += Buf_Size (buf);

#if GT81
  Tcl_MutexUnlock (&q->lock);
#endif
  return;
}

/*
 *------------------------------------------------------*
 *
 *	BufQueue_Size --
 *
 *	Returns the current number of bytes stored in the queue.
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

int
Buf_QueueSize (queue)
     Buf_BufferQueue queue;
{
  Queue* q = (Queue*) queue;
  int size;

#if GT81
  Tcl_MutexLock (&q->lock);
#endif

  size = q->size;

#if GT81
  Tcl_MutexUnlock (&q->lock);
#endif
  return size;
}
