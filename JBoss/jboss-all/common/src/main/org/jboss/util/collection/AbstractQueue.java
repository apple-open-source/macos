/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.collection;

import java.util.AbstractCollection;

/**
 * An abstract implementation of a Queue.  Sub-classes must provide methods
 * for <code>addLast(Object)</code> and <code>removeFirst()</code>.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public abstract class AbstractQueue
   extends AbstractCollection
   implements Queue
{
   /** Default maximum queue size */
   public static int DEFAULT_MAXIMUM_SIZE = UNLIMITED_MAXIMUM_SIZE;

   /** Maximum queue size */
   protected int maximumSize = DEFAULT_MAXIMUM_SIZE;

   /**
    * Initializes the AbstractQueue.
    */
   protected AbstractQueue() {}

   /**
    * Initializes the AbstractQueue.
    *
    * @param maxSize    Maximum queue size.
    *
    * @exception IllegalArgumentException    Illegal size.
    */
   protected AbstractQueue(final int maxSize) {
      setMaximumSize(maxSize);
   }

   /**
    * Get the maximum size of the queue.
    *
    * @return  Maximum queue size or {@link #UNLIMITED_MAXIMUM_SIZE}.
    */
   public int getMaximumSize() {
      return maximumSize;
   }

   /**
    * Set the maximum size of the queue
    *
    * @param size    New maximim queue size or {@link #UNLIMITED_MAXIMUM_SIZE}.
    *
    * @exception IllegalArgumentException    Illegal size.
    */
   public void setMaximumSize(final int size) {
      if (size < 0 && size != UNLIMITED_MAXIMUM_SIZE)
         throw new IllegalArgumentException("illegal size: " + size);

      maximumSize = size;
   }

   /**
    * Check if the queue is full.
    *
    * @return  True if the queue is full.
    */
   public boolean isFull() {
      if (maximumSize != UNLIMITED_MAXIMUM_SIZE && size() >= maximumSize)
         return true;

      return false;
   }

   /**
    * Check if the queue is empty.
    *
    * @return True if the queue is empty.
    */
   public boolean isEmpty() {
      if (size() <= 0)
         return true;

      return false;
   }

   /**
    * Append and object to the underling list.
    *
    * @param obj     Object to enqueue.
    * @return        True if collection was modified.
    *
    * @exception FullCollectionException     The queue is full.
    */
   public boolean add(Object obj) throws FullCollectionException {
      if (isFull())
         throw new FullCollectionException();

      return addLast(obj);
   }

   /**
    * Remove and return the first object in the queue.
    *
    * @return  Dequeued object.
    *
    * @exception EmptyCollectionException    The queue is empty.
    */
   public Object remove() throws EmptyCollectionException {
      if (isEmpty())
         throw new EmptyCollectionException();

      return removeFirst();
   }

   /**
    * Removes all of the elements from this queue
    */
   public void clear() {
      while (!isEmpty()) {
         remove();
      }
   }

   /**
    * Appends the given element to the end of the queue
    *
    * @param obj  Object to append
    * @return     Per Collection.add(), we return a boolean to indicate if
    *             the object modified the collection.
    */
   protected abstract boolean addLast(Object obj);

   /**
    * Remove the first object in the queue
    *
    * @return  First object in the queue
    */
   protected abstract Object removeFirst();
}
