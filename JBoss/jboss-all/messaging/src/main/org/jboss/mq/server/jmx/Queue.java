/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mq.server.jmx;

import java.util.ArrayList;

import javax.jms.IllegalStateException;

import org.jboss.mq.SpyQueue;
import org.jboss.mq.server.JMSQueue;
import org.jboss.mq.server.JMSDestinationManager;
import org.jboss.mq.server.MessageCounter;

/**
 * This class is a message queue which is stored (hashed by Destination)
 * on the JMS provider
 *
 * @jmx:mbean extends="org.jboss.mq.server.jmx.DestinationMBean"
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     <a href="hiram.chirino@jboss.org">Hiram Chirino</a>
 * @author     <a href="pra@tim.se">Peter Antman</a>
 * @version    $Revision: 1.3.2.5 $
 */
public class Queue
   extends DestinationMBeanSupport
   implements QueueMBean
{
   JMSQueue destination;

   /**
    * @jmx:managed-attribute
    */
   public String getQueueName()
   {
      return destinationName;
   }

   /**
    * Gets the QueueDepth attribute of the BasicQueue object
    * @jmx:managed-attribute
    *
    * @return                The QueueDepth value
    * @exception  Exception  Description of Exception
    */
   public int getQueueDepth() throws Exception
   {
      return destination.queue.getQueueDepth();
   }

   /**
    * Gets the ScheduledMessageCount attribute of the BasicQueue object
    * @jmx:managed-attribute
    *
    * @return                The ScheduledMessageCount value
    * @exception  Exception  Description of Exception
    */
   public int getScheduledMessageCount() throws Exception
   {
      return destination.queue.getScheduledMessageCount();
   }

   public void startService() throws Exception
   {
      if (destinationName == null || destinationName.length() == 0)
      {
         throw new IllegalStateException("QueueName was not set");
      }

      JMSDestinationManager jmsServer = (JMSDestinationManager)
         server.getAttribute(jbossMQService, "Interceptor");

      spyDest = new SpyQueue(destinationName);
      destination = new JMSQueue((SpyQueue)spyDest, null, jmsServer, parameters);

      jmsServer.addDestination(destination);

      if (jndiName == null) {
         setJNDIName("queue/" + destinationName);
      }
      else {
         // in config phase, all we did was store the name, and not actually bind
         setJNDIName(jndiName);
      }
      super.startService();
   }   

   /**
    * @see DestinationMBean#removeAllMessages()
    */
   public void removeAllMessages() throws Exception
   {
      destination.removeAllMessages();
   }

   /**
    * @jmx:managed-attribute
    */
   public int getReceiversCount()
   {
      return destination.queue.getReceiversCount();
   }
   
   /**
    * @jmx:managed-operation
    */
   public ArrayList listReceivers()
   {
      return destination.queue.getReceivers();
   }

   /**
    * @see DestinationMBeanSupport#getMessageCounter()
    */
   public MessageCounter[] getMessageCounter()
   {
      return destination.getMessageCounter();
   }
}
