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
 * The gauge monitor service.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.2 $
 *
 */
public class GaugeMonitor
  extends Monitor
  implements GaugeMonitorMBean
{
  // Constants -----------------------------------------------------

  /**
   * The gauge high threshold exceeded has been notified.
   */
  int THRESHOLD_HIGH_EXCEEDED_NOTIFIED = 16;

  /**
   * The gauge low threshold exceeded has been notified.
   */
  int THRESHOLD_LOW_EXCEEDED_NOTIFIED = 32;

  /**
   * The threshold type error has been notified.
   */
  int THRESHOLD_ERROR_NOTIFIED = 64;

  // Attributes ----------------------------------------------------

  /**
   * The derived gauge.
   */
  private Number derivedGauge = new Integer(0);

  /**
   * The last value.
   */
  private Number lastValue = null;

  /**
   * The derived gauge timeStamp.
   */
  private long derivedGaugeTimeStamp = 0;

  /**
   * Difference mode.
   */
  boolean differenceMode = false;

  /**
   * The high threshold.
   */
  Number highThreshold = new Integer(0);

  /**
   * The low threshold.
   */
  Number lowThreshold = new Integer(0);

  /**
   * High Notify.
   */
  boolean notifyHigh = false;

  /**
   * Low Notify.
   */
  boolean notifyLow = false;

  // Static --------------------------------------------------------

  // Constructors --------------------------------------------------

  /**
   * Default Constructor
   */
  public GaugeMonitor()
  {
    dbgTag = "GaugeMonitor";
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
      MonitorNotification.THRESHOLD_ERROR,
      MonitorNotification.THRESHOLD_HIGH_VALUE_EXCEEDED,
      MonitorNotification.THRESHOLD_LOW_VALUE_EXCEEDED
    };
    result[0] = new MBeanNotificationInfo(types,
      "javax.management.monitor.MonitorNotification",
      "Notifications sent by the Gauge Monitor Service MBean");
    return result;
  }

  // GaugeMonitorMBean implementation ------------------------------

  public Number getDerivedGauge()
  {
    return derivedGauge;
  }

  public long getDerivedGaugeTimeStamp()
  {
    return derivedGaugeTimeStamp;
  }

  public boolean getDifferenceMode()
  {
    return differenceMode;
  }

  public void setDifferenceMode(boolean value)
  {
    differenceMode = value;
  }

  public boolean getNotifyHigh()
  {
    return notifyHigh;
  }

  public void setNotifyHigh(boolean value)
  {
    notifyHigh = value;
  }

  public boolean getNotifyLow()
  {
    return notifyLow;
  }

  public void setNotifyLow(boolean value)
  {
    notifyLow = value;
  }

  public Number getHighThreshold()
  {
    return highThreshold;
  }

  public Number getLowThreshold()
  {
    return lowThreshold;
  }

  public void setThresholds(Number highValue, Number lowValue)
    throws IllegalArgumentException
  {
    if (highValue == null)
      throw new IllegalArgumentException("Null high threshold");
    if (lowValue == null)
      throw new IllegalArgumentException("Null low threshold");
    if (highValue.getClass() != lowValue.getClass())
      throw new IllegalArgumentException("High and low different types");
    if (highValue.doubleValue() < lowValue.doubleValue())
      throw new IllegalArgumentException("High less than low threshold");
    highThreshold = highValue;
    lowThreshold = lowValue;
    alreadyNotified = RESET_FLAGS_ALREADY_NOTIFIED;
  }

  // Override start to reset the last value for difference mode and
  // to get the initial gauge.
  public synchronized void start()
  {
    lastValue = null;
    derivedGauge = new Integer(0);
    derivedGaugeTimeStamp = System.currentTimeMillis();
    super.start();
  }

  // Package protected ---------------------------------------------

  // REVIEW: This works but needs tidying up!
  void monitor(MBeanAttributeInfo attributeInfo, Object value)
    throws Exception
  {
    // Wrong type of attribute
    if (!(value instanceof Number))
    {
       sendAttributeTypeErrorNotification("Attribute is not a number");
       return;
    }

    // Wrong threshold types
    if (highThreshold.getClass() != value.getClass()
        || lowThreshold.getClass() != value.getClass())
    {
       sendThresholdErrorNotification(value);
       return;
    }

    // Cast the gauge to a Number
    Number number = (Number) value;

    // Get the gauge and record when we got it.
    if (differenceMode)
    {
      if (lastValue == null)
        derivedGauge = getZero(number);
      else
        derivedGauge = sub(number, lastValue);
    }
    else
      derivedGauge = number;
    derivedGaugeTimeStamp = System.currentTimeMillis();

    // Fire the event if the low threshold has been exceeded
    if (derivedGauge.doubleValue() <= lowThreshold.doubleValue())
    {
      if ((alreadyNotified & THRESHOLD_LOW_EXCEEDED_NOTIFIED) == 0)
      {
        // Reset high threshold
        alreadyNotified &= ~THRESHOLD_HIGH_EXCEEDED_NOTIFIED;

        // Send the notification once
        sendThresholdLowExceededNotification(derivedGauge);
        alreadyNotified |= THRESHOLD_LOW_EXCEEDED_NOTIFIED;
      }
    }

    // Fire the event if the high threshold has been exceeded
    if (derivedGauge.doubleValue() >= highThreshold.doubleValue())
    {

      if ((alreadyNotified & THRESHOLD_HIGH_EXCEEDED_NOTIFIED) == 0)
      {
        // Reset low threshold
        alreadyNotified &= ~THRESHOLD_LOW_EXCEEDED_NOTIFIED;

        // Send the notification once
        sendThresholdHighExceededNotification(derivedGauge);
        alreadyNotified |= THRESHOLD_HIGH_EXCEEDED_NOTIFIED;
      }
    }

    // Remember the last value
    lastValue = number;
  }

  /**
   * Get zero for the type passed.
   * 
   * @param the reference object
   * @return zero for the correct type
   */
  Number getZero(Number value)
  {
     if (value instanceof Byte)
       return new Byte((byte) 0);
     if (value instanceof Integer)
       return new Integer(0);
     if (value instanceof Long)
       return new Long(0);
     if (value instanceof Short)
       return new Short((short) 0);
     if (value instanceof Float)
       return new Float((float) 0);
     return new Double((double) 0);
  }

  /**
   * Subtract two numbers.
   * @param value1 the first value.
   * @param value2 the second value.
   * @return value1 - value2 of the correct type
   */
  Number sub(Number value1, Number value2)
  {
     if (value1 instanceof Byte)
       return new Byte((byte) (value1.byteValue() - value2.byteValue()));
     if (value1 instanceof Integer)
       return new Integer(value1.intValue() - value2.intValue());
     if (value1 instanceof Long)
       return new Long((value1.longValue() - value2.longValue()));
     if (value1 instanceof Short)
       return new Short((short) (value1.shortValue() - value2.shortValue()));
     if (value1 instanceof Float)
       return new Float((value1.floatValue() - value2.floatValue()));
     return new Double(value1.doubleValue() - value2.doubleValue());
  }

  /**
   * Send a threshold low exceeded event.<p>
   *
   * This is only performed when requested and it has not already been sent.
   *
   * @param value the attribute value.
   */
  void sendThresholdLowExceededNotification(Object value)
  {
    if (notifyLow)
    {
      sendNotification(MonitorNotification.THRESHOLD_LOW_VALUE_EXCEEDED,
        derivedGaugeTimeStamp, "low threshold exceeded", observedAttribute, 
        value, lowThreshold);
    }
  }

  /**
   * Send a high threshold exceeded event.<p>
   *
   * This is only performed when requested and it has not already been sent.
   *
   * @param value the attribute value.
   */
  void sendThresholdHighExceededNotification(Object value)
  {
    if (notifyHigh)
    {
      sendNotification(MonitorNotification.THRESHOLD_HIGH_VALUE_EXCEEDED,
        derivedGaugeTimeStamp, "high threshold exceeded", observedAttribute, 
        value, highThreshold);
    }
  }

  /**
   * Send a threshold error event.<p>
   *
   * This is only performed when requested and it has not already been sent.
   *
   * @param value the attribute value.
   */
  void sendThresholdErrorNotification(Object value)
  {
    if ((alreadyNotified & THRESHOLD_ERROR_NOTIFIED) == 0)
    {
      sendNotification(MonitorNotification.THRESHOLD_ERROR,
        derivedGaugeTimeStamp, 
        "High or Low Threshold not the correct type", 
        observedAttribute, null, null);
      alreadyNotified |= THRESHOLD_ERROR_NOTIFIED;
    }
  }

  // Protected -----------------------------------------------------

  // Private -------------------------------------------------------

  // Inner classes -------------------------------------------------
}
