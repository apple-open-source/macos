/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.framework.interfaces;

import java.util.ArrayList;
import org.jboss.invocation.Invocation;

/**
 * LoadBalancingPolicy implementation that always favor the next available
 * target load balancing always occurs.
 *
 * @see org.jboss.ha.framework.interfaces.LoadBalancePolicy
 *
 * @author <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>.
 * @version $Revision: 1.3.4.5 $
 */
public class RoundRobin implements LoadBalancePolicy
{
   // Constants -----------------------------------------------------
   /** @since 1.3.4.2 */
   private static final long serialVersionUID = 8660076707279597114L;

   // Attributes ----------------------------------------------------
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
       
   // Public --------------------------------------------------------
   
   public void init (HARMIClient father)
   {
      // do not use the HARMIClient in this policy
   }

   public Object chooseTarget (FamilyClusterInfo clusterFamily)
   {
      return this.chooseTarget(clusterFamily, null);
   }
   public Object chooseTarget (FamilyClusterInfo clusterFamily, Invocation routingDecision)
   {
      int cursor = clusterFamily.getCursor ();
      ArrayList targets = clusterFamily.getTargets ();

      if (targets.size () == 0)
         return null;
      
      if (cursor == FamilyClusterInfo.UNINITIALIZED_CURSOR)
      {         
         // Obtain a random index into targets
         cursor = RandomRobin.localRandomizer.nextInt(targets.size());
      }
      else
      {
         // Choose the next target
         cursor = ( (cursor + 1) % targets.size() );
      }
      clusterFamily.setCursor (cursor);

      return targets.get(cursor);
   }

}
