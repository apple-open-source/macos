/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.query.support;

public class BooleanTest implements BooleanTestMBean
{
   private Boolean bool;

   public BooleanTest()
   {
   }

   public BooleanTest(boolean bool)
   {
      this.bool = new Boolean(bool);
   }

   public BooleanTest(Boolean bool)
   {
      this.bool = bool;
   }

   public Boolean getBoolean()
   {
      return bool;
   }
}
