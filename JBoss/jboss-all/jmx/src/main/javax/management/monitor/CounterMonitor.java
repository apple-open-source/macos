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
 * The counter monitor service.
 *
 * <p><b>Revisions:</b>
 * <p><b>20020319 Adrian Brock:</b>
 * <ul>
 * <li>Reset the threshold when the value becomes negative in difference mode
 * </ul>
 * <p><b>20020326 Adrian Brock:</b>
 * <ul>
 * <li>The spec says the modulus should be *strictly* exceeded. It appears
 * from testing the RI, it is a mathematical definition of modulus. e.g.
 * 10 exceeds a modulus of 10
 * </ul>
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.3 $
 *
 */
public class CounterMonitor
  extends Monitor
  implements CounterMonitorMBean
{
  // Constants -----------------------------------------------------

  /**
   * The counter threshold exceeded has been notified.
   */
  int THRESHOLD_EXCEEDED_NOTIFIED = 16;

  /**
   * The threshold type error has been notified.
   */
  int THRESHOLD_ERROR_NOTIFIED = 32;

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
   * The offset.
   */
  Number offset = new Integer(0);

  /**
   * The modulus.
   */
  Number modulus = new Integer(0);

  /**
   * The threshold.
   */
  Number threshold = new Integer(0);

  /**
   * The last stated threshold.
   */
  Number initialThreshold = new Integer(0);

  /**
   * Difference mode.
   */
  boolean differenceMode = false;

  /**
   * Notify.
   */
  boolean notify = false;

  // Static --------------------------------------------------------

  // Constructors --------------------------------------------------

  /**
   * Default Constructor
   */
  public CounterMonitor()
  {
    dbgTag = "CounterMonitor";
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
      MonitorNotification.THRESHOLD_VALUE_EXCEEDED
    };
    result[0] = new MBeanNotificationInfo(types,
      "javax.management.monitor.MonitorNotification",
      "Notifications sent by the Counter Monitor Service MBean");
    return result;
  }

  // CounterMonitorMBean implementation ----------------------------

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

  public Number getModulus()
  {
    return modulus;
  }

  public void setModulus(Number value)
    throws IllegalArgumentException
  {
    if (value == null)
      throw new IllegalArgumentException("Null modulus");
    if (value.longValue() < 0)
      throw new IllegalArgumentException("Negative modulus");
    modulus = value;
    alreadyNotified = RESET_FLAGS_ALREADY_NOTIFIED;
  }

  public boolean getNotify()
  {
    return notify;
  }

  public void setNotify(boolean value)
  {
    notify = value;
  }

  public Number getOffset()
  {
    return offset;
  }

  public void setOffset(Number value)
    throws IllegalArgumentException
  {
    if (value == null)
      throw new IllegalArgumentException("Null offset");
    if (value.longValue() < 0)
      throw new IllegalArgumentException("Negative offset");
    offset = value;
    alreadyNotified = RESET_FLAGS_ALREADY_NOTIFIED;
  }

  public Number getThreshold()
  {
    return threshold;
  }

  public void setThreshold(Number value)
    throws IllegalArgumentException
  {
    if (value == null)
      throw new IllegalArgumentException("Null threshold");
    if (value.longValue() < 0)
      throw new IllegalArgumentException("Negative threshold");
    threshold = value;
    initialThreshold = value;
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
    if (!(value instanceof Byte) && !(value instanceof Integer) &&
        !(value instanceof Short) && !(value instanceof Long))
    {
       sendAttributeTypeErrorNotification("Attribute is not an integer type");
       return;
    }

    // Wrong threshold types
    if (threshold.getClass() != value.getClass()
        || offset.longValue() != 0 && offset.getClass() != value.getClass()
        || modulus.longValue() != 0 && modulus.getClass() != value.getClass()) 
    {
       sendThresholdErrorNotification(value);
       return;
    }

    // Cast the counter to a Number
    Number number = (Number) value;

    // Get the gauge and record when we got it.
    if (differenceMode)
    {
      if (lastValue == null)
        derivedGauge = getZero(number);
      else
        derivedGauge = sub(number, lastValue);
      if (derivedGauge.longValue() < 0 && modulus.longValue() != 0)
        derivedGauge = add(derivedGauge, modulus);
    }
    else
      derivedGauge = number;
    derivedGaugeTimeStamp = System.currentTimeMillis();

    // Fire the event if the threshold has been exceeded
    if (derivedGauge.longValue() >= threshold.longValue())
    {
      if ((alreadyNotified & THRESHOLD_EXCEEDED_NOTIFIED) == 0)
      {
        sendThresholdExceededNotification(derivedGauge);
        alreadyNotified |= THRESHOLD_EXCEEDED_NOTIFIED;

        // Add any offsets required to get a new threshold
        if (offset.longValue() != 0)
        {
          while(threshold.longValue() <= derivedGauge.longValue())
            threshold = add(threshold, offset);
          alreadyNotified &= ~THRESHOLD_EXCEEDED_NOTIFIED;
        }
      }
    }
    else
    {
      // Reset notfication when it becomes less than threshold
      if (derivedGauge.longValue() < threshold.longValue() 
          && offset.longValue() == 0)
        alreadyNotified &= ~THRESHOLD_EXCEEDED_NOTIFIED;
    }

    // For difference mode, restart when the counter decreases
    if (differenceMode == true && lastValue !=null && 
        lastValue.longValue() > number.longValue())
    {
      threshold = initialThreshold;
      alreadyNotified &= ~THRESHOLD_EXCEEDED_NOTIFIED;
    }

    // For normal mode, restart when modulus exceeded
    if (differenceMode == false && modulus.longValue() != 0 &&
        number.longValue() >= modulus.longValue())
    {
      threshold = initialThreshold;
      alreadyNotified &= ~THRESHOLD_EXCEEDED_NOTIFIED;
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
     if (value instanceof Short)
       return new Short((short) 0);
     return new Long(0);
  }

  /**
   * Add two numbers together.
   * @param value1 the first value.
   * @param value2 the second value.
   * @return value1 + value2 of the correct type
   */
  Number add(Number value1, Number value2)
  {
     if (value1 instanceof Byte)
       return new Byte((byte) (value1.byteValue() + value2.byteValue()));
     if (value1 instanceof Integer)
       return new Integer(value1.intValue() + value2.intValue());
     if (value1 instanceof Short)
       return new Short((short) (value1.shortValue() + value2.shortValue()));
     return new Long(value1.longValue() + value2.longValue());
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
     if (value1 instanceof Short)
       return new Short((short) (value1.shortValue() - value2.shortValue()));
     return new Long(value1.longValue() - value2.longValue());
  }

  /**
   * Send a threshold exceeded event.<p>
   *
   * This is only performed when requested and it has not already been sent.
   *
   * @param value the attribute value.
   */
  void sendThresholdExceededNotification(Object value)
  {
    if (notify)
    {
      sendNotification(MonitorNotification.THRESHOLD_VALUE_EXCEEDED,
        derivedGaugeTimeStamp, "threshold exceeded", observedAttribute, value, 
        threshold);
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
        "Threshold, offset or modulus not the correct type", 
        observedAttribute, null, null);
      alreadyNotified |= THRESHOLD_ERROR_NOTIFIED;
    }
  }

  // Protected -----------------------------------------------------

  // Private -------------------------------------------------------

  // Inner classes -------------------------------------------------
}
