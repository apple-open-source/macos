/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.singleton;

import java.util.List;

import org.jboss.ha.jmx.HAServiceMBeanSupport;

/** 
 *   Management Bean for an HA-Singleton service.
 *
 *   @author <a href="mailto:ivelin@apache.org">Ivelin Ivanov</a>
 *   @version $Revision: 1.1.2.3 $
 *
 * <p><b>Revisions:</b><br>
 * <p><b>2003/05/11 Ivelin Ivanov:</b>
 * <ul>
 * <li> implemented abstract HA-Singleton service </li>
 * </ul>
 */

public class HASingletonSupport
  extends HAServiceMBeanSupport
  implements HASingletonMBean, HASingleton
{
  // Constants -----------------------------------------------------

  private boolean isMasterNode = false;

  public HASingletonSupport()
  {
    // for JMX
  }

  /**
   * 
   * @return true if this cluster node has the active mbean singleton.
   * false otherwise
   *
   * @jmx:managed-operation
   */
  public boolean isMasterNode()
  {
    return isMasterNode;
  }

  // Protected ------------------------------

  /**
   * <p>
   * Extending classes should override this method and implement the custom singleton logic.
   * Only one node in the cluster is the active master.
   * If the current node is elected for master, this method is invoked.
   * When another node is elected for master for some reason, the stopSingleton() method is invokded.
   * </p>
   * 
   * <p>
   * When the extending class is a stateful singleton, it will
   * usually use putDistributedState() and getDistributedState() to save in the cluster environment
   * information that will be needed by the next node elected for master should the current master node
   * fail.  
   * </p>  
   *  
   * @see HASingleton
   * 
   */
  public void startSingleton()
  {
    boolean debug = log.isDebugEnabled();
    if (debug) log.debug("startSingleton() : elected for master singleton node");
    // Extending classes will implement the singleton logic here
  }

  /**
   * <p>
   * Extending classes should override this method and implement the custom singleton logic.
   * Only one node in the cluster is the active master.
   * If the current node is master and another node is elected for master, this method is invoked.
   * </p>
   * 
   * @see HASingleton
   * 
   */
  public void stopSingleton()
  {
    boolean debug = log.isDebugEnabled();
    if (debug) log.debug( "stopSingleton() : another node in the partition (if any) is elected for master ");
    // Extending classes will implement the singleton logic here
  }


  /**
   * 
   * when topology changes, a new master may need to be elected
   * 
   * @see HAServiceMBeanSupport#partitionTopologyChanged(List, int)
   * 
   */
  public void partitionTopologyChanged(List newReplicants, int newReplicantsViewId)
  {
    boolean debug = log.isDebugEnabled();

    boolean isElectedNewMaster = isDRMMasterReplica();

    // if this node is already the master, don't bother electing it again
    if (isElectedNewMaster && isMasterNode)
    {
      return;
    }
    // just becoming master
    else if (isElectedNewMaster && !isMasterNode)
    {
      makeThisNodeMaster();
    }
    // When moving from master to slave, then the 
    // new master will call back the _stopOldMaster().
    // However, in case that the last participating node is being removed,
    // and there is no other node to become the new master,
    // we have to call _stopOldMaster() locally
    else if (newReplicants.size() == 0)
    {
      if (debug) 
      {
        log.debug("considerElectingNewMaster() : stopping the only remaining node. No other node will become master. cluster view id: " + newReplicantsViewId);
      } 
      _stopOldMaster();
    }
    
  }

  protected void makeThisNodeMaster()
  {
    try
    {
      // stop the old master (if there is one) before starting the new one
      callMethodOnPartition("_stopOldMaster", new Object[0]);

      isMasterNode = true;

      // start new master
      startSingleton();
    }
    catch (Exception ex)
    {
      log.error(
        "_stopOldMaster failed. New master singleton will not start.",
        ex);
    }
  }

  /**
   * 
   * This method will be invoked twice by the local node 
   * when it stops as well as by the remote
   * 
   *
   */
  public void _stopOldMaster()
  {
    // since this is a cluster call, all nodes will hear it
    // if the node is not the master, then ignore 
    if (!isMasterNode) return;
    isMasterNode = false;
    stopSingleton();
  }


  // Private -------------------------------------------------------

  // Inner classes -------------------------------------------------

}
