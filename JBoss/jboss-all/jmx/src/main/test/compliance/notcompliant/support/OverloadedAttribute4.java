/*
 * User: squitr
 * Date: Jan 18, 2002
 * Time: 1:52:22 PM
 */
package test.compliance.notcompliant.support;

public class OverloadedAttribute4 implements OverloadedAttribute4MBean
{
   public void setSomething(boolean something)
   {
   }

   public Boolean isSomething()
   {
      return null;
   }

   public Boolean getSomething()
   {
      return null;
   }
}
