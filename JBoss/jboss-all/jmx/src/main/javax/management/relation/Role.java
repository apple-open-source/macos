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

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import org.jboss.mx.util.Serialization;

/**
 * A role is a role name and an ordered list of object names to
 * the MBeans in the role.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.4.6.2 $
 *
 * <p><b>Revisions:</b>
 * <p><b>20020716 Adrian Brock:</b>
 * <ul>
 * <li> Serialization
 * </ul>
 */
public class Role
  implements Serializable
{
   // Attributes ----------------------------------------------------

   /**
    * The role name
    */
   private String name;

   /**
    * An ordered list of MBean object names.
    */
   private List objectNameList;

   // Static --------------------------------------------------------

   private static final long serialVersionUID;
   private static final ObjectStreamField[] serialPersistentFields;

   static
   {
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         serialVersionUID = -1959486389343113026L;
         serialPersistentFields = new ObjectStreamField[]
         {
            new ObjectStreamField("myName",  String.class),
            new ObjectStreamField("myObjNameList", List.class)
         };
         break;
      default:
         serialVersionUID = -279985518429862552L;
         serialPersistentFields = new ObjectStreamField[]
         {
            new ObjectStreamField("name",  String.class),
            new ObjectStreamField("objectNameList", List.class)
         };
      }
   }

   /**
    * Formats the role value for output.<p>
    *
    * The spec says it should be a comma separated list of object names.
    * But the RI uses new lines which makes more sense for object names.
    *
    * @param roleValue the role value to print
    * @return the string representation
    * @exception IllegalArgumentException for null value.
    */
   public static String roleValueToString(List roleValue)
     throws IllegalArgumentException
   {
     if (roleValue == null)
       throw new IllegalArgumentException("null roleValue");
     StringBuffer buffer = new StringBuffer();
     for (int i = 0; i < roleValue.size(); i++)
     {
       buffer.append(roleValue.get(i));
       if (i + 1 < roleValue.size())
         buffer.append("\n");
     }
     return buffer.toString();
   }

   // Constructors --------------------------------------------------

   /**
    * Construct a new role.<p>
    *
    * No validation is performed until the role is set of in a
    * relation. Passed parameters must not be null.<p>
    * 
    * The passed list must be an ArrayList.
    *
    * @param roleName the role name
    * @param roleValue the MBean object names in the role
    * @exception IllegalArgumentException for null values.
    */
   public Role(String roleName, List roleValue)
     throws IllegalArgumentException
   {
     setRoleName(roleName);
     setRoleValue(roleValue); 
   }

   // Public ---------------------------------------------------------

   /**
    * Retrieve the role name.
    * 
    * @return the role name.
    */
   public String getRoleName()
   {
     return name;
   }

   /**
    * Retrieve the role value.
    * 
    * @return a list of MBean object names.
    */
   public List getRoleValue()
   {
     return new ArrayList(objectNameList);
   }

   /**
    * Set the role name.
    * 
    * @param roleName the role name.
    * @exception IllegalArgumentException for a null value
    */
   public void setRoleName(String roleName)
     throws IllegalArgumentException
   {
     if (roleName == null)
       throw new IllegalArgumentException("Null roleName");
     name = roleName;
   }

   /**
    * Set the role value it must be an ArrayList.
    * A list of mbean object names.
    * 
    * @param roleValue the role value.
    * @exception IllegalArgumentException for a null value or not an
    *            array list
    */
   public void setRoleValue(List roleValue)
     throws IllegalArgumentException
   {
     if (roleValue == null)
       throw new IllegalArgumentException("Null roleValue");
     objectNameList = new ArrayList(roleValue);
   }

   // Object Overrides -------------------------------------------------

   /**
    * Clones the object.
    *
    * @todo fix this not to use the copy constructor
    *
    * @return a copy of the role
    * @throws CloneNotSupportedException
    */
   public synchronized Object clone()
   {
      return new Role(name, objectNameList);
/*      try
      {
         Role clone = (Role) super.clone();
         clone.name = this.name;
         clone.objectNameList = new ArrayList(this.objectNameList);
         return clone;
      }
      catch (CloneNotSupportedException e)
      {
         throw new RuntimeException(e.toString());
      }
*/  }

   /**
    * Formats the role for output.
    *
    * @return a human readable string
    */
   public synchronized String toString()
   {
     StringBuffer buffer = new StringBuffer("Role Name (");
     buffer.append(name);
     buffer.append(") Object Names (");

     for (int i = 0; i < objectNameList.size(); i++)
     {
       buffer.append(objectNameList.get(i));
       if (i + 1 < objectNameList.size())
         buffer.append(" & ");
     }
     buffer.append(")");
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
         name = (String) getField.get("myName", null);
         objectNameList = (List) getField.get("myObjNameList", null);
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
         putField.put("myName", name);
         putField.put("myObjNameList", objectNameList);
         oos.writeFields();
         break;
      default:
         oos.defaultWriteObject();
      }
   }
}

