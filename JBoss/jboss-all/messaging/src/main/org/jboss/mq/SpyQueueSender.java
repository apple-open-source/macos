/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;
import javax.jms.DeliveryMode;
import javax.jms.InvalidDestinationException;
import javax.jms.JMSException;
import javax.jms.Message;
import javax.jms.Queue;

import javax.jms.QueueSender;
import javax.jms.TemporaryQueue;

/**
 *  This class implements javax.jms.QueueSender
 *
 * A sender created with a null Queue will now be interpreted as
 * created as an unidentifyed sender and follows the spec in throwing
 * UnsupportedOperationException at the correct places.
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @author     <a href="pra@tim.se">Peter Antman</a>
 * @created    August 16, 2001
 * @version    $Revision: 1.4 $
 */
public class SpyQueueSender
       extends SpyMessageProducer
       implements QueueSender {
   // Attributes ----------------------------------------------------

   //The session to which this sender is linked
   private SpyQueueSession session;
   //The queue of this sender
   private Queue    queue = null;

   // Constructor ---------------------------------------------------

   SpyQueueSender( SpyQueueSession session, Queue queue ) {
      this.session = session;
      this.queue = queue;
      try {
         if ( queue instanceof TemporaryQueue ) {
            setDeliveryMode( DeliveryMode.NON_PERSISTENT );
         } else {
            setDeliveryMode( DeliveryMode.PERSISTENT );
         }
      } catch ( JMSException e ) {
      }
   }

   // Public --------------------------------------------------------

   public Queue getQueue()
      throws JMSException {
      return queue;
   }

   //Send methods
   public void send( Message message )
      throws JMSException {
      if ( queue == null ) {
         throw new UnsupportedOperationException("Not constructed with identifyed queue. Usage of method not allowed");
      }
      internalSend( queue, message, defaultDeliveryMode, defaultPriority, defaultTTL );
   }
 
   public void send( Message message, int deliveryMode, int priority, long timeToLive )
      throws JMSException {
      if ( queue == null ) {
         throw new UnsupportedOperationException("Not constructed with identifyed queue. Usage of method not allowed");
      }
      internalSend( queue, message, deliveryMode, priority, timeToLive );
   }
     
   public void send( Queue queue, Message message )
      throws JMSException {
      if (this.queue != null && !this.queue.equals(queue))
         throw new UnsupportedOperationException("Sending to unidentifyed queue not allowed when sender created with an identifyed queue");
      
      internalSend( queue, message, defaultDeliveryMode, defaultPriority, defaultTTL );
   }

   public void send( Queue queue, Message message, int deliveryMode, int priority, long timeToLive )
      throws JMSException {
      if (this.queue != null  && !this.queue.equals(queue))
         throw new UnsupportedOperationException("Sending to unidentifyed queue not allowed when sender created with an identifyed queue");
      internalSend(queue,message,deliveryMode,priority,timeToLive);

   }
   public void internalSend( Queue queue, Message message, int deliveryMode, int priority, long timeToLive )
      throws JMSException {
      
      //Set the header fields
      message.setJMSDestination( queue );
      message.setJMSDeliveryMode( deliveryMode );
      long ts = System.currentTimeMillis();
      message.setJMSTimestamp( ts );
      if ( timeToLive == 0 ) {
         message.setJMSExpiration( 0 );
      } else {
         message.setJMSExpiration( timeToLive + ts );
      }
      message.setJMSPriority( priority );
      message.setJMSMessageID( session.getNewMessageID() );
      
      // Encapsulate the message if not a SpyMessage
      if ( !( message instanceof SpyMessage ) ) {
         SpyEncapsulatedMessage m = MessagePool.getEncapsulatedMessage();
         m.setMessage( message );
         message = m;
      }
      
      //Send the message.
      session.sendMessage( ( SpyMessage )message );
   }
}
