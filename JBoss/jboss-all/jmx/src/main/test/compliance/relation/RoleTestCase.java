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

import javax.management.ObjectName;
import javax.management.MalformedObjectNameException;
import javax.management.relation.Role;

import junit.framework.TestCase;

/**
 * Role tests.<p>
 *
 * Test it to death.<p>
 *
 * NOTE: The tests use String literals to ensure the comparisons are
 *       not performed on object references.
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class RoleTestCase
  extends TestCase
{
  // Attributes ----------------------------------------------------------------

  // Constructor ---------------------------------------------------------------

  /**
   * Construct the test
   */
  public RoleTestCase(String s)
  {
    super(s);
  }

  // Tests ---------------------------------------------------------------------

  /**
   * Basic tests.
   */
  public void testBasic()
  {
    try
    {
      // Create the role
      ArrayList roleValue = new ArrayList();
      ObjectName a = new ObjectName(":a=a");
      ObjectName b = new ObjectName(":b=b");
      roleValue.add(a);
      roleValue.add(b);
      Role role = new Role("RoleName", roleValue);

      // Check the role name
      assertEquals("RoleName", role.getRoleName());      

      // Check the role value
      assertEquals(roleValue, role.getRoleValue());      

      // Change the role name
      role.setRoleName("Changed");
      assertEquals("Changed", role.getRoleName());      

      // Change the role value
      ArrayList roleValue2 = new ArrayList();
      ObjectName c = new ObjectName(":c=c");
      ObjectName d = new ObjectName(":d=d");
      roleValue2.add(c);
      roleValue2.add(d);
      role.setRoleValue(roleValue2);

      // Check the new role value
      assertEquals(roleValue2, role.getRoleValue());      
    }
    catch (MalformedObjectNameException mfone)
    {
      fail(mfone.toString());
    }
  }

  /**
   * toString tests.
   */
  public void testToString()
  {
    try
    {
      // Create the role
      ArrayList roleValue = new ArrayList();
      ObjectName a = new ObjectName(":a=a");
      ObjectName b = new ObjectName(":b=b");
      roleValue.add(a);
      roleValue.add(b);
      Role role = new Role("XYZZY", roleValue);

      // Check the value formatter
      String result = Role.roleValueToString(roleValue);
      if (result.lastIndexOf(":a=a") == -1)
        fail("Missing object name :a=a in roleValueToString");
      if (result.lastIndexOf(":b=b") == -1)
        fail("Missing object name :b=b in roleValueToString");

      // Check the human readable string
      result = role.toString();
      if (result.lastIndexOf("XYZZY") == -1)
        fail("Missing role name in toString");
      if (result.lastIndexOf(":a=a") == -1)
        fail("Missing object name :a=a in toString");
      if (result.lastIndexOf(":b=b") == -1)
        fail("Missing object name :b=b in toString");
    }
    catch (MalformedObjectNameException mfone)
    {
      fail(mfone.toString());
    }
  }

  /**
   * Test Error Handling.
   */
  public void testErrorHandling()
  {
    try
    {
      // Create the role
      ArrayList roleValue = new ArrayList();
      ObjectName a = new ObjectName(":a=a");
      ObjectName b = new ObjectName(":b=b");
      roleValue.add(a);
      roleValue.add(b);

      // Shouldn't allow null for the name in constructor
      boolean caught = false;
      try
      {
        new Role(null, roleValue);
      }
      catch (IllegalArgumentException e)
      {
        caught = true;
      }
      if (caught == false)
        fail ("Constructor accepts null role name");

      // Shouldn't allow null for the value in constructor
      caught = false;
      try
      {
        new Role("RoleName", null); 
      }
      catch (IllegalArgumentException e)
      {
        caught = true;
      }
      if (caught == false)
        fail ("Constructor accepts null role value");

      Role role = new Role("RoleName", roleValue);

      // Shouldn't allow null for name
      caught = false;
      try
      {
        role.setRoleName(null);
      }
      catch (IllegalArgumentException e)
      {
        caught = true;
      }
      if (caught == false)
        fail ("setRoleName accepts null");

      // Shouldn't allow null for value
      caught = false;
      try
      {
        role.setRoleValue(null);
      }
      catch (IllegalArgumentException e)
      {
        caught = true;
      }
      if (caught == false)
        fail ("setRoleValue accepts null");

      // Shouldn't allow null for value
      caught = false;
      try
      {
        Role.roleValueToString(null);
      }
      catch (IllegalArgumentException e)
      {
        caught = true;
      }
      if (caught == false)
        fail ("roleValueToString accepts null");
    }
    catch (MalformedObjectNameException mfone)
    {
      fail(mfone.toString());
    }
  }

  /**
   * Test clone.
   */
  public void testClone()
  {
    // Create the role
    ArrayList roleValue = new ArrayList();
    try
    {
      roleValue.add(new ObjectName(":a=a"));
      roleValue.add(new ObjectName(":b=b"));
    }
    catch (MalformedObjectNameException mfone)
    {
      fail(mfone.toString());
    }
    Role role = new Role("RoleName", roleValue);
    Role role2 = (Role) role.clone();

    // Did it work?
    assertEquals(role.getRoleName(), role2.getRoleName());
    assertEquals(role.getRoleValue(), role2.getRoleValue());
    if(role.getRoleValue() == role2.getRoleValue())
      fail("Role.clone() didn't clone");
  }

  /**
   * Test serialization.
   */
  public void testSerialization()
  {
    // Create the role
    ArrayList roleValue = new ArrayList();
    try
    {
      roleValue.add(new ObjectName(":a=a"));
      roleValue.add(new ObjectName(":b=b"));
    }
    catch (MalformedObjectNameException mfone)
    {
      fail(mfone.toString());
    }
    Role role = new Role("RoleName", roleValue);
    Role role2 = null;

    try
    {
      // Serialize it
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(role);
    
      // Deserialize it
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new ObjectInputStream(bais);
      role2 = (Role) ois.readObject();
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
    assertEquals(role.getRoleName(), role2.getRoleName());
    assertEquals(role.getRoleValue(), role2.getRoleValue());
  }
}
