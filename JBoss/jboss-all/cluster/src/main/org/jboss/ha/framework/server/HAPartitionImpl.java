/**
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.framework.server;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.Serializable;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Vector;

import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.Name;
import javax.naming.NameNotFoundException;
import javax.naming.Reference;
import javax.naming.StringRefAddr;
import javax.management.MBeanServer;

import EDU.oswego.cs.dl.util.concurrent.LinkedQueue;

import org.jgroups.JChannel;
import org.jgroups.MergeView;
import org.jgroups.View;
import org.jgroups.Message;
import org.jgroups.blocks.GroupRequest;
import org.jgroups.blocks.MethodCall;
import org.jgroups.blocks.MethodLookup;
import org.jgroups.blocks.MethodLookupClos;
import org.jgroups.stack.IpAddress;
import org.jgroups.util.Rsp;
import org.jgroups.util.RspList;
import org.jgroups.util.Util;

import org.jboss.invocation.MarshalledValueInputStream;
import org.jboss.invocation.MarshalledValueOutputStream;
import org.jboss.ha.framework.interfaces.DistributedReplicantManager;
import org.jboss.ha.framework.interfaces.DistributedState;
import org.jboss.ha.framework.interfaces.HAPartition;
import org.jboss.ha.framework.interfaces.HAPartition.HAPartitionStateTransfer;
import org.jboss.ha.framework.interfaces.HAPartition.HAMembershipListener;
import org.jboss.ha.framework.interfaces.ClusterNode;

import org.jboss.naming.NonSerializableFactory;
import org.jboss.logging.Logger;

/**
 * This class is an abstraction class for a JGroups RPCDispatch and JChannel.
 * It is a default implementation of HAPartition for the
 * <a href="http://www.jgroups.com/">JGroups</A> framework
 *
 * @author <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>.
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.20.2.14 $
 *
 * <p><b>Revisions:</b><br>
 */
public class HAPartitionImpl
   extends org.jgroups.blocks.RpcDispatcher
   implements org.jgroups.MessageListener, org.jgroups.MembershipListener,
      HAPartition
{
   private static class NoHandlerForRPC implements Serializable {}

   // Constants -----------------------------------------------------

   final MethodLookup method_lookup_clos = new MethodLookupClos();

   // Attributes ----------------------------------------------------

   protected HashMap rpcHandlers = new HashMap();
   protected HashMap stateHandlers = new HashMap();
   /** The HAMembershipListener and HAMembershipExtendedListeners */
   protected ArrayList listeners = new ArrayList();
   /** The asynch HAMembershipListener and HAMembershipExtendedListeners */
   protected ArrayList asynchListeners = new ArrayList();
   /** The LinkedQueue<ViewChangeEvent> of changes to notify asynch listeners of */
   protected LinkedQueue asynchViewChanges = new LinkedQueue();
   /** The Thread used to send membership change notifications asynchronously */
   protected Thread asynchNotifyThread;
   /** The current cluster partition members */
   protected Vector members = null;
   protected Vector jgmembers = null;

   public Vector history = null;

   /** The partition members other than this node */
   protected Vector otherMembers = null;
   protected Vector jgotherMembers = null;
   /** The JChannel name */
   protected String partitionName;
   /** the local JG IP Address */
   protected org.jgroups.stack.IpAddress localJGAddress = null;
   /** The cluster transport protocol address string */
   protected String nodeName;
   /** me as a ClusterNode */
   protected ClusterNode me = null;
   /** The timeout for cluster RPC calls */
   protected int timeout = 60000;
   /** The JGroups partition channel */
   protected JChannel channel;
   /** The cluster replicant manager */
   protected DistributedReplicantManagerImpl replicantManager;
   /** The cluster state manager */
   protected DistributedStateImpl dsManager;
   /** The cluster instance log category */
   protected Logger log;
   protected Logger clusterLifeCycleLog;   
   /** The current cluster view id */
   protected long currentViewId = -1;
   /** The JMX MBeanServer to use for registrations */
   protected MBeanServer server;

   // Static --------------------------------------------------------
   
   /**
    * Creates an object from a byte buffer
    */
   public static Object objectFromByteBuffer (byte[] buffer) throws Exception
   {
      if(buffer == null) 
         return null;

      ByteArrayInputStream bais = new ByteArrayInputStream(buffer);
      MarshalledValueInputStream mvis = new MarshalledValueInputStream(bais);
      return mvis.readObject();
   }
   
   /**
    * Serializes an object into a byte buffer.
    * The object has to implement interface Serializable or Externalizable
    */
   public static byte[] objectToByteBuffer (Object obj) throws Exception
   {
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      MarshalledValueOutputStream mvos = new MarshalledValueOutputStream(baos);
      mvos.writeObject(obj);
      mvos.flush();
      return baos.toByteArray();
   }

    // Constructors --------------------------------------------------
       
   public HAPartitionImpl(String partitionName, org.jgroups.JChannel channel, boolean deadlock_detection, MBeanServer server) throws Exception
   {
      this(partitionName, channel, deadlock_detection);
      this.server = server;
   }
   
   public HAPartitionImpl(String partitionName, org.jgroups.JChannel channel, boolean deadlock_detection) throws Exception
   {
      super(channel, null, null, new Object(), false); // init RpcDispatcher with a fake target object
      this.log = Logger.getLogger(HAPartition.class.getName() + "." + partitionName);
      this.clusterLifeCycleLog = Logger.getLogger(HAPartition.class.getName() + ".lifecycle." + partitionName);
      this.channel = channel;
      this.partitionName = partitionName;
      this.history = new Vector();
      logHistory ("Partition object created");
   }
   
    // Public --------------------------------------------------------
   
   public void init() throws Exception
   {
      log.info("Initializing");
      logHistory ("Initializing partition");

      // Subscribe to dHA events comming generated by the org.jgroups. protocol stack
      //
      log.debug("setMembershipListener");
      setMembershipListener(this);
      log.debug("setMessageListener");
      setMessageListener(this);
      
      // Create the DRM and link it to this HAPartition
      //
      log.debug("create replicant manager");
      this.replicantManager = new DistributedReplicantManagerImpl(this, this.server);
      log.debug("init replicant manager");
      this.replicantManager.init();
      log.debug("bind replicant manager");
      
      // Create the DS and link it to this HAPartition
      //
      log.debug("create distributed state");
      this.dsManager = new DistributedStateImpl(this, this.server);
      log.debug("init distributed state service");
      this.dsManager.init();
      log.debug("bind distributed state service");

      
      // Bind ourself in the public JNDI space
      //
      Context ctx = new InitialContext();
      this.bind("/HAPartition/" + partitionName, this, HAPartitionImpl.class, ctx);
      
      log.debug("done initing.");
   }
   
   public void startPartition() throws Exception
   {
      // get current JG group properties
      //
      logHistory ("Starting partition");
      log.debug("get nodeName");
      this.localJGAddress = (IpAddress)channel.getLocalAddress();
      this.me = new ClusterNode(this.localJGAddress);
      this.nodeName = this.me.getName();

      log.debug("Get current members");
      org.jgroups.View view = channel.getView();
      this.jgmembers = (Vector)view.getMembers().clone();
      this.members = translateAddresses(this.jgmembers); // TRANSLATE
      log.info("Number of cluster members: " + members.size());
      for(int m = 0; m > members.size(); m ++)
      {
         Object node = members.get(m);
         log.debug(node);
      }
      // Keep a list of other members only for "exclude-self" RPC calls
      //
      this.jgotherMembers = (Vector)view.getMembers().clone();
      this.jgotherMembers.remove (channel.getLocalAddress());
      this.otherMembers = translateAddresses(this.jgotherMembers); // TRANSLATE
      log.info ("Other members: " + this.otherMembers.size ());

      verifyNodeIsUnique (view.getMembers());

      // Update the initial view id
      //
      this.currentViewId = view.getVid().getId();

      // We must now syncrhonize new state transfer subscriber
      //
      boolean rc = channel.getState(null, 8000);
      if (rc)
         log.debug("State was retrieved successfully");
      else
         log.debug("State could not be retrieved, (must be first member of group)");
      
      // We start now able to start our DRM and DS
      //
      this.replicantManager.start();
      this.dsManager.start();

      // Create the asynch listener handler thread
      AsynchViewChangeHandler asynchHandler = new AsynchViewChangeHandler();
      asynchNotifyThread = new Thread(asynchHandler, "AsynchHAMembershipListener Thread");
      asynchNotifyThread.start();
   }

   public void closePartition() throws Exception
   {
      logHistory ("Closing partition");
      log.info("Closing partition " + partitionName);

      try
      {
         asynchNotifyThread.interrupt();
      }
      catch( Exception e)
      {
         log.warn("Failed to interrupte asynchNotifyThread", e);
      }

      // Stop the DRM and DS services
      //
      try
      {
         this.replicantManager.stop();
      }
      catch (Exception e)
      {
         log.error("operation failed", e);
      }

      try
      {
         this.dsManager.stop();
      }
      catch (Exception e)
      {
         log.error("operation failed", e);
      }

      try
      {
         channel.close();
      }
      catch (Exception e)
      {
         log.error("operation failed", e);
      }

      String boundName = "/HAPartition/" + partitionName;

      InitialContext ctx = new InitialContext();
      try
      {
         
         ctx.unbind(boundName);
      }
      finally
      {
         ctx.close();
      }
      NonSerializableFactory.unbind (boundName);
      
      log.info("Partition " + partitionName + " closed.");
   }

   // org.jgroups.MessageListener implementation ----------------------------------------------

   // MessageListener methods
   //
   public byte[] getState()
   {
      logHistory ("GetState called on partition");
      boolean debug = log.isDebugEnabled();
      
      log.debug("getState called.");
      try
      {
         // we now get the sub-state of each HAPartitionStateTransfer subscribers and
         // build a "macro" state
         //
         HashMap state = new HashMap();
         Iterator keys = stateHandlers.keySet().iterator();
         while (keys.hasNext())
         {
            String key = (String)keys.next();
            HAPartition.HAPartitionStateTransfer subscriber = (HAPartition.HAPartitionStateTransfer)stateHandlers.get(key);
            if (debug)
               log.debug("getState for " + key);
            state.put(key, subscriber.getCurrentState());
         }
         return objectToByteBuffer(state);
      }
      catch (Exception ex)
      {
         log.error("GetState failed", ex);
      }
      return null;
   }
   
   public void setState(byte[] obj)
   {
      logHistory ("SetState called on partition");
      try
      {
         log.debug("setState called");
         if (obj == null)
         {
            log.debug("state is null");
            return;
         }
         
         HashMap state = (HashMap)objectFromByteBuffer(obj);
         java.util.Iterator keys = state.keySet().iterator();
         while (keys.hasNext())
         {
            String key = (String)keys.next();
            log.debug("setState for " + key);
            Object someState = state.get(key);
            HAPartition.HAPartitionStateTransfer subscriber = (HAPartition.HAPartitionStateTransfer)stateHandlers.get(key);
            if (subscriber != null)
            {
               subscriber.setCurrentState((java.io.Serializable)someState);
            }
            else
            {
               log.debug("There is no stateHandler for: " + key);
            }
         }
      }
      catch (Exception ex)
      {
         log.error("setState failed", ex);
      }
   }
   
   public void receive(org.jgroups.Message msg)
   { /* complete */}
   
   // org.jgroups.MembershipListener implementation ----------------------------------------------
   
   public void suspect(org.jgroups.Address suspected_mbr)
   {      
      logHistory ("Node suspected: " + (suspected_mbr==null?"null":suspected_mbr.toString()));
      if (isCurrentNodeCoordinator ())
         clusterLifeCycleLog.info ("Suspected member: " + suspected_mbr);
      else
         log.info("Suspected member: " + suspected_mbr);
   }

   public void block() {}
   
   /** Notification of a cluster view change. This is done from the JG protocol
    * handlder thread and we must be careful to not unduly block this thread.
    * Because of this there are two types of listeners, synchronous and
    * asynchronous. The synchronous listeners are messaged with the view change
    * event using the calling thread while the asynchronous listeners are
    * messaged using a seperate thread.
    *
    * @param newView
    */
   public void viewAccepted(View newView)
   {
      try
      {
         // we update the view id
         //
         this.currentViewId = newView.getVid().getId();

         // Keep a list of other members only for "exclude-self" RPC calls
         //
         this.jgotherMembers = (Vector)newView.getMembers().clone();
         this.jgotherMembers.remove (channel.getLocalAddress());
         this.otherMembers = translateAddresses (this.jgotherMembers); // TRANSLATE!
         Vector translatedNewView = translateAddresses ((Vector)newView.getMembers().clone());
         logHistory ("New view: " + translatedNewView + " with viewId: " + this.currentViewId +
                     " (old view: " + this.members + " )");


         // Save the previous view and make a copy of the new view
         Vector oldMembers = this.members;

         Vector newjgMembers = (Vector)newView.getMembers().clone();
         Vector newMembers = translateAddresses(newjgMembers); // TRANSLATE
         if (this.members == null)
         {
            // Initial viewAccepted
            this.members = newMembers;
            this.jgmembers = newjgMembers;
            log.debug("ViewAccepted: initial members set");
            return;
         }
         this.members = newMembers;
         this.jgmembers = newjgMembers;

         int difference = 0;
         if (oldMembers == null)
            difference = newMembers.size () - 1;
         else
            difference = newMembers.size () - oldMembers.size ();
         
         if (isCurrentNodeCoordinator ())
            clusterLifeCycleLog.info ("New cluster view (id: " + this.currentViewId + ", delta: " + difference + ") : " + this.members);
         else
            log.info("New cluster view: " + this.currentViewId + " (" + this.members + " delta: " + difference + ")");

         // Build a ViewChangeEvent for the asynch listeners
         ViewChangeEvent event = new ViewChangeEvent();
         event.viewId = currentViewId;
         event.allMembers = translatedNewView;
         event.deadMembers = getDeadMembers(oldMembers, event.allMembers);
         event.newMembers = getNewMembers(oldMembers, event.allMembers);
         event.originatingGroups = null;
         // if the new view occurs because of a merge, we first inform listeners of the merge
         if(newView instanceof MergeView)
         {
            MergeView mergeView = (MergeView) newView;
            event.originatingGroups = mergeView.getSubgroups();
         }

         log.debug("membership changed from " + this.members.size() + " to "
            + event.allMembers.size());
         // Put the view change to the asynch queue
         this.asynchViewChanges.put(event);

         // Broadcast the new view to the synchronous view change listeners
         this.notifyListeners(listeners, event.viewId, event.allMembers,
            event.deadMembers, event.newMembers, event.originatingGroups);
      }
      catch (Exception ex)
      {
         log.error("ViewAccepted failed", ex);
      }
   }

   // HAPartition implementation ----------------------------------------------
   
   public String getNodeName()
   {
      return nodeName;
   }
   
   public String getPartitionName()
   {
      return partitionName;
   }
   
   public DistributedReplicantManager getDistributedReplicantManager()
   {
      return replicantManager;
   }
   
   public DistributedState getDistributedStateService()
   {
      return this.dsManager;
   }

   public long getCurrentViewId()
   {
      return this.currentViewId;
   }
   
   public Vector getCurrentView()
   {
      Vector result = new Vector (this.members.size());
      for (int i = 0; i < members.size(); i++)
      {
         result.add( ((ClusterNode) members.elementAt(i)).getName() );
      }
      return result;
   }

   public ClusterNode[] getClusterNodes ()
   {
      ClusterNode[] nodes = new ClusterNode[this.members.size()];
      this.members.toArray(nodes);
      return nodes;
   }

   public boolean isCurrentNodeCoordinator ()
   {
      return this.members.elementAt (0).equals (this.me);
   }

   // ***************************
   // ***************************
   // RPC multicast communication
   // ***************************
   // ***************************
   //
   public void registerRPCHandler(String objName, Object subscriber)
   {
      rpcHandlers.put(objName, subscriber);
   }
   
   public void unregisterRPCHandler(String objName, Object subscriber)
   {
      rpcHandlers.remove(objName);
   }
      
   /**
    * This function is an abstraction of RpcDispatcher.
    */
   public ArrayList callMethodOnCluster(String objName, String methodName, Object[] args, boolean excludeSelf) throws Exception
   {
      ArrayList rtn = new ArrayList();
      MethodCall m = new MethodCall(objName + "." + methodName, args);
      RspList rsp = null;
      
      if (excludeSelf)
      {
         rsp = this.callRemoteMethods(this.jgotherMembers, m, GroupRequest.GET_ALL, timeout);
      }
      else
         rsp = this.callRemoteMethods(null, m, GroupRequest.GET_ALL, timeout);
         
      if (rsp != null)
      {
         for (int i = 0; i < rsp.size(); i++)
         {
            Object item = rsp.elementAt(i);
            if (item instanceof Rsp)
            {
               Rsp response = (Rsp) item;
               // Only include received responses
               boolean wasReceived = response.wasReceived();
               if( wasReceived == true )
               {
                  item = response.getValue();
                  if (!(item instanceof NoHandlerForRPC))
                     rtn.add(item);
               }
               else if( log.isTraceEnabled() )
                  log.trace("Ignoring non-received response: "+response);
            }
            else
            {
               if (!(item instanceof NoHandlerForRPC))
                  rtn.add(item);
            }
         }
      }

      return rtn;
   }
   
   /**
    * This function is an abstraction of RpcDispatcher for asynchronous messages
    */
   public void callAsynchMethodOnCluster(String objName, String methodName, Object[] args, boolean excludeSelf) throws Exception
   {
      MethodCall m = new MethodCall(objName + "." + methodName, args);
      if (excludeSelf)
         this.callRemoteMethods(this.jgotherMembers, m, GroupRequest.GET_NONE, timeout);
      else
         this.callRemoteMethods(null, m, GroupRequest.GET_NONE, timeout);
   }
   
   // *************************
   // *************************
   // State transfer management
   // *************************
   // *************************
   //      
   public void subscribeToStateTransferEvents(String objectName, HAPartitionStateTransfer subscriber)
   {
      stateHandlers.put(objectName, subscriber);
   }
   
   public void unsubscribeFromStateTransferEvents(String objectName, HAPartitionStateTransfer subscriber)
   {
      stateHandlers.remove(objectName);
   }
   
   // *************************
   // *************************
   // Group Membership listeners
   // *************************
   // *************************
   //   
   public void registerMembershipListener(HAMembershipListener listener)
   {
      synchronized(this.listeners)
      {
         this.listeners.add(listener);
      }
   }
   
   public void unregisterMembershipListener(HAMembershipListener listener)
   {
      synchronized(this.listeners)
      {
         this.listeners.remove(listener);
      }
   }
   
   // org.jgroups.RpcDispatcher overrides ---------------------------------------------------
   
   /**
    * Message contains MethodCall. Execute it against *this* object and return result.
    * Use MethodCall.Invoke() to do this. Return result.
    *
    * This overrides RpcDispatcher.Handle so that we can dispatch to many different objects.
    * @param req The org.jgroups. representation of the method invocation
    * @return The serializable return value from the invocation
    */
   public Object handle(Message req)
   {
      Object body = null;
      Object retval = null;
      MethodCall  method_call = null;
      
      if (log.isDebugEnabled())
         log.debug("Partition " + partitionName + " received msg");
      if(req == null || req.getBuffer() == null)
      {
         log.warn("RpcProtocol.Handle(): message or message buffer is null !");
         return null;
      }
      
      try
      {
         body = Util.objectFromByteBuffer(req.getBuffer());
      }
      catch(Exception e)
      {
         log.warn("RpcProtocol.Handle(): " + e);
         return null;
      }
      
      if(body == null || !(body instanceof MethodCall))
      {
         log.warn("RpcProtocol.Handle(): message does not contain a MethodCall object !");
         return null;
      }
      
      // get method call informations
      //
      method_call = (MethodCall)body;
      String methodName = method_call.getName();      
      
      if (log.isDebugEnabled()) log.debug("pre methodName: " + methodName);
      
      int idx = methodName.lastIndexOf('.');
      String handlerName = methodName.substring(0, idx);
      String newMethodName = methodName.substring(idx + 1);
      
      if (log.isDebugEnabled()) 
      {
         log.debug("handlerName: " + handlerName + " methodName: " + newMethodName);
         log.debug("Handle: " + methodName);
      }
      
      // prepare method call
      //
      method_call.setName(newMethodName);
      Object handler = rpcHandlers.get(handlerName);
      
      if (handler == null)
      {
         return new NoHandlerForRPC();
      }

      // Invoke it
      //
      try
      {
         retval = method_call.invoke(handler, method_lookup_clos);
      }
      catch (Error er)
      {
         log.warn (er);
         throw er;
      }
      catch (RuntimeException re)
      {
         log.warn (re);
         throw re;
      }
      catch (Exception ex)
      {
         log.warn (ex);
      }
      catch (Throwable t)
      {
         log.info (t);
      }
      
      return retval;
   }

   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------

   protected void verifyNodeIsUnique (Vector javaGroupIpAddresses) throws Exception
   {
      byte[] localUniqueName = this.localJGAddress.getAdditionalData();
      if (localUniqueName == null)
         log.warn("No additional information has been found in the JavaGroup address: " +
                  "make sure you are running with a correct version of JGroups and that the protocol " +
                  " you are using supports the 'additionalData' behaviour");

      for (int i = 0; i < javaGroupIpAddresses.size(); i++)
      {
         IpAddress address = (IpAddress) javaGroupIpAddresses.elementAt(i);
         if (!address.equals(this.localJGAddress))
         {
            if (localUniqueName.equals(address.getAdditionalData()))
               throw new Exception ("Local node removed from cluster (" + this.localJGAddress + "): another node (" + address + ") publicizing the same name was already there");
         }
      }
   }

   /**
    * Helper method that binds the partition in the JNDI tree.
    * @param jndiName Name under which the object must be bound
    * @param who Object to bind in JNDI
    * @param classType Class type under which should appear the bound object
    * @param ctx Naming context under which we bind the object
    * @throws Exception Thrown if a naming exception occurs during binding
    */   
   protected void bind(String jndiName, Object who, Class classType, Context ctx) throws Exception
   {
      // Ah ! This service isn't serializable, so we use a helper class
      //
      NonSerializableFactory.bind(jndiName, who);
      Name n = ctx.getNameParser("").parse(jndiName);
      while (n.size () > 1)
      {
         String ctxName = n.get (0);
         try
         {
            ctx = (Context)ctx.lookup (ctxName);
         }
         catch (NameNotFoundException e)
         {
            log.debug ("creating Subcontext" + ctxName);
            ctx = ctx.createSubcontext (ctxName);
         }
         n = n.getSuffix (1);
      }

      // The helper class NonSerializableFactory uses address type nns, we go on to
      // use the helper class to bind the service object in JNDI
      //
      StringRefAddr addr = new StringRefAddr("nns", jndiName);
      Reference ref = new Reference(classType.getName (), addr, NonSerializableFactory.class.getName (), null);
      ctx.rebind (n.get (0), ref);
   }
   
   /**
    * Helper method that returns a vector of dead members from two input vectors: new and old vectors of two views.
    * @param oldMembers Vector of old members
    * @param newMembers Vector of new members
    * @return Vector of members that have died between the two views
    */   
   protected Vector getDeadMembers(Vector oldMembers, Vector newMembers)
   {
      boolean debug = log.isDebugEnabled();
      
      Vector dead = new Vector();
      for (int i=0; i<oldMembers.size ();i++)
      {
         if (debug) 
            log.debug("is node " + oldMembers.elementAt(i).toString() + "dead?");
         if (!newMembers.contains(oldMembers.elementAt (i)))
         {
            if (debug) 
               log.debug("node " + oldMembers.elementAt(i).toString()  + "is dead");
            dead.add(oldMembers.elementAt (i));
         }
         else
         {
            if (debug) 
               log.debug("node " + oldMembers.elementAt(i).toString()  + "is NOT dead");
         }
      }
      
      return dead;
   }
   
   /**
    * Helper method that returns a vector of new members from two input vectors: new and old vectors of two views.
    * @param oldMembers Vector of old members
    * @param allMembers Vector of new members
    * @return Vector of members that have joined the partition between the two views
    */   
   protected Vector getNewMembers(Vector oldMembers, Vector allMembers)
   {
      Vector newMembers = new Vector();
      for (int i=0; i<allMembers.size();i++)
         if (!oldMembers.contains (allMembers.elementAt (i)))
            newMembers.add (allMembers.elementAt (i));
      return newMembers;
   }

   protected void notifyListeners(ArrayList theListeners, long viewID,
      Vector allMembers, Vector deadMembers, Vector newMembers,
      Vector originatingGroups)
   {
      log.debug("Begin notifyListeners, viewID: "+viewID);
      synchronized(theListeners)
      {
         for (int i = 0; i < theListeners.size(); i++)
         {
            HAMembershipListener aListener = null;
            try
            {
               aListener = (HAMembershipListener) theListeners.get(i);
               if(originatingGroups != null && (aListener instanceof HAMembershipExtendedListener))
               {
                  HAMembershipExtendedListener exListener = (HAMembershipExtendedListener) aListener;
                  exListener.membershipChangedDuringMerge (deadMembers, newMembers,
                     allMembers, originatingGroups);
               }
               else
               {
                  aListener.membershipChanged(deadMembers, newMembers, allMembers);
               }
            }
            catch (Throwable e)
            {
               // a problem in a listener should not prevent other members to receive the new view
               log.warn("HAMembershipListener callback failure: "+aListener, e);
            }
         }
      }
      log.debug("End notifyListeners, viewID: "+viewID);
   }

   protected Vector translateAddresses (Vector jgAddresses)
   {
      if (jgAddresses == null)
         return null;

      Vector result = new Vector (jgAddresses.size());
      for (int i = 0; i < jgAddresses.size(); i++)
      {
         IpAddress addr = (IpAddress) jgAddresses.elementAt(i);
         result.add(new ClusterNode (addr));
      }

      return result;
   }

   public void logHistory (String message)
   {
      try
      {
         history.add(new SimpleDateFormat().format (new Date()) + " : " + message);
      }
      catch (Exception ignored){}
   }

   /** A simply data class containing the view change event needed to
    * message the HAMembershipListeners
    */
   private static class ViewChangeEvent
   {
      long viewId;
      Vector deadMembers;
      Vector newMembers;
      Vector allMembers;
      Vector originatingGroups;
   }

   /** The Runnable that handles the asynchronous listener notifications
    */
   private class AsynchViewChangeHandler implements Runnable
   {
      public void run()
      {
         log.debug("Begin AsynchViewChangeHandler");
         while( true )
         {
            try
            {
               ViewChangeEvent event = (ViewChangeEvent) asynchViewChanges.take();
               notifyListeners(asynchListeners, event.viewId, event.allMembers,
                  event.deadMembers, event.newMembers, event.originatingGroups);
            }
            catch(InterruptedException e)
            {
               log.debug("AsynchViewChangeHandler interrupted", e);
               break;
            }
         }
         log.debug("End AsynchViewChangeHandler");
      }
   }

   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------

}
