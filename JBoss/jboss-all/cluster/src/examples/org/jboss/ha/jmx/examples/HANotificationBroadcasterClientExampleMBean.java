/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ha.jmx.examples;

import java.util.Collection;

import javax.management.InstanceNotFoundException;
import javax.management.MBeanException;
import javax.management.ReflectionException;

import org.jboss.system.ServiceMBean;



/**
 * 
 * @see org.jboss.ha.jmx.notification.examples.HANotificationBroadcasterExampleMBean
 * 
 * @author  Ivelin Ivanov <ivelin@apache.org>
 *
 */
public interface HANotificationBroadcasterClientExampleMBean
  extends ServiceMBean
{
  /**
   * @return the name of the broadcaster MBean
   */
  public String getHANotificationBroadcasterName();

  /**
   * 
   * Sets the name of the broadcaster MBean.
   * 
   * @param 
   */
  public void setHANotificationBroadcasterName( String newBroadcasterName );

  /**
   * Broadcasts a notification to the cluster partition
   * via the HANBExample MBean
   *
   */
  public void sendTextMessageViaHANBExample(String message) 
    throws InstanceNotFoundException, MBeanException, ReflectionException;
  
  /**
   * Lists the notifications received on the cluster partition
   */
  public Collection getReceivedNotifications();
}
