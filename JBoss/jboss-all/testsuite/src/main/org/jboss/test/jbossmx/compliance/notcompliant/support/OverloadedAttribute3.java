/*
 * User: squitr
 * Date: Jan 18, 2002
 * Time: 1:52:22 PM
 */
package org.jboss.test.jbossmx.compliance.notcompliant.support;

public class OverloadedAttribute3 implements OverloadedAttribute3MBean
{
   public void setSomething(boolean something)
   {
   }

   public void setSomething(Boolean something)
   {
   }
}
