// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: BlockingQueue.java,v 1.15.2.4 2003/06/04 04:47:58 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.util;

/* ------------------------------------------------------------ */
/** Blocking queue.
 *
 * XXX temp implementation. Should use java2 containers.
 * Implemented as circular buffer in a Vector. Synchronization is on the
 * vector to avoid double synchronization.
 *
 * @version $Id: BlockingQueue.java,v 1.15.2.4 2003/06/04 04:47:58 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public class BlockingQueue
{
    Object[] elements;
    Object lock;
    int maxSize;
    int size=0;
    int head=0;
    int tail=0;

    /* ------------------------------------------------------------ */
    /** Constructor. 
     */
    public BlockingQueue(int maxSize)
    {
        this(null,maxSize);
    }

    /* ------------------------------------------------------------ */
    /** Constructor. 
     */
    public BlockingQueue(Object lock, int maxSize)
    {
        this.maxSize=maxSize;
        if (maxSize==0)
            this.maxSize=255;
        elements = new Object[this.maxSize];
        this.lock=lock==null?elements:lock;
    }
    
    /* ------------------------------------------------------------ */
    public  void clear()
    {
        synchronized(lock)
        {
            size=0;
            head=0;
            tail=0;
        }
    }
    
    /* ------------------------------------------------------------ */
    public int size()
    {
        return size;
    }
    
    /* ------------------------------------------------------------ */
    public int maxSize()
    {
        return maxSize;
    }
    
  
    /* ------------------------------------------------------------ */
    /** Put object in queue.
     * @param o Object
     */
    public void put(Object o)
        throws InterruptedException
    {
        synchronized(lock)
        {
            while (size==maxSize)
                lock.wait();

            elements[tail]=o;
            if(++tail==maxSize)
                tail=0;
            size++;
            lock.notify();
        }
    }
    
    /* ------------------------------------------------------------ */
    /** Put object in queue.
     * @param timeout If timeout expires, throw InterruptedException
     * @param o Object
     * @exception InterruptedException Timeout expired or otherwise interrupted
     */
    public void put(Object o, int timeout)
        throws InterruptedException
    {
        synchronized(lock)
        {
            if (size==maxSize)
            {
                lock.wait(timeout);
                if (size==maxSize)
                    throw new InterruptedException("Timed out");
            }
            
            elements[tail]=o;
            if(++tail==maxSize)
                tail=0;
            size++;
            lock.notify();
        }
    }

    /* ------------------------------------------------------------ */
    /** Get object from queue.
     * Block if there are no objects to get.
     * @return The next object in the queue.
     */
    public Object get()
        throws InterruptedException
    {
        synchronized(lock)
        {
            while (size==0)
                lock.wait();
            
            Object o = elements[head];
            elements[head]=null;
            if(++head==maxSize)
                head=0;
            if (size==maxSize)
                lock.notifyAll();
            size--;
            return o;
        }
    }
    
        
    /* ------------------------------------------------------------ */
    /** Get from queue.
     * Block for timeout if there are no objects to get.
     * @param timeoutMs the time to wait for a job
     * @return The next object in the queue, or null if timedout.
     */
    public Object get(int timeoutMs)
        throws InterruptedException
    {
        synchronized(lock)
        {
            if (size==0 && timeoutMs!=0)
                lock.wait((long)timeoutMs);
            
            if (size==0)
                return null;
            
            Object o = elements[head];
            elements[head]=null;
            if(++head==maxSize)
                head=0;

            if (size==maxSize)
                lock.notifyAll();
            size--;
            
            return o;
        }
    }
    
    /* ------------------------------------------------------------ */
    /** Peek at the  queue.
     * Block  if there are no objects to peek.
     * @return The next object in the queue, or null if timedout.
     */
    public Object peek()
        throws InterruptedException
    {
        synchronized(lock)
        {
            if (size==0)
                lock.wait();
            
            if (size==0)
                return null;
            
            Object o = elements[head];
            return o;
        }
    }
    
    /* ------------------------------------------------------------ */
    /** Peek at the  queue.
     * Block for timeout if there are no objects to peek.
     * @param timeoutMs the time to wait for a job
     * @return The next object in the queue, or null if timedout.
     */
    public Object peek(int timeoutMs)
        throws InterruptedException
    {
        synchronized(lock)
        {
            if (size==0)
                lock.wait((long)timeoutMs);
            
            if (size==0)
                return null;
            
            Object o = elements[head];
            return o;
        }
    }
}








