/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.loading.support;

/**
 * Goes into MyMBeans.jar
 */
public class Trivial implements TrivialMBean
{
   private String something = null;
   private boolean anAttribute = true;

   public void setSomething(String thing)
   {
      this.something = thing;
   }

   public String getSomething()
   {
      return something;
   }

   public void doOperation(String arg)
   {
   }
}
