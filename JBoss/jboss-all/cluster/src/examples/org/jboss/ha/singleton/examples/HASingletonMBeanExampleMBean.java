/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ha.singleton.examples;

/**
 * 
 * Sample Singleton MBean interface
 * 
 * @author  Ivelin Ivanov <ivelin@apache.org>
 *
 */
public interface HASingletonMBeanExampleMBean
{

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
   * @param String gracefulShutdown is an example argument passed on singleton stop
   *
   */
  public void stopSingleton( String gracefulShutdown );

  
}
