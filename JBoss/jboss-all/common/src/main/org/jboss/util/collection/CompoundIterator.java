/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.collection;

import java.util.Iterator;
import java.util.NoSuchElementException;

/**
 * A compound iterator, which iterates over all of the elements in the
 * given iterators.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class CompoundIterator
   implements Iterator
{
   /** The array of iterators to iterate over. */
   protected final Iterator iters[];

   /** The index of the current iterator. */
   protected int index;

   /**
    * Construct a CompoundIterator over the given array of iterators.
    *
    * @param iters   Array of iterators to iterate over.
    *
    * @throws IllegalArgumentException    Array is <kk>null</kk> or empty.
    */
   public CompoundIterator(final Iterator iters[]) {
      if (iters == null || iters.length == 0)
         throw new IllegalArgumentException("array is null or empty");
     
      this.iters = iters;
   }

   /**
    * Check if there are more elements.
    *
    * @return  True if there are more elements.
    */
   public boolean hasNext() {
      for (; index < iters.length; index++) {
         if (iters[index] != null && iters[index].hasNext()) {
            return true;
         }
      }

      return false;
   }

   /**
    * Return the next element from the current iterator.
    *
    * @return  The next element from the current iterator.
    *
    * @throws NoSuchElementException   There are no more elements.
    */
   public Object next() {
      if (!hasNext()) {
         throw new NoSuchElementException();
      }

      return iters[index].next();
   }

   /**
    * Remove the current element from the current iterator.
    *
    * @throws IllegalStateException
    * @throws UnsupportedOperationException
    */
   public void remove() {
      iters[index].remove();
   }
}
