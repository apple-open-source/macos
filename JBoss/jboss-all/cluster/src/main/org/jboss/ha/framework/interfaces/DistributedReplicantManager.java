/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.framework.interfaces;

import org.jboss.ha.framework.interfaces.DistributedReplicantManager.ReplicantListener;
import java.io.Serializable;
import java.util.List;
import java.util.Collection;

/** 
 *
 *   @author  <a href="mailto:bill@burkecentral.com">Bill Burke</a>.
 *   @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 *   @version $Revision: 1.6.4.2 $
 *
 * <p><b>Revisions:</b><br>
 * <p><b>2001/10/31: marcf</b>
 * <ol>
 *   <li>DRM is no longer remote
 * </ol>
 * <p><b>2002/08/23: Sacha Labourey</b>
 * <ol>
 *   <li>added isMasterReplica
 * </ol>
 */
public interface DistributedReplicantManager
{
   /**
    * When a particular key in the DistributedReplicantManager table gets modified, all listeners
    * will be notified of replicant changes for that key.
    */
   public interface ReplicantListener
   {
      /**
       * Callback called when the content/list of replicant for a given replicant key has changed
       * @param key The name of the key of the replicant that has changed
       * @param newReplicants The list of new replicants for the give replicant key
       * @param newReplicantsViewId The new replicant view id corresponding to this change
       */      
      public void replicantsChanged(String key, List newReplicants, int newReplicantsViewId);
   }

   /**
    * Subscribe a new listener {@link ReplicantListener} for replicants change
    * @param key Name of the replicant, must be identical cluster-wide for all identical replicants
    * @param subscriber The subsribing {@link ReplicantListener}
    */   
   public void registerListener(String key, ReplicantListener subscriber);
   /**
    * Unsubscribe a listener {@link ReplicantListener} that had subscribed for replicants changes
    * @param key Name of the replicant, must be identical cluster-wide for all identical replicants
    * @param subscriber The unsubscribing {@link ReplicantListener}
    */   
   public void unregisterListener(String key, ReplicantListener subscriber);

   // State binding methods
   //

   /**
    * Add a replicant, it will be attached to this cluster node
    * @param key Replicant name. All replicas around the cluster must use the same key name.
    * @param replicant Local data of the replicant, that is, any serializable data
    * @throws Exception Thrown if a cluster communication problem occurs
    */
   public void add(String key, Serializable replicant) throws Exception;

   /**
    * Remove the entire key from the ReplicationService
    * @param key Name of the replicant
    * @throws Exception Thrown if a cluster communication problem occurs
    */
   public void remove(String key) throws Exception;

   /**
    * Lookup the replicant attached to this cluster node
    * @param key The name of the replicant
    * @return The local replicant for the give key name
    */   
   public Serializable lookupLocalReplicant(String key);

   /**
    * Return a list of all replicants.
    * @param key The replicant name
    * @return An array of serialized replicants available around the cluster for the given key
    */
   public List lookupReplicants(String key);

   /**
    * Return a list of all replicants node names.
    * @param key The replicant name
    * @return An array of replicants node names available around the cluster for the given key
    */
   public List lookupReplicantsNodeNames(String key);

   /**
    * Return a list of all services that have a least one replicant.
    * @return A collection of services names (String)
    */
   public Collection getAllServices ();

   /**
    * Returns an id corresponding to the current view of this set of replicants.
    * @param key The replicant name
    * @return A view id (doesn't grow sequentially)
    */
   public int getReplicantsViewId(String key);
   
   /**
    * Indicates if the current node is the master replica for this given key.
    * @param key The replicant name
    * @return True if this node is the master
    */
   public boolean isMasterReplica (String key);

}
