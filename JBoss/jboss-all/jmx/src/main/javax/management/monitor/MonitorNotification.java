/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package javax.management.monitor;

import java.io.Serializable;

import javax.management.Notification;
import javax.management.ObjectName;

/**
 * A notification from one of the monitor services.<p>
 *
 * The notification occurs only when the state is first entered.<p>
 *
 * All monitor services produce the following notifications.
 * <ul>
 * <li> {@link #OBSERVED_OBJECT_ERROR} when the MBean is not registered.
 * </li>
 * <li> {@link #OBSERVED_ATTRIBUTE_ERROR} when the MBean's attribute does
 *      not exist.
 * </li>
 * <li> {@link #OBSERVED_ATTRIBUTE_TYPE_ERROR} when the MBean's attribute is
 *      not of the correct type for the monitor or the derived gauge value.
 * </li>
 * <li> {@link #RUNTIME_ERROR} for any other error.
 * </li>
 * </ul>
 * The counter monitor produces the following notifications.
 * <ul>
 * <li> {@link #THRESHOLD_ERROR} when one of the monitors threshold vaues is 
 * of an incorrect type.
 * </li>
 * <li> {@link #THRESHOLD_VALUE_EXCEEDED} when the counter exceeds the 
 * threshold.
 * </li>
 * </ul>
 * The gauge monitor produces the following notifications.
 * <ul>
 * <li> {@link #THRESHOLD_ERROR} when one of the monitors threshold vaues is 
 * of an incorrect type.
 * </li>
 * <li> {@link #THRESHOLD_HIGH_VALUE_EXCEEDED} when the attribute exceeds
 * the high threshold value.
 * </li>
 * <li> {@link #THRESHOLD_LOW_VALUE_EXCEEDED} when the attribute exceeds
 * the low threshold value.
 * </li>
 * </ul>
 * The string monitor produces the following notifications.
 * <ul>
 * <li> {@link #STRING_TO_COMPARE_VALUE_DIFFERED} when the attribute no longer
 * matches the specified value.
 * </li>
 * <li> {@link #STRING_TO_COMPARE_VALUE_DIFFERED} when the attribute matches 
 * the specified value.
 * </li>
 * </ul>
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.8.1 $
 *
 * <p><b>Revisions:</b>
 * <p><b>20020816 Adrian Brock:</b>
 * <ul>
 * <li> Serialization </li>
 * </ul>
 */
public class MonitorNotification
  extends Notification
  implements Serializable
{
  // Constants -----------------------------------------------------

   private static final long serialVersionUID = -4608189663661929204L;

  /**
   * Notification type when an MBean doesn't contain the specified
   * attribute. The observed object name and observed attribute name
   * are sent in the notification.
   */
  public static final String OBSERVED_ATTRIBUTE_ERROR = 
                             "jmx.monitor.error.attribute";

  /**
   * Notification type when an attribute is null or the attribute has
   * an incorrect type for the monitor service. The observed object name 
   * and observed attribute name are sent in the notification.
   */
  public static final String OBSERVED_ATTRIBUTE_TYPE_ERROR = 
                             "jmx.monitor.error.type";

  /**
   * Notification type when an MBean is not registered. The observed object 
   * name is sent in the notification.
   */
  public static final String OBSERVED_OBJECT_ERROR = 
                             "jmx.monitor.error.mbean";

  /**
   * Notification type for any other error.
   */
  public static final String RUNTIME_ERROR = 
                             "jmx.monitor.error.runtime";

  /**
   * Notification type when an attribute no longer matches the specified
   * value of a StringMonitor. The observed object name, observed attribute
   * name, derived gauge (actual value) and trigger (monitor value) are
   * sent in the notification.
   * REVIEW: Verify this.
   */
  public static final String STRING_TO_COMPARE_VALUE_DIFFERED = 
                             "jmx.monitor.string.differs";

  /**
   * Notification type when an attribute changes to match the specified
   * value of a StringMonitor. The observed object name, observed attribute
   * name, derived gauge (actual value) and trigger (monitor value) are
   * sent in the notification.
   * REVIEW: Verify this.
   */
  public static final String STRING_TO_COMPARE_VALUE_MATCHED = 
                             "jmx.monitor.string.matches";

  /**
   * Notification type when an attribute's threshold parameters (threshold,
   * low threshold, high threshold, offset or modules) are not of the
   * correct type. The observed object name and observed attribute
   * are sent in the notification.
   * REVIEW: Verify this.
   */
  public static final String THRESHOLD_ERROR = 
                             "jmx.monitor.error.threshold";
  /**
   * Notification type when a counter attribute changes to exceed the
   * specified threshold value. The observed object name, observed attribute
   * name, derived gauge (actual value) and trigger (threshold value) are
   * sent in the notification.
   * REVIEW: Verify this.
   */
  public static final String THRESHOLD_VALUE_EXCEEDED = 
                             "jmx.monitor.counter.threshold";

  /**
   * Notification type when a guage attribute changes to exceed the
   * specified threshold high value. The observed object name, 
   * observed attribute name, derived gauge (actual value) and 
   * trigger (threshold value) are sent in the notification.
   * REVIEW: Verify this.
   */
  public static final String THRESHOLD_HIGH_VALUE_EXCEEDED = 
                             "jmx.monitor.gauge.high";

  /**
   * Notification type when a gauge attribute changes to exceed the
   * specified threshold low value. The observed object name, 
   * observed attribute name, derived gauge (actual value) and 
   * trigger (threshold value) are sent in the notification.
   * REVIEW: Verify this.
   */
  public static final String THRESHOLD_LOW_VALUE_EXCEEDED = 
                             "jmx.monitor.gauge.low";
  
  // Attributes ----------------------------------------------------
  
  /**
   * The derived gauge.
   */
  private Object derivedGauge;
  
  /**
   * The observed attribute.
   */
  private String observedAttribute;
  
  /**
   * The observed object.
   */
  private ObjectName observedObject;
  
  /**
   * The trigger of the notification.
   */
  private Object trigger;

  // Static --------------------------------------------------------
  
  // Constructors --------------------------------------------------

  /**
   * Construct a new monitor notification.
   *
   * @param type the notification type.
   * @param source the notification source.
   * @param sequenceNumber the notification sequence within the source object.
   * @param timeStamp the time the notification was sent.
   * @param message the detailed message.
   * @param derivedGauge the actual value.
   * @param observedAttribute the monitored attribute.
   * @param observedObject the monitored MBean.
   * @param trigger the value monitor value.
   */
  /*package*/ MonitorNotification(String type, Object source, long sequenceNumber, 
               long timeStamp, String message, Object derivedGauge, 
               String observedAttribute, ObjectName observedObject,
               Object trigger)
  {
    super(type, source, sequenceNumber, timeStamp, message);
    this.derivedGauge = derivedGauge;
    this.observedAttribute = observedAttribute;
    this.observedObject = observedObject;
    this.trigger = trigger;
  }

  // Public --------------------------------------------------------

  /**
   * Retrieves the derived gauge. See each monitor service for
   * the definition of this value.
   *
   * @return the derived gauge.
   */
  public Object getDerivedGauge()
  {
    return derivedGauge;
  }

  /**
   * Retrieves the name of the attribute monitored.
   *
   * @return the name of the monitored attribute.
   */
  public String getObservedAttribute()
  {
    return observedAttribute;
  }

  /**
   * Retrieves the name of the MBean monitored.
   *
   * @return the registered object name.
   */
  public ObjectName getObservedObject()
  {
    return observedObject;
  }

  /**
   * Retrieves the trigger value of the notification. See each monitor
   * service for the values that trigger notifications.
   *
   * @return the trigger.
   */
  public Object getTrigger()
  {
    return trigger;
  }

  // X implementation ----------------------------------------------

  // Notification overrides ----------------------------------------

   /**
    * @return human readable string.
    */
   public String toString()
   {
      StringBuffer buffer = new StringBuffer(100);
      buffer.append(getClass().getName()).append(":");
      buffer.append(" type=").append(getType());
      buffer.append(" source=").append(getSource());
      buffer.append(" sequence=").append(getSequenceNumber());
      buffer.append(" time=").append(getTimeStamp());
      buffer.append(" message=").append(getMessage());
      buffer.append(" derivedGauge=").append(getDerivedGauge());
      buffer.append(" observedAttribute=").append(getObservedAttribute());
      buffer.append(" observedObject=").append(getObservedObject());
      buffer.append(" trigger=").append(getTrigger());
      return buffer.toString();
   }

  // Package protected ---------------------------------------------

  // Protected -----------------------------------------------------

  // Private -------------------------------------------------------

  // Inner classes -------------------------------------------------
}
