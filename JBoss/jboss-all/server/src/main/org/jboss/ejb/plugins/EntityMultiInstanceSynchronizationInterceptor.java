/**
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins;


import javax.transaction.Status;
import javax.transaction.Synchronization;
import javax.transaction.Transaction;

import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.metadata.ConfigurationMetaData;

/**
 * The role of this interceptor is to synchronize the state of the cache with
 * the underlying storage.  It does this with the ejbLoad and ejbStore
 * semantics of the EJB specification.  In the presence of a transaction this
 * is triggered by transaction demarcation. It registers a callback with the
 * underlying transaction monitor through the JTA interfaces.  If there is no
 * transaction the policy is to store state upon returning from invocation.
 * The synchronization polices A,B,C of the specification are taken care of
 * here.
 *
 * <p><b>WARNING: critical code</b>, get approval from senior developers
 *    before changing.
 *
 * @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>
 * @version $Revision: 1.5.4.7 $
 */
public class EntityMultiInstanceSynchronizationInterceptor
        extends EntitySynchronizationInterceptor
{
   public void create()
           throws Exception
   {
      super.create();
   }

   public void start()
   {
      // This is in here so that EntityMultiInstanceInterceptor can avoid doing a lock.sync().  
      // 
      if (!container.getLockManager().lockClass.equals(org.jboss.ejb.plugins.lock.NoLock.class)
          && !container.getLockManager().lockClass.equals(org.jboss.ejb.plugins.lock.JDBCOptimisticLock.class)
          && !container.getLockManager().lockClass.equals(org.jboss.ejb.plugins.lock.MethodOnlyEJBLock.class)
          )
      {
         throw new IllegalStateException("the <locking-policy> must be org.jboss.ejb.plugins.lock.NoLock, JDBCOptimisticLock, or MethodOnlyEJBLock for Instance Per Transaction:"
                                          + container.getLockManager().lockClass.getName());
      }
   }

   protected Synchronization createSynchronization(Transaction tx, EntityEnterpriseContext ctx)
   {
      return new MultiInstanceSynchronization(tx, ctx);
   }
   // Protected  ----------------------------------------------------

   // Inner classes -------------------------------------------------

   protected class MultiInstanceSynchronization implements Synchronization
   {
      /**
       *  The transaction we follow.
       */
      protected Transaction tx;

      /**
       *  The context we manage.
       */
      protected EntityEnterpriseContext ctx;

      /**
       *  Create a new instance synchronization instance.
       */
      MultiInstanceSynchronization(Transaction tx, EntityEnterpriseContext ctx)
      {
         this.tx = tx;
         this.ctx = ctx;
      }

      // Synchronization implementation -----------------------------

      public void beforeCompletion()
      {
         //synchronization is handled by GlobalTxEntityMap.
      }

      public void afterCompletion(int status)
      {
         boolean trace = log.isTraceEnabled();

         // This is an independent point of entry. We need to make sure the
         // thread is associated with the right context class loader
         ClassLoader oldCl = Thread.currentThread().getContextClassLoader();
         Thread.currentThread().setContextClassLoader(container.getClassLoader());

         ctx.hasTxSynchronization(false);
         ctx.setTransaction(null);
         try
         {
            try
            {
               // If rolled back -> invalidate instance
               if (status != Status.STATUS_ROLLEDBACK)
               {
                  switch (commitOption)
                  {
                     // Keep instance cached after tx commit
                     case ConfigurationMetaData.A_COMMIT_OPTION:
                        throw new IllegalStateException("Commit option A not allowed with this Interceptor");
                        // Keep instance active, but invalidate state
                     case ConfigurationMetaData.B_COMMIT_OPTION:
                        break;
                        // Invalidate everything AND Passivate instance
                     case ConfigurationMetaData.C_COMMIT_OPTION:
                        break;
                     case ConfigurationMetaData.D_COMMIT_OPTION:
                        throw new IllegalStateException("Commit option D not allowed with this Interceptor");
                  }
               }
               try
               {
                  if (ctx.getId() != null)
                     container.getPersistenceManager().passivateEntity(ctx);
               }
               catch (Exception ignored)
               {
               }
               container.getInstancePool().free(ctx);
            }
            finally
            {
               if (trace)
                  log.trace("afterCompletion, clear tx for ctx=" + ctx + ", tx=" + tx);

            }
         } // synchronized(lock)
         finally
         {
            Thread.currentThread().setContextClassLoader(oldCl);
         }
      }

   }

}
