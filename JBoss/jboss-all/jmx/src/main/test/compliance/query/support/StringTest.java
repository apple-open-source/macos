/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.query.support;

public class StringTest implements StringTestMBean
{
   private String string = "";

   public StringTest()
   {
   }

   public StringTest(String string)
   {
      this.string = string;
   }

   public String getString()
   {
      return string;
   }
}
