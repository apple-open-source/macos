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
public class AnotherTrivial implements AnotherTrivialMBean
{
   private AClass something = null;
   private boolean anAttribute = true;

   public void setSomething(AClass thing)
   {
      this.something = thing;
   }

   public AClass getSomething()
   {
      return something;
   }

   public void doOperation(String arg)
   {
   }
}
