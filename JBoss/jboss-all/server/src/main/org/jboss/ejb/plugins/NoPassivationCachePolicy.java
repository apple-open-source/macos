/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins;

import java.util.HashMap;

import org.jboss.ejb.EnterpriseContext;
import org.jboss.util.CachePolicy;

/**
 * Implementation of a no passivation cache policy.
 *
 * @see AbstractInstanceCache
 * @author <a href="mailto:simone.bordet@compaq.com">Simone Bordet</a>
 * @version $Revision: 1.9.4.2 $
 */
public class NoPassivationCachePolicy
      implements CachePolicy
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------
   private HashMap m_map;


   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   /**
    * Creates a no passivation cache policy object given the instance
    * cache that use this policy object, that btw is not used.
    */
   public NoPassivationCachePolicy(AbstractInstanceCache eic)
   {
   }

   // Public --------------------------------------------------------

   // Z implementation ----------------------------------------------
   public void create() throws Exception
   {
      m_map = new HashMap();
   }

   public void start() throws Exception
   {
   }

   public void stop()
   {
   }

   public void destroy()
   {
      synchronized (m_map)
      {
         m_map.clear();
      }
   }

   public Object get(Object key)
   {
      if (key == null)
      {
         throw new IllegalArgumentException("Requesting an object using a null key");
      }
      EnterpriseContext ctx = null;
      synchronized (m_map)
      {
         ctx = (EnterpriseContext) m_map.get(key);
      }
      return ctx;
   }

   public Object peek(Object key)
   {
      return get(key);
   }

   public void insert(Object key, Object ctx)
   {
      if (ctx == null)
      {
         throw new IllegalArgumentException("Cannot insert a null object in the cache");
      }
      if (key == null)
      {
         throw new IllegalArgumentException("Cannot insert an object in the cache with null key");
      }

      synchronized (m_map)
      {
         Object obj = m_map.get(key);
         if (obj == null)
         {
            m_map.put(key, ctx);
         }
         else
         {
            throw new IllegalStateException("Attempt to put in the cache an object that is already there");
         }
      }
   }

   public void remove(Object key)
   {
      if (key == null)
      {
         throw new IllegalArgumentException("Removing an object using a null key");
      }

      synchronized (m_map)
      {
         Object value = m_map.get(key);
         if (value != null)
         {
            m_map.remove(key);
         }
         else
         {
            throw new IllegalArgumentException("Cannot remove an object that isn't in the cache");
         }
      }
   }

   public void flush()
   {
   }

   public int size()
   {
      synchronized (m_map)
      {
         return m_map.size();
      }
   }

   // Y overrides ---------------------------------------------------

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}
