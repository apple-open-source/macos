/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;
import java.util.HashMap;

import java.util.HashSet;
import java.util.Iterator;
import javax.jms.DeliveryMode;
import javax.jms.Destination;
import javax.jms.IllegalStateException;
import javax.jms.InvalidDestinationException;
import javax.jms.JMSException;
import javax.jms.MessageListener;
import javax.jms.Queue;
import javax.jms.QueueBrowser;
import javax.jms.QueueReceiver;
import javax.jms.QueueSender;

import javax.jms.QueueSession;
import javax.jms.TemporaryQueue;
import javax.jms.XAQueueSession;

/**
 *  This class implements javax.jms.QueueSession and javax.jms.XAQueueSession
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.4 $
 */
public class SpyQueueSession
       extends SpySession
       implements QueueSession, XAQueueSession {

   SpyQueueSession( Connection myConnection, boolean transacted, int acknowledgeMode ) {
      this( myConnection, transacted, acknowledgeMode, false );
   }

   // Constructor ---------------------------------------------------

   SpyQueueSession( Connection myConnection, boolean transacted, int acknowledgeMode, boolean xaSession ) {
      super( myConnection, transacted, acknowledgeMode, xaSession );
   }

   /**
    *  getQueueSession method comment.
    *
    * @return                             The QueueSession value
    * @exception  javax.jms.JMSException  Description of Exception
    */
   public QueueSession getQueueSession()
      throws javax.jms.JMSException {
      return this;
   }

   // Public --------------------------------------------------------

   public QueueBrowser createBrowser( Queue queue )
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The session is closed" );
      }
      if ( queue == null ) {
         throw new InvalidDestinationException("Cannot browse a null queue.");
      }
      return new SpyQueueBrowser( this, queue, null );
   }

   public QueueBrowser createBrowser( Queue queue, String messageSelector )
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The session is closed" );
      }
      if ( queue == null ) {
         throw new InvalidDestinationException("Cannot browse a null queue.");
      }
      return new SpyQueueBrowser( this, queue, messageSelector );
   }

   public Queue createQueue( String queueName )
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The session is closed" );
      }
      if ( queueName == null ) {
         throw new InvalidDestinationException("Queue name cannot be null.");
      }
      return ( ( SpyConnection )connection ).createQueue( queueName );
   }

   public QueueReceiver createReceiver( Queue queue )
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The session is closed" );
      }
      if ( queue == null ) {
         throw new InvalidDestinationException("Queue cannot be null.");
      }

      SpyQueueReceiver receiver = new SpyQueueReceiver( this, queue, null );
      addConsumer( receiver );

      return receiver;
   }

   public QueueReceiver createReceiver( Queue queue, String messageSelector )
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The session is closed" );
      }
      if ( queue == null ) {
         throw new InvalidDestinationException("Queue cannot be null.");
      }

      SpyQueueReceiver receiver = new SpyQueueReceiver( this, queue, messageSelector );
      addConsumer( receiver );

      return receiver;
   }

   public QueueSender createSender( Queue queue )
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The session is closed" );
      }
      return new SpyQueueSender( this, queue );
   }

   public TemporaryQueue createTemporaryQueue()
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The session is closed" );
      }

      return ( ( SpyConnection )connection ).getTemporaryQueue();
   }
}
