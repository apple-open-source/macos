/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package javax.management.timer;

import java.io.Serializable;

import javax.management.Notification;

/**
 * A notification from the timer service.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.3.8.1 $
 *
 * <p><b>Revisions:</b>
 * <p><b>20020816 Adrian Brock:</b>
 * <ul>
 * <li> Serialization </li>
 * </ul>
 */
public class TimerNotification
  extends Notification
  implements Serializable
{
  // Constants -----------------------------------------------------
  
  // Attributes ----------------------------------------------------
  
  /**
   * The notification id of this timer notification.
   */
  private Integer notificationID;

  // Static --------------------------------------------------------

   private static final long serialVersionUID = 1798492029603825750L;

  // Constructors --------------------------------------------------

  /**
   * Construct a new timer notification.
   *
   * @param type the notification type.
   * @param source the notification source.
   * @param sequenceNumber the notification sequence within the source object.
   * @param timeStamp the time the notification was sent.
   * @param message the detailed message.
   * @param id the timer notification id.
   * @param userData additional notification user data
   */
  /*package*/ TimerNotification(String type, Object source, long sequenceNumber, 
               long timeStamp, String message, Integer id, Object userData)
  {
    super(type, source, sequenceNumber, timeStamp, message);
    this.notificationID = id;
    this.setUserData(userData);
  }

  // Public --------------------------------------------------------

  /**
   * Retrieves the notification id of this timer notification.
   *
   * @return the notification id.
   */
  public Integer getNotificationID()
  {
    return notificationID;
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
      buffer.append(" id=").append(getNotificationID());
      buffer.append(" userData=").append(getUserData());
      return buffer.toString();
   }

  // Package protected ---------------------------------------------

  // Protected -----------------------------------------------------

  // Private -------------------------------------------------------

  // Inner classes -------------------------------------------------
}
