/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.relation;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Stack;

import javax.management.Attribute;
import javax.management.AttributeNotFoundException;
import javax.management.InstanceNotFoundException;
import javax.management.InvalidAttributeValueException;
import javax.management.MBeanException;
import javax.management.MBeanNotificationInfo;
import javax.management.MBeanRegistration;
import javax.management.MBeanServer;
import javax.management.MBeanServerNotification;
import javax.management.Notification;
import javax.management.NotificationBroadcasterSupport;
import javax.management.NotificationListener;
import javax.management.ObjectName;
import javax.management.ReflectionException;

import org.jboss.mx.server.MBeanServerImpl;

/**
 * Implements the management interface for a relation service.<p>
 *
 * <p><b>Revisions:</b>
 * <p><b>20020311 Adrian Brock:</b>
 * <ul>
 * <li>Fixed setRole for external MBean and exception handling
 * <li>EmptyStack exception in purging
 * <li>Unregistered mbeans should only contain relation mbeans
 * <li>Unregister notifications not working after change to MBean Filter
 * </ul>
 * <p><b>20020312 Adrian Brock:</b>
 * <ul>
 * <li>Fixed wrong exception types thrown and missing exceptions
 * <li>Allow null role list in createRelation
 * </ul>
 *
 * @see RelationServiceMBean
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.6.6.2 $
 */
public class RelationService
  extends NotificationBroadcasterSupport
  implements RelationServiceMBean, MBeanRegistration, NotificationListener
{
  // Constants -----------------------------------------------------

  // Attributes ----------------------------------------------------

  /**
   * Relation ids by relation
   * Note: A relation is an ObjectName for external relations
   * and a RelationSupport object for internal relations
   */
  private HashMap idsByRelation = new HashMap();

  /**
   * The relation service object name
   */
  private ObjectName relationService;

  /**
   * The notification sequence
   */
  private long notificationSequence = 0;

  /**
   * The purge flag
   */
  private boolean purgeFlag;

  /**
   * Relations by relation id
   * Note: A relation is an ObjectName for external relations
   * and a RelationSupport object for internal relations
   */
  private HashMap relationsById = new HashMap();

  /**
   * The mbean server we are registered with
   */
  private MBeanServer server;

  /**
   * The relation types by name
   */
  private HashMap typesByName = new HashMap();

  /**
   * Relation type names by relation ids
   */
  private HashMap typeNamesById = new HashMap();

  /**
   * A notification listener for unregistration
   */
  private MBeanServerNotificationFilter filter;

  /**
   * A list of MBeans unregistered but not yet removed.
   */
  private Stack unregistered = new Stack();

  /**
   * Relation ids an MBean is part of by MBean object name
   * The values side is a HashMap keyed by relation ids
   * with values of a HashSet of role names.
   */
  private HashMap idRolesMapByMBean = new HashMap();
  
  // The name of the delegate
  private ObjectName delegate;

  // Static --------------------------------------------------------

  // Constructors --------------------------------------------------

  /**
   * Construct a new relation service
   *
   * @param purgeFlag whether immediate purges should be performed,
   *        pass true for immediate, false otherwise
   */
  public RelationService(boolean purgeFlag)
  {
    setPurgeFlag(purgeFlag);
  }

  // RelationServiceMBean implementation ---------------------------

  public synchronized void addRelation(ObjectName relation)
    throws IllegalArgumentException, NoSuchMethodException,
           RelationServiceNotRegisteredException, InvalidRelationIdException,
           InvalidRelationServiceException, RelationTypeNotFoundException,
           InvalidRoleValueException, RoleNotFoundException,
           InstanceNotFoundException
  {
    // Check we have a relation
    if (relation == null)
      throw new IllegalArgumentException("null relation");
    isActive();

    // Get the information we need from the relation
    ObjectName otherService = null;
    String relationId = null;
    String relationTypeName = null;
    RoleList roleList = null;
    try
    {
      server.isInstanceOf(relation, Relation.class.getName());
      otherService = (ObjectName) server.getAttribute(relation, 
                                                      "RelationServiceName");
      relationId = (String) server.getAttribute(relation, "RelationId");
      relationTypeName = (String) server.getAttribute(relation,
                                                      "RelationTypeName");
      roleList = (RoleList) server.invoke(relation, "retrieveAllRoles",
                                          new Object[0], new String[0]);
    }
    catch (InstanceNotFoundException e)
    {
       throw e;
    }
    catch (Exception e)
    {
      throw new NoSuchMethodException("Not a relation or not registered");
    }

    // Check we are in the correct relation service
    if (otherService.equals(relationService) == false)
      throw new InvalidRelationServiceException(otherService + " != " + 
                                                relationService);

    // Create a copy of the role list
    RoleList copy = new RoleList(roleList);

    // Create any missing roles
    createMissingRoles(relationTypeName, copy);

    // Validate the role list
    RoleValidator.validateRoles(relationService, server, relationTypeName,
                                copy, false);

    // Add the relation if it is not already present
    validateAndAddRelation(relationId, relation, relationTypeName);

    // Monitor this relation
    filter.enableObjectName(relation);
  }

  public synchronized void addRelationType(RelationType relationType)
    throws IllegalArgumentException, InvalidRelationTypeException
  {
    if (relationType == null)
      throw new IllegalArgumentException("null relation type");
    synchronized (typesByName)
    {
      String name = relationType.getRelationTypeName();
      if (typesByName.containsKey(name))
        throw new InvalidRelationTypeException("duplicate relation id: " + name);
      validateRelationType(relationType);
      typesByName.put(name, relationType);
    }
  }

  public Integer checkRoleReading(String roleName, String relationTypeName)
    throws IllegalArgumentException, RelationTypeNotFoundException
  {
    if (roleName == null)
       throw new IllegalArgumentException("Null role name");

    // Get the relation type
    RelationType relationType = retrieveRelationTypeForName(relationTypeName);

    // Get the role information
    RoleInfo roleInfo = null;
    try
    {
      roleInfo = relationType.getRoleInfo(roleName);
    }
    catch (RoleInfoNotFoundException e)
    { 
      return new Integer(RoleStatus.NO_ROLE_WITH_NAME);
    }

    // Is it readable?
    if (roleInfo.isReadable() == false)
      return new Integer(RoleStatus.ROLE_NOT_READABLE);

    // Yes it is
    return new Integer(0);
  }

  public Integer checkRoleWriting(Role role, String relationTypeName,
                                  Boolean initFlag)
    throws IllegalArgumentException, RelationTypeNotFoundException
  {
    if (role == null)
       throw new IllegalArgumentException("Null role name");
    if (initFlag == null)
       throw new IllegalArgumentException("Null init flag");

    // Get the relation type
    RelationType relationType = retrieveRelationTypeForName(relationTypeName);

    // Get the role information
    RoleInfo roleInfo = null;
    try
    {
      roleInfo = relationType.getRoleInfo(role.getRoleName());
    }
    catch (RoleInfoNotFoundException e)
    { 
      return new Integer(RoleStatus.NO_ROLE_WITH_NAME);
    }

    // Is it writable?
    if (initFlag.booleanValue() == false && roleInfo.isWritable() == false)
      return new Integer(RoleStatus.ROLE_NOT_WRITABLE);

    // Yes it is
    return new Integer(0);
  }

  public synchronized void createRelation(String relationId, 
                             String relationTypeName, RoleList roleList)
    throws IllegalArgumentException,
           RelationServiceNotRegisteredException, InvalidRelationIdException,
           RelationTypeNotFoundException, InvalidRoleValueException, 
           RoleNotFoundException
  {
    // Take a copy of the role list
    RoleList copy = null;
    if (roleList != null)
       copy = new RoleList(roleList);
    else
       copy = new RoleList();

    // Create a relation
    isActive();
    RelationSupport relation = new RelationSupport(relationId,
                                                   relationService,
                                                   server,
                                                   relationTypeName,
                                                   copy);

    // Create any missing roles
    createMissingRoles(relationTypeName, copy);

    // Validate the role list
    RoleValidator.validateRoles(relationService, server, relationTypeName,
                                copy, false);

    // Add the relation if it is not already present
    validateAndAddRelation(relationId, relation, relationTypeName);
  }

  public synchronized void createRelationType(String relationTypeName,
                                 RoleInfo[] roleInfos)
    throws IllegalArgumentException, InvalidRelationTypeException
  {
    if (relationTypeName == null)
      throw new IllegalArgumentException("null relation type name");
    synchronized (typesByName)
    {
      if (typesByName.containsKey(relationTypeName))
        throw new InvalidRelationTypeException("duplicate relation id: "
                                               + relationTypeName);
      RelationType relationType = new RelationTypeSupport(relationTypeName,
                                                          roleInfos);
      typesByName.put(relationTypeName, relationType);
    }
  }

  public Map findAssociatedMBeans(ObjectName mbeanName, 
                                  String relationTypeName, String roleName)
    throws IllegalArgumentException
  {
    HashMap referencing = (HashMap) findReferencingRelations(mbeanName,
                                                             relationTypeName,
                                                             roleName);

    HashMap result = new HashMap();

    // Loop through our relations
    Iterator relationIterator = referencing.entrySet().iterator();
    while (relationIterator.hasNext())
    {
      Map.Entry referencingEntry = (Map.Entry) relationIterator.next();
      String relationId = (String) referencingEntry.getKey();
      ArrayList referencingRoleNames = (ArrayList) referencingEntry.getValue();

      // Get the all beans in this relation
      HashMap referenced = null;
      try
      {
        referenced = (HashMap) getReferencedMBeans(relationId);
      }
      catch (RelationNotFoundException e)
      {
        throw new RuntimeException(e.toString());
      }

      // Check each bean's roles
      Iterator mbeanIterator = referenced.entrySet().iterator();
      while (mbeanIterator.hasNext())
      {
        Map.Entry referencedEntry = (Map.Entry) mbeanIterator.next();
        ObjectName objectName = (ObjectName) referencedEntry.getKey();

        // Exclude ourselves from the test
        if (objectName.equals(mbeanName) == false)
        {
          ArrayList referencedRoleNames = (ArrayList) referencedEntry.getValue();

          // Do we share a role?
          Iterator roleIterator = referencedRoleNames.iterator();
          while (roleIterator.hasNext())
          {
            String currentRoleName = (String) roleIterator.next();
            if (referencedRoleNames.contains(currentRoleName))
            {
              // Ok this is one of our associated mbeans
              ArrayList resultList = (ArrayList) result.get(objectName);
              if (resultList == null)
              {
                resultList = new ArrayList();
                resultList.add(currentRoleName);
                result.put(objectName, resultList);
              }
              else if (resultList.contains(currentRoleName) == false)
                resultList.add(currentRoleName);
              // We've findished with this mbean do the next
              break;
            }
          }
        }
      }
    }
    // All done
    return result;
  }

  public Map findReferencingRelations(ObjectName mbeanName, 
                                      String relationTypeName, String roleName)
    throws IllegalArgumentException
  {
    if (mbeanName == null)
      throw new IllegalArgumentException("null object name");

    HashMap result = new HashMap();

    // Get the relations to roles map for the passed mbean
    HashMap idRolesMap = (HashMap) idRolesMapByMBean.get(mbeanName);
    Iterator iterator = idRolesMap.entrySet().iterator();
    while (iterator.hasNext())
    {
      Map.Entry entry = (Map.Entry) iterator.next();
      String relationId = (String) entry.getKey();
      HashSet roleNames = (HashSet) entry.getValue();

      // See if we have the correct relation type
      if (relationTypeName == null ||
          typeNamesById.get(relationId).equals(relationTypeName))
      {
        ArrayList resultRoleNames = new ArrayList();
  
        // No role specified, add them all
        if (roleName == null)
          resultRoleNames.add(roleNames);
        // See if we have this role
        else if (roleNames.contains(roleName) &&
                 resultRoleNames.contains(roleName) == false)
          resultRoleNames.add(roleName);

        // Did we find anything, use it
        if (resultRoleNames.size() > 0)
          result.put(relationId, resultRoleNames);
      }
    }
    // All done
    return result;
  }

  public List findRelationsOfType(String relationTypeName)
    throws IllegalArgumentException, RelationTypeNotFoundException
  {
    if (relationTypeName == null)
      throw new IllegalArgumentException("null relation type name");
    if (typesByName.containsKey(relationTypeName) == false)
      throw new RelationTypeNotFoundException("relation type name not found");

    // Build the list
    ArrayList result = new ArrayList();
    Iterator iterator = typeNamesById.entrySet().iterator();
    while (iterator.hasNext())
    {
      Map.Entry entry = (Map.Entry) iterator.next();
      String typeName = (String) entry.getValue();
      if (typeName.equals(relationTypeName))
        result.add((String) entry.getKey());
    }
    // All done
    return result;
  }

  public List getAllRelationIds()
  {
    ArrayList result = new ArrayList(relationsById.size());
    synchronized (relationsById)
    {
      Iterator iterator = relationsById.keySet().iterator();
      while (iterator.hasNext())
        result.add(iterator.next());
    }
    return result;
  }

  public List getAllRelationTypeNames()
  {
    ArrayList result = new ArrayList(typesByName.size());
    synchronized(typesByName)
    {
      Iterator iterator = typesByName.keySet().iterator();
      while (iterator.hasNext())
        result.add(iterator.next());
    }
    return result;
  }

  public RoleResult getAllRoles(String relationId)
    throws IllegalArgumentException, RelationNotFoundException,
           RelationServiceNotRegisteredException
  {
    isActive();
    Object relation = retrieveRelationForId(relationId);
    
    // Ask the relation for the roles
    if (relation instanceof RelationSupport)
    {
      return ((RelationSupport) relation).getAllRoles();
    }
    else
    {
      ObjectName objectName = (ObjectName) relation;
      try
      {
        return (RoleResult) server.getAttribute(objectName, "AllRoles");
      }
      catch (InstanceNotFoundException e)
      {
        throw new RelationNotFoundException(objectName.toString());
      }
      catch (Exception e)
      {
        throw new RuntimeException(e.toString());
      }
    }
  }

  public boolean getPurgeFlag()
  {
    return purgeFlag;
  }

  public Map getReferencedMBeans(String relationId)
    throws IllegalArgumentException, RelationNotFoundException
  {
    Object relation = retrieveRelationForId(relationId);
    
    // Ask the relation for the referenced mbeans
    if (relation instanceof RelationSupport)
    {
      return ((RelationSupport) relation).getReferencedMBeans();
    }
    else
    {
      ObjectName objectName = (ObjectName) relation;
      try
      {
        return (Map) server.getAttribute(objectName, "ReferencedMBeans");
      }
      catch (InstanceNotFoundException e)
      {
        throw new RelationNotFoundException(objectName.toString());
      }
      catch (Exception e)
      {
        throw new RuntimeException(e.toString());
      }
    }
  }

  public String getRelationTypeName(String relationId)
    throws IllegalArgumentException, RelationNotFoundException
  {
    return retrieveTypeNameForId(relationId);
  }

  public List getRole(String relationId, String roleName)
    throws IllegalArgumentException, RelationNotFoundException,
           RelationServiceNotRegisteredException, RoleNotFoundException
  {
    // Get the relation object name
    if (roleName == null)
      throw new IllegalArgumentException("null role");
    isActive();
    Object relation = retrieveRelationForId(relationId);
    
    // Ask the relation for the role value
    if (relation instanceof RelationSupport)
    {
      return ((RelationSupport) relation).getRole(roleName);
    }
    else
    {
      ObjectName objectName = (ObjectName) relation;
      try
      {
        List result = (List) server.invoke(objectName, "getRole",
        new Object[] { roleName },
        new String[] { "java.lang.String" });
        return result;
      }
      catch (InstanceNotFoundException e)
      {
        throw new RelationNotFoundException(objectName.toString());
      }
      catch (MBeanException mbe)
      {
        Exception e = mbe.getTargetException();
        if (e instanceof RoleNotFoundException)
          throw (RoleNotFoundException) e;
        else
          throw new RuntimeException(e.toString());
      }
      catch (ReflectionException e)
      {
        throw new RuntimeException(e.toString());
      }
    }
  }

  public Integer getRoleCardinality(String relationId, String roleName)
    throws IllegalArgumentException, RelationNotFoundException,
           RoleNotFoundException
  {
    // Get the relation object name
    if (roleName == null)
      throw new IllegalArgumentException("null role");
    Object relation = retrieveRelationForId(relationId);
    
    // Ask the relation for the role cardinality
    if (relation instanceof RelationSupport)
    {
      return ((RelationSupport) relation).getRoleCardinality(roleName);
    }
    else
    {
      ObjectName objectName = (ObjectName) relation;
      try
      {
        Integer result = (Integer) server.invoke(
          objectName, "getRoleCardinality",
          new Object[] { roleName },
          new String[] { "java.lang.String" });
        return result;
      }
      catch (InstanceNotFoundException e)
      {
        throw new RelationNotFoundException(objectName.toString());
      }
      catch (MBeanException mbe)
      {
        Exception e = mbe.getTargetException();
        if (e instanceof RoleNotFoundException)
          throw (RoleNotFoundException) e;
        else
          throw new RuntimeException(e.toString());
      }
      catch (ReflectionException e)
      {
        throw new RuntimeException(e.toString());
      }
    }
  }

  public RoleInfo getRoleInfo(String relationTypeName, String roleInfoName)
    throws IllegalArgumentException, RelationTypeNotFoundException,
           RoleInfoNotFoundException
  {
    // Get the relation type
    RelationType relationType = retrieveRelationTypeForName(relationTypeName);

    // Return the role information
    return relationType.getRoleInfo(roleInfoName);
  }

  public List getRoleInfos(String relationTypeName)
    throws IllegalArgumentException, RelationTypeNotFoundException
  {
    // Get the relation type
    RelationType relationType = retrieveRelationTypeForName(relationTypeName);

    // Return the role information
    return relationType.getRoleInfos();
  }

  public RoleResult getRoles(String relationId, String[] roleNames)
    throws IllegalArgumentException, RelationNotFoundException,
           RelationServiceNotRegisteredException
  {
    // Get the relation object name
    if (roleNames == null)
      throw new IllegalArgumentException("null role names");
    isActive();
    Object relation = retrieveRelationForId(relationId);
    
    // Ask the relation for the role value
    if (relation instanceof RelationSupport)
    {
      return ((RelationSupport) relation).getRoles(roleNames);
    }
    else
    {
      ObjectName objectName = (ObjectName) relation;
      try
      {
        RoleResult result = (RoleResult) server.invoke(objectName, "getRoles",
        new Object[] { roleNames },
        new String[] { new String[0].getClass().getName() });
        return result;
      }
      catch (InstanceNotFoundException e)
      {
        throw new RelationNotFoundException(objectName.toString());
      }
      catch (MBeanException e)
      {
        throw new RuntimeException(e.toString());
      }
      catch (ReflectionException e)
      {
        throw new RuntimeException(e.toString());
      }
    }
  }

  public Boolean hasRelation(String relationId)
    throws IllegalArgumentException
  {
    if (relationId == null)
      throw new IllegalArgumentException("null relation id");
    return new Boolean(relationsById.get(relationId) != null);
  }

  public void isActive()
    throws RelationServiceNotRegisteredException
  {
    if (server == null)
      throw new RelationServiceNotRegisteredException("Not registered");
  }

  public String isRelation(ObjectName objectName)
    throws IllegalArgumentException
  {
    if (objectName == null)
      throw new IllegalArgumentException("null object name");
    return (String) idsByRelation.get(objectName);
  }

  public ObjectName isRelationMBean(String relationId)
    throws IllegalArgumentException, RelationNotFoundException
  {
    if (relationId == null)
      throw new IllegalArgumentException("null relation id");
    Object result = relationsById.get(relationId);
    if (result == null)
       throw new RelationNotFoundException(relationId);
    if (result instanceof ObjectName)
       return (ObjectName) result;
    else
       return null;
  }

  public void purgeRelations()
    throws RelationServiceNotRegisteredException
  {
    isActive();
    // Keep going until they are all done
    while (unregistered.empty() == false)
    {
      // Get the next object
      ObjectName mbean = (ObjectName) unregistered.pop();

      // Keep track of the remain relations/roles
      HashMap relationRoles = new HashMap();
      
      // Get the relations for this mbean
      HashMap idRolesMap = (HashMap) idRolesMapByMBean.get(mbean);

      // Go through each relation/role
      Iterator iterator = idRolesMap.entrySet().iterator();
      while (iterator.hasNext())
      {
        Map.Entry entry = (Map.Entry) iterator.next();
        String relationId = (String) entry.getKey();
        HashSet roleNames = (HashSet) entry.getValue();
        Iterator inner = roleNames.iterator();
        while (inner.hasNext())
        {
          String roleName = (String) inner.next();
          relationRoles.put(relationId, roleName);
        }
      }

      // Tell the relation about the removed role
      iterator = relationRoles.entrySet().iterator();
      while (iterator.hasNext())
      {
        Map.Entry entry = (Map.Entry) iterator.next();
        String relationId = (String) entry.getKey();
        String roleName = (String) entry.getValue();
        Object relation = relationsById.get(relationId);
        String typeName = (String) typeNamesById.get(relationId);
        RelationType relationType = (RelationType) typesByName.get(typeName);
        if (relation instanceof RelationSupport)
        {
          RelationSupport support = (RelationSupport) relation;
          try
          {
            // Check to see if the relation is now invalid
            Integer cardinality = support.getRoleCardinality(roleName);
            RoleInfo roleInfo = (RoleInfo) relationType.getRoleInfo(roleName);
            int minDegree = roleInfo.getMinDegree();

            // It's invalid, remove it
            if (cardinality.intValue() == minDegree)
              removeRelation(relationId);
            else
            // It's ok tell the relation about the unregistration
              support.handleMBeanUnregistration(mbean, roleName);
          }
          catch (Exception e)
          {
            throw new RuntimeException(e.toString());
          }
        }
        else
        {
          try
          {
            // Check to see if the relation is now invalid
            ObjectName objectName = (ObjectName) relation;
            Integer cardinality = (Integer) server.invoke(
               objectName, "getRoleCardinality",
               new Object[] { roleName },
               new String[] { "java.lang.String" });
            RoleInfo roleInfo = (RoleInfo) relationType.getRoleInfo(roleName);
            int minDegree = roleInfo.getMinDegree();

            // It's invalid, remove it
            if (cardinality.intValue() == minDegree)
              removeRelation(relationId);
            else
            // It's ok tell the relation about the unregistration
              server.invoke(objectName, "handleMBeanUnregistration",
                new Object[] { mbean, roleName },
                new String[] { "java.lang.String", "java.lang.String "} );
          }
          catch (MBeanException mbe)
          {
            throw new RuntimeException(mbe.getTargetException().toString());
          }
          catch (Exception e)
          {
            throw new RuntimeException(e.toString());
          }
        }
      }      
    }
  }

  public synchronized void removeRelation(String relationId)
    throws IllegalArgumentException, RelationNotFoundException,
           RelationServiceNotRegisteredException
  {
    isActive();

    // Get the MBeans that are part of this relation
    ArrayList unregMBeans = new ArrayList(
      getReferencedMBeans(relationId).keySet());

    // Check to see whether this will remove the MBean

    Iterator iterator = unregMBeans.iterator();
    while (iterator.hasNext())
    {
       // Remove the MBeans relation role map
       ObjectName mbean = (ObjectName) iterator.next();
       HashMap idRolesMap = (HashMap) idRolesMapByMBean.get(mbean);
       idRolesMap.remove(relationId);

       // We were the last?
       if (idRolesMap.size() == 0)
         idRolesMapByMBean.remove(mbean);

       // Is this an MBean a relation?
       if (idsByRelation.containsKey(mbean) == false)
         iterator.remove();
    }

    // Send a notification of the removal
    sendRelationRemovalNotification(relationId, unregMBeans);

    // Remove the relation from all the data structures
    Object relation = retrieveRelationForId(relationId);
    relationsById.remove(relationId);
    idsByRelation.remove(relation);
    typeNamesById.remove(relationId);

    // Update the relation management flag
    if (relation instanceof ObjectName)
    {
      try
      {
        Attribute attribute = new Attribute("RelationServiceManagementFlag",
                                            new Boolean(false));
        server.setAttribute((ObjectName) relation, attribute);
      }
      catch (Exception doesntImplementRelationSupportMBean) {}

      // Don't monitor this relation anymore
      filter.disableObjectName((ObjectName) relation);
    }
  }

  public synchronized void removeRelationType(String relationTypeName)
    throws IllegalArgumentException, RelationTypeNotFoundException,
           RelationServiceNotRegisteredException
  {
    if (relationTypeName == null)
      throw new IllegalArgumentException("null relation type name");
    isActive();
    if (typesByName.containsKey(relationTypeName) == false)
      throw new RelationTypeNotFoundException("relation type name not found");

    // Remove any relations for this relation type
    // FIXME: This is rubbish
    ArrayList ids = new ArrayList();
    Map.Entry entry;
    Iterator iterator = typeNamesById.entrySet().iterator();
    while(iterator.hasNext())
    {
      entry = (Map.Entry) iterator.next();
      if (entry.getValue().equals(relationTypeName))
        ids.add(entry.getKey());
    }
    for (int i = 0; i < ids.size(); i++)
    {
      try
      {
        removeRelation((String) ids.get(i));
      }
      catch (RelationNotFoundException ignored) {}
    }

    // Remove the relation type
    typesByName.remove(relationTypeName);
  }

  public void sendRelationCreationNotification(String relationId)
    throws IllegalArgumentException, RelationNotFoundException
  {
    // Determine the type of relation
    String type = null;
    String description = null;
    if (relationsById.get(relationId) instanceof RelationSupport)
    {
      type = RelationNotification.RELATION_BASIC_CREATION;
      description = "Creation of internal relation.";
    }
    else
    {
      type = RelationNotification.RELATION_MBEAN_CREATION;
      description = "Creation of external relation.";
    }
    // Send the notification
    sendNotification(type, description, relationId, null, null, null);
  }

  public void sendRelationRemovalNotification(String relationId,
                                              List unregMBeans)
    throws IllegalArgumentException, RelationNotFoundException
  {
    // Determine the type of relation
    String type = null;
    String description = null;
    if (relationsById.get(relationId) instanceof RelationSupport)
    {
      type = RelationNotification.RELATION_BASIC_REMOVAL;
      description = "Removal of internal relation.";
    }
    else
    {
      type = RelationNotification.RELATION_MBEAN_REMOVAL;
      description = "Removal of external relation.";
    }
    // Send the notification
    sendNotification(type, description, relationId, unregMBeans, null, null);
  }

  public void sendRoleUpdateNotification(String relationId,
                                         Role newRole, List oldRoleValue)
    throws IllegalArgumentException, RelationNotFoundException
  {
    // Determine the type of relation
    String type = null;
    String description = null;
    if (relationsById.get(relationId) instanceof RelationSupport)
    {
      type = RelationNotification.RELATION_BASIC_UPDATE;
      description = "Update of internal relation.";
    }
    else
    {
      type = RelationNotification.RELATION_MBEAN_UPDATE;
      description = "Update of external relation.";
    }
    if (newRole == null)
       throw new IllegalArgumentException("null role");

    if (oldRoleValue == null)
       throw new IllegalArgumentException("null old role value");

    // Send the notification
    sendNotification(type, description, relationId, null, newRole, oldRoleValue);
  }

  public void setPurgeFlag(boolean value)
  {
    purgeFlag = value;
  }

  public void setRole(String relationId, Role role)
    throws IllegalArgumentException, RelationServiceNotRegisteredException,
           RelationNotFoundException, RoleNotFoundException,
           InvalidRoleValueException, RelationTypeNotFoundException
  {
    // Get the relation object name
    if (role == null)
      throw new IllegalArgumentException("null role");
    isActive();
    Object relation = retrieveRelationForId(relationId);
    
    // Ask the relation to set the role
    if (relation instanceof RelationSupport)
    {
      ((RelationSupport) relation).setRole(role);
    }
    else
    {
      ObjectName objectName = (ObjectName) relation;
      try
      {
        server.setAttribute(objectName, new Attribute("Role", role));
      }
      catch (InstanceNotFoundException e)
      {
        throw new RelationNotFoundException(objectName.toString());
      }
      catch (MBeanException mbe)
      {
        Exception e = mbe.getTargetException();
        if (e instanceof RoleNotFoundException)
          throw (RoleNotFoundException) e;
        else if (e instanceof InvalidRoleValueException)
          throw (InvalidRoleValueException) e;
        else
          throw new RuntimeException(e.toString());
      }
      catch (AttributeNotFoundException e)
      {
        throw new RuntimeException(e.toString());
      }
      catch (InvalidAttributeValueException e)
      {
        throw new RuntimeException(e.toString());
      }
      catch (ReflectionException e)
      {
        throw new RuntimeException(e.toString());
      }
    }
  }

  public RoleResult setRoles(String relationId, RoleList roles)
    throws IllegalArgumentException, RelationServiceNotRegisteredException,
           RelationNotFoundException
  {
    // Get the relation object name
    if (roles == null)
      throw new IllegalArgumentException("null roles");
    isActive();
    Object relation = retrieveRelationForId(relationId);
    
    // Ask the relation to set the roles
    if (relation instanceof RelationSupport)
    {
      try
      {
        return ((RelationSupport) relation).setRoles(roles);
      }
      // Why doesn't it throw this?
      catch (RelationTypeNotFoundException e)
      {
        throw new RuntimeException(e.toString());
      }
    }
    else
    {
      ObjectName objectName = (ObjectName) relation;
      try
      {
        RoleResult result = (RoleResult) server.invoke(objectName, "setRoles", 
          new Object[] { roles }, 
          new String[] { "javax.management.relation.RoleList" });
        return result;
      }
      catch (InstanceNotFoundException e)
      {
        throw new RelationNotFoundException(objectName.toString());
      }
      catch (MBeanException e)
      {
        throw new RuntimeException(e.getTargetException().toString());
      }
      catch (ReflectionException e)
      {
        throw new RuntimeException(e.toString());
      }
    }
  }

  public void updateRoleMap(String relationId, Role newRole,
                            List oldRoleValue)
    throws IllegalArgumentException, RelationServiceNotRegisteredException,
           RelationNotFoundException
  {
    if (relationId == null)
      throw new IllegalArgumentException("null relation id");
    if (newRole == null)
      throw new IllegalArgumentException("null role");
    if (oldRoleValue == null)
      throw new IllegalArgumentException("null old role value");
    isActive();

    if (relationsById.containsKey(relationId) == false)
      throw new RelationNotFoundException("Invalid relation id: " + relationId);

    // Get the role name and new Value
    String roleName = newRole.getRoleName();
    ArrayList newRoleValue = (ArrayList) newRole.getRoleValue();

    // Remove the unused and unchanged object names
    Iterator iterator = oldRoleValue.iterator();
    while (iterator.hasNext())
    {
      ObjectName objectName = (ObjectName) iterator.next();
      if (newRoleValue.contains(objectName) == false)
      {
        // We have to remove this relation/role
        HashMap idRolesMap = (HashMap) idRolesMapByMBean.get(objectName);
        HashSet roleNames = (HashSet) idRolesMap.get(relationId);
        roleNames.remove(roleName);
        if (roleNames.size() == 0)
        {
          idRolesMap.remove(relationId);
          if (idRolesMap.size() == 0)
          {
            idRolesMapByMBean.remove(objectName);
            filter.disableObjectName(objectName);
          }
        }
      }
    }

    // Make sure all the roles exist

    iterator = newRoleValue.iterator();
    for (int i = 0; i < newRoleValue.size(); i++)
    {
      ObjectName objectName = (ObjectName) newRoleValue.get(i);
      HashMap idRolesMap = (HashMap) idRolesMapByMBean.get(objectName);
      if (idRolesMap == null)
      {
        idRolesMap = new HashMap();
        idRolesMapByMBean.put(objectName, idRolesMap);
        filter.enableObjectName(objectName);
      }
      HashSet roleNames = (HashSet) idRolesMap.get(relationId);
      if (roleNames == null)
      {
         roleNames = new HashSet();
         idRolesMap.put(relationId, roleNames);
      }
      if (roleNames.contains(roleName) == false)
        roleNames.add(roleName);
    }
  }

  // MBeanRegistration implementation ------------------------------

  public ObjectName preRegister(MBeanServer server, ObjectName objectName)
    throws Exception
  {
    // Save some data
    this.server = server;
    this.relationService = objectName;

    // Install our notification listener we aren't interested in registration
    // We aren't monitoring anything at start-up
    filter = new MBeanServerNotificationFilter();
    filter.enableType(MBeanServerNotification.UNREGISTRATION_NOTIFICATION);
    filter.disableAllObjectNames();
    delegate = new ObjectName(MBeanServerImpl.MBEAN_SERVER_DELEGATE);
    server.addNotificationListener(delegate, this, filter, null);
    
    // Ok go ahead and register
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
    try
    {
      // Remove the notification listener
      server.removeNotificationListener(delegate, this);
    }
    catch(Exception ignored) {}

    // REVIEW: Should we remove relation types/relations here?

    // We are no longer registered
    server = null;
  }

  // NotificationListener implementation ----------------------------

  public void handleNotification(Notification notification, Object handback)
  {
    // Make sure we are not called malicously
    if (notification == null || 
        !(notification instanceof MBeanServerNotification))
      return;

    // It still might be malicous
    MBeanServerNotification mbsn = (MBeanServerNotification) notification;
    if (mbsn.getType().equals(MBeanServerNotification.UNREGISTRATION_NOTIFICATION) == false)
      return;

    // Add the object to the list of mbeans to remove
    ObjectName objectName = mbsn.getMBeanName();
    unregistered.push(objectName);

    try
    {
      // Are we set to automatic purge?
      if (purgeFlag == true)
        purgeRelations();
    }
    catch (Exception ignored) {}

    try
    {
      // Is this a relation?
      String relationId = (String) idsByRelation.get(objectName);
      if (relationId != null)
        removeRelation(relationId);
    }
    catch (Exception ignored) {}
  }

  // NotificationBroadcasterSupport overrides -----------------------

  public MBeanNotificationInfo[] getNotificationInfo()
  {
    MBeanNotificationInfo[] result = new MBeanNotificationInfo[1];
    String[] types = new String[]
    {
      RelationNotification.RELATION_BASIC_CREATION,
      RelationNotification.RELATION_BASIC_REMOVAL,
      RelationNotification.RELATION_BASIC_UPDATE,
      RelationNotification.RELATION_MBEAN_CREATION,
      RelationNotification.RELATION_MBEAN_REMOVAL,
      RelationNotification.RELATION_MBEAN_UPDATE
    };
    result[0] = new MBeanNotificationInfo(types,
      "javax.management.relation.RelationNotification",
      "Notifications sent by the Relation Service MBean");
    return result;
  }

  // Private --------------------------------------------------------

  /**
   * Create missing roles
   *
   * @param relationTypeName the relation type name
   * @param roleList the existing roles
   * @exception IllegalArgumentException for a null relation id
   * @exception RelationTypeNotFoundException for a missing relation type
   */
  private void createMissingRoles(String relationTypeName, RoleList roleList)
    throws IllegalArgumentException, RelationTypeNotFoundException
  {
    // Get the relation type
    RelationType relationType = retrieveRelationTypeForName(relationTypeName);

    // Get the Role Information
    ArrayList roleInfos = (ArrayList) relationType.getRoleInfos();

    // Add empty roles for missing roles
    for (int i = 0; i < roleInfos.size(); i++)
    {
      RoleInfo roleInfo = (RoleInfo) roleInfos.get(i);
      boolean found = false;
      Iterator inner = roleList.iterator();
      while (inner.hasNext())
      {
        Role role = (Role) inner.next();
        if (role.getRoleName().equals(roleInfo.getName()))
        {
          found = true;
          break;
        }
      }
      if (found == false)
        roleList.add(new Role(roleInfo.getName(), new RoleList()));
    }
  }

  /**
   * Get the relation for a relation id
   *
   * @param relationId the relation id
   * @return the relation's object name or a relation support object
   * @exception IllegalArgumentException for a null relation id
   * @exception RelationNotFoundException when the relation is not registered
   */
  private Object retrieveRelationForId(String relationId)
    throws IllegalArgumentException, RelationNotFoundException
  {
    if (relationId == null)
      throw new IllegalArgumentException("null relation id");
    Object result = relationsById.get(relationId);
    if (result == null)
      throw new RelationNotFoundException(relationId);
    return result;
  }

  /**
   * Get the relation type name for a relation id
   *
   * @param relationId the relation id
   * @return the relation type name
   * @exception IllegalArgumentException for a null relation id
   * @exception RelationNotFoundException when the relation is not registered
   */
  private String retrieveTypeNameForId(String relationId)
    throws IllegalArgumentException, RelationNotFoundException
  {
    if (relationId == null)
      throw new IllegalArgumentException("null relation id");
    String result = (String) typeNamesById.get(relationId);
    if (result == null)
      throw new RelationNotFoundException(relationId);
    return result;
  }

  /**
   * Get the relation type for a relation type name
   *
   * @param relationTypeName the relation type name
   * @return the relation type
   * @exception IllegalArgumentException for a null relation type name
   * @exception RelationTypeNotFoundException when the relation is not 
   *            registered.
   */
  private RelationType retrieveRelationTypeForName(String relationTypeName)
    throws IllegalArgumentException, RelationTypeNotFoundException
  {
    if (relationTypeName == null)
       throw new IllegalArgumentException("Null relation type name");
    // Get the relation type
    RelationType result = (RelationType) typesByName.get(relationTypeName);
    if (result == null)
      throw new RelationTypeNotFoundException(relationTypeName);
    return result;
  }

  /**
   * Send a notification.
   *
   * @param type the notification type
   * @param description the human readable description
   * @param relationId the relation id of the relation
   * @param unregMBeans the mbeans removed when a relation is removed
   * @param newRole the role after an update
   * @param oldRoleValue the old role values after an update
   * @exception IllegalArgumentException for a null relation id
   * @exception RelationNotFoundException for an invalid relation id.
   */
  private void sendNotification(String type, String description,
                                String relationId, List unregMBeans,
                                Role newRole, List oldRoleValue)
    throws IllegalArgumentException, RelationNotFoundException
  {
    if (type == null)
      throw new IllegalArgumentException("null notification type");

    // Get the relation type name
    String typeName = retrieveTypeNameForId(relationId);

    // Get the relation's object name (if it has one)
    Object relation = retrieveRelationForId(relationId);
    ObjectName relationName = null;
    if (relation instanceof ObjectName)
       relationName = (ObjectName) relation;

    // Get the next sequence number
    long sequence;
    synchronized (this)
    {
      sequence = ++notificationSequence;
    }
    // Send the notification
    // REVIEW: According to the spec, the source should be a RelationService
    // that doesn't make any sense, it's not serializable
    // I use the object name
    if (type.equals(RelationNotification.RELATION_BASIC_UPDATE) ||
        type.equals(RelationNotification.RELATION_MBEAN_UPDATE))
      sendNotification(new RelationNotification(type, relationService, sequence,
        System.currentTimeMillis(), description, relationId, typeName,
        relationName, newRole.getRoleName(), newRole.getRoleValue(), 
        oldRoleValue));
    else
      sendNotification(new RelationNotification(type, relationService, sequence,
        System.currentTimeMillis(), description, relationId, typeName,
        relationName, unregMBeans));
  }

  /**
   * Validate and add the relation.
   *
   * @param relationId the relation id
   * @param relation the relation to add
   * @param relationTypeName the relation type name
   * @exception InvalidRelationIdException when it is already present
   * @exception RelationNotFoundException when an error occurs invoking the
   *            relation
   * @exception IllegalArgumentException when there is an error in the
   *            parameters
   * @exception RelationServiceNotRegisteredException when the relation
   *            service has not started.
   */
  private synchronized void validateAndAddRelation(String relationId,
                              Object relation, String relationTypeName)
    throws InvalidRelationIdException
  {
    if (relationsById.containsKey(relationId))
      throw new InvalidRelationIdException(relationId);
    relationsById.put(relationId, relation);
    idsByRelation.put(relation, relationId);
    typeNamesById.put(relationId, relationTypeName);

    // Retrieve all the roles
    RoleList roles = null;
    if (relation instanceof RelationSupport)
    {
      RelationSupport support = (RelationSupport) relation;
      roles = support.retrieveAllRoles();
    }
    else
    {
      try
      {
        roles = (RoleList) server.invoke((ObjectName) relation, 
                             "retrieveAllRoles", new Object[0], new String[0]);
      }
      catch (Exception e)
      {
        throw new RuntimeException(e.toString());
      }
    }

    // Update the roles
    Iterator iterator = roles.iterator();
    while (iterator.hasNext())
    {
      Role role = (Role) iterator.next();
      try
      {
        updateRoleMap(relationId, role, role.getRoleValue());
      }
      catch (Exception e)
      {
        throw new RuntimeException(e.toString());
      }
    }

    // We are now managing this relation
    if (relation instanceof RelationSupport)
    {
      RelationSupport support = (RelationSupport) relation;
      support.setRelationServiceManagementFlag(new Boolean(true));
    }
    else
    {
      try
      {
        Attribute attribute = new Attribute("RelationServiceManagementFlag",
                                            new Boolean(true));
        server.setAttribute((ObjectName) relation, attribute);
      }
      catch (Exception doesntImplementRelationSupportMBean) {}
    }

    // Send a notification of the addition
    try
    {
      sendRelationCreationNotification(relationId);
    }
    catch (RelationNotFoundException e)
    {
      throw new RuntimeException(e.toString());
    }
  }

  /**
   * Validate a relation type.
   *
   * @param relationType the relation to validate
   * @exception InvalidRelationTypeException when it is not valid
   */
  private void validateRelationType(RelationType relationType)
    throws InvalidRelationTypeException
  {
    // Check the role information
    HashSet roleNames = new HashSet();
    ArrayList roleInfos = (ArrayList) relationType.getRoleInfos();
    synchronized (roleInfos)
    {

      for (int i = 0; i < roleInfos.size(); i++)
      {
        RoleInfo roleInfo = (RoleInfo) roleInfos.get(i);
        if (roleInfo == null)
          throw new InvalidRelationTypeException("Null role");
        if (roleNames.contains(roleInfo.getName()))
          throw new InvalidRelationTypeException(
                  "Duplicate role name" + roleInfo.getName());
        roleNames.add(roleInfo.getName());
      }
    }
  }
}
