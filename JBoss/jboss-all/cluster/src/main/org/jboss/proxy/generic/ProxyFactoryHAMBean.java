/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.proxy.generic;

import javax.management.ObjectName;

import org.jboss.invocation.jrmp.server.JRMPProxyFactoryMBean;

/**
 * ProxyFactory for Clustering
 *
 *  @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 *  @version $Revision: 1.1.2.1 $
 */
public interface ProxyFactoryHAMBean
   extends JRMPProxyFactoryMBean
{
   ObjectName getPartitionObjectName();
   void setPartitionObjectName(ObjectName partitionObjectName);
   String getLoadBalancePolicy();
   void setLoadBalancePolicy(String loadBalancePolicy);
}
