/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins;

import org.jboss.ejb.EnterpriseContext;
import org.jboss.ejb.StatelessSessionEnterpriseContext;

/**
 * A stateless session bean instance pool.
 *      
 * @version <tt>$Revision: 1.13.2.1 $</tt>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:andreas.schaefer@madplanet.com">Andreas Schaefer</a>
 * @author <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>
 */
public class StatelessSessionInstancePool
   extends AbstractInstancePool
{
   protected void createService() throws Exception
   {
      super.createService();

      // for SLSB, we *do* pool
      this.reclaim = true;
   }

   protected EnterpriseContext create(Object instance)
      throws Exception
   {
      return new StatelessSessionEnterpriseContext(instance, getContainer());
   }
}

