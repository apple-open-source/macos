/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;

import java.io.Serializable;

import javax.jms.InvalidSelectorException;
import javax.jms.JMSException;

import org.jboss.mq.selectors.Selector;
import org.jboss.logging.Logger;

/**
 * This class contains all the data needed to for a the provider to to
 * determine if a message can be routed to a consumer.
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @author     David Maplesden (David.Maplesden@orion.co.nz)
 * @created    August 16, 2001
 * @version    $Revision: 1.7.2.2 $
 */
public class Subscription
       implements Serializable
{  
   private static final long serialVersionID = -4045603824932803577L;

   /** This gets set to a unique value at the SpyConnection. */   
   public int       subscriptionId;
   
   /** The queue we want to subscribe to. */
   public SpyDestination destination;
   
   /** The selector which will filter out messages. */
   public String    messageSelector;
   
   /** Should this message destroy the subscription? */
   public boolean   destroyDurableSubscription;

   /** Topics might not want locally produced messages. */
   public boolean   noLocal;

   // Transient Values
   public transient Selector selector;
   public transient ConnectionToken connectionToken;
   public transient Object clientConsumer; // = null;

   /**
    * Determines the consumer would accept the message.
    */
   public Selector getSelector() throws InvalidSelectorException
   {
      if (messageSelector == null || messageSelector.trim().length() == 0)
      {
         return null;
      }

      if (selector == null) {
         selector = new Selector(messageSelector);
      }

      return selector;
   }

   /**
    * Determines the consumer would accept the message.
    */
   public boolean accepts(SpyMessage.Header header) throws JMSException {
      if (header.jmsDestination instanceof SpyTopic) {
         
         // In the Topic case we allways deliver unless we have a noLocal
         if (noLocal ) {
         	if( header.producerClientId.equals(connectionToken.getClientID())) 
               return false;
         }
      }
      
      Selector ms = getSelector();
      if (ms != null) {
         if (!ms.test(header)) {
            return false;
         }
      }
      return true;
   }

   public Subscription myClone() {
      Subscription result = new Subscription();
      //only need to clone non-transient fields for our purposes.

      result.subscriptionId = subscriptionId;
      result.destination = destination;
      result.messageSelector = messageSelector;
      result.destroyDurableSubscription = destroyDurableSubscription;
      result.noLocal = noLocal;

      return result;
   }

   public String toString()
   {
      StringBuffer buffer = new StringBuffer(100);
      buffer.append(getClass().getName()).append("{");

      if (connectionToken != null)
         buffer.append("connection=").append(connectionToken);
      buffer.append(" subscriptionId=").append(subscriptionId);
      buffer.append(" destination=").append(destination);
      buffer.append(" messageSelector=").append(messageSelector);
      if (noLocal)
         buffer.append(" NoLocal");
      else
         buffer.append(" Local");
      if (destroyDurableSubscription)
         buffer.append(" Destroy");
      else
         buffer.append(" Create");

      buffer.append("}");
      return buffer.toString();
   }
}
