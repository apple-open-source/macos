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
import javax.jms.JMSException;
import javax.jms.ServerSession;
import javax.jms.ServerSessionPool;

import org.jboss.logging.Logger;

/**
 *  This class implements javax.jms.ConnectionConsumer
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.8 $
 */
public class SpyConnectionConsumer
   implements javax.jms.ConnectionConsumer, SpyConsumer, Runnable
{
   static Logger log = Logger.getLogger( SpyConnectionConsumer.class );

   // The connection is the consumer was created with
   Connection       connection;
   // The destination this consumer will receive messages from
   Destination      destination;
   // The ServerSessionPool that is implemented by the AS
   javax.jms.ServerSessionPool serverSessionPool;
   // The maximum number of messages that a single session will be loaded with.
   int              maxMessages;
   // This queue will hold messages until they are dispatched to the MessageListener
   LinkedList       queue = new LinkedList();
   // Is the ConnectionConsumer closed?
   boolean          closed = false;
   boolean          waitingForMessage = false;
   // The subscription info the consumer
   Subscription     subscription = new Subscription();
   // The "listening" thread that gets messages from destination and queues them for delivery to sessions
   Thread           internalThread;


   /**
    *  SpyConnectionConsumer constructor comment.
    *
    * @param  connection         Description of Parameter
    * @param  destination        Description of Parameter
    * @param  messageSelector    Description of Parameter
    * @param  serverSessionPool  Description of Parameter
    * @param  maxMessages        Description of Parameter
    * @exception  JMSException   Description of Exception
    */
   public SpyConnectionConsumer( Connection connection, Destination destination, String messageSelector, ServerSessionPool serverSessionPool, int maxMessages )
      throws JMSException {

      this.connection = connection;
      this.destination = destination;
      this.serverSessionPool = serverSessionPool;
      this.maxMessages = maxMessages;
      if(this.maxMessages < 1)
         this.maxMessages = 1;

      subscription.destination = ( SpyDestination )destination;
      subscription.messageSelector = messageSelector;
      subscription.noLocal = false;

      connection.addConsumer( this );
      internalThread = new Thread( this, "Connection Consumer for dest " + destination );
      internalThread.start();
   }

   /**
    *  getServerSessionPool method comment.
    *
    * @return                             The ServerSessionPool value
    * @exception  javax.jms.JMSException  Description of Exception
    */
   public javax.jms.ServerSessionPool getServerSessionPool()
      throws javax.jms.JMSException {
      return serverSessionPool;
   }

   public Subscription getSubscription() {
      return subscription;
   }

   public void addMessage( SpyMessage mes )
      throws JMSException
   {
      if( log.isTraceEnabled() )
         log.trace( "" + this + "->addMessage(mes=" + mes + ")" );
      synchronized ( queue )
      {
         if ( closed )
         {
            log.warn( "NACK issued. The connection consumer was closed." );
            connection.send( mes.getAcknowledgementRequest( false ) );
            return;
         }

         if ( waitingForMessage )
         {
            queue.addLast( mes );
            queue.notifyAll();
         }
         else
         {
            //unwanted message (due to consumer receive timing out?) Nack it.
            connection.send( mes.getAcknowledgementRequest( false ) );
         }
      }
   }

   /**
    *  close method comment.
    *
    * @exception  javax.jms.JMSException  Description of Exception
    */
   public void close()
      throws javax.jms.JMSException
   {
      synchronized ( queue )
      {
         if ( closed )
         {
            return;
         }

         closed = true;
         queue.notifyAll();
      }
      if ( internalThread != null && !internalThread.equals( Thread.currentThread() ) )
      {
         try
         {
            internalThread.join();
         }
         catch ( InterruptedException e )
         {
         }
      }
      synchronized ( queue )
      {
         //nack messages on queue
         while ( !queue.isEmpty() )
         {
            SpyMessage message = ( SpyMessage )queue.removeFirst();
            connection.send( message.getAcknowledgementRequest( false ) );
         }
         connection.removeConsumer( this );
      }
   }

   public String toString()
   {
      return "SpyConnectionConsumer:" + destination;
   }

   //Used to facilitate delivery of messages to sessions from server session pool.
   public void run()
   {
      java.util.ArrayList mesList = new java.util.ArrayList();
      try
      {
         boolean trace = log.isTraceEnabled();
         outer :
         while ( true )
         {
            synchronized( queue )
            {
               if(closed)
                  break outer;
            }
            //get Messages
            for(int i=0;i<maxMessages;i++)
            {
               SpyMessage mes = connection.receive(subscription, -1); //receive no wait
               if(mes == null)
                  break;
               else
                  mesList.add(mes);
            }
            if(mesList.isEmpty())
            {
               SpyMessage mes = null;
               synchronized ( queue )
               {
                  mes = connection.receive( subscription, 0 );
                  if ( mes == null )
                  {
                     waitingForMessage = true;
                     while ( queue.isEmpty() && !closed )
                     {
                        try
                        {
                           queue.wait();
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
                     mes = ( SpyMessage )queue.removeFirst();
                     waitingForMessage = false;
                  }
               }
               mesList.add(mes);
            }

            ServerSession serverSession = serverSessionPool.getServerSession();
            SpySession spySession = ( SpySession )serverSession.getSession();

            if ( spySession.sessionConsumer == null )
            {
               if (log.isDebugEnabled())
                  log.debug( "" + this + " Session did not have a set MessageListner" );
            }
            else
            {
               spySession.sessionConsumer.subscription = subscription;
            }

            for(int i=0;i<mesList.size();i++)
            {
               spySession.addMessage( (SpyMessage)mesList.get(i) );
            }

            if( trace )
               log.trace( "" + this + " Starting the ServerSession." );
            serverSession.start();
            mesList.clear();
         }
      }
      catch ( JMSException e )
      {
         log.warn( "Connection consumer closing due to error in listening thread.", e );
         try
         {
            for(int i=0;i<mesList.size();i++)
            {
               SpyMessage msg = (SpyMessage)mesList.get(i);
               connection.send(msg.getAcknowledgementRequest( false ) );
            }
            close();
         }
         catch ( Exception ignore )
         {
         }
      }
   }
}
