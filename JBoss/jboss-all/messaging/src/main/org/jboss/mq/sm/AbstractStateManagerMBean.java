/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.sm;

import org.jboss.system.ServiceMBean;
/**
 * AbstractStateManagerMBean.java
 *
 * @author     <a href="pra@tim.se">Peter Antman</a>
 * @version $Revision: 1.1 $
 */

public interface AbstractStateManagerMBean
   extends ServiceMBean
{
   /**
    * Get an instance if the StateManager (Singleton).
    */
   StateManager getInstance();
   
} // AbstractStateManagerMBean
