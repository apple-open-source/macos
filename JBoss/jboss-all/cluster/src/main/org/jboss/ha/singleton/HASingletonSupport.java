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
 * Management Bean for an HA-Singleton service.
 *
 * @author <a href="mailto:ivelin@apache.org">Ivelin Ivanov</a>
 * Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.6 $
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
    * Extending classes should override this method and implement the custom
    * singleton logic. Only one node in the cluster is the active master.
    * If the current node is elected for master, this method is invoked.
    * When another node is elected for master for some reason, the
    * stopSingleton() method is invokded.
    * </p>
    * 
    * <p>
    * When the extending class is a stateful singleton, it will
    * usually use putDistributedState() and getDistributedState() to save in
    * the cluster environment information that will be needed by the next node
    * elected for master should the current master node
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
    * Extending classes should override this method and implement the custom
    * singleton logic. Only one node in the cluster is the active master.
    * If the current node is master and another node is elected for master, this
    * method is invoked.
    * 
    * @see HASingleton
    * 
    */
   public void stopSingleton()
   {
      boolean debug = log.isDebugEnabled();
      if (debug) log.debug("stopSingleton() : another node in the partition (if any) is elected for master ");
      // Extending classes will implement the singleton logic here
   }


   /**
    * When topology changes, a new master is elected based on the result
    * of the isDRMMasterReplica() call.
    * 
    * @see HAServiceMBeanSupport#partitionTopologyChanged(List, int)
    * @see  DistributedReplicantManager#isMasterReplica(String);
    */
   public void partitionTopologyChanged(List newReplicants, int newViewID)
   {
      boolean debug = log.isDebugEnabled();

      boolean isElectedNewMaster = isDRMMasterReplica();
      if( debug )
      {
         log.debug("partitionTopologyChanged, isElectedNewMaster="+isElectedNewMaster
            +", isMasterNode="+isMasterNode+", viewID="+newViewID);
      }

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
      // transition from master to slave
      else if( isMasterNode == true )
      {
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
    * This method will be invoked twice by the local node 
    * when it stops as well as by the remote
    */
   public void _stopOldMaster()
   {
      log.debug("_stopOldMaster, isMasterNode="+isMasterNode);
      // since this is a cluster call, all nodes will hear it
      // if the node is not the master, then ignore 
      if (isMasterNode == true)
      {
         isMasterNode = false;
         stopSingleton();
      }
   }

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------

}
