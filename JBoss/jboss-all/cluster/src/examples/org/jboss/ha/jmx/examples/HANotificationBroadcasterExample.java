/* 
 * ====================================================================
 * This is Open Source Software, distributed
 * under the Apache Software License, Version 1.1
 * 
 * 
 *  This software  consists of voluntary contributions made  by many individuals
 *  on  behalf of the Apache Software  Foundation and was  originally created by
 *  Ivelin Ivanov <ivelin@apache.org>. For more  information on the Apache
 *  Software Foundation, please see <http://www.apache.org/>.
 */

package org.jboss.ha.jmx.examples;

import java.util.Collection;
import java.util.LinkedList;

import javax.management.Notification;
import javax.management.NotificationListener;

import org.jboss.ha.jmx.HAServiceMBeanSupport;

/**
 * 
 * This MBean is an example showing how to extend a cluster notification broadcaster 
 * Use the sendNotiication() operation to trigger new clustered notifications.
 * Observe the status of each instance of this mbean in the participating cluster partition nodes.
 * 
 * @author  Ivelin Ivanov <ivelin@apache.org>
 *
 */
public class HANotificationBroadcasterExample
  extends HAServiceMBeanSupport
  implements HANotificationBroadcasterExampleMBean
{
  
  /**
   * 
   * On service start, subscribes to notification sent by this broadcaster or its remote peers.
   * 
   */
  protected void startService() throws Exception
  {
    super.startService();
    addNotificationListener(listener_, /* no need for filter */ null, /* no handback object */ null);
  }
  
  /**
   * 
   * On service stop, unsubscribes to notification sent by this broadcaster or its remote peers.
   * 
   */  
  protected void stopService() throws Exception
  {
    removeNotificationListener(listener_);
    super.stopService();
  }
  
  /**
   * Broadcasts a notification to the cluster partition.
   * 
   * This example does not ensure that a notification sequence number 
   * is unique throughout the partition.
   * 
   */
  public void sendTextMessage(String message)
  {
    long now = System.currentTimeMillis();
    Notification notification =  
      new Notification("hanotification.example.counter", super.getServiceName(), now, now, message);
    sendNotification(notification);
  }

  /**
   * Lists the notifications received on the cluster partition
   */
  public Collection getReceivedNotifications()
  {
    return messages_;
  }


  Collection messages_ = new LinkedList();
 
  NotificationListener listener_ = new NotificationListener()
  {
    public void handleNotification(Notification notification,
                                   java.lang.Object handback)
     {
       messages_.add( notification.getMessage() );
     }
  };
  
}
