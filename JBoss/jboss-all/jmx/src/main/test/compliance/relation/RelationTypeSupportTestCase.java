/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.relation;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;

import java.util.ArrayList;

import javax.management.relation.InvalidRelationTypeException;
import javax.management.relation.RelationSupport;
import javax.management.relation.RelationTypeSupport;
import javax.management.relation.RoleInfo;
import javax.management.relation.RoleInfoNotFoundException;

import junit.framework.TestCase;

/**
 * Relation Type Support tests
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class RelationTypeSupportTestCase
  extends TestCase
{

  // Attributes ----------------------------------------------------------------

  // Constructor ---------------------------------------------------------------

  /**
   * Construct the test
   */
  public RelationTypeSupportTestCase(String s)
  {
    super(s);
  }

  // Tests ---------------------------------------------------------------------

  /**
   * Basic tests
   */
  public void testBasic()
  {
    RoleInfo roleInfo1 = null;
    RoleInfo roleInfo2 = null;
    RoleInfo[] roleInfos = null;
    RelationTypeSupport support = null;
    try
    {
      roleInfo1 = new RoleInfo("roleInfo1", RelationSupport.class.getName());
      roleInfo2 = new RoleInfo("roleInfo2", RelationSupport.class.getName());
      roleInfos = new RoleInfo[] { roleInfo1, roleInfo2 };
      support = new RelationTypeSupport("name", roleInfos);
    }
    catch (Exception e)
    {
      fail(e.toString());
    }

    // Check the name
    assertEquals("name", support.getRelationTypeName());

    // Check the roleInfos
    ArrayList result = (ArrayList) support.getRoleInfos();
    assertEquals(2, result.size());

    // Check get
    try
    {
      assertEquals(roleInfo1.toString(), support.getRoleInfo("roleInfo1").toString());
      assertEquals(roleInfo2.toString(), support.getRoleInfo("roleInfo2").toString());
    }
    catch (Exception e)
    {
      fail(e.toString());
    }

    // Check the protected constructor
    MyRelationTypeSupport mySupport = new MyRelationTypeSupport("myName");

    // Did it work?
    assertEquals("myName", mySupport.getRelationTypeName());
    result = (ArrayList) mySupport.getRoleInfos();
    assertEquals(0, result.size());

    // Add a role info
    try
    {
      mySupport.addRoleInfo(roleInfo1);
      result = (ArrayList) mySupport.getRoleInfos();
      assertEquals(1, result.size());
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
  }

  /**
   * Error handling
   */
  public void testErrorHandling()
  {
    RoleInfo roleInfo1 = null;
    RoleInfo roleInfo2 = null;
    RoleInfo[] roleInfos = null;
    RelationTypeSupport support = null;

    boolean caught = false;
    try
    {
      roleInfo1 = new RoleInfo("roleInfo1", RelationSupport.class.getName());
      roleInfo2 = new RoleInfo("roleInfo2", RelationSupport.class.getName());
      roleInfos = new RoleInfo[] { roleInfo1, roleInfo2 };
      support = new RelationTypeSupport(null, roleInfos);
    }
    catch (IllegalArgumentException e)
    {
      caught = true;
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    if (caught == false)
      fail("Constructor accepts null relation type name");

    caught = false;
    try
    {
      support = new RelationTypeSupport("name", null);
    }
    catch (IllegalArgumentException e)
    {
      caught = true;
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    if (caught == false)
      fail("Constructor accepts null role infos");

    caught = false;
    try
    {
      support = new RelationTypeSupport("name", new RoleInfo[0]);
    }
    catch (InvalidRelationTypeException e)
    {
      caught = true;
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    if (caught == false)
      fail("Constructor accepts no role infos");

    caught = false;
    try
    {
      roleInfo1 = new RoleInfo("roleInfo1", RelationSupport.class.getName());
      roleInfos = new RoleInfo[] { roleInfo1, null };
      support = new RelationTypeSupport("name", roleInfos);
    }
    catch (InvalidRelationTypeException e)
    {
      caught = true;
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    if (caught == false)
      fail("Constructor accepts null role");

    caught = false;
    try
    {
      roleInfo1 = new RoleInfo("roleInfo1", RelationSupport.class.getName());
      roleInfos = new RoleInfo[] { roleInfo1 };
      support = new RelationTypeSupport("name", roleInfos);
      support.getRoleInfo(null);
    }
    catch (IllegalArgumentException e)
    {
      caught = true;
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    if (caught == false)
      fail("getRoleInfo allows a null role info name");

    caught = false;
    try
    {
      roleInfo1 = new RoleInfo("roleInfo1", RelationSupport.class.getName());
      roleInfos = new RoleInfo[] { roleInfo1 };
      support = new RelationTypeSupport("name", roleInfos);
      support.getRoleInfo("rubbish");
    }
    catch (RoleInfoNotFoundException e)
    {
      caught = true;
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    if (caught == false)
      fail("getRoleInfo returns a not existent role info");

    // Check the protected addRoleInfo
    MyRelationTypeSupport mySupport = new MyRelationTypeSupport("myName");

    caught = false;
    try
    {
      mySupport.addRoleInfo(null);
    }
    catch (IllegalArgumentException e)
    {
      caught = true;
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    if (caught == false)
      fail("addRoleInfo accepts null");

    caught = false;
    try
    {
      mySupport.addRoleInfo(roleInfo1);
      mySupport.addRoleInfo(roleInfo1);
    }
    catch (InvalidRelationTypeException e)
    {
      caught = true;
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    if (caught == false)
      fail("addRoleInfo accepts duplicates role infos");
  }

  /**
   * Test serialization.
   */
  public void testSerialization()
  {
    // Create the relationt type support
    RoleInfo roleInfo1 = null;
    RoleInfo roleInfo2 = null;
    RoleInfo[] roleInfos = null;
    RelationTypeSupport support = null;
    try
    {
      roleInfo1 = new RoleInfo("roleInfo1", RelationSupport.class.getName());
      roleInfo2 = new RoleInfo("roleInfo2", RelationSupport.class.getName());
      roleInfos = new RoleInfo[] { roleInfo1, roleInfo2 };
      support = new RelationTypeSupport("name", roleInfos);
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    RelationTypeSupport support2 = null;

    try
    {
      // Serialize it
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(support);
    
      // Deserialize it
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new ObjectInputStream(bais);
      support2 = (RelationTypeSupport) ois.readObject();
    }
    catch (IOException ioe)
    {
      fail(ioe.toString());
    }
    catch (ClassNotFoundException cnfe)
    {
      fail(cnfe.toString());
    }

    // Did it work?
    assertEquals("name", support2.getRelationTypeName());
    ArrayList result = (ArrayList) support2.getRoleInfos();
    assertEquals(2, result.size());
  }

  // Support -------------------------------------------------------------------

  class MyRelationTypeSupport
    extends RelationTypeSupport
  {
    public MyRelationTypeSupport(String relationTypeName)
    {
      super(relationTypeName);
    }
    public void addRoleInfo(RoleInfo roleInfo)
      throws IllegalArgumentException, InvalidRelationTypeException
    {
      super.addRoleInfo(roleInfo);
    }
  }
}
