/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.relation;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;

import javax.management.MBeanServer;
import javax.management.MBeanException;
import javax.management.ObjectName;

/**
 * This is a helper class for performing role validation. It is used by
 * both the RelationSupport and RelationService classes.<p>
 *
 * It is package private and NOT part of the specification.
 *
 * <p><b>Revisions:</b>
 * <p><b>20020311 Adrian Brock:</b>
 * <ul>
 * <li>ValidateRole always failed
 * <li>Throws wrong exception when not writable
 * </ul>
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.3.6.1 $
 *
 */
class RoleValidator
{
  // Constants ---------------------------------------------------

  // Static ------------------------------------------------------

  /**
   * Check a role for a relation type
   *
   * @param relationService the relation service object name
   * @param server the MBeanServer of the relation service
   * @param relationTypeName the relation to validate against
   * @param role the role to validate
   * @param write pass to true to check for a writable role
   * @return zero for success or a RoleStatus value for failure
   * @exception RelationTypeNotFoundException when the relation type
   *            does not exist in the relation service
   */
  public static int checkRole(ObjectName relationService, MBeanServer server,
                       String relationTypeName, Role role, boolean write)
    throws RelationTypeNotFoundException
  {
    // Get the role information
    RoleInfo roleInfo = null;
    try
    {
      roleInfo = (RoleInfo) server.invoke(relationService, "getRoleInfo",
        new Object[] { relationTypeName, role.getRoleName() },
        new String[] { "java.lang.String", "java.lang.String" });
    }
    catch (MBeanException mbe)
    {
      Exception e=mbe.getTargetException();
      if (e instanceof RelationTypeNotFoundException)
        throw (RelationTypeNotFoundException) e;
      if (e instanceof RoleInfoNotFoundException)
        return RoleStatus.NO_ROLE_WITH_NAME;
      throw new RuntimeException(e.toString());
    }
    catch (Exception e)
    {
      throw new RuntimeException(e.toString());
    }

    // Check if the role is writable
    if (write == true && roleInfo.isWritable() == false)
      return RoleStatus.ROLE_NOT_WRITABLE;

    // Check the cardinality of the role
    ArrayList mbeans = (ArrayList) role.getRoleValue();
    int beanCount = mbeans.size();
    int minimum = roleInfo.getMinDegree();
    if (minimum != RoleInfo.ROLE_CARDINALITY_INFINITY && minimum > beanCount)
      return RoleStatus.LESS_THAN_MIN_ROLE_DEGREE;
    int maximum = roleInfo.getMaxDegree();
    if (maximum != RoleInfo.ROLE_CARDINALITY_INFINITY && maximum < beanCount)
      return RoleStatus.MORE_THAN_MAX_ROLE_DEGREE;

    // Check the MBeans
    String className = roleInfo.getRefMBeanClassName();

    for (int i = 0; i < mbeans.size(); i++)
    {
      try
      {
        ObjectName objectName = (ObjectName) mbeans.get(i);
        if (server.isInstanceOf(objectName, className) == false)
          return RoleStatus.REF_MBEAN_OF_INCORRECT_CLASS;
      }
      catch (Exception e)
      {
        return RoleStatus.REF_MBEAN_NOT_REGISTERED;
      }
    }
    // All done
    return 0;
  }

  /**
   * Check the Roles for a relation Type.
   *
   * @param relationService the relation service object name
   * @param server the MBeanServer of the relation service
   * @param relationTypeName the relation to validate against
   * @param roleList the roles to validate
   * @param write pass to true to check for a writable role
   * @return a RoleResult containing resolved and unresolved roles
   * @exception RelationTypeNotFoundException when the relation type
   *            does not exist in the relation service
   */
  public static RoleResult checkRoles(ObjectName relationService, 
              MBeanServer server, String relationTypeName, RoleList roleList,
              boolean write)
    throws RelationTypeNotFoundException
  {
    // Set up the return value
    RoleList resolved = new RoleList();
    RoleUnresolvedList unresolved = new RoleUnresolvedList();
    RoleResult result = new RoleResult(resolved, unresolved);

    // Check each role
    Iterator iterator = roleList.iterator();
    while (iterator.hasNext())
    {
      Role role = (Role) iterator.next();
      int status = checkRole(relationService, server, relationTypeName, role,
                             write);
      if (status == 0)
        resolved.add(role);
      else
        unresolved.add(new RoleUnresolved(role.getRoleName(),
                                          role.getRoleValue(), status));
    }
    // All Done
    return result;
  }

  /**
   * Validate a role for a relation Type.
   *
   * @param relationService the relation service object name
   * @param server the MBeanServer of the relation service
   * @param relationTypeName the relation to validate against
   * @param role the role to validate
   * @param write pass to true to check for a writable role
   * @exception InvalidRoleValueException when a role does not match its 
   *            definition in the relation type's roleinfos
   * @exception RelationTypeNotFoundException when the relation type
   *            does not exist in the relation service
   * @exception RoleNotFoundException when a role does not exist
   *            in the role
   */
  public static void validateRole(ObjectName relationService, 
         MBeanServer server, String relationTypeName, Role role, boolean write)
    throws InvalidRoleValueException, RelationTypeNotFoundException,
           RoleNotFoundException
  {
    int status = checkRole(relationService, server, relationTypeName, role,
                           write);

    if (status == RoleStatus.NO_ROLE_WITH_NAME)
      throw new RoleNotFoundException(role.getRoleName());
    if (status == RoleStatus.ROLE_NOT_WRITABLE)
      throw new RoleNotFoundException(role.getRoleName() + " not writable");
    else if (status != 0)
      throw new InvalidRoleValueException(role.getRoleName());
  }

  /**
   * Validate the Roles for a relation Type.
   *
   * @param relationService the relation service object name
   * @param server the MBeanServer of the relation service
   * @param relationTypeName the relation to validate against
   * @param roleList the roles to validate
   * @param write pass to true to check for a writable role
   * @exception InvalidRoleValueException when there is a duplicate role name
   *            or a role does not match its definition in the 
   *            relation type's roleinfos
   * @exception RelationTypeNotFoundException when the relation type
   *            does not exist in the relation service
   * @exception RoleNotFoundException when a role does not exist
   *            in the role
   */
  public static void validateRoles(ObjectName relationService, 
              MBeanServer server, String relationTypeName, RoleList roleList,
              boolean write)
    throws InvalidRoleValueException, RelationTypeNotFoundException,
           RoleNotFoundException
  {
    Iterator iterator;

    // Check for duplicate roles
    HashSet roleNames = new HashSet();
    iterator = roleList.iterator();
    while (iterator.hasNext())
    {
      Object roleName = iterator.next();
      if (roleNames.contains(roleName))
        throw new InvalidRoleValueException("Duplicate role " + roleName);
      roleNames.add(roleName);
    }

    // Check the roles
    RoleResult result = checkRoles(relationService, server, relationTypeName, 
                                   roleList, write);
    RoleUnresolvedList errors = result.getRolesUnresolved();  
    iterator = errors.iterator();
    if (iterator.hasNext())
    {
      RoleUnresolved unresolved = (RoleUnresolved) iterator.next();
      int status = unresolved.getProblemType();
      if (status == RoleStatus.NO_ROLE_WITH_NAME)
        throw new RoleNotFoundException(unresolved.getRoleName());
      if (status == RoleStatus.ROLE_NOT_WRITABLE)
        throw new RoleNotFoundException(unresolved.getRoleName() + " not writable");
      else
        throw new InvalidRoleValueException(unresolved.getRoleName());
    }
  }
}
