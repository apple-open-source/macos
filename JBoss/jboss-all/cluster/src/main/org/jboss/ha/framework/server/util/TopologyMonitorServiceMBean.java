package org.jboss.ha.framework.server.util;

import java.util.Vector;
import javax.management.ObjectName;
import org.jboss.system.Service;

/** A utility mbean that monitors membership of a cluster parition
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.2 $
 */
public interface TopologyMonitorServiceMBean extends Service
{
   /** Get the cluster parition name the mbean is monitoring
    */
   public String getPartitionName();
   /** Set the cluster parition name the mbean is monitoring
    */
   public void setPartitionName(String name);

   /** Get the trigger mbean to notify on cluster membership changes
    */
   public ObjectName getTriggerServiceName();
   /** Set the trigger mbean to notify on cluster membership changes
    */
   public void setTriggerServiceName(ObjectName name);

   /** Get the current cluster parition membership info
    *@return a Vector of org.jgroups.Address implementations, for example,
    *org.jgroups.stack.IpAddress
    */
   public Vector getClusterNodes();
}
