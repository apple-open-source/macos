/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mq.il.ha;

import java.net.InetAddress;

import org.jboss.ha.jmx.HAServiceMBeanSupport;

/**
 * Stores the address and port 
 * of the current HA JMS singleton server.
 * Also used to broadcast a notification when the singleton moves to another node.
 * @author  Ivelin Ivanov <ivelin@apache.org>
 *
 */
public class HAILSharedState extends HAServiceMBeanSupport implements HAILSharedStateMBean
{

  /**
   * Get the address of the singleton UIL Server host (which connects to the current JMS Server) 
   *
   * @jmx:managed-attribute
   */
  public InetAddress getServerAddress()
  {
    InetAddress addr = (InetAddress)getDistributedState(SERVER_BIND_ADDRESS_KEY);
    return addr;
  }
  
  /**
   * Set the address of the singleton UIL Server host (which connects to the current JMS Server) 
   *
   * @jmx:managed-attribute
   */
  public void setServerAddress(InetAddress address) throws Exception
  {
    setDistributedState(SERVER_BIND_ADDRESS_KEY, address);
  }

  /**
   * Get the address of the singleton UIL Server host (which connects to the current JMS Server) 
   *
   * @jmx:managed-attribute
   */
  public Integer getServerPort()
  {
    Integer port = (Integer)getDistributedState(SERVER_BIND_PORT_KEY);
    return port;
  }
  
  /**
   * Set the address of the singleton UIL Server host (which connects to the current JMS Server) 
   *
   * @jmx:managed-attribute
   */
  public void setServerPort(Integer port) throws Exception
  {
    setDistributedState(SERVER_BIND_PORT_KEY, port);
  }


  private static String SERVER_BIND_ADDRESS_KEY = "uil.server.bind.address";
  private static String SERVER_BIND_PORT_KEY = "uil.server.bind.port";
  
}
