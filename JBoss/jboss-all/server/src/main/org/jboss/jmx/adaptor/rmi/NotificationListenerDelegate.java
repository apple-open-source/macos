package org.jboss.jmx.adaptor.rmi;

import javax.management.Notification;
import javax.management.NotificationListener;
import org.jboss.logging.Logger;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class NotificationListenerDelegate
   implements NotificationListener
{
   private static Logger log = Logger.getLogger(NotificationListenerDelegate.class);
   /** */
   private RMINotificationListener client;

   public NotificationListenerDelegate(RMINotificationListener client)
   {
      this.client = client;
   }

   public void handleNotification(Notification notification,
      Object handback)
   {
      try
      {
         if( log.isTraceEnabled() )
         {
            log.trace("Sending notification to client, event:"+notification);
         }
         client.handleNotification(notification, handback);
      }
      catch(Throwable t)
      {
         log.warn("Failed to notify client", t);
      }
   }
}
