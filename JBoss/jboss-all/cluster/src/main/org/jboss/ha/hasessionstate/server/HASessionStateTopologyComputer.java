/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.hasessionstate.server;


import java.util.ArrayList;

import org.jboss.ha.framework.interfaces.SubPartitionsInfo;

/**
 *   Helper interface to which is delegated new HASessionState topology computation
 *
 *   @see org.jboss.ha.hasessionstate.interfaces.HASessionState, HASessionStateTopologyComputerImpl
 *   @author sacha.labourey@cogito-info.ch
 *   @version $Revision: 1.1.4.1 $
 *
 * <p><b>Revisions:</b><br>
 */

public interface HASessionStateTopologyComputer {

   public void init (String sessionStateName, long nodesPerSubPartition);   
   public void start ();
   
   public SubPartitionsInfo computeNewTopology (SubPartitionsInfo currentTopology, ArrayList newReplicants);

}

