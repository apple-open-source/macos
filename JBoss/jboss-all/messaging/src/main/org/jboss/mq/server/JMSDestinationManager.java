/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.server;

import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.TreeMap;

import javax.jms.Destination;
import javax.jms.InvalidClientIDException;
import javax.jms.InvalidDestinationException;
import javax.jms.JMSException;
import javax.jms.Queue;
import javax.jms.TemporaryQueue;
import javax.jms.TemporaryTopic;
import javax.jms.Topic;

import org.jboss.logging.Logger;
import org.jboss.mq.AcknowledgementRequest;
import org.jboss.mq.ConnectionToken;
import org.jboss.mq.DurableSubscriptionID;
import org.jboss.mq.SpyDestination;
import org.jboss.mq.SpyJMSException;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.SpyQueue;
import org.jboss.mq.SpyTemporaryQueue;
import org.jboss.mq.SpyTemporaryTopic;
import org.jboss.mq.SpyTopic;
import org.jboss.mq.SpyTransactionRolledBackException;
import org.jboss.mq.Subscription;
import org.jboss.mq.TransactionRequest;
import org.jboss.mq.pm.PersistenceManager;
import org.jboss.mq.pm.Tx;
import org.jboss.mq.sm.StateManager;

import EDU.oswego.cs.dl.util.concurrent.ConcurrentReaderHashMap;

/**
 * This class implements the JMS provider
 *
 * @author    Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author    Hiram Chirino (Cojonudo14@hotmail.com)
 * @author    David Maplesden (David.Maplesden@orion.co.nz)
 * @author <a href="mailto:pra@tim.se">Peter Antman</a>
 * @version   $Revision: 1.2.2.17 $
 */
public class JMSDestinationManager extends JMSServerInterceptorSupport
{
   static Logger log = Logger.getLogger(JMSDestinationManager.class);

   /**
    * Description of the Field
    */
   public final static String JBOSS_VESION = "JBossMQ Version 3.2";

   /////////////////////////////////////////////////////////////////////
   // Attributes
   /////////////////////////////////////////////////////////////////////

   //messages pending for a Destination ( HashMap of JMSServerQueue objects )
   /**
    * Description of the Field
    */
   public Map destinations = new ConcurrentReaderHashMap();
   //Thread group for server side threads.
   /**
    * Description of the Field
    */
   public ThreadGroup threadGroup = new ThreadGroup("JBossMQ Server Threads");

   //The list of ClientConsumers hased by ConnectionTokens
   Map clientConsumers = new ConcurrentReaderHashMap();

   //last id given to a client
   private int lastID = 1;
   //last id given to a temporary topic
   private int lastTemporaryTopic = 1;
   private Object lastTemporaryTopicLock = new Object();
   //last id given to a temporary queue
   private int lastTemporaryQueue = 1;
   private Object lastTemporaryQueueLock = new Object();
   //The security manager
   private StateManager stateManager;
   //The persistence manager
   private PersistenceManager persistenceManager;
   //The Cache Used to hold messages
   private MessageCache messageCache;

   private Object stateLock = new Object();

   private Object idLock = new Object();

   /**
    * <code>true</code> when the server is running. <code>false</code> when the
    * server should stop running.
    */
   private boolean alive = true;

   /**
    * Because there can be a delay between killing the JMS service and the
    * service actually dying, this field is used to tell external classes that
    * that server has actually stopped.
    */
   private boolean stopped = true;

   /** Temporary queue/topic parameters */
   BasicQueueParameters parameters;

   /////////////////////////////////////////////////////////////////////
   // Constructors
   /////////////////////////////////////////////////////////////////////
   /**
    * Constructor for the JMSServer object
    */
   public JMSDestinationManager(BasicQueueParameters parameters)
   {
      this.parameters = parameters;
   }

   /**
    * Sets the Enabled attribute of the JMSServer object
    *
    * @param dc                The new Enabled value
    * @param enabled           The new Enabled value
    * @exception JMSException  Description of Exception
    */
   public void setEnabled(ConnectionToken dc, boolean enabled) throws JMSException
   {
      ClientConsumer ClientConsumer = getClientConsumer(dc);
      ClientConsumer.setEnabled(enabled);
   }

   /**
    * Sets the StateManager attribute of the JMSServer object
    *
    * @param newStateManager  The new StateManager value
    */
   public void setStateManager(StateManager newStateManager)
   {
      stateManager = newStateManager;
   }

   /**
    * Sets the PersistenceManager attribute of the JMSServer object
    *
    * @param newPersistenceManager  The new PersistenceManager value
    */
   public void setPersistenceManager(org.jboss.mq.pm.PersistenceManager newPersistenceManager)
   {
      persistenceManager = newPersistenceManager;
   }

   /////////////////////////////////////////////////////////////////////
   // Public Methods
   /////////////////////////////////////////////////////////////////////

   /**
    * Returns <code>false</code> if the JMS server is currently running and
    * handling requests, <code>true</code> otherwise.
   *
    * @return   <code>false</code> if the JMS server is currently running and
    *      handling requests, <code>true</code> otherwise.
    */
   public boolean isStopped()
   {
      synchronized (stateLock)
      {
         return this.stopped;
      }
   }

   /**
    *
    * @return the current client count
    */
   public int getClientCount()
   {
      return clientConsumers.size();
   }
   /** Obtain a copy of the current clients
    * @return a HashMap<ConnectionToken, ClientConsumer> of current clients
    */
   public HashMap getClients()
   {
      return new HashMap(clientConsumers);
   }

   public ThreadGroup getThreadGroup()
   {
      return threadGroup;
   }

   //Get a new ClientID for a connection
   /**
    * Gets the ID attribute of the JMSServer object
    *
    * @return   The ID value
    */
   public String getID()
   {
      String ID = null;

      while (true)
      {
         try
         {
            synchronized (idLock)
            {
               ID = "ID:" + (new Integer(lastID++).toString());
            }
            stateManager.addLoggedOnClientId(ID);
            break;
         }
         catch (Exception e)
         {
         }
      }

      return ID;
   }

   /**
    * Gets the TemporaryTopic attribute of the JMSServer object
    *
    * @param dc                Description of Parameter
    * @return                  The TemporaryTopic value
    * @exception JMSException  Description of Exception
    */
   public TemporaryTopic getTemporaryTopic(ConnectionToken dc) throws JMSException
   {
      String topicName;
      synchronized (lastTemporaryTopicLock)
      {
         topicName = "JMS_TT" + (new Integer(lastTemporaryTopic++).toString());
      }
      SpyTemporaryTopic topic = new SpyTemporaryTopic(topicName, dc);

      ClientConsumer ClientConsumer = getClientConsumer(dc);
      JMSDestination queue = new JMSTopic(topic, ClientConsumer, this, parameters);
      destinations.put(topic, queue);

      return topic;
   }

   /**
    * Gets the TemporaryQueue attribute of the JMSServer object
    *
    * @param dc                Description of Parameter
    * @return                  The TemporaryQueue value
    * @exception JMSException  Description of Exception
    */
   public TemporaryQueue getTemporaryQueue(ConnectionToken dc) throws JMSException
   {
      String queueName;
      synchronized (lastTemporaryQueueLock)
      {
         queueName = "JMS_TQ" + (new Integer(lastTemporaryQueue++).toString());
      }
      SpyTemporaryQueue newQueue = new SpyTemporaryQueue(queueName, dc);

      ClientConsumer ClientConsumer = getClientConsumer(dc);
      JMSDestination queue = new JMSQueue(newQueue, ClientConsumer, this, parameters);
      destinations.put(newQueue, queue);

      return newQueue;
   }

   // Gets the ClientConsumers mapped the the connection
   // If the connection is not mapped, a new ClientConsumer is created
   /**
    * Gets the ClientConsumer attribute of the JMSServer object
    *
    * @param dc                Description of Parameter
    * @return                  The ClientConsumer value
    * @exception JMSException  Description of Exception
    */
   public ClientConsumer getClientConsumer(ConnectionToken dc) throws JMSException
   {
      ClientConsumer cq = (ClientConsumer) clientConsumers.get(dc);
      if (cq == null)
      {
         cq = new ClientConsumer(this, dc);
         clientConsumers.put(dc, cq);
      }
      return cq;
   }

   /**
    * Gets the JMSDestination attribute of the JMSServer object
    *
    * @param dest  Description of Parameter
    * @return      The JMSDestination value
    */
   public JMSDestination getJMSDestination(SpyDestination dest)
   {
      return (JMSDestination) destinations.get(dest);
   }

   /**
    * Gets the StateManager attribute of the JMSServer object
    *
    * @return   The StateManager value
    */
   public StateManager getStateManager()
   {
      return stateManager;
   }

   /**
    * Gets the PersistenceManager attribute of the JMSServer object
    *
    * @return   The PersistenceManager value
    */
   public org.jboss.mq.pm.PersistenceManager getPersistenceManager()
   {
      return persistenceManager;
   }

   /**
    * #Description of the Method
    */
   public void startServer()
   {
      synchronized (stateLock)
      {
         this.stopped = false;
      }
   }

   /**
    * #Description of the Method
    */
   public void stopServer()
   {
      synchronized (stateLock)
      {
         this.stopped = true;
         //Any cleanup work that needs doing should be done here

         //At the moment there is nothing to do due to the fact that the individual
         //parts of the JBossMQ (pm, ils etc) each have their own mbean service
         //which starts and stops them separately

         //We could wait in here for the client consumers to finish delivering any
         //messages they have, but it is not neccessary as the client acks will be
         //lost anyway.
         this.alive = false;
      }
   }

   /**
    * Check a clienID set by the client.
    *
    * To be a valid id it must not look like an internal JBossMQ id
    * and the state manager must allso allow it.
    *
    * @param ID                Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void checkID(String ID) throws JMSException
   {
      if (ID != null && ID.startsWith("ID"))
         throw new InvalidClientIDException("clientID is not a valid ID: " + ID);
      stateManager.addLoggedOnClientId(ID);
   }

   //A connection has sent a new message
   /**
    * Adds a feature to the Message attribute of the JMSServer object
    *
    * @param dc                The feature to be added to the Message attribute
    * @param val               The feature to be added to the Message attribute
    * @exception JMSException  Description of Exception
    */
   public void addMessage(ConnectionToken dc, SpyMessage val) throws JMSException
   {
      addMessage(dc, val, null);
   }

   //A connection has sent a new message
   /**
    * Adds a feature to the Message attribute of the JMSServer object
    *
    * @param dc                The feature to be added to the Message attribute
    * @param val               The feature to be added to the Message attribute
    * @param txId              The feature to be added to the Message attribute
    * @exception JMSException  Description of Exception
    */
   public void addMessage(ConnectionToken dc, SpyMessage val, Tx txId) throws JMSException
   {
      JMSDestination queue = (JMSDestination) destinations.get(val.getJMSDestination());
      if (queue == null)
         throw new InvalidDestinationException("This destination does not exist! " + val.getJMSDestination());

      // Reset any redelivered information
      val.setJMSRedelivered(false);
      val.header.jmsProperties.remove(SpyMessage.PROPERTY_REDELIVERY_COUNT);

      //Add the message to the queue
      val.setReadOnlyMode();
      queue.addMessage(val, txId);
   }

   /**
    * The following function performs a Unit Of Work.
    *
    * @param dc                Description of Parameter
    * @param t                 Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void transact(ConnectionToken dc, TransactionRequest t) throws JMSException
   {

      org.jboss.mq.pm.TxManager txManager = persistenceManager.getTxManager();
      if (t.requestType == TransactionRequest.ONE_PHASE_COMMIT_REQUEST)
      {

         Tx txId = txManager.createTx();

         try
         {

            if (t.messages != null)
            {
               for (int i = 0; i < t.messages.length; i++)
               {
                  addMessage(dc, t.messages[i], txId);
               }
            }

            if (t.acks != null)
            {
               for (int i = 0; i < t.acks.length; i++)
               {
                  acknowledge(dc, t.acks[i], txId);
               }
            }

            txManager.commitTx(txId);

         }
         catch (JMSException e)
         {
            log.debug("Exception occured, rolling back transaction: ", e);
            txManager.rollbackTx(txId);
            throw new SpyTransactionRolledBackException("Transaction was rolled back.", e);
         }
      }
      else if (t.requestType == TransactionRequest.TWO_PHASE_COMMIT_PREPARE_REQUEST)
      {

         Tx txId = txManager.createTx(dc, t.xid);
         try
         {

            if (t.messages != null)
            {
               for (int i = 0; i < t.messages.length; i++)
               {
                  addMessage(dc, t.messages[i], txId);
               }
            }

            if (t.acks != null)
            {
               for (int i = 0; i < t.acks.length; i++)
               {
                  acknowledge(dc, t.acks[i], txId);
               }
            }

         }
         catch (JMSException e)
         {
            log.debug("Exception occured, rolling back transaction: ", e);
            txManager.rollbackTx(txId);
            throw new SpyTransactionRolledBackException("Transaction was rolled back.", e);
         }
      }
      else if (t.requestType == TransactionRequest.TWO_PHASE_COMMIT_ROLLBACK_REQUEST)
      {

         Tx txId = txManager.getPrepared(dc, t.xid);
         txManager.rollbackTx(txId);

      }
      else if (t.requestType == TransactionRequest.TWO_PHASE_COMMIT_COMMIT_REQUEST)
      {

         Tx txId = txManager.getPrepared(dc, t.xid);
         txManager.commitTx(txId);

      }

   }

   //Sent by a client to Ack or Nack a message.
   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param item              Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void acknowledge(ConnectionToken dc, AcknowledgementRequest item) throws JMSException
   {
      acknowledge(dc, item, null);
   }

   //Sent by a client to Ack or Nack a message.
   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param item              Description of Parameter
    * @param txId              Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void acknowledge(ConnectionToken dc, AcknowledgementRequest item, Tx txId) throws JMSException
   {

      ClientConsumer cc = getClientConsumer(dc);
      cc.acknowledge(item, txId);

   }

   //A connection is closing [error or notification]
   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void connectionClosing(ConnectionToken dc) throws JMSException
   {
      if (dc == null)
         return;

      // Close it's ClientConsumer
      ClientConsumer cq = (ClientConsumer) clientConsumers.remove(dc);
      if (cq != null)
         cq.close();

      //unregister its clientID
      if (dc.getClientID() != null)
         stateManager.removeLoggedOnClientId(dc.getClientID());

      //Remove any temporary destinations the consumer may have created.
      Iterator i = destinations.entrySet().iterator();
      while (i.hasNext())
      {
         Map.Entry entry = (Map.Entry) i.next();
         JMSDestination sq = (JMSDestination) entry.getValue();
         ClientConsumer cc = sq.temporaryDestination;
         if (cc != null && dc.equals(cc.connectionToken))
         {
            i.remove();
            deleteTemporaryDestination(dc, sq);
         }
      }
      // Close the clientIL
      try
      {
         if (dc.clientIL != null)
            dc.clientIL.close();
      }
      catch (Exception ex)
      {
         // We skipp warning, to often the client will allways
         // have gone when we get here
         //log.warn("Could not close clientIL: " +ex,ex);
      }

   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void connectionFailure(ConnectionToken dc) throws JMSException
   {
      //We should try again :) This behavior should under control of a Failure-Plugin
      log.error("The connection to client " + dc.getClientID() + " failed.");
      connectionClosing(dc);
   }

   //A connection object wants to subscribe to a Destination
   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param sub               Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void subscribe(ConnectionToken dc, Subscription sub) throws JMSException
   {
      ClientConsumer clientConsumer = getClientConsumer(dc);
      clientConsumer.addSubscription(sub);
   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param subscriptionId    Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void unsubscribe(ConnectionToken dc, int subscriptionId) throws JMSException
   {
      ClientConsumer clientConsumer = getClientConsumer(dc);
      clientConsumer.removeSubscription(subscriptionId);
   }

   /**
    * #Description of the Method
    *
    * @param id                Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void destroySubscription(ConnectionToken dc, DurableSubscriptionID id) throws JMSException
   {
      getStateManager().setDurableSubscription(this, id, null);
   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param dest              Description of Parameter
    * @param selector          Description of Parameter
    * @return                  Description of the Returned Value
    * @exception JMSException  Description of Exception
    */
   public SpyMessage[] browse(ConnectionToken dc, Destination dest, String selector) throws JMSException
   {

      JMSDestination queue = (JMSDestination) destinations.get(dest);
      if (queue == null)
         throw new InvalidDestinationException("That destination does not exist! " + dest);
      if (!(queue instanceof JMSQueue))
         throw new JMSException("That destination is not a queue");

      return ((JMSQueue) queue).browse(selector);
   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param subscriberId      Description of Parameter
    * @param wait              Description of Parameter
    * @return                  Description of the Returned Value
    * @exception JMSException  Description of Exception
    */
   public SpyMessage receive(ConnectionToken dc, int subscriberId, long wait) throws JMSException
   {
      ClientConsumer clientConsumer = getClientConsumer(dc);
      SpyMessage msg = clientConsumer.receive(subscriberId, wait);
      return msg;
   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param name              Description of Parameter
    * @return                  Description of the Returned Value
    * @exception JMSException  Description of Exception
    */
   public Queue createQueue(ConnectionToken dc, String name) throws JMSException
   {
      SpyQueue newQueue = new SpyQueue(name);
      if (!destinations.containsKey(newQueue))
      {
         throw new JMSException("This destination does not exist !");
      }
      return newQueue;
   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param name              Description of Parameter
    * @return                  Description of the Returned Value
    * @exception JMSException  Description of Exception
    */
   public Topic createTopic(ConnectionToken dc, String name) throws JMSException
   {
      SpyTopic newTopic = new SpyTopic(name);
      if (!destinations.containsKey(newTopic))
      {
         throw new JMSException("This destination does not exist !");
      }
      return newTopic;
   }

   /**
    * #Description of the Method
    *
    * @param dc    Description of Parameter
    * @param dest  Description of Parameter
    */
   public void deleteTemporaryDestination(ConnectionToken dc, SpyDestination dest)
      throws JMSException
   {
      JMSDestination destination = (JMSDestination) destinations.get(dest);
      if (destination == null)
         throw new InvalidDestinationException("That destination does not exist! " + destination);

      destinations.remove(dest);
      deleteTemporaryDestination(dc, destination);
   }

   protected void deleteTemporaryDestination(ConnectionToken dc, JMSDestination destination)
      throws JMSException
   {
      try
      {
         destination.removeAllMessages();
      }
      catch (Exception e)
      {
         log.error("An exception happened while removing all messages from temporary destination " 
                   + destination.getSpyDestination().getName(), e);
      }

   }

   /**
    * #Description of the Method
    *
    * @param userName          Description of Parameter
    * @param password          Description of Parameter
    * @return                  Description of the Returned Value
    * @exception JMSException  Description of Exception
    */
   public String checkUser(String userName, String password) throws JMSException
   {
      return stateManager.checkUser(userName, password);
   }
   /**
    * authenticate user and return a session id. Same as checkID.
    *
    * @param ID                Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public String authenticate(String id, String password) throws JMSException
   {
      // do nothing
      return null;
   }
   // Administration calls
   /**
    * Adds a feature to the Destination attribute of the JMSServer object
    *
    * @param topic             The feature to be added to the Destination
    *      attribute
    * @param queue             The feature to be added to the Destination
    *      attribute
    * @exception JMSException  Description of Exception
    */
   public void addDestination(JMSDestination destination) throws JMSException
   {
      if (destinations.containsKey(destination.getSpyDestination()))
         throw new JMSException("This destination has already been added to the server!");

      //Add this new destination to the list
      destinations.put(destination.getSpyDestination(), destination);

      // Restore the messages
      if (destination instanceof JMSTopic)
      {
         Collection durableSubs =
            getStateManager().getDurableSubscriptionIdsForTopic((SpyTopic) destination.getSpyDestination());
         for (Iterator i = durableSubs.iterator(); i.hasNext();)
         {
            DurableSubscriptionID sub = (DurableSubscriptionID) i.next();
            log.debug("creating the durable subscription for :" + sub);
            ((JMSTopic) destination).createDurableSubscription(sub);
         }
      }
   }

   /**
    * Closed a destination that was opened previously
    *
    * @param dest              the destionation to close
    * @exception JMSException  Description of Exception
    */
   public void closeDestination(SpyDestination dest) throws JMSException
   {
      JMSDestination destination = (JMSDestination) destinations.get(dest);
      if (destination == null)
         throw new InvalidDestinationException("This destination is not open! " + dest);

      // Is it in use??
      if (destination.isInUse())
         throw new JMSException("The destination is being used.");

      //remove the destination from the list
      destinations.remove(dest);

      destination.close();
   }

   /**
    * #Description of the Method
    *
    * @return   Description of the Returned Value
    */
   public String toString()
   {
      return JBOSS_VESION;
   }

   /**
    * #Description of the Method
    *
    * @param dc                Description of Parameter
    * @param clientTime        Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void ping(ConnectionToken dc, long clientTime) throws JMSException
   {
      try
      {
         dc.clientIL.pong(System.currentTimeMillis());
      }
      catch (Exception e)
      {
         throw new SpyJMSException("Could not pong", e);
      }
   }

   /**
    * Gets the messageCache
    * @return Returns a MessageCache
    */
   public MessageCache getMessageCache()
   {
      return messageCache;
   }

   /**
    * Sets the messageCache
    * @param messageCache The messageCache to set
    */
   public void setMessageCache(MessageCache messageCache)
   {
      this.messageCache = messageCache;
   }

   public SpyTopic getDurableTopic(DurableSubscriptionID sub) throws JMSException
   {
      return getStateManager().getDurableTopic(sub);
   }

   public Subscription getSubscription(ConnectionToken dc, int subscriberId) throws JMSException
   {
      ClientConsumer clientConsumer = getClientConsumer(dc);
      return clientConsumer.getSubscription(subscriberId);
   }

   /**
    * Gets message counters of all configured destinations
    *
    * @return MessageCounter[]      message counter array sorted by name
    */
   public MessageCounter[] getMessageCounter()
   {
      TreeMap map = new TreeMap(); // for sorting

      Iterator i = destinations.values().iterator();

      while (i.hasNext())
      {
         JMSDestination dest = (JMSDestination) i.next();

         MessageCounter[] counter = dest.getMessageCounter();

         for (int j = 0; j < counter.length; j++)
         {
            // sorting order name + subscription + type
            String key =
               counter[j].getDestinationName()
                  + "-"
                  + counter[j].getDestinationSubscription()
                  + "-"
                  + (counter[j].getDestinationTopic() ? "Topic" : "Queue");

            map.put(key, counter[j]);
         }
      }

      return (MessageCounter[]) map.values().toArray(new MessageCounter[0]);
   }

   /**
    * Resets message counters of all configured destinations
    */
   public void resetMessageCounter()
   {
      Iterator i = destinations.values().iterator();

      while (i.hasNext())
      {
         JMSDestination dest = (JMSDestination) i.next();

         MessageCounter[] counter = dest.getMessageCounter();

         for (int j = 0; j < counter.length; j++)
         {
            counter[j].resetCounter();
         }
      }
   }
}
