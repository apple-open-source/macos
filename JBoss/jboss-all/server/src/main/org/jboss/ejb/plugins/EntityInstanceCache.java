/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins;

import java.rmi.RemoteException;
import java.rmi.NoSuchObjectException;

import org.jboss.ejb.Container;
import org.jboss.ejb.EntityContainer;
import org.jboss.ejb.EnterpriseContext;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.metadata.EntityMetaData;
import org.jboss.util.propertyeditor.PropertyEditors;

/**
 * Cache subclass for entity beans.
 * 
 * @author <a href="mailto:simone.bordet@compaq.com">Simone Bordet</a>
 * @author <a href="bill@burkecentral.com">Bill Burke</a>
 * @version $Revision: 1.17.2.7 $
 * @jmx:mbean extends="org.jboss.ejb.plugins.AbstractInstanceCacheMBean"
 */
public class EntityInstanceCache
   extends AbstractInstanceCache 
   implements org.jboss.ejb.EntityCache, EntityInstanceCacheMBean
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------
   /* The container */
   private EntityContainer m_container;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   // Public --------------------------------------------------------

   /* From ContainerPlugin interface */
   public void setContainer(Container c) 
   {
      m_container = (EntityContainer)c;
   }

   // Z implementation ----------------------------------------------
   public Object createCacheKey(Object id)
   {
      return id;
   }

   // Y overrides ---------------------------------------------------
   public EnterpriseContext get(Object id) 
      throws RemoteException, NoSuchObjectException 
   {
      EnterpriseContext rtn = null;
      rtn = super.get(id);
      return rtn;
   }

   /**
    * @jmx:managed-operation
    */
   public void remove(String id)
      throws Exception
   {
      EntityMetaData metaData = (EntityMetaData) m_container.getBeanMetaData();
      String primKeyClass = metaData.getPrimaryKeyClass();
      Object key = PropertyEditors.convertValue(id, primKeyClass);
      remove(key);
   }

   public void destroy()
   {
      synchronized( this )
      {
         this.m_container = null;
      }
      super.destroy();
   }

   protected Object getKey(EnterpriseContext ctx) 
   {
      return ((EntityEnterpriseContext)ctx).getCacheKey();
   }

   protected void setKey(Object id, EnterpriseContext ctx) 
   {
      ((EntityEnterpriseContext)ctx).setCacheKey(id);
      ctx.setId(id);
   }

   protected synchronized Container getContainer()
   {
      return m_container;
   }

   protected void passivate(EnterpriseContext ctx) throws RemoteException
   {
      m_container.getPersistenceManager().passivateEntity((EntityEnterpriseContext)ctx);
   }

   protected void activate(EnterpriseContext ctx) throws RemoteException
   {
      m_container.getPersistenceManager().activateEntity((EntityEnterpriseContext)ctx);
   }

   protected EnterpriseContext acquireContext() throws Exception
   {
      return m_container.getInstancePool().get();
   }

   protected void freeContext(EnterpriseContext ctx)
   {
      m_container.getInstancePool().free(ctx);
   }

   protected boolean canPassivate(EnterpriseContext ctx) 
   {
      if (ctx.isLocked()) 
      {
         // The context is in the interceptor chain
         return false;
      }

      if (ctx.getTransaction() != null)
      {
         return false;
      }

      Object key = ((EntityEnterpriseContext)ctx).getCacheKey();
      return m_container.getLockManager().canPassivate(key);
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}
