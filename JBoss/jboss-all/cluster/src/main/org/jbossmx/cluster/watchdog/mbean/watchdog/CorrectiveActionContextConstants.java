/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean.watchdog;

/**
 * Property keys to use for CorrectiveActionContext
 */
public class CorrectiveActionContextConstants
{
    /** No need to construct CorrectiveActionContextConstants
     */
    private CorrectiveActionContextConstants() {}

    /** Property key which refers to the Agent's MBeanServer */
    public static final String Agent_MBeanServer = "Agent.MBeanServer";
    /** Property key which refers to the Agent's RMI Binding */
    public static final String Agent_RmiBinding = "Agent.RmiBinding";
    /** Property key which refers to the Agent's Remote Interface */
    public static final String Agent_RemoteInterface = "Agent.RemoteInterface";
    /** Property key which refers to the MBean's ObjectName */
    public static final String MBean_ObjectName = "MBean.ObjectName";
    /** Property key which refers to the MBean's class */
    public static final String MBean_Class = "MBean.Class";
}
