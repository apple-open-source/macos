/*
 * User: squitr
 * Date: Jan 18, 2002
 * Time: 1:52:22 PM
 */
package org.jboss.test.jbossmx.compliance.notcompliant.support;

public class OverloadedAttribute1 implements OverloadedAttribute1MBean
{
   public void setSomething(boolean something)
   {
   }

   public Boolean getSomething()
   {
      return null;
   }
}
