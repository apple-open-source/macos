/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins;

import org.jboss.ejb.EnterpriseContext;
import org.jboss.ejb.StatefulSessionEnterpriseContext;

/**
 * A stateful session bean instance pool.
 *
 * @version <tt>$Revision: 1.10.2.1 $</tt>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:andreas.schaefer@madplanet.com">Andreas Schaefer</a>
 */
public class StatefulSessionInstancePool
   extends AbstractInstancePool
{
   public synchronized void free(EnterpriseContext ctx)
   {
      discard(ctx);
   }
	
   protected EnterpriseContext create(Object instance)
      throws Exception
   {
      // The instance is created by the caller and is a newInstance();
      return new StatefulSessionEnterpriseContext(instance, getContainer());
   }
}

