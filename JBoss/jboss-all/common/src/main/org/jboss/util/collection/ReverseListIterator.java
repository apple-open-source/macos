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
import java.util.List;
import java.util.NoSuchElementException;

/**
 * An iterator that returns elements in reverse order from a list.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class ReverseListIterator
   implements Iterator
{
   /** The list to get elements from */
   protected final List list;
   
   /** The current index of the list */
   protected int current;

   /**
    * Construct a ReverseListIterator for the given list.
    *
    * @param list    List to iterate over.
    */
   public ReverseListIterator(final List list) {
      this.list = list;
      current = list.size() - 1;
   }

   /**
    * Check if there are more elements.
    *
    * @return  True if there are more elements.
    */
   public boolean hasNext() {
      return current > 0;
   }

   /**
    * Get the next element.
    *
    * @return  The next element.
    *
    * @throws NoSuchElementException
    */
   public Object next() {
      if (current <= 0) {
         throw new NoSuchElementException();
      }
      
      return list.get(current--);
   }

   /**
    * Remove the current element.
    */
   public void remove() {
      list.remove(current);
   }
}
