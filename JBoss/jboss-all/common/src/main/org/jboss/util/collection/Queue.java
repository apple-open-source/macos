/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.collection;

import java.util.Collection;

/**
 * An iterface used to implement a first-in, first-out container.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public interface Queue
   extends Collection
{
   /** Unlimited maximum queue size identifier. */
   int UNLIMITED_MAXIMUM_SIZE = -1;

   /**
    * Get the maximum size of the queue.
    *
    * @return  Maximum pool size or {@link #UNLIMITED_MAXIMUM_SIZE}.
    */
   int getMaximumSize();

   /**
    * Set the maximum size of the queue.
    *
    * @param size    New maximim pool size or {@link #UNLIMITED_MAXIMUM_SIZE}.
    *
    * @exception IllegalArgumentException    Illegal size.
    */
   void setMaximumSize(int size) throws IllegalArgumentException;

   /**
    * Check if the queue is full.
    *
    * @return  True if the queue is full.
    */
   boolean isFull();

   /**
    * Check if the queue is empty.
    *
    * @return True if the queue is empty.
    */
   boolean isEmpty();

   /**
    * Enqueue an object onto the queue.
    *
    * @param obj     Object to enqueue.
    * @return        True if collection was modified.
    *
    * @exception FullCollectionException     The queue is full.
    */
   boolean add(Object obj) throws FullCollectionException;

   /**
    * Dequeue an object from the queue.
    *
    * @return     Dequeued object.
    *
    * @exception EmptyCollectionException    The queue is empty.
    */
   Object remove() throws EmptyCollectionException;

   /**
    * Get the object at the front of the queue.
    *
    * @return  Object at the front of the queue.
    *
    * @exception EmptyCollectionException    The queue is empty.
    */
   Object getFront() throws EmptyCollectionException;

   /**
    * Get the object at the back of the queue.
    *
    * @return  Object at the back of the queue.
    *
    * @exception EmptyCollectionException    The queue is empty.
    */
   Object getBack() throws EmptyCollectionException;
}
