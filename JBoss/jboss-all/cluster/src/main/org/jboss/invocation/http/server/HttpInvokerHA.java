/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.invocation.http.server;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.HashMap;

import javax.management.JMException;
import javax.management.ObjectName;

import org.jboss.mx.util.JMXExceptionDecoder;
import org.jboss.ha.framework.interfaces.HARMIResponse;
import org.jboss.ha.framework.server.HATarget;
import org.jboss.ha.framework.interfaces.LoadBalancePolicy;
import org.jboss.ha.framework.interfaces.GenericClusteringException;
import org.jboss.invocation.http.interfaces.HttpInvokerProxyHA;
import org.jboss.invocation.Invocation;
import org.jboss.invocation.Invoker;
import org.jboss.invocation.InvokerHA;
import org.jboss.system.Registry;

/** An extension of the HttpInvoker and supports clustering of HTTP invokers.
 *
 * @author <a href="mailto:scott.stark@jboss.org>Scott Stark</a>
 * @version $Revision: 1.1.4.4 $
 */
public class HttpInvokerHA extends HttpInvoker
   implements InvokerHA
{
   protected HashMap targetMap = new HashMap();

   // Public --------------------------------------------------------

   protected void startService()
      throws Exception
   {
      // Export the Invoker interface
      ObjectName name = super.getServiceName();
      Registry.bind(name, this);
      // Make sure the invoker URL is valid
      super.checkInvokerURL();
      log.debug("Bound HttpHA invoker for JMX node");
   }

   protected void stopService()
   {
      // Unxport the Invoker interface
      ObjectName name = super.getServiceName();
      Registry.unbind(name);
      log.debug("Unbound HttpHA invoker for JMX node");
   }

   protected void destroyService()
   {
      // Export references to the bean
      Registry.unbind(serviceName);
   }

   public void registerBean(ObjectName targetName, HATarget target) throws Exception
   {
      Integer hash = new Integer(targetName.hashCode());
      log.debug("Registered targetName("+targetName+"), hash="+hash
         + ", target="+target);
      if (targetMap.containsKey(hash))
      {
         throw new IllegalStateException("Duplicate targetName("+targetName
            + ") hashCode: "+hash);
      }
      targetMap.put(hash, target);
   }

   public void unregisterBean(ObjectName targetName) throws Exception
   {
      Integer hash = new Integer(targetName.hashCode());
      targetMap.remove(hash);
      log.debug("Unregistered targetName("+targetName+"), hash="+hash);
   }

   public Invoker createProxy(ObjectName targetName, LoadBalancePolicy policy,
                              String proxyFamilyName)
      throws Exception
   {
      Integer hash = new Integer(targetName.hashCode());
      HATarget target = (HATarget) targetMap.get(hash);
      if (target == null)
      {
         throw new IllegalStateException("The targetName("+targetName
            + "), hashCode("+hash+") not found");
      }
      Invoker proxy = new HttpInvokerProxyHA(target.getReplicants(), target.getCurrentViewId (),
                                             policy, proxyFamilyName);
      return proxy;
   }

   public Serializable getStub()
   {
      return super.getInvokerURL();
   }

   /**
    * Invoke a Remote interface method.
    */
   public Object invoke(Invocation invocation)
      throws Exception
   {
      ClassLoader oldCl = Thread.currentThread().getContextClassLoader();
      try
      {
         Integer nameHash = (Integer) invocation.getObjectName();
         ObjectName mbean = (ObjectName) Registry.lookup(nameHash);

         // The cl on the thread should be set in another interceptor
         Object[] args = {invocation};
         String[] sig = {"org.jboss.invocation.Invocation"};
         Object rtn = super.getServer().invoke(mbean, 
            "invoke", args, sig);

         // Update the targets list if the client view is out of date
         Long clientViewId = (Long) invocation.getValue("CLUSTER_VIEW_ID");
         HARMIResponse rsp = new HARMIResponse();
         HATarget target = (HATarget) targetMap.get(nameHash);
         if (target == null)
         {
            throw new IllegalStateException("The name for hashCode("+nameHash+") was not found");
         }
         if (clientViewId.longValue() != target.getCurrentViewId())
         {
            rsp.newReplicants = new ArrayList(target.getReplicants());
            rsp.currentViewId = target.getCurrentViewId();
         }
         rsp.response = rtn;

         // Return the raw object and let the http layer marshall it
         return rsp;
      }
      catch (Exception e)
      {
         // Unwrap any JMX exceptions
         e = (Exception) JMXExceptionDecoder.decode(e);
         // Don't send JMX exception back to client to avoid needing jmx
         if( e instanceof JMException )
            e = new GenericClusteringException (GenericClusteringException.COMPLETED_NO, e.getMessage());

         // Only log errors if trace is enabled
         if( log.isTraceEnabled() )
            log.trace("operation failed", e);
         throw e;
      }
      finally
      {
         Thread.currentThread().setContextClassLoader(oldCl);
      }
   }
}
