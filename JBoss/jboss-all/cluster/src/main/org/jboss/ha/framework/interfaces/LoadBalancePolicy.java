/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.framework.interfaces;

import org.jboss.invocation.Invocation;

/**
 * Base interface for load-balancing policies. It is possible to implement many
 * different load-balancing policies by implementing this simple interface and
 * using it in the different clustered services (home interface of SLSB for
 * example)
 *
 * @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>.
 * @author <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.3.4.4 $
 */
public interface LoadBalancePolicy extends java.io.Serializable
{
   /** The serialVersionUID
    * @since 1.3.4.2
    */ 
   static final long serialVersionUID = -5071668971774090555L;
   /**
    * Initialize the policy with a reference to its parent stub. the load-balancing policy
    * implementation can use HARMIClient data to take its decision
    * @param father The stub that owns the policy
    */   
   public void init (HARMIClient father);

   /**
    * Called when the stub wishes to know on which node the next invocation must
    * be performed.
    * @param clusterFamily A list of potential target nodes
    * @return The selected target for the next invocation
    */   
   public Object chooseTarget (FamilyClusterInfo clusterFamily);

   /**
    * Called when the stub wishes to know on which node the next invocation must
    * be performed.
    * @param clusterFamily A list of potential target nodes
    * @param routingDecision The actual invocation object if the policy wants
    * to have some kind of invocation-based routing strategy
    * @return The selected target for the next invocation
    */   
   public Object chooseTarget (FamilyClusterInfo clusterFamily, Invocation routingDecision);
}
