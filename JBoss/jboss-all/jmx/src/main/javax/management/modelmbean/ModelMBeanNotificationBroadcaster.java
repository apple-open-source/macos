/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.modelmbean;

import javax.management.Attribute;
import javax.management.AttributeChangeNotification;
import javax.management.NotificationBroadcaster;
import javax.management.Notification;
import javax.management.NotificationListener;
import javax.management.MBeanException;
import javax.management.RuntimeOperationsException;
import javax.management.ListenerNotFoundException;

public interface ModelMBeanNotificationBroadcaster extends NotificationBroadcaster
{
   public void sendNotification(Notification ntfyObj) throws MBeanException, RuntimeOperationsException;

   public void sendNotification(String ntfyText) throws MBeanException, RuntimeOperationsException;

   public void sendAttributeChangeNotification(AttributeChangeNotification ntfyObj)
   throws MBeanException, RuntimeOperationsException;

   public void sendAttributeChangeNotification(Attribute inOldVal, Attribute inNewVal)
   throws MBeanException, RuntimeOperationsException;

   public void addAttributeChangeNotificationListener(NotificationListener inlistener,
         String inAttributeName, Object inhandback)
   throws MBeanException, RuntimeOperationsException, IllegalArgumentException;

   public void removeAttributeChangeNotificationListener(NotificationListener inlistener, String inAttributeName)
   throws MBeanException, RuntimeOperationsException, ListenerNotFoundException;

}

