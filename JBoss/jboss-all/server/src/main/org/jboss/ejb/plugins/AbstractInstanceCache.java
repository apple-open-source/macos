/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/

package org.jboss.ejb.plugins;

import java.lang.reflect.Constructor;
import java.rmi.NoSuchObjectException;
import java.rmi.RemoteException;
import java.util.Map;

import org.jboss.deployment.DeploymentException;
import org.jboss.ejb.BeanLock;
import org.jboss.ejb.Container;
import org.jboss.ejb.EnterpriseContext;
import org.jboss.ejb.InstanceCache;
import org.jboss.logging.Logger;
import org.jboss.metadata.MetaData;
import org.jboss.metadata.XmlLoadable;
import org.jboss.monitor.MetricsConstants;
import org.jboss.monitor.Monitorable;
import org.jboss.monitor.client.BeanCacheSnapshot;
import org.jboss.util.CachePolicy;
import org.w3c.dom.Element;

/**
 * Base class for caches of entity and stateful beans. <p>
 * It manages the cache entries through a {@link CachePolicy} object;
 * the implementation of the cache policy object must respect the following
 * requirements:
 * <ul>
 * <li> Have a public constructor that takes a single argument of type
 * AbstractInstanceCache.class or a subclass
 * </ul>
 *
 * @author <a href="mailto:simone.bordet@compaq.com">Simone Bordet</a>
 * @author <a href="bill@burkecentral.com">Bill Burke</a>
 * @author <a href="marc.fleury@jboss.org">Marc Fleury</a>
 * @version $Revision: 1.32.2.10 $
 * @jmx:mbean
 */
public abstract class AbstractInstanceCache
   implements InstanceCache, XmlLoadable, Monitorable, MetricsConstants,
      AbstractInstanceCacheMBean
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------
   protected static Logger log = Logger.getLogger(AbstractInstanceCache.class);

   /* The object that is delegated to implement the desired caching policy */
   private CachePolicy m_cache;
   /* The mutex object for the cache */
   private final Object m_cacheLock = new Object();

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   // Monitorable implementation ------------------------------------
   public void sample(Object s)
   {
      if( m_cache == null )
         return;

      synchronized (getCacheLock())
      {
         BeanCacheSnapshot snapshot = (BeanCacheSnapshot)s;
         snapshot.m_passivatingBeans = 0;
         CachePolicy policy = getCache();
         if (policy instanceof Monitorable)
         {
            ((Monitorable)policy).sample(s);
         }
      }
   }
   public Map retrieveStatistic()
   {
      return null;
   }
   public void resetStatistic()
   {
   }

   // Public --------------------------------------------------------
   /* From InstanceCache interface */
   public EnterpriseContext get(Object id)
      throws RemoteException, NoSuchObjectException
   {
      if (id == null) throw new IllegalArgumentException("Can't get an object with a null key");

      EnterpriseContext ctx;

      synchronized (getCacheLock())
      {
         CachePolicy cache = getCache();
         ctx = (EnterpriseContext)cache.get(id);
         if (ctx == null)
         {
            try
            {
               ctx = acquireContext();
               setKey(id, ctx);
               activate(ctx);
               logActivation(id);
               // the cache will throw an IllegalStateException if we try to insert
               // something that is in the cache already, so we don't check here
               cache.insert(id, ctx);
            }
            catch (Throwable x)
            {
               log.debug("Activation failure", x);
               throw new NoSuchObjectException(x.getMessage());
            }
         }
      }

      return ctx;
   }

   /* From InstanceCache interface */
   public void insert(EnterpriseContext ctx)
   {
      if (ctx == null) throw new IllegalArgumentException("Can't insert a null object in the cache");

      Object key = getKey(ctx);
      synchronized (getCacheLock())
      {
         // the cache will throw an IllegalStateException if we try to insert
	 // something that is in the cache already, so we don't check here
         getCache().insert(key, ctx);
      }
   }
  
   protected void tryToPassivate(EnterpriseContext ctx)
   {
      Object id = ctx.getId();
      if (id == null) return;
      BeanLock lock = getContainer().getLockManager().getLock(id);
      try
      {
         lock.sync();
         if (canPassivate(ctx))
         {
            try
            {
               remove(id);
               passivate(ctx);
               freeContext(ctx);
            }
            catch (Exception ignored)
            {
               log.warn("failed to passivate, id="+id, ignored);
            }
         }
         else
         {
            log.warn("Unable to passivate due to ctx lock, id="+id);

            // Touch the entry to make it MRU
            synchronized (getCacheLock())
            {
               getCache().get(id);
            }
         }
      }
      finally
      {
         lock.releaseSync();
         getContainer().getLockManager().removeLockRef(id);
      }
   }

   /* From InstanceCache interface */
   public void release(EnterpriseContext ctx)
   {
      if (ctx == null) throw new IllegalArgumentException("Can't release a null object");

      // Here I remove the bean; call to remove(id) is wrong
      // cause will remove also the cache lock that is needed
      // by the passivation, that eventually will remove it.
      Object id = getKey(ctx);
      synchronized (getCacheLock())
      {
         if (getCache().peek(id) != null)
            getCache().remove(id);
      }
      tryToPassivate(ctx);
   }

   /** 
    * From InstanceCache interface 
    * @jmx:managed-operation 
    */
   public void remove(Object id)
   {
      if (id == null) throw new IllegalArgumentException("Can't remove an object using a null key");

      synchronized (getCacheLock())
      {
         if (getCache().peek(id) != null)
         {
            getCache().remove(id);
         }
      }
   }

   public boolean isActive(Object id)
   {
      // Check whether an object with the given id is available in the cache
      synchronized (getCacheLock())
      {
         return getCache().peek(id) != null;
      }
   }

   /** Get the current cache size
    * @jmx:managed-attribute
    * @return the size of the cache
    */
   public long getCacheSize()
   {
      int cacheSize = m_cache != null ? m_cache.size() : 0;
      return cacheSize;
   }
   /** Flush the cache.
    * @jmx:managed-operation
    */
   public void flush()
   {
      if( m_cache != null )
         m_cache.flush();
   }
   /** Get the passivated count.
    * @jmx:managed-attribute
    * @return the number of passivated instances.
    */
   public long getPassivatedCount()
   {
      return 0;
   }

   // XmlLoadable implementation ----------------------------------------------
   public void importXml(Element element) throws DeploymentException
   {
      // This one is mandatory
      String p = MetaData.getElementContent(MetaData.getUniqueChild(element, "cache-policy"));
      try
      {
         Class cls = Thread.currentThread().getContextClassLoader().loadClass(p);
         Constructor ctor = cls.getConstructor(new Class[] {AbstractInstanceCache.class});
         m_cache = (CachePolicy)ctor.newInstance(new Object[] {this});
      }
      catch (Exception x)
      {
         throw new DeploymentException("Can't create cache policy", x);
      }

      Element policyConf = MetaData.getOptionalChild(element, "cache-policy-conf");
      if (policyConf != null)
      {
         if (m_cache instanceof XmlLoadable)
         {
            try
            {
               ((XmlLoadable)m_cache).importXml(policyConf);
            }
            catch (Exception x)
            {
               throw new DeploymentException("Can't import policy configuration", x);
            }
         }
      }
   }

   /* From Service interface*/
   public void create() throws Exception
   {
      getCache().create();
   }
   /* From Service interface*/
   public void start() throws Exception
   {
      getCache().start();
   }
   /* From Service interface*/
   public void stop()
   {
      // Empty the cache
      synchronized (getCacheLock())
      {
         getCache().stop();
      }
   }
   /* From Service interface*/
   public void destroy()
   {
      synchronized (getCacheLock())
      {
         getCache().destroy();
      }
      this.m_cache = null;
   }

   // Y overrides ---------------------------------------------------

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------
   protected void logActivation(Object id)
   {
      if( log.isTraceEnabled() )
      {
         StringBuffer m_buffer=new StringBuffer(100);
         m_buffer.append("Activated bean ");
         m_buffer.append(getContainer().getBeanMetaData().getEjbName());
         m_buffer.append(" with id = ");
         m_buffer.append(id);
         log.trace(m_buffer.toString());
      }
   }

   protected void logPassivation(Object id)
   {
      if( log.isTraceEnabled() )
      {
         StringBuffer m_buffer=new StringBuffer(100);
         m_buffer.append("Passivated bean ");
         m_buffer.append(getContainer().getBeanMetaData().getEjbName());
         m_buffer.append(" with id = ");
         m_buffer.append(id);
         log.trace(m_buffer.toString());
      }
   }

   /**
    * Returns the container for this cache.
    */
   protected abstract Container getContainer();
   /**
    * Returns the cache policy used for this cache.
    */
   protected CachePolicy getCache() {return m_cache;}
   /**
    * Returns the mutex used to sync access to the cache policy object
    */
   public Object getCacheLock()
   {
      return m_cacheLock;
   }
   /**
    * Passivates the given EnterpriseContext
    */
   protected abstract void passivate(EnterpriseContext ctx) throws RemoteException;
   /**
    * Activates the given EnterpriseContext
    */
   protected abstract void activate(EnterpriseContext ctx) throws RemoteException;
   /**
    * Acquires an EnterpriseContext from the pool
    */
   protected abstract EnterpriseContext acquireContext() throws Exception;
   /**
    * Frees the given EnterpriseContext to the pool
    */
   protected abstract void freeContext(EnterpriseContext ctx);
   /**
    * Returns the key used by the cache to map the given context
    */
   protected abstract Object getKey(EnterpriseContext ctx);
   /**
    * Sets the given id as key for the given context
    */
   protected abstract void setKey(Object id, EnterpriseContext ctx);
   /**
    * Returns whether the given context can be passivated or not
    *
    */
   protected abstract boolean canPassivate(EnterpriseContext ctx);

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}
