/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.compliance.monitor.support;

public class StringSupport 
  implements StringSupportMBean
{
  private String value;

  public String getValue()
  {
    return value;
  }

  public void setValue(String value)
  {
     this.value = value;
  }
}
