/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.relation;

import junit.framework.TestCase;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.Notification;
import javax.management.NotificationListener;
import javax.management.ObjectName;

import javax.management.relation.RelationNotification;
import javax.management.relation.RelationService;
import javax.management.relation.RelationSupport;
import javax.management.relation.RelationType;
import javax.management.relation.Role;
import javax.management.relation.RoleInfo;
import javax.management.relation.RoleList;
import javax.management.relation.RoleResult;
import javax.management.relation.RoleUnresolved;
import javax.management.relation.RoleUnresolvedList;

import test.compliance.relation.support.Trivial;

/**
 * Relation Support tests
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class RelationSupportTestCase
  extends TestCase
{

  // Constants -----------------------------------------------------------------

  // Attributes ----------------------------------------------------------------

  HashMap services = new HashMap();

  RoleList rolesA;
  HashMap roleInfosA = new HashMap();
  RoleList rolesB;
  HashMap roleInfosB = new HashMap();
  Role roleC1;
  Role roleC2;
  RoleList rolesC;
  HashMap roleInfosC = new HashMap();
  Role roleCX1;
  Role roleCX2;
  RoleList rolesCX;
  HashMap roleInfosCX = new HashMap();

  // Constructor ---------------------------------------------------------------

  /**
   * Construct the test
   */
  public RelationSupportTestCase(String s)
  {
    super(s);
  }

  // Tests ---------------------------------------------------------------------

  /**
   * Test the delegate constructor
   */
  public void testDelegateConstructor()
  {
    MBeanServer server = createMBeanServer();
    ObjectName service = createRelationService("test:type=service", null);
    createRolesB(null);
    RelationSupport support = null;
    try
    {
      support = new RelationSupport("id", service, server, "relationTypeB",
                                    rolesB);
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
    assertEquals("id", support.getRelationId());
    assertEquals("relationTypeB", support.getRelationTypeName());
    assertEquals("test:type=service", support.getRelationServiceName().toString());
    RoleList roleList = support.retrieveAllRoles();
    compare(rolesB, roleList);
  }

  /**
   * Test get all roles
   */
  public void testGetAllRoles()
  {
    MBeanServer server = createMBeanServer();
    ObjectName service = createRelationService("test:type=service", server);
    createRelationTypeB(service);
    createRolesB(server);
    RoleResult result = null;
    try
    {
      RelationSupport support = new RelationSupport("id", service, server, 
                                  "relationTypeB", rolesB);
      addRelation(server, service, support, "test:type=support");
      result = support.getAllRoles();
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
    checkResult(result, roleInfosB, rolesB);
  }

  /**
   * Test get referenced mbeans when not registered in service
   */
  public void testGetReferencedMBeansNotRegistered()
  {
    MBeanServer server = createMBeanServer();
    ObjectName service = createRelationService("test:type=service", null);
    createRelationTypeC(service);
    createRolesC(server);
    Map result = null;
    try
    {
      RelationSupport support = new RelationSupport("id", service, server, 
                                  "relationTypeC", rolesC);
      result = support.getReferencedMBeans();
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
    checkMBeans(result, rolesC);
  }

  /**
   * Test get referenced mbeans when registered in service
   */
  public void testGetReferencedMBeansWhenRegistered()
  {
    MBeanServer server = createMBeanServer();
    ObjectName service = createRelationService("test:type=service", server);
    createRelationTypeC(service);
    createRolesC(server);
    Map result = null;
    try
    {
      RelationSupport support = new RelationSupport("id", service, server, 
                                  "relationTypeC", rolesC);
      addRelation(server, service, support, "test:type=support");
      result = support.getReferencedMBeans();
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
    checkMBeans(result, rolesC);
  }

  /**
   * Test get Role
   */
  public void testGetRole()
  {
    MBeanServer server = createMBeanServer();
    ObjectName service = createRelationService("test:type=service", server);
    createRelationTypeC(service);
    createRolesC(server);
    List result = null;
    try
    {
      RelationSupport support = new RelationSupport("id", service, server, 
                                  "relationTypeC", rolesC);
      addRelation(server, service, support, "test:type=support");
      result = support.getRole("roleC1");
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
    compareListOfObjectNames(getRole(rolesC, "roleC1").getRoleValue(), result);
  }

  /**
   * Test get Role cardinality when not registered
   */
  public void testGetRoleCardinalityUnregistered()
  {
    MBeanServer server = createMBeanServer();
    ObjectName service = createRelationService("test:type=service", null);
    createRolesC(null);
    Integer result = null;
    RelationSupport support = null;
    try
    {
      support = new RelationSupport("id", service, server, 
                                  "relationTypeC", rolesC);
      result = support.getRoleCardinality("roleC1");
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
    assertEquals(2, result.intValue());

    try
    {
      result = support.getRoleCardinality("roleC2");
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
    assertEquals(3, result.intValue());
  }

  /**
   * Test get Role cardinality registered
   */
  public void testGetRoleCardinalityRegistered()
  {
    MBeanServer server = createMBeanServer();
    ObjectName service = createRelationService("test:type=service", server);
    createRelationTypeC(service);
    createRolesC(server);
    Integer result = null;
    RelationSupport support = null;
    try
    {
      support = new RelationSupport("id", service, server, 
                                  "relationTypeC", rolesC);
      addRelation(server, service, support, "test:type=support");
      result = support.getRoleCardinality("roleC1");
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
    assertEquals(2, result.intValue());

    try
    {
      result = support.getRoleCardinality("roleC2");
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
    assertEquals(3, result.intValue());
  }

  /**
   * Test get Roles
   */
  public void testGetRoles()
  {
    MBeanServer server = createMBeanServer();
    ObjectName service = createRelationService("test:type=service", server);
    createRelationTypeC(service);
    createRolesC(server);
    RoleResult result = null;
    RelationSupport support = null;
    try
    {
      support = new RelationSupport("id", service, server, 
                                  "relationTypeC", rolesC);
      addRelation(server, service, support, "test:type=support");
      result = support.getRoles(new String[] {"roleC1", "roleC2" });
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
    checkResult(result, roleInfosC, rolesC);

    try
    {
      result = support.getRoles(new String[] {"roleC1" });
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
    RoleList resolved = result.getRoles();
    assertEquals(1, resolved.size());
    assertEquals(0, result.getRolesUnresolved().size());
    compare(getRole(rolesC, "roleC1"), (Role) resolved.get(0));

    try
    {
      result = support.getRoles(new String[] {"roleC2" });
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
    RoleUnresolvedList unresolved = result.getRolesUnresolved();
    assertEquals(0, result.getRoles().size());
    assertEquals(1, unresolved.size());
    assertEquals("roleC2", ((RoleUnresolved)unresolved.get(0)).getRoleName());
  }

  /**
   * Test relation service flag
   */
  public void testRelationServiceFlag()
  {
    MBeanServer server = createMBeanServer();
    ObjectName service = createRelationService("test:type=service", null);
    createRolesB(null);
    RelationSupport support = null;
    try
    {
      support = new RelationSupport("id", service, server, "relationTypeB",
                                    rolesB);
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
    assertEquals(false, support.isInRelationService().booleanValue());
    support.setRelationServiceManagementFlag(new Boolean(true));
    assertEquals(true, support.isInRelationService().booleanValue());
    support.setRelationServiceManagementFlag(new Boolean(false));
    assertEquals(false, support.isInRelationService().booleanValue());
  }

  /**
   * Test retrieve all roles when unregistered
   */
  public void testRetrieveAllRolesUnRegistered()
  {
    MBeanServer server = createMBeanServer();
    ObjectName service = createRelationService("test:type=service", null);
    createRolesB(null);
    RoleList result = null;
    try
    {
      RelationSupport support = new RelationSupport("id", service, server, 
                                  "relationTypeB", rolesB);
      result = support.retrieveAllRoles();
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
    compare(rolesB, result);
  }

  /**
   * Test retrieve all roles registered
   */
  public void testRetrieveAllRolesRegistered()
  {
    MBeanServer server = createMBeanServer();
    ObjectName service = createRelationService("test:type=service", server);
    createRelationTypeB(service);
    createRolesB(server);
    RoleList result = null;
    try
    {
      RelationSupport support = new RelationSupport("id", service, server, 
                                  "relationTypeB", rolesB);
      addRelation(server, service, support, "test:type=support");
      result = support.retrieveAllRoles();
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
    compare(rolesB, result);
  }

  /**
   * Test set a role
   */
  public void testSetRole()
  {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeC(service);
         createRolesC(server);
         createRolesCX(server);
         RelationSupport support = null;
         ObjectName on = null;
         Listener listener = new Listener(RelationNotification.RELATION_MBEAN_UPDATE);
         try
         {
            support = new RelationSupport("id1", service, server, 
                                          "relationTypeC", rolesC);
            addRelation(server, service, support, "test:type=support1");
            server.addNotificationListener(service, listener, null, null);
            support.setRole(roleCX2);
            on = new ObjectName("test:type=support1");
         }
         catch(Exception e)
         {
            fail(e.toString());
         }
         RoleList shouldBe = new RoleList();
         shouldBe.add(roleC1);
         shouldBe.add(roleCX2);
         compare(shouldBe, support.retrieveAllRoles());
         RelationNotification rn = listener.check(1);
         assertEquals(new ArrayList(), rn.getMBeansToUnregister());
         assertEquals(roleCX2.getRoleValue(), rn.getNewRoleValue());
         assertEquals(on, rn.getObjectName());
         assertEquals(roleC2.getRoleValue(), rn.getOldRoleValue());
         assertEquals("id1", rn.getRelationId());
         assertEquals("relationTypeC", rn.getRelationTypeName());
         assertEquals("roleC2", rn.getRoleName());
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
  }

  /**
   * Test set roles
   * @info.todo different permutations
   */
  public void testSetRoles()
  {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeC(service);
         createRolesC(server);
         createRolesCX(server);
         RelationSupport support = null;
         ObjectName on = null;
         Listener listener = new Listener(RelationNotification.RELATION_MBEAN_UPDATE);
         RoleList shouldBe = new RoleList();
         shouldBe.add(roleC1);
         shouldBe.add(roleCX2);
         try
         {
            support = new RelationSupport("id1", service, server, 
                                          "relationTypeC", rolesC);
            addRelation(server, service, support, "test:type=support1");
            server.addNotificationListener(service, listener, null, null);
            
            support.setRoles(shouldBe);
            on = new ObjectName("test:type=support1");
         }
         catch(Exception e)
         {
            fail(e.toString());
         }
         compare(shouldBe, support.retrieveAllRoles());
         RelationNotification rn = listener.check(1);
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
  }

  /**
   * Test error handling
   */
  public void testErrors()
  {
// TODO !!!!!!!!
  }

  // Support -------------------------------------------------------------------

  private MBeanServer createMBeanServer()
  {
      return MBeanServerFactory.createMBeanServer();
  }

  private ObjectName createRelationService(String name, MBeanServer server)
  {
    ObjectName result = null;
    RelationService relationService = new RelationService(true);
    try
    {
      result = new ObjectName(name);
      services.put(result, relationService);
      if (server !=null)
        server.registerMBean(relationService, result);
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
    return result;
  }

  private ObjectName addRelation(MBeanServer server, ObjectName service,
                                 RelationSupport support, String name)
  {
    ObjectName result = null;
    try
    {
      result = new ObjectName(name);
      server.registerMBean(support, result);
      if (service != null)
      {
        RelationService relationService = (RelationService) services.get(service);
        relationService.addRelation(result);
      }
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
    return result;
  }

  private RoleInfo createRoleInfo(String name, Class mbean, 
                                  boolean read, boolean write,
                                  int min, int max)
  {
    RoleInfo result = null;
    try
    {
      result = new RoleInfo(name, mbean.getName(), read, write, min, max, "");
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
    return result;
  }

  private void createRelationType(ObjectName relationService, String name, RoleInfo[] roleInfos)
  {
    try
    {
      RelationService service = (RelationService) services.get(relationService);
      service.createRelationType(name, roleInfos);
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
  }

  private void compare(RoleList original, RoleList result)
  {
    assertEquals(original.size(), result.size());
    Iterator iterator = original.iterator();
    while (iterator.hasNext())
    {
      Role originalRole = (Role) iterator.next();
      Iterator iterator2 = result.iterator();
      while (iterator2.hasNext())
      {
        Role resultRole = (Role) iterator2.next();
        if (originalRole.getRoleName().equals(resultRole.getRoleName()))
        {
          compare(originalRole, resultRole);
          iterator2.remove();
        }
      }
    }
    assertEquals(0, result.size());   
  }

  private void compare(Role original, Role result)
  {
    assertEquals(original.getRoleName(), result.getRoleName());
    compareListOfObjectNames(original.getRoleValue(), result.getRoleValue());
  }

  private void compareListOfObjectNames(List original, List result)
  {
    assertEquals(original.size(), result.size());
    Iterator iterator = original.iterator();
    while (iterator.hasNext())
    {
      ObjectName originalBean = (ObjectName) iterator.next();
      Iterator iterator2 = result.iterator();
      while (iterator2.hasNext())
      {
        ObjectName resultBean = (ObjectName) iterator2.next();
        if (originalBean.equals(resultBean))
        {
          iterator2.remove();
        }
      }
    }
    assertEquals(0, result.size());   
  }

  private void compareListOfStrings(List original, List result)
  {
    assertEquals(original.size(), result.size());
    Iterator iterator = original.iterator();
    while (iterator.hasNext())
    {
      String originalString = (String) iterator.next();
      Iterator iterator2 = result.iterator();
      while (iterator2.hasNext())
      {
        String resultString = (String) iterator2.next();
        if (originalString.equals(resultString))
        {
          iterator2.remove();
        }
      }
    }
    assertEquals(0, result.size());   
  }

  private ObjectName createRoleValueBean(String name, Class mbean, MBeanServer server)
  {
    ObjectName result = null;
    try
    {
      result = new ObjectName(name);
      if (server != null)
      {
        server.registerMBean(mbean.newInstance(), result);
      }
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
    return result;
  }

  private void checkResult(RoleResult result, HashMap infos, RoleList roles)
  {
    checkResolved(result.getRoles(), infos, roles);
    checkUnresolved(result.getRolesUnresolved(), infos, roles);
  }

  private void checkResolved(RoleList resolved, HashMap infos, RoleList roles)
  {
    RoleList copy = (RoleList) roles.clone();
    Iterator iterator = resolved.iterator();
    while (iterator.hasNext())
    {
      Role role = (Role) iterator.next();
      String roleName = role.getRoleName();
      RoleInfo info = (RoleInfo) infos.get(roleName);
      if (info == null)
        fail("unknown role " + roleName);
      if (info.isReadable() == false)
        fail("role should not be readable " + roleName);
      Role original = removeRole(copy, roleName);
      compareListOfObjectNames(original.getRoleValue(), role.getRoleValue());
    }

    iterator = copy.iterator();
    while (iterator.hasNext())
    {
      Role role = (Role) iterator.next();
      String roleName = role.getRoleName();
      RoleInfo info = (RoleInfo) infos.get(roleName);
      if (info.isReadable() == true)
        fail("missing role " + roleName);
    }
  }

  private void checkUnresolved(RoleUnresolvedList unresolved, HashMap infos, RoleList roles)
  {
    RoleList copy = (RoleList) roles.clone();
    Iterator iterator = unresolved.iterator();
    while (iterator.hasNext())
    {
      RoleUnresolved roleUnresolved = (RoleUnresolved) iterator.next();
      String roleName = roleUnresolved.getRoleName();
      RoleInfo info = (RoleInfo) infos.get(roleName);
      if (info == null)
        fail("unknown role " + roleName);
      if (info.isReadable() == true)
        fail("role should be readable " + roleName);
      removeRole(copy, roleName);
    }

    iterator = copy.iterator();
    while (iterator.hasNext())
    {
      Role role = (Role) iterator.next();
      String roleName = role.getRoleName();
      RoleInfo info = (RoleInfo) infos.get(roleName);
      if (info.isReadable() == false)
        fail("missing unresolved role " + roleName);
    }
  }

  private Role removeRole(RoleList roles, String roleName)
  {
    Iterator iterator = roles.iterator();
    while (iterator.hasNext())
    {
      Role role = (Role) iterator.next();
      if (role.getRoleName().equals(roleName))
      {
        iterator.remove();
        return role;
      }
    }
    fail("role was not in the original " + roleName);
    return null;
  }

  private Role getRole(RoleList roles, String roleName)
  {
    Iterator iterator = roles.iterator();
    while (iterator.hasNext())
    {
      Role role = (Role) iterator.next();
      if (role.getRoleName().equals(roleName))
      {
        return role;
      }
    }
    fail("role was not in the original " + roleName);
    return null;
  }

  private void checkMBeans(Map result, RoleList roles)
  {
    // Construct what we think the value should be
    Map expected = calcMBeanRoleMap(roles);
    
    // Check the actual result
    Iterator iterator = result.entrySet().iterator();
    while (iterator.hasNext())
    {
       Map.Entry entry = (Map.Entry) iterator.next();
       ObjectName key = (ObjectName) entry.getKey();
       ArrayList roleNames = (ArrayList) entry.getValue();
       ArrayList expectedNames = (ArrayList) expected.get(key);
       if (expectedNames == null)
         fail("Unexpected object name " + key);
       compareListOfStrings(expectedNames, roleNames);
       expected.remove(key);
    }
    assertEquals(0, expected.size());
  }

  private Map calcMBeanRoleMap(RoleList roles)
  {
    HashMap result = new HashMap();
    Iterator iterator = roles.iterator();
    while (iterator.hasNext())
    {
      Role role = (Role) iterator.next();
      String roleName = role.getRoleName();
      ArrayList mbeans = (ArrayList) role.getRoleValue();
      Iterator iterator2 = mbeans.iterator();
      while (iterator2.hasNext())
      {
        ObjectName objectName = (ObjectName) iterator2.next();
        ArrayList names = (ArrayList) result.get(objectName);
        if (names == null)
        {
          names = new ArrayList();
          result.put(objectName, names);
        }
        // It seems the role name should be duplicated?
        // Include the following test if this is a bug in RI.
        // if (names.contains(roleName) == false)

        names.add(roleName);
      }
    }
    return result;
  }

  private void createRolesA(MBeanServer server)
  {
    try
    {
      ArrayList roleA1Values = new ArrayList();
      roleA1Values.add(createRoleValueBean("x:relation=a,role=1,bean=1", 
                                            Trivial.class, server));
      Role roleA1 = new Role("roleA1", roleA1Values);
      rolesA = new RoleList();
      rolesA.add(roleA1);
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
  }

  private void createRelationTypeA(ObjectName relationService)
  {
    try
    {
      RoleInfo roleInfoA1 = createRoleInfo("roleA1", Trivial.class, true, true, 1, 1);
      RoleInfo[] roleInfos = new RoleInfo[] { roleInfoA1 };
      createRelationType(relationService, "relationTypeA", roleInfos);
      for (int i=0; i < roleInfos.length; i++)
        roleInfosA.put(roleInfos[i].getName(), roleInfos[i]);
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
  }

  private void createRolesB(MBeanServer server)
  {
    try
    {
      ArrayList roleB1Values = new ArrayList();
      roleB1Values.add(createRoleValueBean("x:relation=b,role=1,bean=1", 
                                            Trivial.class, server));
      roleB1Values.add(createRoleValueBean("x:relation=b,role=1,bean=2", 
                                            Trivial.class, server));
      Role roleB1 = new Role("roleB1", roleB1Values);

      ArrayList roleB2Values = new ArrayList();
      roleB2Values.add(createRoleValueBean("x:relation=b,role=2,bean=1", 
                                            Trivial.class, server));
      roleB2Values.add(createRoleValueBean("x:relation=b,role=2,bean=2", 
                                            Trivial.class, server));
      roleB2Values.add(createRoleValueBean("x:relation=b,role=2,bean=3", 
                                            Trivial.class, server));
      Role roleB2 = new Role("roleB2", roleB2Values);

      rolesB = new RoleList();
      rolesB.add(roleB1);
      rolesB.add(roleB2);
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
  }

  private void createRelationTypeB(ObjectName relationService)
  {
    try
    {
      RoleInfo roleInfoB1 = createRoleInfo("roleB1", Trivial.class, true, false, 1, 2);
      RoleInfo roleInfoB2 = createRoleInfo("roleB2", Trivial.class, false, true, 3, 4);
      RoleInfo[] roleInfos = new RoleInfo[] { roleInfoB1, roleInfoB2 };
      createRelationType(relationService, "relationTypeB", roleInfos );
      for (int i=0; i < roleInfos.length; i++)
        roleInfosB.put(roleInfos[i].getName(), roleInfos[i]);
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
  }

  private void createRolesC(MBeanServer server)
  {
    try
    {
      ArrayList roleC1Values = new ArrayList();
      roleC1Values.add(createRoleValueBean("x:relation=c,role=1,bean=1", 
                                            Trivial.class, server));
      roleC1Values.add(createRoleValueBean("x:relation=c,role=1,bean=1", 
                                            Trivial.class, null));
      roleC1 = new Role("roleC1", roleC1Values);

      ArrayList roleC2Values = new ArrayList();
      roleC2Values.add(createRoleValueBean("x:relation=c,role=1,bean=1", 
                                            Trivial.class, null));
      roleC2Values.add(createRoleValueBean("x:relation=c,role=2,bean=1", 
                                            Trivial.class, server));
      roleC2Values.add(createRoleValueBean("x:relation=c,role=2,bean=2", 
                                            Trivial.class, server));
      roleC2 = new Role("roleC2", roleC2Values);

      rolesC = new RoleList();
      rolesC.add(roleC1);
      rolesC.add(roleC2);
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
  }

  private void createRelationTypeC(ObjectName relationService)
  {
    try
    {
      RoleInfo roleInfoC1 = createRoleInfo("roleC1", Trivial.class, true, false, 1, 2);
      RoleInfo roleInfoC2 = createRoleInfo("roleC2", Trivial.class, false, true, 3, 4);
      RoleInfo[] roleInfos = new RoleInfo[] { roleInfoC1, roleInfoC2 };
      createRelationType(relationService, "relationTypeC", roleInfos );
      for (int i=0; i < roleInfos.length; i++)
        roleInfosC.put(roleInfos[i].getName(), roleInfos[i]);
    }
    catch(Exception e)
    {
      fail(e.toString());
    }
  }

   private void createRolesCX(MBeanServer server)
   {
      try
      {
         ArrayList roleCX1Values = new ArrayList();
         roleCX1Values.add(createRoleValueBean("x:relation=c,role=2,bean=1", 
                                              Trivial.class, null));
         roleCX1 = new Role("roleC1", roleCX1Values);

         ArrayList roleCX2Values = new ArrayList();
         roleCX2Values.add(createRoleValueBean("x:relation=c,role=1,bean=1", 
                                              Trivial.class, null));
         roleCX2Values.add(createRoleValueBean("x:relation=c,role=1,bean=2", 
                                              Trivial.class, server));
         roleCX2Values.add(createRoleValueBean("x:relation=c,role=1,bean=3", 
                                              Trivial.class, server));
         roleCX2Values.add(createRoleValueBean("x:relation=c,role=1,bean=4", 
                                              Trivial.class, server));
         roleCX2 = new Role("roleC2", roleCX2Values);

         rolesCX = new RoleList();
         rolesCX.add(roleCX1);
         rolesCX.add(roleCX2);
      }
      catch(Exception e)
      {
         fail(e.toString());
      }
   }

   private class Listener
      implements NotificationListener
   {
      String type;
      HashSet notifications = new HashSet();
      public Listener(String type)
      {
         this.type = type;
      }
      public void handleNotification(Notification n, Object h)
      {
         notifications.add(n);
      }
      public RelationNotification check(int size)
      {
         RelationNotification result = null;
         assertEquals(size, notifications.size());
         Iterator iterator = notifications.iterator();
         while (iterator.hasNext())
         {
            RelationNotification rn = (RelationNotification) iterator.next();
            assertEquals(type, rn.getType());
            result = rn;
         }
         return result;
      }
   }
}
