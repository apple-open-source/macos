/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * A notification sent by the MBeanServer delegate when an MBean is
 * registered or unregisterd.<p>
 *
 * NOTE: The values from the spec are wrong, the real values are<b>
 * REGISTRATION_NOTIFICATION = "JMX.mbean.registered"<b>
 * UNREGISTRATION_NOTIFICATION = "JMX.mbean.registered"
 *
 * <p><b>Revisions:</b>
 * <p><b>20020315 Adrian Brock:</b>
 * <ul>
 * <li>Spec has wrong values for notification values
 * </ul>
 * <p><b>20020711 Adrian Brock:</b>
 * <ul>
 * <li> Serialization </li>
 * </ul>
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.2.6.1 $
 */
public class MBeanServerNotification
   extends Notification
{
   // Constants ---------------------------------------------------

   private static final long serialVersionUID = 2876477500475969677L;

   /**
    * Notification type sent at MBean registration
    */   
   public static final java.lang.String REGISTRATION_NOTIFICATION   = "JMX.mbean.registered";

   /**
    * Notification type sent at MBean registration
    */   
   public static final java.lang.String UNREGISTRATION_NOTIFICATION = "JMX.mbean.unregistered";

   // Attributes --------------------------------------------------

   /**
    * The object name of the mbean being (un)registered
    */
   private ObjectName objectName = null;

   // Static ------------------------------------------------------

   // Constructors ------------------------------------------------

   /**
    * Construct a new MBeanServer notification
    *
    * @param type the type of notification to construct
    * @param source the source of the notification
    * @param sequence the sequence number of the notification
    * @param objectName the object name of the mbean being (un)registered
    */
   public MBeanServerNotification(String type, Object source, 
                                  long sequence, ObjectName objectName)
   {
      super(type, source, sequence);
      this.objectName = objectName;
   }

   // Public ------------------------------------------------------
   
   /**
    * Get the object name of the mbean being (un)registered
    *
    * @return the object name
    */
   public ObjectName getMBeanName()
   {
      return objectName;
   }

   /**
    * @return human readable string.
    */
   public String toString()
   {
      StringBuffer buffer = new StringBuffer(50);
      buffer.append(getClass().getName()).append(":");
      buffer.append(" notificationType=").append(getType());
      buffer.append(" source=").append(getSource());
      buffer.append(" seq-no=").append(getSequenceNumber());
      buffer.append(" time=").append(getTimeStamp());
      buffer.append(" message=").append(getMessage());
      buffer.append(" objectName=").append(getMBeanName());
      buffer.append(" userData=").append(getUserData());
      return buffer.toString();
   }

   // X Implementation --------------------------------------------

   // Y Overrides -------------------------------------------------

   // Protected ---------------------------------------------------

   // Private -----------------------------------------------------

   // Inner classes -----------------------------------------------
}
