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
public class Trivial implements TrivialMBean
{
   private String something = null;
   private boolean anAttribute = true;
   private boolean gmsWasInvoked = false;
   
   public void setSomething(String thing)
   {
      this.something = thing;
   }

   public String getSomething()
   {
      return something;
   }

   public void doOperation()
   {
   }
   
   public MBeanServer getMBeanServer() 
   {
      gmsWasInvoked = true;
      return null;
   }
   
   public boolean isGMSInvoked()
   {
      return gmsWasInvoked;
   }
}
