/*
* JBoss, the OpenSource J2EE WebOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.invocation.http.server;

import javax.management.ObjectName;

import org.jboss.invocation.http.server.HttpProxyFactoryMBean;

/** An mbean interface that extends the HttpProxyFactoryMBean to provide
 * support for cluster aware proxies. This interface adds the
 * ability to configure the load-balancing policy of the proxy as well
 * as the cluster partition name the mbean belongs to.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.2 $
 */
public interface HttpProxyFactoryHAMBean extends HttpProxyFactoryMBean
{
   /** Get the server side mbean that exposes the invoke operation for the
    exported interface */
   public Class getLoadBalancePolicy();
   /** Set the server side mbean that exposes the invoke operation for the
    exported interface */
   public void setLoadBalancePolicy(Class policyClass);

   /** Get the name of the cluster partition the invoker is deployed in
    */
   public String getPartitionName();
   /** Set the name of the cluster partition the invoker is deployed in
    */
   public void setPartitionName(String name);

   /** A read-only property for accessing the non-wrapped JMX invoker
    *
    */
   public ObjectName getRealJmxInvokerName();
}
