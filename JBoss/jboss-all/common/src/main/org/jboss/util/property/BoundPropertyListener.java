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
 * The listener interface for receiving bound property events (as well as
 * property events).
 *
 * <p>Classes that are interested in processing a bound property event 
 *    implement this interface, and register instance objects with a given
 *    {@link PropertyMap} or via
 *    {@link PropertyManager#addPropertyListener(PropertyListener)}.
 *
 * <p>Note that this is not the typical listener interface, as it extends
 *    from {@link PropertyListener}, and defines {@link #getPropertyName()}
 *    which is not an event triggered method.  This method serves to instruct
 *    the {@link PropertyMap} the listener is registered with, which property
 *    it will bind to.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public interface BoundPropertyListener
   extends PropertyListener
{
   /**
    * Get the property name which this listener is bound to.
    *
    * @return  Property name.
    */
   String getPropertyName();

   /**
    * Notifies that this listener was bound to a property.
    *
    * @param map     <tt>PropertyMap</tt> which contains property bound to.
    */
   void propertyBound(PropertyMap map);

   /**
    * Notifies that this listener was unbound from a property.
    *
    * @param map     <tt>PropertyMap</tt> which contains property bound to.
    */
   void propertyUnbound(PropertyMap map);
}
