/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.ObjectOutputStream;

import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import javax.jms.Destination;
import javax.jms.IllegalStateException;
import javax.jms.InvalidDestinationException;
import javax.jms.JMSException;
import javax.jms.MessageListener;
import javax.jms.TemporaryTopic;
import javax.jms.Topic;
import javax.jms.TopicPublisher;

import javax.jms.TopicSession;
import javax.jms.TopicSubscriber;
import javax.jms.XATopicSession;

/**
 * This class implements <tt>javax.jms.TopicSession</tt> and
 * <tt>javax.jms.XATopicSession</tt>.
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.7.2.1 $
 */
public class SpyTopicSession
       extends SpySession
       implements TopicSession, XATopicSession
{
   SpyTopicSession( Connection myConnection, boolean transacted, int acknowledgeMode ) {
      this( myConnection, transacted, acknowledgeMode, false );
   }

   // Constructor ---------------------------------------------------

   SpyTopicSession( Connection myConnection, boolean transacted, int acknowledgeMode, boolean xaSession ) {
      super( myConnection, transacted, acknowledgeMode, xaSession );
   }

   /**
    *  getTopicSession method comment.
    *
    * @return                             The TopicSession value
    * @exception  javax.jms.JMSException  Description of Exception
    */
   public javax.jms.TopicSession getTopicSession()
      throws javax.jms.JMSException {
      return this;
   }

   // Public --------------------------------------------------------

   public Topic createTopic( String topicName )
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The session is closed" );
      }
      if( topicName == null )
         throw new InvalidDestinationException("The topic name cannot be null");

      return ( ( SpyConnection )connection ).createTopic( topicName );
   }

   public TopicSubscriber createSubscriber( Topic topic )
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The session is closed" );
      }
      if( topic == null )
         throw new InvalidDestinationException("Topic cannot be null");
      return createSubscriber( topic, null, false );
   }

   public TopicSubscriber createSubscriber( Topic topic, String messageSelector, boolean noLocal )
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The session is closed" );
      }
      if( topic == null )
          throw new InvalidDestinationException("Topic cannot be null");

      SpyTopicSubscriber sub = new SpyTopicSubscriber( this, ( SpyTopic )topic, noLocal, messageSelector );
      addConsumer( sub );

      return sub;
   }

   public TopicSubscriber createDurableSubscriber( Topic topic, String name )
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The session is closed" );
      }
      if( topic == null )
         throw new InvalidDestinationException("Topic cannot be null");
      if (topic instanceof TemporaryTopic)
         throw new InvalidDestinationException("Attempt to create a durable subscription for a temporary topic");

      if (name == null || name.trim().length() == 0)
         throw new JMSException("Null or empty subscription");

      SpyTopic t = new SpyTopic( ( SpyTopic )topic, connection.getClientID(), name, null );
      SpyTopicSubscriber sub = new SpyTopicSubscriber( this, t, false, null );
      addConsumer( sub );

      return sub;
   }

   public TopicSubscriber createDurableSubscriber( Topic topic, String name, String messageSelector, boolean noLocal )
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The session is closed" );
      }
      if( topic == null )
         throw new InvalidDestinationException("Topic cannot be null");
      if (topic instanceof TemporaryTopic)
         throw new InvalidDestinationException("Attempt to create a durable subscription for a temporary topic");

      if (name == null || name.trim().length() == 0)
         throw new JMSException("Null or empty subscription");

      SpyTopic t = new SpyTopic( ( SpyTopic )topic, connection.getClientID(), name, messageSelector );
      SpyTopicSubscriber sub = new SpyTopicSubscriber( this, t, noLocal, messageSelector );
      addConsumer( sub );

      return sub;
   }

   public TopicPublisher createPublisher( Topic topic )
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The session is closed" );
      }
      return new SpyTopicPublisher( this, topic );
   }

   public TemporaryTopic createTemporaryTopic()
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The session is closed" );
      }
      return ( ( SpyConnection )connection ).getTemporaryTopic();
   }

   public void unsubscribe( String name )
      throws JMSException {
      //Not yet implemented
      DurableSubscriptionID id = new DurableSubscriptionID( connection.getClientID(), name, null );
      connection.unsubscribe( id );
   }
}
