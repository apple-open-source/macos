/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package javax.management.timer;

import java.io.Serializable;

import java.util.Date;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Vector;

import javax.management.InstanceNotFoundException;
import javax.management.MBeanRegistration;
import javax.management.MBeanServer;
import javax.management.NotificationBroadcasterSupport;
import javax.management.ObjectName;

import org.jboss.mx.util.RunnableScheduler;
import org.jboss.mx.util.SchedulableRunnable;

/**
 * The timer service.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.10 $
 */
public class Timer
  extends NotificationBroadcasterSupport
  implements TimerMBean, MBeanRegistration, Serializable
{
  // Constants -----------------------------------------------------

  /**
   * The number of milliseconds in one second.
   */
  public static final long ONE_SECOND = 1000;

  /**
   * The number of milliseconds in one minute.
   */
  public static final long ONE_MINUTE = ONE_SECOND * 60;

  /**
   * The number of milliseconds in one hour.
   */
  public static final long ONE_HOUR = ONE_MINUTE * 60;

  /**
   * The number of milliseconds in one day.
   */
  public static final long ONE_DAY = ONE_HOUR * 24;

  /**
   * The number of milliseconds in one week.
   */
  public static final long ONE_WEEK = ONE_DAY * 7;

  /**
   * Don't send notifications at initial start up.
   */
  private static final int SEND_NO = 0;

  /**
   * Send all past notifications at initial start up.
   */
  private static final int SEND_START = 1;

  /**
   * Normal operation sending
   */
  private static final int SEND_NORMAL = 2;

  // Attributes ----------------------------------------------------

  /**
   * The next notification id.
   */
  int nextId = 0;

  /**
   * The next notification sequence number.
   */
  long sequenceNumber = 0;

  /**
   * The send past events attribute.
   */
  boolean sendPastNotifications = false;

  /**
   * Whether the service is active.
   */
  boolean active = false;

  /**
   * Our object name.
   */
  ObjectName objectName;

  /**
   * The registered notifications.
   */
  HashMap notifications = new HashMap();

  /**
   * The scheduler
   */
  private RunnableScheduler scheduler = new RunnableScheduler();

  // Static --------------------------------------------------------

  // Constructors --------------------------------------------------

  // Public --------------------------------------------------------

  // TimerMBean implementation -------------------------------------

  public Integer addNotification(String type, String message, Object userData,
                                 Date date)
    throws IllegalArgumentException
  {
    return addNotification(type, message, userData, date, 0);
  }

  public Integer addNotification(String type, String message, Object userData,
                                 Date date, long period)
    throws IllegalArgumentException
  {
    return addNotification(type, message, userData, date, period, 0);
  }

  public Integer addNotification(String type, String message,
                Object userData, Date date, long period, long occurences)
    throws IllegalArgumentException
  {
     // Generate the next id.
    int newId = 0;
    newId = ++nextId;
    Integer id = new Integer(newId);

    // Validate and create the registration.
    RegisteredNotification rn =
      new RegisteredNotification(id, type, message, userData, date, period,
                                 occurences);

    // Add the registration.
    synchronized(notifications)
    {
       notifications.put(id, rn);
       rn.setNextRun(rn.nextDate);
       rn.setScheduler(scheduler);
    }

    return id;
  }

  public Vector getAllNotificationIDs()
  {
     synchronized(notifications)
     {
        return new Vector(notifications.keySet());
     }
  }

  public Date getDate(Integer id)
  {
    // Make sure there is a registration
    RegisteredNotification rn = (RegisteredNotification) notifications.get(id);
    if (rn == null)
      return null;

    // Return a copy of the date.
    return new Date(rn.startDate);
  }

  public int getNbNotifications()
  {
    return notifications.size();
  }

  public Long getNbOccurences(Integer id)
  {
    // Make sure there is a registration
    RegisteredNotification rn = (RegisteredNotification) notifications.get(id);
    if (rn == null)
      return null;

    // Return a copy of the occurences.
    return new Long(rn.occurences);
  }

  public Vector getNotificationIDs(String type)
  {
    Vector result = new Vector();

    // Loop through the notifications looking for the passed type.
    synchronized (notifications)
    {
       Iterator iterator = notifications.values().iterator();
       while (iterator.hasNext())
       {
          RegisteredNotification rn = (RegisteredNotification) iterator.next();
          if (rn.type.equals(type))
             result.add(rn.id);
       }
    }
      
    return result;
  }

  public String getNotificationMessage(Integer id)
  {
    // Make sure there is a registration
    RegisteredNotification rn = (RegisteredNotification) notifications.get(id);
    if (rn == null)
      return null;

    // Return the message
    return rn.message;
  }

  public String getNotificationType(Integer id)
  {
    // Make sure there is a registration
    RegisteredNotification rn = (RegisteredNotification) notifications.get(id);
    if (rn == null)
      return null;

    // Return the type.
    return rn.type;
  }

  public Object getNotificationUserData(Integer id)
  {
    // Make sure there is a registration
    RegisteredNotification rn = (RegisteredNotification) notifications.get(id);
    if (rn == null)
      return null;

    // Return the user data.
    return rn.userData;
  }

  public Long getPeriod(Integer id)
  {
    // Make sure there is a registration
    RegisteredNotification rn = (RegisteredNotification) notifications.get(id);
    if (rn == null)
      return null;

    // Return a copy of the period
    return new Long(rn.period);
  }

  public boolean getSendPastNotifications()
  {
    return sendPastNotifications;
  }

  public boolean isActive()
  {
    return active;
  }

  public boolean isEmpty()
  {
    return notifications.isEmpty();
  }

  public void removeAllNotifications()
  {
    // Remove the notifications
    synchronized(notifications)
    {
       Iterator iterator = notifications.values().iterator();
       while (iterator.hasNext())
       {
          RegisteredNotification rn = (RegisteredNotification) iterator.next();
          rn.setScheduler(null);
          iterator.remove();
       }
    }

    // The spec says to reset the identifiers, seems like a bad idea to me
    synchronized (this)
    {
       nextId = 0;
    }
  }

  public void removeNotification(Integer id)
    throws InstanceNotFoundException
  {
    // Check if there is a notification.
    synchronized(notifications)
    {
       RegisteredNotification rn = (RegisteredNotification) notifications.get(id);
       if (rn == null)
         throw new InstanceNotFoundException("No notification id : " +
                                          id.toString());

       // Remove the notification
       rn.setScheduler(null);
       notifications.remove(id);
    }
  }

  public void removeNotifications(String type)
    throws InstanceNotFoundException
  {
    boolean found = false;

    // Loop through the notifications removing the passed type.
    synchronized(notifications)
    {
       Iterator iterator = notifications.values().iterator();
       while (iterator.hasNext())
       {
          RegisteredNotification rn = (RegisteredNotification) iterator.next();
          if (rn.type.equals(type))
          {
             rn.setScheduler(null);
             iterator.remove();
             found = true;
          }
       }
    }

    // The spec says to through an exception when nothing removed.
    if (found == false)
      throw new InstanceNotFoundException("Nothing registered for type: " +
                                          type);
  }

  public void setSendPastNotifications(boolean value)
  {
    sendPastNotifications = value;
  }

  public synchronized void start()
  {
    // Ignore if already active
    if (active == true)
      return;
    active = true;

    // Perform the initial sends, for past notifications send missed events
    // otherwise ignore them
    synchronized(notifications)
    {
       Iterator iterator = notifications.values().iterator();
       while (iterator.hasNext())
       {
         RegisteredNotification rn = (RegisteredNotification) iterator.next();
         if (sendPastNotifications)
            rn.sendType = SEND_START;
         else
            rn.sendType = SEND_NO;
         sendNotifications(rn);
         rn.sendType = SEND_NORMAL;
       }
    }

    // Start 'em up
    scheduler.start();
  }

  public synchronized void stop()
  {
    // Ignore if not active
    if (active == false)
      return;

    // Stop the threads
    active = false;
    scheduler.stop();
  }

  // MBeanRegistrationImplementation overrides ---------------------

  public ObjectName preRegister(MBeanServer server, ObjectName objectName)
    throws Exception
  {
    // Save the object name
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
    // Stop the timer before deregistration.
    stop();
  }

  public void postDeregister()
  {
  }

  // Package protected ---------------------------------------------

  // Protected -----------------------------------------------------

  // Private -------------------------------------------------------

  /**
   * Send any outstanding notifications.
   *
   * @param rn the registered notification to send.
   */
  private void sendNotifications(RegisteredNotification rn)
  {
    // Keep going until we have done all outstanding notifications.
    // The loop ends when not active, or there are no outstanding
    // notifications.
    // REVIEW: In practice for normal operation it never loops. We
    // ignore sends that we have missed. This avoids problems where
    // the notification takes longer than the period. Correct???
    while (isActive() && rn.nextDate != 0
           && rn.nextDate <= System.currentTimeMillis())
    {
      // Do we actually send it?
      // Yes, unless start and not sending past notifications.
      if (rn.sendType != SEND_NO)
      {
        long seq = 0;
        synchronized(this)
        {
           seq = ++sequenceNumber;
        }
        sendNotification(new TimerNotification(rn.type, objectName, seq,
                         rn.nextDate, rn.message, rn.id, rn.userData));
      }

      // Calculate the next date.
      // Except for when we are sending past notifications at start up,
      // it cannot be in the future.
      do
      {
        // If no next run, remove it sets the next date to zero.
        if (rn.calcNextDate() == false)
        {
          synchronized(notifications)
          {
             notifications.remove(rn.id);
          }
        }
      }
      while (isActive() == true && rn.sendType != SEND_START && rn.nextDate != 0
             && rn.occurences == 0 && rn.nextDate < System.currentTimeMillis());
    }

    if (rn.nextDate != 0)
       rn.setNextRun(rn.nextDate);
  }

  // Inner classes -------------------------------------------------

  /**
   * A registered notification. These run as separate threads.
   */
  private class RegisteredNotification
    extends SchedulableRunnable
  {
    // Attributes ----------------------------------------------------

    /**
     * The notification id.
     */
    public Integer id;

    /**
     * The notification type.
     */
    public String type;

    /**
     * The message.
     */
    public String message;

    /**
     * The user data.
     */
    public Object userData;

    /**
     * The start date.
     */
    public long startDate;

    /**
     * The period.
     */
    public long period;

    /**
     * The maximum number of occurences.
     */
    public long occurences;

    /**
     * The send type, no send, past notifications or normal
     */
    public int sendType = SEND_NORMAL;

    /**
     * The next run date
     */
    public long nextDate = 0;

    // Constructors --------------------------------------------------

    /**
     * The default constructor.
     *
     * @param id the notification id.
     * @param type the notification type.
     * @param message the notification's message string.
     * @param userData the notification's user data.
     * @param date the date/time the notification will occur.
     * @param period the repeat period in milli-seconds. Passing zero means
     *        no repeat.
     * @param occurences the maximum number of repeats. When the period is not
     *        zero and this parameter is zero, it will repeat indefinitely.
     * @return the notification id for this notification.
     * @exception IllegalArgumentException when the date is before the current
     *        date, the period is negative or the number of repeats is
     *        negative.
     */
    public RegisteredNotification(Integer id, String type, String message,
                                  Object userData, Date startDate,
                                  long period, long occurences)
      throws IllegalArgumentException
    {
      // Basic validation
      if (startDate == null)
        throw new IllegalArgumentException("Null Date");
      if (period < 0)
        throw new IllegalArgumentException("Negative Period");
      if (occurences < 0)
        throw new IllegalArgumentException("Negative Occurences");

      // Remember the values
      this.id = id;
      this.type = type;
      this.message = message;
      this.userData = userData;
      this.startDate = startDate.getTime();
      this.period = period;
      this.occurences = occurences;

      // Can we get a next date in the future
      nextDate = this.startDate;
      while (nextDate < System.currentTimeMillis())
      {
        // If we have no more occurences its an error
        if (calcNextDate() == false)
          throw new IllegalArgumentException("Requested notification(s) " +
                                             "in the past.");
      }
    }

    // Public --------------------------------------------------------

    /**
     * Calculate the next notification date. Add on the period until
     * the number of occurences is exhausted.
     *
     * @return false when there are no more occurences, true otherwise.
     */
    boolean calcNextDate()
    {
      // No period, we've finished
      if (period == 0)
      {
        nextDate = 0;
        return false;
      }

      // Limited number of repeats have we finished?
      if (occurences != 0 && --occurences == 0)
      {
        nextDate = 0;
        return false;
      }

      // Calculate the next occurence
      nextDate += period;

      return true;
    }

    // SchedulableRunnable overrides ---------------------------------

    /**
     * Send the notifications.
     */
    public void doRun()
    {
        // Send any notifications
        sendNotifications(this);
    }
  }
}
