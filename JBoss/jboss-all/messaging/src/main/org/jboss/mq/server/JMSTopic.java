/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.server;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.TreeMap;

import javax.jms.JMSException;

import org.jboss.mq.DestinationFullException;
import org.jboss.mq.DurableSubscriptionID;
import org.jboss.mq.SpyDestination;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.SpyTopic;
import org.jboss.mq.Subscription;

/**
 *  This class is a message queue which is stored (hashed by Destination) on the
 *  JMS provider
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @author     David Maplesden (David.Maplesden@orion.co.nz)
 * @created    August 16, 2001
 * @version    $Revision: 1.17.2.9 $
 */
public class JMSTopic extends JMSDestination
{

   //Hashmap of ExclusiveQueues
   HashMap durQueues = new HashMap();
   HashMap tempQueues = new HashMap();

   // Constructor ---------------------------------------------------
   public JMSTopic(SpyDestination dest, ClientConsumer temporary, JMSDestinationManager server, BasicQueueParameters parameters) throws JMSException
   {
      super(dest, temporary, server, parameters);
   }

   public void clientConsumerStopped(ClientConsumer clientConsumer)
   {
      synchronized (durQueues)
      {
         Iterator iter = durQueues.values().iterator();
         while (iter.hasNext())
         {
            ((BasicQueue) iter.next()).clientConsumerStopped(clientConsumer);
         }
      }
      synchronized (tempQueues)
      {
         Iterator iter = tempQueues.values().iterator();
         while (iter.hasNext())
         {
            ((BasicQueue) iter.next()).clientConsumerStopped(clientConsumer);
         }
      }
   }

   public void addSubscriber(Subscription sub) throws JMSException
   {
      SpyTopic topic = (SpyTopic) sub.destination;
      DurableSubscriptionID id = topic.getDurableSubscriptionID();

      if (id == null)
      {
         // create queue
         ExclusiveQueue q = new ExclusiveQueue(server, destination, sub, parameters);

         // create topic queue message counter
         q.createMessageCounter(destination.getName(), null, true, false, -1);

         synchronized (tempQueues)
         {
            tempQueues.put(sub, q);
         }
      }
      else
      {
         PersistentQueue q = null;
         synchronized (durQueues)
         {
            q = (PersistentQueue) durQueues.get(id);
         }

         // Check for a changed selector
         boolean selectorChanged = false;
         if (q != null)
         {
            String newSelector = sub.messageSelector;
            String oldSelector = null;
            if (q instanceof SelectorPersistentQueue)
               oldSelector = ((SelectorPersistentQueue) q).selectorString;
            if ((newSelector == null && oldSelector != null)
               || (newSelector != null && newSelector.equals(oldSelector) == false))
               selectorChanged = true;
         }

         if (q == null || //Brand new durable subscriber
         !q.destination.equals(topic) || selectorChanged)
         {
            //subscription changed to new topic
            server.getStateManager().setDurableSubscription(server, id, topic);
         }
      }
   }

   public void removeSubscriber(Subscription sub) throws JMSException
   {
      BasicQueue queue = null;
      SpyTopic topic = (SpyTopic) sub.destination;
      DurableSubscriptionID id = topic.getDurableSubscriptionID();
      if (id == null)
      {
         synchronized (tempQueues)
         {
            queue = (BasicQueue) tempQueues.get(sub);
         }
      }
      else
      {
         synchronized (durQueues)
         {
            queue = (BasicQueue) durQueues.get(id);
            //note DON'T remove
         }
      }
      // The queue may be null if the durable subscription
      // is destroyed before the consumer is unsubscribed!
      if (queue == null)
      {
         ((ClientConsumer) sub.clientConsumer).removeRemovedSubscription(sub.subscriptionId);
      }
      else
      {
         queue.removeSubscriber(sub);
      }
   }

   public void nackMessages(Subscription sub) throws JMSException
   {
      BasicQueue queue = null;
      SpyTopic topic = (SpyTopic) sub.destination;
      DurableSubscriptionID id = topic.getDurableSubscriptionID();
      if (id == null)
      {
         synchronized (tempQueues)
         {
            queue = (BasicQueue) tempQueues.get(sub);
         }
      }
      else
      {
         synchronized (durQueues)
         {
            queue = (BasicQueue) durQueues.get(id);
         }
      }
      if (queue != null)
      {
         queue.nackMessages(sub);
      }
   }

   void cleanupSubscription(Subscription sub)
   {
      //just try to remove from tempQueues, don't worry if its not there
      synchronized (tempQueues)
      {
         BasicQueue queue = (BasicQueue) tempQueues.remove(sub);
         try
         {
            if (queue != null)
               queue.removeAllMessages();
         }
         catch (JMSException e)
         {
            cat.debug("Error removing messages for subscription " + sub, e);
         }
      }
   }

   public void addReceiver(Subscription sub)
   {
      getQueue(sub).addReceiver(sub);
   }

   public void removeReceiver(Subscription sub)
   {
      getQueue(sub).removeReceiver(sub);
   }

   public void restoreMessage(MessageReference messageRef)
   {
      try
      {
         SpyMessage spyMessage = messageRef.getMessage();
         synchronized (this)
         {
            messageIdCounter = Math.max(messageIdCounter, spyMessage.header.messageId + 1);
         }
         if (spyMessage.header.durableSubscriberID == null)
         {
            cat.debug("Trying to restore message with null durableSubscriberID");
         }
         else
         {
            BasicQueue queue = ((BasicQueue) durQueues.get(spyMessage.header.durableSubscriberID));
            messageRef.queue = queue;
            queue.restoreMessage(messageRef);
         }
      }
      catch (JMSException e)
      {
         cat.error("Could not restore message:", e);
      }
   }

   public void restoreMessage(SpyMessage message)
   {
      try
      {
         synchronized (this)
         {
            messageIdCounter = Math.max(messageIdCounter, message.header.messageId + 1);
         }
         if (message.header.durableSubscriberID == null)
         {
            cat.debug("Trying to restore message with null durableSubscriberID");
         }
         else
         {
            BasicQueue queue = (BasicQueue) durQueues.get(message.header.durableSubscriberID);
            MessageReference messageRef = server.getMessageCache().add(message, queue, MessageReference.STORED);
            queue.restoreMessage(messageRef);
         }
      }
      catch (JMSException e)
      {
         cat.error("Could not restore message:", e);
      }
   }

   //called by state manager when a durable sub is created
   public void createDurableSubscription(DurableSubscriptionID id) throws JMSException
   {
      if (temporaryDestination != null)
      {
         throw new JMSException("Not a valid operation on a temporary topic");
      }

      SpyTopic dstopic = new SpyTopic((SpyTopic) destination, id);

      // Create a 
      BasicQueue queue;
      if (id.getSelector() == null)
      {
         queue = new PersistentQueue(server, dstopic, parameters);
      }
      else
      {
         queue = new SelectorPersistentQueue(server, dstopic, id.getSelector(), parameters);
      }

      // create topic queue message counter
      queue.createMessageCounter(destination.getName(), id.toString(), true, true, -1);

      synchronized (durQueues)
      {
         durQueues.put(id, queue);
      }
      server.getPersistenceManager().restoreQueue(this, dstopic);
   }

   //called by JMSServer when a destination is being closed.
   public void close() throws JMSException
   {
      if (temporaryDestination != null)
      {
         throw new JMSException("Not a valid operation on a temporary topic");
      }

      synchronized (durQueues)
      {
         Iterator i = durQueues.values().iterator();
         while (i.hasNext())
         {
            PersistentQueue queue = (PersistentQueue) i.next();
            server.getPersistenceManager().closeQueue(this, queue.getSpyDestination());
         }
      }
   }

   //called by state manager when a durable sub is deleted
   public void destroyDurableSubscription(DurableSubscriptionID id) throws JMSException
   {
      BasicQueue queue;
      synchronized (durQueues)
      {
         queue = (BasicQueue) durQueues.remove(id);
      }
      queue.removeAllMessages();
   }

   public SpyMessage receive(Subscription sub, boolean wait) throws javax.jms.JMSException
   {
      return getQueue(sub).receive(sub, wait);
   }

   public void acknowledge(org.jboss.mq.AcknowledgementRequest req, Subscription sub, org.jboss.mq.pm.Tx txId)
      throws JMSException
   {
      getQueue(sub).acknowledge(req, txId);
   }

   public void addMessage(SpyMessage message, org.jboss.mq.pm.Tx txId) throws JMSException
   {
      StringBuffer errorMessage = null;

      //Number the message so that we can preserve order of delivery.
      long messageId = 0;
      synchronized (this)
      {
         messageId = messageIdCounter++;
         synchronized (durQueues)
         {
            Iterator iter = durQueues.keySet().iterator();
            while (iter.hasNext())
            {
               DurableSubscriptionID id = (DurableSubscriptionID) iter.next();
               PersistentQueue q = (PersistentQueue) durQueues.get(id);
               SpyMessage clone = message.myClone();
               clone.header.durableSubscriberID = id;
               clone.header.messageId = messageId;
               clone.setJMSDestination(q.getSpyDestination());
               MessageReference ref = server.getMessageCache().add(clone, q, MessageReference.NOT_STORED);
               try
               {
                  q.addMessage(ref, txId);
               }
               catch (DestinationFullException e)
               {
                  if (errorMessage == null)
                     errorMessage = new StringBuffer(e.getText());
                  else
                     errorMessage.append(", ").append(e.getText());
               }
            }
         }
         synchronized (tempQueues)
         {
            Iterator iter = tempQueues.values().iterator();
            while (iter.hasNext())
            {
               BasicQueue q = (BasicQueue) iter.next();
               SpyMessage clone = message.myClone();
               clone.header.messageId = messageId;
               MessageReference ref = server.getMessageCache().add(clone, q, MessageReference.NOT_STORED);
               try
               {
                  q.addMessage(ref, txId);
               }
               catch (DestinationFullException e)
               {
                  if (errorMessage == null)
                     errorMessage = new StringBuffer(e.getText());
                  else
                     errorMessage.append(", ").append(e.getText());
               }
            }
         }
      }
      if (errorMessage != null)
         throw new DestinationFullException(errorMessage.toString());
   }

   public int getAllMessageCount()
   {
      return calculateMessageCount(getAllQueues());
   }

   public int getDurableMessageCount()
   {
      return calculateMessageCount(getPersistentQueues());
   }

   public int getNonDurableMessageCount()
   {
      return calculateMessageCount(getTemporaryQueues());
   }

   public ArrayList getAllQueues()
   {
      ArrayList result = new ArrayList(getAllSubscriptionsCount());
      result.addAll(getPersistentQueues());
      result.addAll(getTemporaryQueues());
      return result;
   }

   public ArrayList getTemporaryQueues()
   {
      synchronized (tempQueues)
      {
         return new ArrayList(tempQueues.values());
      }
   }

   public ArrayList getPersistentQueues()
   {
      synchronized (durQueues)
      {
         return new ArrayList(durQueues.values());
      }
   }

   public int getAllSubscriptionsCount()
   {
      return durQueues.size() + tempQueues.size();
   }

   public int getDurableSubscriptionsCount()
   {
      return durQueues.size();
   }

   public int getNonDurableSubscriptionsCount()
   {
      return tempQueues.size();
   }

   public ArrayList getAllSubscriptions()
   {
      ArrayList result = new ArrayList(getAllSubscriptionsCount());
      result.addAll(getDurableSubscriptions());
      result.addAll(getNonDurableSubscriptions());
      return result;
   }

   public ArrayList getDurableSubscriptions()
   {
      synchronized (durQueues)
      {
         return new ArrayList(durQueues.keySet());
      }
   }

   public ArrayList getNonDurableSubscriptions()
   {
      synchronized (tempQueues)
      {
         return new ArrayList(tempQueues.keySet());
      }
   }

   // Package protected ---------------------------------------------
   PersistentQueue getDurableSubscription(DurableSubscriptionID id)
   {
      synchronized (durQueues)
      {
         return (PersistentQueue) durQueues.get(id);
      }
   }

   private BasicQueue getQueue(Subscription sub)
   {
      SpyTopic topic = (SpyTopic) sub.destination;
      DurableSubscriptionID id = topic.getDurableSubscriptionID();
      if (id != null)
      {
         return getDurableSubscription(id);
      }
      else
      {
         synchronized (tempQueues)
         {
            return (BasicQueue) tempQueues.get(sub);
         }
      }
   }

   /*
    * @see JMSDestination#isInUse()
    */
   public boolean isInUse()
   {
      if (tempQueues.size() > 0)
         return true;
      Iterator iter = durQueues.values().iterator();
      while (iter.hasNext())
      {
         PersistentQueue q = (PersistentQueue) iter.next();
         if (q.isInUse())
            return true;
      }
      return false;
   }
   /**
    * @see JMSDestination#destroy()
    */
   public void removeAllMessages() throws JMSException
   {
      synchronized (durQueues)
      {
         Iterator i = durQueues.values().iterator();
         while (i.hasNext())
         {
            PersistentQueue queue = (PersistentQueue) i.next();
            queue.removeAllMessages();
         }
      }
   }

   private int calculateMessageCount(ArrayList queues)
   {
      int count = 0;
      for (Iterator i = queues.listIterator(); i.hasNext();)
      {
         BasicQueue queue = (BasicQueue) i.next();
         count += queue.getQueueDepth();
      }
      return count;
   }

   /**
    * Get message counter of all topic internal queues
    * 
    * @return MessageCounter[]  topic queue message counter array
    */
   public MessageCounter[] getMessageCounter()
   {
      TreeMap map = new TreeMap();

      synchronized (durQueues)
      {
         Iterator i = durQueues.values().iterator();

         while (i.hasNext())
         {
            BasicQueue queue = (BasicQueue) i.next();
            MessageCounter counter = queue.getMessageCounter();

            if (counter != null)
            {
               String key = counter.getDestinationName() + counter.getDestinationSubscription();

               map.put(key, counter);
            }
         }
      }

      synchronized (tempQueues)
      {
         Iterator i = tempQueues.values().iterator();

         while (i.hasNext())
         {
            BasicQueue queue = (BasicQueue) i.next();
            MessageCounter counter = queue.getMessageCounter();

            if (counter != null)
            {
               String key = counter.getDestinationName() + counter.getDestinationSubscription();

               map.put(key, counter);
            }
         }
      }

      return (MessageCounter[]) map.values().toArray(new MessageCounter[0]);
   }
}
