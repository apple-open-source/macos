/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package javax.management.monitor;

import javax.management.MBeanAttributeInfo;
import javax.management.MBeanNotificationInfo;

// REVIEW: Check synchronization

/**
 * The string monitor service.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1 $
 *
 */
public class StringMonitor
  extends Monitor
  implements StringMonitorMBean
{
  // Constants -----------------------------------------------------

  /**
   * The string match has been notified.
   */
  int STRING_MATCHES_NOTIFIED = 16;

  /**
   * The string differs has been notified.
   */
  int STRING_DIFFERS_NOTIFIED = 32;

  // Attributes ----------------------------------------------------

  /**
   * The derived gauge.
   */
  private String derivedGauge = new String();

  /**
   * The derived gauge timeStamp.
   */
  private long derivedGaugeTimeStamp = 0;

  /**
   * The comparison string.
   */
  String stringToCompare = new String();

  /**
   * Notify Matches.
   */
  boolean notifyMatch = false;

  /**
   * Notify Differs.
   */
  boolean notifyDiffer = false;

  // Static --------------------------------------------------------

  // Constructors --------------------------------------------------

  /**
   * Default Constructor
   */
  public StringMonitor()
  {
    dbgTag = "StringMonitor";
  }

  // Public --------------------------------------------------------

  public MBeanNotificationInfo[] getNotificationInfo()
  {
    MBeanNotificationInfo[] result = new MBeanNotificationInfo[1];
    String[] types = new String[]
    {
      MonitorNotification.RUNTIME_ERROR,
      MonitorNotification.OBSERVED_OBJECT_ERROR,
      MonitorNotification.OBSERVED_ATTRIBUTE_ERROR,
      MonitorNotification.OBSERVED_ATTRIBUTE_TYPE_ERROR,
      MonitorNotification.STRING_TO_COMPARE_VALUE_MATCHED,
      MonitorNotification.STRING_TO_COMPARE_VALUE_DIFFERED
    };
    result[0] = new MBeanNotificationInfo(types,
      "javax.management.monitor.MonitorNotification",
      "Notifications sent by the String Monitor Service MBean");
    return result;
  }

  // StringMonitorMBean implementation -----------------------------

  public String getDerivedGauge()
  {
    return derivedGauge;
  }

  public long getDerivedGaugeTimeStamp()
  {
    return derivedGaugeTimeStamp;
  }

  public String getStringToCompare()
  {
    return stringToCompare;
  }

  public void setStringToCompare(String value)
    throws IllegalArgumentException
  {
    if (value == null)
      throw new IllegalArgumentException("Null string to compare.");
    this.stringToCompare = value;
  }

  public boolean getNotifyMatch()
  {
    return notifyMatch;
  }

  public void setNotifyMatch(boolean value)
  {
    notifyMatch = value;
  }

  public boolean getNotifyDiffer()
  {
    return notifyDiffer;
  }

  public void setNotifyDiffer(boolean value)
  {
    notifyDiffer = value;
  }

  // Override start to set initial gauge.
  public synchronized void start()
  {
    derivedGauge = new String();
    derivedGaugeTimeStamp = System.currentTimeMillis();
    super.start();
  }
  // Package protected ---------------------------------------------

  void monitor(MBeanAttributeInfo attributeInfo, Object value)
    throws Exception
  {
    // Must be a string.
    if (!(value instanceof String))
    {
      sendAttributeTypeErrorNotification("Not a string attribute");
      return;
    }

    // Get the gauge and record when we got it.
    derivedGauge = (String) value;
    derivedGaugeTimeStamp = System.currentTimeMillis();

    // Try to fire the relevant event.
    if (value.equals(stringToCompare))
      sendStringMatchesNotification((String) value);
    else
      sendStringDiffersNotification((String) value);
  }

  /**
   * Send an string matches notification.<p>
   *
   * This is only performed when requested and it has not already been sent.
   *
   * @param value the attribute value.
   */
  void sendStringMatchesNotification(String value)
  {
    if (notifyMatch && ((alreadyNotified & STRING_MATCHES_NOTIFIED) == 0))
    {
      sendNotification(MonitorNotification.STRING_TO_COMPARE_VALUE_MATCHED,
        derivedGaugeTimeStamp, "matches", observedAttribute, value, 
        stringToCompare);
      alreadyNotified |= STRING_MATCHES_NOTIFIED;
    }
    alreadyNotified &= ~STRING_DIFFERS_NOTIFIED;
  }

  /**
   * Send an string differs notification.<p>
   *
   * This is only performed when requested and it has not already been sent.
   *
   * @param value the attribute value.
   */
  void sendStringDiffersNotification(String value)
  {
    if (notifyDiffer && ((alreadyNotified & STRING_DIFFERS_NOTIFIED) == 0))
    {
      sendNotification(MonitorNotification.STRING_TO_COMPARE_VALUE_DIFFERED,
        derivedGaugeTimeStamp, "differs", observedAttribute, value, 
        stringToCompare);
      alreadyNotified |= STRING_DIFFERS_NOTIFIED;
    }
    alreadyNotified &= ~STRING_MATCHES_NOTIFIED;
  }

  // Protected -----------------------------------------------------

  // Private -------------------------------------------------------

  // Inner classes -------------------------------------------------
}
