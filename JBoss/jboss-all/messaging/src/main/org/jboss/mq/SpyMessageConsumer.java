/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;
import java.util.Iterator;

import java.util.LinkedList;
import javax.jms.Destination;
import javax.jms.IllegalStateException;
import javax.jms.JMSException;
import javax.jms.Message;
import javax.jms.MessageConsumer;
import javax.jms.MessageListener;
import javax.jms.Session;

import org.jboss.logging.Logger;

/**
 * This class implements <tt>javax.jms.MessageConsumer</tt>.
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @author     David Maplesden (David.Maplesden@orion.co.nz)
 * @created    August 16, 2001
 * @version    $Revision: 1.20.2.4 $
 */
public class SpyMessageConsumer
   implements MessageConsumer, SpyConsumer, Runnable
{
   static Logger log = Logger.getLogger( SpyMessageConsumer.class );
   
   //Link to my session
   public SpySession session;
   // The subscription structure should be fill out by the decendent
   public Subscription subscription = new Subscription();
   //Am I closed ?
   protected boolean closed;
   
   protected Object stateLock = new Object();
   protected boolean receiving = false;
   protected boolean waitingForMessage = false;
   protected boolean listening = false;
   protected Thread listenerThread = null;
   //My message listener (null if none)
   MessageListener  messageListener;
   
   //List of Pending messages (not yet delivered)
   LinkedList       messages;
   
   //Is this a session consumer?
   boolean          sessionConsumer;
   
   // Constructor ---------------------------------------------------
   
   SpyMessageConsumer( SpySession s, boolean sessionConsumer )
   {
      session = s;
      this.sessionConsumer = sessionConsumer;
      messageListener = null;
      closed = false;
      messages = new LinkedList();
   }
   
   public void setMessageListener( MessageListener listener )
      throws JMSException
   {
      
      if ( closed )
      {
         throw new IllegalStateException( "The MessageConsumer is closed" );
      }
      
      synchronized ( stateLock )
      {
         if ( receiving )
         {
            throw new JMSException( "Another thread is already in receive." );
         }
         
         boolean oldListening = listening;
         listening = ( listener != null );
         messageListener = listener;
         
         if ( !sessionConsumer && listening && !oldListening )
         {
            //Start listener thread (if one is not already running)
            if ( listenerThread == null )
            {
               listenerThread = new Thread( this, "MessageListenerThread - " + subscription.destination.getName() );
               listenerThread.start();
            }
         }
      }
   }
   
   // Public --------------------------------------------------------
   
   public String getMessageSelector()
      throws JMSException
   {
      if ( closed )
      {
         throw new IllegalStateException( "The MessageConsumer is closed" );
      }
      
      return subscription.messageSelector;
   }
   
   public MessageListener getMessageListener()
      throws JMSException
   {
      if ( closed )
      {
         throw new IllegalStateException( "The MessageConsumer is closed" );
      }
      
      return messageListener;
   }
   
   public Subscription getSubscription()
   {
      return subscription;
   }
   
   public Message receive()
      throws JMSException
   {
      
      if ( closed )
      {
         throw new IllegalStateException( "The MessageConsumer is closed" );
      }
      
      synchronized ( stateLock )
      {
         if ( receiving )
         {
            throw new JMSException( "Another thread is already in receive." );
         }
         if ( listening )
         {
            throw new JMSException( "A message listener is already registered" );
         }
         receiving = true;
      }
      
      synchronized ( messages )
      {
         //see if we have any undelivered messages before we go to the JMS
         //server to look.
         Message message = getMessage();
         if ( message != null )
         {
            synchronized ( stateLock )
            {
               receiving = false;
            }
            return message;
         } 
         // Loop through expired messages
         while (true)
         {
            SpyMessage msg = session.connection.receive( subscription, 0 );
            if ( msg != null )
            {
               Message mes = preProcessMessage( msg );
               if ( mes != null )
               {
                  synchronized ( stateLock )
                  {
                     receiving = false;
                  }
                  return mes;
               }
            }
            else
               break;
         }
         
         try
         {
            waitingForMessage = true;
            while ( true )
            {
               if ( closed )
               {
                  return null;
               }
               Message mes = getMessage();
               if ( mes != null )
               {
                  return mes;
               }
               messages.wait();
            }
         }
         catch ( InterruptedException e )
         {
            JMSException newE = new SpyJMSException( "Receive interupted" );
            newE.setLinkedException( e );
            throw newE;
         }
         finally
         {
            waitingForMessage = false;
            synchronized ( stateLock )
            {
               receiving = false;
            }
         }
      }
      
   }
   
   public Message receive( long timeOut )
      throws JMSException
   {
      if ( timeOut == 0 )
      {
         return receive();
      }
      
      if ( closed )
      {
         throw new IllegalStateException( "The MessageConsumer is closed" );
      }
      
      synchronized ( stateLock )
      {
         if ( receiving )
         {
            throw new JMSException( "Another thread is already in receive." );
         }
         if ( listening )
         {
            throw new JMSException( "A message listener is already registered" );
         }
         receiving = true;
      }
      
      long endTime = System.currentTimeMillis() + timeOut;
      
      synchronized ( messages )
      {
         //see if we have any undelivered messages before we go to the JMS
         //server to look.
         Message message = getMessage();
         if ( message != null )
         {
            synchronized ( stateLock )
            {
               receiving = false;
            }
            return message;
         }
         // Loop through expired messages
         while (true)
         {
            SpyMessage msg = session.connection.receive( subscription, timeOut );
            if ( msg != null )
            {
               Message mes = preProcessMessage( msg );
               if ( mes != null )
               {
                  synchronized ( stateLock )
                  {
                     receiving = false;
                  }
                  return mes;
               }
            }
            else
               break;
         }
         
         try
         {
            waitingForMessage = true;
            while ( true )
            {
               if ( closed )
               {
                  return null;
               }
               
               Message mes = getMessage();
               if ( mes != null )
               {
                  return mes;
               }
               
               long att = endTime - System.currentTimeMillis();
               if ( att <= 0 )
               {
                  return null;
               }
               
               messages.wait( att );
            }
         }
         catch ( InterruptedException e )
         {
            JMSException newE = new SpyJMSException( "Receive interupted" );
            newE.setLinkedException( e );
            throw newE;
         } finally
         {
            waitingForMessage = false;
            synchronized ( stateLock )
            {
               receiving = false;
            }
         }
      }
      
   }
   
   public Message receiveNoWait()
      throws JMSException
   {
      if ( closed )
      {
         throw new IllegalStateException( "The MessageConsumer is closed" );
      }
      
      synchronized ( stateLock )
      {
         if ( receiving )
         {
            throw new JMSException( "Another thread is already in receive." );
         }
         if ( listening )
         {
            throw new JMSException( "A message listener is already registered" );
         }
         receiving = true;
      }
      //see if we have any undelivered messages before we go to the JMS
      //server to look.
      synchronized ( messages )
      {
         Message mes = getMessage();
         if ( mes != null )
         {
            synchronized ( stateLock )
            {
               receiving = false;
            }
            return mes;
         }
      } 
      // Loop through expired messages
      while (true)
      {
         SpyMessage msg = session.connection.receive( subscription, -1 );
         if ( msg != null )
         {
            Message mes = preProcessMessage( msg );
            if ( mes != null )
            {
               synchronized ( stateLock )
               {
                  receiving = false;
               }
               return mes;
            }
         }
         else
         {
            synchronized ( stateLock )
            {
               receiving = false;
            }
            return null;
         }
      }
   }
   
   public void close()
      throws JMSException
   {
      
      log.debug("Message consumer closing.");
      
      synchronized ( messages )
      {
         if ( closed )
         {
            return;
         }
         
         closed = true;
         messages.notify();
      }
      
      if ( listenerThread != null && !Thread.currentThread().equals(listenerThread) )
      {
         try
         {
            listenerThread.join();
         }
         catch(InterruptedException e)
         { }
      }
      
      if ( !sessionConsumer )
      {
         session.removeConsumer( this );
      }
   }
   
   public void addMessage( SpyMessage message )
      throws JMSException
   {
      if ( closed )
      {
         log.debug( "WARNING: NACK issued. The message consumer was closed." );
         session.connection.send( message.getAcknowledgementRequest( false ) );
         return;
      }
      
      //Add a message to the queue
      
      //  Consider removing this test (subscription.accepts).  I don't think it will ever fail
      //  because the test is also done by the server before message is even sent.
      if ( subscription.accepts( message.header ) )
      {
         if ( sessionConsumer )
         {
            sessionConsumerProcessMessage( message );
         }
         else
         {
            synchronized ( messages )
            {
               if ( waitingForMessage )
               {
                  messages.addLast( message );
                  messages.notifyAll();
               }
               else
               {
                  //unwanted message (due to consumer receive timing out) Nack it.
                  log.debug( "WARNING: NACK issued. The message consumer was not waiting for a message." );
                  session.connection.send( message.getAcknowledgementRequest( false ) );
               }
            }
         }
      }
      else
      {
         log.debug( "WARNING: NACK issued. The subscription did not accept the message" );
         session.connection.send( message.getAcknowledgementRequest( false ) );
      }
   }
   
   //Used to facilitate delivery of messages to a message listener.
   public void run()
   {
      SpyMessage mes = null;
      try
      {
         outer :
            while ( true )
            {
               //get Message
               while ( mes == null )
               {
                  synchronized ( messages )
                  {
                     if ( closed )
                     {
                        waitingForMessage = false;
                        break outer;
                     }
                     if (messages.isEmpty())
                        mes = session.connection.receive( subscription, 0 );
                     if ( mes == null )
                     {
                        waitingForMessage = true;
                        while ( ( messages.isEmpty() && !closed ) || ( !session.running ) )
                        {
                           try
                           {
                              messages.wait();
                           }
                           catch ( InterruptedException e )
                           {
                           }
                        }
                        if ( closed )
                        {
                           waitingForMessage = false;
                           break outer;
                        }
                        mes = ( SpyMessage )messages.removeFirst();
                        waitingForMessage = false;
                     }
                  }
                  mes.session = session;
                  if ( mes.isOutdated() )
                  {
                     //Drop message (it has expired)
                     mes.doAcknowledge();
                     mes = null;
                  }
               }
               
               MessageListener thisListener;
               synchronized ( stateLock )
               {
                  if ( !isListening() )
                  {
                     //send NACK cause we have closed listener
                     if ( mes != null )
                     {
                        session.connection.send( mes.getAcknowledgementRequest( false ) );
                     }
                     //this thread is about to die, so we will need a new one if a new listener is added
                     listenerThread = null;
                     mes = null;
                     break;
                  }
                  thisListener = messageListener;
               }
               Message message = mes;
               if ( mes instanceof SpyEncapsulatedMessage )
               {
                  message = ( ( SpyEncapsulatedMessage )mes ).getMessage();
               }
               
               if ( session.transacted )
               {
                  session.connection.spyXAResourceManager.ackMessage( session.getCurrentTransactionId(), mes );
               }
               
               //Handle runtime exceptions.  These are handled as per the spec if you assume
               //the number of times erroneous messages are redelivered in auto_acknowledge mode
               //is 0.   :)
               try
               {
                  session.addUnacknowlegedMessage( ( SpyMessage )message );
                  thisListener.onMessage( message );
               }
               catch ( RuntimeException e )
               {
                  log.warn( "Message listener " + thisListener + " threw a RuntimeException." );
               }
               
               if ( !session.transacted && ( session.acknowledgeMode == session.AUTO_ACKNOWLEDGE || session.acknowledgeMode == session.DUPS_OK_ACKNOWLEDGE ) )
               {
                  // Only acknowledge the message if the message wasn't recovered
                  boolean recovered;
                  synchronized (messages)
                  {
                     recovered = messages.contains(message);
                  }
                  if (recovered == false)
                     mes.doAcknowledge();
               }
               mes = null;
            }
      }
      catch ( JMSException e )
      {
         log.warn( "Message consumer closing due to error in listening thread.", e );
         try
         {
            close();
         }
         catch ( Exception ignore )
         {
         }
      }
   }
   
   public String toString()
   {
      return "SpyMessageConsumer: " + subscription;
   }
   
   protected boolean isListening()
   {
      synchronized ( stateLock )
      {
         return listening;
      }
   }
   
   protected void sessionConsumerProcessMessage( SpyMessage message )
      throws JMSException
   {
      message.session = session;
      if ( message.isOutdated() )
      {
         log.debug( "I dropped a message (timeout)" );
         message.doAcknowledge();
         return;
      }
      //simply pass on to messageListener (if there is one)
      MessageListener thisListener;
      synchronized ( stateLock )
      {
         thisListener = messageListener;
      }
      
      // Add the message to XAResource manager before we call onMessages since the
      // resource may get elisted IN the onMessage method.
      // This gives onMessage a chance to roll the message back.
      Object anonymousTXID=null;
      if ( session.transacted )
      {
         // Only happens with XA transactions
         if( session.getCurrentTransactionId() == null )
         {
            anonymousTXID = session.connection.spyXAResourceManager.startTx();
            session.setCurrentTransactionId(anonymousTXID);
         }
         session.connection.spyXAResourceManager.ackMessage( session.getCurrentTransactionId(), message );
      }
      
      if ( thisListener != null )
      {
         Message mes = message;
         if ( message instanceof SpyEncapsulatedMessage )
         {
            mes = ( ( SpyEncapsulatedMessage )message ).getMessage();
         }
         session.addUnacknowlegedMessage( ( SpyMessage ) mes );
         thisListener.onMessage( mes );
      }
      
      if (session.transacted)
      {
         // If we started an anonymous tx
         if (anonymousTXID != null)
         {
            if (session.getCurrentTransactionId() == anonymousTXID)
            {
               // This is bad..  We are an XA controled TX but no TM ever elisted us.
               // rollback the work and spit an error
               try
               {
                  session.connection.spyXAResourceManager.endTx(anonymousTXID, true);
                  session.connection.spyXAResourceManager.rollback(anonymousTXID);
               }
               catch (javax.transaction.xa.XAException e)
               {
                  log.error("Could not rollback", e);
               }
               finally
               {
                  session.unsetCurrentTransactionId(anonymousTXID);
               }
               throw new SpyJMSException("Messaged delivery was not controled by a Transaction Manager");
            }
         }
      }
      else
      {
         // Should we Auto-ack the message since the message has now been processesed
         if (session.acknowledgeMode == session.AUTO_ACKNOWLEDGE || session.acknowledgeMode == session.DUPS_OK_ACKNOWLEDGE)
         {
            message.doAcknowledge();
         }
      }
   }
   
   /**
    * Restarts the processing of the messages in case of a recovery
    **/
   public void restartProcessing()
   {
      synchronized ( messages )
      {
         messages.notifyAll();
      }
   }

   Message getMessage()
   {
      synchronized ( messages )
      {   
         while ( true )
         {
            
            try
            {
               if ( messages.size() == 0 )
               {
                  return null;
               }
               
               SpyMessage mes = ( SpyMessage )messages.removeFirst();
               
               Message rc = preProcessMessage( mes );
               // could happen if the message has expired.
               if ( rc == null )
               {
                  continue;
               }
               
               return rc;
            }
            catch ( Exception e )
            {
               log.error("Ignoring error", e);
            }
         }
      }
   }

   Message preProcessMessage( SpyMessage message )
      throws JMSException
   {
      message.session = session;
      session.addUnacknowlegedMessage( message );

      // Has the message expired?
      if ( message.isOutdated() )
      {
         log.debug( "I dropped a message (timeout)" );
         message.doAcknowledge();
         return null;
      }
      
      // Should we try to ack before the message is processed?
      if ( !isListening() )
      {
         
         if ( session.transacted )
         {
            session.connection.spyXAResourceManager.ackMessage( session.getCurrentTransactionId(), message );
         }
         else if ( session.acknowledgeMode == session.AUTO_ACKNOWLEDGE || session.acknowledgeMode == session.DUPS_OK_ACKNOWLEDGE )
         {
            message.doAcknowledge();
         }
         
         if ( message instanceof SpyEncapsulatedMessage )
         {
            return ( ( SpyEncapsulatedMessage )message ).getMessage();
         }
         return message;
      }
      else
      {
         return message;
      }
   }
}
