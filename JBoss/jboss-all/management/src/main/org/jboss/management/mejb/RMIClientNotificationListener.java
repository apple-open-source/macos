/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.management.mejb;

import java.rmi.Remote;
import java.rmi.RemoteException;
import java.rmi.server.UnicastRemoteObject;

import javax.management.JMException;
import javax.management.Notification;
import javax.management.NotificationFilter;
import javax.management.NotificationListener;
import javax.management.ObjectName;

/**
 * Client-side RMI Listener to receive the message and send to the
 * clients listener. Its stub is used on the server-side to hand
 * the Notifications over to this class.
 *
 * @author <A href="mailto:andreas@jboss.org">Andreas &quot;Mad&quot; Schaefer</A>
 **/
public class RMIClientNotificationListener
      extends ClientNotificationListener
      implements RMIClientNotificationListenerInterface
{

   public RMIClientNotificationListener(
      ObjectName pSender,
      NotificationListener pClientListener,
      Object pHandback,
      NotificationFilter pFilter,
      MEJB pConnector)
      throws RemoteException,
      JMException
   {
      super(pSender, pClientListener, pHandback);
      // Export the RMI object to become a callback object
      Remote lStub = UnicastRemoteObject.exportObject(this);
      // Register the listener as MBean on the remote JMX server
      createListener(
            pConnector,
            "org.jboss.management.mejb.RMINotificationListener",
            new Object[]{lStub},
            new String[]{RMIClientNotificationListenerInterface.class.getName()}
      );
      addNotificationListener(pConnector, pFilter);
   }

   /**
    * Handles the given notification by sending this to the remote
    * client listener
    *
    * @param pNotification            Notification to be send
    * @param pHandback               Handback object
    **/
   public void handleNotification(Notification pNotification,
      Object pHandback)
      throws RemoteException
   {
      try
      {
         mClientListener.handleNotification(
               pNotification,
               mHandback
         );
      }
      catch (RuntimeException re)
      {
         throw new RemoteException("Exceptions returned by the client listener", re);
      }
      catch (Error e)
      {
         throw new RemoteException("Error returned by the client listener", e);
      }
   }
}
