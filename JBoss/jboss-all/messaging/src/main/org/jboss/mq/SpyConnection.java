/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;

import java.io.Serializable;
import javax.jms.ConnectionConsumer;
import javax.jms.JMSException;

import javax.jms.Queue;
import javax.jms.QueueConnection;

import javax.jms.IllegalStateException;
import javax.jms.InvalidDestinationException;
import javax.jms.QueueSession;
import javax.jms.ServerSessionPool;
import javax.jms.TemporaryQueue;
import javax.jms.TemporaryTopic;
import javax.jms.Topic;

import javax.jms.TopicConnection;
import javax.jms.TopicSession;

import org.jboss.mq.il.ServerIL;

/**
 *  This class implements javax.jms.TopicConnection
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.5.2.2 $
 */
public class SpyConnection
       extends Connection
       implements Serializable, TopicConnection, QueueConnection {

   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   // Constructor ---------------------------------------------------

   public SpyConnection( String userId, String password, GenericConnectionFactory gcf )
      throws JMSException {
      super( userId, password, gcf );
   }

   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   // Constructor ---------------------------------------------------

   public SpyConnection( GenericConnectionFactory gcf )
      throws JMSException {
      super( gcf );
   }


   // Public --------------------------------------------------------

   public TopicSession createTopicSession( boolean transacted, int acknowledgeMode )
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The connection is closed" );
      }
      checkClientID();
      
      TopicSession session = new SpyTopicSession( this, transacted, acknowledgeMode );

      //add the new session to the createdSessions list
      synchronized ( createdSessions ) {
         createdSessions.add( session );
      }

      return session;
   }

   public ConnectionConsumer createConnectionConsumer( Topic topic,
         String messageSelector,
         ServerSessionPool sessionPool,
         int maxMessages )
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The connection is closed" );
      }

      if (topic == null)
         throw new InvalidDestinationException("Null topic");

      checkClientID();
      
      return new SpyConnectionConsumer( this, topic, messageSelector, sessionPool, maxMessages );
   }

   public ConnectionConsumer createDurableConnectionConsumer( Topic topic, String subscriptionName, String messageSelector, ServerSessionPool sessionPool, int maxMessages )
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The connection is closed" );
      }

      if (topic == null)
         throw new InvalidDestinationException("Null topic");
      if (topic instanceof TemporaryTopic)
         throw new InvalidDestinationException("Attempt to create a durable subscription for a temporary topic");

      if (subscriptionName == null || subscriptionName.trim().length() == 0)
         throw new JMSException("Null or empty subscription");

      SpyTopic t = new SpyTopic( ( SpyTopic )topic, getClientID(), subscriptionName, messageSelector );
      return new SpyConnectionConsumer( this, t, messageSelector, sessionPool, maxMessages );
   }

   public ConnectionConsumer createConnectionConsumer( Queue queue,
         String messageSelector,
         ServerSessionPool sessionPool,
         int maxMessages )
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The connection is closed" );
      }

      if (queue == null)
         throw new InvalidDestinationException("Null queue");

      return new SpyConnectionConsumer( this, queue, messageSelector, sessionPool, maxMessages );
   }

   // Public --------------------------------------------------------

   public QueueSession createQueueSession( boolean transacted, int acknowledgeMode )
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The connection is closed" );
      }
      checkClientID();
      
      QueueSession session = new SpyQueueSession( this, transacted, acknowledgeMode );

      //add the new session to the createdSessions list
      synchronized ( createdSessions ) {
         createdSessions.add( session );
      }

      return session;
   }


   TemporaryTopic getTemporaryTopic()
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The connection is closed" );
      }
      checkClientID();
      
      try
      {
         SpyTemporaryTopic temp = (SpyTemporaryTopic) serverIL.getTemporaryTopic( connectionToken );
         temp.setConnection(this);
         return temp;
      }
      catch ( Exception e )
      {
         throw new SpyJMSException( "Cannot create a Temporary Topic", e );
      }
   }

   TemporaryQueue getTemporaryQueue()
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The connection is closed" );
      }
      checkClientID();
      
      try
      {
         SpyTemporaryQueue temp = (SpyTemporaryQueue) serverIL.getTemporaryQueue( connectionToken );
         temp.setConnection(this);
         return temp;
      }
      catch ( Exception e )
      {
         throw new SpyJMSException( "Cannot create a Temporary Queue", e );
      }
   }

   Topic createTopic( String name )
      throws JMSException {
      try {
         if ( closed ) {
            throw new IllegalStateException( "The connection is closed" );
         }
         checkClientID();
         
         return serverIL.createTopic( connectionToken, name );
      } catch ( Exception e ) {
         throw new SpyJMSException( "Cannot get the Topic from the provider", e );
      }
   }

   //Get a queue
   Queue createQueue( String name )
      throws JMSException {
      try {
         if ( closed ) {
            throw new IllegalStateException( "The connection is closed" );
         }
         checkClientID();
         
         return serverIL.createQueue( connectionToken, name );
      } catch ( Exception e ) {
         throw new SpyJMSException( "Cannot get the Queue from the provider", e );
      }
   }
}
