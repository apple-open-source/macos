/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.relation;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

import javax.management.InstanceNotFoundException;
import javax.management.IntrospectionException;
import javax.management.MBeanException;
import javax.management.MBeanRegistration;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.ReflectionException;
import javax.management.RuntimeMBeanException;

import javax.management.relation.RelationServiceMBean;

import org.jboss.mx.util.MBeanProxy;
import org.jboss.mx.util.MBeanProxyCreationException;

/**
 * Implements the management interface for a relation
 * created internally within the relation service. The relation can
 * have only roles - no attributes or methods.<p>
 *
 * The relation support managed bean can be created externally, including
 * extending it, and then registered with the relation service.<p>
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>
 *
 * @version $Revision: 1.5.6.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>20020412 Juha Lindfors:</b>
 * <ul>
 * <li>Changed MBeanProxy exception handling on create methods -- only need to
 *     catch one MBeanProxyCreationException
 * </li>
 * </ul> 
 */
public class RelationSupport
  implements RelationSupportMBean, MBeanRegistration
{
  // Constants -----------------------------------------------------

  // Attributes ----------------------------------------------------

  /**
   * The relation id.
   */
  String relationId;

  /**
   * The relation service
   */
  ObjectName relationService;

  /**
   * A proxy to the relation service
   */
  private RelationServiceMBean serviceProxy;

  /**
   * The mbean server
   */
  MBeanServer server;

  /**
   * The relation type name
   */
  String relationTypeName;

  /**
   * The roles mapped by role name
   */
  HashMap roles;

  /**
   * Wether this relation is registered in the relation service
   */
  Boolean registered;

  // Static --------------------------------------------------------

  // Constructors --------------------------------------------------

  /**
   * Construct a new relation support object.<p>
   *
   * This constructor is intended for use when manually registrating
   * a relation support object or an class that extends it.<p>
   *
   * Constructing the object does not register it with the relation
   * service, only the following is validated at this stage.<p>
   *
   * The relation id is mandatory.<br>
   * The relation service name is mandatory.<br>.
   * The relation type name is mandatory.<br>
   * The same name is used for more than one role.<p>
   *
   * @param relationId the relation Id
   * @param relationService the object name of the relation service
   * @param relationTypeName the name of the relation type
   * @param roleList the roles in this relation
   * @exception IllegalArgumentException for null values.
   * @exception InvalidRoleValueException when two roles have the same name.
   */
  public RelationSupport(String relationId, ObjectName relationService,
                         String relationTypeName, RoleList roleList)
    throws IllegalArgumentException, InvalidRoleValueException
  {
    init(relationId, relationService, null, relationTypeName, roleList);
  }

  /**
   * Construct a new relation support object.<p>
   *
   * This constructor is intended for use when manually registrating
   * a relation that delegates to relation support.<p>
   *
   * Constructing the object does not register it with the relation
   * service, only the following is validated at this stage.<p>
   *
   * The relation id is mandatory.<br>
   * The relation service name is mandatory.<br>.
   * The mbean service is mandatory.<br>.
   * The relation type name is mandatory.<br>
   * The same name is used for more than one role.<p>
   *
   * @param relationId the relation Id
   * @param relationService the object name of the relation service
   * @param mbeanServer the object name of the relation service
   * @param relationTypeName the name of the relation type
   * @param roleList the roles in this relation
   * @exception IllegalArgumentException for null values.
   * @exception InvalidRoleValueException when two roles have the same name.
   */
  public RelationSupport(String relationId, ObjectName relationService,
                         MBeanServer mbeanServer, String relationTypeName, 
                         RoleList roleList)
    throws IllegalArgumentException, InvalidRoleValueException
  {
    init(relationId, relationService, mbeanServer, relationTypeName,
         roleList);
  }

  // Relation implementation -----------------------------------------

  public RoleResult getAllRoles()
    throws RelationServiceNotRegisteredException
  {
    RoleList resolvedResult = new RoleList();
    RoleUnresolvedList unresolvedResult = new RoleUnresolvedList();
    RoleResult result = new RoleResult(resolvedResult, unresolvedResult);
    synchronized (roles)
    {
      Iterator iterator = roles.values().iterator();
      while (iterator.hasNext())
      {
        Role role = (Role) iterator.next();
        int status = checkRoleReadable(role);
        if (status == 0)
          resolvedResult.add(role);
        else
          unresolvedResult.add(new RoleUnresolved(role.getRoleName(),
                                                  role.getRoleValue(),
                                                  status));
      }
    }
    return result;
  }

  public Map getReferencedMBeans()
  {
    HashMap result = new HashMap();
    synchronized (roles)
    {
      // Look through the roles in this relation
      Iterator iterator = roles.values().iterator();
      while (iterator.hasNext())
      {
        Role role = (Role) iterator.next();
        String roleName = role.getRoleName();

        // Get the mbeans in the role
        ArrayList mbeans = (ArrayList) role.getRoleValue();

        for (int i = 0; i < mbeans.size(); i++)
        {
          ObjectName mbean = (ObjectName) mbeans.get(i);

          // Make sure this mbean has an entry
          ArrayList resultRoles = (ArrayList) result.get(mbean);
          if (resultRoles == null)
          {
            resultRoles = new ArrayList();
            result.put(mbean, resultRoles);
          }

          // It seems the role name should be duplicated?
          // Include the following test if this is a bug in RI.
          // if (resultRoles.contains(roleName) == false)

          // Add the role to this mbean
          resultRoles.add(roleName);
        }
      }
    }

    // All done 
    return result;
  }

  public String getRelationId()
  {
    return relationId;
  }

  public ObjectName getRelationServiceName()
  {
    return relationService;
  }

  public String getRelationTypeName()
  {
    return relationTypeName;
  }

  public List getRole(String roleName)
    throws IllegalArgumentException, RoleNotFoundException, 
           RelationServiceNotRegisteredException
  {
    if (roleName == null)
      throw new IllegalArgumentException("null role name");
    validateRoleReadable(roleName);
    Role role = validateRoleFound(roleName);
    return role.getRoleValue();
  }

  public Integer getRoleCardinality(String roleName)
    throws IllegalArgumentException, RoleNotFoundException
  {
    if (roleName == null)
      throw new IllegalArgumentException("null role name");
    Role role = validateRoleFound(roleName);
    return new Integer(role.getRoleValue().size());
  }

  public RoleResult getRoles(String[] roleNames)
    throws IllegalArgumentException, RelationServiceNotRegisteredException
  {
    RoleList resolvedResult = new RoleList();
    RoleUnresolvedList unresolvedResult = new RoleUnresolvedList();
    RoleResult result = new RoleResult(resolvedResult, unresolvedResult);
    for (int i = 0; i < roleNames.length; i++)
    {
      Role role = (Role) roles.get(roleNames[i]);
      int status = checkRoleReadable(role);
      if (status == 0)
        resolvedResult.add(role);
      else
        unresolvedResult.add(new RoleUnresolved(role.getRoleName(),
                                                role.getRoleValue(),
                                                status));
    }
    return result;
  }

  public void handleMBeanUnregistration(ObjectName objectName, String roleName)
    throws RoleNotFoundException, InvalidRoleValueException,
           RelationServiceNotRegisteredException, RelationTypeNotFoundException,
           RelationNotFoundException
  {
    checkRegistered();
    Role role = validateRoleFound(roleName);
    ArrayList values = (ArrayList) role.getRoleValue();
    ArrayList oldRoleValue = new ArrayList(values);
    if (values.remove(objectName) == false)
      throw new InvalidRoleValueException(roleName + " " + objectName.toString());
    updateRole(role, oldRoleValue);
  }

  public RoleList retrieveAllRoles()
  {
    RoleList result = new RoleList(roles.size());
    synchronized (roles)
    {
      Iterator iterator = roles.values().iterator();
      while (iterator.hasNext())
        result.add((Role) iterator.next());
    }
    return result;
  }

  public void setRole(Role role)
    throws IllegalArgumentException, InvalidRoleValueException,
           RoleNotFoundException, RelationServiceNotRegisteredException,
           RelationTypeNotFoundException, RelationNotFoundException
  {
    if (role == null)
      throw new IllegalArgumentException("null role");
    Role copy = (Role) role.clone();
    checkRegistered();
    RoleValidator.validateRole(relationService, server, relationTypeName, copy,
                               true);
    Role oldRole = (Role) roles.get(role.getRoleName());
    ArrayList oldRoleValue = (ArrayList) oldRole.getRoleValue();
    updateRole(copy, oldRoleValue);
  }

  public RoleResult setRoles(RoleList roleList)
    throws IllegalArgumentException, RelationServiceNotRegisteredException,
           RelationTypeNotFoundException, RelationNotFoundException
  {
    if (roleList == null)
      throw new IllegalArgumentException("null role list");
    RoleList copy = new RoleList(roleList);
    checkRegistered();
    RoleResult result = RoleValidator.checkRoles(relationService, server,
                                                 relationTypeName, copy, true);
    synchronized (result.getRoles())
    {
      Iterator iterator = result.getRoles().iterator();
      while (iterator.hasNext())
      {
        Role role = (Role) iterator.next();
        Role oldRole = (Role) roles.get(role.getRoleName());
        ArrayList oldRoleValue = (ArrayList) oldRole.getRoleValue();
        updateRole(role, oldRoleValue);
      }
    }
    return result;
  }

  // RelationSupport implementation --------------------------------

  public Boolean isInRelationService()
  {
    return registered;
  }

  public void setRelationServiceManagementFlag(Boolean value)
  {
    synchronized (registered)
    {
      registered = new Boolean(value.booleanValue());
    }
  }

  // MBeanRegistration implementation ------------------------------

  public ObjectName preRegister(MBeanServer server, ObjectName objectName)
    throws Exception
  {
    this.server = server;
    return objectName;
  }

  public void postRegister(Boolean registered)
  {
  }

  public void preDeregister()
    throws Exception
  {
  }

  public void postDeregister()
  {
    server = null;
  }

  // Private ----------------------------------------------------------

  /**
   * Constructor support.<p>
   *
   * See the constructors for more information
   *
   * @param relationId the relation Id
   * @param relationService the object name of the relation service
   * @param mbeanServer the object name of the relation service
   * @param relationTypeName the name of the relation type
   * @param roleList the roles in this relation
   * @exception IllegalArgumentException for null values.
   * @exception InvalidRoleValueException when two roles have the same name.
   */
  private void init(String relationId, ObjectName relationService,
                    MBeanServer mbeanServer, String relationTypeName, 
                    RoleList roleList)
    throws IllegalArgumentException, InvalidRoleValueException
  {
    // Validation
    if (relationId == null)
      throw new IllegalArgumentException("null relation id");
    if (relationService == null)
      throw new IllegalArgumentException("null relation service");
    if (relationTypeName == null)
      throw new IllegalArgumentException("null relation type name");

    // Easy parameters
    this.relationId = relationId;
    this.relationTypeName = relationTypeName;
    this.relationService = relationService;
    if (mbeanServer != null)
      server = mbeanServer;
    registered = new Boolean(false);

    // Set up a hash map for the roles for quicker access
    if (roleList == null)
      roles = new HashMap();
    else
    {
      Object[] roleArray = roleList.toArray();
      roles = new HashMap(roleArray.length);
      for (int i = 0; i < roleArray.length; i++)
      {
        Role role = (Role) roleArray[i];
        if (roles.containsKey(role.getRoleName()))
          throw new IllegalArgumentException("duplicate role name");
        roles.put(role.getRoleName(), role);
      }
    }
  }

  /**
   * Check that we are registered with the relation service
   *
   * @exception RelationNotFoundException when the relation
   *            is not registered with the relation service.
   */
  private void checkRegistered()
     throws RelationNotFoundException
  {
    if (isInRelationService().booleanValue() == false)
      throw new RelationNotFoundException("not registered with relation service");
    // What is the purpose of this flag? Why not invoke hasRelation?
  }

  /**
   * Check a role is readable
   *
   * @param role the role to check
   * @return zero for success a value from RoleStatus otherwise.
   * @exception RelationServiceNotRegisteredException when the relation
   *            is not registered with an MBeanServer.
   */
  private int checkRoleReadable(Role role)
    throws RelationServiceNotRegisteredException
  {
    checkServiceProxy();
    try
    {
      return serviceProxy.checkRoleReading(role.getRoleName(), 
               relationTypeName).intValue();
    }
    // RelationTypeNotFound has to be a runtime exception because
    // the spec doesn't allow for this exception
    catch (RelationTypeNotFoundException e)
    {
      throw new RuntimeException(e.toString());
    }
  }

  /**
   * Check a role is writable
   *
   * @param role the role to check
   * @param initFlag don't check the writeablitiy when this flag is true
   * @return zero for success a value from RoleStatus otherwise.
   * @exception RelationServiceNotRegisteredException when the relation
   *            is not registered with an MBeanServer.
   * @exception RelationTypeNotFoundException when the relation type
   *            is not registered with the relation service
   */
  private int checkRoleWritable(Role role, boolean initFlag)
    throws RelationServiceNotRegisteredException, RelationTypeNotFoundException
  {
    checkServiceProxy();
    try
    {
      return serviceProxy.checkRoleWriting(role, relationTypeName, 
               new Boolean(initFlag)).intValue();
    }
    // RelationTypeNotFound has to be a runtime exception because
    // the spec doesn't allow for this exception
    catch (RelationTypeNotFoundException e)
    {
      throw new RuntimeException(e.toString());
    }
  }

  /**
   * Update the role
   *
   * @param role the role to set
   * @param oldRoleVale the old role value
   */
  private void updateRole(Role role, ArrayList oldRoleValue)
  {
    roles.put(role.getRoleName(), role);
    try
    {
      checkServiceProxy();
      serviceProxy.updateRoleMap(relationId, role, oldRoleValue);
      serviceProxy.sendRoleUpdateNotification(relationId, role, oldRoleValue);
    }
    catch (Exception e)
    {
        throw new RuntimeException(e.toString());
    }
  }

  /**
   * Validate the role is found
   *
   * @param roleName the role name to validate
   * @return the found role
   * @exception RoleNotFoundException when the role does not exist
   */
  private Role validateRoleFound(String roleName)
    throws RoleNotFoundException
  {
    Role result = (Role) roles.get(roleName);
    if (result == null)
      throw new RoleNotFoundException(roleName);
    return result;
  }

  /**
   * Validate the role is readable, i.e. it is found and readable
   *
   * @param roleName the role name to validate
   * @exception RoleNotFoundException when the role is not readable
   * @exception RelationServiceNotRegisteredException when the relation
   *            is not registered with an MBeanServer.
   */
  private void validateRoleReadable(String roleName)
    throws RoleNotFoundException, RelationServiceNotRegisteredException
  {
    int status = 0;
    checkServiceProxy();
    try
    {
      status = serviceProxy.checkRoleReading(roleName, 
                                             relationTypeName).intValue();
    }
    // RelationTypeNotFound has to be a runtime exception because
    // the spec doesn't allow for this exception
    catch (RelationTypeNotFoundException e)
    {
      throw new RuntimeException(e.toString());
    }

    if (status == 3)
      throw new RoleNotFoundException(roleName);
    if (status == 6)
      throw new RoleNotFoundException(roleName + " is not readable");
  }

  /**
   * Validate the role is writable, i.e. it is found and readable
   *
   * @param roleName the role name to validate
   * @param initFlag don't check the writeability when this flag is true
   * @return the found role
   * @exception RoleNotFoundException when the role is not writable
   * @exception RelationServiceNotRegisteredException when the relation
   *            is not registered with an MBeanServer.
   * @exception RelationTypeNotFoundException when the relation type
   *            is not registered with the relation service
   */
  private Role validateRoleWritable(String roleName, boolean initFlag)
    throws RoleNotFoundException, RelationServiceNotRegisteredException,
           RelationTypeNotFoundException
  {
    Role result = validateRoleFound(roleName);
    if (checkRoleWritable(result, initFlag) != 0)
      throw new RoleNotFoundException("role not writable");
    return result;
  }

  /**
   * Check the relation service proxy has been constructed.
   *
   * @exception RelationServiceNotRegisteredException when the relation
   *            service has not been registered with the MBeanServer
   */
  private void checkServiceProxy()
    throws RelationServiceNotRegisteredException
  {
    if (serviceProxy == null)
    {
      try
      {
          serviceProxy = (RelationServiceMBean) MBeanProxy.get(
                        RelationServiceMBean.class, relationService, server);
      }
      catch (MBeanProxyCreationException e)
      {
        throw new RelationServiceNotRegisteredException(e.toString());
      }
    }
  }
}
