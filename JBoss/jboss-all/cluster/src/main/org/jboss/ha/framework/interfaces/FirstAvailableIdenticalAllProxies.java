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
 * LoadBalancingPolicy implementation that always favor the first available target i.e.
 * no load balancing occurs. Nevertheless, the first target is randomly selected.
 * This does not mean that fail-over will not occur if the
 * first member in the list dies. In this case, fail-over will occur, and a new target
 * will become the first member and invocation will continously be invoked on the same
 * new target until its death. Each proxy using this policy will *not* elect its own
 * prefered target: the target *is* shared with all proxies that belong to the same family 
 * (for a different behaviour please take a look at FirstAvailable)
 *
 * @see org.jboss.ha.framework.interfaces.LoadBalancePolicy
 *
 * @author <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.6 $
 */
public class FirstAvailableIdenticalAllProxies implements LoadBalancePolicy
{
   // Constants -----------------------------------------------------
   private static final long serialVersionUID = 2910756623413400467L;

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
      return chooseTarget(clusterFamily, null);
   }
   public Object chooseTarget (FamilyClusterInfo clusterFamily, Invocation routingDecision)
   {
      Object target = clusterFamily.getObject ();
      ArrayList targets = clusterFamily.getTargets ();

      if (targets.size () == 0)
         return null;
      
      if (target != null && targets.contains (target) )
      {
         return target;
      }
      else
      {
         int cursor = RandomRobin.localRandomizer.nextInt (targets.size());
         target = targets.get(cursor);
         clusterFamily.setObject (target);
         return target;
      }   
   }

}
