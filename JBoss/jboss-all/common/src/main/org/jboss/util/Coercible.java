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
 * An iterface which an object implements to indicate that it will handle
 * coercion by itself.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public interface Coercible
{
   /**
    * Coerce this object into a specified type
    *
    * @param type    Type to coerce to
    * @return        Coereced object
    *
    * @exception CoercionException     Failed to coerce
    */
   Object coerce(Class type) throws CoercionException;
}
