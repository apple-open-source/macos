/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.server;

import java.lang.ref.Reference;
import java.lang.ref.ReferenceQueue;
import java.util.HashMap;
import EDU.oswego.cs.dl.util.concurrent.SynchronizedLong;

import javax.jms.JMSException;
import javax.management.MBeanRegistration;
import javax.management.ObjectName;

import org.jboss.mq.SpyMessage;
import org.jboss.mq.pm.CacheStore;
import org.jboss.system.ServiceMBeanSupport;

/**
 * This class implements a Message cache so that larger amounts of messages
 * can be processed without running out of memory.  When memory starts getting tight
 * it starts moving messages out of memory and into a file so that they can be recovered
 * later.
 *
 * The locks should be obtained in the following order:<br>
 * mr, the relevent message we are working with<br>
 * lruCache, when maintaining the usage order
 *
 * @author <a href="mailto:hiram.chirino@jboss.org">Hiram Chirino</a>
 * @author <a href="mailto:David.Maplesden@orion.co.nz">David Maplesden</a>
 * @author <a href="mailto:pra@tim.se">Peter Antman</a>
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version    $Revision: 1.17.2.6 $
 *
 * @jmx.mbean name="jboss.mq:service=MessageCache"
 *      extends="org.jboss.system.ServiceMBean"
 */
public class MessageCache extends ServiceMBeanSupport implements MessageCacheMBean, MBeanRegistration, Runnable
{
   // The cached messages are orded in a LRU linked list
   private LRUCache lruCache = new LRUCache();

   // Provides a Unique ID to MessageHanles
   private SynchronizedLong messageCounter = new SynchronizedLong(0);
   long cacheHits = 0;
   long cacheMisses = 0;

   CacheStore cacheStore;
   ObjectName cacheStoreName;

   private Thread referenceSoftner;

   private long highMemoryMark = 1024L * 1000 * 16;
   private long maxMemoryMark = 1024L * 1000 * 32;
   public static final long ONE_MEGABYTE = 1024L * 1000;

   int softRefCacheSize = 0;
   int totalCacheSize = 0;

   // Used to get notified when message are being deleted by GC
   ReferenceQueue referenceQueue = new ReferenceQueue();

   // The historical number of softenings
   long softenedSize = 0;

   // Check the soft reference depth
   boolean checkSoftReferenceDepth = false;

   /**
    * The <code>getInstance</code> method
    *
    * @return a <code>MessageCache</code> value
    *
    * @jmx.managed-attribute
    */
   public MessageCache getInstance()
   {
      return this;
   }

   /**
    * Adds a message to the cache
    */
   public MessageReference add(SpyMessage message) throws javax.jms.JMSException
   {
      return addInternal(message, null, MessageReference.NOT_STORED);
   }

   /**
    * Adds a message to the cache.
    */
   public MessageReference add(SpyMessage message, BasicQueue queue, int stored) throws javax.jms.JMSException
   {
      return addInternal(message, queue, stored);
   }

   /**
    * Adds a message to the cache.
    */
   public MessageReference addInternal(SpyMessage message, BasicQueue queue, int stored) throws javax.jms.JMSException
   {
      boolean trace = log.isTraceEnabled();

      // Create the message reference
      MessageReference mh = new MessageReference();
      mh.init(this, messageCounter.increment(), message, queue);
      mh.setStored(stored);

      // Add it to the cache
      if (trace)
         log.trace("add lock aquire message " + mh);
      synchronized (mh)
      {
         if (trace)
            log.trace("add lock aquire lruCache " + mh);
         synchronized (lruCache)
         {
            lruCache.addMostRecent(mh);
            totalCacheSize++;
            if (trace)
               log.trace("add locks release" + mh);
         }
      }
      validateSoftReferenceDepth();

      return mh;
   }

   /**
    * removes a message from the cache
    */
   public void remove(MessageReference mr) throws JMSException
   {
      // Remove if not done already
      removeInternal(mr, true, true);
   }

   /**
    * removes a message from the cache without returning it to the pool
    * used in two phase removes for joint cache/persistence
    */
   public void removeDelayed(MessageReference mr) throws JMSException
   {
      // Remove from the cache
      removeInternal(mr, true, false);
   }

   /**
    * removes a message from the cache but does not clear it,
    * used in softening
    */
   void soften(MessageReference mr) throws JMSException
   {
      // Remove from the cache
      removeInternal(mr, false, false);

      softRefCacheSize++;
   }

   /**
    * removes a message from the cache
    */
   protected void removeInternal(MessageReference mr, boolean clear, boolean reset) throws JMSException
   {
      boolean trace = log.isTraceEnabled();

      if (trace)
         log.trace("remove lock aquire message " + mr + " clear=" + clear + " reset=" + reset);
      synchronized (mr)
      {
         if (mr.stored != MessageReference.REMOVED)
         {
            if (trace)
               log.trace("remove lock aquire lrucache " + mr + " clear= " + clear + " reset=" + reset);
            synchronized (lruCache)
            {
               if (mr.hardReference != null) //If message is not hard, dont do lru stuff
                  lruCache.remove(mr);
               if (clear)
                  totalCacheSize--;

               if (trace)
                  log.trace("remove lock release lrucache " + mr + " clear= " + clear + " reset=" + reset);
            }
            if (clear)
               mr.clear();
            //Will remove it from storage if stored
         }

         if (reset)
            mr.reset();
         //Return to the pool

         if (trace)
            log.trace("remove lock release message " + mr + " clear= " + clear + " reset=" + reset);
      }
   }

   /**
    * The strategy is that we keep the most recently used messages as
    * Hard references.  Then we make the older ones soft references.  Making
    * something a soft reference stores it to disk so we need to avoid making
    * soft references if we can avoid it.  But once it is made a soft reference does
    * not mean that it is removed from memory.  Depending on how agressive the JVM's
    * GC is, it may stay around long enough for it to be used by a client doing a read,
    * saving us read from the file system.  If memory gets tight the GC will remove
    * the soft references.  What we want to do is make sure there are at least some
    * soft references available so that the GC can reclaim memory.
    * @see Runnable#run()
    */
   public void run()
   {
      try
      {
         while (true)
         {
            // Get the next soft reference that was canned by the GC
            Reference r = null;
            if (checkSoftReferenceDepth)
               r = referenceQueue.poll();
            else
               r = referenceQueue.remove(1000);
            if (r != null)
            {
               softRefCacheSize--;
               // the GC will free a set of messages together, so we poll them
               // all before we validate the soft reference depth.
               while ((r = referenceQueue.poll()) != null)
               {
                  softRefCacheSize--;
               }
               if (log.isTraceEnabled())
                  log.trace("soft reference cache size is now: " + softRefCacheSize);

               checkSoftReferenceDepth = true;
            }

            if (checkSoftReferenceDepth)
               checkSoftReferenceDepth = validateSoftReferenceDepth();
         }
      }
      catch (JMSException e)
      {
         log.error("Message Cache Thread Stopped: ", e);
      }
      catch (InterruptedException e)
      {
         // Signal to exit the thread.
      }
      log.debug("Thread exiting.");
   }

   /**
    * This method is in charge of determining if it time to convert some
    * hard references over to soft references.
    */
   boolean validateSoftReferenceDepth() throws JMSException
   {
      boolean trace = log.isTraceEnabled();

      // Loop until softening is not required or we find a message we can soften
      while (getState() == ServiceMBeanSupport.STARTED)
      {
         MessageReference messageToSoften = null;

         if (trace)
            log.trace("run lock aquire, validateSoftReferenceDepth");
         synchronized (lruCache)
         {
            // howmany to change over to soft refs
            int softenCount = 0;
            int hardCount = getHardRefCacheSize();
            int softCount = getSoftRefCacheSize();

            // Only soften when there is more than one hard reference
            // Should probably parameterise this?
            if (hardCount < 2)
               return false;

            long currentMem = Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory();
            if (currentMem > highMemoryMark)
            {
               // we need to get more aggresive... how much?? lets get
               // a mesurment from 0 to 1
               float severity = ((float) (currentMem - highMemoryMark)) / (maxMemoryMark - highMemoryMark);
               severity = Math.min(severity, 1.0F);
               if (trace)
                  log.trace("Memory usage serverity=" + severity);
               int totalMessageInMem = hardCount + softCount;
               int howManyShouldBeSoft = (int) ((totalMessageInMem) * severity);
               softenCount = howManyShouldBeSoft - softCount;
            }

            // We can only do so much, somebody else is using all the memory?
            if (softenCount > hardCount)
            {
               log.debug("Soften count " + softenCount + " greater than hard references " + hardCount);
               softenCount = hardCount;
            }

            // Ignore soften counts of 1 since this will happen too often even
            // if the serverity is low since it will round up.
            if (softenCount > 1)
            {
               if (trace)
                  log.trace("Need to soften " + softenCount + " messages");
               Node node = lruCache.getLeastRecent();
               messageToSoften = (MessageReference) node.data;
            }
            if (trace)
               log.trace("run lock release, validateSoftReferenceDepth");
         }

         // No softening required
         if (messageToSoften == null)
            return false;

         if (trace)
            log.trace("soften lock acquire " + messageToSoften);
         synchronized (messageToSoften)
         {
            // Soften unless it was removed
            if (messageToSoften.messageCache != null && messageToSoften.stored != MessageReference.REMOVED)
            {
               messageToSoften.makeSoft();
               if (messageToSoften.stored == MessageReference.STORED)
               {
                  softenedSize++;
                  return true;
               }
               else if (messageToSoften.isPersistent())
               {
                  // Avoid going into a cpu loop if there are persistent
                  // messages just about to be persisted
                  return false;
               }
            }
            else if (trace)
               log.trace("not softening removed message " + messageToSoften);

            if (trace)
               log.trace("soften lock release " + messageToSoften);
         }
      }
      return false;
   }

   /**
    * This gets called when a MessageReference is de-referenced.
    * We will pop it to the top of the RLU
    */
   void messageReferenceUsedEvent(MessageReference mh, boolean wasHard, boolean trace) throws JMSException
   {
      if (trace)
         log.trace("messageReferenceUsedEvent lock aquire message " + mh + " wasHard=" + wasHard);

      synchronized (mh)
      {

         if (trace)
            log.trace("messageReferenceUsedEvent lock aquire lrucache " + mh + " wasHard=" + wasHard);
         synchronized (lruCache)
         {
            if (wasHard)
               lruCache.makeMostRecent(mh);
            else
            {
               lruCache.addMostRecent(mh);
            }

            if (trace)
               log.trace("messageReferenceUsedEvent locks released " + mh + " wasHard=" + wasHard);
         }
      }

      if (wasHard == false)
         checkSoftReferenceDepth = true;
   }

   //////////////////////////////////////////////////////////////////////////////////
   // Perisitence methods used by the MessageReference.
   //////////////////////////////////////////////////////////////////////////////////
   SpyMessage loadFromStorage(MessageReference mh) throws JMSException
   {
      return (SpyMessage) cacheStore.loadFromStorage(mh);
   }

   void saveToStorage(MessageReference mh, SpyMessage message) throws JMSException
   {
      cacheStore.saveToStorage(mh, message);
   }

   void removeFromStorage(MessageReference mh) throws JMSException
   {
      cacheStore.removeFromStorage(mh);
   }

   //////////////////////////////////////////////////////////////////////////////////
   //
   // The following section deals the the JMX interface to manage the Cache
   //
   //////////////////////////////////////////////////////////////////////////////////

   /**
    * This gets called to start the cache service. Synch. by start
    */
   protected void startService() throws Exception
   {

      cacheStore = (CacheStore) getServer().getAttribute(cacheStoreName, "Instance");

      referenceSoftner = new Thread(this, "JBossMQ Cache Reference Softner");
      referenceSoftner.setDaemon(true);
      referenceSoftner.start();
   }

   /**
    * This gets called to stop the cache service.
    */
   protected void stopService()
   {
      synchronized (lruCache)
      {
         referenceSoftner.interrupt();
         referenceSoftner = null;
      }
      cacheStore = null;
   }

   /**
    * Gets the hardRefCacheSize
    * @return Returns a int
    *
    * @jmx.managed-attribute
    */
   public int getHardRefCacheSize()
   {
      synchronized (lruCache)
      {
         return lruCache.size();
      }
   }

   /**
    * The <code>getSoftenedSize</code> method
    *
    * @return a <code>long</code> value
    *
    * @jmx.managed-attribute
    */
   public long getSoftenedSize()
   {
      return softenedSize;
   }

   /**
    * Gets the softRefCacheSize
    * @return Returns a int
    *
    * @jmx.managed-attribute
    */
   public int getSoftRefCacheSize()
   {
      return softRefCacheSize;
   }

   /**
    * Gets the totalCacheSize
    * @return Returns a int
    *
    * @jmx.managed-attribute
    */
   public int getTotalCacheSize()
   {
      return totalCacheSize;
   }

   /**
    * Gets the cacheMisses
    * @return Returns a int
    *
    * @jmx.managed-attribute
    */
   public long getCacheMisses()
   {
      return cacheMisses;
   }

   /**
    * Gets the cacheHits
    * @return Returns a long
    *
    * @jmx.managed-attribute
    */
   public long getCacheHits()
   {
      return cacheHits;
   }

   /**
    * Gets the highMemoryMark
    * @return Returns a long
    *
    * @jmx.managed-attribute
    */
   public long getHighMemoryMark()
   {
      return highMemoryMark / ONE_MEGABYTE;
   }
   /**
    * Sets the highMemoryMark
    * @param highMemoryMark The highMemoryMark to set
    *
    * @jmx.managed-attribute
    */
   public void setHighMemoryMark(long highMemoryMark)
   {
      this.highMemoryMark = highMemoryMark * ONE_MEGABYTE;
   }

   /**
    * Gets the maxMemoryMark
    * @return Returns a long
    *
    * @jmx.managed-attribute
    */
   public long getMaxMemoryMark()
   {
      return maxMemoryMark / ONE_MEGABYTE;
   }

   /**
    * Gets the CurrentMemoryUsage
    * @return Returns a long
    *
    * @jmx.managed-attribute
    */
   public long getCurrentMemoryUsage()
   {
      return (Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory()) / ONE_MEGABYTE;
   }

   /**
    * Sets the maxMemoryMark
    * @param maxMemoryMark The maxMemoryMark to set
    *
    * @jmx.managed-attribute
    */
   public void setMaxMemoryMark(long maxMemoryMark)
   {
      this.maxMemoryMark = maxMemoryMark * ONE_MEGABYTE;
   }

   /**
    * @see ServiceMBeanSupport#getName()
    */
   public String getName()
   {
      return "MessageCache";
   }

   /**
    * @see MessageCacheMBean#setCacheStore(ObjectName)
    *
    * @jmx.managed-attribute
    */
   public void setCacheStore(ObjectName cacheStoreName)
   {
      this.cacheStoreName = cacheStoreName;
   }

   /**
    * The <code>getCacheStore</code> method
    *
    * @return an <code>ObjectName</code> value
    *
    * @jmx.managed-attribute
    */
   public ObjectName getCacheStore()
   {
      return cacheStoreName;
   }

   /**
    * This class implements a simple, efficient LRUCache.  It is pretty much a
    * cut down version of the code in org.jboss.pool.cache.LeastRecentlyUsedCache
    *
    *
    */
   class LRUCache
   {
      int currentSize = 0;
      //maps objects to their nodes
      HashMap map = new HashMap();
      Node mostRecent = null;
      Node leastRecent = null;
      public void addMostRecent(Object o)
      {
         Node newNode = new Node();
         newNode.data = o;
         //insert into map
         Object oldNode = map.put(o, newNode);
         if (oldNode != null)
         {
            map.put(o, oldNode);
            throw new RuntimeException("Can't add object '" + o + "' to LRUCache that is already in cache.");
         }
         //insert into linked list
         if (mostRecent == null)
         {
            //first element
            mostRecent = newNode;
            leastRecent = newNode;
         }
         else
         {
            newNode.lessRecent = mostRecent;
            mostRecent.moreRecent = newNode;
            mostRecent = newNode;
         }
         ++currentSize;
      }
      // Not used anywhere!!
      public void addLeastRecent(Object o)
      {
         Node newNode = new Node();
         newNode.data = o;
         //insert into map
         Object oldNode = map.put(o, newNode);
         if (oldNode != null)
         {
            map.put(o, oldNode);
            throw new RuntimeException("Can't add object '" + o + "' to LRUCache that is already in cache.");
         }
         //insert into linked list
         if (leastRecent == null)
         {
            //first element
            mostRecent = newNode;
            leastRecent = newNode;
         }
         else
         {
            newNode.moreRecent = leastRecent;
            leastRecent.lessRecent = newNode;
            leastRecent = newNode;
         }
         ++currentSize;
      }
      public void remove(Object o)
      {
         //remove from map
         Node node = (Node) map.remove(o);
         if (node == null)
            throw new RuntimeException("Can't remove object '" + o + "' that is not in cache.");
         //remove from linked list
         Node more = node.moreRecent;
         Node less = node.lessRecent;
         if (more == null)
         { //means node is mostRecent
            mostRecent = less;
            if (mostRecent != null)
            {
               mostRecent.moreRecent = null; //Mark it as beeing at the top of tree
            }
         }
         else
         {
            more.lessRecent = less;
         }
         if (less == null)
         { //means node is leastRecent
            leastRecent = more;
            if (leastRecent != null)
            {
               leastRecent.lessRecent = null; //Mark it last in tree
            }
         }
         else
         {
            less.moreRecent = more;
         }
         --currentSize;
      }
      public void makeMostRecent(Object o)
      {
         //get node from map
         Node node = (Node) map.get(o);
         if (node == null)
            throw new RuntimeException("Can't make most recent object '" + o + "' that is not in cache.");
         //reposition in linked list, first remove
         Node more = node.moreRecent;
         Node less = node.lessRecent;
         if (more == null) //means node is mostRecent
            return;
         else
            more.lessRecent = less;
         if (less == null) //means node is leastRecent
            leastRecent = more;
         else
            less.moreRecent = more;
         //now add back in at most recent position
         node.lessRecent = mostRecent;
         node.moreRecent = null; //We are at the top
         mostRecent.moreRecent = node;
         mostRecent = node;
      }
      public int size()
      {
         return currentSize;
      }
      public Node getMostRecent()
      {
         return mostRecent;
      }
      public Node getLeastRecent()
      {
         return leastRecent;
      }
   }

   class Node
   {
      Node moreRecent = null;
      Node lessRecent = null;
      Object data = null;
   }
}
/*
vim:tabstop=3:expandtab:ai
*/
