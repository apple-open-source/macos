/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.server;

import org.jboss.mq.ReceiveRequest;

/**
 *  This class contians all the data needed to perform a JMS transaction
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @version    $Revision: 1.1.4.1 $
 */
public class RoutedMessage
{
   // The message
   public MessageReference message;
   public Integer subscriptionId;

   ReceiveRequest toReceiveRequest() throws javax.jms.JMSException
   {
      ReceiveRequest rr = new ReceiveRequest();
      rr.message = message.getMessage();
      rr.subscriptionId = subscriptionId;
      return rr;
   }
}
