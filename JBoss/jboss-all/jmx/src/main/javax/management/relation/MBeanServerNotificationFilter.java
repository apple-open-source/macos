/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.relation;

import java.io.InvalidObjectException;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.ObjectStreamField;
import java.io.StreamCorruptedException;

import java.util.HashSet;
import java.util.List;
import java.util.Vector;

import javax.management.MBeanServerNotification;
import javax.management.Notification;
import javax.management.NotificationFilterSupport;
import javax.management.ObjectName;

import org.jboss.mx.util.Serialization;

/**
 * A helper class, used to filter notifications of registration,
 * unregistration of selected object names.
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.3.8.1 $
 *
 * <p><b>Revisions:</b>
 * <p><b>20020711 Adrian Brock:</b>
 * <ul>
 * <li> Serialization </li>
 * </ul>
 */
public class MBeanServerNotificationFilter
  extends NotificationFilterSupport
{
  // Constants ---------------------------------------------------

  // Attributes --------------------------------------------------

  /**
   * Enabled Object Names.
   */
  private HashSet enabled = new HashSet();

  /**
   * Disable Object Names.
   */
  private HashSet disabled = null;

  // Static ------------------------------------------------------

   private static final long serialVersionUID;
   private static final ObjectStreamField[] serialPersistentFields;
   static
   {
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         serialVersionUID = 6001782699077323605L;
         serialPersistentFields = new ObjectStreamField[]
         {
            new ObjectStreamField("myDeselectObjNameList", Vector.class),
            new ObjectStreamField("mySelectObjNameList",   Vector.class),
         };
         break;
      default:
         serialVersionUID = 2605900539589789736L;
         serialPersistentFields = new ObjectStreamField[]
         {
            new ObjectStreamField("deselectedNames", List.class),
            new ObjectStreamField("selectedNames",   List.class)
         };
      }
   }

  // Constructors ------------------------------------------------

  /**
   * Create a filter selecting nothing by default<p>
   *
   * WARNING!! WARNING!! The spec says the MBeanServerNotificationFilter
   * accepts everything by default. The RI does exactly the opposite.
   * I follow the RI.
   */
  public MBeanServerNotificationFilter()
  {
  }

  // Public ------------------------------------------------------

  /**
   * Disable all object names. Rejects all notifications.
   */
  public synchronized void disableAllObjectNames()
  {
    enabled = new HashSet();
    disabled = null;
  }

  /**
   * Disable an object name.
   *
   * @param objectName the object name to disable.
   * @exception IllegalArgumentException for a null object name
   */
  public synchronized void disableObjectName(ObjectName objectName)
  {
    if (objectName == null)
      throw new IllegalArgumentException("null object name");
    if (enabled != null)
      enabled.remove(objectName);
    if (disabled != null && disabled.contains(objectName) == false)
      disabled.add(objectName);
  }

  /**
   * Enable all object names. Accepts all notifications.
   */
  public synchronized void enableAllObjectNames()
  {
    enabled = null;
    disabled = new HashSet();
  }

  /**
   * Enable an object name.
   *
   * @param objectName the object name to enable.
   * @exception IllegalArgumentException for a null object name
   */
  public synchronized void enableObjectName(ObjectName objectName)
  {
    if (objectName == null)
      throw new IllegalArgumentException("null object name");
    if (disabled != null)
      disabled.remove(objectName);
    if (enabled != null && enabled.contains(objectName) == false)
      enabled.add(objectName);
  }

  /**
   * Get all the disabled object names.<p>
   *
   * Returns a vector of disabled object names.<br>
   * Null for all object names disabled.
   * An empty vector means all object names enabled.
   *
   * @return the vector of disabled object names.
   */
  public synchronized Vector getDisabledObjectNames()
  {
    if (disabled == null)
      return null;
    return new Vector(disabled);
  }

  /**
   * Get all the enabled object names.<p>
   *
   * Returns a vector of enabled object names.<br>
   * Null for all object names enabled.
   * An empty vector means all object names disabled.
   *
   * @return the vector of enabled object names.
   */
  public synchronized Vector getEnabledObjectNames()
  {
    if (enabled == null)
      return null;
    return new Vector(enabled);
  }

   /**
    * @return human readable string.
    */
    public String toString()
   {
      StringBuffer buffer = new StringBuffer(100);
      buffer.append(getClass().getName()).append(":");
      buffer.append(" enabledTypes=").append(getEnabledTypes());
      buffer.append(" enabledObjectNames=").append(getEnabledObjectNames());
      buffer.append(" disabledObjectNames=").append(getDisabledObjectNames());
      return buffer.toString();
   }

  // NotificationFilterSupport overrides -------------------------

  /**
   * Test to see whether this notification is enabled
   *
   * @param notification the notification to filter
   * @return true when the notification should be sent, false otherwise
   * @exception IllegalArgumentException for null notification.
   */
  public synchronized boolean isNotificationEnabled(Notification notification)
    throws IllegalArgumentException
  {
    if (notification == null)
      throw new IllegalArgumentException("null notification");

    // Check the notification type
    if (super.isNotificationEnabled(notification) == false)
      return false;

    // Get the object name
    MBeanServerNotification mbsNotification = (MBeanServerNotification) notification;
    ObjectName objectName = mbsNotification.getMBeanName();

    // Is it enabled?
    if (enabled != null)
      return enabled.contains(objectName);

    // Is it not disabled?
    if (disabled.contains(objectName) == false)
      return true;

    // Disabled
    return false;
  }

  // Private -----------------------------------------------------

   private void readObject(ObjectInputStream ois)
      throws IOException, ClassNotFoundException
   {
      ObjectInputStream.GetField getField = ois.readFields();
      List deselectedNames;
      List selectedNames;
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         deselectedNames = (List) getField.get("myDeselectObjNameList", null);
         selectedNames = (List) getField.get("mySelectObjNameList", null);
         break;
      default:
         deselectedNames = (List) getField.get("deselectedNames", null);
         selectedNames = (List) getField.get("selectedNames", null);
      }
      if (deselectedNames == null && selectedNames == null)
         throw new StreamCorruptedException("Nothing enabled or disabled?");
      if (deselectedNames == null)
         disabled = null;
      else
         disabled = new HashSet(deselectedNames);
      if (selectedNames == null)
         enabled = null;
      else
         enabled = new HashSet(selectedNames);
   }

   private void writeObject(ObjectOutputStream oos)
      throws IOException
   {
      ObjectOutputStream.PutField putField = oos.putFields();
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         putField.put("myDeselectObjNameList", getDisabledObjectNames());
         putField.put("mySelectObjNameList", getEnabledObjectNames());
         break;
      default:
         putField.put("deselectedNames", getDisabledObjectNames());
         putField.put("selectedNames", getEnabledObjectNames());
      }
      oos.writeFields();
   }
}
