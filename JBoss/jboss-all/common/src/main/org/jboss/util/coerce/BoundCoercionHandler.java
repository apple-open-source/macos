/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.coerce;

/**
 * A bound CoersionHandler, which is bound to a specific class type.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public abstract class BoundCoercionHandler
   extends CoercionHandler
{
   /**
    * Get the target class type for this CoercionHandler.
    *
    * @return     Class type
    */
   public abstract Class getType();
}
