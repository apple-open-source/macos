/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.invocation.http.server;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.Hashtable;
import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import javax.management.MBeanServer;
import javax.naming.InitialContext;

import org.jboss.invocation.Invoker;
import org.jboss.invocation.InvokerInterceptor;
import org.jboss.invocation.http.interfaces.ClientMethodInterceptorHA;
import org.jboss.invocation.http.interfaces.HttpInvokerProxyHA;
import org.jboss.ha.framework.interfaces.LoadBalancePolicy;
import org.jboss.ha.framework.interfaces.HAPartition;
import org.jboss.ha.framework.server.HATarget;

/** An extension of HttpProxyFactory that supports clustering of invoker proxies.
 * It does this by creating a HATarget that monitors the replication of the
 * invoker url and creates a HAInvokerWrapper that handles wrapping the
 * underlying invocation result with changes to the HATarget replication
 * view.
 *
 *  @author <a href="mailto:scott.stark@jboss.org">Scott Stark</a>
 *  @version $Revision: 1.1.4.5 $
 */
public class HttpProxyFactoryHA extends HttpProxyFactory
   implements HttpProxyFactoryHAMBean
{
   private ObjectName realJmxInvokerName;
   private ObjectName wrappedJmxInvokerName;
   private String partitionName = "DefaultPartition";
   private Class policyClass;
   private HAInvokerWrapper invokerWrapper;
   private HATarget invokerTarget;

   /** Get the server side mbean that exposes the invoke operation for the
    exported interface */
   public Class getLoadBalancePolicy()
   {
      return this.policyClass;
   }
   /** Set the server side mbean that exposes the invoke operation for the
    exported interface */
   public void setLoadBalancePolicy(Class policyClass)
   {
      this.policyClass = policyClass;
   }

   /** Get the name of the cluster partition the invoker is deployed in
    */
   public String getPartitionName()
   {
      return this.partitionName;
   }
   /** Set the name of the cluster partition the invoker is deployed in
    */
   public void setPartitionName(String name)
   {
      this.partitionName = name;
   }

   /** Override the superclass method to create a wrapped ObjectName for the
    * HAInvokerWrapper mbean. This will be the name of the invoker as known
    * by the proxy. The HAInvokerWrapper is the HttpProxyFactoryHA name +
    * wrapperType=httpHA
    *
    * @param jmxInvokerName
    */
   public void setInvokerName(ObjectName jmxInvokerName)
   {
      realJmxInvokerName = jmxInvokerName;
      ObjectName factoryName = getServiceName();
      Hashtable props = factoryName.getKeyPropertyList();
      props.put("wrapperType", "httpHA");
      try
      {
         wrappedJmxInvokerName = new ObjectName(factoryName.getDomain(), props);
         super.setInvokerName(wrappedJmxInvokerName);
      }
      catch(MalformedObjectNameException e)
      {
         throw new IllegalStateException("Was not able to create wrapped ObjectName");
      }
   }

   /**
    * @return the ObjectName the HAInvokerWrapper delegates to
    */
   public ObjectName getRealJmxInvokerName()
   {
      return realJmxInvokerName;
   }

   /**
    *
    */
   protected ArrayList defineInterceptors()
   {
      ArrayList interceptorClasses = new ArrayList();
      interceptorClasses.add(ClientMethodInterceptorHA.class);
      interceptorClasses.add(InvokerInterceptor.class);
      return interceptorClasses;
   }

   /** Override the HttpProxyFactory method to create a HttpInvokerProxyHA.
    * @return
    * @throws Exception
    */
   protected Invoker createInvoker() throws Exception
   {
      InitialContext iniCtx = new InitialContext();
      HAPartition partition = (HAPartition) iniCtx.lookup("/HAPartition/" + partitionName);

      /* Create a HATarget for the local invoker mbean stub. The name passed
      to the HATarget must be the wrappedJmxInvokerName so that different
      more than one HttpProxyFactoryHA may be configured for the same
      realJmxInvokerName.
      */
      checkInvokerURL();
      Serializable invokerStub = super.getInvokerURL();
      invokerTarget = new HATarget(partition, wrappedJmxInvokerName.toString(),
         invokerStub, HATarget.MAKE_INVOCATIONS_WAIT);
      log.debug("Created invoker: "+invokerTarget);
      // Create and register the invoker wrapper
      MBeanServer mbeanServer = super.getServer();
      invokerWrapper = new HAInvokerWrapper(mbeanServer, realJmxInvokerName, invokerTarget);
      mbeanServer.registerMBean(invokerWrapper, wrappedJmxInvokerName);

      // Create the LoadBalancePolicy instance
      LoadBalancePolicy policy = (LoadBalancePolicy) policyClass.newInstance();

      // Finally, create the invoker proxy, a HttpInvokerProxyHA
      String clusterFamilyName = partitionName + "/" + wrappedJmxInvokerName.toString();
      Invoker delegateInvoker = new HttpInvokerProxyHA(invokerTarget.getReplicants(), invokerTarget.getCurrentViewId (),
                                                       policy, clusterFamilyName);
      return delegateInvoker;
   }

   /** Override the HttpProxyFactory stop to unregister the HAInvokerWrapper
    * mbean
    *
    * @throws Exception
    */
   protected void stopService() throws Exception
   {
      try
      {
         MBeanServer mbeanServer = super.getServer();
         mbeanServer.unregisterMBean(wrappedJmxInvokerName);
      }
      catch(Exception e)
      {
         log.debug("Failed to unregister HAInvokerWrapper: "+wrappedJmxInvokerName, e);
      }
      super.stopService();
   }

   /** Destroys the HATarget
    */
   public void destroy()
   {
      super.destroy();
      try
      {
         invokerTarget.destroy();
      }
      catch(Exception ignore)
      {
      }
   }

}
