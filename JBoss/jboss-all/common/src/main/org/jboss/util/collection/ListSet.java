/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.collection;

import java.util.List;
import java.util.ArrayList;
import java.util.Set;
import java.util.AbstractSet;
import java.util.Iterator;
import java.util.Collection;

import org.jboss.util.NullArgumentException;

/**
 * A thin wrapper around a <code>List</code> transforming it into a
 * modifiable <code>Set</code>.
 *
 * @version <tt>$Revision: 1.2 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class ListSet
   extends AbstractSet
   implements Set, Cloneable
{
   /** The <tt>List</tt> which will be used for element storage. */
   protected final List list;

   /**
    * Construct a <tt>ListSet</tt>.
    *
    * @param list    The <tt>List</tt> which will be used for element storage.
    *
    * @throws IllegalArgumentException    List is <tt>null</tt> or contains
    *                                     duplicate entries.
    */
   public ListSet(final List list) {
      if (list == null)
         throw new NullArgumentException("list");

      // make sure there are no duplicates
      int size = list.size();
      for (int i=0; i<size; i++) {
         Object obj = list.get(i);
         if (list.indexOf(obj) != list.lastIndexOf(obj)) {
            throw new IllegalArgumentException
               ("list contains duplicate entries");
         }
      }

      this.list = list;
   }

   /**
    * Construct a <tt>ListSet</tt> using an <tt>ArrayList</tt> for backing.
    */
   public ListSet() {
      this(new ArrayList());
   }

   /**
    * Construct a <tt>ListSet</tt> using an <tt>ArrayList</tt> for backing
    * and populated with the given elements.
    *
    * @param elements   The elements for the list.
    */
   public ListSet(final Collection elements) {
      this(new ArrayList(elements));
   }
   
   public List getList()
   {
      return list;
   }
   
   /**
    * Return the size of the set.
    *
    * @return  The size of the set.
    */
   public int size() {
      return list.size();
   }

   /**
    * Return an iteration over the elements in the set.
    *
    * @return  An iteration over the elements in the set.
    */
   public Iterator iterator() {
      return list.iterator();
   }

   /**
    * Add an element to the set.
    *
    * @param obj  Element to add to the set.
    * @return     True if the element was added.
    */
   public boolean add(final Object obj) {
      boolean added = false;

      if (!list.contains(obj)) {
         added = list.add(obj);
      }

      return added;
   }

   /**
    * Returns <tt>true</tt> if this set contains no elements.
    *
    * @return  <tt>true</tt> if this set contains no elements.
    */
   public boolean isEmpty() {
      return list.isEmpty();
   }

   /**
    * Returns <tt>true</tt> if this set contains the specified element.
    *
    * @param obj  Element whose presence in this set is to be tested.
    * @return     <tt>true</tt> if this set contains the specified element.
    */
   public boolean contains(final Object obj) {
      return list.contains(obj);
   }

   /**
    * Removes the given element from this set if it is present.
    *
    * @param obj  Object to be removed from this set, if present.
    * @return     <tt>true</tt> if the set contained the specified element.
    */
   public boolean remove(final Object obj) {
      return list.remove(obj);
   }

   /**
    * Removes all of the elements from this set.
    */
   public void clear() {
      list.clear();
   }

   /**
     * Returns a shallow copy of this <tt>ListSet</tt> instance.
     *
     * @return    A shallow copy of this set.
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
