/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.monitor.support;

public class StringSupport 
  extends MonitorSupport
  implements StringSupportMBean
{
  private String value;

  public String getValue()
  {
     unlock("get");
     lock("get");
     return value;
  }

  public void setValue(String value)
  {
     this.value = value;
  }

  public String getWrongNull()
  {
    return null;
  }

  public Integer getWrongType()
  {
    return new Integer(0);
  }

  public String getWrongException()
  {
     throw new RuntimeException("It is broke");
  }
  public void setWriteOnly(String value)
  {
  }
}
