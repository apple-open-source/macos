/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util;

import java.lang.ref.WeakReference;
import java.lang.ref.ReferenceQueue;

/**
 * Convenience class to wrap an <tt>Object</tt> into a <tt>WeakReference</tt>.
 *
 * <p>Modified from <tt>java.util.WeakHashMap.WeakKey</tt>.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public final class WeakObject
   extends WeakReference
{
   /** The hash code of the nested object */
   protected final int hashCode;
   
   /**
    * Construct a <tt>WeakObject</tt>.
    *
    * @param obj  Object to reference.
    */
   public WeakObject(final Object obj) {
      super(obj);
      hashCode = obj.hashCode();
   }
   
   /**
    * Construct a <tt>WeakObject</tt>.
    *
    * @param obj     Object to reference.
    * @param queue   Reference queue.
    */
   public WeakObject(final Object obj, final ReferenceQueue queue) {
      super(obj, queue);
      hashCode = obj.hashCode();
   }
   
   /**
    * Check the equality of an object with this.
    *
    * @param obj  Object to test equality with.
    * @return     True if object is equal.
    */
   public boolean equals(final Object obj) {
      if (obj == this) return true;

      if (obj != null && obj.getClass() == getClass()) {
         WeakObject soft = (WeakObject)obj;
         
         Object a = this.get();
         Object b = soft.get();
         if (a == null || b == null) return false;
         if (a == b) return true;

         return a.equals(b);
      }

      return false;
   }
   
   /**
    * Return the hash code of the nested object.
    *
    * @return  The hash code of the nested object.
    */
   public int hashCode() {
      return hashCode;
   }


   /////////////////////////////////////////////////////////////////////////
   //                            Factory Methods                          //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Create a <tt>WeakObject</tt> for the given object.
    *
    * @param obj     Object to reference.
    * @return        <tt>WeakObject</tt> or <tt>null</tt> if object is null.
    */
   public static WeakObject create(final Object obj) {
      if (obj == null) return null;
      else return new WeakObject(obj);
   }
   
   /**
    * Create a <tt>WeakObject</tt> for the given object.
    *
    * @param obj     Object to reference.
    * @param queue   Reference queue.
    * @return        <tt>WeakObject</tt> or <tt>null</tt> if object is null.
    */
   public static WeakObject create(final Object obj,
                                   final ReferenceQueue queue)
   {
      if (obj == null) return null;
      else return new WeakObject(obj, queue);
   }
}
