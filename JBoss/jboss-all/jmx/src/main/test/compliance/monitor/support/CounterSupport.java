/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.monitor.support;

public class CounterSupport 
  extends MonitorSupport
  implements CounterSupportMBean
{
  private Number value;

  public Number getValue()
  {
     unlock("get");
     lock("get");
     return value;
  }

  public void setValue(Number value)
  {
     this.value = value;
  }

  public Number getWrongNull()
  {
    return null;
  }

  public String getWrongType()
  {
    return "Wrong";
  }

  public Number getWrongException()
  {
     throw new RuntimeException("It is broke");
  }
  public void setWriteOnly(Number value)
  {
  }
}
