/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.server;

import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.SortedSet;
import java.util.TreeSet;

import javax.jms.JMSException;

import org.jboss.logging.Logger;
import org.jboss.mq.AcknowledgementRequest;
import org.jboss.mq.DestinationFullException;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.Subscription;
import org.jboss.mq.selectors.Selector;

/**
 *  This class represents a queue which provides it's messages exclusively to one
 *  consumer at a time.<p>
 *
 * Notes about synchronization: Much of the work is synchronized on
 * the receivers or messages depending on the work performed.
 * However, anything to do with unacknowledged messages and removed
 * subscriptions must be done synchronized on both (receivers first).
 * This is because there are multiple entry points with the possibility
 * that a message acknowledgement (or NACK) is being processed at
 * the same time as a network failure removes the subscription.
 *
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     David Maplesden (David.Maplesden@orion.co.nz)
 * @author     Adrian Brock (Adrian@jboss.org)
 * @created    August 16, 2001
 * @version    $Revision: 1.20.2.19 $
 */
public class BasicQueue
{
   // Constants -----------------------------------------------------

   static final Logger log = Logger.getLogger(BasicQueue.class);

   // Attributes ----------------------------------------------------

   /** List of messages waiting to be dispatched<p>
       synchronized access on itself */
   SortedSet messages = new TreeSet();

   /** Timer which executes a scheduled message (daemon) */
   SimpleTimer messageTimer = new SimpleTimer();
   /** Estimate of number of tasks on the timer (for the size estimate)<p>
       synchronized access on the message timer */
   int scheduledMessageCount = 0;

   /** The JMSServer object */
   JMSDestinationManager server;

   /** The subscribers waiting for messages<p>
       synchronized access on itself */
   HashSet receivers = new HashSet();

   /** The description used to seperate persistence for multiple subscriptions to a topic */
   String description;

   /** Simple Counter for gathering message add statistic data */
   MessageCounter counter;

   /** Unacknowledged messages AcknowledgementRequest -> UnackedMessageInfo<p>
       synchronized access on receivers and messages */
   HashMap unacknowledgedMessages = new HashMap();
   /** Unacknowledged messages MessageRef -> UnackedMessageInfo <p>
       synchronized access on receivers and messages */
   HashMap unackedByMessageRef = new HashMap();
   /** Unacknowledged messages Subscription -> UnackedMessageInfo <p>
       synchronized access on receivers and messages */
   HashMap unackedBySubscription = new HashMap();

   /** Removed subscribers <p>
       synchronized access on receivers and messages */
   HashSet removedSubscribers = new HashSet();
   
   /** The basic queue parameters */
   BasicQueueParameters parameters;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new basic queue
    * 
    * @param server the destination manager
    * @param description a description to uniquely identify the queue
    * @param parameters the basic queue parameters
    */
   public BasicQueue(JMSDestinationManager server, String description, BasicQueueParameters parameters)
      throws JMSException
   {
      this.server = server;
      this.description = description;
      this.parameters = parameters;
   }

   // Public --------------------------------------------------------

   /**
    * Retrieve the unique description for this queue
    *
    * @return the description
    */
   public String getDescription()
   {
      return description;
   }

   /**
    * Retrieve the number of receivers waiting for a message
    *
    * @return the number of receivers
    */
   public int getReceiversCount()
   {
      return receivers.size();
   }

   /**
    * Retrieve the receivers waiting for a message
    *
    * @return an array of subscriptions
    */
   public ArrayList getReceivers()
   {
      synchronized (receivers)
      {
         return new ArrayList(receivers);
      }
   }

   /**
    * Test whether the queue is in use
    *
    * @return true when there are waiting receivers
    */
   public boolean isInUse()
   {
      synchronized (receivers)
      {
         return receivers.size() > 0;
      }
   }

   /**
    * Add a receiver to the queue
    *
    * @param sub the subscription to add
    */
   public void addReceiver(Subscription sub)
   {
      boolean trace = log.isTraceEnabled();

      synchronized (messages)
      {
         if (messages.size() != 0)
         {
            for (Iterator it = messages.iterator(); it.hasNext();)
            {
               MessageReference message = (MessageReference) it.next();
               try
               {
                  if (message.isExpired())
                  {
                     it.remove();
                     if (trace)
                        log.trace("message expired: " + message);
                     dropMessage(message);
                  }
                  else if (sub.accepts(message.getHeaders()))
                  {
                     //queue message for sending to this sub
                     queueMessageForSending(sub, message);
                     it.remove();
                     return;
                  }
               }
               catch (JMSException ignore)
               {
                  log.info("Caught unusual exception in addToReceivers.", ignore);
               }
            }
         }
      }
      addToReceivers(sub);
   }

   /**
    * Removes a subscription from the queue
    *
    * @param sub the subscription to remove
    */
   public void removeSubscriber(Subscription sub)
   {
      boolean trace = log.isTraceEnabled();
      removeReceiver(sub);
      synchronized (receivers)
      {
         synchronized (messages)
         {
            if (hasUnackedMessages(sub))
            {
               if (trace)
                  log.trace("Delaying removal of subscriber is has unacked messages " + sub);
               removedSubscribers.add(sub);
            }
            else
            {
               if (trace)
                  log.trace("Removing subscriber " + sub);
               ((ClientConsumer) sub.clientConsumer).removeRemovedSubscription(sub.subscriptionId);
            }
         }
      }
   }

   /**
    * Process a notification that a client is stopping
    *
    * @param clientConsumer the client that is stopping
    */
   public void clientConsumerStopped(ClientConsumer clientConsumer)
   {
      //remove all waiting subs for this clientConsumer and send to its blocked list.
      synchronized (receivers)
      {
         for (Iterator it = receivers.iterator(); it.hasNext();)
         {
            Subscription sub = (Subscription) it.next();
            if (sub.clientConsumer.equals(clientConsumer))
            {
               clientConsumer.addBlockedSubscription(sub, 0);
               it.remove();
            }
         }
      }
   }

   /**
    * Retrieve the queue depth
    *
    * @return the number of messages in the queue
    */
   public int getQueueDepth()
   {
      return messages.size();
   }

   /**
    * Returns the number of scheduled messages in the queue
    */
   public int getScheduledMessageCount()
   {
      return scheduledMessageCount;
   }

   /**
    * Add a message to the queue
    *
    * @param mes the message reference
    * @param txId the transaction
    * @throws JMSException for any error
    */
   public void addMessage(MessageReference mes, org.jboss.mq.pm.Tx txId) throws JMSException
   {
      if (parameters.maxDepth > 0)
      {
         synchronized (messages)
         {
            if (messages.size() >= parameters.maxDepth)
            {
               dropMessage(mes);
               String message = "Maximum size " + parameters.maxDepth +
                  " exceeded for " + description;
               log.warn(message);
               throw new DestinationFullException(message);
            }
         }
      }

      // The message is removed from the cache on a rollback
      Runnable task = new AddMessagePostRollBackTask(mes);
      server.getPersistenceManager().getTxManager().addPostRollbackTask(txId, task);

      // The message gets added to the queue after the transaction commits
      task = new AddMessagePostCommitTask(mes);
      server.getPersistenceManager().getTxManager().addPostCommitTask(txId, task);
   }

   /**
    * Restores a message.
    */
   public void restoreMessage(MessageReference mes)
   {
      internalAddMessage(mes);
   }

   /**
    * Browse the queue
    *
    * @param selector the selector to apply, pass null for
    *                 all messages
    * @throws JMSException for any error
    */
   public SpyMessage[] browse(String selector) throws JMSException
   {
      if (selector == null)
      {
         SpyMessage list[];
         synchronized (messages)
         {
            list = new SpyMessage[messages.size()];
            Iterator iter = messages.iterator();
            for (int i = 0; iter.hasNext(); i++)
               list[i] = ((MessageReference) iter.next()).getMessage();
         }
         return list;
      }
      else
      {
         Selector s = new Selector(selector);
         LinkedList selection = new LinkedList();

         synchronized (messages)
         {
            Iterator i = messages.iterator();
            while (i.hasNext())
            {
               MessageReference m = (MessageReference) i.next();
               if (s.test(m.getHeaders()))
                  selection.add(m.getMessage());
            }
         }

         SpyMessage list[];
         list = new SpyMessage[selection.size()];
         list = (SpyMessage[]) selection.toArray(list);
         return list;
      }
   }

   /**
    * Receive a message from the queue
    *
    * @param the subscription requiring a message
    * @param wait whether to wait for a message
    * @throws JMSException for any error
    */
   public SpyMessage receive(Subscription sub, boolean wait) throws JMSException
   {
      boolean trace = log.isTraceEnabled();

      MessageReference messageRef = null;
      synchronized (receivers)
      {
         // If the subscription is not picky, the first message will be it
         if (sub.getSelector() == null && sub.noLocal == false)
         {
            synchronized (messages)
            {
               // find a non-expired message
               while (messages.size() != 0)
               {
                  messageRef = (MessageReference) messages.first();
                  messages.remove(messageRef);

                  if (messageRef.isExpired())
                  {
                     if (trace)
                        log.trace("message expired: " + messageRef);
                     dropMessage(messageRef);
                     messageRef = null;
                  }
                  else
                     break;
               }
            }
         }
         else
         {
            // The subscription is picky, so we have to iterate.
            synchronized (messages)
            {
               Iterator i = messages.iterator();
               while (i.hasNext())
               {
                  MessageReference mr = (MessageReference) i.next();
                  if (mr.isExpired())
                  {
                     i.remove();
                     if (trace)
                        log.trace("message expired: " + mr);
                     dropMessage(mr);
                  }
                  else if (sub.accepts(mr.getHeaders()))
                  {
                     messageRef = mr;
                     i.remove();
                     break;
                  }
               }
            }
         }

         if (messageRef == null)
         {
            if (wait)
               addToReceivers(sub);
         }
         else
         {
            setupMessageAcknowledgement(sub, messageRef);
         }
      }

      if (messageRef == null)
         return null;
      return messageRef.getMessage();
   }

   /**
    * Acknowledge a message
    *
    * @param item the acknowledgement request
    * @param txid the transaction
    * @throws JMSException for any error
    */
   public void acknowledge(AcknowledgementRequest item, org.jboss.mq.pm.Tx txId) throws javax.jms.JMSException
   {
      UnackedMessageInfo unacked = null;

      synchronized (messages)
      {
         unacked = (UnackedMessageInfo) unacknowledgedMessages.remove(item);
         if (unacked == null)
            return;
         unackedByMessageRef.remove(unacked.messageRef);
         HashMap map = (HashMap) unackedBySubscription.get(unacked.sub);
         map.remove(unacked.messageRef);
         if (map.isEmpty())
            unackedBySubscription.remove(unacked.sub);
      }

      MessageReference m = unacked.messageRef;

      // Was it a negative acknowledge??
      if (!item.isAck)
      {
         Runnable task = new RestoreMessageTask(m);
         server.getPersistenceManager().getTxManager().addPostCommitTask(txId, task);

      }
      else
      {
         if (m.isPersistent())
            server.getPersistenceManager().remove(m, txId);

         Runnable task = new RestoreMessageTask(m);
         server.getPersistenceManager().getTxManager().addPostRollbackTask(txId, task);

         task = new RemoveMessageTask(m);
         server.getPersistenceManager().getTxManager().addPostCommitTask(txId, task);
      }

      synchronized (receivers)
      {
         synchronized (messages)
         {
            checkRemovedSubscribers(unacked.sub);
         }
      }
   }

   /**
    * Nack all messages for a subscription
    *
    * @param sub the subscription
    */
   public void nackMessages(Subscription sub)
   {
      // Send nacks for unacknowledged messages
      synchronized (receivers)
      {
         synchronized (messages)
         {
            int count = 0;
            HashMap map = (HashMap) unackedBySubscription.get(sub);
            if (map != null)
            {
               Iterator i = ((HashMap) map.clone()).values().iterator();
               while (i.hasNext())
               {
                  AcknowledgementRequest item = (AcknowledgementRequest) i.next();
                  try
                  {
                     acknowledge(item, null);
                     count++;
                  }
                  catch (JMSException ignore)
                  {
                     log.debug("Unable to nack message: " + item, ignore);
                  }
               }
               if (log.isDebugEnabled())
                  log.debug("Nacked " + count + " messages for removed subscription " + sub);
            }
         }
      }
   }

   public void removeAllMessages() throws JMSException
   {
      // Cancel scheduled messages
      messageTimer.clear();
      scheduledMessageCount = 0;

      synchronized (receivers)
      {
         synchronized (messages)
         {
            Iterator i = ((HashMap) unacknowledgedMessages.clone()).keySet().iterator();
            while (i.hasNext())
            {
               AcknowledgementRequest item = (AcknowledgementRequest) i.next();
               try
               {
                  acknowledge(item, null);
               }
               catch (JMSException ignore)
               {
               }
            }

            // Remove all remaining messages
            i = messages.iterator();
            while (i.hasNext())
            {
               MessageReference message = (MessageReference) i.next();
               i.remove();
               dropMessage(message);
            }
         }
      }
   }

   /**
    * Create message counter object
    * 
    * @param name             topic/queue name
    * @param subscription     topic subscription
    * @param topic            topic flag
    * @param durable          durable subscription flag
    * @param daycountmax      message history day count limit
    *                           0: disabled,
    *                          >0: max day count,
    *                          <0: unlimited
    */
   public void createMessageCounter(String name, String subscription, boolean topic, boolean durable, int daycountmax)
   {
      // create message counter object
      counter = new MessageCounter(name, subscription, this, topic, durable, daycountmax);
   }

   /**
    * Get message counter object
    *
    * @return MessageCounter     message counter object or null 
    */
   public MessageCounter getMessageCounter()
   {
      return counter;
   }

   // Protected -----------------------------------------------------

   /**
    * Add a receiver
    *
    * @param sub the receiver to add
    */
   protected void addToReceivers(Subscription sub)
   {
      synchronized (receivers)
      {
         receivers.add(sub);
      }
   }

   /**
    * Remove a receiver
    *
    * @param sub the receiver to remove
    */
   protected void removeReceiver(Subscription sub)
   {
      synchronized (receivers)
      {
         receivers.remove(sub);
      }
   }

   /**
    * Add a message
    *
    * @param message the message to add
    */
   private void internalAddMessage(MessageReference message)
   {
      boolean trace = log.isTraceEnabled();

      // If scheduled, put in timer queue
      long ts = message.messageScheduledDelivery;
      if (ts > 0 && ts > System.currentTimeMillis())
      {
         EnqueueMessageTask t = new EnqueueMessageTask(message);
         messageTimer.schedule(t, ts);
         synchronized (messageTimer)
         {
            scheduledMessageCount++;
         }
         if (trace)
            log.trace("scheduled message at " + new Date(ts) + ": " + message);
         // Can't deliver now
         return;
      }

      // Don't bother with expired messages
      if (message.isExpired())
      {
         if (trace)
            log.trace("message expired: " + message);
         dropMessage(message);
         return;
      }

      try
      {
         synchronized (receivers)
         {
            if (receivers.isEmpty() == false)
            {
               for (Iterator it = receivers.iterator(); it.hasNext();)
               {
                  Subscription sub = (Subscription) it.next();
                  if (sub.accepts(message.getHeaders()))
                  {
                     //queue message for sending to this sub
                     queueMessageForSending(sub, message);
                     it.remove();
                     return;
                  }
               }
            }

            synchronized (messages)
            {
               messages.add(message);

               // If a message is set to expire, and nobody wants it, put its reaper in
               // the timer queue
               if (message.messageExpiration > 0)
               {
                  // This could be a waste of memory for large numbers of expirations
                  ExpireMessageTask t = new ExpireMessageTask(message);
                  messageTimer.schedule(t, message.messageExpiration);
               }
            }
         }
      }
      catch (JMSException e)
      {
         // Could happen at the accepts() calls
         log.error("Caught unusual exception in internalAddMessage.", e);
         // And drop the message, otherwise we have a leak in the cache
         dropMessage(message);
      }
   }

   /**
    * Queue a message for sending through the client consumer
    *
    * @param sub the subscirption to receive the message
    * @param message the message reference to queue
    */
   protected void queueMessageForSending(Subscription sub, MessageReference message) throws JMSException
   {
      setupMessageAcknowledgement(sub, message);
      RoutedMessage r = new RoutedMessage();
      r.message = message;
      r.subscriptionId = new Integer(sub.subscriptionId);
      ((ClientConsumer) sub.clientConsumer).queueMessageForSending(r);
   }

   /**
    * Setup a message acknowledgement
    *
    * @param sub the subscription receiving the message
    * @param messageRef the message to be acknowledged
    * @throws JMSException for any error
    */
   protected void setupMessageAcknowledgement(Subscription sub, MessageReference messageRef) throws JMSException
   {
      SpyMessage message = messageRef.getMessage();
      AcknowledgementRequest ack = new AcknowledgementRequest();
      ack.destination = message.getJMSDestination();
      ack.messageID = message.getJMSMessageID();
      ack.subscriberId = sub.subscriptionId;
      ack.isAck = false;

      synchronized (messages)
      {
         UnackedMessageInfo unacked = new UnackedMessageInfo(messageRef, sub);
         unacknowledgedMessages.put(ack, unacked);
         unackedByMessageRef.put(messageRef, ack);
         HashMap map = (HashMap) unackedBySubscription.get(sub);
         if (map == null)
         {
            map = new HashMap();
            unackedBySubscription.put(sub, map);
         }
         map.put(messageRef, ack);
      }
   }

   /**
    * Remove a message
    *
    * @param message the message to remove
    */
   protected void dropMessage(MessageReference message)
   {
      try
      {
         if (message.isPersistent())
         {
            try
            {
               server.getPersistenceManager().remove(message, null);
            }
            catch (JMSException e)
            {
               try
               {
                  log.warn("Message removed from queue, but not from the persistent store: " + message.getMessage(), e);
               }
               catch (JMSException x)
               {
                  log.warn("Message removed from queue, but not from the persistent store: " + message, e);
               }
            }
         }
         server.getMessageCache().remove(message);
      }
      catch (JMSException e)
      {
         log.warn("Error dropping message " + message, e);
      }
   }

   // Package Private -----------------------------------------------

   // Private -------------------------------------------------------

   /**
    * Check whether a removed subscription can be permenantly removed.
    * This method is private because it assumes external synchronization
    *
    * @param the subscription to check
    */
   private void checkRemovedSubscribers(Subscription sub)
   {
      boolean trace = log.isTraceEnabled();
      if (removedSubscribers.contains(sub) && hasUnackedMessages(sub) == false)
      {
         if (trace)
            log.trace("Removing subscriber " + sub);
         removedSubscribers.remove(sub);
         ((ClientConsumer) sub.clientConsumer).removeRemovedSubscription(sub.subscriptionId);
      }
   }

   /**
    * Check whether a subscription has unacknowledged messages.
    * This method is private because it assumes external synchronization
    *
    * @param sub the subscription to check
    * @return true when it has unacknowledged messages
    */
   private boolean hasUnackedMessages(Subscription sub)
   {
      return unackedBySubscription.containsKey(sub);
   }

   // Inner classes -------------------------------------------------

   /**
    * Rollback an add message
    */
   class AddMessagePostRollBackTask implements Runnable
   {
      MessageReference message;

      AddMessagePostRollBackTask(MessageReference m)
      {
         message = m;
      }

      public void run()
      {
         try
         {
            server.getMessageCache().remove(message);
         }
         catch (JMSException e)
         {
            log.error("Could not remove message from the message cache after an add rollback: ", e);
         }
      }
   }

   /**
    * Add a message to the queue
    */
   class AddMessagePostCommitTask implements Runnable
   {
      MessageReference message;

      AddMessagePostCommitTask(MessageReference m)
      {
         message = m;
      }

      public void run()
      {
         internalAddMessage(message);

         // update message counter
         if (counter != null)
         {
            counter.incrementCounter();
         }
      }
   }

   /**
    * Restore a message to the queue
    */
   class RestoreMessageTask implements Runnable
   {
      MessageReference message;

      RestoreMessageTask(MessageReference m)
      {
         message = m;
      }

      public void run()
      {
         if (log.isTraceEnabled())
            log.trace("Restoring message: " + message);

         try
         {
            SpyMessage spyMessage = message.getMessage();

            // Set redelivered, vendor-specific flags
            spyMessage.setJMSRedelivered(true);
            if (spyMessage.propertyExists(SpyMessage.PROPERTY_REDELIVERY_DELAY))
            {
               log.trace("message has redelivery delay");
               long delay = spyMessage.getLongProperty(SpyMessage.PROPERTY_REDELIVERY_DELAY);
               message.messageScheduledDelivery = System.currentTimeMillis() + delay;
            }

            if (spyMessage.propertyExists(SpyMessage.PROPERTY_REDELIVERY_COUNT) == false)
               spyMessage.header.jmsProperties.put(SpyMessage.PROPERTY_REDELIVERY_COUNT, new Integer(1));
            else
            {
               int c = spyMessage.getIntProperty(SpyMessage.PROPERTY_REDELIVERY_COUNT);
               spyMessage.header.jmsProperties.put(SpyMessage.PROPERTY_REDELIVERY_COUNT, new Integer(c + 1));
            }

            message.invalidate();
            // Update the persistent message outside the transaction
            // We want to know the message might have been delivered regardless
            if (message.isPersistent())
               server.getPersistenceManager().update(message, null);
         }
         catch (JMSException e)
         {
            log.error("Caught unusual exception in restoreMessageTask.", e);
         }

         internalAddMessage(message);
      }
   }

   /**
    * Remove a message
    */
   class RemoveMessageTask implements Runnable
   {
      MessageReference message;

      RemoveMessageTask(MessageReference m)
      {
         message = m;
      }

      public void run()
      {
         try
         {
            server.getMessageCache().remove(message);
         }
         catch (JMSException e)
         {
            log.error("Could not remove an acknowleged message from the message cache: ", e);
         }
      }
   }

   /**
    * Schedele message delivery
    */
   private class EnqueueMessageTask extends SimpleTimerTask
   {
      private MessageReference messageRef;

      public EnqueueMessageTask(MessageReference messageRef)
      {
         this.messageRef = messageRef;
      }

      public void run()
      {
         if (log.isTraceEnabled())
            log.trace("running message: " + messageRef);
         internalAddMessage(messageRef);
         synchronized (messageTimer)
         {
            scheduledMessageCount--;
         }
      }
   }

   /**
    * Drop a message when it expires
    */
   private class ExpireMessageTask extends SimpleTimerTask
   {
      private MessageReference messageRef;

      public ExpireMessageTask(MessageReference messageRef)
      {
         this.messageRef = messageRef;
      }

      public void run()
      {
         synchronized (messages)
         {
            // If the message was already sent, then do nothing
            // (This probably happens more than not)
            if (messages.remove(messageRef) == false)
               return;
         }
         if (log.isTraceEnabled())
            log.trace("message expired: " + messageRef);
         dropMessage(messageRef);
      }
   }

   /**
    * Information about unacknowledged messages
    */
   private static class UnackedMessageInfo
   {
      public MessageReference messageRef;
      public Subscription sub;
      public UnackedMessageInfo(MessageReference messageRef, Subscription sub)
      {
         this.messageRef = messageRef;
         this.sub = sub;
      }
   }
}
