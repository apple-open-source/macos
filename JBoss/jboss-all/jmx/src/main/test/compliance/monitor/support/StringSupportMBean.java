/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.monitor.support;

public interface StringSupportMBean
{
  public String getValue();
  public void setValue(String value);
  public String getWrongNull();
  public Integer getWrongType();
  public String getWrongException();
  public void setWriteOnly(String value);
}
