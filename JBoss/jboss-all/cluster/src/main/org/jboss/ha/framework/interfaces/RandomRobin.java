/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.framework.interfaces;

import java.util.ArrayList;
import java.util.Random;

import org.jboss.invocation.Invocation;

/**
 * LoadBalancingPolicy implementation that always fully randomly select its target
 * (without basing its decision on any historic).
 *
 * @author <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.6 $
 * @see org.jboss.ha.framework.interfaces.LoadBalancePolicy
 */
public class RandomRobin implements LoadBalancePolicy
{
   // Constants -----------------------------------------------------
   /** @since 1.1.2.3 */
   private static final long serialVersionUID = -3599638697906618428L;
   /** This needs to be a class variable or else you end up with multiple
    * Random numbers with the same seed when many clients lookup a proxy.
    */
   public static final Random localRandomizer = new Random (System.currentTimeMillis ());

   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
       
    // Public --------------------------------------------------------
   
   // LoadBalancePolicy implementation ----------------------------------------------

   public void init (HARMIClient father)
   {
      // do not use the HARMIClient in this policy
   }

   public Object chooseTarget (FamilyClusterInfo clusterFamily)
   {
      return chooseTarget(clusterFamily, null);
   }
   public Object chooseTarget (FamilyClusterInfo clusterFamily, Invocation routingDecision)
   {
      ArrayList targets = clusterFamily.getTargets ();
      int max = targets.size();

      if (max == 0)
         return null;

      int cursor = localRandomizer.nextInt (max);
      return targets.get(cursor);
   }

}
