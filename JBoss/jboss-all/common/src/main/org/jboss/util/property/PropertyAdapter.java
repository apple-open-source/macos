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
 * An abstract adapter class for receving property events.
 *
 * <p>Methods defined in this class are empty.  This class exists as
 *    as convenience for creating listener objects.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public abstract class PropertyAdapter
   implements PropertyListener
{
   /**
    * Notifies that a property was added.
    *
    * @param event   Property event.
    */
   public void propertyAdded(final PropertyEvent event) {}

   /**
    * Notifies that a property was removed.
    *
    * @param event   Property event.
    */
   public void propertyRemoved(final PropertyEvent event) {}

   /**
    * Notifies that a property has changed.
    *
    * @param event   Property event.
    */
   public void propertyChanged(final PropertyEvent event) {}
}
