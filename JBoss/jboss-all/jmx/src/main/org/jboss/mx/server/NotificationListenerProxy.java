/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.server;

import javax.management.Notification;
import javax.management.NotificationListener;
import javax.management.ObjectName;

/**
 * A notification listener used to forward notifications to listeners
 * added through the mbean server.<p>
 *
 * The original source is replaced with the object name.
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.1.6.1 $
 */
public class NotificationListenerProxy
   implements NotificationListener
{
   // Constants ---------------------------------------------------

   // Attributes --------------------------------------------------

   /**
    * The original listener
    */
   private NotificationListener listener;

   /**
    * The object name we are proxying
    */
   private ObjectName name;

   // Static ------------------------------------------------------

   // Constructors ------------------------------------------------

   /**
    * Create a new Notification Listener Proxy
    * 
    * @param name the object name
    * @param listener the original listener
    */
   public NotificationListenerProxy(ObjectName name, 
                                    NotificationListener listener)
   {
      this.name = name;
      this.listener = listener;
   }

   // Public ------------------------------------------------------

   // implementation NotificationListener -------------------------

   public void handleNotification(Notification notification, Object handback)
   {
      if (notification == null)
         return;

      // Forward the notification with the object name as source
      // FIXME: This overwrites the original source, there is no way
      //        to put it back with the current spec
      notification.setSource(name);
      listener.handleNotification(notification, handback);
   }

   // overrides ---------------------------------------------------

   // Protected ---------------------------------------------------

   // Private -----------------------------------------------------

   // Inner classes -----------------------------------------------
}
