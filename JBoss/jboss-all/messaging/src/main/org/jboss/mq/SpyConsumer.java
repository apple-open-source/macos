/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;

import javax.jms.JMSException;

/**
 *  This class defines the interface which is used by the ConnectionReceiver to
 *  send messages to the consumers.
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.2 $
 */
public interface SpyConsumer {
   // A ConnectionReceiver uses this method to load a Consumer with a message
   public void addMessage( SpyMessage mes )
      throws JMSException;

   // This is used to know what type of messages the consumer wants
   public Subscription getSubscription();
}
