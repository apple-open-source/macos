/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.management.mejb;

import javax.management.Notification;
import javax.management.NotificationListener;

/**
 * MBean Interface of a Notification Listener MBean
 *
 * @author <A href="mailto:andreas@jboss.org">Andreas &quot;Mad&quot; Schaefer</A>
 * @version $Revision: 1.3.2.1 $
 **/
public interface ListenerMBean
      extends NotificationListener
{
   /**
    * Handles the given notifcation event and passed it to the registered
    * listener
    *
    * @param pNotification    NotificationEvent
    * @param pHandback        Handback object
    *
    * @throws RemoteException    If a Remote Exception occurred
    */
   void handleNotification(Notification pNotification, Object pHandback);
}
