/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.singleton;

/**
 * 
 * Basic interface for clustered singleton services 
 * 
 * @author  Ivelin Ivanov <ivelin@apache.org>
 * @version $Revision: 1.1.2.3 $
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
