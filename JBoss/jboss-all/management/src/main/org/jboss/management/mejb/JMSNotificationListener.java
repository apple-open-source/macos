/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.management.mejb;

import javax.jms.JMSException;
import javax.jms.Message;
import javax.jms.Queue;
import javax.jms.QueueConnection;
import javax.jms.QueueConnectionFactory;
import javax.jms.QueueSender;
import javax.jms.QueueSession;
import javax.jms.Session;

import javax.management.Notification;

import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.rmi.PortableRemoteObject;

import org.jboss.logging.Logger;

/**
 * Remote Listener using JMS to send the event
 *
 * @jmx:mbean extends="org.jboss.management.mejb.ListenerMBean"
 *
 * @author  ???
 * @version $Revision: 1.2.2.1 $
 **/
public class JMSNotificationListener
      implements JMSNotificationListenerMBean
{
   private static final Logger log = Logger.getLogger(JMSNotificationListener.class);

   // JMS Queue Session and Sender must be created on the server-side
   // therefore they are transient and created on the first notification
   // call
   private transient QueueSender mSender;
   private transient QueueSession mSession;
   private String mJNDIName;
   private Queue mQueue;

   public JMSNotificationListener(
         String pJNDIName,
         Queue pQueue
         ) throws JMSException
   {
      mJNDIName = pJNDIName;
      mQueue = pQueue;
   }

   /**
    * Handles the given notification by sending this to the remote
    * client listener
    *
    * @param pNotification				Notification to be send
    * @param pHandback					Handback object
    */
   public void handleNotification(
         Notification pNotification,
         Object pHandback
         )
   {
      try
      {
         if (mSender == null)
         {
            // Get QueueConnectionFactory, create Connection, Session and then Sender
            QueueConnection lConnection = getQueueConnection(mJNDIName);
            mSession = lConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
            mSender = mSession.createSender(mQueue);
         }
         // Create a message and send to the Queue
         Message lMessage = mSession.createObjectMessage(pNotification);
         mSender.send(lMessage);
      }
      catch (Exception e)
      {
         log.error("failed to handle notification", e);
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
      if (pTest instanceof JMSNotificationListener)
      {
         try
         {
            return mQueue.getQueueName().equals(
                  ((JMSNotificationListener) pTest).mQueue.getQueueName()
            );
         }
         catch (JMSException e)
         {
            log.error("unexpcted failure while tetsing equality", e);
         }
      }
      return false;
   }

   /**
    * @return							Hashcode of the local listener
    **/
   public int hashCode()
   {
      return mQueue.hashCode();
   }

   /**
    * Creates a SurveyManagement bean.
    *
    * @return Returns a SurveyManagement bean for use by the Survey handler.
    **/
   private QueueConnection getQueueConnection(String pJNDIName)
         throws NamingException, JMSException
   {
      Context aJNDIContext = new InitialContext();
      Object aRef = aJNDIContext.lookup(pJNDIName);
      QueueConnectionFactory aFactory = (QueueConnectionFactory)
            PortableRemoteObject.narrow(aRef, QueueConnectionFactory.class);
      QueueConnection lConnection = aFactory.createQueueConnection();
      lConnection.start();
      return lConnection;
   }
}
