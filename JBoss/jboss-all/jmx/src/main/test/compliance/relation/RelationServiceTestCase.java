/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.relation;

import test.compliance.relation.support.Trivial;

import junit.framework.TestCase;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

import javax.management.InstanceNotFoundException;
import javax.management.MBeanNotificationInfo;
import javax.management.MBeanServer;
import javax.management.Notification;
import javax.management.NotificationListener;
import javax.management.MBeanServerFactory;
import javax.management.ObjectName;

import javax.management.relation.InvalidRelationIdException;
import javax.management.relation.InvalidRelationServiceException;
import javax.management.relation.InvalidRelationTypeException;
import javax.management.relation.InvalidRoleValueException;
import javax.management.relation.RelationNotFoundException;
import javax.management.relation.RelationNotification;
import javax.management.relation.RelationService;
import javax.management.relation.RelationServiceNotRegisteredException;
import javax.management.relation.RelationSupport;
import javax.management.relation.RelationTypeNotFoundException;
import javax.management.relation.RelationTypeSupport;
import javax.management.relation.Role;
import javax.management.relation.RoleInfo;
import javax.management.relation.RoleInfoNotFoundException;
import javax.management.relation.RoleList;
import javax.management.relation.RoleNotFoundException;
import javax.management.relation.RoleResult;
import javax.management.relation.RoleStatus;
import javax.management.relation.RoleUnresolved;
import javax.management.relation.RoleUnresolvedList;

/**
 * Relation Service tests
 *
 * //TODO test internal relations
 * //TODO test relations as mbeans in roles
 * //TODO test multiple relation services (avoid future mods adding static data)
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class RelationServiceTestCase
   extends TestCase
{
   // Constants -----------------------------------------------------------------

   // Attributes ----------------------------------------------------------------

   HashMap services = new HashMap();

   RoleList rolesA;
   HashMap roleInfosA = new HashMap();
   Role roleB1;
   Role roleB2;
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
   Role roleCZ2;
   RoleList rolesCZ;
   HashMap roleInfosCZ = new HashMap();
   Role roleCZZ;
   RoleList rolesCZZ;
   HashMap roleInfosCZZ = new HashMap();
   Role roleCZZZ;
   RoleList rolesCZZZ;
   HashMap roleInfosCZZZ = new HashMap();

   // Constructor ---------------------------------------------------------------

   /**
    * Construct the test
    */
   public RelationServiceTestCase(String s)
   {
      super(s);
   }

   // Tests ---------------------------------------------------------------------

   /**
    * Test the constructor
    */
   public void testConstructor() throws Exception
   {
      RelationService rs = null;
      rs = new RelationService(true);
      assertEquals(true, rs.getPurgeFlag());

      rs = new RelationService(false);
      assertEquals(false, rs.getPurgeFlag());
   }

   /**
    * Test add a relation
    */
   public void testAddRelation() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeB(service);
         createRolesB(server);
         RelationSupport support = null;
         ObjectName rsupp = null;
         String result = null;
         Listener listener = new Listener(RelationNotification.RELATION_MBEAN_CREATION);
         RelationService rs = (RelationService) services.get(service);
         server.addNotificationListener(service, listener, null, null);
         support = new RelationSupport("id", service, server, "relationTypeB",
                                       rolesB);
         rsupp = new ObjectName("test:add=relation");
         server.registerMBean(support, rsupp);
         rs.addRelation(rsupp);
         result = rs.isRelation(rsupp);
         assertEquals("id", result);
         listener.check(1);
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test add a relation errors
    */
   public void testAddRelationErrors() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", null);
         RelationService rs = (RelationService) services.get(service);
         createRelationTypeB(service);
         createRolesB(server);
         RelationSupport support = null;
         ObjectName name = null;
         support = new RelationSupport("id", service, server, "relationTypeB",
                                          rolesB);
         name = new ObjectName("test:type=relation");
         server.registerMBean(support, name);

         boolean caught = false;
         try
         {
            rs.addRelation(null);
         }
         catch (IllegalArgumentException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("addRelation allows null relation");

         caught = false;
         try
         {
            rs.addRelation(name);
         }
         catch (RelationServiceNotRegisteredException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("addRelation allowed when not registered");

         ObjectName badRelation = null;
         server.registerMBean(rs, service);
         badRelation = new ObjectName("test:type=bad");
         server.registerMBean(new Trivial(), badRelation);

         caught = false;
         try
         {
            rs.addRelation(badRelation);
         }
         catch (NoSuchMethodException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("addRelation allowed when not a relation");

         caught = false;
         try
         {
            rs.addRelation(name);
            rs.addRelation(name);
         }
         catch (InvalidRelationIdException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("addRelation allows duplicate relation ids");

         rs.removeRelation("id");
         server.unregisterMBean(name);

         caught = false;
         try
         {
            rs.addRelation(name);
         }
         catch (InstanceNotFoundException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("addRelation allows unregistered relation");

         ObjectName service2 = createRelationService("test:type=service2", null);
         RelationService rs2 = (RelationService) services.get(service);
         createRelationTypeB(service2);
         RelationSupport support2 = null;
         ObjectName name2 = null;
         support = new RelationSupport("id", service2, server, "relationTypeB",
                                          rolesB);
         name2 = new ObjectName("test:type=relation2");
         server.registerMBean(support, name2);

         caught = false;
         try
         {
            rs.addRelation(name2);
         }
         catch (InvalidRelationServiceException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("addRelation allows registration in the wrong relation service");

         support = new RelationSupport("id", service, server, "relationTypeX",
                                          rolesB);
         name = new ObjectName("test:type=relationX");
         server.registerMBean(support, name);

         caught = false;
         try
         {
            rs.addRelation(name);
         }
         catch (RelationTypeNotFoundException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("addRelation allows registration with invalid relation type");

         createRelationTypeC(service);
         createRolesC(server);
         createRolesCZ(server);
         support = new RelationSupport("idC", service, server, "relationTypeB",
                                          rolesC);
         name = new ObjectName("test:type=relationC");
         server.registerMBean(support, name);

         caught = false;
         try
         {
            rs.addRelation(name);
         }
         catch (RoleNotFoundException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("addRelation allows registration with invalid role name");

         support = new RelationSupport("idCZ", service, server, "relationTypeC",
                                          rolesCZ);
         name = new ObjectName("test:type=relationCZ");
         server.registerMBean(support, name);

         caught = false;
         try
         {
            rs.addRelation(name);
         }
         catch (InvalidRoleValueException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("addRelation allows registration with invalid role value");
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test add a relation type
    */
   public void testAddRelationType() throws Exception
   {
      RoleInfo roleInfo1 = null;
      RoleInfo roleInfo2 = null;
      RoleInfo[] roleInfos = null;
      RelationService rs = null;
      ArrayList result = null;
      RoleInfo result1 = null;
      RoleInfo result2 = null;
      roleInfo1 = new RoleInfo("roleInfo1", Trivial.class.getName());
      roleInfo2 = new RoleInfo("roleInfo2", Trivial.class.getName());
      roleInfos = new RoleInfo[] { roleInfo1, roleInfo2 };
      RelationTypeSupport rtsupp = new RelationTypeSupport("RelationTypeName",
                                                  roleInfos);
      rs = new RelationService(true);
      rs.addRelationType(rtsupp);
      result = (ArrayList) rs.getRoleInfos("RelationTypeName");
      result1 = rs.getRoleInfo("RelationTypeName", "roleInfo1");
      result2 = rs.getRoleInfo("RelationTypeName", "roleInfo2");

      // Check the roleInfos
      assertEquals(2, result.size());
      assertEquals(roleInfo1.toString(), result1.toString());
      assertEquals(roleInfo2.toString(), result2.toString());
   }

   /**
    * Test create relation type errors
    */
   public void testAddRelationTypeErrors() throws Exception
   {
      RoleInfo roleInfo1 = null;
      RoleInfo roleInfo2 = null;
      RoleInfo[] roleInfos = null;
      RelationService rs = null;
      RelationTypeSupport rtsupp = null;

      // Null relation type
      boolean caught = false;
      try
      {
         roleInfo1 = new RoleInfo("roleInfo1", Trivial.class.getName());
         roleInfo2 = new RoleInfo("roleInfo2", Trivial.class.getName());
         roleInfos = new RoleInfo[] { roleInfo1, roleInfo2 };
         rs = new RelationService(true);
         rs.addRelationType(null);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("addRelationType allows null relation type");

      // Duplicate relation types
      caught = false;
      try
      {
         rtsupp = new RelationTypeSupport("RelationTypeName", roleInfos);
         rs.addRelationType(rtsupp);
         rs.addRelationType(rtsupp);
      }
      catch (InvalidRelationTypeException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("addRelationType allows duplication relation types");

      // Duplicate role infos
      caught = false;
      try
      {
         roleInfos[1] = roleInfos[0];
         rtsupp = new RelationTypeSupport("RelationTypeName1", roleInfos);
         rs.addRelationType(rtsupp);
      }
      catch (InvalidRelationTypeException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("addRelationType allows duplicate role names");

      // Null role info
      caught = false;
      try
      {
         roleInfos[1] = null;
         rtsupp = new RelationTypeSupport("RelationTypeName1", roleInfos);
         rs.addRelationType(rtsupp);
      }
      catch (InvalidRelationTypeException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("addRelationType allows null role info");

      // No role info
      caught = false;
      try
      {
         rtsupp = new RelationTypeSupport("RelationTypeName1", new RoleInfo[0]);
         rs.addRelationType(rtsupp);
      }
      catch (InvalidRelationTypeException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("addRelationType allows no role info");
   }

   /**
    * Test check role reading
    */
   public void testCheckRoleReadingExternal() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeB(service);
         createRolesB(server);
         RelationSupport support = null;
         Integer readB1 = null;
         Integer readB2 = null;
         RelationService rs = (RelationService) services.get(service);
         support = new RelationSupport("id", service, server, 
                                          "relationTypeB", rolesB);
         addRelation(server, service, support, "test:type=support");
         readB1 = rs.checkRoleReading("roleB1", "relationTypeB");
         readB2 = rs.checkRoleReading("roleB2", "relationTypeB");

         assertEquals(0, readB1.intValue());
         assertEquals(RoleStatus.ROLE_NOT_READABLE, readB2.intValue());
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test check role reading errors
    */
   public void testCheckRoleReadingErrors() throws Exception
   {
      ObjectName service = createRelationService("test:type=service", null);
      RelationService rs = (RelationService) services.get(service);
      createRelationTypeB(service);
      createRolesB(null);

      boolean caught = false;
      try
      {
         rs.checkRoleReading(null, "relationTypeB");
      }
      catch(IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("checkRoleReading allows null role name");

      caught = false;
      try
      {
         rs.checkRoleReading("roleB1", null);
      }
      catch(IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("checkRoleReading allows null relation name");

      caught = false;
      try
      {
         rs.checkRoleReading("roleB1", "rubbish");
      }
      catch(RelationTypeNotFoundException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("checkRoleReading allows invalid relation type name");
   }

   /**
    * Test check role writing
    */
   public void testCheckRoleWritingExternal() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeB(service);
         createRolesB(server);
         RelationSupport support = null;
         Integer writeB1normal = null;
         Integer writeB2normal = null;
         Integer writeB1init = null;
         Integer writeB2init = null;
         RelationService rs = (RelationService) services.get(service);
         support = new RelationSupport("id", service, server, 
                                       "relationTypeB", rolesB);
         addRelation(server, service, support, "test:type=support");
         writeB1normal = rs.checkRoleWriting(roleB1, "relationTypeB", new Boolean(false));
         writeB2normal = rs.checkRoleWriting(roleB2, "relationTypeB", new Boolean(false));
         writeB1init = rs.checkRoleWriting(roleB1, "relationTypeB", new Boolean(true));
         writeB2init = rs.checkRoleWriting(roleB2, "relationTypeB", new Boolean(true));
         assertEquals(RoleStatus.ROLE_NOT_WRITABLE, writeB1normal.intValue());
         assertEquals(0, writeB2normal.intValue());
         assertEquals(0, writeB1init.intValue());
         assertEquals(0, writeB2init.intValue());
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test check role writing errors
    */
   public void testCheckRoleWritingErrors() throws Exception
   {
      ObjectName service = createRelationService("test:type=service", null);
      RelationService rs = (RelationService) services.get(service);
      createRelationTypeB(service);
      createRolesB(null);

      boolean caught = false;
      try
      {
         rs.checkRoleWriting(null, "relationTypeB", new Boolean(true));
      }
      catch(IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("checkRoleWriting allows null role name");

      caught = false;
      try
      {
         rs.checkRoleWriting(roleB1, null, new Boolean(true));
      }
      catch(IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("checkRoleWriting allows null relation name");

      caught = false;
      try
      {
         rs.checkRoleWriting(roleB1, "relationTypeB", null);
      }
      catch(IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("checkRoleWriting allows null init flag");

      caught = false;
      try
      {
         rs.checkRoleWriting(roleB1, "rubbish", new Boolean(true));
      }
      catch(RelationTypeNotFoundException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("checkRoleWriting allows invalid relation type name");
   }

   /**
    * Test create relation
    */
   public void testCreateRelation() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeB(service);
         createRolesB(server);
         RelationSupport support = null;
         Listener listener = new Listener(RelationNotification.RELATION_BASIC_CREATION);
         RelationService rs = (RelationService) services.get(service);
         server.addNotificationListener(service, listener, null, null);
         rs.createRelation("id", "relationTypeB", rolesB);
         boolean result = rs.hasRelation("id").booleanValue();
         assertEquals(true, result);
         listener.check(1);
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test create relation errors
    */
   public void testCreateRelationErrors() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         RelationService rs = (RelationService) services.get(service);
         createRelationTypeB(service);
         createRolesB(server);
         createRelationTypeC(service);
         createRolesC(server);
         createRolesCZ(server);
         createRolesCZZ(server);

         boolean caught = false;
         try
         {
            rs.createRelation(null, "relationTypeC", rolesC);
         }
         catch(IllegalArgumentException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("createRelation allows null relation id");
        
         caught = false;
         try
         {
            rs.createRelation("relationId", null, rolesC);
         }
         catch(IllegalArgumentException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("createRelation allows null relation type name");
        
         caught = false;
         try
         {
            rs.createRelation("relationId", "rubbish", rolesC);
         }
         catch(RelationTypeNotFoundException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("createRelation allows invalid relation type name");
        
         caught = false;
         try
         {
            RoleList roleList = new RoleList();
            roleList.add(roleC1);
            roleList.add(roleB1);
            rs.createRelation("relationId", "relationTypeC", roleList);
         }
         catch(RoleNotFoundException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("createRelation allows invalid role name");

         caught = false;
         try
         {
            RoleList roleList = new RoleList();
            roleList.add(roleC1);
            roleList.add(roleCZ2);
            rs.createRelation("relationId", "relationTypeC", roleList);
         }
         catch(InvalidRoleValueException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("createRelation allows a role below the minimum");
        
         caught = false;
         try
         {
            RoleList roleList = new RoleList();
            roleList.add(roleC1);
            roleList.add(roleCZZ);
            rs.createRelation("relationId", "relationTypeC", roleList);
         }
         catch(InvalidRoleValueException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("createRelation allows a role above the maximum");
        
         caught = false;
         try
         {
            RoleList roleList = new RoleList();
            roleList.add(roleC1);
            roleList.add(roleCZZZ);
            rs.createRelation("relationId", "relationTypeC", roleList);
         }
         catch(InvalidRoleValueException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("createRelation allows a role with unregistered beans");
        
         caught = false;
         try
         {
            rs.createRelation("relationId", "relationTypeC", rolesC);
            rs.createRelation("relationId", "relationTypeC", rolesC);
         }
         catch(InvalidRelationIdException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("createRelation allows duplicate relation id");

         server.unregisterMBean(service);
         caught = false;
         try
         {
            rs.createRelation("relationId2", "relationTypeC", rolesC);
         }
         catch(RelationServiceNotRegisteredException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("FAILS IN RI: createRelation allowed when not registered");
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test create relation type, getRoleInfo and getRoleInfos
    */
   public void testCreationRelationType() throws Exception
   {
      RoleInfo roleInfo1 = null;
      RoleInfo roleInfo2 = null;
      RoleInfo[] roleInfos = null;
      RelationService rs = null;
      ArrayList result = null;
      RoleInfo result1 = null;
      RoleInfo result2 = null;
      roleInfo1 = new RoleInfo("roleInfo1", Trivial.class.getName());
      roleInfo2 = new RoleInfo("roleInfo2", Trivial.class.getName());
      roleInfos = new RoleInfo[] { roleInfo1, roleInfo2 };
      rs = new RelationService(true);
      rs.createRelationType("RelationTypeName", roleInfos);
      result = (ArrayList) rs.getRoleInfos("RelationTypeName");
      result1 = rs.getRoleInfo("RelationTypeName", "roleInfo1");
      result2 = rs.getRoleInfo("RelationTypeName", "roleInfo2");

      // Check the roleInfos
      assertEquals(2, result.size());
      assertEquals(roleInfo1.toString(), result1.toString());
      assertEquals(roleInfo2.toString(), result2.toString());
   }

   /**
    * Test create relation type errors
    */
   public void testCreateRelationTypeErrors() throws Exception
   {
      RoleInfo roleInfo1 = null;
      RoleInfo roleInfo2 = null;
      RoleInfo[] roleInfos = null;
      RelationService rs = null;

      // Null relation type name
      boolean caught = false;
      try
      {
         roleInfo1 = new RoleInfo("roleInfo1", Trivial.class.getName());
         roleInfo2 = new RoleInfo("roleInfo2", Trivial.class.getName());
         roleInfos = new RoleInfo[] { roleInfo1, roleInfo2 };
         rs = new RelationService(true);
         rs.createRelationType(null, roleInfos);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("createRelationType allows null relation type name");

      // Null role infos
      caught = false;
      try
      {
         rs.createRelationType("relationTypeName", null);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("createRelationType allows null role infos");

      // Duplicate relation types
      caught = false;
      try
      {
         rs.createRelationType("relationTypeName", roleInfos);
         rs.createRelationType("relationTypeName", roleInfos);
      }
      catch (InvalidRelationTypeException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("createRelationType allows duplicate relation type names");

      // Duplicate role infos
      caught = false;
      try
      {
         roleInfos[1] = roleInfos[0];
         rs.createRelationType("relationTypeName1", roleInfos);
      }
      catch (InvalidRelationTypeException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("createRelationType allows duplicate role names");

      // Null role info
      caught = false;
      try
      {
         roleInfos[1] = null;
         rs.createRelationType("relationTypeName1", roleInfos);
      }
      catch (InvalidRelationTypeException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("createRelationType allows null role info");

      // No role info
      caught = false;
      try
      {
         rs.createRelationType("relationTypeName1", new RoleInfo[0]);
      }
      catch (InvalidRelationTypeException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("createRelationType allows no role info");
   }

   /**
    * Test getRoleInfo errors
    */
   public void testGetRoleInfoErrors() throws Exception
   {
      RoleInfo roleInfo1 = null;
      RoleInfo roleInfo2 = null;
      RoleInfo[] roleInfos = null;
      RelationService rs = null;
      ArrayList result = null;
      RoleInfo result1 = null;
      roleInfo1 = new RoleInfo("roleInfo1", Trivial.class.getName());
      roleInfo2 = new RoleInfo("roleInfo2", Trivial.class.getName());
      roleInfos = new RoleInfo[] { roleInfo1, roleInfo2 };
      rs = new RelationService(true);
      rs.createRelationType("RelationTypeName", roleInfos);

      boolean caught = false;
      try
      {
         rs.getRoleInfo(null, "roleInfo1");
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("getRoleInfo allows null relation type name");

      caught = false;
      try
      {
         rs.getRoleInfo("RelationTypeName", null);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("getRoleInfo allows null role info name");

      caught = false;
      try
      {
         rs.getRoleInfo("RelationTypeNameX", "roleInfo1");
      }
      catch (RelationTypeNotFoundException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("getRoleInfo allows non-existent relation type name");

      caught = false;
      try
      {
         rs.getRoleInfo("RelationTypeName", "roleInfoX");
      }
      catch (RoleInfoNotFoundException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("getRoleInfo allows non-existent role info name");
   }

   /**
    * Test getRoleInfos errors
    */
   public void testGetRoleInfosErrors() throws Exception
   {
      RoleInfo roleInfo1 = null;
      RoleInfo roleInfo2 = null;
      RoleInfo[] roleInfos = null;
      RelationService rs = null;
      ArrayList result = null;
      RoleInfo result1 = null;
      roleInfo1 = new RoleInfo("roleInfo1", Trivial.class.getName());
      roleInfo2 = new RoleInfo("roleInfo2", Trivial.class.getName());
      roleInfos = new RoleInfo[] { roleInfo1, roleInfo2 };
      rs = new RelationService(true);
      rs.createRelationType("RelationTypeName", roleInfos);

      boolean caught = false;
      try
      {
         rs.getRoleInfos(null);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("getRoleInfos allows null relation type name");

      caught = false;
      try
      {
         rs.getRoleInfos("RelationTypeNameX");
      }
      catch (RelationTypeNotFoundException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("getRoleInfos allows non-existent relation type name");
   }

   /**
    * Test find associated mbeans
    * 
    * REVIEW: This test needs more thought
    */
   public void testFindAssociatedMBeansExternal() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeC(service);
         createRolesC(server);
         createRelationTypeCX(service);
         createRolesCX(server);
         Map result1 = null;
         Map result2 = null;
         Map result3 = null;
         Map result4 = null;
         Map result5 = null;
         Map result6 = null;
         Map result7 = null;
         Map result8 = null;
         Map result9 = null;
         Map result10 = null;
         Map result11 = null;
         Map result12 = null;
         Map result13 = null;
         Map result14 = null;
         Map result15 = null;
         Map result16 = null;
         RelationSupport supportCX = new RelationSupport("idcx", service, server, 
                                       "relationTypeCX", rolesCX);
         RelationSupport supportC = new RelationSupport("idc", service, server, 
                                       "relationTypeC", rolesC);
         addRelation(server, service, supportCX, "test:type=supportCX");
         addRelation(server, service, supportC, "test:type=supportC");
         RelationService rs = (RelationService) services.get(service);
         result1 = rs.findAssociatedMBeans(new ObjectName("x:relation=c,role=1,bean=1"), null, null);
         result2 = rs.findAssociatedMBeans(new ObjectName("x:relation=c,role=1,bean=1"), null, null);
         result3 = rs.findAssociatedMBeans(new ObjectName("x:relation=c,role=1,bean=1"), "relationTypeCX", null);
         result4 = rs.findAssociatedMBeans(new ObjectName("x:relation=c,role=1,bean=1"), "relationTypeC", null);
         result5 = rs.findAssociatedMBeans(new ObjectName("x:relation=c,role=1,bean=1"), null, "roleC1");
         result6 = rs.findAssociatedMBeans(new ObjectName("x:relation=c,role=1,bean=1"), null, "roleC2");
         result7 = rs.findAssociatedMBeans(new ObjectName("x:relation=c,role=1,bean=1"), null, "roleC1");
         result8 = rs.findAssociatedMBeans(new ObjectName("x:relation=c,role=1,bean=1"), null, "roleC2");
         result9 = rs.findAssociatedMBeans(new ObjectName("x:relation=c,role=1,bean=1"), "relationTypeCX", "roleC1");
         result10 = rs.findAssociatedMBeans(new ObjectName("x:relation=c,role=1,bean=1"), "relationTypeC", "roleC1");
         result11 = rs.findAssociatedMBeans(new ObjectName("x:relation=c,role=1,bean=1"), "relationTypeCX", "roleC2");
         result12 = rs.findAssociatedMBeans(new ObjectName("x:relation=c,role=1,bean=1"), "relationTypeC", "roleC2");
         result13 = rs.findAssociatedMBeans(new ObjectName("x:relation=c,role=1,bean=1"), "relationTypeCX", "roleC1");
         result14 = rs.findAssociatedMBeans(new ObjectName("x:relation=c,role=1,bean=1"), "relationTypeC", "roleC1");
         result15 = rs.findAssociatedMBeans(new ObjectName("x:relation=c,role=1,bean=1"), "relationTypeCX", "roleC2");
         result16 = rs.findAssociatedMBeans(new ObjectName("x:relation=c,role=1,bean=1"), "relationTypeC", "roleC2");
         assertEquals(5, result1.size());
         assertEquals(5, result2.size());
         assertEquals(4, result3.size());
         assertEquals(2, result4.size());
         assertEquals(2, result5.size());
         assertEquals(5, result6.size());
         assertEquals(2, result7.size());
         assertEquals(5, result8.size());
         assertEquals(0, result9.size());
         assertEquals(2, result10.size());
         assertEquals(4, result11.size());
         assertEquals(2, result12.size());
         assertEquals(0, result13.size());
         assertEquals(2, result14.size());
         assertEquals(4, result15.size());
         assertEquals(2, result16.size());
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test find associated mbeans errors
    */
   public void testFindAssociatedMBeansErrors() throws Exception
   {
      RelationService rs = new RelationService(true);
      
      boolean caught = false;
      try
      {
         rs.findAssociatedMBeans(null, null, null);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail ("findAssociatedMBeans allows a null mbean name");
   }

   /**
    * Test find referencing relations
    * 
    * REVIEW: This test needs more thought
    */
   public void testFindReferencingRelationsExternal() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeC(service);
         createRolesC(server);
         createRelationTypeCX(service);
         createRolesCX(server);
         Map result1 = null;
         Map result2 = null;
         Map result3 = null;
         Map result4 = null;
         Map result5 = null;
         Map result6 = null;
         Map result7 = null;
         Map result8 = null;
         Map result9 = null;
         Map result10 = null;
         Map result11 = null;
         Map result12 = null;
         Map result13 = null;
         Map result14 = null;
         Map result15 = null;
         Map result16 = null;
         RelationSupport supportCX = new RelationSupport("idcx", service, server, 
                                       "relationTypeCX", rolesCX);
         RelationSupport supportC = new RelationSupport("idc", service, server, 
                                       "relationTypeC", rolesC);
         addRelation(server, service, supportCX, "test:type=supportCX");
         addRelation(server, service, supportC, "test:type=supportC");
         RelationService rs = (RelationService) services.get(service);
         result1 = rs.findReferencingRelations(new ObjectName("x:relation=c,role=1,bean=1"), null, null);
         result2 = rs.findReferencingRelations(new ObjectName("x:relation=c,role=1,bean=1"), null, null);
         result3 = rs.findReferencingRelations(new ObjectName("x:relation=c,role=1,bean=1"), "relationTypeCX", null);
         result4 = rs.findReferencingRelations(new ObjectName("x:relation=c,role=1,bean=1"), "relationTypeC", null);
         result5 = rs.findReferencingRelations(new ObjectName("x:relation=c,role=1,bean=1"), null, "roleC1");
         result6 = rs.findReferencingRelations(new ObjectName("x:relation=c,role=1,bean=1"), null, "roleC2");
         result7 = rs.findReferencingRelations(new ObjectName("x:relation=c,role=1,bean=1"), null, "roleC1");
         result8 = rs.findReferencingRelations(new ObjectName("x:relation=c,role=1,bean=1"), null, "roleC2");
         result9 = rs.findReferencingRelations(new ObjectName("x:relation=c,role=1,bean=1"), "relationTypeCX", "roleC1");
         result10 = rs.findReferencingRelations(new ObjectName("x:relation=c,role=1,bean=1"), "relationTypeC", "roleC1");
         result11 = rs.findReferencingRelations(new ObjectName("x:relation=c,role=1,bean=1"), "relationTypeCX", "roleC2");
         result12 = rs.findReferencingRelations(new ObjectName("x:relation=c,role=1,bean=1"), "relationTypeC", "roleC2");
         result13 = rs.findReferencingRelations(new ObjectName("x:relation=c,role=1,bean=1"), "relationTypeCX", "roleC1");
         result14 = rs.findReferencingRelations(new ObjectName("x:relation=c,role=1,bean=1"), "relationTypeC", "roleC1");
         result15 = rs.findReferencingRelations(new ObjectName("x:relation=c,role=1,bean=1"), "relationTypeCX", "roleC2");
         result16 = rs.findReferencingRelations(new ObjectName("x:relation=c,role=1,bean=1"), "relationTypeC", "roleC2");
         assertEquals(2, result1.size());
         assertEquals(2, result2.size());
         assertEquals(1, result3.size());
         assertEquals(1, result4.size());
         assertEquals(1, result5.size());
         assertEquals(2, result6.size());
         assertEquals(1, result7.size());
         assertEquals(2, result8.size());
         assertEquals(0, result9.size());
         assertEquals(1, result10.size());
         assertEquals(1, result11.size());
         assertEquals(1, result12.size());
         assertEquals(0, result13.size());
         assertEquals(1, result14.size());
         assertEquals(1, result15.size());
         assertEquals(1, result16.size());
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test find referencing relations errors
    */
   public void testFindReferencingRelationsErrors() throws Exception
   {
      RelationService rs = new RelationService(true);
      
      boolean caught = false;
      try
      {
         rs.findReferencingRelations(null, null, null);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail ("findReferencingRelations allows a null mbean name");
   }

   /**
    * Test find relations of types
    * 
    * REVIEW: This test needs more thought
    */
   public void testFindRelationsOfTypeExternal() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeA(service);
         createRolesA(server);
         createRelationTypeB(service);
         createRolesB(server);
         createRelationTypeC(service);
         createRolesC(server);
         createRelationTypeCX(service);
         createRolesCX(server);
         List result1 = null;
         List result2 = null;
         List result3 = null;
         List result4 = null;
         RelationSupport supportA1 = new RelationSupport("ida1", service, server, 
                                          "relationTypeA", rolesA);
         RelationSupport supportA2 = new RelationSupport("ida2", service, server, 
                                          "relationTypeA", rolesA);
         RelationSupport supportCX = new RelationSupport("idcx", service, server, 
                                          "relationTypeCX", rolesCX);
         RelationSupport supportC = new RelationSupport("idc", service, server, 
                                          "relationTypeC", rolesC);
         addRelation(server, service, supportA1, "test:type=supportA1");
         addRelation(server, service, supportA2, "test:type=supportA2");
         addRelation(server, service, supportCX, "test:type=supportCX");
         addRelation(server, service, supportC, "test:type=supportC");
         RelationService rs = (RelationService) services.get(service);
         result1 = rs.findRelationsOfType("relationTypeA");
         result2 = rs.findRelationsOfType("relationTypeB");
         result3 = rs.findRelationsOfType("relationTypeC");
         result4 = rs.findRelationsOfType("relationTypeCX");
         assertEquals(2, result1.size());
         assertEquals(0, result2.size());
         assertEquals(1, result3.size());
         assertEquals(1, result4.size());
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test find relations of type errors
    */
   public void testFindRelationsOfTypeErrors() throws Exception
   {
      RelationService rs = new RelationService(true);
      
      boolean caught = false;
      try
      {
         rs.findRelationsOfType(null);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail ("findRelationsOfType allows a null relation type name");

      caught = false;
      try
      {
         rs.findRelationsOfType("rubbish");
      }
      catch (RelationTypeNotFoundException e)
      {
         caught = true;
      }
      if (caught == false)
         fail ("findRelationsOfType allows an invalid relation type name");
   }

   /**
    * Test get all relations ids
    * 
    * REVIEW: This test needs more thought
    */
   public void testGetAllRelationsIdsExternal() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeA(service);
         createRolesA(server);
         createRelationTypeB(service);
         createRolesB(server);
         createRelationTypeC(service);
         createRolesC(server);
         createRelationTypeCX(service);
         createRolesCX(server);
         List result = null;
         RelationSupport supportA1 = new RelationSupport("ida1", service, server, 
                                          "relationTypeA", rolesA);
         RelationSupport supportA2 = new RelationSupport("ida2", service, server, 
                                          "relationTypeA", rolesA);
         RelationSupport supportCX = new RelationSupport("idcx", service, server, 
                                          "relationTypeCX", rolesCX);
         RelationSupport supportC = new RelationSupport("idc", service, server, 
                                          "relationTypeC", rolesC);
         addRelation(server, service, supportA1, "test:type=supportA1");
         addRelation(server, service, supportA2, "test:type=supportA2");
         addRelation(server, service, supportCX, "test:type=supportCX");
         addRelation(server, service, supportC, "test:type=supportC");
         RelationService rs = (RelationService) services.get(service);
         result = rs.getAllRelationIds();
         assertEquals(4, result.size());
         assertEquals(true, result.contains("ida1"));
         assertEquals(true, result.contains("ida2"));
         assertEquals(true, result.contains("idcx"));
         assertEquals(true, result.contains("idc"));
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test get all relation type names
    */
   public void testGetAllRelationTypeNames() throws Exception
   {
      RelationService rs = new RelationService(true);
      assertEquals(0, rs.getAllRelationTypeNames().size());

      RoleInfo roleInfo1 = null;
      RoleInfo roleInfo2 = null;
      RoleInfo[] roleInfos = null;
      roleInfo1 = new RoleInfo("roleInfo1", Trivial.class.getName());
      roleInfo2 = new RoleInfo("roleInfo2", Trivial.class.getName());
      roleInfos = new RoleInfo[] { roleInfo1, roleInfo2 };
      rs.createRelationType("name1", roleInfos);
      rs.createRelationType("name2", roleInfos);
      rs.createRelationType("name3", roleInfos);

      ArrayList result = (ArrayList) rs.getAllRelationTypeNames();
      assertEquals(3, result.size());
      assertEquals(true, result.contains("name1"));
      assertEquals(true, result.contains("name2"));
      assertEquals(true, result.contains("name3"));
   }

   /**
    * Test get all roles
    */
   public void testGetAllRolesExternal() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeB(service);
         createRolesB(server);
         RoleResult result = null;
         RelationSupport support = new RelationSupport("id", service, server, 
                                         "relationTypeB", rolesB);
         addRelation(server, service, support, "test:type=support");
         result = support.getAllRoles();
         checkResult(result, roleInfosB, rolesB);
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test get all roles errors
    */
   public void testGetAllRolesErrors() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         RelationService rs = (RelationService) services.get(service);
         createRelationTypeC(service);
         createRolesC(server);
         RoleList roles = new RoleList();
         roles.add(roleC1);
         roles.add(roleC2);
         rs.createRelation("relationId", "relationTypeC", roles);
         String[] roleNames = new String[] {"roleC1"};

         boolean caught = false;
         try
         {
            rs.getAllRoles(null);
         }
         catch(IllegalArgumentException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("getAllRoles allows null relation id");
        
         caught = false;
         try
         {
            rs.getAllRoles("rubbish");
         }
         catch(RelationNotFoundException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("getAllRoles allows invalid relation id");

         server.unregisterMBean(service);
         caught = false;
         try
         {
            rs.getAllRoles("relationId");
         }
         catch(RelationServiceNotRegisteredException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("FAILS IN RI: getAllRoles allowed when not registered");
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test get Notification info
    */
   public void testGetNotificationInfo()
   {
      RelationService rs = new RelationService(true);
      MBeanNotificationInfo[] mbni = rs.getNotificationInfo();
      assertEquals(1, mbni.length);
      HashSet types = new HashSet();
      types.add(RelationNotification.RELATION_BASIC_CREATION);
      types.add(RelationNotification.RELATION_BASIC_REMOVAL);
      types.add(RelationNotification.RELATION_BASIC_UPDATE);
      types.add(RelationNotification.RELATION_MBEAN_CREATION);
      types.add(RelationNotification.RELATION_MBEAN_REMOVAL);
      types.add(RelationNotification.RELATION_MBEAN_UPDATE);
      String[] mbniTypes = mbni[0].getNotifTypes();
      assertEquals(types.size(), mbniTypes.length);
      for (int i = 0; i < mbniTypes.length; i++)
      {
         if (types.contains(mbniTypes[i]) == false)
            fail("Unexpected relation notification type: " + mbniTypes[i]);
      }
   }

   /**
    * Test get/set Purge Flag
    */
   public void testGetSetPurgeFlag() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         RelationService rs = null;
         ObjectName name = null;
         rs = new RelationService(true);
         name = new ObjectName("test:type = rs");
         server.registerMBean(rs, name);
         assertEquals(true, rs.getPurgeFlag());
         rs.setPurgeFlag(false);
         assertEquals(false, rs.getPurgeFlag());
         rs.setPurgeFlag(true);
         assertEquals(true, rs.getPurgeFlag());
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test get Referenced MBeans
    */
   public void testGetReferencedMBeansExternal() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeC(service);
         createRolesC(server);
         Map result = null;
         RelationSupport support = new RelationSupport("id", service, server, 
                                         "relationTypeC", rolesC);
         addRelation(server, service, support, "test:type=support");
         result = support.getReferencedMBeans();
         checkMBeans(result, rolesC);
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test get referenced mbeans errors
    */
   public void testGetReferencedMBeansErrors() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         RelationService rs = (RelationService) services.get(service);
         createRelationTypeC(service);
         createRolesC(server);
         RoleList roles = new RoleList();
         roles.add(roleC1);
         roles.add(roleC2);
         rs.createRelation("relationId", "relationTypeC", roles);
         String[] roleNames = new String[] {"roleC1"};

         boolean caught = false;
         try
         {
            rs.getReferencedMBeans(null);
         }
         catch(IllegalArgumentException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("getReferencedMBeans allows null relation id");
        
         caught = false;
         try
         {
            rs.getReferencedMBeans("rubbish");
         }
         catch(RelationNotFoundException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("getReferencedMBeans allows invalid relation id");
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test get Relation Type Names
    */
   public void testGetRelationTypeNameExternal() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         RelationService rs = (RelationService) services.get(service);
         createRelationTypeB(service);
         createRolesB(server);
         createRelationTypeC(service);
         createRolesC(server);
         RelationSupport relB = null;
         RelationSupport relC = null;
         String resultB = null;
         String resultC = null;
         relB = new RelationSupport("idB", service, server, 
                                       "relationTypeB", rolesB);
         addRelation(server, service, relB, "test:type=supportB");
         relC = new RelationSupport("idC", service, server, 
                                       "relationTypeC", rolesC);
         addRelation(server, service, relC, "test:type=supportC");
         resultB = rs.getRelationTypeName("idB");
         resultC = rs.getRelationTypeName("idC");
         assertEquals("relationTypeB", resultB);
         assertEquals("relationTypeC", resultC);
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test get relation type name errors
    */
   public void testGetRelationTypeNameErrors() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         RelationService rs = (RelationService) services.get(service);
         createRelationTypeC(service);
         createRolesC(server);
         RoleList roles = new RoleList();
         roles.add(roleC1);
         roles.add(roleC2);
         rs.createRelation("relationId", "relationTypeC", roles);
         String[] roleNames = new String[] {"roleC1"};

         boolean caught = false;
         try
         {
            rs.getRelationTypeName(null);
         }
         catch(IllegalArgumentException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("getRelationTypeName allows null relation id");
        
         caught = false;
         try
         {
            rs.getRelationTypeName("rubbish");
         }
         catch(RelationNotFoundException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("getRelationTypeName allows invalid relation id");
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test get Role
    */
   public void testGetRoleExternal() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeC(service);
         createRolesC(server);
         List result = null;
         RelationSupport support = new RelationSupport("id", service, server, 
                                         "relationTypeC", rolesC);
         addRelation(server, service, support, "test:type=support");
         result = support.getRole("roleC1");
         compareListOfObjectNames(getRole(rolesC, "roleC1").getRoleValue(), result);
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test get role errors
    */
   public void testGetRoleErrors() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         RelationService rs = (RelationService) services.get(service);
         createRelationTypeC(service);
         createRolesC(server);
         RoleList roles = new RoleList();
         roles.add(roleC1);
         roles.add(roleC2);
         rs.createRelation("relationId", "relationTypeC", roles);

         boolean caught = false;
         try
         {
            rs.getRole(null, "roleC1");
         }
         catch(IllegalArgumentException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("getRole allows null relation id");
        
         caught = false;
         try
         {
            rs.getRole("relationId", null);
         }
         catch(IllegalArgumentException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("getRole allows null role");
        
         caught = false;
         try
         {
            rs.getRole("rubbish", "roleC1");
         }
         catch(RelationNotFoundException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("getRole allows invalid relation id");
        
         caught = false;
         try
         {
            rs.getRole("relationId", "rubbish");
         }
         catch(RoleNotFoundException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("getRole allows invalid role name");
        
         caught = false;
         try
         {
            rs.getRole("relationId", "roleC2");
         }
         catch(RoleNotFoundException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("getRole allows unreadable role");

         server.unregisterMBean(service);
         caught = false;
         try
         {
            rs.getRole("relationId", "roleC1");
         }
         catch(RelationServiceNotRegisteredException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("FAILS IN RI: getRole allowed when not registered");
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test get Role Cardinality
    */
   public void testGetRoleCardinalityExternal() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeC(service);
         createRolesC(server);
         Integer result = null;
         RelationSupport support = null;
         support = new RelationSupport("id", service, server, 
                                          "relationTypeC", rolesC);
         addRelation(server, service, support, "test:type=support");
         result = support.getRoleCardinality("roleC1");
         assertEquals(2, result.intValue());

         result = support.getRoleCardinality("roleC2");
         assertEquals(3, result.intValue());
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test get role cardinality errors
    */
   public void testGetRoleCardinalityErrors() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         RelationService rs = (RelationService) services.get(service);
         createRelationTypeC(service);
         createRolesC(server);
         RoleList roles = new RoleList();
         roles.add(roleC1);
         roles.add(roleC2);
         rs.createRelation("relationId", "relationTypeC", roles);

         boolean caught = false;
         try
         {
            rs.getRoleCardinality(null, "roleC1");
         }
         catch(IllegalArgumentException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("getRoleCardinality allows null relation id");
        
         caught = false;
         try
         {
            rs.getRoleCardinality("relationId", null);
         }
         catch(IllegalArgumentException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("getRoleCardinality allows null role");
        
         caught = false;
         try
         {
            rs.getRoleCardinality("rubbish", "roleC1");
         }
         catch(RelationNotFoundException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("getRoleCardinality allows invalid relation id");
        
         caught = false;
         try
         {
            rs.getRoleCardinality("relationId", "rubbish");
         }
         catch(RoleNotFoundException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("getRoleCardinality allows invalid role name");
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test get Roles
    */
   public void testGetRolesExternal() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         RelationService rs = (RelationService) services.get(service);
         createRelationTypeC(service);
         createRolesC(server);
         RoleResult result = null;
         RelationSupport support = null;
         support = new RelationSupport("id", service, server, 
                                       "relationTypeC", rolesC);
         addRelation(server, service, support, "test:type=support");
         result = rs.getRoles("id", new String[] {"roleC1", "roleC2" });
         checkResult(result, roleInfosC, rolesC);

         result = rs.getRoles("id", new String[] {"roleC1" });
         RoleList resolved = result.getRoles();
         assertEquals(1, resolved.size());
         assertEquals(0, result.getRolesUnresolved().size());
         compare(getRole(rolesC, "roleC1"), (Role) resolved.get(0));

         result = rs.getRoles("id", new String[] {"roleC2" });
         RoleUnresolvedList unresolved = result.getRolesUnresolved();
         assertEquals(0, result.getRoles().size());
         assertEquals(1, unresolved.size());
         assertEquals("roleC2", ((RoleUnresolved)unresolved.get(0)).getRoleName());
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test get roles errors
    */
   public void testGetRolesErrors() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         RelationService rs = (RelationService) services.get(service);
         createRelationTypeC(service);
         createRolesC(server);
         RoleList roles = new RoleList();
         roles.add(roleC1);
         roles.add(roleC2);
         rs.createRelation("relationId", "relationTypeC", roles);
         String[] roleNames = new String[] {"roleC1"};

         boolean caught = false;
         try
         {
            rs.getRoles(null, roleNames);
         }
         catch(IllegalArgumentException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("getRoles allows null relation id");
        
         caught = false;
         try
         {
            rs.getRoles("relationId", null);
         }
         catch(IllegalArgumentException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("getRoles allows null role name array");
        
         caught = false;
         try
         {
            rs.getRoles("rubbish", roleNames);
         }
         catch(RelationNotFoundException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("getRoles allows invalid relation id");

         server.unregisterMBean(service);
         caught = false;
         try
         {
            rs.getRoles("relationId", roleNames);
         }
         catch(RelationServiceNotRegisteredException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("FAILS IN RI: getRoles allowed when not registered");
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test Has Relation
    */
   public void testHasRelationExternal() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeC(service);
         createRolesC(server);
         RelationSupport support = null;
         Boolean result1 = null;
         Boolean result2 = null;
         Boolean result3 = null;
         Boolean result4 = null;
         Boolean result5 = null;
         Boolean result6 = null;
         support = new RelationSupport("id1", service, server, 
                                       "relationTypeC", rolesC);
         addRelation(server, service, support, "test:type=support1");
         support = new RelationSupport("id2", service, server, 
                                       "relationTypeC", rolesC);
         addRelation(server, service, support, "test:type=support2");
         RelationService rs = (RelationService) services.get(service);
         result1 = rs.hasRelation("id1");
         result2 = rs.hasRelation("id2");
         result3 = rs.hasRelation("id3");
         rs.removeRelation("id2");
         result4 = rs.hasRelation("id1");
         result5 = rs.hasRelation("id2");
         result6 = rs.hasRelation("id3");
         assertEquals(true, result1.booleanValue());
         assertEquals(true, result2.booleanValue());
         assertEquals(false, result3.booleanValue());
         assertEquals(true, result4.booleanValue());
         assertEquals(false, result5.booleanValue());
         assertEquals(false, result6.booleanValue());
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test Has Relation Errors
    */
   public void testHasRelationErrors() throws Exception
   {
      RelationService rs = new RelationService(true);

      boolean caught = false;
      try
      {
         rs.hasRelation(null);
      }
      catch(IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("hasRelation allows null relation id");
   }

   /**
    * Test Is Active
    */
   public void testIsActive() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         RelationService rs = null;
         boolean caught = false;
         try
         {
            rs = new RelationService(true);
            rs.isActive();
         }
         catch (RelationServiceNotRegisteredException e)
         {
            caught = true;
         }
         catch (Exception e)
         {
            fail(e.toString());
         }
         assertEquals(true, caught);

         ObjectName name = null;
         name = new ObjectName("test:type = rs");
         server.registerMBean(rs, name);
         rs.isActive();

         caught = false;
         try
         {
            server.unregisterMBean(name);
            rs.isActive();
         }
         catch (RelationServiceNotRegisteredException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("FAILS IN RI: Relation Service still reports itself active.");
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test Is Relation
    */
   public void testIsRelationExternal() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeC(service);
         createRolesC(server);
         RelationSupport support = null;
         String result1 = null;
         String result2 = null;
         String result3 = null;
         String result4 = null;
         String result5 = null;
         String result6 = null;
         support = new RelationSupport("id1", service, server, 
                                       "relationTypeC", rolesC);
         addRelation(server, service, support, "test:type=support1");
         support = new RelationSupport("id2", service, server, 
                                       "relationTypeC", rolesC);
         addRelation(server, service, support, "test:type=support2");
         RelationService rs = (RelationService) services.get(service);
         result1 = rs.isRelation(new ObjectName("test:type=support1"));
         result2 = rs.isRelation(new ObjectName("test:type=support2"));
         result3 = rs.isRelation(new ObjectName("test:type=support3"));
         rs.removeRelation("id2");
         result4 = rs.isRelation(new ObjectName("test:type=support1"));
         result5 = rs.isRelation(new ObjectName("test:type=support2"));
         result6 = rs.isRelation(new ObjectName("test:type=support3"));
         assertEquals("id1", result1);
         assertEquals("id2", result2);
         assertEquals(null, result3);
         assertEquals("id1", result4);
         assertEquals(null, result5);
         assertEquals(null, result6);
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test Is Relation Errors
    */
   public void testIsRelationErrors() throws Exception
   {
      RelationService rs = new RelationService(true);

      boolean caught = false;
      try
      {
         rs.isRelation(null);
      }
      catch(IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("isRelation allows null relation id");
   }

   /**
    * Test Is Relation MBean
    */
   public void testIsRelationMBeanExternal() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeC(service);
         createRolesC(server);
         RelationSupport support = null;
         ObjectName result1 = null;
         ObjectName result2 = null;
         ObjectName on1 = null;
         ObjectName on2 = null;
         support = new RelationSupport("id1", service, server, 
                                       "relationTypeC", rolesC);
         addRelation(server, service, support, "test:type=support1");
         support = new RelationSupport("id2", service, server, 
                                       "relationTypeC", rolesC);
         addRelation(server, service, support, "test:type=support2");
         RelationService rs = (RelationService) services.get(service);
         result1 = rs.isRelationMBean("id1");
         result2 = rs.isRelationMBean("id2");
         on1 = new ObjectName("test:type=support1");
         on2 = new ObjectName("test:type=support2");
         assertEquals(on1, result1);
         assertEquals(on2, result2);
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test Is Relation MBean Errors
    */
   public void testIsRelationMBeanErrors() throws Exception
   {
      RelationService rs = new RelationService(true);

      boolean caught = false;
      try
      {
         rs.isRelationMBean(null);
      }
      catch(IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("isRelationMBean allows null relation id");

      caught = false;
      try
      {
         rs.isRelationMBean("rubbish");
      }
      catch(RelationNotFoundException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("isRelationMBean allows non-existent relation");
   }

   /**
    * Test purge Relations Automatically
    */
   public void testPurgeRelationsAutomaticExternal() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeC(service);
         createRolesC(server);
         RelationSupport support = null;
         ObjectName on = null;
         Listener listener = new Listener(RelationNotification.RELATION_MBEAN_REMOVAL);
         boolean result = false;
         support = new RelationSupport("id1", service, server, 
                                       "relationTypeC", rolesC);
         addRelation(server, service, support, "test:type=support1");
         server.addNotificationListener(service, listener, null, null);
         RelationService rs = (RelationService) services.get(service);
         server.unregisterMBean(new ObjectName("x:relation=c,role=2,bean=1"));
         on = new ObjectName("test:type=support1");
         result = rs.hasRelation("id1").booleanValue();
         assertEquals(false, result);
         RelationNotification rn = listener.check(1);
         assertEquals(new ArrayList(), rn.getMBeansToUnregister());
         assertEquals(on, rn.getObjectName());
         assertEquals("id1", rn.getRelationId());
         assertEquals("relationTypeC", rn.getRelationTypeName());
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test purge Relations Manually
    */
   public void testPurgeRelationsManuallyExternal() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeC(service);
         createRolesC(server);
         RelationSupport support = null;
         ObjectName on = null;
         Listener listener = new Listener(RelationNotification.RELATION_MBEAN_REMOVAL);
         boolean result = false;
         RelationService rs = null;
         support = new RelationSupport("id1", service, server, 
                                       "relationTypeC", rolesC);
         addRelation(server, service, support, "test:type=support1");
         server.addNotificationListener(service, listener, null, null);
         rs = (RelationService) services.get(service);
         rs.setPurgeFlag(false);
         server.unregisterMBean(new ObjectName("x:relation=c,role=2,bean=1"));
         on = new ObjectName("test:type=support1");
         result = rs.hasRelation("id1").booleanValue();
 
         assertEquals(true, result);
         RelationNotification rn = listener.check(0);

         rs.purgeRelations();
         result = rs.hasRelation("id1").booleanValue();
         assertEquals(false, result);
         rn = listener.check(1);
         assertEquals(new ArrayList(), rn.getMBeansToUnregister());
         assertEquals(on, rn.getObjectName());
         assertEquals("id1", rn.getRelationId());
         assertEquals("relationTypeC", rn.getRelationTypeName());
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test purge relations errors
    */
   public void testPurgeRelationsErrors() throws Exception
   {
      RelationService rs = new RelationService(true);

      boolean caught = false;
      try
      {
         rs.purgeRelations();
      }
      catch (RelationServiceNotRegisteredException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("purgeRelations allowed when not registered");
   }

   /**
    * Test remove relation
    */
   public void testRemoveRelationExternal() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeA(service);
         createRolesA(server);
         createRelationTypeB(service);
         createRolesB(server);
         createRelationTypeC(service);
         createRolesC(server);
         createRelationTypeCX(service);
         createRolesCX(server);
         List result = null;
         RelationSupport supportA1 = new RelationSupport("ida1", service, server, 
                                          "relationTypeA", rolesA);
         RelationSupport supportA2 = new RelationSupport("ida2", service, server, 
                                          "relationTypeA", rolesA);
         RelationSupport supportCX = new RelationSupport("idcx", service, server, 
                                          "relationTypeCX", rolesCX);
         RelationSupport supportC = new RelationSupport("idc", service, server, 
                                          "relationTypeC", rolesC);
         addRelation(server, service, supportA1, "test:type=supportA1");
         addRelation(server, service, supportA2, "test:type=supportA2");
         addRelation(server, service, supportCX, "test:type=supportCX");
         addRelation(server, service, supportC, "test:type=supportC");
         RelationService rs = (RelationService) services.get(service);
         rs.removeRelation("idcx");
         result = rs.getAllRelationIds();
         assertEquals(3, result.size());
         assertEquals(true, result.contains("ida1"));
         assertEquals(true, result.contains("ida2"));
         assertEquals(true, result.contains("idc"));
         assertEquals(false, result.contains("idcx"));
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test remove relation errors
    */
   public void testRemoveRelationErrors() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=serviceA", null);
         RelationService rs = null;
         rs = (RelationService) services.get(service);

         boolean caught = false;
         try
         {
            rs.removeRelation("RelationId");
         }
         catch (RelationServiceNotRegisteredException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("removeRelation allowed when not registered");

         service = createRelationService("test:type=service", server);
         createRelationTypeA(service);
         createRolesA(server);
         RelationSupport supportA1 = new RelationSupport("ida1", service, server, 
                                       "relationTypeA", rolesA);
         addRelation(server, service, supportA1, "test:type=supportA1");
         rs = (RelationService) services.get(service);

         caught = false;
         try
         {
            rs.removeRelation(null);
         }
         catch (IllegalArgumentException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("removeRelation accepts a null relation");

         caught = false;
         try
         {
            rs.removeRelation("rubbish");
         }
         catch (RelationNotFoundException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("removeRelation accepts a non existent relation");
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test remove relation type
    */
   public void testRemoveRelationType() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         RelationService rs = new RelationService(true);
         assertEquals(0, rs.getAllRelationTypeNames().size());

         RoleInfo roleInfo1 = null;
         RoleInfo roleInfo2 = null;
         RoleInfo[] roleInfos = null;
         ObjectName name = new ObjectName("test:type = rs");
         server.registerMBean(rs, name);
         roleInfo1 = new RoleInfo("roleInfo1", Trivial.class.getName());
         roleInfo2 = new RoleInfo("roleInfo2", Trivial.class.getName());
         roleInfos = new RoleInfo[] { roleInfo1, roleInfo2 };
         rs.createRelationType("name1", roleInfos);
         rs.createRelationType("name2", roleInfos);
         rs.createRelationType("name3", roleInfos);
         rs.removeRelationType("name3");

         ArrayList result = (ArrayList) rs.getAllRelationTypeNames();
         assertEquals(2, result.size());
         assertEquals(true, result.contains("name1"));
         assertEquals(true, result.contains("name2"));
         assertEquals(false, result.contains("name3"));
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test remove relation type errors
    */
   public void testRemoveRelationTypeErrors() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         RelationService rs = null;
         RoleInfo roleInfo1 = null;
         RoleInfo roleInfo2 = null;
         RoleInfo[] roleInfos = null;
         rs = new RelationService(true);
         roleInfo1 = new RoleInfo("roleInfo1", Trivial.class.getName());
         roleInfo2 = new RoleInfo("roleInfo2", Trivial.class.getName());
         roleInfos = new RoleInfo[] { roleInfo1, roleInfo2 };
         rs.createRelationType("name1", roleInfos);

         boolean caught = false;
         try
         {
            rs.removeRelationType("name1");
         }
         catch (RelationServiceNotRegisteredException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("Remove relation type allowed when not registered, why not?");

         server.registerMBean(rs, new ObjectName("test:type=RelationService"));

         caught = false;
         try
         {
            rs.removeRelationType(null);
         }
         catch (IllegalArgumentException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("Remove relation type allows null relation type name");

         caught = false;
         try
         {
            rs.removeRelationType("rubbish");
         }
         catch (RelationTypeNotFoundException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("Remove relation type allows non-existent relation type name");
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test send relation creation notification
    */
   public void testSendRelationCreationNotificationExternal() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeC(service);
         createRolesC(server);
         RelationSupport support = null;
         ObjectName on = null;
         Listener listener = new Listener(RelationNotification.RELATION_MBEAN_CREATION);
         support = new RelationSupport("id1", service, server, 
                                       "relationTypeC", rolesC);
         addRelation(server, service, support, "test:type=support1");
         server.addNotificationListener(service, listener, null, null);
         RelationService rs = (RelationService) services.get(service);
         rs.sendRelationCreationNotification("id1");
         on = new ObjectName("test:type=support1");
         RelationNotification rn = listener.check(1);
         assertEquals(new ArrayList(), rn.getMBeansToUnregister());
         assertEquals(new ArrayList(), rn.getNewRoleValue());
         assertEquals(on, rn.getObjectName());
         assertEquals(new ArrayList(), rn.getOldRoleValue());
         assertEquals("id1", rn.getRelationId());
         assertEquals("relationTypeC", rn.getRelationTypeName());
         assertEquals(null, rn.getRoleName());
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test send relation removal notification errors
    */
   public void testSendRelationCreationNotificationErrors() throws Exception
   {
      RelationService rs = new RelationService(true);

      boolean caught = false;
      try
      {
         rs.sendRelationCreationNotification(null);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("sendRelationCreationNotification allows null relation id");

      caught = false;
      try
      {
         rs.sendRelationCreationNotification("rubbish");
      }
      catch (RelationNotFoundException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("sendRelationCreationNotification allows invalid relation id");
   }

   /**
    * Test send relation removal notification
    */
   public void testSendRelationRemovalNotificationExternal() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeC(service);
         createRolesC(server);
         RelationSupport support = null;
         ObjectName on = null;
         ArrayList test = new ArrayList();
         Listener listener = new Listener(RelationNotification.RELATION_MBEAN_REMOVAL);
         support = new RelationSupport("id1", service, server, 
                                          "relationTypeC", rolesC);
         addRelation(server, service, support, "test:type=support1");
         server.addNotificationListener(service, listener, null, null);
         RelationService rs = (RelationService) services.get(service);
         test.add(new ObjectName("test:type=test"));
         rs.sendRelationRemovalNotification("id1", test);
         on = new ObjectName("test:type=support1");
         RelationNotification rn = listener.check(1);
         assertEquals(test, rn.getMBeansToUnregister());
         assertEquals(new ArrayList(), rn.getNewRoleValue());
         assertEquals(on, rn.getObjectName());
         assertEquals(new ArrayList(), rn.getOldRoleValue());
         assertEquals("id1", rn.getRelationId());
         assertEquals("relationTypeC", rn.getRelationTypeName());
         assertEquals(null, rn.getRoleName());
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test send relation removal notification errors
    */
   public void testSendRelationRemovalNotificationErrors() throws Exception
   {
      RelationService rs = new RelationService(true);

      boolean caught = false;
      try
      {
         rs.sendRelationRemovalNotification(null, new ArrayList());
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("sendRelationRemovalNotification allows null relation id");


      caught = false;
      try
      {
         rs.sendRelationRemovalNotification("rubbish", new ArrayList());
      }
      catch (RelationNotFoundException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("sendRelationRemovalNotification allows invalid relation id");
   }

   /**
    * Test send role update notification
    */
   public void testSendRoleUpdateNotificationExternal() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         createRelationTypeC(service);
         createRolesB(server);
         createRolesC(server);
         RelationSupport support = null;
         ObjectName on = null;
         ArrayList test = new ArrayList();
         Listener listener = new Listener(RelationNotification.RELATION_MBEAN_UPDATE);
         support = new RelationSupport("id1", service, server, 
                                          "relationTypeC", rolesC);
         addRelation(server, service, support, "test:type=support1");
         server.addNotificationListener(service, listener, null, null);
         RelationService rs = (RelationService) services.get(service);
         test.add(new ObjectName("test:type=test"));
         rs.sendRoleUpdateNotification("id1", roleB1, test);
         on = new ObjectName("test:type=support1");
         RelationNotification rn = listener.check(1);
         assertEquals(new ArrayList(), rn.getMBeansToUnregister());
         assertEquals(roleB1.getRoleValue(), rn.getNewRoleValue());
         assertEquals(on, rn.getObjectName());
         assertEquals(test, rn.getOldRoleValue());
         assertEquals("id1", rn.getRelationId());
         assertEquals("relationTypeC", rn.getRelationTypeName());
         assertEquals("roleB1", rn.getRoleName());
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test send role update notification errors
    */
   public void testSendRoleUpdateNotificationErrors() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         RelationService rs = (RelationService) services.get(service);
         createRelationTypeB(service);
         createRolesB(server);

         boolean caught = false;
         try
         {
            rs.sendRoleUpdateNotification(null, roleB1, new ArrayList());
         }
         catch (IllegalArgumentException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("sendRoleUpdateNotification allows null relation id");

         RoleList roleList = new RoleList();
         roleList.add(roleB1);
         roleList.add(roleB2);
         rs.createRelation("relationId", "relationTypeB", roleList);

         caught = false;
         try
         {
            rs.sendRoleUpdateNotification("relationId", null, new ArrayList());
         }
         catch (IllegalArgumentException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("sendRoleUpdateNotification allows null role");


         caught = false;
         try
         {
            rs.sendRoleUpdateNotification("rubbish", roleB1, new ArrayList());
         }
         catch (RelationNotFoundException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("sendRoleUpdateNotification allows invalid relation id");
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test set a role
    */
   public void testSetRoleExternal() throws Exception
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
         support = new RelationSupport("id1", service, server, 
                                          "relationTypeC", rolesC);
         addRelation(server, service, support, "test:type=support1");
         server.addNotificationListener(service, listener, null, null);
         RelationService rs = (RelationService) services.get(service);
         rs.setRole("id1", roleCX2);
         on = new ObjectName("test:type=support1");
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
    * Test set role errors
    */
   public void testSetRoleErrors() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         RelationService rs = (RelationService) services.get(service);
         createRelationTypeB(service);
         createRolesB(server);
         createRelationTypeC(service);
         createRolesC(server);
         createRolesCZ(server);
         createRolesCZZ(server);
         RoleList roles = new RoleList();
         roles.add(roleC1);
         roles.add(roleC2);
         rs.createRelation("relationId", "relationTypeC", roles);

         boolean caught = false;
         try
         {
            rs.setRole(null, roleC2);
         }
         catch(IllegalArgumentException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("setRole allows null relation id");
        
         caught = false;
         try
         {
            rs.setRole("relationId", null);
         }
         catch(IllegalArgumentException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("setRole allows null role");
        
         caught = false;
         try
         {
            rs.setRole("rubbish", roleC2);
         }
         catch(RelationNotFoundException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("setRole allows invalid relation id");
        
         caught = false;
         try
         {
            rs.setRole("relationId", roleB1);
         }
         catch(RoleNotFoundException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("setRole allows invalid role name");
        
         caught = false;
         try
         {
            rs.setRole("relationId", roleC1);
         }
         catch(RoleNotFoundException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("setRole allows non-writable role");

         caught = false;
         try
         {
            rs.setRole("relationId", roleCZ2);
         }
         catch(InvalidRoleValueException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("setRole allows a role below the minimum");
        
         caught = false;
         try
         {
            rs.setRole("relationId", roleCZZ);
         }
         catch(InvalidRoleValueException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("setRole allows a role above the maximum");
        
         caught = false;
         try
         {
            rs.setRole("relationId", roleCZZZ);
         }
         catch(InvalidRoleValueException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("setRole allows a role with unregistered beans");

         server.unregisterMBean(service);
         caught = false;
         try
         {
            rs.setRole("relationId", roleC2);
         }
         catch(RelationServiceNotRegisteredException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("FAILS IN RI: setRole allowed when not registered");
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
   public void testSetRolesExternal() throws Exception
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
         support = new RelationSupport("id1", service, server, 
                                          "relationTypeC", rolesC);
         addRelation(server, service, support, "test:type=support1");
         server.addNotificationListener(service, listener, null, null);
         RelationService rs = (RelationService) services.get(service);
            
         rs.setRoles("id1", shouldBe);
         on = new ObjectName("test:type=support1");
         compare(shouldBe, support.retrieveAllRoles());
         RelationNotification rn = listener.check(1);
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test set roles errors
    */
   public void testSetRolesErrors() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         RelationService rs = (RelationService) services.get(service);
         createRelationTypeC(service);
         createRolesC(server);
         RoleList roles = new RoleList();
         roles.add(roleC1);
         roles.add(roleC2);
         rs.createRelation("relationId", "relationTypeC", roles);
         RoleList newRoles = new RoleList();
         newRoles.add(roleC2);

         boolean caught = false;
         try
         {
            rs.setRoles(null, newRoles);
         }
         catch(IllegalArgumentException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("setRoles allows null relation id");
        
         caught = false;
         try
         {
            rs.setRole("relationId", null);
         }
         catch(IllegalArgumentException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("setRoles allows null role list");
        
         caught = false;
         try
         {
            rs.setRoles("rubbish", newRoles);
         }
         catch(RelationNotFoundException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("setRoles allows invalid relation id");

         server.unregisterMBean(service);
         caught = false;
         try
         {
            rs.setRoles("relationId", newRoles);
         }
         catch(RelationServiceNotRegisteredException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("FAILS IN RI: setRoles allowed when not registered");
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   /**
    * Test update role map errors
    */
   public void testUpdateRoleMapErrors() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      try
      {
         ObjectName service = createRelationService("test:type=service", server);
         RelationService rs = (RelationService) services.get(service);
         createRelationTypeB(service);
         createRolesB(server);
         RoleList roleList = new RoleList();
         roleList.add(roleB1);
         roleList.add(roleB2);
         rs.createRelation("relationId", "relationTypeB", roleList);

         boolean caught = false;
         try
         {
            rs.updateRoleMap(null, roleB1, new ArrayList());
         }
         catch (IllegalArgumentException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("updateRoleMap allows null relation id");

         caught = false;
         try
         {
            rs.updateRoleMap("relationId", null, new ArrayList());
         }
         catch (IllegalArgumentException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("updateRoleMap allows null role");

         caught = false;
         try
         {
            rs.updateRoleMap("rubbish", roleB1, new ArrayList());
         }
         catch (RelationNotFoundException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("updateRoleMap allows invalid relation id");

         server.unregisterMBean(service);

         caught = false;
         try
         {
            rs.updateRoleMap("relationId", roleB1, new ArrayList());
         }
         catch (RelationServiceNotRegisteredException e)
         {
            caught = true;
         }
         if (caught == false)
            fail("FAILS IN RI: updateRoleMap allowed when not registered");
      }
      finally
      {
         MBeanServerFactory.releaseMBeanServer(server);
      }
   }

   // Support -----------------------------------------------------------------

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
         roleB1 = new Role("roleB1", roleB1Values);

         ArrayList roleB2Values = new ArrayList();
         roleB2Values.add(createRoleValueBean("x:relation=b,role=2,bean=1", 
                                              Trivial.class, server));
         roleB2Values.add(createRoleValueBean("x:relation=b,role=2,bean=2", 
                                              Trivial.class, server));
         roleB2Values.add(createRoleValueBean("x:relation=b,role=2,bean=3", 
                                              Trivial.class, server));
         roleB2 = new Role("roleB2", roleB2Values);

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

   private void createRelationTypeCX(ObjectName relationService)
   {
      try
      {
         RoleInfo roleInfoCX1 = createRoleInfo("roleC1", Trivial.class, true, false, 1, 2);
         RoleInfo roleInfoCX2 = createRoleInfo("roleC2", Trivial.class, false, true, 3, 4);
         RoleInfo[] roleInfos = new RoleInfo[] { roleInfoCX1, roleInfoCX2 };
         createRelationType(relationService, "relationTypeCX", roleInfos );
         for (int i=0; i < roleInfos.length; i++)
            roleInfosCX.put(roleInfos[i].getName(), roleInfos[i]);
      }
      catch(Exception e)
      {
         fail(e.toString());
      }
   }

   private void createRolesCZ(MBeanServer server)
   {
      try
      {
         ArrayList roleCZ2Values = new ArrayList();
         roleCZ2Values.add(createRoleValueBean("x:relation=c,role=1,bean=1", 
                                              Trivial.class, null));
         roleCZ2 = new Role("roleC2", roleCZ2Values);

         rolesCZ = new RoleList();
         rolesCZ.add(roleCZ2);
      }
      catch(Exception e)
      {
         fail(e.toString());
      }
   }

   private void createRolesCZZ(MBeanServer server)
   {
      try
      {
         ArrayList roleCZZValues = new ArrayList();
         roleCZZValues.add(createRoleValueBean("x:relation=c,role=1,bean=1", 
                                              Trivial.class, null));
         roleCZZValues.add(createRoleValueBean("x:relation=c,role=1,bean=2", 
                                              Trivial.class, null));
         roleCZZValues.add(createRoleValueBean("x:relation=c,role=1,bean=3", 
                                              Trivial.class, null));
         roleCZZValues.add(createRoleValueBean("x:relation=c,role=1,bean=4", 
                                              Trivial.class, null));
         roleCZZValues.add(createRoleValueBean("x:relation=c,role=1,bean=5", 
                                              Trivial.class, server));
         roleCZZ = new Role("roleC2", roleCZZValues);

         rolesCZZ = new RoleList();
         rolesCZZ.add(roleCZZ);

         ArrayList roleCZZZValues = new ArrayList();
         roleCZZZValues.add(createRoleValueBean("x:relation=c,role=1x,bean=1", 
                                              Trivial.class, null));
         roleCZZZValues.add(createRoleValueBean("x:relation=c,role=1x,bean=2", 
                                              Trivial.class, null));
         roleCZZZValues.add(createRoleValueBean("x:relation=c,role=1x,bean=3", 
                                              Trivial.class, null));
         roleCZZZ = new Role("roleC2", roleCZZZValues);

         rolesCZZZ = new RoleList();
         rolesCZZZ.add(roleCZZZ);
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
