/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb;

import org.jboss.ejb.plugins.lock.NonReentrantLock;

import java.rmi.RemoteException;

import javax.ejb.EJBContext;
import javax.ejb.EJBObject;
import javax.ejb.EJBLocalObject;
import javax.ejb.EJBLocalObject;
import javax.ejb.EJBObject;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;


/**
 * The EntityEnterpriseContext is used to associate EntityBean instances
 * with metadata about it.
 *
 * @see EnterpriseContext
 * 
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard �berg</a>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:docodan@mvcsoft.com">Daniel OConnor</a>
 * @version $Revision: 1.27.2.3 $
 */
public class EntityEnterpriseContext extends EnterpriseContext
{
   private EJBObject ejbObject;
   private EJBLocalObject ejbLocalObject;
   private EntityContext ctx;
	
   /**
    * True if this instance has been registered with the TM for transactional
    * demarcation.
    */
   private boolean hasTxSynchronization = false;
	
   /**
    * True if this instances' state is valid when a bean is called the state
    * is not synchronized with the DB but "valid" as long as the transaction
    * runs.
    */
   private boolean valid = false;
	
   /**
    * Is this context in a readonly invocation.
    */
   private boolean readOnly = false;

   /**
    * The persistence manager may attach any metadata it wishes to this
    * context here.
    */
   private Object persistenceCtx;
	
   /** The cacheKey for this context */
   private Object key;

   private NonReentrantLock methodLock = new NonReentrantLock();

   public EntityEnterpriseContext(Object instance, Container con)
      throws RemoteException
   {
      super(instance, con);
      ctx = new EntityContextImpl();
      ((EntityBean)instance).setEntityContext(ctx);
   }
	
   /**
    * A non-reentrant deadlock detectable lock that is used to protected against
    * entity bean reentrancy.
    * @return
    */
   public NonReentrantLock getMethodLock()
   {
      return methodLock;
   }

   public void clear()
   {
      super.clear();
      
      hasTxSynchronization = false;
      valid = false;
      readOnly = false;
      key = null;
      persistenceCtx = null;
      ejbObject = null;
   }
	
   public void discard() throws RemoteException
   {
      ((EntityBean)instance).unsetEntityContext();
   }
	
   public EJBContext getEJBContext()
   {
      return ctx;
   }
	
   public void setEJBObject(EJBObject eo)
   {
      ejbObject = eo;
   }
	
   public EJBObject getEJBObject()
   {
      // Context can have no EJBObject (created by finds) in which case
      // we need to wire it at call time
      return ejbObject;
   }
	
   public void setEJBLocalObject(EJBLocalObject eo)
   {
      ejbLocalObject = eo;
   }
	
   public EJBLocalObject getEJBLocalObject()
   {
      return ejbLocalObject;
   }
	
   public void setCacheKey(Object key)
   {
      this.key = key;
   }
	
   public Object getCacheKey()
   {
      return key;
   }
	
   public void setPersistenceContext(Object ctx)
   {
      this.persistenceCtx = ctx;
   }
	
   public Object getPersistenceContext()
   {
      return persistenceCtx;
   }
	
   public void hasTxSynchronization(boolean value)
   {
      hasTxSynchronization = value;
   }
	
   public boolean hasTxSynchronization()
   {
      return hasTxSynchronization;
   }
	
   public void setValid(boolean valid)
   {
      this.valid = valid;
   }
	
   public boolean isValid()
   {
      return valid;
   }

	public void setReadOnly(boolean readOnly)
   {
      this.readOnly = readOnly;
   }
	
   public boolean isReadOnly()
   {
      return readOnly;
   }
	
   protected class EntityContextImpl
      extends EJBContextImpl
      implements EntityContext
   {
      public EJBObject getEJBObject()
      {
         if(((EntityContainer)con).getRemoteClass() == null)
         {
            throw new IllegalStateException( "No remote interface defined." );
         }

         if (ejbObject == null)
         {   
            // Create a new CacheKey
            Object cacheKey = ((EntityCache)((EntityContainer)con).getInstanceCache()).createCacheKey(id);
            EJBProxyFactory proxyFactory = con.getProxyFactory();
            if(proxyFactory == null)
            {
               String defaultInvokerName = con.getBeanMetaData().
                  getContainerConfiguration().getDefaultInvokerName();
               proxyFactory = con.lookupProxyFactory(defaultInvokerName);
            }
            ejbObject = (EJBObject)proxyFactory.getEntityEJBObject(cacheKey);
         }

         return ejbObject;
      }
		
      public EJBLocalObject getEJBLocalObject()
      {
         if (con.getLocalHomeClass()==null)
            throw new IllegalStateException( "No local interface for bean." );
         
         if (ejbLocalObject == null)
         {
            Object cacheKey = ((EntityCache)((EntityContainer)con).getInstanceCache()).createCacheKey(id);            
            ejbLocalObject = ((EntityContainer)con).getLocalProxyFactory().getEntityEJBLocalObject(cacheKey);
         }
         return ejbLocalObject;
      }
		
      public Object getPrimaryKey()
      {
         return id;
      }
   }
}
