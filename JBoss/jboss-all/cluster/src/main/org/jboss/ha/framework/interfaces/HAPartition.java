/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.framework.interfaces;

import java.io.Serializable;
import java.util.Vector;
import java.util.ArrayList;

import org.jboss.ha.framework.interfaces.HAPartition.HAMembershipListener;

/** 
 *
 *   @author  <a href="mailto:bill@burkecentral.com">Bill Burke</a>.
 *   @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 *   @version $Revision: 1.7.2.2 $
 *
 * <p><b>Revisions:</b><br>
 * <p><b>28.07.2002 - Sacha Labourey:</b>
 * <ul>
 * <li> Added network-partition merge callback for listeners</li>
 * </ul>
 */

public interface HAPartition
{
   // *******************************
   // *******************************
   // Partition information accessors
   // *******************************
   // *******************************
   //
   /**
    * Return the name of the current name in the current partition. The name is
    * dynamically determined by the partition.
    * @return The partition name
    */   
   public String getNodeName();
   /**
    * The name of the partition. Either set when creating the partition
    * (MBEAN definition) or uses the default name
    * @return Name of the current partition
    */   
   public String getPartitionName();

   /**
    * Accessor to the DRM that is linked to this partition.
    * @return the DRM linked to this partition
    */   
   public DistributedReplicantManager getDistributedReplicantManager();
   /**
    * Accessor the the DistributedState (DS) that is linked to this partition.
    * @return the DistributedState service
    */   
   public DistributedState getDistributedStateService ();

   // ***************************
   // ***************************
   // RPC multicast communication
   // ***************************
   // ***************************
   //
   /**
    * The partition receives RPC calls from other nodes in the cluster and demultiplex
    * them, according to a service name, to a particular service. Consequently, each
    * service must first subscribe with a particular service name in the partition. The subscriber
    * does not need to implement any specific interface: the call is handled
    * dynamically through reflection.
    * @param serviceName Name of the subscribing service (demultiplexing key)
    * @param handler object to be called when receiving a RPC for its key.
    */   
   public void registerRPCHandler(String serviceName, Object handler);
   /**
    * Unregister the service from the partition
    * @param serviceName Name of the service key (on which the demultiplexing occurs)
    * @param subscriber The target object that unsubscribes
    */   
   public void unregisterRPCHandler(String serviceName, Object subscriber);

   // Called only on all members of this partition on all nodes
   //
   /**
    * Invoke a synchronous RPC call on all nodes of the partition/cluster
    * @param serviceName Name of the target service name on which calls are de-multiplexed
    * @param methodName name of the Java method to be called on remote services
    * @param args array of Java Object representing the set of parameters to be
    * given to the remote method
    * @param excludeSelf indicates if the RPC must also be made on the current
    * node of the partition or only on remote nodes
    * @throws Exception Throws if a communication exception occurs
    * @return an array of answers from remote nodes
    */
   public ArrayList callMethodOnCluster(String serviceName, String methodName,
         Object[] args, boolean excludeSelf) throws Exception;

   /**
    * Invoke a asynchronous RPC call on all nodes of the partition/cluster. The
    * call will return immediately and will not wait that the nodes answer. Thus
    * no answer is available.
    * @param serviceName Name of the target service name on which calls are de-multiplexed
    * @param methodName name of the Java method to be called on remote services
    * @param args array of Java Object representing the set of parameters to be
    * given to the remote method
    * @param excludeSelf indicates if the RPC must also be made on the current
    * node of the partition or only on remote nodes
    * @throws Exception Throws if a communication exception occurs
    */   
   public void callAsynchMethodOnCluster (String serviceName, String methodName,
         Object[] args, boolean excludeSelf) throws Exception;

   // *************************
   // *************************
   // State transfer management
   // *************************
   // *************************
   //
   
   /**
    * State management is higly important for clustered services. Consequently, services that wish to manage their state
    * need to subscribe to state transfer events. When their node start, a state is pushed from another node to them.
    * When another node starts, they may be asked to provide such a state to initialise the newly started node.
    */   
   public interface HAPartitionStateTransfer
   {
      /**
       * Called when a new node need to be initialized. This is called on any existing node to determine a current state for this service.
       * @return A serializable representation of the state
       */      
      public Serializable getCurrentState ();
      /**
       * This callback method is called when a new service starts on a new node: the state that it should hold is transfered to it through this callback
       * @param newState The serialized representation of the state of the new service.
       */      
      public void setCurrentState(Serializable newState);
   }
   
   /**
    * Register a service that will participate in state transfer protocol and receive callbacks
    * @param serviceName Name of the service that subscribes for state stransfer events. This name must be identical for all identical services in the cluster.
    * @param subscriber Object implementing {@link HAPartitionStateTransfer} and providing or receiving state transfer callbacks
    */   
   public void subscribeToStateTransferEvents (String serviceName, HAPartition.HAPartitionStateTransfer subscriber);
   /**
    * Unregister a service from state transfer callbacks.
    * @param serviceName Name of the service that participates in the state transfer protocol
    * @param subscriber Service implementing the state transfer callback methods
    */   
   public void unsubscribeFromStateTransferEvents (String serviceName, HAPartition.HAPartitionStateTransfer subscriber);

   // *************************
   // *************************
   // Group Membership listeners
   // *************************
   // *************************
   //
   /**
    * When a new node joins the cluster or an existing node leaves the cluster
    * (or simply dies), membership events are raised.
    *
    */   
   public interface HAMembershipListener
   {
      /** Called when a new partition topology occurs. This callback is made
       * using the JG protocol handler thread and so you cannot execute new
       * cluster calls that need this thread. If you need to do that implement
       * the aynchronous version of the listener interface.
       *
       * @param deadMembers A list of nodes that have died since the previous view
       * @param newMembers A list of nodes that have joined the partition since the previous view
       * @param allMembers A list of nodes that built the current view
       */      
      public void membershipChanged(Vector deadMembers, Vector newMembers, Vector allMembers);
   }

   /** A tagging interface for HAMembershipListener callbacks that will
    * be performed in a thread seperate from the JG protocl handler thread.
    * The ordering of view changes is preserved, but listeners are free to
    * execute cluster calls.
    */
   public interface AsynchHAMembershipListener extends HAMembershipListener
   {
      // Nothing new
   }

   public interface HAMembershipExtendedListener extends HAPartition.HAMembershipListener
   {
      /** Extends HAMembershipListener to receive a specific callback when a
       * network-partition merge occurs. The same restriction on interaction
       * with the JG protocol stack applies.
       *
       * @param deadMembers A list of nodes that have died since the previous view
       * @param newMembers A list of nodes that have joined the partition since the previous view
       * @param allMembers A list of nodes that built the current view
       * @param originatingGroups A list of list of nodes that were previously partionned and that are now merged
       */      
      public void membershipChangedDuringMerge(Vector deadMembers, Vector newMembers,
            Vector allMembers, Vector originatingGroups);
   }

   /** A tagging interface for HAMembershipExtendedListener callbacks that will
    * be performed in a thread seperate from the JG protocl handler thread.
    * The ordering of view changes is preserved, but listeners are free to
    * execute cluster calls.
    */
   public interface AsynchHAMembershipExtendedListener extends HAMembershipExtendedListener
   {
      // Nothing new
   }

   /**
    * Subscribes to receive {@link HAMembershipListener} events.
    * @param listener The membership listener object
    */   
   public void registerMembershipListener(HAMembershipListener listener);
   /**
    * Unsubscribes from receiving {@link HAMembershipListener} events.
    * @param listener The listener wishing to unsubscribe
    */   
   public void unregisterMembershipListener(HAMembershipListener listener);

   /**
    * Each time the partition topology changes, a new view is computed. A view is a list of members,
    * the first member being the coordinator of the view. Each view also has a distinct identifier.
    * @return The identifier of the current view
    */   
   public long getCurrentViewId();
   /**
    * Return the list of member nodes that built the current view i.e. the current partition.
    * @return An array of Strings containing the node names
    */   
   public Vector getCurrentView ();

   /**
    * Return the member nodes that built the current view i.e. the current partition.
    * @return An array of ClusterNode containing the node names
    */
   public ClusterNode[] getClusterNodes ();
}