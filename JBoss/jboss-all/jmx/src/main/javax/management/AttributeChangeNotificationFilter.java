/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.ObjectStreamField;

import java.util.Set;
import java.util.HashSet;
import java.util.Vector;

import javax.management.AttributeChangeNotification;
import javax.management.NotificationFilter;

/**
 * Notification filter support for attribute change notifications.
 *
 * @see javax.management.AttributeChangeNotification
 * @see javax.management.NotificationFilter
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author  a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.3.4.1 $
 *
 * <p><b>Revisions:</b>
 * <p><b>20020710 Adrian Brock:</b>
 * <ul>
 * <li> Serialization </li>
 * </ul>
 */
public class AttributeChangeNotificationFilter
   implements NotificationFilter, java.io.Serializable
{
   
   // Constants -----------------------------------------------------

   private static final long serialVersionUID = -6347317584796410029L;
   private static final ObjectStreamField[] serialPersistentFields = new ObjectStreamField[]
   {
      new ObjectStreamField("enabledAttributes", Vector.class)
   };
   
   // Attributes ----------------------------------------------------
   private Set attributes = new HashSet();
   
   
   // Constructors --------------------------------------------------
   
   /**
    * Constructs an attribute change notification filter. All attribute
    * notifications are filtered by default. Use {@link #enableAttribute}
    * to enable notifications of a given attribute to pass this filter.
    */
   public AttributeChangeNotificationFilter()
   {
   }

   // Public --------------------------------------------------------
   
   /**
    * Enables the attribute change notifications of the given attribute to be
    * sent to the listener.
    *
    * @param   name  name of the management attribute
    */
   public void enableAttribute(String name)
   {
      attributes.add(name);
   }

   /**
    * Disable the attribute change notifications of the given attribute.
    * Attribute change notifications for this attribute will not be sent to
    * the listener.
    *
    * @param   name name of the management attribute
    */
   public void disableAttribute(String name)
   {
      attributes.remove(name);
   }

   /**
    * Disables all attribute change notifications.
    */
   public void disableAllAttributes()
   {
      attributes.clear();
   }

   /**
    * Returns the names of the attributes whose notifications are allowed to
    * pass this filter.
    *
    * @return  a vector containing the name strings of the enabled attributes
    */
   public Vector getEnabledAttributes()
   {
      return new Vector(attributes);
   }

   /**
    * @return human readable string.
    */
   public String toString()
   {
      StringBuffer buffer = new StringBuffer(100);
      buffer.append(getClass().getName()).append(":");
      buffer.append(" enabledAttributes=").append(getEnabledAttributes());
      return buffer.toString();
   }

   // NotificationFilter implementation -----------------------------
   public boolean isNotificationEnabled(Notification notification)
   {
      AttributeChangeNotification notif = (AttributeChangeNotification)notification;
      if (attributes.contains(notif.getAttributeName()))
         return true;
         
      return false;
   }

   // Private -------------------------------------------------------

   private void readObject(ObjectInputStream ois)
      throws IOException, ClassNotFoundException
   {
      ObjectInputStream.GetField getField = ois.readFields();
      Vector enabled = (Vector) getField.get("enabledAttributes", null);
      attributes = new HashSet(enabled);
   }

   private void writeObject(ObjectOutputStream oos)
      throws IOException
   {
      ObjectOutputStream.PutField putField = oos.putFields();
      putField.put("enabledAttributes", new Vector(attributes));
      oos.writeFields();
   }
}

