/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.invocation.jrmp.interfaces;

import java.io.Externalizable;
import java.io.IOException;
import java.io.ObjectInput;
import java.io.ObjectOutput;
import java.rmi.MarshalledObject;
import java.rmi.RemoteException;
import java.rmi.ServerException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.WeakHashMap;
import javax.transaction.TransactionRolledbackException;

import org.jboss.ha.framework.interfaces.ClusteringTargetsRepository;
import org.jboss.ha.framework.interfaces.FamilyClusterInfo;
import org.jboss.ha.framework.interfaces.GenericClusteringException;
import org.jboss.ha.framework.interfaces.HARMIResponse;
import org.jboss.ha.framework.interfaces.LoadBalancePolicy;
import org.jboss.invocation.Invocation;
import org.jboss.invocation.Invoker;
import org.jboss.invocation.InvokerInterceptor;
import org.jboss.invocation.InvokerProxyHA;
import org.jboss.invocation.MarshalledInvocation;
import org.jboss.invocation.PayloadKey;
import org.jboss.logging.Logger;

/**
 * An extension of the JRMPInvokerProxy that supports failover and load
 * balancing among a
 * 
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.7.2.10 $
 */
public class JRMPInvokerProxyHA
   extends JRMPInvokerProxy
   implements InvokerProxyHA, Externalizable
{
   // Public --------------------------------------------------------
   /** The serialVersionUID
    * @since 1.7.2.8
    */ 
   private static final long serialVersionUID = -967671822225981666L;
   private static final Logger log = Logger.getLogger(JRMPInvokerProxyHA.class);
   public static final HashSet colocation = new HashSet();
   public static final WeakHashMap txFailoverAuthorizations = new WeakHashMap();

   protected LoadBalancePolicy loadBalancePolicy;
   protected String proxyFamilyName = null;

   FamilyClusterInfo familyClusterInfo = null;
   //protected transient long currentViewId = 0;
   /** Trace level logging flag only set when the proxy is created or read from JNDI */
   protected transient boolean trace = false;

   public JRMPInvokerProxyHA() {}

   public JRMPInvokerProxyHA(ArrayList targets, LoadBalancePolicy policy,
                             String proxyFamilyName, long viewId)
   {
      this.familyClusterInfo = ClusteringTargetsRepository.initTarget (proxyFamilyName, targets, viewId);
      this.loadBalancePolicy = policy;
      this.proxyFamilyName = proxyFamilyName;
      this.trace = log.isTraceEnabled();
      if( trace )
         log.trace("Init, cluterInfo: "+familyClusterInfo+", policy="+loadBalancePolicy);
   }

   public void updateClusterInfo (ArrayList targets, long viewId)
   {
      if (familyClusterInfo != null)
         this.familyClusterInfo.updateClusterInfo (targets, viewId);
   }

   public Object getRemoteTarget()
   {
      return getRemoteTarget(null);
   }
   public Object getRemoteTarget(Invocation invocationBasedRouting)
   {
      return loadBalancePolicy.chooseTarget(this.familyClusterInfo, invocationBasedRouting);
   }

   public void remoteTargetHasFailed(Object target)
   {
      removeDeadTarget(target);
   }

   protected void removeDeadTarget(Object target)
   {
      //System.out.println("Removing a dead target: Size before : " + Integer.toString(this.familyClusterInfo.getTargets ().size()));
      if (this.familyClusterInfo != null)
         this.familyClusterInfo.removeDeadTarget (target);
   }

   protected int totalNumberOfTargets ()
   {
      if (this.familyClusterInfo != null)
         return this.familyClusterInfo.getTargets ().size ();
      else
         return 0;
   }

   protected void resetView ()
   {
      this.familyClusterInfo.resetView ();
   }

   /**
   * Returns wether we are local to the originating container or not.
   */
   public boolean isLocal(Invocation invocation)
   {
      return colocation.contains(invocation.getObjectName());
   }
   
   public boolean txContextAllowsFailover (Invocation invocation)
   {
      javax.transaction.Transaction tx = invocation.getTransaction();
      if (tx != null)
      {
         synchronized (tx)
         {
            return ! txFailoverAuthorizations.containsKey (tx);               
         }
      }
      else
      {
         return true;
      }
   }
   
   public void invocationHasReachedAServer (Invocation invocation)
   {
      javax.transaction.Transaction tx = invocation.getTransaction();
      if (tx != null)
      {
         synchronized (tx)
         {
            txFailoverAuthorizations.put (tx, null);               
         }
      }
   }

   /**
   * The invocation on the delegate, calls the right invoker.  Remote if we are remote, local if we
   * are local.
   */
   public Object invoke(Invocation invocation)
      throws Exception
   {
      // we give the opportunity, to any server interceptor, to know if this a
      // first invocation to a node or if it is a failovered call
      //
      int failoverCounter = 0;
      invocation.setValue ("FAILOVER_COUNTER", new Integer(failoverCounter), PayloadKey.AS_IS);

      // optimize if calling another bean in same EJB-application
      if (isLocal(invocation))
      {
         return InvokerInterceptor.getLocal().invoke(invocation);
      }
      else
      {
         // We are going to go through a Remote invocation, switch to a Marshalled Invocation
         MarshalledInvocation mi = new MarshalledInvocation(invocation);

         // Set the transaction propagation context
         mi.setTransactionPropagationContext(getTransactionPropagationContext());
         mi.setValue("CLUSTER_VIEW_ID", new Long(this.familyClusterInfo.getCurrentViewId ()));
         Invoker target = (Invoker)getRemoteTarget(invocation);
         
         boolean failoverAuthorized = true;
         Exception lastException;
         while (target != null && failoverAuthorized)
         {                        
            boolean definitivlyRemoveNodeOnFailure = true;
            try
            {
               if( trace )
                  log.trace("Invoking on target="+target);
               Object rtnObj = target.invoke(mi);
               HARMIResponse rsp = null;
               if (rtnObj instanceof MarshalledObject)
               {
                  rsp = (HARMIResponse)((MarshalledObject)rtnObj).get();
               }
               else
               {
                  rsp = (HARMIResponse)rtnObj;
               }
               if (rsp.newReplicants != null)
               {
                  if( trace )
                  {
                     log.trace("newReplicants: "+rsp.newReplicants);
                  }
                  updateClusterInfo (rsp.newReplicants, rsp.currentViewId);
               }
               //else System.out.println("Static set of replicants: " + this.familyClusterInfo.getCurrentViewId () + " (me = " + this + ")");
               
               invocationHasReachedAServer (invocation);

               return rsp.response;
            }
            catch (java.net.ConnectException e)
            {
               lastException = e;
            }
            catch (java.net.UnknownHostException e)
            {
               lastException = e;
            }
            catch (java.rmi.ConnectException e)
            {
               lastException = e;
            }
            catch (java.rmi.ConnectIOException e)
            {
               lastException = e;
            }
            catch (java.rmi.NoSuchObjectException e)
            {
               lastException = e;
            }
            catch (java.rmi.UnmarshalException e)
            {
               lastException = e;
            }
            catch (java.rmi.UnknownHostException e)
            {
               lastException = e;
            }
            catch (GenericClusteringException e)
            {
               lastException = e;
               // this is a generic clustering exception that contain the
               // completion status: usefull to determine if we are authorized
               // to re-issue a query to another node
               //
               if (e.getCompletionStatus () == GenericClusteringException.COMPLETED_NO)
               {
                  // we don't want to remove the node from the list of failed
                  // node UNLESS there is a risk to indefinitively loop
                  //
                  if (totalNumberOfTargets() >= failoverCounter)
                  {
                     if (!e.isDefinitive ())
                        definitivlyRemoveNodeOnFailure = false;
                  }
               }
               else
               {
                  invocationHasReachedAServer (invocation);
                  throw new ServerException("Clustering error", e);
               }
            }
            catch (ServerException e)
            {
               //Why do NoSuchObjectExceptions get ignored for a retry here
               //unlike in the non-HA case?
               invocationHasReachedAServer (invocation);
               if (e.detail instanceof TransactionRolledbackException)
               {                  
                  throw (TransactionRolledbackException) e.detail;
               }
               throw e;
            }
            catch (Exception e)
            {
               lastException = e;
               invocationHasReachedAServer (invocation);
               throw e;
            }

            if( trace )
               log.trace("Invoke failed, target="+target, lastException);

            // If we reach here, this means that we must fail-over
            remoteTargetHasFailed(target);
            if (!definitivlyRemoveNodeOnFailure)
            {
               resetView ();
            }

            failoverAuthorized = txContextAllowsFailover (invocation);            
            target = (Invoker)getRemoteTarget(invocation);

            failoverCounter++;
            mi.setValue ("FAILOVER_COUNTER", new Integer(failoverCounter), PayloadKey.AS_IS);
         }
         // if we get here this means list was exhausted
         if (failoverAuthorized)
            throw new RemoteException("Service unavailable.");
         else
            throw new RemoteException("Service unavailable (failover not possible inside a user transaction).");
      }
   }

   /**
   *  Externalize this instance.
   *
   *  If this instance lives in a different VM than its container
   *  invoker, the remote interface of the container invoker is
   *  not externalized.
   */
   public void writeExternal(final ObjectOutput out)
      throws IOException
   {
      ArrayList targets = this.familyClusterInfo.getTargets();
      long vid = this.familyClusterInfo.getCurrentViewId ();
      targets.trimToSize();
      out.writeObject(targets);
      out.writeObject(this.loadBalancePolicy);
      out.writeObject (this.proxyFamilyName);
      out.writeLong (vid);
   }

   /**
   *  Un-externalize this instance.
   *
   *  We check timestamps of the interfaces to see if the instance is in the original VM of creation
   */
   public void readExternal(final ObjectInput in)
   throws IOException, ClassNotFoundException
   {
      ArrayList targets = (ArrayList)in.readObject();
      this.loadBalancePolicy = (LoadBalancePolicy)in.readObject();
      this.proxyFamilyName = (String)in.readObject();
      long vid = in.readLong ();

      // keep a reference on our family object
      //
      this.familyClusterInfo = ClusteringTargetsRepository.initTarget (this.proxyFamilyName, targets, vid);
      this.trace = log.isTraceEnabled();
      if( trace )
         log.trace("Init, clusterInfo: "+familyClusterInfo+", policy="+loadBalancePolicy);
   }

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}
