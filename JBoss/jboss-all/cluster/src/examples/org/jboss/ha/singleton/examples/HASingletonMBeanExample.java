/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.singleton.examples;

import org.jboss.logging.Logger;

/**
 * <p>
 * An sample singleton MBean.
 * </p>
 * 
 * @author  Ivelin Ivanov <ivelin@apache.org>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.3 $
 */
public class HASingletonMBeanExample
   implements HASingletonMBeanExampleMBean
{
   private static Logger log = Logger.getLogger(HASingletonMBeanExample.class);
   private boolean isMasterNode = false;

   public void startSingleton()
   {
      isMasterNode = true;
      log.info("Notified to start as singleton");
   }

   public boolean isMasterNode()
   {
      return isMasterNode;
   }

  public void stopSingleton( String gracefulShutdown )
   {
      isMasterNode = false;
      log.info("Notified to stop as singleton with argument: " + gracefulShutdown);
   }

}
