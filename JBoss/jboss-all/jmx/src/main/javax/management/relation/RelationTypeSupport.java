/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.relation;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.ObjectStreamField;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

import org.jboss.mx.util.Serialization;

/**
 * This class can be used to implement relation types.<p>
 *
 * It holds RoleInfo objects for all roles in the relation.
 *
 * <p><b>Revisions:</b>
 * <p><b>20020312 Adrian Brock:</b>
 * <ul>
 * <li>Fixed error handling for getRoleInfo
 * </ul>
 * <p><b>20020715 Adrian Brock:</b>
 * <ul>
 * <li> Serialization
 * </ul>
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.4.6.1 $
 */
public class RelationTypeSupport
  implements RelationType
{
  // Constants ---------------------------------------------------

  // Attributes --------------------------------------------------

  /**
   * The name of the relation type.
   */
  private String name;

  /**
   * The role infos by role name in the relation.
   */
  private HashMap roleInfos;

  // Static ------------------------------------------------------

   private static final long serialVersionUID;
   private static final ObjectStreamField[] serialPersistentFields;

   static
   {
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         serialVersionUID = -8179019472410837190L;
         serialPersistentFields = new ObjectStreamField[]
         {
            new ObjectStreamField("myIsInRelServFlg",  Boolean.TYPE),
            new ObjectStreamField("myRoleName2InfoMap", HashMap.class),
            new ObjectStreamField("myTypeName", String.class),
         };
         break;
      default:
         serialVersionUID = 4611072955724144607L;
         serialPersistentFields = new ObjectStreamField[]
         {
            new ObjectStreamField("isInRelationService",  Boolean.TYPE),
            new ObjectStreamField("roleName2InfoMap", HashMap.class),
            new ObjectStreamField("typeName", String.class),
         };
      }
   }

  // Constructors ------------------------------------------------

  /**
   * Create a relation type with a name but no role infos.<p>
   *
   * WARNING: No check is made on the arguments.
   *
   * @param name the relation type name.
   */
  protected RelationTypeSupport(String name)
  {
    this.name = name;
    roleInfos = new HashMap();
  }

  /**
   * Create a relation type with a name and the passed role infos.<p>
   *
   * A relation type is invalid if the same name is used in two
   * different role infos, no role information is provided or a null
   * role is passed.
   *
   * @param name the relation type name.
   * @param roleInfos an array of role info objects.
   * @exception IllegalArgumentException for null parameters
   * @exception InvalidRelationTypeException for an invalid relation
   */
  public RelationTypeSupport(String name, RoleInfo[] infos)
    throws IllegalArgumentException, InvalidRelationTypeException
  {
    if (name == null)
      throw new IllegalArgumentException("Null name");
    if (infos == null)
      throw new IllegalArgumentException("No role information");
    if (infos.length == 0)
      throw new InvalidRelationTypeException("No role information");
    this.name = name;
    // Check and store the role information
    roleInfos = new HashMap();
    for (int i = 0; i < infos.length; i++)
    {
      if (infos[i] == null)
        throw new InvalidRelationTypeException("Null role");
      if (roleInfos.containsKey(infos[i].getName()))
        throw new InvalidRelationTypeException(
                  "Duplicate role name" + infos[i].getName());
      roleInfos.put(infos[i].getName(), infos[i]);
    }
  }

  // Public ------------------------------------------------------

  // Relation Type Implementation --------------------------------

  public String getRelationTypeName()
  {
    return name;
  }

  public List getRoleInfos()
  {
    return new ArrayList(roleInfos.values());
  }

  public RoleInfo getRoleInfo(String roleInfoName)
    throws RoleInfoNotFoundException
  {
    if (roleInfoName == null)
       throw new IllegalArgumentException("Null role info name");
    RoleInfo result = (RoleInfo) roleInfos.get(roleInfoName);
    // REVIEW: The spec is contradictory here it says throw an
    // exception and return null????
    if (result == null)
      throw new RoleInfoNotFoundException(roleInfoName);
    return result;
  }

  // Protected ---------------------------------------------------

  /**
   * Add a role information object to the relation type.
   *
   * @param roleInfos an array of role info objects.
   * @exception IllegalArgumentException for null parameters
   * @exception InvalidRelationTypeException for a duplicate role name.
   */
  protected void addRoleInfo(RoleInfo roleInfo)
    throws IllegalArgumentException, InvalidRelationTypeException
  {
    if (roleInfo == null)
      throw new IllegalArgumentException("No role information");

    // Check for a duplciate role name.
    String newName = roleInfo.getName();
    if (roleInfos.containsKey(newName))
       throw new InvalidRelationTypeException("Duplicate role name");
    roleInfos.put(newName, roleInfo);
  }

  // Private -----------------------------------------------------

   private void readObject(ObjectInputStream ois)
      throws IOException, ClassNotFoundException
   {
      ObjectInputStream.GetField getField = ois.readFields();
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         roleInfos = (HashMap) getField.get("myRoleName2InfoMap", null);
         name = (String) getField.get("myTypeName", null);
         break;
      default:
         roleInfos = (HashMap) getField.get("roleName2InfoMap", null);
         name = (String) getField.get("typeName", null);
      }
   }

   private void writeObject(ObjectOutputStream oos)
      throws IOException
   {
      ObjectOutputStream.PutField putField = oos.putFields();
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         putField.put("myTypeName", name);
         putField.put("myRoleName2InfoMap", roleInfos);
         break;
      default:
         putField.put("typeName", name);
         putField.put("roleName2InfoMap", roleInfos);
      }
      oos.writeFields();
   }
}
