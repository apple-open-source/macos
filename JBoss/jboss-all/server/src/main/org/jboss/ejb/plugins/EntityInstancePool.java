/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins;

import org.jboss.ejb.EnterpriseContext;
import org.jboss.ejb.EntityEnterpriseContext;

/**
 * An entity bean instance pool.
 *
 * @version <tt>$Revision: 1.21.2.2 $</tt>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:andreas.schaefer@madplanet.com">Andreas Schaefer</a>
 * @author <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>
 */
public class EntityInstancePool
   extends AbstractInstancePool
{
   /**
    * Return an instance to the free pool. Reset state
    *
    * <p>Called in 3 cases:
    * <ul>
    *   <li>Done with finder method
    *   <li>Removed
    *   <li>Passivated
    * </ul>
    *
    * @param   ctx  
    */
   public void free(EnterpriseContext ctx)
   {
       // If transaction still present don't do anything (let the instance be GC)
       if (ctx.getTransaction() != null)
       {
          if( log.isTraceEnabled() )
             log.trace("Can Not FREE Entity Context because a Transaction exists.");
          return ;
       }

       super.free(ctx);
   }

   protected EnterpriseContext create(Object instance)
      throws Exception
   {
      return new EntityEnterpriseContext(instance, getContainer());
   }
}
