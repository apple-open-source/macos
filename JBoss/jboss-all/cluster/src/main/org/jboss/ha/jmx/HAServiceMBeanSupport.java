/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.jmx;

import java.io.Serializable;
import java.util.List;
import java.util.Set;

import javax.management.Notification;
import javax.management.ObjectInstance;
import javax.management.Query;
import javax.management.QueryExp;

import org.jboss.ha.framework.interfaces.DistributedReplicantManager;
import org.jboss.ha.framework.interfaces.DistributedState;
import org.jboss.ha.framework.interfaces.HAPartition;
import org.jboss.ha.framework.server.ClusterPartition;
import org.jboss.ha.framework.server.ClusterPartitionMBean;
import org.jboss.mx.util.MBeanProxy;
import org.jboss.system.ServiceMBeanSupport;

/**
 *  
 * Management Bean for an HA-Service.
 * Provides a convenient common base for cluster symmetric MBeans.
 * 
 * This class is also a user transparent extension
 * of the standard NotificationBroadcasterSupport
 * to a clustered environment.
 * Listeners register with their local broadcaster.
 * Invoking sendNotification() on any broadcaster,
 * will notify all listeners in the same cluster partition.
 * TODO: The performance can be further optimized by avoiding broadcast messages
 * when remote listener nodes are not interested (e.g. have no local subscribers)
 * or by iterating locally over filters or remote listeners. 
 *  
 * @author  Ivelin Ivanov <ivelin@apache.org> *
 * @version $Revision: 1.1.2.3 $
 *
 */

public class HAServiceMBeanSupport
  extends ServiceMBeanSupport
  implements HAServiceMBean
{
  // Constants -----------------------------------------------------
 
  // Attributes ----------------------------------------------------
  private HAPartition partition_;
  private String partitionName = "DefaultPartition";

  private DistributedReplicantManager.ReplicantListener drmListener = null;

  /** 
   * DRM participation TOKEN 
   */
  private String REPLICANT_TOKEN = "";

  // Public --------------------------------------------------------

  public HAServiceMBeanSupport()
  {
    // for JMX
  }

  public String getPartitionName()
  {
    return partitionName;
  }

  public void setPartitionName(String newPartitionName)
  {
    if ((getState() != STARTED) && (getState() != STARTING))
    {
      partitionName = newPartitionName;
    }
  }


  // Protected ------------------------------

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
    throws Exception
  {
    DistributedState ds = getPartition().getDistributedStateService();
    ds.set(getServiceHAName(), key, value);
  }

  /**
   * 
   * Convenience method for sharing state across a cluster partition.
   * Delegates to the DistributedStateService
   * 
   * @param key key for the distributed object
   * @return Serializable the distributed object 
   * 
   */
  public Serializable getDistributedState(String key)
  {
    DistributedState ds = getPartition().getDistributedStateService();
    return ds.get(getServiceHAName(), key);
  }

  /**
   * <p>
   * Implementors of this method should not
   * code the singleton logic here. 
   * The MBean lifecycle create/start/stop are separate from
   * the singleton logic.
   * Singleton logic should originate in becomeMaster().
   * </p>
   * 
   * <p>
   * <b>Attention</b>: Always call this method when you overwrite it in a subclass
   *                   because it elects the master singleton node.
   * </p>
   * 
   */
  protected void startService() throws Exception
  {
    boolean debug = log.isDebugEnabled();
    if (debug)
      log.debug("start HASingletonController");

    setupPartition();

    registerRPCHandler();

    registerDRMListener();
  }

  /**
   * <p>
   * <b>Attention</b>: Always call this method when you overwrite it in a subclass
   * </p>
   * 
   */
  protected void stopService() throws Exception
  {
    boolean debug = log.isDebugEnabled();
    if (debug)
      log.debug("stop HASingletonController");

    unregisterDRMListener();

    unregisterRPCHandler();
  }

  protected void setupPartition() throws Exception
  {
    // get handle to the cluster partition
    String pName = getPartitionName();
    partition_ = findHAPartitionWithName(pName);
  }

  protected void registerRPCHandler()
  {
    partition_.registerRPCHandler(getServiceHAName(), this);
  }

  protected void unregisterRPCHandler()
  {
    partition_.unregisterRPCHandler(getServiceHAName(), this);
  }

  protected void registerDRMListener() throws Exception
  {
    DistributedReplicantManager drm =
      this.partition_.getDistributedReplicantManager();

    // register to listen to topology changes, which might cause the election of a new master
    drmListener = new DistributedReplicantManager.ReplicantListener()
    {
      public void replicantsChanged(
        String key,
        List newReplicants,
        int newReplicantsViewId)
      {
        if (key.equals(getServiceHAName()))
        {
          // change in the topology callback
          HAServiceMBeanSupport.this.partitionTopologyChanged(newReplicants, newReplicantsViewId);
        }
      }
    };
    drm.registerListener(getServiceHAName(), drmListener);

    // this ensures that the DRM knows that this node has the MBean deployed 
    drm.add(getServiceHAName(), REPLICANT_TOKEN);
  }

  protected void unregisterDRMListener() throws Exception
  {
    DistributedReplicantManager drm =
      this.partition_.getDistributedReplicantManager();

    // remove replicant node  
    drm.remove(getServiceHAName());

    // unregister 
    drm.unregisterListener(getServiceHAName(), drmListener);
  }

  public void partitionTopologyChanged(List newReplicants, int newReplicantsViewId)
  {
    boolean debug = log.isDebugEnabled();

    if (debug) 
    {
      log.debug("partitionTopologyChanged(). cluster view id: " + newReplicantsViewId);
    } 
  }

  protected boolean isDRMMasterReplica()
  {
    DistributedReplicantManager drm =
      getPartition().getDistributedReplicantManager();

    return drm.isMasterReplica(getServiceHAName());
  }


  public HAPartition getPartition()
  {
    return partition_;
  }

  public void callMethodOnPartition(String methodName, Object[] args)
    throws Exception
  {
    getPartition().callMethodOnCluster(
      getServiceHAName(),
      methodName,
      args,
      true);
  }



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
  public void sendNotification(Notification notification)
  {
    try
    {
      // Overriding the source MBean with its ObjectName
      // to ensure that it can be safely transferred over the wire
      notification.setSource(this.getServiceName());
      sendNotificationRemote(notification);
    }
    
    catch (Throwable th)
    {
      boolean debug = log.isDebugEnabled();
      if (debug)
        log.debug("sendNotificationRemote( " + notification + " ) failed ", th);
      // even if broadcast failed, local notification should still be sent

    }
    sendNotificationToLocalListeners( notification );
  }

  protected void sendNotificationToLocalListeners( Notification notification )
  {
    super.sendNotification(notification);
  }

  protected void callAsyncMethodOnPartition(String methodName, Object[] args)
    throws Exception
  {
    HAPartition partition = getPartition();
    if (partition != null)
    {
      getPartition().callMethodOnCluster(
        getServiceHAName(),
        methodName,
        args,
        true);
    }
  }
  

  /**
   * 
   * Broadcast a notifcation remotely to the partition participants
   * 
   * @param notification
   */
  protected void sendNotificationRemote(Notification notification)
    throws Exception
  {
    callAsyncMethodOnPartition("_receiveRemoteNotification", new Object[] { notification });
  }

  /**
   * 
   * Invoked by remote broadcasters. 
   * Delegates to the super class
   * 
   */
  public void _receiveRemoteNotification(Notification notification)
  {
    super.sendNotification(notification);
  }


  /**
   * 
   * Override this method only if you need to provide a custom partition wide unique service name. 
   * The default implementation will usually work, provided that 
   * the getServiceName() method returns a unique canonical MBean name.
   * 
   * @return partition wide unique service name
   */
  public String getServiceHAName()
  {
    return getServiceName().getCanonicalName();
  }



  protected HAPartition findHAPartitionWithName(String name) throws Exception
  {
    HAPartition result = null;
    QueryExp exp =
      Query.and(
        Query.eq(
          Query.classattr(),
          Query.value(ClusterPartition.class.getName())),
        Query.match(Query.attr("PartitionName"), Query.value(name)));

    Set mbeans = this.getServer().queryMBeans(null, exp);
    if (mbeans != null && mbeans.size() > 0)
    {
      ObjectInstance inst = (ObjectInstance) (mbeans.iterator().next());
      ClusterPartitionMBean cp =
        (ClusterPartitionMBean) MBeanProxy.get(
          ClusterPartitionMBean.class,
          inst.getObjectName(),
          this.getServer());
      result = cp.getHAPartition();
    }

    return result;
  }

  // Private -------------------------------------------------------

  // Inner classes -------------------------------------------------

}
