/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package javax.management.monitor;

/**
 * The string monitor service MBean interface. <p>
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1 $
 *
 */
public interface StringMonitorMBean
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
  public String getDerivedGauge();

  /**
   * Retrieves the derived gauge timestamp.
   *
   * @return the derived gauge timestamp.
   */
  public long getDerivedGaugeTimeStamp();

  /**
   * Retrieves the string to compare with the observed attribute.
   *
   * @return the comparison string.
   */
  public String getStringToCompare();

  /**
   * Sets the string to compare with the observed attribute.
   *
   * @param value the comparison string.
   * @exception IllegalArgumentException when specified string is null.
   */
  public void setStringToCompare(String value)
    throws IllegalArgumentException;

  /**
   * Retrieves the matching on/off switch.
   *
   * @return true if the notification occurs when the string matches, false
   *         otherwise.
   */
  public boolean getNotifyMatch();

  /**
   * Sets the matching on/off switch.
   *
   * @param value pass true for a notification when the string matches, false
   *        otherwise.
   */
  public void setNotifyMatch(boolean value);

  /**
   * Retrieves the differs on/off switch.
   *
   * @return true if the notification occurs when the string differs, false
   *         otherwise.
   */
  public boolean getNotifyDiffer();

  /**
   * Sets the differs on/off switch.
   *
   * @param value pass true for a notification when the string differs, false
   *        otherwise.
   */
  public void setNotifyDiffer(boolean value);
}