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

import java.io.Serializable;

import org.jboss.util.NullArgumentException;

/**
 * An array iterator.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class ArrayIterator
   implements Iterator, Serializable, Cloneable
{
   /** Array to iterate over. */
   protected final Object[] array;

   /** The current position in the array. */
   protected int index;

   /**
    * Construct an ArrayIterator.
    *
    * @param array   The array to iterate over.
    */
   public ArrayIterator(final Object[] array) {
      if (array == null)
         throw new NullArgumentException("array");

      this.array = array;
   }

   /**
    * Returns true if there are more elements in the iteration.
    *
    * @return  True if there are more elements in the iteration.
    */
   public boolean hasNext() {
      return index < array.length;
   }

   /**
    * Returns the next element in the iteration.
    *
    * @return  The next element in the iteration.
    *
    * @throws NoSuchElementException   The are no more elements available.
    */
   public Object next() {
      if (! hasNext())
         throw new NoSuchElementException();

      return array[index++];
   }

   /**
    * Unsupported.
    *
    * @throws UnsupportedOperationException
    */
   public void remove() {
      throw new UnsupportedOperationException();
   }

   /**
    * Returns a shallow cloned copy of this object.
    *
    * @return  A shallow cloned copy of this object.
    */
   public Object clone() {
      try {
         return super.clone();
      }
      catch (CloneNotSupportedException e) {
         throw new InternalError();
      }
   }
}
