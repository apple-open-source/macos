/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.management.mejb;

import java.rmi.RemoteException;
import java.util.Iterator;
import java.util.List;

import javax.management.JMException;
import javax.management.Notification;
import javax.management.NotificationFilter;
import javax.management.NotificationListener;
import javax.management.ObjectName;

/**
 * Local Polling Listener to receive the message and send to the listener
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.4.2.1 $
 **/
public class PollingClientNotificationListener
      extends ClientNotificationListener
      implements Runnable
{

   private MEJB mConnector;
   private int mSleepingPeriod = 2000;

   public PollingClientNotificationListener(
         ObjectName pSender,
         NotificationListener pClientListener,
         Object pHandback,
         NotificationFilter pFilter,
         int pSleepingPeriod,
         int pMaximumListSize,
         MEJB pConnector
         ) throws
         JMException,
         RemoteException
   {
      super(pSender, pClientListener, pHandback);
      if (pSleepingPeriod > 0)
      {
         mSleepingPeriod = pSleepingPeriod;
      }
      mConnector = pConnector;
      // Register the listener as MBean on the remote JMX server
      createListener(
            pConnector,
            "org.jboss.management.mejb.PollingNotificationListener",
            new Object[]{new Integer(pMaximumListSize), new Integer(pMaximumListSize)},
            new String[]{Integer.TYPE.getName(), Integer.TYPE.getName()}
      );
      addNotificationListener(pConnector, pFilter);
      new Thread(this).start();
   }

   public void run()
   {
      while (true)
      {
         try
         {
            try
            {
               List lNotifications = (List) mConnector.invoke(
                     getRemoteListenerName(),
                     "getNotifications",
                     new Object[]{},
                     new String[]{}
               );
               Iterator i = lNotifications.iterator();
               while (i.hasNext())
               {
                  Notification lNotification = (Notification) i.next();
                  mClientListener.handleNotification(
                        lNotification,
                        mHandback
                  );
               }
            }
            catch (Exception e)
            {
               log.error("PollingClientNotificationListener.getNotifications() failed", e);
            }
            Thread.sleep(mSleepingPeriod);
         }
         catch (InterruptedException e)
         {
         }
      }
   }
}
