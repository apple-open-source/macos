/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.property;

/**
 * An abstract adapter class for receiving bound property events.
 *
 * <p>Methods defined in this class are empty.  This class exists as
 *    as convenience for creating listener objects.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public abstract class BoundPropertyAdapter
   extends PropertyAdapter
   implements BoundPropertyListener
{
   /**
    * Notifies that this listener was bound to a property.
    *
    * @param map     PropertyMap which contains property bound to.
    */
   public void propertyBound(final PropertyMap map) {}

   /**
    * Notifies that this listener was unbound from a property.
    *
    * @param map     PropertyMap which contains property bound to.
    */
   public void propertyUnbound(final PropertyMap map) {}
}
