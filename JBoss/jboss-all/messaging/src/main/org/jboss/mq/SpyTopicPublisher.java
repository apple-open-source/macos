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
import javax.jms.TemporaryTopic;
import javax.jms.Topic;

import javax.jms.TopicPublisher;

/**
 *  This class implements javax.jms.TopicPublisher
 *
 * A publisher created with a null Topic will now be interpreted as
 * created as an unidentifyed publisher and follows the spec in throwing
 * UnsupportedOperationException  at the correct places.
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @author     <a href="pra@tim.se">Peter Antman</a>
 * @created    August 16, 2001
 * @version    $Revision: 1.5 $
 */
public class SpyTopicPublisher
       extends SpyMessageProducer
       implements TopicPublisher {
   // Attributes ----------------------------------------------------

   //The session to which this publisher is linked
   private SpyTopicSession mySession;
   //The topic of this publisher
   private Topic    myTopic = null;

   // Constructor ---------------------------------------------------

   SpyTopicPublisher( SpyTopicSession s, Topic t ) {
      mySession = s;
      myTopic = t;
      try {
         if ( t instanceof TemporaryTopic ) {
            setDeliveryMode( DeliveryMode.NON_PERSISTENT );
         } else {
            setDeliveryMode( DeliveryMode.PERSISTENT );
         }
      } catch ( JMSException e ) {
      }
   }

   // Public --------------------------------------------------------

   public Topic getTopic()
      throws JMSException {
      return myTopic;
   }

   //Publish methods
   public void publish( Message message )
      throws JMSException {
      if ( myTopic == null ) {
         throw new UnsupportedOperationException("Not constructed with identifyed topic. Usage of method not allowed");
      }
      internalPublish( myTopic, message, defaultDeliveryMode, defaultPriority, defaultTTL );
   }
   
   public void publish( Message message, int deliveryMode, int priority, long timeToLive )
      throws JMSException {
      if ( myTopic == null ) {
         throw new UnsupportedOperationException("Not constructed with identifyed topic. Usage of method not allowed");
      }
      internalPublish( myTopic, message, deliveryMode, priority, timeToLive );
   }

   public void publish( Topic topic, Message message )
      throws JMSException {
      // If its the same object (place in memmory) its ok.
      if (myTopic != null && !myTopic.equals(topic))
         throw new UnsupportedOperationException("Publishing to unidentifyed topic not allowed when publisher created with an identifyed topic");
      internalPublish( topic, message, defaultDeliveryMode, defaultPriority, defaultTTL );
   }
   
   
   
   public void publish( Topic topic, Message message, int deliveryMode, int priority, long timeToLive )
      throws JMSException {
      if (myTopic != null && !myTopic.equals(topic))
         throw new UnsupportedOperationException("Publishing to unidentifyed topic not allowed when publisher created with an identifyed topic");
      internalPublish(topic, message, deliveryMode, priority, timeToLive);
   }


   protected void internalPublish(Topic topic, Message message, int deliveryMode, int priority, long timeToLive )
      throws JMSException {
      //Set the header fields
      message.setJMSDestination( topic );
      message.setJMSDeliveryMode( deliveryMode );
      long ts = System.currentTimeMillis();
      message.setJMSTimestamp( ts );
      if ( timeToLive == 0 ) {
         message.setJMSExpiration( 0 );
      } else {
         message.setJMSExpiration( timeToLive + ts );
      }
      message.setJMSPriority( priority );
      message.setJMSMessageID( mySession.getNewMessageID() );
      
      // Encapsulate the message if not a SpyMessage
      if ( !( message instanceof SpyMessage ) ) {
         SpyEncapsulatedMessage m = MessagePool.getEncapsulatedMessage();
         m.setMessage( message );
         message = m;
      }
      
      //Send the message
      mySession.sendMessage( ( SpyMessage )message );
   }
}
