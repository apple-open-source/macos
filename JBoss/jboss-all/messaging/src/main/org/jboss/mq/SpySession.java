/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;
import java.io.File;

import java.io.Serializable;
import java.util.Collection;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import javax.jms.BytesMessage;
import javax.jms.Destination;
import javax.jms.IllegalStateException;
import javax.jms.InvalidDestinationException;
import javax.jms.JMSException;
import javax.jms.JMSSecurityException;
import javax.jms.MapMessage;
import javax.jms.Message;
import javax.jms.MessageListener;
import javax.jms.ObjectMessage;

import javax.jms.Session;
import javax.jms.StreamMessage;
import javax.jms.TextMessage;
import javax.jms.XASession;
import javax.transaction.xa.XAResource;

import org.jboss.logging.Logger;

/**
 *  This class implements javax.jms.Session and javax.jms.XASession
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.16.2.7 $
 */
public abstract class SpySession
       implements Session, XASession
{

   //The connection object to which this session is linked
   public Connection connection;
   // Attributes ----------------------------------------------------

   //Is this session running right now?
   public boolean running;
   //Is this session transacted ?
   protected boolean transacted;
   //What is the type of acknowledgement ?
   protected int    acknowledgeMode;
   //MessageConsumers created by this session
   protected HashSet consumers;
   
   // This consumer is the consumer that receives messages for the MessageListener
   // assigned to the session.  The SpyConnectionConsumer delivers messages to him
   SpyMessageConsumer sessionConsumer;

   //Is the session closed ?
   boolean          closed;

   // Used to lock the run() method
   Object           runLock = new Object();

   /**
    * The transctionId of the current transaction (registed with the SpyXAResourceManager).
    */
   private Object currentTransactionId;
   
   // If this is an XASession, we have an associated XAResource
   SpyXAResource    spyXAResource;

   // Optional Connection consumer methods
   LinkedList messages = new LinkedList();

   // keep track of unacknowledged messages
   ArrayList unacknowledgedMessages = new ArrayList();

   static Logger log = Logger.getLogger( SpySession.class );

   // Constructor ---------------------------------------------------

   SpySession( Connection conn, boolean trans, int acknowledge, boolean xaSession )
   {
      connection = conn;
      transacted = trans;
      acknowledgeMode = acknowledge;
      if ( xaSession )
      {
         spyXAResource = new SpyXAResource( this );
      }

      running = true;
      closed = false;
      consumers = new HashSet();

      //Have a TX ready with the resource manager.
      if ( spyXAResource == null && transacted )
      {
         currentTransactionId = connection.spyXAResourceManager.startTx();
         if (log.isTraceEnabled())
         {
            log.trace("Current transaction id: " + currentTransactionId);
         }
      }
   }

   void setCurrentTransactionId(final Object xid)
   {
      if (xid == null)
         throw new org.jboss.util.NullArgumentException("xid");
      
      if (log.isTraceEnabled())
         log.trace("Setting current tx " + this + " xid=" + xid + " previous: " + currentTransactionId);

      this.currentTransactionId = xid;
   }

   void unsetCurrentTransactionId(final Object xid)
   {
      if (xid == null)
         throw new org.jboss.util.NullArgumentException("xid");

      if (log.isTraceEnabled())
         log.trace("Unsetting current tx " + this + " xid=" + xid + " previous: " + currentTransactionId);

      // Don't unset the xid if it has previously been suspended
      // The session could have been recycled 
      if (xid.equals(currentTransactionId))
         this.currentTransactionId = null;
   }
   
   Object getCurrentTransactionId()
   {
      return currentTransactionId;
   }
   
   public void setMessageListener( MessageListener listener )
      throws JMSException
   {
      if ( closed )
      {
         throw new IllegalStateException( "The session is closed" );
      }
      sessionConsumer = new SpyMessageConsumer( this, true );
      sessionConsumer.setMessageListener( listener );
   }

   public boolean getTransacted()
      throws JMSException
   {
      if ( closed )
      {
         throw new IllegalStateException( "The session is closed" );
      }

      return transacted;
   }

   public MessageListener getMessageListener()
      throws JMSException
   {
      if ( closed )
      {
         throw new IllegalStateException( "The session is closed" );
      }
      if ( sessionConsumer == null )
      {
         return null;
      }
      return sessionConsumer.getMessageListener();
   }


   public javax.transaction.xa.XAResource getXAResource()
   {
      return spyXAResource;
   }

   // Public --------------------------------------------------------

   public BytesMessage createBytesMessage()
      throws JMSException
   {
      if ( closed )
      {
         throw new IllegalStateException( "The session is closed" );
      }

      SpyBytesMessage message = MessagePool.getBytesMessage();
      message.header.producerClientId = connection.getClientID();
      return message;
   }

   public MapMessage createMapMessage()
      throws JMSException
   {
      if ( closed )
      {
         throw new IllegalStateException( "The session is closed" );
      }

      SpyMapMessage message = MessagePool.getMapMessage();
      message.header.producerClientId = connection.getClientID();
      return message;
   }

   public Message createMessage()
      throws JMSException
   {
      if ( closed )
      {
         throw new IllegalStateException( "The session is closed" );
      }

      SpyMessage message = MessagePool.getMessage();
      message.header.producerClientId = connection.getClientID();
      return message;
   }

   public ObjectMessage createObjectMessage()
      throws JMSException
   {
      if ( closed )
      {
         throw new IllegalStateException( "The session is closed" );
      }

      SpyObjectMessage message = MessagePool.getObjectMessage();
      message.header.producerClientId = connection.getClientID();
      return message;
   }

   public ObjectMessage createObjectMessage( Serializable object )
      throws JMSException
   {
      if ( closed )
      {
         throw new IllegalStateException( "The session is closed" );
      }

      SpyObjectMessage message = MessagePool.getObjectMessage();
      message.setObject( object );
      message.header.producerClientId = connection.getClientID();
      return message;
   }

   public StreamMessage createStreamMessage()
      throws JMSException
   {
      if ( closed )
      {
         throw new IllegalStateException( "The session is closed" );
      }

      SpyStreamMessage message = MessagePool.getStreamMessage();
      message.header.producerClientId = connection.getClientID();
      return message;
   }

   public TextMessage createTextMessage()
      throws JMSException
   {
      if ( closed )
      {
         throw new IllegalStateException( "The session is closed" );
      }

      SpyTextMessage message = MessagePool.getTextMessage();
      message.header.producerClientId = connection.getClientID();
      return message;
   }

   // Delivers messages queued by ConnectionConsumer to the message listener
   public void run()
   {
      synchronized ( messages )
      {
         while ( messages.size() > 0 )
         {
            SpyMessage message = ( SpyMessage )messages.removeFirst();
            try
            {
               if ( sessionConsumer == null )
               {
                  log.warn( "Session has no message listener set, cannot process message." );
                  //Nack message
                  connection.send( message.getAcknowledgementRequest( false ) );
               }
               else
               {
                  sessionConsumer.addMessage( message );
               }
            }
            catch ( JMSException ignore )
            {
            }
         }
      }
   }

   public void close()
      throws JMSException
   {
      log.debug("Session closing.");

      synchronized ( runLock )
      {
         if ( closed )
            return;

         closed = true;
      }

      JMSException exception = null;

      Iterator i;
      synchronized ( consumers )
      {
         //notify the sleeping synchronous listeners
         if ( sessionConsumer != null )
         {
            try
            {
               sessionConsumer.close();
            }
            catch (InvalidDestinationException ignored)
            {
               log.warn(ignored.getMessage(), ignored);
            }
            catch (JMSException e)
            {
               log.trace(e.getMessage(), e);
               exception = e;
            }
         }

         i = consumers.iterator();
      }

      while ( i.hasNext() )
      {
         SpyMessageConsumer messageConsumer = ( SpyMessageConsumer )i.next();
         try
         {
            messageConsumer.close();
         }
         catch (InvalidDestinationException ignored)
         {
            log.warn(ignored.getMessage(), ignored);
         }
         catch (JMSException e)
         {
            log.trace(e.getMessage(), e);
            if (exception == null)
               exception = e;
         }
      }

      //deal with any unacked messages
      try
      {
         if (spyXAResource == null)
         {
            if (transacted)
               internalRollback();
            else
            {
               i = unacknowledgedMessages.iterator();
               while (i.hasNext())
               {
                  SpyMessage message = (SpyMessage) i.next();
                  connection.send(message.getAcknowledgementRequest(false));
                  i.remove();
               }
            }
         }
      }
      catch (JMSException e)
      {
         log.trace(e.getMessage(), e);
         if (exception == null)
            exception = e;
      }

      connection.sessionClosing( this );

      // Throw the first exception
      if (exception != null)
         throw exception;
   }


   //Commit a transacted session
   public void commit()
      throws JMSException
   {

      //Don't deliver any more messages while commiting
      synchronized ( runLock )
      {
         if ( spyXAResource != null )
         {
            throw new javax.jms.TransactionInProgressException( "Should not be call from a XASession" );
         }
         if ( closed )
         {
            throw new IllegalStateException( "The session is closed" );
         }
         if ( !transacted )
         {
            throw new IllegalStateException( "The session is not transacted" );
         }

         // commit transaction with onePhase commit
         try
         {
            connection.spyXAResourceManager.endTx( currentTransactionId, true );
            connection.spyXAResourceManager.commit( currentTransactionId, true );
         }
         catch ( javax.transaction.xa.XAException e )
         {
            throw new SpyJMSException( "Could not commit", e );
         }
         finally
         {
            unacknowledgedMessages.clear();
            try
            {
               currentTransactionId = connection.spyXAResourceManager.startTx();

               if (log.isTraceEnabled())
               {
                  log.trace("Current transaction id: " + currentTransactionId);
               }
            }
            catch ( Exception ignore )
            {
               if (log.isTraceEnabled())
               {
                  log.trace("Failed to start tx", ignore);
               }
            }
         }
      }
   }

   //Rollback a transacted session
   public void rollback()
      throws JMSException
   {
      synchronized (runLock)
      {
         if ( closed )
            throw new IllegalStateException( "The session is closed" );
         internalRollback();
      }
   }

   private void internalRollback()
      throws JMSException
   {
      synchronized ( runLock )
      {
         if ( spyXAResource != null )
         {
            throw new javax.jms.TransactionInProgressException( "Should not be call from a XASession" );
         }
         if ( !transacted )
         {
            throw new IllegalStateException( "The session is not transacted" );
         }

         // rollback transaction
         try
         {
            connection.spyXAResourceManager.endTx( currentTransactionId, true );
            connection.spyXAResourceManager.rollback( currentTransactionId );
         }
         catch ( javax.transaction.xa.XAException e )
         {
            throw new SpyJMSException( "Could not rollback", e );
         }
         finally
         {
            unacknowledgedMessages.clear();
            try
            {
               currentTransactionId = connection.spyXAResourceManager.startTx();
               if (log.isTraceEnabled())
               {
                  log.trace("Current transaction id: " + currentTransactionId);
               }               
            }
            catch ( Exception ignore )
            {
               if (log.isTraceEnabled())
               {
                  log.trace("Failed to start tx", ignore);
               }
            }
         }
      }
   }

   public void recover()
      throws JMSException
   {
      synchronized (runLock)
      {
         if ( closed )
         {
            throw new IllegalStateException( "The session is closed" );
         }
         if ( transacted )
         {
            throw new IllegalStateException( "The session is transacted" );
         }

         //stop message delivery
         try
         {
            connection.stop();
            running = false;
         }
         catch (JMSException e)
         {
            throw new SpyJMSException("Could not stop message delivery", e);
         }

         // Loop over all consumers, check their unacknowledged messages, set
         // then as redelivered and add back to the list of messages
         try
         {
            synchronized ( messages )
            {
               boolean trace = log.isTraceEnabled();
               if (trace)
                  log.trace("Recovering: unacknowledged messages=" + unacknowledgedMessages);
               Iterator i = consumers.iterator();
               while ( i.hasNext() )
               {
                  SpyMessageConsumer consumer = (SpyMessageConsumer) i.next();

                  Iterator ii = unacknowledgedMessages.iterator();
                  while ( ii.hasNext() )
                  {
                     SpyMessage message = ( SpyMessage )ii.next();

                     if ( consumer.getSubscription().accepts( message.header ) )
                     {
                        message.setJMSRedelivered( true );
                        consumer.messages.addLast( message );
                        ii.remove();
                        if (trace)
                           log.trace("Recovered: message=" + message + " consumer=" + consumer);
                     }
                  }
               }

               // We no longer have consumers for the remaining messages
               i = unacknowledgedMessages.iterator();
               while (i.hasNext())
               {
                  SpyMessage message = (SpyMessage) i.next();
                  connection.send(message.getAcknowledgementRequest(false));
                  i.remove();
                  if (trace)
                     log.trace("Recovered: nacked with no consumer message=" + message);
               }
            }
         }
         catch ( Exception e )
         {
            throw new SpyJMSException ("Unable to recover session ", e );
         }
         // Restart the delivery sequence including all unacknowledged messages that had
         // been previously delivered. Redelivered messages do not have to be delivered
         // in exactly their original delivery order.
         try
         {
            running = true;
            connection.start();
         
            Iterator i = consumers.iterator();
            while ( i.hasNext() )
            {
               ( (SpyMessageConsumer)i.next() ).restartProcessing();
            }
         }
         catch ( JMSException e )
         {
            throw new SpyJMSException("Could not resume message delivery", e);
         }
      }
   }
   
   /**
    * JMS 11.2.21.2
    * Note that the acknowledge method of Message acknowledges all messages
    * received on that messages session.
    *
    * JMS 11.3.2.2.3
    * Message.acknowledge method: Clarify that the method applies to all consumed
    * messages of the session.
    * Rationale for this change: A possible misinterpretation of the existing Java
    * API documentation for Message.acknowledge assumed that only messages
    * received prior to this message should be acknowledged. The updated Java
    * API documentation statement emphasizes that message acknowledgement
    * is really a session-level activity and that this message is only being used to
    * identify the session in order to acknowledge all messages consumed by the
    * session. The acknowledge method was placed in the message object only to
    * enable easy access to acknowledgement capability within a message
    * listeners onMessage method. This change aligns the specification and Java
    * API documentation to define Message.acknowledge in the same manner.
    **/
   public void doAcknowledge ( Message message, AcknowledgementRequest ack )
      throws JMSException
   {
      //if we are nacking, only nack the one message
      //if we are acking, ack all messages consumed by this session
      if ( ack.isAck )
      {
         synchronized ( unacknowledgedMessages )
         {
            //ack the current message
            connection.send( ( ( SpyMessage ) message ).getAcknowledgementRequest( true ) );
            unacknowledgedMessages.remove( message );

            //ack the other messages consumed in this session
            Iterator i = unacknowledgedMessages.iterator();
            while ( i.hasNext() )
            {
               Message mess = (Message)i.next();
               i.remove();
               connection.send( ( ( SpyMessage ) mess ).getAcknowledgementRequest( true ) );
            }
         }
      }
      else
      {
         //nack the current message
         unacknowledgedMessages.remove( message );
         connection.send( ack );
      }
   }

   public void deleteTemporaryDestination( SpyDestination dest )
      throws JMSException
   {
      if (log.isDebugEnabled())
         log.debug( "SpySession: deleteDestination(dest=" + dest.toString() + ")" );

      synchronized ( consumers )
      {
         HashSet newMap = ( HashSet )consumers.clone();
         newMap.remove( dest );
         consumers = newMap;
      }

      //We could look at our incoming and outgoing queues to drop messages
   }

   public TextMessage createTextMessage( String string )
      throws JMSException
   {
      if ( closed )
      {
         throw new IllegalStateException( "The session is closed" );
      }

      SpyTextMessage message = new SpyTextMessage();
      message.setText( string );
      message.header.producerClientId = connection.getClientID();
      return message;
   }


   String getNewMessageID()
      throws JMSException
   {
      if ( closed )
      {
         throw new IllegalStateException( "The session is closed" );
      }

      return connection.getNewMessageID();
   }
   
   void addMessage( SpyMessage message )
   {
      synchronized ( messages )
      {
         messages.addLast( message );
      }
   }
   
   void addUnacknowlegedMessage( SpyMessage message )
   {
      if ( !transacted )
      {
         synchronized ( unacknowledgedMessages ) {
            unacknowledgedMessages.add( message );
         }
      }
   }

   //called by a MessageProducer object which needs to publish a message
   void sendMessage( SpyMessage m )
      throws JMSException
   {
      if ( closed )
      {
         throw new IllegalStateException( "The session is closed" );
      }

      // Make sure the message has the correct client id
      m.header.producerClientId = connection.getClientID();

      if ( transacted )
      {
         connection.spyXAResourceManager.addMessage( currentTransactionId, m.myClone() );
      }
      else
      {
         connection.sendToServer( m );
      }

   }

   void addConsumer( SpyMessageConsumer who )
      throws JMSException
   {
      if ( closed )
      {
         throw new IllegalStateException( "The session is closed" );
      }

      synchronized ( consumers )
      {
         HashSet newMap = ( HashSet )consumers.clone();
         newMap.add( who );
         consumers = newMap;
      }
      try
      {
         connection.addConsumer( who );
      }
      catch(JMSSecurityException ex)
      {
         removeConsumerInternal(who);
         throw ex;
      }

   }

   void removeConsumer( SpyMessageConsumer who )
      throws JMSException
   {
      connection.removeConsumer( who );
      removeConsumerInternal(who);
   }

   private void removeConsumerInternal( SpyMessageConsumer who )
   {
      synchronized ( consumers )
      {
         HashSet newMap = ( HashSet )consumers.clone();
         newMap.remove( who );
         consumers = newMap;
      }
   }

   /**
    * This is used by the ASF SessionPool stuff
    */
   public SpyXAResourceManager getXAResourceManager()
   {
      return connection.spyXAResourceManager;
   }

   // JMS 1.1
   public int getAcknowledgeMode()
      throws JMSException
   {
      return acknowledgeMode;
   }
}
/*
vim:ts=3:sw=3:et
*/
