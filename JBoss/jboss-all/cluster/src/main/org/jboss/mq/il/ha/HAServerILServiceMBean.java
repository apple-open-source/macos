/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.ha;

import java.lang.Object;

/**
 *  This interface is used to define which methods will be exposed via JMX.
 *
 * @created    August 16, 2001
 */
public interface HAServerILServiceMBean extends org.jboss.mq.il.ServerILJMXServiceMBean {
	public String getPartitionName();
	public void setPartitionName(String partitionName);
	public String getLoadBalancePolicy();
	public void setLoadBalancePolicy(String loadBalancePolicy);
}
