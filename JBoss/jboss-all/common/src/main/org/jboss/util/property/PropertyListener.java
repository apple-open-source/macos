/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.property;

import java.util.EventListener;

/**
 * The listener interface for receiving property events.
 *
 * <p>Classes that are interested in processing a property event implement
 *    this interface, and register instance objects with a given PropertyMap
 *    or the PropertyManager via <code>addPropertyListener()</code>.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public interface PropertyListener
   extends EventListener
{
   /**
    * Notifies that a property was added
    *
    * @param event   Property event
    */
   void propertyAdded(PropertyEvent event);

   /**
    * Notifies that a property was removed
    *
    * @param event   Property event
    */
   void propertyRemoved(PropertyEvent event);

   /**
    * Notifies that a property has changed
    *
    * @param event   Property event
    */
   void propertyChanged(PropertyEvent event);
}
