/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * This interface is implemented by any class acting as a notification
 * filter.<p>
 *
 * The filter is used before the notification is sent to see whether
 * the notification is required.
 *
 * @see javax.management.NotificationFilterSupport
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.2 $
 *
 */
public interface NotificationFilter
  extends java.io.Serializable
{
   // Constants ---------------------------------------------------

   // Public ------------------------------------------------------

   /**
    * This method is called before a notification is sent to see whether
    * the listener wants the notification.
    *
    * @param notification the notification to be sent.
    * @return true if the listener wants the notification, false otherwise
    */
   public boolean isNotificationEnabled(Notification notification);
}
