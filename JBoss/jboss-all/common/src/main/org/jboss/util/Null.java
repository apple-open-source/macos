/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util;

import java.io.Serializable;

/**
 * A class that represents <tt>null</tt>.
 *
 * <p>{@link Null#VALUE} is used to given an object variable a dual-mode
 *    nullified value, where <tt>null</tt> would indicate that the value is 
 *    empty, and {@link Null#VALUE} would idicate that the value has been 
 *    set to <tt>null</tt> (or something to that effect).
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public final class Null
   implements Serializable
{
   /** The primary instance of Null. */
   public static final Null VALUE = new Null();

   /** Do not allow public construction. */
   private Null() {}

   /**
    * Return a string representation.
    *
    * @return  Null
    */
   public String toString() {
      return null;
   }

   /**
    * Returns zero.
    *
    * @return  Zero.
    */
   public int hashCode() {
      return 0;
   }

   /**
    * Check if the given object is a Null instance or <tt>null</tt>.
    *
    * @param obj  Object to test.
    * @return     True if the given object is a Null instance or <tt>null</tt>.
    */
   public boolean equals(final Object obj) {
      if (obj == this) return true;
      return (obj == null || obj.getClass() == getClass());
   }
}

