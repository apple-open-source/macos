/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package javax.management.relation;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.ObjectStreamField;
import java.io.Serializable;

import java.util.ArrayList;
import java.util.List;

import javax.management.Notification;
import javax.management.ObjectName;

import org.jboss.mx.util.Serialization;

/**
 * A notification from the relation service.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.4.8.1 $
 *
 * <p><b>Revisions:</b>
 * <p><b>20020715 Adrian Brock:</b>
 * <ul>
 * <li> Serialization
 * </ul>
 */
public class RelationNotification
  extends Notification
  implements Serializable
{
  // Constants -----------------------------------------------------

  /**
   * Creation of an internal relation.
   */
  public static String RELATION_BASIC_CREATION = "jmx.relation.creation.basic";

  /**
   * Removal of an internal relation.
   */
  public static String RELATION_BASIC_REMOVAL = "jmx.relation.removal.basic";

  /**
   * Update of an internal relation.
   */
  public static String RELATION_BASIC_UPDATE = "jmx.relation.update.basic";

  /**
   * Creation of MBean relation added to the relation service.
   */
  public static String RELATION_MBEAN_CREATION = "jmx.relation.creation.mbean";

  /**
   * Removal of MBean relation added to the relation service.
   */
  public static String RELATION_MBEAN_REMOVAL = "jmx.relation.removal.mbean";

  /**
   * Update of MBean relation added to the relation service.
   */
  public static String RELATION_MBEAN_UPDATE = "jmx.relation.update.mbean";

  /**
   * Tag used to identify creation/removal constructor used.
   */
  private static int CREATION_REMOVAL = 0;

  /**
   * Tag used to identify update constructor used.
   */
  private static int UPDATE = 1;
  
  // Attributes ----------------------------------------------------

  /**
   * The MBeans removed when a relation type is removed.
   */
  private List unregisterMBeanList;
  
  /**
   * The new list of object names in the role.
   */
  private List newRoleValue;

  /**
   * The relation's object name.
   */
  private ObjectName relationObjName;
  
  /**
   * The old list of object names in the role.
   */
  private List oldRoleValue;

  /**
   * The relation id of this notification.
   */
  private String relationId;

  /**
   * The relation type name of this notification.
   */
  private String relationTypeName;

  /**
   * The role name of an updated role, only for role updates.
   */
  private String roleName;

  // Static --------------------------------------------------------

   private static final long serialVersionUID;
   private static final ObjectStreamField[] serialPersistentFields;

   static
   {
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         serialVersionUID = -2126464566505527147L;
         serialPersistentFields = new ObjectStreamField[]
         {
            new ObjectStreamField("myNewRoleValue", ArrayList.class),
            new ObjectStreamField("myOldRoleValue", ArrayList.class),
            new ObjectStreamField("myRelId", String.class),
            new ObjectStreamField("myRelObjName", ObjectName.class),
            new ObjectStreamField("myRelTypeName", String.class),
            new ObjectStreamField("myRoleName", String.class),
            new ObjectStreamField("myUnregMBeanList", ArrayList.class)
         };
         break;
      default:
         serialVersionUID = -6871117877523310399L;
         serialPersistentFields = new ObjectStreamField[0];
      }
   }
  
  // Constructors --------------------------------------------------

  /**
   * Construct a new relation notification for a creation or removal.<p>
   *
   * The notification type should be one {@link #RELATION_BASIC_CREATION},
   * {@link #RELATION_BASIC_REMOVAL}, {@link #RELATION_MBEAN_CREATION} or
   * {@link #RELATION_MBEAN_REMOVAL}.<p>
   *
   * The relation type cannot be null, the source cannot be null and it
   * must be a relation service, the relation id cannot be null, the
   * relation type name cannot null.
   *
   * @param type the notification type.
   * @param source the object sending the notification (always the
   *        the relation service).
   * @param sequenceNumber the number identifying the notification
   * @param timeStamp the time of the notification
   * @param message human readable string
   * @param relationId the relation id
   * @param relTypeName the relation type name
   * @param relObjName the relation MBean object name (null
   *        for internal relations)
   * @param unregMBeans the list of object names of mbeans to be
   *        unregistered from the relation service because of a relation
   *        removal. Only relevant for removals, can be null.
   * @exception IllegalArgumentException for null or invalid parameters.
   */
  public RelationNotification(String type, Object source, long sequenceNumber, 
               long timeStamp, String message, String relationId, 
               String relTypeName, ObjectName relObjName, List unregMBeans)
    throws IllegalArgumentException
  {
    super(type, source, sequenceNumber, timeStamp, message);
    init(CREATION_REMOVAL, type, source, sequenceNumber, timeStamp, message, 
         relationId, relTypeName, relObjName, unregMBeans, null, null, null);
  }

  /**
   * Construct a new relation notification for an update.<p>
   *
   * The notification type should be one {@link #RELATION_BASIC_UPDATE},
   * {@link #RELATION_MBEAN_UPDATE}
   *
   * The relation type cannot be null, the source cannot be null and it
   * must be a relation service, the relation id cannot be null, the
   * relation type name cannot null.
   *
   * @param type the notification type.
   * @param source the object sending the notification (always the
   *        the relation service).
   * @param sequenceNumber the number identifying the notification
   * @param timeStamp the time of the notification
   * @param message human readable string
   * @param relationId the relation id
   * @param relTypeName the relation type name
   * @param relObjName the relation MBean object name (null
   *        for internal relations)
   * @param roleName the role name
   * @param newRoleValue the new value of the role
   * @param newRoleValue the old value of the role
   * @exception IllegalArgumentException for null or invalid parameters.
   */
  public RelationNotification(String type, Object source, long sequenceNumber, 
               long timeStamp, String message, String relationId, 
               String relTypeName, ObjectName relObjName, String roleName, 
               List newRoleValue, List oldRoleValue)
    throws IllegalArgumentException
  {
    super(type, source, sequenceNumber, timeStamp, message);
    init(UPDATE, type, source, sequenceNumber, timeStamp, message, relationId,
         relTypeName, relObjName, null, roleName, newRoleValue, oldRoleValue);
  }

  // Public --------------------------------------------------------

  /**
   * Retrieves a list of Object names of the mbeans that will be removed
   * from the relation service because of a relation's removal. This
   * is only relevant for relation removal events.
   *
   * @return the list of removed mbeans.
   */
  public List getMBeansToUnregister()
  {
    if (unregisterMBeanList == null)
      return new ArrayList();
    else
      return new ArrayList(unregisterMBeanList); 
  }
  
  /**
   * Retrieves the new list of object names in the role.
   *
   * @return the new list.
   */
  public List getNewRoleValue()
  {
    if (newRoleValue == null)
      return new ArrayList();
    else
      return new ArrayList(newRoleValue); 
  }

  /**
   * Retrieves the object name of the mbean (null for an internal relation).
   *
   * @return the relation's object name.
   */
  public ObjectName getObjectName()
  {
    return relationObjName;
  }
  
  /**
   * Retrieves the old list of object names in the role.
   *
   * @return the old list.
   */
  public List getOldRoleValue()
  {
    if (oldRoleValue == null)
      return new ArrayList();
    else
      return new ArrayList(oldRoleValue); 
  }

  /**
   * Retrieves the relation id of this notification.
   *
   * @return the relation id.
   */
  public String getRelationId()
  {
    return relationId;
  }

  /**
   * Retrieves the relation type name of this notification.
   *
   * @return the relation type name.
   */
  public String getRelationTypeName()
  {
    return relationTypeName;
  }

  /**
   * Retrieves the role name of an updated role, only for role updates.
   *
   * @return the name of the updated role.
   */
  public String getRoleName()
  {
    return roleName;
  }

  // Notification overrides ----------------------------------------

  // Package protected ---------------------------------------------

  // Protected -----------------------------------------------------

  // Private -------------------------------------------------------

  /**
   * Does most the work for the constructors, see the contructors
   * for details.<p>
   *
   * @param which the constructor called.
   * @param type the notification type.
   * @param source the object sending the notification (always the
   *        the relation service).
   * @param sequenceNumber the number identifying the notification
   * @param timeStamp the time of the notification
   * @param message human readable string
   * @param relationId the relation id
   * @param relTypeName the relation type name
   * @param relObjName the relation MBean object name (null
   *        for internal relations)
   * @param unregMBeans the mbeans unregistered when a relation is removed.
   * @param roleName the role name
   * @param newRoleValue the new value of the role
   * @param newRoleValue the old value of the role
   * @exception IllegalArgumentException for null or invalid parameters.
   */
  private void init(int which, String type, Object source, 
               long sequenceNumber, long timeStamp, String message, 
               String relationId, String relTypeName, ObjectName relObjName,
               List unregMBeans, String roleName, List newRoleValue,  
               List oldRoleValue)
    throws IllegalArgumentException
  {
    // Invalid notification type
    if (type == null)
      throw new IllegalArgumentException("null notification type");
    if (which == CREATION_REMOVAL && type != RELATION_BASIC_CREATION &&
        type != RELATION_BASIC_REMOVAL && type != RELATION_MBEAN_CREATION &&
        type != RELATION_MBEAN_REMOVAL)
      throw new IllegalArgumentException("Invalid creation/removal notifcation");
    if (which == UPDATE && type != RELATION_BASIC_UPDATE && 
        type != RELATION_MBEAN_UPDATE)
      throw new IllegalArgumentException("Invalid update notifcation");

    // Source must be a Relation Service
    if (type == null)
      throw new IllegalArgumentException("null source");

    // REVIEW: According to the spec, this should be a RelationService
    // that doesn't make any sense, it's not serializable
    // I use the object name
    //if ((source instanceof RelationService) == false)
    //  throw new IllegalArgumentException("Source not a relation service");

    // Relation id
    if (relationId == null)
      throw new IllegalArgumentException("null relation id");

    // Relation type name
    if (relTypeName == null)
      throw new IllegalArgumentException("null relation type name");

    // Role Info
    if (which == UPDATE && roleName == null)
      throw new IllegalArgumentException("null role name");

    // New role value
    if (which == UPDATE && newRoleValue == null)
      throw new IllegalArgumentException("null new role value");

    // Old role value
    if (which == UPDATE && oldRoleValue == null)
      throw new IllegalArgumentException("null old role value");

    this.relationId = relationId;
    this.relationTypeName = relTypeName;
    this.relationObjName = relObjName;
    if (unregMBeans != null)
      this.unregisterMBeanList = new ArrayList(unregMBeans);
    if (roleName != null)
      this.roleName = roleName;
    if (newRoleValue != null)
      this.newRoleValue = new ArrayList(newRoleValue);
    if (oldRoleValue != null)
      this.oldRoleValue = new ArrayList(oldRoleValue);
  }

   private void readObject(ObjectInputStream ois)
      throws IOException, ClassNotFoundException
   {
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         ObjectInputStream.GetField getField = ois.readFields();
         newRoleValue = (ArrayList) getField.get("myNewRoleValue", null);
         oldRoleValue = (ArrayList) getField.get("myOldRoleValue", null);
         relationId = (String) getField.get("myRelId", null);
         relationObjName = (ObjectName) getField.get("myRelObjName", null);
         relationTypeName = (String) getField.get("myRelTypeName", null);
         roleName = (String) getField.get("myRoleName", null);
         unregisterMBeanList = (ArrayList) getField.get("myUnregMBeanList", null);
         break;
      default:
         ois.defaultReadObject();
      }
   }

   private void writeObject(ObjectOutputStream oos)
      throws IOException
   {
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         ObjectOutputStream.PutField putField = oos.putFields();
         putField.put("myNewRoleValue", newRoleValue);
         putField.put("myOldRoleValue", oldRoleValue);
         putField.put("myRelId", relationId);
         putField.put("myRelObjName", relationObjName);
         putField.put("myRelTypeName", relationTypeName);
         putField.put("myRoleName", roleName);
         putField.put("myUnregMBeanList", unregisterMBeanList);
         oos.writeFields();
         break;
      default:
         oos.defaultWriteObject();
      }
   }

  // Inner classes -------------------------------------------------
}
