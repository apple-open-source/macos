/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.util.Enumeration;
import java.util.Vector;

import java.io.Serializable;

/**
 * An implementation of the {@link NotificationFilter} interface.<p>
 *
 * It filters on the notification type. It Maintains a list of enabled
 * notification types. By default no notifications are enabled.<p>
 *
 * The enabled types are prefixes. That is a notification is enabled if
 * it starts with an enabled string.
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class NotificationFilterSupport
  implements NotificationFilter
{
  // Constants ---------------------------------------------------

   private static final long serialVersionUID = 6579080007561786969L;

  // Attributes --------------------------------------------------

  /**
   * Enabled notification types.
   */
  private Vector enabledTypes;

  // Static ------------------------------------------------------

  // Constructors ------------------------------------------------

  /**
   * Create a filter that filters out all notification types.
   */
  public NotificationFilterSupport()
  {
    enabledTypes = new Vector();
  }

  // Public ------------------------------------------------------

  /**
   * Disable all notification types. Rejects all notifications.
   */
  public synchronized void disableAllTypes()
  {
    enabledTypes = new Vector();
  }

  /**
   * Disable a notification type.
   *
   * @param type the notification type to disable.
   */
  public synchronized void disableType(String type)
  {
    // Null won't be in the list anyway.
    enabledTypes.removeElement(type);
  }

  /**
   * Enable a notification type.
   *
   * @param type the notification type to enable.
   * @exception IllegalArgumentException for a null type
   */
  public synchronized void enableType(String type)
  {
    if (type == null)
      throw new IllegalArgumentException("null notification type");
    if (enabledTypes.contains(type) == false)
      enabledTypes.addElement(type);
  }

  /**
   * Get all the enabled notification types.<p>
   *
   * Returns a vector of enabled notification type.<br>
   * An empty vector means all types disabled.
   *
   * @return the vector of enabled types.
   */
  public synchronized Vector getEnabledTypes()
  {
    return (Vector) enabledTypes.clone();
  }

  // NotificationFilter implementation ---------------------------

  /**
   * Test to see whether this notification is enabled
   *
   * @param notification the notification to filter
   * @return true when the notification should be sent, false otherwise
   * @exception IllegalArgumentException for null notification.
   */
  public synchronized boolean isNotificationEnabled(Notification notification)
  {
    if (notification == null)
      throw new IllegalArgumentException("null notification");
    // Is it enabled?
    String notificationType = notification.getType();
    for (Enumeration e = enabledTypes.elements(); e.hasMoreElements(); )
    {
      String type = (String) e.nextElement();
      if (notificationType.startsWith(type))
        return true;
    }
    return false;
  }

  // Private -----------------------------------------------------
}
