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
 * Mutable object interface.
 *
 * @version <tt>$Revision: 1.2 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public interface Mutable
{
   /**
    * Set the value of a mutable object.
    *
    * @param value   Target value for object.
    */
   void setValue(Object value);

   /**
    * Get the value of a mutable object.
    *
    * @return Object value.
    */
   Object getValue();
}
