/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.relation;

import java.util.List;
import java.util.Map;

import javax.management.InstanceNotFoundException;
import javax.management.ObjectName;

/**
 * This interface defines the management interface for a relation
 * service. <p>
 *
 * The relation service performs the following functions:<br>
 * Creating and deleting relation types and relations.<br>
 * Making sure relations are consistent as defined by information
 * in their relation types/roles.<br>
 * Allowing relations to be queried.
 *
 * <p><b>Revisions:</b>
 * <p><b>20020312 Adrian Brock:</b>
 * <ul>
 * <li>Fixed wrong exception types thrown
 * <li>Add missing method getRoles(String, String[])
 * </ul>
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.5 $
 */
public interface RelationServiceMBean
{
   // Constants ---------------------------------------------------

   // Public ------------------------------------------------------

   /**
    * Add a manually created relation to the relation service. It must
    * be registered with the same MBeanService as the relation service.
    *
    * A RELATION_MBEAN_CREATION notification is sent.
    *
    * @param relation the object name of the relation
    * @exception IllegalArgumentException for a null object name.
    * @exception NoSuchMethodException if the mbean does not implement the
    *            Relation interface.
    * @exception RelationServiceNotRegisteredException when the relation
    *            service is not registered with the MBeanServer.
    * @exception InstanceNotFoundException when the relation is not
    *            registered in the MBeanServer.
    * @exception InvalidRelationIdException if the relation id 
    *            is already used by another relation.
    * @exception InvalidRelationServiceException if the relation service in 
    *            the relation is null or is not the relation service to which
    *            it is being added.
    * @exception RelationTypeNotFoundException if there is no relation type in
    *            the relation or the relation type has not been registered 
    *            with the relation service.
    * @exception InvalidRoleValueException if the number of MBeans in a
    *            role is outside the bounds of the RoleInfo defined in the
    *            relation type, one of the MBeans is not of the correct
    *            class or an MBean does not exist.
    * @exception RoleNotFoundException if a role in the relation is not
    *            in the relation type.
    */
   public void addRelation(ObjectName relation)
     throws IllegalArgumentException, NoSuchMethodException,
            RelationServiceNotRegisteredException, InvalidRelationIdException,
            InvalidRelationServiceException, RelationTypeNotFoundException,
            InvalidRoleValueException, RoleNotFoundException,
            InstanceNotFoundException;

   /**
    * Add a relation type to the relation service. 
    *
    * @param relationType the relation type.
    * @exception IllegalArgumentException for a null relation type.
    * @exception InvalidRelationTypeException if a relation type already
    *            exists in the relation service with the given name.
    */
   public void addRelationType(RelationType relationType)
     throws IllegalArgumentException, InvalidRelationTypeException;

   /**
    * Checks whether the passed role can be read in the given
    * relation type.<p>
    *
    * The return value is either zero when readable or a value from 
    * {@link RoleStatus}.
    *
    * @param roleName the name of the role to check.
    * @param relationTypeName the relation type to check.
    * @return the result described above.
    * @exception IllegalArgumentException for a null parameters.
    * @exception RelationTypeNotFoundException if the relation type does
    *            exist in the relation service.
    */
   public Integer checkRoleReading(String roleName, String relationTypeName)
     throws IllegalArgumentException, RelationTypeNotFoundException;

   /**
    * Checks whether the passed role can be written in the given
    * relation type.<p>
    *
    * The return value is either zero when writable or a value from 
    * {@link RoleStatus}.
    *
    * @param role the role to check.
    * @param relationTypeName the relation type to check.
    * @param initFlag write access is not check when this flag is true.
    * @return the result described above.
    * @exception IllegalArgumentException for a null parameters.
    * @exception RelationTypeNotFoundException if the relation type does
    *            exist in the relation service.
    */
   public Integer checkRoleWriting(Role role, String relationTypeName,
                                   Boolean initFlag)
     throws IllegalArgumentException, RelationTypeNotFoundException;

   /**
    * Create a simple relation using {@link RelationSupport} for a relation
    * type within the relation service.<p>
    *
    * Roles not initialised are set to an empty ArrayList.<p>
    *
    * A RELATION_BASIC_CREATION notification is sent.
    *
    * @param relationId the relation id of the relation
    * @param relationTypeName the relation type of the relation
    * @param roleList the roles to initialise in the relation (can be null)
    * @exception IllegalArgumentException for a null parameter.
    * @exception RelationServiceNotRegisteredException when the relation
    *            service is not registered with the MBeanServer.
    * @exception InvalidRelationIdException if the relation id 
    *            is already used by another relation.
    * @exception RelationTypeNotFoundException if there is no relation type in
    *            the relation or the relation type has not been registered 
    *            with the relation service.
    * @exception InvalidRoleValueException if the number of MBeans in a
    *            role is outside the bounds of the RoleInfo defined in the
    *            relation type, one of the MBeans is not of the correct
    *            class, an MBean does not exist, the same role name is
    *            used in two different relations.
    * @exception RoleNotFoundException if a role in the relation is not
    *            in the relation type.
    */
   public void createRelation(String relationId, String relationTypeName,
                              RoleList roleList)
     throws IllegalArgumentException,
            RelationServiceNotRegisteredException, InvalidRelationIdException,
            RelationTypeNotFoundException, InvalidRoleValueException, 
            RoleNotFoundException;

   /**
    * Create a relation type within the relation service.
    *
    * @param relationTypeName the relation type name.
    * @param roleInfos an array of role infos.
    * @exception IllegalArgumentException for a null relation type.
    * @exception InvalidRelationTypeException if a relation type already
    *            exists in the relation service with the given name, there
    *            are problems with the role infos.
    */
   public void createRelationType(String relationTypeName,
                                  RoleInfo[] roleInfos)
     throws IllegalArgumentException, InvalidRelationTypeException;

   /**
    * Retrieves MBeans associated with the passed MBean in the passed
    * relation type and role.
    *
    * @param mbeanName the name of the reference MBean
    * @param relationTypeName the relation type, null means check all
    *        relation types.
    * @param roleName the role, null means check all roles.
    * @return a HashMap with keys of the related MBeans with the
    *         value for each MBean an ArrayList of Relation Types.
    * @exception IllegalArgumentException for a null object name.
    */
   public Map findAssociatedMBeans(ObjectName mbeanName, 
                                   String relationTypeName, String roleName)
     throws IllegalArgumentException;

   /**
    * Retrieves MBeans referencing the passed MBean in the passed
    * relation type and role.
    *
    * @param mbeanName the name of the referenced MBean
    * @param relationTypeName the relation type, null means check all
    *        relation types.
    * @param roleName the role, null means check all roles.
    * @return a HashMap with keys of the referencing MBeans with the
    *         value for each MBean an ArrayList of Relation Types.
    * @exception IllegalArgumentException for a null object name.
    */
   public Map findReferencingRelations(ObjectName mbeanName, 
                                        String relationTypeName, String roleName)
     throws IllegalArgumentException;

   /**
    * Retrieves the relation ids for relations of the passed type.
    *
    * @param relationTypeName the relation type.
    * @return an ArrayList of relation ids.
    * @exception IllegalArgumentException for a null relation type name.
    * @exception RelationTypeNotFoundException if there is no relation type 
    *            with the passed name.
    */
   public List findRelationsOfType(String relationTypeName)
     throws IllegalArgumentException, RelationTypeNotFoundException;

   /**
    * Retrieves all the relation ids.
    *
    * @return an ArrayList of relation ids.
    */
   public List getAllRelationIds();

   /**
    * Retrieves all the relation type names.
    *
    * @return an ArrayList of relation type names
    */
   public List getAllRelationTypeNames();

   /**
    * Retrieves all the roles for a given relation id.
    *
    * @param relationId the relation id
    * @return a RoleResult with a RoleList for readable roles and a
    *         RoleUnresolvedList for roles that are not readable.
    * @exception IllegalArgumentException for a null relation id.
    * @exception RelationNotFoundException when the relation id does not
    *            exist.
    * @exception RelationServiceNotRegisteredException when the relation
    *            service is not registered with an MBeanServer.
    */
   public RoleResult getAllRoles(String relationId)
     throws IllegalArgumentException, RelationNotFoundException,
            RelationServiceNotRegisteredException;

   /**
    * Retrieves the purge flag. This flag controls whether all relations
    * have to validated when notification is received of an MBeans
    * unregistration or whether the purgeRelations() method has to be
    * called.
    *
    * @return true for an immediate purge on unregistration, false otherwise.
    */
   public boolean getPurgeFlag();

   /**
    * Retrieves all the MBeans referenced in all the roles of a
    * relation.
    *
    * @param relationId the relation id
    * @return a HashMap with a key of the MBeans and the values an
    *         array list of the role names for each MBean.
    * @exception IllegalArgumentException for a null relation id.
    * @exception RelationNotFoundException when the relation id does not
    *            exist.
    */
   public Map getReferencedMBeans(String relationId)
     throws IllegalArgumentException, RelationNotFoundException;

   /**
    * Retrieves the relation type name for the passed relation.
    *
    * @param relationId the relation id
    * @return the relation type name.
    * @exception IllegalArgumentException for a null relation id.
    * @exception RelationNotFoundException when the relation id does not
    *            exist.
    */
   public String getRelationTypeName(String relationId)
     throws IllegalArgumentException, RelationNotFoundException;

   /**
    * Retrieves MBeans in a role for a given relation id.
    *
    * @param relationId the relation id
    * @param roleName the role name
    * @return an ArrayList of object names for mbeans in the role.
    * @exception IllegalArgumentException for a null parameter.
    * @exception RelationNotFoundException when the relation id does not
    *            exist.
    * @exception RelationServiceNotRegisteredException when the relation 
    *            service is not registered with an MBeanServer.
    * @exception RoleNotFoundException when the role does not exist or
    *            is not readable.
    */
   public List getRole(String relationId, String roleName)
     throws IllegalArgumentException, RelationNotFoundException,
            RelationServiceNotRegisteredException, RoleNotFoundException;

   /**
    * Retrieves the number of MBeans in a role for a given relation id.
    *
    * @param relationId the relation id
    * @param roleName the role name
    * @return the number of mbeans in the role
    * @exception IllegalArgumentException for a null parameter.
    * @exception RelationNotFoundException when the relation id does not
    *            exist.
    * @exception RoleNotFoundException when the role does not exist or
    *            is not readable.
    */
   public Integer getRoleCardinality(String relationId, String roleName)
     throws IllegalArgumentException, RelationNotFoundException,
            RoleNotFoundException;

   /**
    * Retrieves the role information for a given relation type.
    *
    * @param relationTypeName the relation type name
    * @param roleInfoName the role information name
    * @return the role information.
    * @exception IllegalArgumentException for a null parameter.
    * @exception RelationTypeNotFoundException when the relation type does not
    *            exist.
    * @exception RoleInfoNotFoundException when the role information does not
    *            exist for the relation type.
    */
   public RoleInfo getRoleInfo(String relationTypeName, String roleInfoName)
     throws IllegalArgumentException, RelationTypeNotFoundException,
            RoleInfoNotFoundException;

   /**
    * Retrieves all the role information for a given relation type.
    *
    * @param relationTypeName the relation type name
    * @return an ArrayList of role information.
    * @exception IllegalArgumentException for a null parameter.
    * @exception RelationTypeNotFoundException when the relation id does not
    *            exist.
    */
   public List getRoleInfos(String relationTypeName)
     throws IllegalArgumentException, RelationTypeNotFoundException;

   /**
    * Retrieves selected roles for a relation.
    *
    * @param relationId the relation id
    * @param roleNames an array of role name
    * @return a RoleResult containing resolved and unresolved roles.
    * @exception IllegalArgumentException for a null parameter.
    * @exception RelationNotFoundException when the relation id does not
    *            exist.
    * @exception RelationServiceNotRegisteredException when the relation 
    *            service is not registered with an MBeanServer.
    */
   public RoleResult getRoles(String relationId, String[] roleNames)
     throws IllegalArgumentException, RelationNotFoundException,
            RelationServiceNotRegisteredException;

   /**
    * Checks whether the relation service has the passed relation id.
    *
    * @param relationId the relation id
    * @return true when it has the relationId, false otherwise.
    * @exception IllegalArgumentException for a null parameter.
    */
   public Boolean hasRelation(String relationId)
     throws IllegalArgumentException;

   /**
    * Checks whether the relation service is active, i.e. it is registered
    * with an MBeanServer.
    *
    * @exception RelationServiceNotRegisteredException when the relation 
    *            service is not registered with an MBeanServer.
    */
   public void isActive()
     throws RelationServiceNotRegisteredException;

   /**
    * Checks whether the passed object name is a relation in this relation
    * service.
    *
    * @param objectName the name of the MBean to check
    * @return the relation id the MBean is registered as, or null
    *         when not registered.
    * @exception IllegalArgumentException for a null object name.
    */
   public String isRelation(ObjectName objectName)
     throws IllegalArgumentException;

   /**
    * Checks whether the passed relation id is an MBean created by a user
    * or has been internally created by the relation service.
    *
    * @param relationId the relation id to check
    * @return the object name of the MBean when it is externally created,
    *         null otherwise.
    * @exception IllegalArgumentException for a null object name.
    * @exception RelationNotFoundException when the relation id does not
    *            exist.
    */
   public ObjectName isRelationMBean(String relationId)
     throws IllegalArgumentException, RelationNotFoundException;

   /**
    * Purge relations. This method is called automatically when the purge
    * flag is true and an MBean in a relation is unregistered.<p>
    *
    * Not purging relations automatically can lead to problems when the
    * same object name is reused.<p>
    *
    * If the unregistration causes a role to go below its minimal cardinality,
    * the relation is removed. Otherwise the relation's 
    * handleMBeanUnregistration() is called.
    *
    * @exception RelationServiceNotRegisteredException when the relation 
    *            service is not registered with an MBeanServer.
    */
   public void purgeRelations()
     throws RelationServiceNotRegisteredException;

   /**
    * Removes a relation from the relation service.<p>
    *
    * For internally created relations, a RELATION_BASIC_REMOVAL
    * notification is sent.<p>
    *
    * For externally created relations, a RELATION_MBEAN_REMOVAL
    * notification is sent.<p>
    *
    * The MBeans referenced in the relation are unaffected.
    *
    * @param relationId the relation id
    * @exception IllegalArgumentException for a null parameter.
    * @exception RelationNotFoundException when the relation id does not
    *            exist.
    * @exception RelationServiceNotRegisteredException when the relation 
    *            service is not registered with an MBeanServer.
    */
   public void removeRelation(String relationId)
     throws IllegalArgumentException, RelationNotFoundException,
            RelationServiceNotRegisteredException;

   /**
    * Removes a relation type from the relation service.<p>
    *
    * Any relations using this relation type are also removed.
    *
    * @param relationTypeName the relation type name
    * @exception IllegalArgumentException for a null parameter.
    * @exception RelationTypeNotFoundException when the relation type does not
    *            exist.
    * @exception RelationServiceNotRegisteredException when the relation 
    *            service is not registered with an MBeanServer.
    */
   public void removeRelationType(String relationTypeName)
     throws IllegalArgumentException, RelationTypeNotFoundException,
            RelationServiceNotRegisteredException;

   /**
    * Sends a relation creation notification
    *
    * For internally created relations, a RELATION_BASIC_CREATION
    * notification is sent.<p>
    *
    * For externally created relations, a RELATION_MBEAN_CREATION
    * notification is sent.<p>
    *
    * The source is this relation service.<p>
    *
    * This method is called by addRelation() and createRelation()
    *
    * @param relationId the relation id
    * @exception IllegalArgumentException for a null parameter.
    * @exception RelationNotFoundException when the relation id does not
    *            exist.
    */
   public void sendRelationCreationNotification(String relationId)
     throws IllegalArgumentException, RelationNotFoundException;

   /**
    * Sends a relation removal notification
    *
    * For internally created relations, a RELATION_BASIC_REMOVAL
    * notification is sent.<p>
    *
    * For externally created relations, a RELATION_MBEAN_REMOVAL
    * notification is sent.<p>
    *
    * The source is this relation service.<p>
    *
    * This method is called by removeRelation()
    *
    * @param relationId the relation id
    * @param unregMBeans a list of MBeans to be unregistered due to this
    *        removal (can be null)
    * @exception IllegalArgumentException for a null parameter.
    * @exception RelationNotFoundException when the relation id does not
    *            exist.
    */
   public void sendRelationRemovalNotification(String relationId, 
                                               List unregMBeans)
     throws IllegalArgumentException, RelationNotFoundException;

   /**
    * Sends a relation update notification
    *
    * For internally created relations, a RELATION_BASIC_UPDATE
    * notification is sent.<p>
    *
    * For externally created relations, a RELATION_MBEAN_UPDATE
    * notification is sent.<p>
    *
    * The source is this relation service.<p>
    *
    * This method is called from the RelationSupport setRole() and
    * setRoles() methods.
    *
    * @param relationId the relation id
    * @param newRole the new role
    * @param oldRoleValue a list of MBeans in the old role
    * @exception IllegalArgumentException for a null parameter.
    * @exception RelationNotFoundException when the relation id does not
    *            exist.
    */
   public void sendRoleUpdateNotification(String relationId,
                                          Role newRole, List oldRoleValue)
     throws IllegalArgumentException, RelationNotFoundException;

   /**
    * Sets the purge flag. This flag controls whether all relations
    * have to validated when notification is received of an MBeans
    * unregistration or whether the purgeRelations() method has to be
    * called.
    *
    * @param value true for an immediate purge on unregistration, false 
    *        otherwise.
    */
   public void setPurgeFlag(boolean value);

   /**
    * Sets the role in the passed relation.<p>
    *
    * The role will be validated according to information in the relation
    * type.<p>
    *
    * The relation service will keep track the MBeans unregistration
    * to maintain the consistency of the relation.
    *
    * @param relationId the relation to change the role for.
    * @param role the new role
    * @param IllegalArgumentException for null parameters
    * @param RelationServiceNotRegisteredException when the relation service
    *        has not been registered with an MBeanServer
    * @param RelationNotFoundException when the relation does not exist in
    *        the relation service.
    * @param RoleNotFoundException when this is an internal relation or
    *        the role does not exist or it is not writable.
    * @param InvalidRoleValueException when the role is not valid according
    *        to information in the relation type.
    * @param RelationTypeNotFoundException if the relation type is not known.
    */
   public void setRole(String relationId, Role role)
     throws IllegalArgumentException, RelationServiceNotRegisteredException,
            RelationNotFoundException, RoleNotFoundException,
            InvalidRoleValueException, RelationTypeNotFoundException;

   /**
    * Sets the roles in the passed relation.<p>
    *
    * The roles will be validated according to information in the relation
    * type.<p>
    *
    * The relation service will keep track the MBeans unregistration
    * to maintain the consistency of the relation.
    *
    * @param relationId the relation to change the role for.
    * @param roles the list of roles
    * @param a RoleResult
    * @param IllegalArgumentException for null parameters
    * @param RelationServiceNotRegisteredException when the relation service
    *        has not been registered with an MBeanServer
    * @param RelationNotFoundException when the relation does not exist in
    *        the relation service.
    */
   public RoleResult setRoles(String relationId, RoleList roles)
     throws IllegalArgumentException, RelationServiceNotRegisteredException,
            RelationNotFoundException;

   /**
    * Handles the update of the relation service role map when a role
    * is changed.<p>
    *
    * It is called from RelationSupport setRole() and setRoles() and
    * the relation service's setRole() and setRoles() methods.<p>
    *
    * The relation service will keep track the MBeans unregistration
    * to maintain the consistency of the relation.
    *
    * @param relationId the relation id
    * @param newRole the new role
    * @param oldRoleValue a list of MBeans in the old role
    * @exception IllegalArgumentException for a null parameter.
    * @exception RelationNotFoundException when the relation id does not
    *            exist.
    * @exception RelationServiceNotRegisteredException when the relation
    *            service has not been registered with an MBeanServer
    */
   public void updateRoleMap(String relationId, Role newRole,
                              List oldRoleValue)
     throws IllegalArgumentException, RelationServiceNotRegisteredException,
            RelationNotFoundException;
}
