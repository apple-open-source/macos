/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mq.server.jmx;

import java.util.List;

import javax.jms.IllegalStateException;

import org.jboss.mq.SpyTopic;
import org.jboss.mq.server.JMSDestinationManager;
import org.jboss.mq.server.JMSTopic;
import org.jboss.mq.server.MessageCounter;

/**
 * This class is a message queue which is stored (hashed by Destination) on the
 * JMS provider
 *
 * @jmx:mbean extends="org.jboss.mq.server.jmx.DestinationMBean"
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     <a href="hiram.chirino@jboss.org">Hiram Chirino</a>
 * @author     <a href="pra@tim.se">Peter Antman</a>
 * @version    $Revision: 1.3.2.5 $
 */
public class Topic
   extends DestinationMBeanSupport
   implements TopicMBean
{
   JMSTopic destination;
   
   /**
    * @jmx:managed-attribute
    */
   public String getTopicName()
   {
      return destinationName;
   }

   public void startService() throws Exception
   {
      if (destinationName == null || destinationName.length() == 0)
      {
         throw new IllegalStateException("TopicName was not set");
      }
      
      JMSDestinationManager jmsServer = (JMSDestinationManager)
         server.getAttribute(jbossMQService, "Interceptor");

      spyDest = new SpyTopic(destinationName);
      destination = new JMSTopic((SpyTopic)spyDest, null, jmsServer, parameters);
      
      jmsServer.addDestination(destination);
                          
      if (jndiName == null) {
            setJNDIName("topic/" + destinationName);
      }
      else {
         // in config phase, we only stored the name, and didn't actually bind it
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
   public int getAllMessageCount()
   {
      return destination.getAllMessageCount();
   }

   /**
    * @jmx:managed-attribute
    */
   public int getDurableMessageCount()
   {
      return destination.getDurableMessageCount();
   }

   /**
    * @jmx:managed-attribute
    */
   public int getNonDurableMessageCount()
   {
      return destination.getNonDurableMessageCount();
   }

   /**
    * @jmx:managed-attribute
    */
   public int getAllSubscriptionsCount()
   {
      return destination.getAllSubscriptionsCount();
   }

   /**
    * @jmx:managed-attribute
    */
   public int getDurableSubscriptionsCount()
   {
      return destination.getDurableSubscriptionsCount();
   }

   /**
    * @jmx:managed-attribute
    */
   public int getNonDurableSubscriptionsCount()
   {
      return destination.getNonDurableSubscriptionsCount();
   }

   /**
    * @jmx:managed-operation
    */
   public List listAllSubscriptions()
   {
      return destination.getAllSubscriptions();
   }

   /**
    * @jmx:managed-operation
    */
   public List listDurableSubscriptions()
   {
      return destination.getDurableSubscriptions();
   }

   /**
    * @jmx:managed-operation
    */
   public List listNonDurableSubscriptions()
   {
      return destination.getNonDurableSubscriptions();
   }
   
   /**
    * @see DestinationMBeanSupport#getMessageCounter()
    */
   public MessageCounter[] getMessageCounter()
   {
      return destination.getMessageCounter();
   }
}
