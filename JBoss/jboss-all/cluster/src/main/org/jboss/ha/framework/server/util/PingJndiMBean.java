package org.jboss.ha.framework.server.util;

import java.util.ArrayList;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public interface PingJndiMBean
{
   /** Get the names of JNDI bindings that should be queried on each host
    * @return the array of target names to test
    */
   public String[] getLookupNames();
   /** Set the names of JNDI bindings that should be queried on each host
    * @param names
    */
   public void setLookupNames(String[] names);

   /** Get the Context.PROVIDER_URL regular expression.
    * @return the expression containing the ${host} reference, for example
    * 'jnp://${host}:1099/'
    */
   public String getProviderURLPattern();
   /** Set the expression containing the hostname/IP ${host} reference of
    * the JNDI provider. This expression is used to build the JNDI
    * Context.PROVIDER_URL for each node in the cluster. The expression
    * should contain a "(host)" component that will be replaced with the
    * cluster node hostname.
    *
    * @param regex the regular expression containing the host, for example
    * 'jnp://(host):1099/'
    */
   public void setProviderURLPattern(String regex);


   /** The TopologyMonitorService trigger callback operation.
    *
    * @param deadMembers ArrayList<AddressPort> of nodes that were removed
    * @param newMembers ArrayList<AddressPort> of nodes that were added
    * @param allMembers ArrayList<AddressPort> of nodes currently in the cluster
    * @param logCategoryName the log4j category name used by the
    * TopologyMonitorService. This is used for logging to integrate with
    * the TopologyMonitorService output.
    */
   public void membershipChanged(ArrayList deadMembers, ArrayList newMembers,
      ArrayList allMembers, String logCategoryName);
}
