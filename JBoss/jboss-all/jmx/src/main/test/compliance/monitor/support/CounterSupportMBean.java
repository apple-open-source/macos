/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.monitor.support;

public interface CounterSupportMBean
{
  public Number getValue();
  public void setValue(Number value);
  public Number getWrongNull();
  public String getWrongType();
  public Number getWrongException();
  public void setWriteOnly(Number value);
}
