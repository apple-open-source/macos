/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package javax.management.monitor;

/**
 * The counter monitor service MBean interface. <p>
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1 $
 *
 */
public interface CounterMonitorMBean
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
   * Retrieves the modulus.
   *
   * @return the modulus value, zero means no modulus.
   */
  public Number getModulus();

  /**
   * Sets the modulus.
   *
   * @param value the modulus value, pass zero for no modulus.
   * @exception IllegalArgumentException when the modulus is null or 
   *        less than zero.
   */
  public void setModulus(Number value)
    throws IllegalArgumentException;

  /**
   * Retrieves the notify on/off switch.
   *
   * @return true if notifications occur, false otherwise.
   */
  public boolean getNotify();

  /**
   * Sets the notify on/off switch.
   *
   * @param value pass true notifications, false otherwise.
   */
  public void setNotify(boolean value);

  /**
   * Retrieves the offset.
   *
   * @return the offset value, zero means no offset.
   */
  public Number getOffset();

  /**
   * Sets the offset.
   *
   * @param value the offset value, pass zero for no offset.
   * @exception IllegalArgumentException when the offset is null or 
   *        less than zero.
   */
  public void setOffset(Number value)
    throws IllegalArgumentException;

  /**
   * Retrieves the threshold.
   * REVIEW: zero threshold
   *
   * @return the threshold value, zero means no threshold.
   */
  public Number getThreshold();

  /**
   * Sets the threshold.
   * REVIEW: zero threshold
   *
   * @param value the threshold value, pass zero for no threshold.
   * @exception IllegalArgumentException when the threshold is null or 
   *        less than zero.
   */
  public void setThreshold(Number value)
    throws IllegalArgumentException;
}