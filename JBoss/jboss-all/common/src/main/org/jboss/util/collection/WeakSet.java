/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.collection;

import java.lang.ref.ReferenceQueue;

import java.util.Set;
import java.util.HashSet;
import java.util.AbstractSet;
import java.util.Iterator;
import java.util.NoSuchElementException;

import org.jboss.util.NullArgumentException;
import org.jboss.util.WeakObject;

/**
 * A <tt>Set</tt> implementation with <em>weak elements</em>.  An entry in
 * a <tt>WeakSet</tt> will automatically be removed when the element is no
 * longer in ordinary use.  More precisely, the presence of an given element
 * will not prevent the element from being discarded by the garbage collector,
 * that is, made finalizable, finalized, and then reclaimed.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class WeakSet
   extends AbstractSet
   implements Set
{
   /** The reference queue used to get object removal notifications. */
   protected final ReferenceQueue queue = new ReferenceQueue();

   /** The <tt>Set</tt> which will be used for element storage. */
   protected final Set set;

   /**
    * Construct a <tt>WeakSet</tt>.  Any elements in the given set will be
    * wrapped in {@link WeakObject} references.
    *
    * @param set  The <tt>Set</tt> which will be used for element storage.
    *
    * @throws NullArgumentException    Set is <tt>null</tt>.
    */
   public WeakSet(final Set set) {
      if (set == null)
         throw new NullArgumentException("set");

      // reset any elements to weak objects
      if (set.size() != 0) {
         Object elements[] = set.toArray();
         set.clear();

         for (int i=0; i<elements.length; i++) {
            add(elements[i]);
         }
      }

      this.set = set;
   }

   /**
    * Construct a <tt>WeakSet</tt> based on a <tt>HashSet</tt>.
    */
   public WeakSet() {
      this(new HashSet());
   }

   /**
    * Maintain the elements in the set.  Removes objects from the set that
    * have been reclaimed due to GC.
    */
   protected final void maintain() {
      WeakObject weak;
      while ((weak = (WeakObject)queue.poll()) != null) {
         set.remove(weak);
      }
   }

   /**
    * Return the size of the set.
    *
    * @return  The size of the set.
    */
   public int size() {
      maintain();

      return set.size();
   }

   /**
    * Return an iteration over the elements in the set.
    *
    * @return  An iteration over the elements in the set.
    */
   public Iterator iterator() {
      return new Iterator() {

            /** The set's iterator */
            Iterator iter = set.iterator();

            /** The next available object. */
            Object next = null;

            public boolean hasNext() {
               while (iter.hasNext()) {
                  WeakObject weak = (WeakObject)iter.next();
                  Object obj = null;
                  if (weak != null && (obj = weak.get()) == null) {
                     // object has been reclaimed by the GC
                     continue;
                  }

                  next = obj;
                  return true;
               }

               return false;
            }

            public Object next() {
               if ((next == null) && !hasNext()) {
                  throw new NoSuchElementException();
               }

               Object obj = next;
               next = null;

               return obj;
            }

            public void remove() {
               iter.remove();
            }
         };
   }

   /**
    * Add an element to the set.
    *
    * @param obj  Element to add to the set.
    * @return     True if the element was added.
    */
   public boolean add(final Object obj) {
      maintain();

      return set.add(WeakObject.create(obj, queue));
   }

   /**
    * Returns <tt>true</tt> if this set contains no elements.
    *
    * @return  <tt>true</tt> if this set contains no elements.
    */
   public boolean isEmpty() {
      maintain();

      return set.isEmpty();
   }

   /**
    * Returns <tt>true</tt> if this set contains the specified element.
    *
    * @param obj  Element whose presence in this set is to be tested.
    * @return     <tt>true</tt> if this set contains the specified element.
    */
   public boolean contains(final Object obj) {
      maintain();

      return set.contains(WeakObject.create(obj));
   }

   /**
    * Removes the given element from this set if it is present.
    *
    * @param obj  Object to be removed from this set, if present.
    * @return     <tt>true</tt> if the set contained the specified element.
    */
   public boolean remove(final Object obj) {
      maintain();

      return set.remove(WeakObject.create(obj));
   }

   /**
    * Removes all of the elements from this set.
    */
   public void clear() {
      set.clear();
   }

   /**
     * Returns a shallow copy of this <tt>WeakSet</tt> instance: the elements
     * themselves are not cloned.
     *
     * @return    A shallow copy of this set.
     */
   public Object clone() {
      maintain();

      try { 
         return super.clone();
      }
      catch (CloneNotSupportedException e) { 
         throw new InternalError();
      }
   }
}
