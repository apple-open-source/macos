/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.framework.server;


import java.util.Vector;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;

import java.io.Serializable;

import EDU.oswego.cs.dl.util.concurrent.Latch;

import org.jboss.logging.Logger;

import org.jboss.ha.framework.interfaces.DistributedReplicantManager;
import org.jboss.ha.framework.interfaces.HAPartition;


/** 
 *   This class manages replicated objects.
 *
 *   @author  <a href="mailto:bill@burkecentral.com">Bill Burke</a>.
 *   @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 *   @version $Revision: 1.17.2.5 $
 *
 * <p><b>Revisions:</b><br>
 * <p><b>2001/10/31: marcf</b>
 * <ol>
 *   <li>DRM is no longer remote
 *   <li>DRM no longer is part of state transfer protocol
 * </ol>
 * <p><b>2001/11/09: Sacha Labourey</b>
 * <ol>
 *   <li>Merge protocol no longer raise events when information was already known</li>
 * </ol>
 * <p><b>2001/11/25: Sacha Labourey</b>
 * <ol>
 *   <li>Performs asynchronous calls on the cluster (instead of synchronous) whenever possible</li>
 * </ol>
 * <p><b>2002/08/24: Sacha Labourey</b>
 * <ol>
 *   <li>Now exchange initial state through GetState events and only appy
 *      full merge during a partition-merge. Added isMaster helper method</li>
 * </ol>
 */

public class DistributedReplicantManagerImpl
   implements DistributedReplicantManagerImplMBean,
              HAPartition.HAMembershipExtendedListener,
              HAPartition.HAPartitionStateTransfer
{
   // Constants -----------------------------------------------------
   
   protected final static String SERVICE_NAME = "DistributedReplicantManager";
   
   // Attributes ----------------------------------------------------
   
   protected HashMap localReplicants = new HashMap();
   protected HashMap replicants = new HashMap();
   protected HashMap keyListeners = new HashMap();
   protected HashMap intraviewIdCache = new HashMap();
   protected HAPartition partition;   
   
   protected Logger log;
   
   protected javax.management.MBeanServer mbeanserver;
   protected javax.management.ObjectName jmxName;
   
   protected String nodeName = null;
   
   protected Latch partitionNameKnown = new Latch ();
   protected boolean trace;

   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------       
   
   /**
    * This class manages replicated objects through the given partition
    *
    * @param partition {@link HAPartition} through which replicated objects will be exchanged
    */   
   public DistributedReplicantManagerImpl(HAPartition partition, javax.management.MBeanServer server)
   {
      this.partition = partition;
      this.mbeanserver = server;
      this.log = Logger.getLogger(partition.getPartitionName() + ":ReplicantManager");
      this.trace = log.isTraceEnabled();
   }

   // Public --------------------------------------------------------
   
   public void init() throws Exception
   {
      log.debug("registerRPCHandler");
      partition.registerRPCHandler(SERVICE_NAME, this);
      log.debug("subscribeToStateTransferEvents");
      partition.subscribeToStateTransferEvents(this.SERVICE_NAME, this);
      log.debug("registerMembershipListener");
      partition.registerMembershipListener(this);

     // subscribed this "sub-service" of HAPartition with JMX
      // TODO: In the future (when state transfer issues will be completed), 
      // we will need to redesign the way HAPartitions and its sub-protocols are
      // registered with JMX. They will most probably be independant JMX services.
      //
      String name = "jboss:service=" + SERVICE_NAME + 
                    ",partitionName=" + this.partition.getPartitionName();
      this.jmxName = new javax.management.ObjectName(name);
      this.mbeanserver.registerMBean(this, jmxName);
    }
   
   public void start() throws Exception
   {
      this.nodeName = this.partition.getNodeName ();

      partitionNameKnown.release (); // partition name is now known!
      
      //log.info("mergemembers");
      //mergeMembers();
   }
   
   public void stop() throws Exception
   {
      // we cleanly shutdown. This should be optimized.
      //
      if (localReplicants != null)
      {
         synchronized(localReplicants)
         {
            while (! localReplicants.isEmpty ())
            {               
               this.remove ((String)localReplicants.keySet().iterator().next ());
            }
         }
      }
      
      this.mbeanserver.unregisterMBean (this.jmxName);
   }
   
   public String listContent () throws Exception
   {
      // we merge all replicants services: local only or not
      //
      java.util.Collection services = this.getAllServices ();

      StringBuffer result = new StringBuffer ();
      java.util.Iterator catsIter = services.iterator ();
      
      result.append ("<pre>");
      
      while (catsIter.hasNext ())
      {
         String category = (String)catsIter.next ();
         HashMap content = (HashMap)this.replicants.get (category);
         if (content == null)
            content = new HashMap ();
         java.util.Iterator keysIter = content.keySet ().iterator ();
                  
         result.append ("-----------------------------------------------\n");
         result.append ("Service : ").append (category).append ("\n\n");
         
         Serializable local = lookupLocalReplicant(category);
         if (local == null)
            result.append ("\t- Service is *not* available locally\n");
         else
            result.append ("\t- Service *is* also available locally\n");

         while (keysIter.hasNext ())
         {
            String location = (String)keysIter.next ();            
            result.append ("\t- ").append(location).append ("\n");
         }
         
         result.append ("\n");
         
      }
      
      result.append ("</pre>");
      
      return result.toString ();
   }
   
   public String listXmlContent () throws Exception
   {
      // we merge all replicants services: local only or not
      //
      java.util.Collection services = this.getAllServices ();
      StringBuffer result = new StringBuffer ();

      result.append ("<ReplicantManager>\n");

      java.util.Iterator catsIter = services.iterator ();
      while (catsIter.hasNext ())
      {
         String category = (String)catsIter.next ();
         HashMap content = (HashMap)this.replicants.get (category);
         if (content == null)
            content = new HashMap ();
         java.util.Iterator keysIter = content.keySet ().iterator ();
                  
         result.append ("\t<Service>\n");
         result.append ("\t\t<ServiceName>").append (category).append ("</ServiceName>\n");

         
         Serializable local = lookupLocalReplicant(category);
         if (local != null)
         {
            result.append ("\t\t<Location>\n");
            result.append ("\t\t\t<Name local=\"True\">").append (this.nodeName).append ("</Name>\n");
            result.append ("\t\t</Location>\n");
         }

         while (keysIter.hasNext ())
         {
            String location = (String)keysIter.next ();            
            result.append ("\t\t<Location>\n");
            result.append ("\t\t\t<Name local=\"False\">").append (location).append ("</Name>\n");
            result.append ("\t\t</Location>\n");
         }
         
         result.append ("\t<Service>\n");
         
      }

      result.append ("<ReplicantManager>\n");
      
      return result.toString ();
   }

   // HAPartition.HAPartitionStateTransfer implementation ----------------------------------------------
   
   public Serializable getCurrentState ()
   {
      java.util.Collection services = this.getAllServices ();
      HashMap result = new HashMap ();
      
      java.util.Iterator catsIter = services.iterator ();                       
      while (catsIter.hasNext ())
      {
         String category = (String)catsIter.next ();
         HashMap content = (HashMap)this.replicants.get (category);
         if (content == null)
            content = new HashMap ();
         else
            content = (HashMap)content.clone ();
         
         Serializable local = lookupLocalReplicant(category);
         if (local != null)
            content.put (this.nodeName, local);
         
         result.put (category, content);
      }
      
      // we add the intraviewid cache to the global result
      //
      Object[] globalResult = new Object[] {result, intraviewIdCache};
      return globalResult;
   }

   public void setCurrentState(Serializable newState)
   {
      Object[] globalState = (Object[])newState;
      
      this.replicants = (HashMap)globalState[0];
      this.intraviewIdCache = (HashMap)globalState[1];
   }
      
   public Collection getAllServices ()
   {
      HashSet services = new HashSet();
      services.addAll (localReplicants.keySet ());
      services.addAll (replicants.keySet ());      
      return services;
   }
   
   // HAPartition.HAMembershipListener implementation ----------------------------------------------

   public void membershipChangedDuringMerge(Vector deadMembers, Vector newMembers, Vector allMembers, Vector originatingGroups)
   {
      // Here we only care about deadMembers.  Purge all replicant lists of deadMembers
      // and then notify all listening nodes.
      //
      log.info("Merging partitions...");
      log.info("Dead members: " + deadMembers.size());
      log.info("Originating groups: " + originatingGroups);
      purgeDeadMembers(deadMembers);
      if (newMembers.size() > 0) 
      {
         new MergeMembers().start();
      }
   }
   
   public void membershipChanged(Vector deadMembers, Vector newMembers, Vector allMembers)
   {
      // Here we only care about deadMembers.  Purge all replicant lists of deadMembers
      // and then notify all listening nodes.
      //
      log.info("Dead members: " + deadMembers.size());
      purgeDeadMembers(deadMembers);
      
      // we don't need to merge members anymore
   }
   
   // DistributedReplicantManager implementation ----------------------------------------------              
   
   public void add(String key, Serializable replicant) throws Exception
   {
      if( trace )
         log.trace("add, key="+key+", value="+replicant);
      partitionNameKnown.acquire (); // we don't propagate until our name is known
      
      Object[] args = {key, this.nodeName, replicant};
      partition.callMethodOnCluster(SERVICE_NAME, "_add", args, true);
      synchronized(localReplicants)
      {
         localReplicants.put(key, replicant);
         notifyKeyListeners(key, lookupReplicants(key));
      }
   }
   
   public void remove(String key) throws Exception
   {
      partitionNameKnown.acquire (); // we don't propagate until our name is known
      
      if (localReplicants.containsKey (key)) 
         // optimisation: we don't make a costly network call if there is
         // nothing to remove
         //
      {
         Object[] args = {key, this.nodeName};
         partition.callAsynchMethodOnCluster(SERVICE_NAME, "_remove", args, true);
         synchronized(localReplicants)
         {
            localReplicants.remove(key);
            List result = lookupReplicants(key);
            if (result == null)
               result = new ArrayList (); // don't return null but an empty list
            notifyKeyListeners(key, result);
         }
      }
   }
   
   public Serializable lookupLocalReplicant(String key)
   {
      synchronized(localReplicants)
      {
         return (Serializable)localReplicants.get(key);
      }
   }
   
   public List lookupReplicants(String key)
   {
      Serializable local = lookupLocalReplicant(key);
      synchronized(replicants)
      {
         HashMap replicant = (HashMap)replicants.get(key);
         if (replicant == null && local == null) return null;
         ArrayList rtn = new ArrayList();
         if (local != null) rtn.add(local);
         if (replicant != null) rtn.addAll(replicant.values());
         return rtn;
      }
   }
   
   public List lookupReplicantsNodeNames(String key)
   {      
      boolean locallyReplicated = localReplicants.containsKey (key);
      synchronized(replicants)
      {
         HashMap replicant = (HashMap)replicants.get(key);
         if (replicant == null && !locallyReplicated) return null;
         ArrayList rtn = new ArrayList();
         if (locallyReplicated) rtn.add(this.nodeName);
         if (replicant != null) rtn.addAll(replicant.keySet ());
         return rtn;
      }
   }
   
   
   public void registerListener(String key, DistributedReplicantManager.ReplicantListener subscriber)
   {
      synchronized(keyListeners)
      {
         ArrayList listeners = (ArrayList)keyListeners.get(key);
         if (listeners == null)
         {
            listeners = new ArrayList();
            keyListeners.put(key, listeners);
         }
         listeners.add(subscriber);
      }
   }
   
   public void unregisterListener(String key, DistributedReplicantManager.ReplicantListener subscriber)
   {
      synchronized(keyListeners)
      {
         ArrayList listeners = (ArrayList)keyListeners.get (key);
         if (listeners == null) return;
         
         listeners.remove(subscriber);
         if (listeners.size() == 0)
            keyListeners.remove(key);

      }
   }
   
   public int getReplicantsViewId(String key)   
   {
      Integer result = (Integer)this.intraviewIdCache.get (key);
      
      if (result == null)
         return 0;
      else
         return result.intValue ();      
   }
   
   public boolean isMasterReplica (String key)
   {
      if( trace )
         log.trace("isMasterReplica, key="+key);
      // if I am not a replicat, I cannot be the master...
      //
      if (!localReplicants.containsKey (key))
      {
         if( trace )
            log.trace("no localReplicants, key="+key+", isMasterReplica=false");
         return false;
      }

      Vector allNodes = this.partition.getCurrentView ();
      HashMap repForKey = (HashMap)replicants.get(key);
      if (repForKey==null)
      {
         if( trace )
            log.trace("no replicants, key="+key+", isMasterReplica=true");
         return true;
      }
      Vector replicaNodes = new Vector ((repForKey).keySet ());          
      boolean isMasterReplica = false;
      for (int i=0; i<allNodes.size (); i++)
      {
         String aMember = (String)allNodes.elementAt (i);
         if( trace )
            log.trace("Testing member: "+aMember);
         if (replicaNodes.contains (aMember))
         {
            if( trace )
               log.trace("Member found in replicaNodes, isMasterReplica=false");
            break;
         }
         else if (aMember.equals (this.nodeName))
         {
            if( trace )
               log.trace("Member == nodeName, isMasterReplica=true");
            isMasterReplica = true;
            break;
         }
      }
      return isMasterReplica;
   }

   // DistributedReplicantManager cluster callbacks ----------------------------------------------              
   
   /**
    * cluster callback called when a new replicant is added on another node
    * @param key Replicant key
    * @param nodeName Node that add the current replicant
    * @param replicant Serialized representation of the replicant
    */   
   public void _add(String key, String nodeName, Serializable replicant)
   {
      if (log.isDebugEnabled()) {
         log.debug("_add(" + key + ", " + nodeName);
      }
      
      try
      {
         addReplicant(key, nodeName, replicant);
         notifyKeyListeners(key, lookupReplicants(key));
      }
      catch (Exception ex)
      {
         log.error("_add failed", ex);
      }
   }
   
   /**
    * cluster callback called when a replicant is removed by another node
    * @param key Name of the replicant key
    * @param nodeName Node that wants to remove its replicant for the give key
    */   
   public void _remove(String key, String nodeName)
   {
      try
      {
         if (removeReplicant (key, nodeName))
            notifyKeyListeners(key, lookupReplicants(key));
      }
      catch (Exception ex)
      {
         log.error("_remove failed", ex);
      }
   }
   
   protected boolean removeReplicant (String key, String nodeName) throws Exception
   {
         synchronized(replicants)
         {
            HashMap replicant = (HashMap)replicants.get(key);
            if (replicant == null) return false;
            Object removed = replicant.remove(nodeName);
            if (removed != null)
            {
               Collection values = replicant.values();               
               if (values.size() == 0)
               {
                  replicants.remove(key);
               }
               return true;
            }
         }
         return false;
   }
   
   /**
    * Cluster callback called when a node wants to know our complete list of local replicants
    * @throws Exception Thrown if a cluster communication exception occurs
    * @return A java array of size 2 containing the name of our node in this cluster and the serialized representation of our state
    */   
   public Object[] lookupLocalReplicants() throws Exception
   {
      partitionNameKnown.acquire (); // we don't answer until our name is known
      
      Object[] rtn = {this.nodeName, localReplicants};
      log.debug ("lookupLocalReplicants called ("+ rtn[0] + "). Return: " + localReplicants.size ());
      return rtn;
   }
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   protected int calculateReplicantsHash (List members)
   {
      int result = 0;
      Object obj = null;
      
      for (int i=0; i<members.size (); i++)
      {
         obj = members.get (i);
         if (obj != null)
            result+= obj.hashCode (); // no explicit overflow with int addition
      }
      
      return result;
   }
   
   protected int updateReplicantsHashId (String key)
   {
      // we first get a list of all nodes names that replicate this key
      //
      List nodes = this.lookupReplicantsNodeNames (key);
      int result = 0;
      
      if ( (nodes == null) || (nodes.size () == 0) )
      {
         // no nore replicants for this key: we uncache our view id
         //
         this.intraviewIdCache.remove (key);
      }
      else
      {
         result = this.calculateReplicantsHash (nodes);
         this.intraviewIdCache.put (key, new Integer (result));
      }
      
      return result;
      
   }
   
   ///////////////
   // DistributedReplicantManager API
   ///////////////
   
   /**
    * Add a replicant to the replicants map.
    * @param key replicant key name
    * @param nodeName name of the node that adds this replicant
    * @param replicant Serialized representation of the replica
    */
   protected void addReplicant(String key, String nodeName, Serializable replicant)
   {
      addReplicant(replicants, key, nodeName, replicant);
   }
   
   /**
    * Logic for adding replicant to any map.
    * @param map structure in which adding the new replicant
    * @param key name of the replicant key
    * @param nodeName name of the node adding the replicant
    * @param replicant serialized representation of the replicant that is added
    */
   protected void addReplicant(HashMap map, String key, String nodeName, Serializable replicant)
   {
      synchronized(map)
      {
         HashMap rep = (HashMap)map.get(key);
         if (rep == null)
         {
            log.debug("_adding new HashMap");
            rep = new HashMap();
            map.put(key, rep);
         }
         rep.put(nodeName, replicant);         
      }
   }
   
   protected Vector getKeysReplicatedByNode (String nodeName)
   {
      Vector result = new Vector ();
      synchronized (replicants)
      {         
         Iterator keysIter = replicants.keySet ().iterator ();
         while (keysIter.hasNext ())
         {
            String key = (String)keysIter.next ();
            HashMap values = (HashMap)replicants.get (key);
            if ( (values != null) && values.containsKey (nodeName) )
            {
               result.add (key);
            }
         }
      }
      return result;
   }
   
   /**
    * Indicates if the a replicant already exists for a given key/node pair
    * @param key replicant key name
    * @param nodeName name of the node
    * @return a boolean indicating if a replicant for the given node exists for the given key
    */   
   protected boolean replicantEntryAlreadyExists (String key, String nodeName)
   {
      return replicantEntryAlreadyExists (replicants, key, nodeName);
   }
   
   /**
    * Indicates if the a replicant already exists for a given key/node pair in the give data structure
    */   
   protected boolean replicantEntryAlreadyExists (HashMap map, String key, String nodeName)
   {
         HashMap rep = (HashMap)map.get(key);
         if (rep == null)
            return false;
         else
            return rep.containsKey (nodeName);
   }
   
   /**
    * Notifies, through a callback, the listeners for a given replicant that the set of replicants has changed
    * @param key The replicant key name
    * @param newReplicants The new list of replicants
    */   
   protected void notifyKeyListeners(String key, List newReplicants)
   {
      log.debug("notifyKeyListeners");
      synchronized(keyListeners)
      {
         // we first update the intra-view id for this particular key
         //
         int newId = updateReplicantsHashId (key);
         
         ArrayList listeners = (ArrayList)keyListeners.get(key);
         if (listeners == null)
         {
            log.debug("listeners is null");
            return;
         }
         
         if (log.isDebugEnabled())
           log.debug("notifying " + listeners.size() + " listeners for key change: " + key);
         for (int i = 0; i < listeners.size(); i++)
         {
            DistributedReplicantManager.ReplicantListener listener = (DistributedReplicantManager.ReplicantListener)listeners.get(i);
            listener.replicantsChanged(key, newReplicants, newId);
         }
      }
   }

   ////////////////////
   // Group membership API
   ////////////////////

   protected void mergeMembers()
   {
      try
      {
         log.info("Start merging members in DRM service...");
         java.util.HashSet notifies = new java.util.HashSet ();
         ArrayList rsp = partition.callMethodOnCluster(SERVICE_NAME, "lookupLocalReplicants", null, true);
         if (rsp.size() == 0) 
            log.debug("No responses from other nodes during the DRM merge process.");
         else 
         { 
            log.debug("The DRM merge process has received " + rsp.size() + " answers");
         }
         for (int i = 0; i < rsp.size(); i++)
         {
            Object[] objs = (Object[])rsp.get(i);
            if (objs == null)
            {
               log.warn("As part of the answers received during the DRM merge process, a NULL message was received!");
               continue;
            }
            String node = (String)objs[0];
            HashMap replicants = (HashMap)objs[1];
            Iterator keys = replicants.keySet().iterator();
            
            //FIXME: We don't remove keys in the merge process but only add new keys!
            while (keys.hasNext())
            {
               String key = (String)keys.next();
               if (!replicantEntryAlreadyExists  (key, node)) // done to reduce duplicate notifications
               {
                  addReplicant(key, node, (Serializable)replicants.get(key));
                  notifies.add (key);
               }
            }
            
            Vector currentStatus = getKeysReplicatedByNode (node);
            if (currentStatus.size () > replicants.size ())
            {
               // The merge process needs to remove some (now)
               // unexisting keys
               //
               for (int currentKeysId=0, currentKeysMax=currentStatus.size (); currentKeysId<currentKeysMax; currentKeysId++)
               {
                  String theKey = (String)currentStatus.elementAt (currentKeysId);
                  if (!replicants.containsKey (theKey))
                  {
                     removeReplicant (theKey, node);
                     notifies.add(theKey);
                  }
               }
            }
         }   
         
         Iterator notifIter = notifies.iterator ();
         while (notifIter.hasNext ())
         {
            String key = (String)notifIter.next ();
            notifyKeyListeners(key, lookupReplicants(key));
         }
         log.info ("..Finished merging members in DRM service");

      }               
      catch (Exception ex)
      {
         log.error("merge failed", ex);
      }
   }

   /**
    * get rid of dead members from replicant list
    * return true if anything was purged.
    */
   protected void purgeDeadMembers(Vector deadMembers)
   {
      if (deadMembers.size() <= 0) return;
      try
      {
         synchronized(replicants)
         {
            Iterator keys = replicants.keySet().iterator();
            while (keys.hasNext())
            {
               String key = (String)keys.next();
               HashMap replicant = (HashMap)replicants.get(key);
               boolean modified = false;
               for (int i = 0; i < deadMembers.size(); i++)
               {
                  String node = deadMembers.elementAt(i).toString();
                  log.debug("trying to remove deadMember " + node + " for key " + key);
                  Object removed = replicant.remove(node);
                  if (removed != null) 
                  {
                     log.debug(node + " was removed");
                     modified = true;
                  }
                  else
                  {
                     log.debug(node + " was NOT removed!!!");
                  }
               }
               if (modified)
               {
                  notifyKeyListeners(key, lookupReplicants(key));
               }
            }
         }
      }
      catch (Exception ex)
      {
         log.error("membershipChanged failed", ex);
      }
   }

   /**
    */   
   protected void cleanupKeyListeners()
   {
      // NOT IMPLEMENTED YET
   }

   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------

   protected class MergeMembers extends Thread
   {
      /**
       * Called when the service needs to merge with another partition. This process is performed asynchronously
       */      
      public void run()
      {
         log.debug("Sleeping for 50ms second just in case");
         try {
            // if this thread invokes a cluster method call before
            // membershipChanged event completes, it could timeout/hang
            // we need to discuss this with Bela.
            Thread.sleep(50); 
         } catch (Exception ignored) {}
         mergeMembers();
      }
   }

}
