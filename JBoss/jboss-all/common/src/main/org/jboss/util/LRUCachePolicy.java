/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.util;

import java.util.HashMap;

/**
 * Implementation of a Least Recently Used cache policy.
 *
 * @author <a href="mailto:simone.bordet@compaq.com">Simone Bordet</a>
 * @version $Revision: 1.4.2.3 $
 */
public class LRUCachePolicy
   implements CachePolicy
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------
   /**
    * The map holding the cached objects
    */
   protected HashMap m_map;
   /**
    * The linked list used to implement the LRU algorithm
    */
   protected LRUList m_list;
   /**
    * The maximum capacity of this cache
    */
   protected int m_maxCapacity;
   /**
    * The minimum capacity of this cache
    */
   protected int m_minCapacity;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   /**
    * Creates a LRU cache policy object with zero cache capacity.
    *
    * @see #create
    */
   public LRUCachePolicy()
   {
   }
   /**
    * Creates a LRU cache policy object with the specified minimum
    * and maximum capacity.
    *
    * @see #create
    */
   public LRUCachePolicy(int min, int max)
   {
      if (min < 2 || min > max) {throw new IllegalArgumentException("Illegal cache capacities");}
      m_minCapacity = min;
      m_maxCapacity = max;
   }

   // Public --------------------------------------------------------

   // Service implementation ----------------------------------------------
   /**
    * Initializes the cache, creating all required objects and initializing their
    * values.
    * @see #start
    * @see #destroy
    */
   public void create()
   {
      m_map = new HashMap();
      m_list = createList();
      m_list.m_maxCapacity = m_maxCapacity;
      m_list.m_minCapacity = m_minCapacity;
      m_list.m_capacity = m_maxCapacity;
   }
   /**
    * Starts this cache that is now ready to be used.
    * @see #create
    * @see #stop
    */
   public void start()
   {
   }
   /**
    * Stops this cache thus {@link #flush}ing all cached objects. <br>
    * After this method is called, a call to {@link #start} will restart the cache.
    * @see #start
    * @see #destroy
    */
   public void stop()
   {
      if (m_list != null)
      {
         flush();
      }
   }
   /**
    * Destroys the cache that is now unusable. <br>
    * To have it working again it must be re-{@link #create}ed and
    * re-{@link #start}ed.
    *
    * @see #create
    */
   public void destroy()
   {
      if( m_map != null )
         m_map.clear();
      if( m_list != null )
         m_list.clear();
   }

   public Object get(Object key)
   {
      if (key == null)
      {
         throw new IllegalArgumentException("Requesting an object using a null key");
      }

      LRUCacheEntry value = (LRUCacheEntry)m_map.get(key);
      if (value != null)
      {
         m_list.promote(value);
         return value.m_object;
      }
      else
      {
         cacheMiss();
         return null;
      }
   }
   public Object peek(Object key)
   {
      if (key == null)
      {
         throw new IllegalArgumentException("Requesting an object using a null key");
      }

      LRUCacheEntry value = (LRUCacheEntry)m_map.get(key);
      if (value == null)
      {
         return null;
      }
      else
      {
         return value.m_object;
      }
   }
   public void insert(Object key, Object o)
   {
      if (o == null) {throw new IllegalArgumentException("Cannot insert a null object in the cache");}
      if (key == null) {throw new IllegalArgumentException("Cannot insert an object in the cache with null key");}
      if (m_map.containsKey(key))
      {
         throw new IllegalStateException("Attempt to put in the cache an object that is already there");
      }
      m_list.demote();
      LRUCacheEntry entry = createCacheEntry(key, o);
      m_map.put(key, entry);
      m_list.promote(entry);
   }
   public void remove(Object key)
   {
      if (key == null) {throw new IllegalArgumentException("Removing an object using a null key");}

      Object value = m_map.remove(key);
      if (value != null)
      {
         m_list.remove((LRUCacheEntry)value);
      }
      //else Do nothing, the object isn't in the cache list
   }
   public void flush()
   {
      LRUCacheEntry entry = null;
      while ((entry = m_list.m_tail) != null)
      {
         ageOut(entry);
      }
   }

   public int size() {
      return m_list.m_count;
   }

   // Y overrides ---------------------------------------------------

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------
   /**
    * Factory method for the linked list used by this cache implementation.
    */
   protected LRUList createList() {return new LRUList();}
   /**
    * Callback method called when the cache algorithm ages out of the cache
    * the given entry. <br>
    * The implementation here is removing the given entry from the cache.
    */
   protected void ageOut(LRUCacheEntry entry)
   {
      remove(entry.m_key);
   }
   /**
    * Callback method called when a cache miss happens.
    */
   protected void cacheMiss()
   {
   }
   /**
    * Factory method for cache entries
    */
   protected LRUCacheEntry createCacheEntry(Object key, Object value)
   {
      return new LRUCacheEntry(key, value);
   }

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
   /**
    * Double queued list used to store cache entries.
    */
   public class LRUList
   {
      /** The maximum capacity of the cache list */
      public int m_maxCapacity;
      /** The minimum capacity of the cache list */
      public int m_minCapacity;
      /** The current capacity of the cache list */
      public int m_capacity;
      /** The number of cached objects */
      public int m_count;
      /** The head of the double linked list */
      public LRUCacheEntry m_head;
      /** The tail of the double linked list */
      public LRUCacheEntry m_tail;
      /** The cache misses happened */
      public int m_cacheMiss;
      /**
       * Creates a new double queued list.
       */
      protected LRUList()
      {
         m_head = null;
         m_tail = null;
         m_count = 0;
      }
      /**
       * Promotes the cache entry <code>entry</code> to the last used position
       * of the list. <br>
       * If the object is already there, does nothing.
       * @param entry the object to be promoted, cannot be null
       * @see #demote
       * @throws IllegalStateException if this method is called with a full cache
       */
      protected void promote(LRUCacheEntry entry)
      {
         if (entry == null) {throw new IllegalArgumentException("Trying to promote a null object");}
         if (m_capacity < 1) {throw new IllegalStateException("Can't work with capacity < 1");}

         entryPromotion(entry);

         entry.m_time = System.currentTimeMillis();
         if (entry.m_prev == null)
         {
            if (entry.m_next == null)
            {
               // entry is new or there is only the head
               if (m_count == 0) // cache is empty
               {
                  m_head = entry;
                  m_tail = entry;
                  ++m_count;
                  entryAdded(entry);
               }
               else if (m_count == 1 && m_head == entry) {} // there is only the head and I want to promote it, do nothing
               else if (m_count < m_capacity)
               {
                  entry.m_prev = null;
                  entry.m_next = m_head;
                  m_head.m_prev = entry;
                  m_head = entry;
                  ++m_count;
                  entryAdded(entry);
               }
               else if (m_count < m_maxCapacity)
               {
                  entry.m_prev = null;
                  entry.m_next = m_head;
                  m_head.m_prev = entry;
                  m_head = entry;
                  ++m_count;
                  int oldCapacity = m_capacity;
                  ++m_capacity;
                  entryAdded(entry);
                  capacityChanged(oldCapacity);
               }
               else {throw new IllegalStateException("Attempt to put a new cache entry on a full cache");}
            }
            else {} // entry is the head, do nothing
         }
         else
         {
            if (entry.m_next == null) // entry is the tail
            {
               LRUCacheEntry beforeLast = entry.m_prev;
               beforeLast.m_next = null;
               entry.m_prev = null;
               entry.m_next = m_head;
               m_head.m_prev = entry;
               m_head = entry;
               m_tail = beforeLast;
            }
            else // entry is in the middle of the list
            {
               LRUCacheEntry previous = entry.m_prev;
               previous.m_next = entry.m_next;
               entry.m_next.m_prev = previous;
               entry.m_prev = null;
               entry.m_next = m_head;
               m_head.m_prev = entry;
               m_head = entry;
            }
         }
      }
      /**
       * Demotes from the cache the least used entry. <br>
       * If the cache is not full, does nothing.
       * @see #promote
       */
      protected void demote()
      {
         if (m_capacity < 1) {throw new IllegalStateException("Can't work with capacity < 1");}
         if (m_count > m_maxCapacity) {throw new IllegalStateException("Cache list entries number (" + m_count + ") > than the maximum allowed (" + m_maxCapacity + ")");}
         if (m_count == m_maxCapacity)
         {
            LRUCacheEntry entry = m_tail;

            // the entry will be removed by ageOut
            ageOut(entry);
         }
         else {} // cache is not full, do nothing
      }
      /**
       * Removes from the cache list the specified entry.
       */
      protected void remove(LRUCacheEntry entry)
      {
         if (entry == null) {throw new IllegalArgumentException("Cannot remove a null entry from the cache");}
         if (m_count < 1) {throw new IllegalStateException("Trying to remove an entry from an empty cache");}

         entry.m_key = entry.m_object = null;
         if (m_count == 1)
         {
            m_head = m_tail = null;
         }
         else
         {
            if (entry.m_prev == null) // the head
            {
               m_head = entry.m_next;
               m_head.m_prev = null;
               entry.m_next = null;
            }
            else if (entry.m_next == null) // the tail
            {
               m_tail = entry.m_prev;
               m_tail.m_next = null;
               entry.m_prev = null;
            }
            else // in the middle
            {
               entry.m_next.m_prev = entry.m_prev;
               entry.m_prev.m_next = entry.m_next;
               entry.m_prev = null;
               entry.m_next = null;
            }
         }
         --m_count;
         entryRemoved(entry);
      }
      /**
       * Callback that signals that the given entry is just about to be added.
       */
      protected void entryPromotion(LRUCacheEntry entry) {}
      /**
       * Callback that signals that the given entry has been added to the cache.
       */
      protected void entryAdded(LRUCacheEntry entry) {}
      /**
       * Callback that signals that the given entry has been removed from the cache.
       */
      protected void entryRemoved(LRUCacheEntry entry) {}
      /**
       * Callback that signals that the capacity of the cache is changed.
       * @param oldCapacity the capacity before the change happened
       */
      protected void capacityChanged(int oldCapacity) {}

      protected void clear()
      {
         LRUCacheEntry entry = m_head;
         m_head = null;
         m_tail = null;
         m_count = 0;
         for (; entry != null; entry = entry.m_next)
            entryRemoved(entry);
      }

      public String toString()
      {
         String s = Integer.toHexString(super.hashCode());
         s += " size: " + m_count;
         for (LRUCacheEntry entry = m_head; entry != null; entry = entry.m_next)
         {
            s += "\n" + entry;
         }
         return s;
      }
   }

   /**
    * Double linked cell used as entry in the cache list.
    */
   public class LRUCacheEntry
   {
      /** Reference to the next cell in the list */
      public  LRUCacheEntry m_next;
      /** Reference to the previous cell in the list */
      public LRUCacheEntry m_prev;
      /** The key used to retrieve the cached object */
      public Object m_key;
      /** The cached object */
      public Object m_object;
      /** The timestamp of the creation */
      public long m_time;
      /**
       * Creates a new double linked cell, storing the object we
       * want to cache and the key that is used to retrieve it.
       */
      protected LRUCacheEntry(Object key, Object object)
      {
         m_key = key;
         m_object = object;
         m_next = null;
         m_prev = null;
         m_time = 0; // Set when inserted in the list.
      }
      public String toString()
      {
         return "key: " + m_key + ", object: " + ( m_object==null ? "null" : Integer.toHexString(m_object.hashCode())) + ", entry: " + Integer.toHexString(super.hashCode());
      }
   }
}
