/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.management.mejb;

import java.rmi.RemoteException;
import java.util.Random;

import javax.management.InstanceNotFoundException;
import javax.management.InstanceAlreadyExistsException;
import javax.management.JMException;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanRegistrationException;
import javax.management.MBeanException;
import javax.management.NotCompliantMBeanException;
import javax.management.NotificationFilter;
import javax.management.NotificationListener;
import javax.management.ObjectInstance;
import javax.management.ObjectName;
import javax.management.ReflectionException;

import org.jboss.logging.Logger;

/**
 * Basic Local Listener to receive Notification from a remote JMX Agent
 *
 * @author  ???
 * @version $Revision: 1.3.2.1 $
 **/
public abstract class ClientNotificationListener
{
   private ObjectName mSender;
   private ObjectName mRemoteListener;
   protected NotificationListener mClientListener;
   protected Object mHandback;
   private Random mRandom = new Random();

   protected Logger log = Logger.getLogger(this.getClass());

   public ClientNotificationListener(
         ObjectName pSender,
         NotificationListener pClientListener,
         Object pHandback
         )
   {
      mSender = pSender;
      mClientListener = pClientListener;
      mHandback = pHandback;
   }

   public ObjectName createListener(
         MEJB pConnector,
         String pClass,
         Object[] pParameters,
         String[] pSignatures
         ) throws
         MalformedObjectNameException,
         ReflectionException,
         MBeanRegistrationException,
         MBeanException,
         NotCompliantMBeanException,
         RemoteException
   {
      ObjectName lName = null;
      while (lName == null)
      {
         try
         {
            lName = new ObjectName("JMX:type=listener,id=" + mRandom.nextLong());
            ObjectInstance lInstance = pConnector.createMBean(
                  pClass,
                  lName,
                  pParameters,
                  pSignatures
            );
            lName = lInstance.getObjectName();
         }
         catch (InstanceAlreadyExistsException iaee)
         {
            lName = null;
         }
/* A remote exception could cause an endless loop therefore take it out
         catch( RemoteException re ) {
            lName = null;
         }
*/
      }
      mRemoteListener = lName;
      return lName;
   }

   public void addNotificationListener(
         MEJB pConnector,
         NotificationFilter pFilter
         ) throws
         InstanceNotFoundException,
         RemoteException
   {
      pConnector.addNotificationListener(
            mSender,
            mRemoteListener,
            pFilter,
            null
      );
   }

   public void removeNotificationListener(
         MEJB pConnector
         ) throws
         InstanceNotFoundException,
         RemoteException
   {
      try
      {
         pConnector.removeNotificationListener(
               mSender,
               mRemoteListener
         );
      }
      catch (JMException jme)
      {
      }
      try
      {
         pConnector.unregisterMBean(mRemoteListener);
      }
      catch (JMException jme)
      {
      }
   }

   public ObjectName getSenderMBean()
   {
      return mSender;
   }

   public ObjectName getRemoteListenerName()
   {
      return mRemoteListener;
   }

   public boolean equals(Object pTest)
   {
      if (pTest instanceof ClientNotificationListener)
      {
         ClientNotificationListener lListener = (ClientNotificationListener) pTest;
         return
               mSender.equals(lListener.mSender) &&
               mClientListener.equals(lListener.mClientListener);
      }
      return false;
   }

}
