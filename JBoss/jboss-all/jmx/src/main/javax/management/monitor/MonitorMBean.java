/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package javax.management.monitor;

import javax.management.ObjectName;

/**
 * The monitor service MBean interface. <p>
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1 $
 *
 */
public interface MonitorMBean
{
  // Constants -----------------------------------------------------
  
  // Static --------------------------------------------------------
  
  // Public --------------------------------------------------------

  /**
   * Retrieves the granularity period in milliseconds.<p>
   *
   * The monitoring takes place once per granularity period.
   *
   * @return the granularity period.
   */
  public long getGranularityPeriod();

  /**
   * Retrieves the name of the attribute monitored.
   *
   * @return the attribute monitored.
   */
  public String getObservedAttribute();

  /**
   * Retrieves the object name of the MBean monitored.
   *
   * @return the object name.
   */
  public ObjectName getObservedObject();

  /**
   * Tests whether this monitoring service is active.
   *
   * @return true when the service is active, false otherwise.
   */
  public boolean isActive();

  /**
   * Sets the granularity period in milliseconds.<p>
   *
   * The monitoring takes place once per granularity period.<p>
   *
   * The default value is 10 seconds.
   *
   * @param period the granularity period.
   * @exception IllegalArgumentException when the period is not positive.
   */
  public void setGranularityPeriod(long period)
    throws IllegalArgumentException;

  /**
   * Sets the name of the attribute monitored.<p>
   *
   * The default value is null.
   *
   * @param attribute the attribute monitored.
   * @exception IllegalArgumentException when the period is not positive.
   */
  public void setObservedAttribute(String attribute);

  /**
   * Sets the object name of the MBean monitored.<p>
   *
   * The default value is null.
   *
   * @param object the object name.
   */
  public void setObservedObject(ObjectName object);

  /**
   * Starts the monitor.
   */
  public void start();

  /**
   * Stops the monitor.
   */
  public void stop();
}
