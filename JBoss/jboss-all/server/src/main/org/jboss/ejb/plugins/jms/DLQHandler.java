/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.ejb.plugins.jms;

import java.util.Hashtable;
import java.util.HashMap;
import java.util.Map;
import java.util.Enumeration;
import java.util.Iterator;

import javax.naming.InitialContext;
import javax.naming.Context;
import javax.jms.Session;
import javax.jms.QueueConnection;
import javax.jms.QueueConnectionFactory;
import javax.jms.QueueSession;
import javax.jms.QueueSender;
import javax.jms.Queue;
import javax.jms.Message;
import javax.jms.JMSException;
import javax.transaction.Status;
import javax.transaction.Synchronization;
import javax.transaction.Transaction;

import org.w3c.dom.Element;

import org.jboss.logging.Logger;
import org.jboss.deployment.DeploymentException;
import org.jboss.metadata.MetaData;
import org.jboss.jms.jndi.JMSProviderAdapter;
import org.jboss.system.ServiceMBeanSupport;

/**
 * Places redeliveded messages on a Dead Letter Queue.
 *
 *<p>
 *The Dead Letter Queue handler is used to not set JBoss in an endles loop
 * when a message is resent on and on due to transaction rollback for
 * message receipt.
 *
 * <p>
 * It sends message to a dead letter queue (configurable, defaults to
 * queue/DLQ) when the message has been resent a configurable amount of times,
 * defaults to 10.
 *
 * <p>
 * The handler is configured through the element MDBConfig in
 * container-invoker-conf.
 *
 * <p>
 * The JMS property JBOSS_ORIG_DESTINATION in the resent message is set
 * to the name of the original destination (Destionation.toString()).
 *
 * <p>
 * The JMS property JBOSS_ORIG_MESSAGEID in the resent message is set
 * to the id of the original message.
 *
 * Created: Thu Aug 23 21:17:26 2001
 *
 * @version <tt>$Revision: 1.11.2.9 $</tt>
 * @author ???
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class DLQHandler
   extends ServiceMBeanSupport
{
   /** JMS property name holding original destination. */
   public static final String JBOSS_ORIG_DESTINATION ="JBOSS_ORIG_DESTINATION";
   
   /** JMS property name holding original JMS message id. */
   public static final String JBOSS_ORIG_MESSAGEID="JBOSS_ORIG_MESSAGEID";
   
   /** Properties copied from org.jboss.mq.SpyMessage */
   private static final String JMS_JBOSS_REDELIVERY_COUNT = "JMS_JBOSS_REDELIVERY_COUNT";
   private static final String JMS_JBOSS_REDELIVERY_LIMIT = "JMS_JBOSS_REDELIVERY_LIMIT";
   
   // Configuratable stuff

   /**
    * Destination to send dead letters to.
    * 
    * <p>
    * Defaults to <em>queue/DLQ</em>, configurable through
    * <tt>DestinationQueue</tt> element.
    */
   private String destinationJNDI = "queue/DLQ";
   
   /**
    * Maximum times a message is alowed to be resent.
    *
    * <p>Defaults to <em>10</em>, configurable through
    * <tt>MaxTimesRedelivered</tt> element.
    */
   private int maxResent = 10;
   
   /**
    * Time to live for the message.
    *
    * <p>
    * Defaults to <em>{@link Message#DEFAULT_TIME_TO_LIVE}</em>, 
    * configurable through the <tt>TimeToLive</tt> element.
    */
   private long timeToLive = Message.DEFAULT_TIME_TO_LIVE;
   
   // May become configurable
   
   /** Delivery mode for message, Message.DEFAULT_DELIVERY_MODE. */
   private int deliveryMode = Message.DEFAULT_DELIVERY_MODE;

   /** Priority for the message, Message.DEFAULT_PRIORITY */
   private int priority = Message.DEFAULT_PRIORITY;

   /** The dlq user for the connection */
   private String dlqUser;

   /** The dlq password for the connection */
   private String dlqPass;
   
   // Private stuff
   private QueueConnection connection;
   private Queue dlq;
   private JMSProviderAdapter providerAdapter;
   private Hashtable resentBuffer = new Hashtable();

   public DLQHandler(final JMSProviderAdapter providerAdapter)
   {
      this.providerAdapter = providerAdapter;
   }

   //--- Service
   
   /**
    * Initalize the service.
    *
    * @throws Exception    Service failed to initalize.
    */
   protected void createService() throws Exception
   {
      Context ctx = providerAdapter.getInitialContext();
      
      try {
         String factoryName = providerAdapter.getQueueFactoryRef();
         QueueConnectionFactory factory = (QueueConnectionFactory)
            ctx.lookup(factoryName);
         log.debug("Using factory: " + factory);
         
         if (dlqUser == null)
            connection = factory.createQueueConnection();
         else
            connection = factory.createQueueConnection(dlqUser, dlqPass);
         log.debug("Created connection: " + connection);

         dlq = (Queue)ctx.lookup(destinationJNDI);
         log.debug("Using Queue: " + dlq);
      }
      catch (Exception e)
      {
         if (e instanceof JMSException)
            throw e;
         else
         {
            JMSException x = new JMSException("Error creating the dlq connection: " + e.getMessage());
            x.setLinkedException(e);
            throw x;
         }
      }
      finally {
         ctx.close();
      }
   }
   
   protected void destroyService() throws Exception
   {
      // Help the GC
      if (connection != null)
         connection.close();
      connection = null;
      dlq = null;
      providerAdapter = null;
   }
   
   //--- Logic
   
   /**
    * Check if a message has been redelivered to many times.
    *
    * If message has been redelivered to many times, send it to the
    * dead letter queue (default to queue/DLQ).
    *
    * @return true if message is handled (i.e resent), false if not.
    */
   public boolean handleRedeliveredMessage(final Message msg, final Transaction tx)
   {
      boolean handled = false;
      int max = this.maxResent;
      String id = null;
      boolean jbossmq = true;
      int count = 0;
      
      try
      {

         if (msg.propertyExists(JMS_JBOSS_REDELIVERY_LIMIT))
            max = msg.getIntProperty(JMS_JBOSS_REDELIVERY_LIMIT); 

         if (msg.propertyExists(JMS_JBOSS_REDELIVERY_COUNT))
            count = msg.getIntProperty(JMS_JBOSS_REDELIVERY_COUNT);
         else
         {
            id = msg.getJMSMessageID();
            if (id == null)
            {
               // if we can't get the id we are basically fucked
               log.error("Message id is null, can't handle message");
               return false;
            }
            count = incrementResentCount(id);
            jbossmq = false;
         }
         
         if (count > max)
         {
            id = msg.getJMSMessageID();
            log.warn("Message resent too many times; sending it to DLQ; message id=" + id);
            
            sendMessage(msg);
            deleteFromBuffer(id);

            handled = true;
         }
         else if (jbossmq == false && tx != null)
         {
            // Register a synchronization to remove the buffer entry
            // should the transaction commit
            DLQSynchronization synch = new DLQSynchronization(id);
            try
            {
               tx.registerSynchronization(synch);
            }
            catch (Exception e)
            {
               log.warn("Error registering DlQ Synchronization with transaction " + tx, e);
            }
         }
      }
      catch (JMSException e)
      {
         // If we can't send it ahead, we do not dare to just drop it...or?
         log.error("Could not send message to Dead Letter Queue", e);
      }
      
      return handled;
   }

   /**
    * Send message to the configured dead letter queue, defaults to queue/DLQ.
    */
   protected void sendMessage(Message msg) throws JMSException
   {
      boolean trace = log.isTraceEnabled();
      
      QueueSession session = null;
      QueueSender sender = null;

      try
      {
         msg = makeWritable(msg, trace); // Don't know yet if we are gona clone or not
         
         // Set the properties
         msg.setStringProperty(JBOSS_ORIG_MESSAGEID,
         msg.getJMSMessageID());
         msg.setStringProperty(JBOSS_ORIG_DESTINATION,
         msg.getJMSDestination().toString());
         
         session = connection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
         sender = session.createSender(dlq);
         if (trace) {
            log.trace("Sending message to DLQ; destination=" +
                      dlq + ", session=" + session + ", sender=" + sender);
         }

         sender.send(msg, deliveryMode, priority, timeToLive);

         if (trace) {
            log.trace("Message sent.");
         }
         
      }
      finally
      {
         try
         {
            if (sender != null) sender.close();
            if (session != null) session.close();
         }
         catch(Exception e)
         {
            log.warn("Failed to close sender or session; ignoring", e);
         }
      }
   }

   /**
    * Increment the counter for the specific JMS message id.
    *
    * @return the new counter value.
    */
   protected int incrementResentCount(String id)
   {
      BufferEntry entry = null;
      boolean trace = log.isTraceEnabled();
      if(!resentBuffer.containsKey(id))
      {
         if (trace)
         log.trace("Making new entry for id " + id);
         entry = new BufferEntry();
         entry.id = id;
         entry.count = 1;
         resentBuffer.put(id,entry);
      } else
      {
         entry = (BufferEntry)resentBuffer.get(id);
         entry.count++;
         if (trace)
         log.trace("Incremented old entry for id " + id + " count " + entry.count);
      }
      return entry.count;
   }
   
   /**
    * Delete the entry in the message counter buffer for specifyed JMS id.
    */
   protected void deleteFromBuffer(String id)
   {
      resentBuffer.remove(id);
   }
   
   /**
    * Make the Message properties writable.
    *
    * @return the writable message.
    */
   protected Message makeWritable(Message msg, boolean trace) throws JMSException
   {
      HashMap tmp = new HashMap();

      // Save properties
      for (Enumeration en=msg.getPropertyNames(); en.hasMoreElements();)
      {
         String key = (String)en.nextElement();
         tmp.put(key, msg.getObjectProperty(key));
      }
      
      // Make them writable
      msg.clearProperties();
      
      Iterator i = tmp.entrySet().iterator();
      while (i.hasNext())
      {
         Map.Entry me = (Map.Entry)i.next();
         String key = (String) me.getKey();
         try
         {
            msg.setObjectProperty(key, me.getValue());
         }
         catch (JMSException ignored)
         {
            if (trace)
               log.trace("Could not copy message property " + key, ignored);
         }
      }
      
      return msg;
   }
   
   /**
    * Takes an MDBConfig Element
    */
   public void importXml(final Element element) throws DeploymentException
   {
      destinationJNDI = MetaData.getElementContent
         (MetaData.getUniqueChild(element, "DestinationQueue"));
      
      try
      {
         String mr = MetaData.getElementContent
            (MetaData.getUniqueChild(element, "MaxTimesRedelivered"));
         maxResent = Integer.parseInt(mr);
      }
      catch (Exception ignore) {}

      try {
         String ttl = MetaData.getElementContent
            (MetaData.getUniqueChild(element, "TimeToLive"));
         timeToLive = Long.parseLong(ttl);
         
         if (timeToLive < 0) {
            log.warn("Invalid TimeToLive: " + timeToLive + "; using default");
            timeToLive = Message.DEFAULT_TIME_TO_LIVE;
         }
      }
      catch (Exception ignore) {}

      dlqUser = MetaData.getElementContent(MetaData.getOptionalChild(element, "DLQUser"));
      dlqPass = MetaData.getElementContent(MetaData.getOptionalChild(element, "DLQPassword"));
   }
   
   public String toString()
   {
      return super.toString() +
         "{ destinationJNDI=" +  destinationJNDI +
         ", maxResent=" + maxResent +
         ", timeToLive=" + timeToLive +
         " }";
   }
   
   private static class BufferEntry
   {
      int count;
      String id;
   }

   /**
    * Remove a redelivered message from the DLQ's buffer when it is acknowledged
    */
   protected class DLQSynchronization
      implements Synchronization
   {
      /** The message id */
      String id;

      public DLQSynchronization(String id)
      {
         this.id = id;
      }

      public void beforeCompletion()
      {
      }

      /**
       * Forget the message when the transaction commits
       */
      public void afterCompletion(int status)
      {
         if (status == Status.STATUS_COMMITTED)
            deleteFromBuffer(id);
      }
   }
}
