/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.jmx.connector.notification;

import javax.management.Notification;
import javax.management.NotificationListener;

/**
 * MBean Interface of a Notification Listener MBean
 *
 * @version <tt>$Revision: 1.2 $</tt>
 * @author  <A href="mailto:andreas@jboss.org">Andreas &quot;Mad&quot; Schaefer</A>
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
    */
   public void handleNotification(Notification pNotification,
                                  Object pHandback);
}
