/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/

package org.jboss.test.foedeployer.ejb.message;

import java.rmi.RemoteException;

import javax.ejb.CreateException;
import javax.ejb.MessageDrivenBean;
import javax.ejb.MessageDrivenContext;

import javax.jms.JMSException;
import javax.jms.Message;
import javax.jms.MessageListener;
import javax.jms.ObjectMessage;
import javax.jms.TextMessage;
import javax.jms.QueueConnectionFactory;
import javax.jms.QueueConnection;
import javax.jms.QueueSession;
import javax.jms.Session;
import javax.jms.QueueSender;
import javax.jms.Queue;

import javax.naming.InitialContext;
import javax.naming.NamingException;

import org.apache.log4j.Category;

/**
 * A simple Message Driven Bean
 *
 * @ejb.bean
 *    name="MessageTrader"
 *    generate="true"
 *    jndi-name="MessageTraderBean"
 *    transaction-type="Container"
 *    acknowledge-mode="Auto-acknowledge"
 *    destination-type="javax.jms.Topic"
 *    subscription-durability="NonDurable"
 *
 * @jboss.destination-jndi-name name="topic/testTopic"
 * @weblogic.message-driven destination-jndi-name="topic/testTopic"
 *
 * @author <a href="mailto:loubyansky@hotmail.com">Alex Loubyansky</a>
 */
public class MessageTraderBean
   implements MessageDrivenBean, MessageListener
{
   // Constants -------------------------------------------------
   private static final String QUEUE_CONNECTION_FACTORY = "ConnectionFactory";
   private static final String QUEUE = "queue/testQueue";

   // Attributes ------------------------------------------------
   protected Category log;
   private MessageDrivenContext mdc;
   private transient QueueConnection queueConnection;
   private transient Queue queue;

   // MessageDrivenBean implementation --------------------------
   /**
    * Sets the session context.
    *
    * @param ctx   MessageDrivenContext Context for session
    */
   public void setMessageDrivenContext(MessageDrivenContext ctx)
   {
      mdc = ctx;
   }

   // Implementation of MessageListener -----------------------------------
   /**
    * Handle the message.
    */
   public void onMessage(Message msg)
   {
      getLog().debug("received message of type: " + msg.getClass().getName());

      if( !( msg instanceof ObjectMessage ) )
         getLog().error( "message isn't of type ObjectMessage" );

      try
      {
         QuoteMessage qm = (QuoteMessage) ( (ObjectMessage)msg ).getObject();

         getLog().debug( "received new quote: " + qm.getQuote() );

         send( msg );
      }
      catch(Exception ex)
      {
         getLog().error("ERROR: ", ex);
      }
   }

   public void ejbCreate() { }

   public void ejbRemove()
   {
      if( queueConnection != null )
      {
         getLog().debug( "closing connection" );
         try
         {
            queueConnection.close();
         }
         catch( JMSException jmse )
         {
            getLog().debug( "Exception while closing queue connection: ", jmse );
         }
      }
      else
      {
         // it could be null because not all MDBs in pool might be used
         getLog().debug( "queue connection is null" );
      }
   }

   // Private --------------------------------------------------------
   private void send( Message msg )
      throws Exception
   {
      QueueSession queueSession = getQueueSession();
      queue = getQueue();

      getLog().debug( "creating sender" );
      QueueSender queueSender = queueSession.createSender( queue );

      ObjectMessage objMsg = (ObjectMessage)msg;
      QuoteMessage qm = (QuoteMessage)objMsg.getObject();
      getLog().debug( "resending the message: " + qm.getQuote() );
      queueSender.send( msg );
   }

   private QueueSession getQueueSession()
      throws Exception
   {
      if(queueConnection == null)
      {
         getLog().debug("looking for queue connection factory: "
            + QUEUE_CONNECTION_FACTORY );

         InitialContext ctx = new InitialContext();
         QueueConnectionFactory queueFactory =
            (QueueConnectionFactory) ctx.lookup(QUEUE_CONNECTION_FACTORY);
         queueConnection = queueFactory.createQueueConnection();
      }

      getLog().debug( "creating queue connection" );
      return queueConnection.createQueueSession(false,
                Session.AUTO_ACKNOWLEDGE);
   }

   private Queue getQueue()
      throws Exception
   {
      if(queue == null)
      {
         getLog().debug( "looking for queue: " + QUEUE );

         InitialContext ctx = new InitialContext();
         queue = (Queue) ctx.lookup( QUEUE );
      }

      return queue;
   }

   private Category getLog()
   {
      if( log != null ) return log;
      log = Category.getInstance( this.getClass() );
      return log;
   }
}
