/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.ha;

import java.net.InetAddress;
import java.net.UnknownHostException;

import javax.management.AttributeNotFoundException;
import javax.management.InstanceNotFoundException;
import javax.management.MBeanException;
import javax.management.ObjectName;
import javax.management.ReflectionException;

import org.jboss.mq.il.ServerILJMXServiceMBean;

/** 
 * 
 * This is the server side MBean for the HAIL transport layer.
 * It builts upon UIL2.
 *
 * @author ivelin@apache.org
 * @version   $Revision: 1.1.2.1 $
 *
 * @jmx:mbean extends="org.jboss.mq.il.ServerILJMXServiceMBean"
 */
public interface HAILServerILServiceMBean extends ServerILJMXServiceMBean
{

  /**
   * Get the name of the MBean UILService.
   *
   * @return name of the MBean UILService.
   */
  public ObjectName getUILService();

  /**
   * Set the name of the MBean UILService.
   *
   * @param v  assign the name of the MBean UILService.
   */
  public void setUILService(ObjectName uilService);

  /**
   * Get the name of the MBean HANotificationBroadcaster.
   * Used to keep all participating JMS nodes informed about the current singleton server.
   *
   * @return name of the MBean HANotificationBroadcaster.
   */
  public ObjectName getSharedStateService();

  /**
   * Set the name of the HAILSharedState MBean .
   *
   * @param v  assign the name of the MBean HANotificationBroadcaster.
   */
  public void setSharedStateService(ObjectName sharedStateService);

  /**
   * 
   * @return true if the node that this MBean is registered with
   * is the master node for the singleton service
   * 
   */
  public boolean isMasterNode();
  
  /**
   * 
   * Invoked when this mbean is elected to run the singleton service,
   * or in other words when this node is elected for master.
   *
   */
  public void startSingleton();
  
  /**
   * 
   * Invoked when this mbean is elected to no longer run the singleton service,
   * or in other words when this node is elected for slave.
   * 
   */
  public void stopSingleton();

  /** Get the javax.net.SocketFactory implementation class to use on the
   *client.
   * @jmx:managed-attribute
   */
  public String getClientSocketFactory() throws AttributeNotFoundException, InstanceNotFoundException, MBeanException, ReflectionException;


  public InetAddress getServerAddress() throws UnknownHostException, AttributeNotFoundException, InstanceNotFoundException, MBeanException, ReflectionException;

  public Integer getServerPort() throws AttributeNotFoundException, InstanceNotFoundException, MBeanException, ReflectionException;

  /**
   * Gets the enableTcpNoDelay.
   * @return Returns a boolean
   *
   * @jmx:managed-attribute
   */
  public boolean getEnableTcpNoDelay() throws AttributeNotFoundException, InstanceNotFoundException, MBeanException, ReflectionException;

  /**
   * Gets the chunk size.
   * @return Returns an int
   *
   * @jmx:managed-attribute
   */
  public int getChunkSize() throws AttributeNotFoundException, InstanceNotFoundException, MBeanException, ReflectionException;

  /**
   * Gets the buffer size.
   * @return Returns an int
   *
   * @jmx:managed-attribute
   */
  public int getBufferSize() throws AttributeNotFoundException, InstanceNotFoundException, MBeanException, ReflectionException;
  
   
}
