/*
 * User: squitr
 * Date: Jan 18, 2002
 * Time: 1:52:35 PM
 */
package test.compliance.notcompliant.support;

public interface OverloadedAttribute4MBean
{
   void setSomething(boolean something);
   Boolean isSomething();
   Boolean getSomething();
}
