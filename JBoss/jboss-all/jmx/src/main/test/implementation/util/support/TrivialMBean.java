/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.implementation.util.support;

import javax.management.MBeanServer;

/**
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.2 $
 */
public interface TrivialMBean
{
   void setSomething(String thing);

   String getSomething();

   void doOperation();
   
   MBeanServer getMBeanServer();
   
   boolean isGMSInvoked();
}
