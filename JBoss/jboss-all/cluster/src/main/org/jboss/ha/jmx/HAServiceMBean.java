/***************************************
*                                     *
*  JBoss: The OpenSource J2EE WebOS   *
*                                     *
*  Distributable under LGPL license.  *
*  See terms of license at gnu.org.   *
*                                     *
***************************************/
package org.jboss.ha.jmx;

import java.io.Serializable;

import javax.management.Notification;
import javax.management.NotificationBroadcaster;

/**
 * <p>
 * HA-Service interface.
 * Defines common functionality for partition symmetric (farming) services.
 * </p>
 *
 * @author <a href="mailto:ivelin@apache.org">Ivelin Ivanov</a>
 * @version $Revision: 1.1.2.2 $
 *
 */

public interface HAServiceMBean 
  extends org.jboss.system.ServiceMBean, NotificationBroadcaster
{

	/**
	 * Name of the underlying partition that determine the cluster to use.
	 */
	public String getPartitionName();

	/**
	 * Set the name of the underlying partition that determine the cluster to use.
	 * Can be set only when the MBean is not in a STARTED or STARTING state.
	 */
	public void setPartitionName(String partitionName);


  /**
   * 
   * Convenience method for broadcasting a call to all members 
   * of a partition.
   * 
   * @param methodName
   * @param args
   * @throws Exception
   */
  public void callMethodOnPartition(String methodName, Object[] args)
    throws Exception;

  /**
   * 
   * Convenience method for sharing state across a cluster partition.
   * Delegates to the DistributedStateService
   * 
   * @param key key for the distributed object
   * @return Serializable the distributed object 
   * 
   */
  public Serializable getDistributedState(String key);

  /**
   * 
   * 
   * Convenience method for sharing state across a cluster partition.
   * Delegates to the DistributedStateService
   * 
   * @param key key for the distributed object
   * @param value the distributed object
   * 
   */
  public void setDistributedState(String key, Serializable value)
    throws Exception;


  /**
   * 
   * Broadcast the notification to the remote listener nodes (if any) and then 
   * invoke super.sendNotification() to notify local listeners.
   * 
   * @param notification sent out to local listeners and other nodes. It should be serializable.
   * It is recommended that the source of the notification is an ObjectName of an MBean that 
   * is is available on all nodes where the broadcaster MBean is registered. 
   *   
   * 
   * @see javax.management.NotificationBroadcasterSupport#sendNotification(Notification)
   * 
   */
  public void sendNotification(Notification notification);
  
}
