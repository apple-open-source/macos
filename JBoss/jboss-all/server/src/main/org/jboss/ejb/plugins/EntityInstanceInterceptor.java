/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.ejb.plugins;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.rmi.RemoteException;

import javax.transaction.Transaction;

import org.jboss.ejb.BeanLock;
import org.jboss.ejb.Container;
import org.jboss.ejb.EntityContainer;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.ejb.InstanceCache;
import org.jboss.invocation.Invocation;
import org.jboss.util.NestedRuntimeException;

/**
* The instance interceptors role is to acquire a context representing
* the target object from the cache.
*
* <p>This particular container interceptor implements pessimistic locking
*    on the transaction that is associated with the retrieved instance.  If
*    there is a transaction associated with the target component and it is
*    different from the transaction associated with the Invocation
*    coming in then the policy is to wait for transactional commit. 
*   
* <p>We also implement serialization of calls in here (this is a spec
*    requirement). This is a fine grained notify, notifyAll mechanism. We
*    notify on ctx serialization locks and notifyAll on global transactional
*    locks.
*   
* <p><b>WARNING: critical code</b>, get approval from senior developers
*    before changing.
*    
* @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
* @author <a href="mailto:Scott.Stark@jboss.org">Scott Stark</a>
* @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>
* @version $Revision: 1.53.2.9 $
*/
public class EntityInstanceInterceptor
   extends AbstractInterceptor
{
   // Constants -----------------------------------------------------
	
   // Attributes ----------------------------------------------------
	
   protected EntityContainer container;
	
   // Static --------------------------------------------------------	
   // Constructors --------------------------------------------------
	
	// Public --------------------------------------------------------
	
   public void setContainer(Container container)
   {
      this.container = (EntityContainer)container;
   }
	
   public Container getContainer()
   {
      return container;
   }

   // Interceptor implementation --------------------------------------

   public Object invokeHome(Invocation mi)
      throws Exception
   {
      // Get context
      EntityContainer container = (EntityContainer) getContainer();
      EntityEnterpriseContext ctx = (EntityEnterpriseContext) container.getInstancePool().get();

      // Pass it to the method invocation
      mi.setEnterpriseContext(ctx);

      // Give it the transaction
      ctx.setTransaction(mi.getTransaction());

      // Set the current security information
      ctx.setPrincipal(mi.getPrincipal());

         // Invoke through interceptors
      Object rtn = getNext().invokeHome(mi);
      // Is the context now with an identity? in which case we need to insert
      if (ctx.getId() != null)
      {

         BeanLock lock = container.getLockManager().getLock(ctx.getCacheKey());
         lock.sync(); // lock all access to BeanLock
         try
         {
            // Check there isn't a context already in the cache
            // e.g. commit-option B where the entity was
            // created then removed externally
            InstanceCache cache = container.getInstanceCache();
            cache.remove(ctx.getCacheKey());

            // marcf: possible race on creation and usage
            // insert instance in cache,
            cache.insert(ctx);
         }
         finally
         {
            lock.releaseSync();
            container.getLockManager().removeLockRef(ctx.getCacheKey());
         }
      }
      else
      {
         // It was just a home invocation, put the context back in the pool
         ctx.setTransaction(null);
         container.getInstancePool().free(ctx);
         mi.setEnterpriseContext(null); // EntityCreateInterceptor will access ctx if it is not null and call postCreate
      }
      return rtn;
   }

   public Object invoke(Invocation mi)
      throws Exception
   {
      boolean trace = log.isTraceEnabled();

      // The key
      Object key = mi.getId();

      // The context
      EntityEnterpriseContext ctx = (EntityEnterpriseContext) container.getInstanceCache().get(key);

      if( trace ) log.trace("Begin invoke, key="+key);

      // Associate transaction, in the new design the lock already has the transaction from the
      // previous interceptor

      // Don't set the transction if a read-only method.  With a read-only method, the ctx can be shared
      // between multiple transactions.
      Transaction tx = (Transaction) mi.getTransaction();
      if(!container.isReadOnly()) 
      {
         Method method = mi.getMethod();
         if(method == null ||
            !container.getBeanMetaData().isMethodReadOnly(method.getName()))
         {
            ctx.setTransaction(tx);
         }
      }

      // Set the current security information
      ctx.setPrincipal(mi.getPrincipal());

      // Set context on the method invocation
      mi.setEnterpriseContext(ctx);

      Throwable exceptionThrown = null;

      try
      {
         return getNext().invoke(mi);
      }
      catch (RemoteException e)
      {
         exceptionThrown = e;
         throw e;
      }
      catch (RuntimeException e)
      {
         exceptionThrown = e;
         throw e;
      }
      catch (Error e)
      {
         exceptionThrown = e;
         throw e;
      }
      catch (Exception e)
      {
         exceptionThrown = e;
         throw e;
      }
      catch (Throwable e)
      {
         exceptionThrown = e;
         throw new NestedRuntimeException(e);
      }
      finally
      {
         // ctx can be null if cache.get throws an Exception, for
         // example when activating a bean.
         if (ctx != null)
         {
            // Make sure we clear the transaction on an error before synchronization.
            // But avoid a race with a transaction rollback on a synchronization
            // that may have moved the context onto a different transaction
            if (exceptionThrown != null && tx != null)
            {
               Transaction ctxTx = ctx.getTransaction();
               if (tx.equals(ctxTx) && ctx.hasTxSynchronization() == false)
                  ctx.setTransaction(null);
            }

				// If an exception has been thrown,
            if (exceptionThrown != null &&
                // if tx, the ctx has been registered in an InstanceSynchronization.
                // that will remove the context, so we shouldn't.
                // if no synchronization then we need to do it by hand
                !ctx.hasTxSynchronization())
            {
               // Discard instance
               // EJB 1.1 spec 12.3.1
               container.getInstanceCache().remove(key);

               if( trace ) log.trace("Ending invoke, exceptionThrown, ctx="+ctx, exceptionThrown);
            }
            else if (ctx.getId() == null)
            {
               // The key from the Invocation still identifies the right cachekey
               container.getInstanceCache().remove(key);

               if( trace )	log.trace("Ending invoke, cache removal, ctx="+ctx);
               // no more pool return
            }
         }

         if( trace )	log.trace("End invoke, key="+key+", ctx="+ctx);

      }	// end invoke
   }
}



