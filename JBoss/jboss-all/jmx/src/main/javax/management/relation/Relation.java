/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.relation;

import java.util.List;
import java.util.Map;

import javax.management.ObjectName;

/**
 * This interface is implemented by an MBean that represents a relation.<p>
 *
 * Relations with only roles can be created by the relation service using
 * a {@link RelationSupport} object.<p>
 *
 * More complicated relations have to implemented manually. The
 * {@link RelationSupport} can be used to help in the implementation.<p>
 *
 * Any properties or methods must be exposed for management by the
 * implementing MBean.
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.2 $
 *
 */
public interface Relation
{
   // Constants ---------------------------------------------------

   // Public ------------------------------------------------------

   /**
    * Retrieves all the roles in this relation.
    *
    * @return the roles both resolved and unresolved.
    * @exception RelationServiceNotRegisteredException when the relation service
    *            is not registered with an MBeanServer.
    */
   public RoleResult getAllRoles()
     throws RelationServiceNotRegisteredException;

   /**
    * Retrieves MBeans referenced by roles of this relation.<p>
    *
    * The return value is a map keyed by MBean object names. The objects
    * are associated with an ArrayList that contains all the role names
    * the MBean has within this relation.
    *
    * @return the map of object names and their roles.
    */
   public Map getReferencedMBeans();

   /**
    * Retrieves the relation id used to identify the relation within
    * the relation service.
    *
    * @return the unique id.
    */
   public String getRelationId();

   /**
    * Retrieves the object name of the relation service this relation
    * is registered with.
    *
    * @return the relation service object name.
    */
   public ObjectName getRelationServiceName();

   /**
    * Retrieves the relation type for this relation.
    *
    * @return the relation type.
    */
   public String getRelationTypeName();

   /**
    * Retrieves the role for the passed role name. The role
    * must exist and be readable.<p>
    *
    * The return value is an ArrayList of object names in the role.
    *
    * @param the role name.
    * @return the role.
    * @exception IllegalArgumentException for a null role name.
    * @exception RoleNotFoundException when there is no such role or
    *            it is not readable.
    * @exception RelationServiceNotRegisteredException when the relation service
    *            is not registered with an MBeanServer.
    */
   public List getRole(String roleName)
     throws IllegalArgumentException, RoleNotFoundException, 
            RelationServiceNotRegisteredException;

   /**
    * Retrieves the number of MBeans in a given role.
    *
    * @param the role name.
    * @return the number of MBeans.
    * @exception IllegalArgumentException for a null role name.
    * @exception RoleNotFoundException when there is no such role.
    */
   public Integer getRoleCardinality(String roleName)
     throws IllegalArgumentException, RoleNotFoundException; 

   /**
    * Retrieves the roles in this relation with the passed names.
    *
    * @param roleNames an array of role names
    * @return the roles both resolved and unresolved.
    * @exception IllegalArgumentException for a null role names.
    * @exception RelationServiceNotRegisteredException when the relation service
    *            is not registered with an MBeanServer.
    */
   public RoleResult getRoles(String[] roleNames)
     throws IllegalArgumentException, RelationServiceNotRegisteredException;

   /**
    * The relation service will call this service when an MBean
    * referenced in a role is unregistered.<p>
    *
    * The object name should be removed from the role.<p>
    *
    * <b>Calling this method manually may result in incorrect behaviour</b>
    *
    * @param objectName the object name unregistered.
    * @param roleName the role the containing the object.
    * @exception RoleNotFoundException if the role does exist or it is not
    *            writable.
    * @exception InvalidRoleValueException when the role does not conform
    *            to the associated role info.
    * @exception RelationServiceNotRegisteredException when the relation service
    *            is not registered with an MBeanServer.
    * @exception RelationTypeNotFoundException when the relation type has
    *            not been registered in the relation service.
    * @exception RelationNotFoundException when this method is called for
    *            for an MBean not registered with the relation service.
    */
   public void handleMBeanUnregistration(ObjectName objectName, String roleName)
     throws RoleNotFoundException, InvalidRoleValueException,
            RelationServiceNotRegisteredException, RelationTypeNotFoundException,
            RelationNotFoundException;

   /**
    * Retrieve all the roles in this relation without checking the role mode.
    *
    * @return the list of roles.
    */
   public RoleList retrieveAllRoles();

   /**
    * Sets the passed role for this relation.<p>
    *
    * The role is checked according to its role definition in the relation type.
    * The role is not valid if there are the wrong number of MBeans, an MBean
    * is of an incorrect class or an MBean does not exist.<p>
    *
    * The notification <i>RELATION_BASIC_UPDATE</i> is sent when the relation is
    * not an MBean or <i>RELATION_MBEAN_UPDATE</i> when it is.<p>
    *
    * @param role the new role.
    * @exception IllegalArgumentException for a null role.
    * @exception InvalidRoleValueException if the role is not valid.
    * @exception RoleNotFoundException if the role is not writable.
    *            This test is not performed at initialisation. 
    * @exception RelationServiceNotRegisteredException when the relation service
    *            is not registered with an MBeanServer.
    * @exception RelationTypeNotFoundException when the relation type has
    *            not been registered in the relation service.
    * @exception RelationNotFoundException when this method is called for
    *            for an MBean not registered with the relation service.
    */
   public void setRole(Role role)
     throws IllegalArgumentException, InvalidRoleValueException,
            RoleNotFoundException, RelationServiceNotRegisteredException,
            RelationTypeNotFoundException, RelationNotFoundException;

   /**
    * Sets the roles.<p>
    *
    * The roles are checked according to its role definition in the relation type.
    * The role is not valid if there are the wrong number of MBeans, an MBean
    * is of an incorrect class or an MBean does not exist.<p>
    *
    * A notification <i>RELATION_BASIC_UPDATE</i> is sent when the relation is
    * not an MBean or <i>RELATION_MBEAN_UPDATE</i> when it is for every updated
    * role.<p>
    *
    * The return role result has a role list for successfully updated roles and
    * an unresolved list for roles not set.
    *
    * @param roleList the new roles.
    * @return the resulting role result.
    * @exception IllegalArgumentException for a null role name.
    * @exception RelationServiceNotRegisteredException when the relation service
    *            is not registered with an MBeanServer.
    * @exception RelationTypeNotFoundException when the relation type has
    *            not been registered in the relation service.
    * @exception RelationNotFoundException when this method is called for
    *            for an MBean not registered with the relation service.
    */
   public RoleResult setRoles(RoleList roleList)
     throws IllegalArgumentException, RelationServiceNotRegisteredException,
            RelationTypeNotFoundException, RelationNotFoundException;
}
