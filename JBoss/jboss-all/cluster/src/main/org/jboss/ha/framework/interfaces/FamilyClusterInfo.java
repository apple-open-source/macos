/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.framework.interfaces;

import java.util.ArrayList;

/**
 * Maintain information for a given proxy family. Proxies can statically reference
 * objects implementing this interface: only the content will change as the 
 * cluster topology changes, not the FamilyClusterInfo object itself.
 * Proxies or LoadBalancing policy implementations can use the cursor and object
 * attribute to store arbitrary data that is then shared accross all proxies belonging
 * to the same family. 
 * Initial access to this object is done through the ClusteringTargetsRepository singleton.
 *
 * @see org.jboss.ha.framework.interfaces.FamilyClusterInfoImpl
 * @see org.jboss.ha.framework.interfaces.ClusteringTargetsRepository
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>2002/08/23, Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public interface FamilyClusterInfo
{
   public String getFamilyName ();
   public ArrayList getTargets ();
   public long getCurrentViewId ();
   
   public ArrayList removeDeadTarget(Object target);
   public ArrayList updateClusterInfo (ArrayList targets, long viewId);
   
   public boolean currentMembershipInSyncWithViewId();
   
   // force a reload of the view at the next invocation
   public void resetView ();
   
   // arbitrary usage by the LoadBalancePolicy implementation
   // We could have used an HashMap but the lookup would have taken
   // much more time and we probably don't need as much flexibility
   // (+ it is slow for a simple int)
   //
   public int getCursor();
   public int setCursor (int cursor);
   public Object getObject ();
   public Object setObject (Object whatever);
   
   public final static int UNINITIALIZED_CURSOR = 999999999;
}
