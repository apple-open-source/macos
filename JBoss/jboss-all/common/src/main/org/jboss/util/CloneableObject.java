/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util;

/**
 * A simple base-class for classes which need to be cloneable.
 *
 * @version <tt>$Revision: 1.2 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class CloneableObject
   implements java.lang.Cloneable
{
   /**
    * Clone the object via {@link Object#clone}.  This will return
    * and object of the correct type, with all fields shallowly
    * cloned.
    */
   public Object clone()
   {
      try {
         return super.clone();
      }
      catch (CloneNotSupportedException e) {
         throw new InternalError();
      }
   }

   /**
    * An interface which exposes a <em>public</em> clone method, 
    * unlike {@link Object#clone} which is protected and throws
    * exceptions... how useless is that?
    */
   public static interface Cloneable
      extends java.lang.Cloneable
   {
      Object clone();
   }
}
