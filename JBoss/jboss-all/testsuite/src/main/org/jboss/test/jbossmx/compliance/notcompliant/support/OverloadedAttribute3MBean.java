/*
 * User: squitr
 * Date: Jan 18, 2002
 * Time: 1:52:35 PM
 */
package org.jboss.test.jbossmx.compliance.notcompliant.support;

public interface OverloadedAttribute3MBean
{
   void setSomething(boolean something);
   void setSomething(Boolean something);
}
