/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.server;

import java.lang.ref.SoftReference;

import javax.jms.DeliveryMode;
import javax.jms.JMSException;

import org.jboss.logging.Logger;
import org.jboss.mq.MessagePool;
import org.jboss.mq.SpyMessage;

/**
 * This class holds a reference to an actual Message.  Where it is actually
 * at may vary.  The reference it holds may be a:
 * <ul>
 * <li>Hard Reference - The message is consider recently used and should not be paged out
 * <li>Soft Reference - The message is consider old and CAN be removed from memory by the GC
 * <li>No Reference - The message was removed from memory by the GC, but we can load it from a file.
 * </ul>
 *
 * @author <a href="mailto:hiram.chirino@jboss.org">Hiram Chirino</a>
 * @author <a href="mailto:pra@tim.se">Peter Antman</a>
 * @version    $Revision: 1.10.2.7 $
 */
public class MessageReference implements Comparable
{
   static Logger log = Logger.getLogger(MessageReference.class);

   /**
    * The message is not persisted
    */
   public static final int NOT_STORED = 1;

   /**
    * The message is persisted
    */
   public static final int STORED = 2;

   /**
    * It was a persistent message for a joint
    * cache store/persistent manager.
    * This states guards against double
    * removal from the cache while keeping
    * error checking for incorrect double removal
    * No message should be at this state for very long
    */
   public static final int REMOVED = 3;

   public long referenceId;
   public SpyMessage hardReference;

   // These fields are copied over from the messae itself..
   // they are used too often to not have them handy.
   public byte jmsPriority;
   public long messageId;
   public int jmsDeliveryMode;
   public long messageScheduledDelivery;
   public long messageExpiration;

   // Data used on the server
   public BasicQueue queue;
   public MessageCache messageCache;
   public SoftReference softReference;
   public int stored;
   transient public Object persistData;

   MessageReference()
   {
   }

   //init and reset methods for use by object pool
   void init(MessageCache messageCache, long referenceId, SpyMessage message, BasicQueue queue) throws JMSException
   {
      this.messageCache = messageCache;
      this.hardReference = message;
      this.referenceId = referenceId;
      this.jmsPriority = (byte) message.getJMSPriority();
      this.messageId = message.header.messageId;
      this.stored = NOT_STORED;
      this.jmsDeliveryMode = message.header.jmsDeliveryMode;
      if (message.propertyExists(SpyMessage.PROPERTY_SCHEDULED_DELIVERY))
      {
         this.messageScheduledDelivery = message.getLongProperty(SpyMessage.PROPERTY_SCHEDULED_DELIVERY);
      }
      this.messageExpiration = message.header.jmsExpiration;
      this.queue = queue;
      this.persistData = null;
   }

   void reset()
   {
      if (hardReference != null)
         MessagePool.releaseMessage(hardReference);
      else
      {
         hardReference = (SpyMessage) softReference.get();
         if (hardReference != null)
            MessagePool.releaseMessage(hardReference);
      }

      //clear refs so gc can collect unused objects
      if (softReference != null && softReference.get() != null)
         messageCache.softRefCacheSize--;
      this.messageCache = null;
      this.hardReference = null;
      this.softReference = null;
      this.queue = null;
      this.stored = 0;
      this.jmsDeliveryMode = 0;
      this.persistData = null;
   }

   public SpyMessage getMessage() throws JMSException
   {
      boolean trace = log.isTraceEnabled();
      if (trace)
         log.trace("getMessage lock aquire " + toString());

      SpyMessage result = null;

      synchronized (this)
      {
         if (hardReference == null)
         {
            makeHard();
            result = hardReference;
            messageCache.messageReferenceUsedEvent(this, false, trace);
         }
         else
         {
            result = hardReference;
            messageCache.cacheHits++;
            messageCache.messageReferenceUsedEvent(this, true, trace);
         }
         if (trace)
            log.trace("getMessage lock released " + toString());
         return result;
      }
   }

   /**
    * Returns true if this message reference has expired.
    */
   public boolean isExpired()
   {
      if (messageExpiration == 0)
         return false;
      long ts = System.currentTimeMillis();
      return messageExpiration < ts;
   }

   /**
    * Determines whether the message is persistent in the sense
    * that it survives a crash
    */
   public boolean isPersistent()
   {
      return queue instanceof PersistentQueue && jmsDeliveryMode == DeliveryMode.PERSISTENT;
   }

   /**
    * Determines the persistent for storing the message
    */
   public String getPersistentKey()
   {
      return queue.getDescription();
   }

   /**
    * We could optimize caching by keeping the headers but not the body.
    * The server will uses the headers more often than the body and the
    * headers take up much message memory than the body
    *
    * For now just return the message.
    */
   public SpyMessage.Header getHeaders() throws javax.jms.JMSException
   {
      return getMessage().header;
   }

   void clear() throws JMSException
   {
      boolean trace = log.isTraceEnabled();
      if (trace)
         log.trace("clear lock aquire " + toString());
      synchronized (this)
      {
         if (stored == STORED)
            messageCache.removeFromStorage(this);
         stored = MessageReference.REMOVED;

         if (trace)
            log.trace("clear lock relased " + toString());
      }
   }

   public void invalidate() throws JMSException
   {
      boolean trace = log.isTraceEnabled();
      if (trace)
         log.trace("invalidate lock aquire " + toString());
      synchronized (this)
      {
         if (stored == STORED)
         {
            if (hardReference == null)
            {
               makeHard();
               messageCache.messageReferenceUsedEvent(this, false, trace);
            }
            messageCache.removeFromStorage(this);
         }
         if (trace)
            log.trace("invalidate lock relased " + toString());
      }
   }

   public void removeDelayed() throws JMSException
   {
      messageCache.removeDelayed(this);
   }

   void makeSoft() throws JMSException
   {
      boolean trace = log.isTraceEnabled();
      if (trace)
         log.trace("makeSoft lock aquire " + toString());
      synchronized (this)
      {
         // Attempt to soften a removed message
         if (stored == REMOVED)
            throw new JMSException("CACHE ERROR: makeSoft() on a removed message " + this);

         // It is already soft
         if (softReference != null)
         {
            // Sanity check
            if (stored == NOT_STORED)
               throw new JMSException("CACHE ERROR: soft reference to unstored message " + this);
            return;
         }

         if (stored == NOT_STORED)
            messageCache.saveToStorage(this, hardReference);

         // HACK: allow the jdbc2 driver to reject saveToStorage for persistent messages
         // it is just about to persist the message when it gets some cpu
         if (stored != STORED)
         {
            if (trace)
               log.trace("saveToStorage rejected by cache " + toString());
            return;
         }

         softReference = new SoftReference(hardReference, messageCache.referenceQueue);

         // We don't need the hard ref anymore..
         messageCache.soften(this);
         hardReference = null;

         if (trace)
            log.trace("makeSoft lock released " + toString());
      }
   }

   /**
    * Called from A PeristenceManager/CacheStore, 
    * to let us know that this message is already stored on disk.
    */
   public void setStored(int stored)
   {
      this.stored = stored;
   }

   void makeHard() throws JMSException
   {
      boolean trace = log.isTraceEnabled();
      if (trace)
         log.trace("makeHard lock aquire " + toString());
      synchronized (this)
      {
         // Attempt to harden a removed message
         if (stored == REMOVED)
            throw new JMSException("CACHE ERROR: makeHard() on a removed message " + this);

         // allready hard
         if (hardReference != null)
            return;

         // Get the object via the softref
         hardReference = (SpyMessage) softReference.get();

         // It might have been removed from the cache due to memory constraints
         if (hardReference == null)
         {
            // load it from disk.
            hardReference = messageCache.loadFromStorage(this);
            messageCache.cacheMisses++;
         }
         else
         {
            messageCache.cacheHits++;
         }

         // Since we have hard ref, we do not need the soft one.
         if (softReference != null && softReference.get() != null)
            messageCache.softRefCacheSize--;
         softReference = null;
         if (trace)
            log.trace("makeHard lock released " + toString());
      }
   }

   public boolean equals(Object o)
   {
      try
      {
         return referenceId == ((MessageReference) o).referenceId;
      }
      catch (Throwable e)
      {
         return false;
      }
   }

   /**
    * This method allows message to be order on the server queues
    * by priority and the order that they came in on.
    *
    * @see Comparable#compareTo(Object)
    */
   public int compareTo(Object o)
   {
      MessageReference sm = (MessageReference) o;
      if (jmsPriority > sm.jmsPriority)
      {
         return -1;
      }
      if (jmsPriority < sm.jmsPriority)
      {
         return 1;
      }
      return (int) (messageId - sm.messageId);
   }

   /**
    * For debugging
    */
   public String toString()
   {
      StringBuffer buffer = new StringBuffer(100);
      if (messageCache == null)
         buffer.append(" NOT IN CACHE hashCode=").append(hashCode());

      else
      {
         buffer.append(referenceId);
         buffer.append(" msg=").append(messageId);
         if (hardReference != null)
            buffer.append(" hard");
         if (softReference != null)
            buffer.append(" soft");
         switch (stored)
         {
            case NOT_STORED :
               buffer.append(" NOT_STORED");
               break;
            case STORED :
               buffer.append(" STORED");
               break;
            case REMOVED :
               buffer.append(" REMOVED");
               break;
         }
         switch (jmsDeliveryMode)
         {
            case DeliveryMode.NON_PERSISTENT :
               buffer.append(" NON_PERSISTENT");
               break;
            case DeliveryMode.PERSISTENT :
               buffer.append(" PERSISTENT");
               break;
         }
         if (persistData != null)
            buffer.append(" persistData=").append(persistData);
         if (queue != null)
            buffer.append(" queue=").append(queue.getDescription());
         else
            buffer.append(" NO_QUEUE");
         buffer.append(" priority=").append(jmsPriority);
         buffer.append(" hashCode=").append(hashCode());
      }
      return buffer.toString();
   }
}
