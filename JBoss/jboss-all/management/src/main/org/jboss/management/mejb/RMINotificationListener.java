/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.management.mejb;

import java.rmi.RemoteException;
import javax.management.Notification;

import org.jboss.logging.Logger;

/**
 * Notification Listener Implementation registered as
 * MBean on the remote JMX Server and the added as
 * Notification Listener on the remote JMX Server.
 * Each notification received will be transfered to
 * the remote client using RMI Callback Objects.
 *
 * @jmx:mbean extends="org.jboss.management.mejb.ListenerMBean"
 *
 * @author <A href="mailto:andreas@jboss.org">Andreas Schaefer</A>
 * @version $Revision: 1.4.2.1 $
 **/
public class RMINotificationListener
      implements RMINotificationListenerMBean
{
   private static final Logger log = Logger.getLogger(RMINotificationListener.class);

   // -------------------------------------------------------------------------
   // Members
   // -------------------------------------------------------------------------

   private RMIClientNotificationListenerInterface mClientListener;

   // -------------------------------------------------------------------------
   // Constructor
   // -------------------------------------------------------------------------

   /**
    * Creates the RMI Notification Listener MBean implemenation which
    * will be registered at the remote JMX Server as notificatin listener
    * and then send the notification over the provided RMI Notification
    * sender to the client
    *
    * @param pClientListener RMI-Stub used to transfer the Notification over
    *                        the wire.
    **/
   public RMINotificationListener(RMIClientNotificationListenerInterface pClientListener)
   {
      log.debug("RMINotificationListener(), client listener: " + pClientListener);
      mClientListener = pClientListener;
   }

   // -------------------------------------------------------------------------
   // Public Methods
   // -------------------------------------------------------------------------

   /**
    * Handles the given notifcation event and passed it to the registered
    * RMI Notification Sender
    *
    * @param pNotification				NotificationEvent
    * @param pHandback					Handback object
    */
   public void handleNotification(
         Notification pNotification,
         Object pHandback
         )
   {
      try
      {
         log.debug(
               "RMINotificationListener.handleNotification() " +
               ", notification: " + pNotification +
               ", handback: " + pHandback +
               ", client listener: " + mClientListener
         );
         mClientListener.handleNotification(pNotification, pHandback);
      }
      catch (RemoteException e)
      {
         throw new org.jboss.util.NestedRuntimeException(e);
      }
   }

   /**
    * Test if this and the given Object are equal. This is true if the given
    * object both refer to the same local listener
    *
    * @param pTest						Other object to test if equal
    *
    * @return							True if both are of same type and
    *									refer to the same local listener
    **/
   public boolean equals(Object pTest)
   {
      if (pTest instanceof RMINotificationListener)
      {
         return mClientListener.equals(
               ((RMINotificationListener) pTest).mClientListener
         );
      }
      return false;
   }

   /**
    * @return							Hashcode of the remote listener
    **/
   public int hashCode()
   {
      return mClientListener.hashCode();
   }
}
