/*
 * User: squitr
 * Date: Jan 18, 2002
 * Time: 1:52:22 PM
 */
package test.compliance.notcompliant.support;

public class OverloadedAttribute2 implements OverloadedAttribute2MBean
{
   public void setSomething(boolean something)
   {
   }

   public Boolean isSomething()
   {
      return null;
   }
}
