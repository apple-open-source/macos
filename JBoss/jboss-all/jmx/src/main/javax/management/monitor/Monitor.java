/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package javax.management.monitor;

import java.io.Serializable;

import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

import javax.management.InstanceNotFoundException;
import javax.management.MBeanAttributeInfo;
import javax.management.MBeanInfo;
import javax.management.MBeanRegistration;
import javax.management.MBeanServer;
import javax.management.NotificationBroadcasterSupport;
import javax.management.ObjectName;

import org.jboss.mx.util.RunnableScheduler;
import org.jboss.mx.util.SchedulableRunnable;

/**
 * The monitor service.
 *
 * <p><b>Revisions:</b>
 * <p><b>20020319 Adrian Brock:</b>
 * <ul>
 * <li>Notify using the object name and fix the notification payloads
 * </ul>
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.6 $
 */
public abstract class Monitor
  extends NotificationBroadcasterSupport
  implements MonitorMBean, MBeanRegistration, Serializable
{
  // Constants -----------------------------------------------------

  /**
   * Used to reset errors in {@link #alreadyNotified}.
   * REVIEW: Check
   */
  protected final int RESET_FLAGS_ALREADY_NOTIFIED = 0;

  /**
   * An observed attribute type error has been notified.
   * REVIEW: Check
   */
  protected final int RUNTIME_ERROR_NOTIFIED = 1;

  /**
   * An observed object error has been notified.
   * REVIEW: Check
   */
  protected final int OBSERVED_OBJECT_ERROR_NOTIFIED = 2;

  /**
   * An observed attribute error has been notified.
   * REVIEW: Check
   */
  protected final int OBSERVED_ATTRIBUTE_ERROR_NOTIFIED = 4;

  /**
   * An observed attribute type error has been notified.
   * REVIEW: Check
   */
  protected final int OBSERVED_ATTRIBUTE_TYPE_ERROR_NOTIFIED = 8;

  // Attributes ----------------------------------------------------

  /**
   * The granularity period.
   */
  long granularityPeriod = 10000;

  /**
   * The observed attribute.
   */
  String observedAttribute = null;

  /**
   * The observed object.
   */
  ObjectName observedObject = null;

  /**
   * Whether the service is active.
   */
  boolean active = false;

  /**
   * The server this service is registered in.
   */
  protected MBeanServer server;

  /**
   * The object name of this monitor.
   */
  ObjectName objectName;

  /**
   * The errors that have already been notified.
   * REVIEW: Check
   */
  protected int alreadyNotified = 0;

  /**
   * ????.
   * REVIEW: Implement
   */
  protected String dbgTag = null;

  /**
   * The runnable monitor.
   */
  private MonitorRunnable monitorRunnable;

  /**
   * The notification sequence number.
   */
  private long sequenceNumber;
  
  // Static --------------------------------------------------------

  /**
   * The scheduler.
   */
  static RunnableScheduler scheduler;

  /**
   * Start the scheduler
   */
  static
  {
     scheduler = new RunnableScheduler();
     scheduler.start();
  }
  
  // Constructors --------------------------------------------------

  // Public --------------------------------------------------------

  // MonitorMBean implementation -----------------------------------

  public long getGranularityPeriod()
  {
    return granularityPeriod;
  }

  public String getObservedAttribute()
  {
    return observedAttribute;
  }

  public ObjectName getObservedObject()
  {
    return observedObject;
  }

  public boolean isActive()
  {
    return active;
  }
  public void setGranularityPeriod(long period)
    throws IllegalArgumentException
  {
    if (period <= 0)
      throw new IllegalArgumentException("Period must be positive.");
    granularityPeriod = period;
  }

  public void setObservedAttribute(String attribute)
  {
    observedAttribute = attribute;
    // REVIEW: not find grained enough?
    alreadyNotified = RESET_FLAGS_ALREADY_NOTIFIED;
  }

  public void setObservedObject(ObjectName object)
  {
    observedObject = object;
    // REVIEW: not find grained enough?
    alreadyNotified = RESET_FLAGS_ALREADY_NOTIFIED;
  }

  public synchronized void start()
  {
    // Ignore if already active
    if (active)
      return;
    active = true;

    // Start the monitor runnable
    monitorRunnable = new MonitorRunnable(this);
  }

  public synchronized void stop()
  {
    // Ignore if not active
    if (!active)
      return;

    // Stop the monitor runnable
    active = false;
    monitorRunnable.setScheduler(null);
    monitorRunnable = null;
  }

  // MBeanRegistrationImplementation overrides ---------------------

  public ObjectName preRegister(MBeanServer server, ObjectName objectName)
    throws Exception
  {
    // Remember the server.
    this.server = server;

    // Remember the object name.
    this.objectName = objectName;

    // Use the passed object name.
    return objectName;
  }

  public void postRegister(Boolean registrationDone)
  {
  }

  public void preDeregister()
    throws Exception
  {
    // Stop the monitor before deregistration.
    stop();
  }

  public void postDeregister()
  {
  }

  // Package protected ---------------------------------------------

  /**
   * Run the monitor.<p>
   *
   * Retrieves the monitored attribute and passes it to each service.<p>
   *
   * Peforms the common error processing.
   * REVIEW: Use the internal interface????
   */
  void runMonitor()
  {
    // Monitor for uncaught errors
    try
    {
      MBeanInfo mbeanInfo = null;
      try
      {
        mbeanInfo = server.getMBeanInfo(observedObject);
      }
      catch (InstanceNotFoundException e)
      {
        sendObjectErrorNotification("The observed object is not registered.");
        return;
      }

      // Get the attribute information
      MBeanAttributeInfo[] mbeanAttributeInfo = mbeanInfo.getAttributes();
      MBeanAttributeInfo attributeInfo = null;
      for (int i = 0; i < mbeanAttributeInfo.length; i++)
      {
        if (mbeanAttributeInfo[i].getName().equals(observedAttribute))
        {
          attributeInfo = mbeanAttributeInfo[i];
          break;
        }
      }

      // The attribute must exist
      if (attributeInfo == null)
      {
        sendAttributeErrorNotification(
          "The observed attribute does not exist");
        return;
      }
      // The attribute must exist
      if (!attributeInfo.isReadable())
      {
        sendAttributeErrorNotification("Attribute not readable.");
        return;
      }

      // Get the value
      Object value = null;
      try
      {
        value = server.getAttribute(observedObject, observedAttribute);
      }
      catch (InstanceNotFoundException e)
      {
        sendObjectErrorNotification("The observed object is not registered.");
        return;
      }

      // Check for null value
      if (value == null)
      {
        sendAttributeTypeErrorNotification("Attribute is null");
        return;
      }

      // Now pass the value to the respective monitor.
      monitor(attributeInfo, value);
    }
    // Notify an unexcepted error
    catch (Exception e)
    {
      sendRuntimeErrorNotification("General error: " + e.toString());
    }
  }

  /**
   * Perform the monitor specific processing. 
   *
   * @param attributeInfo the MBean attribute information.
   * @param value the value to monitor.
   */
  abstract void monitor(MBeanAttributeInfo attributeInfo, Object value)
    throws Exception;

  /**
   * Sends the notification
   *
   * @param type the notification type.
   * @param timestamp the time of the notification.
   * @param message the human readable message to send.
   * @param attribute the attribute name.
   * @param gauge the derived gauge.
   * @param trigger the trigger value.
   */
  void sendNotification(String type, long timestamp, String message, 
      String attribute, Object gauge, Object trigger)
  {
    long seq = 0;
    synchronized (this)
    {
      seq = ++sequenceNumber;
    }
    if (timestamp == 0)
      timestamp = System.currentTimeMillis();
    sendNotification(new MonitorNotification(type, objectName, seq,
                     timestamp, message, gauge,
                     attribute, observedObject, trigger));
  }

  /**
   * Send a runtime error notification.
   *
   * @param message the human readable message to send.
   */
  void sendRuntimeErrorNotification(String message)
  {
    if ((alreadyNotified & RUNTIME_ERROR_NOTIFIED) == 0)
      sendNotification(MonitorNotification.RUNTIME_ERROR, 0,
        message, observedAttribute, null, null);
    alreadyNotified |= RUNTIME_ERROR_NOTIFIED;
  }

  /**
   * Send an object error notification.
   *
   * @param message the human readable message to send.
   */
  void sendObjectErrorNotification(String message)
  {
    if ((alreadyNotified & OBSERVED_OBJECT_ERROR_NOTIFIED) == 0)
      sendNotification(MonitorNotification.OBSERVED_OBJECT_ERROR, 0,
        message, observedAttribute, null, null);
    alreadyNotified |= OBSERVED_OBJECT_ERROR_NOTIFIED;
  }

  /**
   * Send an attribute error notification.
   *
   * @param message the human readable message to send.
   */
  void sendAttributeErrorNotification(String message)
  {
    if ((alreadyNotified & OBSERVED_ATTRIBUTE_ERROR_NOTIFIED) == 0)
      sendNotification(MonitorNotification.OBSERVED_ATTRIBUTE_ERROR, 0,
        message, observedAttribute, null, null);
    alreadyNotified |= OBSERVED_ATTRIBUTE_ERROR_NOTIFIED;
  }

  /**
   * Send an attribute type error notification.
   *
   * @param message the human readable message to send.
   */
  void sendAttributeTypeErrorNotification(String message)
  {
    if ((alreadyNotified & OBSERVED_ATTRIBUTE_TYPE_ERROR_NOTIFIED) == 0)
      sendNotification(MonitorNotification.OBSERVED_ATTRIBUTE_TYPE_ERROR, 0,
        message, observedAttribute, null, null);
    alreadyNotified |= OBSERVED_ATTRIBUTE_TYPE_ERROR_NOTIFIED;
  }

  // Protected -----------------------------------------------------

  // Private -------------------------------------------------------

  // Inner classes -------------------------------------------------

  /**
   * A monitor runnable.
   */
  private class MonitorRunnable
     extends SchedulableRunnable
  {
    // Attributes ----------------------------------------------------

    // The monitoring to perform
    private Monitor monitor;

    // Constructors --------------------------------------------------

    /**
     * Create a monitor runnable to periodically perform monitoring.
     *
     * @param monitor the monitoring to perform.
     */
    public MonitorRunnable(Monitor monitor)
    {
      this.monitor = monitor;
      setScheduler(scheduler);
    }

    // Public --------------------------------------------------------

    // SchedulableRunnable overrides ---------------------------------

    /**
     * Run the montior
     */
    public void doRun()
    {
        // Perform the monitoring
        monitor.runMonitor();
 
        // Reschedule
        setNextRun(System.currentTimeMillis() + monitor.granularityPeriod);
    } 
  }
}
