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

package org.jboss.ha.singleton;

/**
 * 
 * Basic interface for clustered singleton services 
 * 
 * @author  Ivelin Ivanov <ivelin@apache.org>
 *
 */
public interface HASingleton
{


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


}
