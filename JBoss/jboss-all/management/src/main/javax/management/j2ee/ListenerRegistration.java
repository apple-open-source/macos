/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.j2ee;

import java.io.Serializable;
import java.rmi.RemoteException;

import javax.management.InstanceNotFoundException;
import javax.management.ListenerNotFoundException;
import javax.management.NotificationFilter;
import javax.management.NotificationListener;
import javax.management.ObjectName;

/**
 * Interface how a client can add its local listener on the
 * remote Management EJB.
 *
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.4.1 $
 */
public interface ListenerRegistration
   extends Serializable
{
   // -------------------------------------------------------------------------
   // Methods
   // -------------------------------------------------------------------------

   /**
    * Adds a new local (client-side) listener to the Management EJB (server-side)
    * to listen for Notifications. If the call is local (in the same JVM) then
    * it can optimize the call to local.
    *
    * @param name Object Name of the Managed Object we want to listen for notifications
    * @param listener Local (client-side) Notification Listener to finally receive the
    *                  notifications
    * @param filter Notification Filter to reduce the notifications to what the client
    *                expects
    * @param handback Handback object sent back to the client on every Notifications
    *                  delivered based on this registration
    */
   public void addNotificationListener(ObjectName name,
      NotificationListener listener,
      NotificationFilter filter,
      Object handback)
      throws InstanceNotFoundException,
         RemoteException;

   /**
    * Removes the notification listener from the Management EJB (server-side)
    * based on the given local (client-side) listener.
    *
    * @param name Object Name of the Managed Object the Listener was added to listen for
    * @param listener Local (client-side) Notification Listener used to add the
    *                  notification listener
    **/
   public void removeNotificationListener(ObjectName name,
      NotificationListener listener)
      throws InstanceNotFoundException,
         ListenerNotFoundException,
         RemoteException;
}
