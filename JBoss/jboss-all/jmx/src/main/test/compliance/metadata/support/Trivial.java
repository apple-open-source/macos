/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.metadata.support;

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

   public void setSomethingInvalid(String thing)
   {
      this.something = thing;
   }
   
   public String getSomethingInvalid(Object invalid)
   {
      return something;
   }
   
   public void setSomethingInvalid2(String thing)
   {
      this.something = thing;
   }
   
   public void getSomethingInvalid2()
   {
      //return this.something;
   }
   
   public void doOperation(String arg)
   {
   }
}
