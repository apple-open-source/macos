/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.management;

import org.jboss.mx.server.ServerConstants;
import org.jboss.mx.util.AgentID;

/**
 * Mandatory MBean server delegate MBean implementation.
 *
 * @see javax.management.MBeanServerDelegateMBean
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.2 $
 *   
 */
public class MBeanServerDelegate
   implements MBeanServerDelegateMBean, NotificationBroadcaster
{

   // Attributes ----------------------------------------------------
   private MBeanNotificationInfo notificationInfo  = null;
   private NotificationBroadcasterSupport notifier = null;
   private String agentID = AgentID.create();
   
   // Constructors --------------------------------------------------
   public MBeanServerDelegate()
   {
      this.notificationInfo = new MBeanNotificationInfo(
                                 new String[] {
                                    MBeanServerNotification.REGISTRATION_NOTIFICATION,
                                    MBeanServerNotification.UNREGISTRATION_NOTIFICATION
                                 },
                                 MBeanServerNotification.class.getName(),
                                 "Describes the MBean registration and unregistration events in a MBean Server."
                              );
      this.notifier = new NotificationBroadcasterSupport();
   }

   // MBeanServerDelegateMBean implementation -----------------------
   public String getMBeanServerId()
   {
      return agentID;
   }

   public String getSpecificationName()
   {
      return ServerConstants.SPECIFICATION_NAME;
   }

   public String getSpecificationVersion()
   {
      return ServerConstants.SPECIFICATION_VERSION;
   }

   public String getSpecificationVendor()
   {
      return ServerConstants.SPECIFICATION_VENDOR;
   }

   public String getImplementationName()
   {
      return ServerConstants.IMPLEMENTATION_NAME;
   }

   public String getImplementationVersion()
   {
      return ServerConstants.IMPLEMENTATION_VERSION;
   }

   public String getImplementationVendor()
   {
      return ServerConstants.IMPLEMENTATION_VENDOR;
   }

   public void addNotificationListener(NotificationListener listener,
         NotificationFilter filter, Object handback) throws IllegalArgumentException
   {
      notifier.addNotificationListener(listener, filter, handback);
   }

   public void removeNotificationListener(NotificationListener listener) throws ListenerNotFoundException
   {
      notifier.removeNotificationListener(listener);
   }

   public MBeanNotificationInfo[] getNotificationInfo()
   {
      return new MBeanNotificationInfo[] { notificationInfo };
   }

   public void sendNotification(Notification notification)
   {
      notifier.sendNotification(notification);
   }

}

