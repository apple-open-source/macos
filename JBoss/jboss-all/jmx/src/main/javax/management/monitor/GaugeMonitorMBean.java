/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package javax.management.monitor;

/**
 * The gauge monitor service MBean interface. <p>
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1 $
 *
 */
public interface GaugeMonitorMBean
  extends MonitorMBean
{
  // Constants -----------------------------------------------------
  
  // Static --------------------------------------------------------
  
  // Public --------------------------------------------------------

  /**
   * Retrieves the derived gauge.
   *
   * @return the derived gauge.
   */
  public Number getDerivedGauge();

  /**
   * Retrieves the derived gauge timestamp.
   *
   * @return the derived gauge timestamp.
   */
  public long getDerivedGaugeTimeStamp();

  /**
   * Retrieves the difference mode flag.
   *
   * @return true when in difference mode, false otherwise.
   */
  public boolean getDifferenceMode();

  /**
   * Sets the difference mode flag.
   *
   * @return value pass true for difference mode, false otherwise.
   */
  public void setDifferenceMode(boolean value);

  /**
   * Retrieves the high notify on/off switch.
   *
   * @return true if high notifications occur, false otherwise.
   */
  public boolean getNotifyHigh();

  /**
   * Sets the high notify on/off switch.
   *
   * @param value pass true for high notifications, false otherwise.
   */
  public void setNotifyHigh(boolean value);

  /**
   * Retrieves the low notify on/off switch.
   *
   * @return true if low notifications occur, false otherwise.
   */
  public boolean getNotifyLow();

  /**
   * Sets the low notify on/off switch.
   *
   * @param value pass true for low notifications, false otherwise.
   */
  public void setNotifyLow(boolean value);

  /**
   * Retrieves the high threshold.
   * REVIEW: zero threshold
   *
   * @return the high threshold value, zero means no threshold.
   */
  public Number getHighThreshold();

  /**
   * Retrieves the low threshold.
   * REVIEW: zero threshold
   *
   * @return the low threshold value, zero means no threshold.
   */
  public Number getLowThreshold();

  /**
   * Sets the high and low threshold.
   * REVIEW: zero threshold
   *
   * @param highValue the high threshold value, pass zero for no high 
   *        threshold.
   * @param lowValue the low threshold value, pass zero for no low
   *        threshold.
   * @exception IllegalArgumentException when either threshold is null or 
   *        the high threshold is lower than the low threshold or the.
   *        thresholds have different types.
   */
  public void setThresholds(Number highValue, Number lowValue)
    throws IllegalArgumentException;
}