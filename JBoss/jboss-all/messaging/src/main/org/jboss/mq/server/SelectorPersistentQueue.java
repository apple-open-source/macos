/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.server;

import javax.jms.JMSException;

import org.jboss.mq.SpyTopic;
import org.jboss.mq.pm.Tx;
import org.jboss.mq.selectors.Selector;

/**
 *  This class adds a selector to a persistent queue.
 *
 *  Factored out of JMSTopic.
 *
 * @author     Adrian Brock (Adrian.Brock@HappeningTimes.com)
 * @created    24th October 2002
 */
public class SelectorPersistentQueue
    extends PersistentQueue
{
   // Attributes ----------------------------------------------------

   /**
    * The string representation of the selector
    */
   String selectorString;

   /**
    * The implementation of the selector
    */
   Selector selector;

   // Constructor ---------------------------------------------------

   /**
    * Create a new persistent queue with a selector
    *
    * @param server the destination manager
    * @param dstopic the topic with a durable subscription
    * @param selector the selector string
    * @exception JMSException for an error
    */
   public SelectorPersistentQueue(JMSDestinationManager server, SpyTopic dstopic, String selector, BasicQueueParameters parameters)
      throws JMSException
   {
      super(server, dstopic, parameters);
      this.selectorString = selector;
      this.selector = new Selector(selector);
   }

   // Public --------------------------------------------------------
      	  
   /**
    * Filters the message with the selector before adding to the queue

    * @param mesRef the message
    * @param txId the transaction
    * @exception JMSException for an error
    */
   public void addMessage(MessageReference mesRef, Tx txId)
      throws JMSException
   {
      if (selector.test(mesRef.getHeaders()))
         super.addMessage( mesRef, txId );
      else
         server.getMessageCache().remove(mesRef);
   }
}
