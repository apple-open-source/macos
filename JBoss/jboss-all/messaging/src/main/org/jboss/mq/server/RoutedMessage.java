/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.server;
import java.io.Externalizable;
import java.io.IOException;
import java.io.ObjectInput;
import java.io.ObjectOutput;
import org.jboss.mq.ReceiveRequest;

import java.io.Serializable;

/**
 *  This class contians all the data needed to perform a JMS transaction
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @version    $Revision: 1.1 $
 */
public class RoutedMessage {
   // The message
   public MessageReference message;
   public Integer   subscriptionId;
   
   ReceiveRequest toReceiveRequest() throws javax.jms.JMSException {
      ReceiveRequest rr = new ReceiveRequest();
      rr.message = message.getMessage();
      rr.subscriptionId = subscriptionId;
      return rr;
   }
}
