/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.implementation.util.support;

/**
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.2 $
 */
public class Trivial2 implements Trivial2MBean
{
   private String something = null;
   private boolean isOperationInvoked = false;
   
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
      isOperationInvoked = true;
   }
   
   public boolean isOperationInvoked()
   {
      return isOperationInvoked;
   }
   
   public void reset()
   {
      something = null;
      isOperationInvoked = false;
   }
   
}
