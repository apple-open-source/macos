/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ha.jmx.examples;

import java.util.Collection;

import org.jboss.ha.jmx.HAServiceMBean;

/**
 * 
 * Sample HANotificationBroadcaster MBean
 * 
 * @author  Ivelin Ivanov <ivelin@apache.org>
 *
 */
public interface HANotificationBroadcasterExampleMBean
  extends HAServiceMBean
{

  /**
   * Name of the underlying partition that determine the cluster to use.
   */
  public String getPartitionName();

  /**
   * Broadcasts a notification to the cluster partition
   *
   */
  public void sendTextMessage(String message);
  
  /**
   * Lists the notifications received on the cluster partition
   */
  public Collection getReceivedNotifications();


}
