/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.ejb.plugins;

import org.jboss.ejb.EntityContainer;
import org.jboss.ejb.EntityPersistenceManager;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.ejb.InstancePool;
import org.jboss.invocation.Invocation;

/**
 * The instance interceptors role is to acquire a context representing
 * the target object from the cache.
 *
 * @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>
 * @version $Revision: 1.8.4.1 $
 */
public class EntityMultiInstanceInterceptor
   extends AbstractInterceptor
{
   // Constants -----------------------------------------------------
	
   // Attributes ----------------------------------------------------
	
   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
	
	// Public --------------------------------------------------------
	
   // Interceptor implementation --------------------------------------
	
   public Object invokeHome(Invocation mi)
      throws Exception
   {
      // Get context
      EntityContainer ec = (EntityContainer) getContainer();
      EntityEnterpriseContext ctx = (EntityEnterpriseContext) ec.getInstancePool().get();

		// Pass it to the method invocation
      mi.setEnterpriseContext(ctx);

      // Give it the transaction
      ctx.setTransaction(mi.getTransaction());

      // Set the current security information
      ctx.setPrincipal(mi.getPrincipal());

      // Invoke through interceptors
      return getNext().invokeHome(mi);
   }

   public Object invoke(Invocation mi)
      throws Exception
   {

      // The key
      Object key = mi.getId();

      EntityEnterpriseContext ctx = null;
      EntityContainer ec = (EntityContainer) container;
      if (mi.getTransaction() != null)
      {
         ctx = ec.getTxEntityMap().getCtx(mi.getTransaction(), key);
      }
      if (ctx == null)
      {
         InstancePool pool = ec.getInstancePool();
         ctx = (EntityEnterpriseContext) pool.get();
         ctx.setCacheKey(key);
         ctx.setId(key);
         EntityPersistenceManager pm = ec.getPersistenceManager();
         pm.activateEntity(ctx);
      }

      boolean trace = log.isTraceEnabled();
      if( trace ) log.trace("Begin invoke, key="+key);

      // Associate transaction, in the new design the lock already has the transaction from the
      // previous interceptor
      ctx.setTransaction(mi.getTransaction());

      // Set the current security information
      ctx.setPrincipal(mi.getPrincipal());

      // Set context on the method invocation
      mi.setEnterpriseContext(ctx);

      return getNext().invoke(mi);
   }

}
