/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mq.il.ha;

import java.net.InetAddress;

import org.jboss.ha.jmx.HAServiceMBean;

/**
 * 
 * @author  Ivelin Ivanov <ivelin@apache.org>
 *
 */
public interface HAILSharedStateMBean extends HAServiceMBean
{
  /**
   * Get the address of the singleton UIL Server host (which connects to the current JMS Server) 
   *
   * @jmx:managed-attribute
   */
  public InetAddress getServerAddress();
  
  /**
   * Set the address of the singleton UIL Server host (which connects to the current JMS Server) 
   *
   * @jmx:managed-attribute
   */
  public void setServerAddress(InetAddress address) throws Exception;

  /**
   * Get the address of the singleton UIL Server host (which connects to the current JMS Server) 
   *
   * @jmx:managed-attribute
   */
  public Integer getServerPort();

  /**
   * Set the address of the singleton UIL Server host (which connects to the current JMS Server) 
   *
   * @jmx:managed-attribute
   */
  public void setServerPort(Integer port) throws Exception;
}