/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.invocation.http.interfaces;

import java.io.Externalizable;
import java.io.IOException;
import java.io.ObjectInput;
import java.io.ObjectOutput;
import java.lang.reflect.InvocationTargetException;
import java.net.URL;
import java.rmi.ServerException;
import java.util.ArrayList;

import org.jboss.ha.framework.interfaces.ClusteringTargetsRepository;
import org.jboss.ha.framework.interfaces.GenericClusteringException;
import org.jboss.ha.framework.interfaces.HARMIResponse;
import org.jboss.ha.framework.interfaces.LoadBalancePolicy;
import org.jboss.ha.framework.interfaces.FamilyClusterInfo;
import org.jboss.invocation.Invocation;
import org.jboss.invocation.InvocationException;
import org.jboss.invocation.Invoker;
import org.jboss.invocation.InvokerProxyHA;
import org.jboss.invocation.MarshalledInvocation;
import org.jboss.invocation.PayloadKey;
import org.jboss.logging.Logger;

/** The client side Http invoker proxy that posts an invocation to the
 InvokerServlet using the HttpURLConnection created from a target url.
 This proxy handles failover using its associated LoadBalancePolicy and
 current list of URL strings. The candidate URLs are updated dynamically
 after an invocation if the cluster partitation view has changed.

* @author Scott.Stark@jboss.org
* @version $Revision: 1.1.4.7 $
*/
public class HttpInvokerProxyHA
   implements InvokerProxyHA, Invoker, Externalizable
{
   // Constants -----------------------------------------------------
   private static Logger log = Logger.getLogger(HttpInvokerProxyHA.class);

   /** Serial Version Identifier.
    * @since 1.1.4.5
    */
   private static final long serialVersionUID = -7081220026780794383L;

   // Attributes ----------------------------------------------------

   // URL to the remote JMX node invoker
   protected LoadBalancePolicy loadBalancePolicy;
   protected String proxyFamilyName = null;
   protected FamilyClusterInfo familyClusterInfo = null;
   /** Trace level logging flag only set when the proxy is created or read from JNDI */
   protected transient boolean trace = false;

   // Constructors --------------------------------------------------
   public HttpInvokerProxyHA()
   {
      // For externalization to work
   }

   /**
    * @param targets  the list of URLs through which clients should contact the
    * InvokerServlet.
    * @param policy  the policy for choosing among targets ClusteringTargetsRepository under
    *    which this proxy is to be stored
    * @param proxyFamilyName  the name into the 
   */
   public HttpInvokerProxyHA(ArrayList targets, long viewId, LoadBalancePolicy policy,
      String proxyFamilyName)
   {
      this.familyClusterInfo = ClusteringTargetsRepository.initTarget (proxyFamilyName, targets, viewId);
      this.loadBalancePolicy = policy;
      this.proxyFamilyName = proxyFamilyName;
      this.trace = log.isTraceEnabled();
      if( trace )
         log.trace("Init, cluterInfo: "+familyClusterInfo+", policy="+loadBalancePolicy);
   }

   // Public --------------------------------------------------------

   public void updateClusterInfo (ArrayList targets, long viewId)
   {
      if (familyClusterInfo != null)
         this.familyClusterInfo.updateClusterInfo (targets, viewId);
   }

   public String getServerHostName() throws Exception
   {
      return null;
   }

   public FamilyClusterInfo getClusterInfo()
   {
      return familyClusterInfo;
   }

   public Object getRemoteTarget()
   {
      return getRemoteTarget(null);
   }
   public Object getRemoteTarget(Invocation invocationBasedRouting)
   {
      Object target = loadBalancePolicy.chooseTarget(this.familyClusterInfo, invocationBasedRouting);
      if( trace )
         log.trace("Choose remoteTarget: "+target);
      return target;
   }

   public void remoteTargetHasFailed(Object target)
   {
      removeDeadTarget(target);
   }   

   protected int totalNumberOfTargets ()
   {
      int size = 0;
      if( familyClusterInfo != null )
         size = familyClusterInfo.getTargets().size();
      return size;
   }

   protected void removeDeadTarget(Object target)
   {
      if( familyClusterInfo != null )
      {
         ArrayList targets = familyClusterInfo.removeDeadTarget(target);
         if( trace )
         {
            log.trace("removeDeadTarget("+target+"), targets.size="+targets.size());
         }
      }
   }
   protected void resetView ()
   {
      familyClusterInfo.resetView();
   }

   /** This method builds a MarshalledInvocation from the invocation passed
    in and then does a post to the target URL.
   */
   public Object invoke(Invocation invocation)
      throws Exception
   {
      // we give the opportunity, to any server interceptor, to know if this a
      // first invocation to a node or if it is a failovered call
      //
      int failoverCounter = 0;

      // We are going to go through a Remote invocation, switch to a Marshalled Invocation
      MarshalledInvocation mi = new MarshalledInvocation(invocation);         
      mi.setValue("CLUSTER_VIEW_ID", new Long(familyClusterInfo.getCurrentViewId()));
      String target = (String) getRemoteTarget(invocation);
      URL externalURL = Util.resolveURL(target);
      Exception lastException = null;
      while( externalURL != null )
      {
         boolean definitivlyRemoveNodeOnFailure = true;
         invocation.setValue("FAILOVER_COUNTER", new Integer(failoverCounter), PayloadKey.AS_IS);
         try
         {
            if( trace )
               log.trace("Invoking on target="+externalURL);
            Object rtn = Util.invoke(externalURL, mi);
            HARMIResponse rsp = (HARMIResponse) rtn;

            if (rsp.newReplicants != null)
               updateClusterInfo(rsp.newReplicants, rsp.currentViewId);
            return rsp.response;
         }
         catch(GenericClusteringException e)
         {
            // this is a generic clustering exception that contain the
            // completion status: usefull to determine if we are authorized
            // to re-issue a query to another node
            //               
            if( e.getCompletionStatus() != e.COMPLETED_NO )
            {
                  // we don't want to remove the node from the list of targets 
                  // UNLESS there is a risk to loop
                  if (totalNumberOfTargets() >= failoverCounter)
                  {
                     if( e.isDefinitive() == false )
                        definitivlyRemoveNodeOnFailure = false;                     
                  }
            }
            else
            {
               throw new ServerException("Cannot proceed beyond target="+externalURL, e);
            }
         }
         catch(InvocationException e)
         {
            // Handle application declared exceptions
            Throwable cause = e.getTargetException();
            if( cause instanceof Exception )
               throw (Exception) cause;
            else if (cause instanceof Error)
               throw (Error) cause;
            throw new InvocationTargetException(cause);
         }
         catch(IOException e)
         {
            if( trace )
               log.trace("Invoke failed, target="+externalURL, e);
            lastException = e;
         }
         catch(Exception e)
         {
            // Rethrow for the application to handle
            throw e;
         }

         // If we reach here, this means that we must fail-over
         remoteTargetHasFailed(target);
         if( definitivlyRemoveNodeOnFailure )
            resetView();
         target = (String) getRemoteTarget(invocation);
         externalURL = Util.resolveURL(target);
         failoverCounter ++;
      }
      // if we get here this means list was exhausted
      throw new ServerException("Service unavailable last exception:", lastException);
   }

   /** Externalize this instance.
   */
   public void writeExternal(final ObjectOutput out)
      throws IOException
   { 
      out.writeObject(this.familyClusterInfo.getTargets());
      out.writeLong(this.familyClusterInfo.getCurrentViewId ());
      out.writeObject(this.loadBalancePolicy);
      out.writeObject(this.proxyFamilyName);
   }

   /** Un-externalize this instance.
   */
   public void readExternal(final ObjectInput in)
      throws IOException, ClassNotFoundException
   {
      ArrayList targets = (ArrayList) in.readObject();
      long vid = in.readLong ();
      this.loadBalancePolicy = (LoadBalancePolicy) in.readObject();
      this.proxyFamilyName = (String)in.readObject();
      this.trace = log.isTraceEnabled();

      // keep a reference on our family object
      this.familyClusterInfo = ClusteringTargetsRepository.initTarget(this.proxyFamilyName, targets, vid);
      if( trace )
         log.trace("Init, clusterInfo: "+familyClusterInfo+", policy="+loadBalancePolicy);
   }
}

