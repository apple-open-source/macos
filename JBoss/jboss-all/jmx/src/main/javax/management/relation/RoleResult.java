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
import java.io.Serializable;

import org.jboss.mx.util.Serialization;

/**
 * Represents the result of multiple access to roles.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.8.1 $
 *
 * <p><b>Revisions:</b>
 * <p><b>20020716 Adrian Brock:</b>
 * <ul>
 * <li> Serialization
 * </ul>
 */
public class RoleResult
  implements Serializable
{
   // Attributes ----------------------------------------------------

   /**
    * The successful roles
    */
   private RoleList roleList;

   /**
    * The unresolved roles.
    */
   private RoleUnresolvedList unresolvedRoleList;

   // Static --------------------------------------------------------

   private static final long serialVersionUID;
   private static final ObjectStreamField[] serialPersistentFields;

   static
   {
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         serialVersionUID = 3786616013762091099L;
         serialPersistentFields = new ObjectStreamField[]
         {
            new ObjectStreamField("myRoleList", RoleList.class),
            new ObjectStreamField("myRoleUnresList", RoleUnresolvedList.class)
         };
         break;
      default:
         serialVersionUID = -6304063118040985512L;
         serialPersistentFields = new ObjectStreamField[]
         {
            new ObjectStreamField("roleList", RoleList.class),
            new ObjectStreamField("unresolvedRoleList", RoleUnresolvedList.class)
         };
      }
   }

   // Constructors --------------------------------------------------

   /**
    * Construct a new role result.
    * 
    * @param roleList the successful roles
    * @param roleUnresolvedList the roles not accessed
    */
   public RoleResult(RoleList roleList, RoleUnresolvedList roleUnresolvedList)
   {
     this.roleList = roleList;
     this.unresolvedRoleList = roleUnresolvedList;
   }

   // Public ---------------------------------------------------------

   /**
    * Retrieve the successful roles.
    * 
    * @return the successful roles.
    */
   public RoleList getRoles()
   {
     return roleList;
   }

   /**
    * Retrieve the unsuccessful roles.
    * 
    * @return the unsuccessful roles.
    */
   public RoleUnresolvedList getRolesUnresolved()
   {
     return unresolvedRoleList;
   }

   /**
    * Set the successful roles.
    * 
    * @param roleList the successful roles.
    */
   public void setRoles(RoleList roleList)
   {
     this.roleList = roleList;
   }

   /**
    * Set the unsuccessful roles.
    * 
    * @param roleUnresolvedList the unsuccessful roles.
    */
   public void setRolesUnresolved(RoleUnresolvedList roleUnresolvedList)
   {
     this.unresolvedRoleList = roleUnresolvedList;
   }

   // Object overrides --------------------------------------------

   public String toString()
   {
      StringBuffer buffer = new StringBuffer("Resolved Roles:\n");
      buffer.append(roleList);
      buffer.append("\nUnresolved Roles\n");
      buffer.append(unresolvedRoleList);
      return buffer.toString();
   }

   // Private -----------------------------------------------------

   private void readObject(ObjectInputStream ois)
      throws IOException, ClassNotFoundException
   {
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         ObjectInputStream.GetField getField = ois.readFields();
         roleList = (RoleList) getField.get("myRoleList", null);
         unresolvedRoleList = (RoleUnresolvedList) getField.get("myRoleUnresList", null);
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
         putField.put("myRoleList", roleList);
         putField.put("myRoleUnresList", unresolvedRoleList);
         oos.writeFields();
         break;
      default:
         oos.defaultWriteObject();
      }
   }
}

