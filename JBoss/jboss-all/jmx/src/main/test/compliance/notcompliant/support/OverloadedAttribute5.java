/*
 * User: squitr
 * Date: Jan 18, 2002
 * Time: 1:52:22 PM
 */
package test.compliance.notcompliant.support;

public class OverloadedAttribute5 implements OverloadedAttribute5MBean
{
   public void setSomething(boolean something)
   {
   }

   public Boolean getSomething()
   {
      return null;
   }

   public Boolean isSomething()
   {
      return null;
   }
}
