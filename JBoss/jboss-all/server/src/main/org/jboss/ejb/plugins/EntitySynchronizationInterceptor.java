/**
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import javax.ejb.EJBException;
import javax.transaction.RollbackException;
import javax.transaction.Status;
import javax.transaction.Synchronization;
import javax.transaction.Transaction;

import org.jboss.ejb.BeanLock;
import org.jboss.ejb.Container;
import org.jboss.ejb.EntityCache;
import org.jboss.ejb.EntityContainer;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.invocation.Invocation;
import org.jboss.metadata.ConfigurationMetaData;
import org.jboss.util.NestedRuntimeException;

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
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:Scott.Stark@jboss.org">Scott Stark</a>
 * @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>
 * @version $Revision: 1.67.2.14 $
 */
public class EntitySynchronizationInterceptor
        extends AbstractInterceptor
{
   /**
    * The variable <code>vcrThread</code> holds the valid context refresher thread for
    * this interceptor, so it may be shut down in stop.
    */
   private Thread vcrThread;

   /**
    *  The current commit option.
    */
   protected int commitOption;

   /**
    *  The refresh rate for commit option d
    */
   protected long optionDRefreshRate;

   /**
    *  The container of this interceptor.
    */
   protected EntityContainer container;

   public Container getContainer()
   {
      return container;
   }

   public void setContainer(Container container)
   {
      this.container = (EntityContainer) container;
   }

   public void create()
           throws Exception
   {

      try
      {
         ConfigurationMetaData configuration = container.getBeanMetaData().getContainerConfiguration();
         commitOption = configuration.getCommitOption();
         optionDRefreshRate = configuration.getOptionDRefreshRate();

      }
      catch (Exception e)
      {
         log.warn(e.getMessage());
      }
   }

   public void start()
   {
      try
      {
         //start up the validContexts thread if commit option D
         if (commitOption == ConfigurationMetaData.D_COMMIT_OPTION)
         {
            ValidContextsRefresher vcr = new ValidContextsRefresher(optionDRefreshRate);
            vcrThread = new Thread(vcr);
            vcrThread.start();
         }
      }
      catch (Exception e)
      {
         vcrThread = null;
         log.warn("problem starting valid contexts refresher thread", e);
      } // end of try-catch
   }

   public void stop()
   {
      if (vcrThread != null)
      {
         Thread temp = vcrThread;
         vcrThread = null;
         temp.interrupt();
      } // end of if ()

   }

   protected Synchronization createSynchronization(Transaction tx, EntityEnterpriseContext ctx)
   {
      return new InstanceSynchronization(tx, ctx);
   }

   /**
    *  Register a transaction synchronization callback with a context.
    */
   protected void register(EntityEnterpriseContext ctx, Transaction tx)
   {
      boolean trace = log.isTraceEnabled();
      if (trace)
         log.trace("register, ctx=" + ctx + ", tx=" + tx);

      EntityContainer ctxContainer = null;
      try
      {
         ctxContainer = (EntityContainer) ctx.getContainer();
         if (!ctx.hasTxSynchronization())
         {
            // Create a new synchronization
            Synchronization synch = createSynchronization(tx, ctx);

            // We want to be notified when the transaction commits
            tx.registerSynchronization(synch);

            // associate the entity bean with the transaction so that
            // we can do things like synchronizeEntitiesWithinTransaction
            // do this after registerSynchronization, just in case there was an exception
            ctxContainer.getTxEntityMap().associate(tx, ctx);

            ctx.hasTxSynchronization(true);
         }
         //mark it dirty in global tx entity map if it is not read only
         if (!ctxContainer.isReadOnly())
         {
            EntityContainer.getGlobalTxEntityMap().associate(tx, ctx);
         }
      }
      catch (RollbackException e)
      {
         // The state in the instance is to be discarded, we force a reload of state
         synchronized (ctx)
         {
            ctx.setValid(false);
            ctx.hasTxSynchronization(false);
            ctx.setTransaction(null);
         }
         throw new EJBException(e);
      }
      catch (Throwable t)
      {
         // If anything goes wrong with the association remove the ctx-tx association
         ctx.hasTxSynchronization(false);
         if (t instanceof RuntimeException)
            throw (RuntimeException) t;
         else if (t instanceof Error)
            throw (Error) t;
         else if (t instanceof Exception)
            throw new EJBException((Exception) t);
         else
            throw new NestedRuntimeException(t);
      }
   }

   public Object invokeHome(Invocation mi) throws Exception
   {

      EntityEnterpriseContext ctx = (EntityEnterpriseContext) mi.getEnterpriseContext();
      Transaction tx = mi.getTransaction();

      Object rtn = getNext().invokeHome(mi);

      // An anonymous context was sent in, so if it has an id it is a real instance now
      if (ctx.getId() != null)
      {

         // it doesn't need to be read, but it might have been changed from the db already.
         ctx.setValid(true);

         if (tx != null)
         {
            BeanLock lock = container.getLockManager().getLock(ctx.getCacheKey());
            try
            {
               lock.schedule(mi);
               register(ctx, tx); // Set tx
               lock.endInvocation(mi);
            }
            finally
            {
               container.getLockManager().removeLockRef(lock.getId());
            }
         }
      }
      return rtn;
   }

   public Object invoke(Invocation mi) throws Exception
   {
      // We are going to work with the context a lot
      EntityEnterpriseContext ctx = (EntityEnterpriseContext) mi.getEnterpriseContext();

      // The Tx coming as part of the Method Invocation
      Transaction tx = mi.getTransaction();

      if (log.isTraceEnabled())
         log.trace("invoke called for ctx " + ctx + ", tx=" + tx);

      // Is my state valid?
      if (!ctx.isValid())
      {
         // If not tell the persistence manager to load the state
         container.getPersistenceManager().loadEntity(ctx);

         // Now the state is valid
         ctx.setValid(true);
      }

      // mark the context as read only if this is a readonly method and the context
      // was not already readonly
      boolean didSetReadOnly = false;
      if (!ctx.isReadOnly() &&
              (container.isReadOnly() ||
              container.getBeanMetaData().isMethodReadOnly(mi.getMethod())))
      {
         ctx.setReadOnly(true);
         didSetReadOnly = true;
      }

      // So we can go on with the invocation

      // Invocation with a running Transaction
      try
      {
         if (tx != null && tx.getStatus() != Status.STATUS_NO_TRANSACTION)
         {
            // readonly does not synchronize, lock or belong with transaction.
            boolean isReadOnly = container.isReadOnly();
            if (isReadOnly == false);
            {
               Method method = mi.getMethod();
               if (method != null)
                  isReadOnly = container.getBeanMetaData().isMethodReadOnly(method.getName());
            }
            try
            {
               if (isReadOnly == false)
               {
                  // register the wrapper with the transaction monitor (but only
                  // register once). The transaction demarcation will trigger the
                  // storage operations
                  register(ctx, tx);
               }

               //Invoke down the chain
               Object retVal = getNext().invoke(mi);

               // Register again as a finder in the middle of a method
               // will de-register this entity, and then the rest of the method can
               // change fields which will never be stored
               if (isReadOnly == false)
               {
                  // register the wrapper with the transaction monitor (but only
                  // register once). The transaction demarcation will trigger the
                  // storage operations
                  register(ctx, tx);
               }

               // return the return value
               return retVal;
            }
            finally
            {
               // We were read-only and the context wasn't already synchronized, tidyup the cache
               if (isReadOnly && ctx.hasTxSynchronization() == false)
               {
                  switch (commitOption)
                  {
                  // Keep instance active, but invalidate state
                  case ConfigurationMetaData.B_COMMIT_OPTION:
                     // Invalidate state (there might be other points of entry)
                     ctx.setValid(false);
                     break;

                     // Invalidate everything AND Passivate instance
                  case ConfigurationMetaData.C_COMMIT_OPTION:
                     try
                     {
                        // FIXME: We cannot passivate here, because previous
                        // interceptors work with the context, in particular
                        // the re-entrance interceptor is doing lock counting
                        // Just remove it from the cache
                        if (ctx.getId() != null) 
                           container.getInstanceCache().remove(ctx.getId());
                     }
                     catch (Exception e)
                     {
                        log.debug("Exception releasing context", e);
                     }
                     break;
                  }
               }
            }
         }
         else
         {
            // No tx
            try
            {

               Object result = getNext().invoke(mi);

               // Store after each invocation -- not on exception though, or removal
               // And skip reads too ("get" methods)
               if (ctx.getId() != null)
               {
                  if (!container.isReadOnly())
                  {
                     container.storeEntity(ctx);
                  }
               }

               return result;
            }
            catch (Exception e)
            {
               // Exception - force reload on next call
               ctx.setValid(false);
               throw e;
            }
            finally
            {
               switch (commitOption)
               {
                  // Keep instance active, but invalidate state
                  case ConfigurationMetaData.B_COMMIT_OPTION:
                     // Invalidate state (there might be other points of entry)
                     ctx.setValid(false);
                     break;

                     // Invalidate everything AND Passivate instance
                  case ConfigurationMetaData.C_COMMIT_OPTION:
                     try
                     {
                        // Do not call release if getId() is null.  This means that
                        // the entity has been removed from cache.
                        // release will schedule a passivation and this removed ctx
                        // could be put back into the cache!
                        // This is necessary because we have no lock, we
                        // don't want to return an instance to the pool that is
                        // being used
                        if (ctx.getId() != null) 
                           container.getInstanceCache().release(ctx);
                     }
                     catch (Exception e)
                     {
                        log.debug("Exception releasing context", e);
                     }
                     break;
               }
            }
         }
      }
      finally
      {
         // if we marked the context as read only we need to reset it
         if (didSetReadOnly)
         {
            ctx.setReadOnly(false);
         }
      }
   }

   protected class InstanceSynchronization
           implements Synchronization
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
       * The context lock
       */
      protected BeanLock lock;

      /**
       *  Create a new instance synchronization instance.
       */
      InstanceSynchronization(Transaction tx, EntityEnterpriseContext ctx)
      {
         this.tx = tx;
         this.ctx = ctx;
         this.lock = container.getLockManager().getLock(ctx.getCacheKey());
      }

      public void beforeCompletion()
      {
         //synchronization is handled by GlobalTxEntityMap.
      }

      public void afterCompletion(int status)
      {
         boolean trace = log.isTraceEnabled();

         // This is an independent point of entry. We need to make sure the
         // thread is associated with the right context class loader
         Thread currentThread = Thread.currentThread();
         ClassLoader oldCl = currentThread.getContextClassLoader();
         currentThread.setContextClassLoader(container.getClassLoader());

         lock.sync();
         // The context is no longer synchronized on the TX
         ctx.hasTxSynchronization(false);
         ctx.setTransaction(null);
         try
         {
            try
            {
               // If rolled back -> invalidate instance
               if (status == Status.STATUS_ROLLEDBACK)
               {
                  // remove from the cache
                  container.getInstanceCache().remove(ctx.getCacheKey());
               }
               else
               {
                  switch (commitOption)
                  {
                     // Keep instance cached after tx commit
                     case ConfigurationMetaData.A_COMMIT_OPTION:
                     case ConfigurationMetaData.D_COMMIT_OPTION:
                        // The state is still valid (only point of access is us)
                        ctx.setValid(true);
                        break;

                        // Keep instance active, but invalidate state
                     case ConfigurationMetaData.B_COMMIT_OPTION:
                        // Invalidate state (there might be other points of entry)
                        ctx.setValid(false);
                        break;
                        // Invalidate everything AND Passivate instance
                     case ConfigurationMetaData.C_COMMIT_OPTION:
                        try
                        {
                           // We weren't removed, passivate
                           // Here we own the lock, so we don't try to passivate
                           // we just passivate
                           if (ctx.getId() != null)
                           {
                              container.getInstanceCache().remove(ctx.getId());
                              container.getPersistenceManager().passivateEntity(ctx);
                           } 
                           // If we get this far, we return to the pool
                           container.getInstancePool().free(ctx);
                        }
                        catch (Exception e)
                        {
                           log.debug("Exception releasing context", e);
                        }
                        break;
                  }
               }
            }
            finally
            {
               if (trace)
                  log.trace("afterCompletion, clear tx for ctx=" + ctx + ", tx=" + tx);
               lock.endTransaction(tx);

               if (trace)
                  log.trace("afterCompletion, sent notify on TxLock for ctx=" + ctx);
            }
         } // synchronized(lock)
         finally
         {
            lock.releaseSync();
            container.getLockManager().removeLockRef(lock.getId());
            currentThread.setContextClassLoader(oldCl);
         }
      }

   }

   class ValidContextsRefresher implements Runnable
   {
      private long refreshRate;

      public ValidContextsRefresher(long refreshRate)
      {
         this.refreshRate = refreshRate;
      }

      public void run()
      {
         while (vcrThread != null)
         {
            if (log.isTraceEnabled())
            {
               log.trace("Flushing the valid contexts");
            }

            try
            {
               Thread.sleep(refreshRate);
            }
            catch (InterruptedException ignored)
            {
            }

            // Guard against NPE at shutdown           
            if (container != null)
            {
               EntityCache cache = (EntityCache) container.getInstanceCache();
               if (cache != null)
                  cache.flush();
            }
         }
      }
   }
}
