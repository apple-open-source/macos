/**
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/

package org.jboss.cache.invalidation.triggers;

/**
 * The role of this interceptor is to detect that an entity has been modified
 * after an invocation has been performed an use the InvalidationsTxGrouper
 * static class so that invalidation messages can be groupped and
 * sent at transaction-commit time in a single batch.
 *
 * @author <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>
 * @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>
 * @version $Revision: 1.2.2.1 $
 */
public class EntityBeanCacheBatchInvalidatorInterceptor 
      extends org.jboss.ejb.plugins.AbstractInterceptor
{
   protected boolean doCacheInvalidations = true;
   protected org.jboss.cache.invalidation.InvalidationManagerMBean invalMgr = null;
   protected org.jboss.cache.invalidation.InvalidationGroup ig = null;
   protected org.jboss.ejb.EntityContainer container = null;
    
   public void start() throws Exception
   {
      org.jboss.metadata.EntityMetaData emd = ((org.jboss.metadata.EntityMetaData)this.getContainer ().getBeanMetaData ());      
      doCacheInvalidations = emd.doDistributedCacheInvalidations ();      
      
      if (doCacheInvalidations)
      {
         // we are interested in receiving cache invalidation callbacks
         //
         String groupName = emd.getDistributedCacheInvalidationConfig ().getInvalidationGroupName ();
         String imName = emd.getDistributedCacheInvalidationConfig ().getInvalidationManagerName ();
         
         this.invalMgr = (org.jboss.cache.invalidation.InvalidationManagerMBean)org.jboss.system.Registry.lookup (imName);         
         this.ig = this.invalMgr.getInvalidationGroup (groupName);
      }
   }      

   public void stop()
   {
      this.invalMgr = null;
      this.ig.removeReference (); // decrease the usage counter
      this.ig = null;
   }
 
   // Interceptor implementation --------------------------------------
 
   protected boolean changed (org.jboss.invocation.Invocation mi, org.jboss.ejb.EntityEnterpriseContext ctx) throws Exception
   {
      if (ctx.getId() == null) return true;

      if(!container.isReadOnly()) 
      {
         java.lang.reflect.Method method = mi.getMethod();
            if(method == null ||
               !container.getBeanMetaData().isMethodReadOnly(method.getName()))
            {
               if (container.getPersistenceManager().isModified(ctx))
               {
                  return true;
               }
            }
      }
      return false;
   }

   public Object invoke(org.jboss.invocation.Invocation mi) throws Exception
   {
      if (doCacheInvalidations)
      {
         // We are going to work with the context a lot
         org.jboss.ejb.EntityEnterpriseContext ctx = (org.jboss.ejb.EntityEnterpriseContext)mi.getEnterpriseContext();
         Object id = ctx.getId();

         // The Tx coming as part of the Method Invocation
         javax.transaction.Transaction tx = mi.getTransaction();

         // Invocation with a running Transaction
         if (tx != null && tx.getStatus() != javax.transaction.Status.STATUS_NO_TRANSACTION)
         {
            //Invoke down the chain
            Object retVal = getNext().invoke(mi);  
            //Object retVal = getNext().invoke(mi);  

            if (changed(mi, ctx))
            {
               org.jboss.cache.invalidation.InvalidationsTxGrouper.registerInvalidationSynchronization (tx, this.ig, (java.io.Serializable)id);
            }

            // return the return value
            return retVal;
         }
         //
         else
         { // No tx
            Object result = getNext().invoke(mi);

            if (changed(mi, ctx))
            {
               org.jboss.cache.invalidation.InvalidationsTxGrouper.registerInvalidationSynchronization (tx, this.ig, (java.io.Serializable)id);
            }
            return result;
         }
      }
      else
      {
         return getNext().invoke(mi);
      }
   }

   public void setContainer(org.jboss.ejb.Container container) { this.container = (org.jboss.ejb.EntityContainer)container; }

   public org.jboss.ejb.Container getContainer() { return container; }
   
}
