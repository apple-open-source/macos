/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.compliance.monitor.support;

public class CounterSupport 
  implements CounterSupportMBean
{
  private int value;

  public int getValue()
  {
    return value;
  }

  public void setValue(int value)
  {
     this.value = value;
  }
}
