/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins;

import java.rmi.RemoteException;

import org.jboss.ejb.Container;
import org.jboss.invocation.Invocation;
import org.jboss.ejb.EnterpriseContext;
import org.jboss.ejb.StatelessSessionContainer;
import org.jboss.ejb.InstancePool;

/**
 * This container acquires the given instance. This must be used after
 * the EnvironmentInterceptor, since acquiring instances requires a proper
 * JNDI environment to be set
 *
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @version $Revision: 1.13.4.3 $
 */
public class StatelessSessionInstanceInterceptor
   extends AbstractInterceptor
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------

   protected StatelessSessionContainer container;
   
   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------

   public void setContainer(final Container container) 
   {
      super.setContainer(container);
      this.container = (StatelessSessionContainer)container;
   }

   // Interceptor implementation --------------------------------------
   
   public Object invokeHome(final Invocation mi) throws Exception
   {
      // We don't need an instance since the call will be handled by container
      return getNext().invokeHome(mi);
   }

   public Object invoke(final Invocation mi) throws Exception
   {
      // Get context
      InstancePool pool = container.getInstancePool();
      EnterpriseContext ctx = pool.get();

      // Set the current security information
      ctx.setPrincipal(mi.getPrincipal());

      // Use this context
      mi.setEnterpriseContext(ctx);

      // There is no need for synchronization since the instance is always fresh also there should
      // never be a tx associated with the instance.
	 
      try
      {
         // Invoke through interceptors
         return getNext().invoke(mi);
      } catch (RuntimeException e) // Instance will be GC'ed at MI return
      {
         mi.setEnterpriseContext(null);
         throw e;
      } catch (RemoteException e) // Instance will be GC'ed at MI return
      {
         mi.setEnterpriseContext(null);
         throw e;
      } catch (Error e) // Instance will be GC'ed at MI return
      {
         mi.setEnterpriseContext(null);
         throw e;
      } finally
      {
         // Return context
         if ( mi.getEnterpriseContext() != null)
         {
            pool.free(((EnterpriseContext) mi.getEnterpriseContext()));
         }
         else
         {
            pool.discard(ctx);
         }
      }
   }
}
