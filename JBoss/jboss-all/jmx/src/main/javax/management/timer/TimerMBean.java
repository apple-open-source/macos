/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package javax.management.timer;

import java.util.Date;
import java.util.Vector;

import javax.management.InstanceNotFoundException;

/**
 * The timer service MBean interface. <p>
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.2 $
 *
 */

public interface TimerMBean
{
  // Constants -----------------------------------------------------
  
  // Static --------------------------------------------------------
  
  // Public --------------------------------------------------------

  /**
   * Creates a new timer notification for a specific date/time.
   * The notification is performed once.
   *
   * @param type the notification type.
   * @param message the notification's message string.
   * @param userData the notification's user data.
   * @param date the date/time the notification will occur.
   * @return the notification id for this notification.
   * @exception IllegalArgumentException when the date is before the current
   *        date.
   */
  public Integer addNotification(String type, String message, Object userData,
                                 Date date)
    throws IllegalArgumentException;

  /**
   * Creates a new timer notification for a specific date/time, with an
   * optional repeat period.
   * When the repeat period is not zero, the notification repeats forever.<p>
   *
   * If the date and time is before the the current date and time the period
   * is repeatedly added until a date after the current date and time is
   * found.
   *
   * @param type the notification type.
   * @param message the notification's message string.
   * @param userData the notification's user data.
   * @param date the date/time the notification will occur.
   * @param period the repeat period in milli-seconds. Passing zero means
   *        no repeat.
   * @return the notification id for this notification.
   * @exception IllegalArgumentException when the date is before the current
   *        date or the period is negative.
   */
  public Integer addNotification(String type, String message, Object userData,
                                 Date date, long period)
    throws IllegalArgumentException;

  /**
   * Creates a new timer notification for a specific date/time, with an
   * optional repeat period and a maximum number of occurences.<p>
   *
   * If the date and time is before the the current date and time the period
   * is repeatedly added until a date after the current date and time is
   * found. If the number of occurences is exceeded before the
   * current date and time is reached, an IllegalArgumentException is raised.
   *
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
  public Integer addNotification(String type, String message, Object userData,
                                 Date date, long period, long occurences)
    throws IllegalArgumentException;

  /**
   * Retrieves all timer notifications ids.
   *
   * @return a vector of Integers containing the ids. The list is empty
   *         when there are no timer notifications.
   */
  public Vector getAllNotificationIDs();

  /**
   * Retrieves a copy of the notification date for a passed notification id.
   *
   * @param id the notification id.
   * @return a copy of the notification date or null when the notification id 
   *         is not registered.
   */
  public Date getDate(Integer id);

  /**
   * Retrieves the number of registered timer notifications.
   *
   * @return the number of notifications.
   */
  public int getNbNotifications();

  /**
   * Retrieves a copy of the maximum notification occurences for a passed
   * notification id.
   *
   * @param id the notification id.
   * @return a copy of the maximum notification occurences or null when the
   *         notification id is not registered.
   */
  public Long getNbOccurences(Integer id);

  /**
   * Retrieves all timer notifications ids of the passed notification type.
   *
   * @param type the notification type.
   * @return a vector of Integers containing the ids. The list is empty
   *         when there are no timer notifications of the passed type.
   */
  public Vector getNotificationIDs(String type);

  /**
   * Retrieves the notification message for a passed notification id.
   *
   * @param id the notification id.
   * @return the notification message or null when the notification id is
   *         not registered.
   */
  public String getNotificationMessage(Integer id);

  /**
   * Retrieves the notification type for a passed notification id.
   *
   * @param id the notification id.
   * @return the notification type or null when the notification id is
   *         not registered.
   */
  public String getNotificationType(Integer id);

  /**
   * Retrieves the notification user data for a passed notification id.
   *
   * @param id the notification id.
   * @return the notification user data or null when the notification id is
   *         not registered.
   */
  public Object getNotificationUserData(Integer id);

  /**
   * Retrieves a copy of the notification period for a passed notification id.
   *
   * @param id the notification id.
   * @return a copy of the notification period or null when the notification 
   *         id is not registered.
   */
  public Long getPeriod(Integer id);

  /**
   * Retrieves the flag indicating whether past notifications are sent.
   *
   * @param id the notification id.
   * @return true when past notifications are sent, false otherwise.
   */
  public boolean getSendPastNotifications();

  /**
   * Test whether the timer MBean is active.
   *
   * @return true when timer is active, false otherwise.
   */
  public boolean isActive();

  /**
   * Test whether the timer MBean has any registered notifications.
   *
   * @return true when timer has no registered notifications, false otherwise.
   */
  public boolean isEmpty();

  /**
   * Removes all notifications from the timer MBean.
   */
  public void removeAllNotifications();

  /**
   * Removes a notification from the timer MBean with the specified
   * notification id.
   *
   * @param id the notification id.
   * @exception InstanceNotFoundException when there are no notification
   *        registered with the id passed.
   */
  public void removeNotification(Integer id)
    throws InstanceNotFoundException;

  /**
   * Removes all notifications from the timer MBean of the specified
   * notification type.
   *
   * @param type the notification type.
   * @exception InstanceNotFoundException when there are no notifications of
   *        the type passed.
   */
  public void removeNotifications(String type)
    throws InstanceNotFoundException;

  /**
   * Sets the flag indicating whether past notifications are sent.
   *
   * @param value the new value of the flag. true when past notifications
   *        are sent, false otherwise.
   */
  public void setSendPastNotifications(boolean value);

  /**
   * Starts the timer. If there are any notifications before the current time
   * these notifications are processed. The notification only takes place
   * when send past notiications is true.
   */
  public void start();

  /**
   * Stops the timer.
   */
  public void stop();
}
