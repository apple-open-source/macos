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
import javax.management.relation.RoleStatus;
import javax.management.relation.RoleUnresolved;

import junit.framework.TestCase;

/**
 * Role Unresolved tests.<p>
 *
 * Test it to death.<p>
 *
 * NOTE: The tests use String literals to ensure the comparisons are
 *       not performed on object references.
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class RoleUnresolvedTestCase
  extends TestCase
{
  // Attributes ----------------------------------------------------------------

  // Constructor ---------------------------------------------------------------

  /**
   * Construct the test
   */
  public RoleUnresolvedTestCase(String s)
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
      // Create the role Unresolved
      ArrayList roleValue = new ArrayList();
      ObjectName a = new ObjectName(":a=a");
      ObjectName b = new ObjectName(":b=b");
      roleValue.add(a);
      roleValue.add(b);
      RoleUnresolved roleUnresolved = new RoleUnresolved("RoleName", roleValue,
                                             RoleStatus.NO_ROLE_WITH_NAME);

      // Check the role name
      assertEquals("RoleName", roleUnresolved.getRoleName());      

      // Check the role value
      assertEquals(roleValue, roleUnresolved.getRoleValue());      

      // Check the problem type
      assertEquals(RoleStatus.NO_ROLE_WITH_NAME, roleUnresolved.getProblemType());      

      // Change the role name
      roleUnresolved.setRoleName("Changed");
      assertEquals("Changed", roleUnresolved.getRoleName());      

      // Change the role value
      ArrayList roleValue2 = new ArrayList();
      ObjectName c = new ObjectName(":c=c");
      ObjectName d = new ObjectName(":d=d");
      roleValue2.add(c);
      roleValue2.add(d);
      roleUnresolved.setRoleValue(roleValue2);

      // Check the new role value
      assertEquals(roleValue2, roleUnresolved.getRoleValue());      

      // Check the problem type
      roleUnresolved.setProblemType(RoleStatus.ROLE_NOT_READABLE);
      assertEquals(RoleStatus.ROLE_NOT_READABLE, roleUnresolved.getProblemType());
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
      RoleUnresolved roleUnresolved = new RoleUnresolved("XYZZY", roleValue, 
                                             RoleStatus.NO_ROLE_WITH_NAME);

      // Check the human readable string
      String result = roleUnresolved.toString();
      if (result.lastIndexOf("XYZZY") == -1)
        fail("Missing role name in toString");
      if (result.lastIndexOf(":a=a") == -1)
        fail("Missing object name :a=a in toString");
      if (result.lastIndexOf(":b=b") == -1)
        fail("Missing object name :b=b in toString");

      // TODO How to test the problem type the string isn't specified?
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
        new RoleUnresolved(null, roleValue, RoleStatus.NO_ROLE_WITH_NAME);
      }
      catch (IllegalArgumentException e)
      {
        caught = true;
      }
      if (caught == false)
        fail ("Constructor accepts null role name");

      // Shouldn't allow an invalid problem type
      caught = false;
      try
      {
        new RoleUnresolved("RoleName", roleValue, -1000); 
      }
      catch (IllegalArgumentException e)
      {
        caught = true;
      }
      if (caught == false)
        fail ("Constructor accepts an invalid problem type");

      RoleUnresolved roleUnresolved = new RoleUnresolved("RoleName", roleValue,
                                           RoleStatus.NO_ROLE_WITH_NAME);

      // Shouldn't allow null for name
      caught = false;
      try
      {
        roleUnresolved.setRoleName(null);
      }
      catch (IllegalArgumentException e)
      {
        caught = true;
      }
      if (caught == false)
        fail ("setRoleName accepts null");

      // Shouldn't allow an invalid problem type
      caught = false;
      try
      {
        roleUnresolved.setProblemType(-1000);
      }
      catch (IllegalArgumentException e)
      {
        caught = true;
      }
      if (caught == false)
        fail ("setProblemType accepts an invalid problem type");
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
    RoleUnresolved roleUnresolved = new RoleUnresolved("RoleName", roleValue, 
                                          RoleStatus.NO_ROLE_WITH_NAME);
    RoleUnresolved roleUnresolved2 = (RoleUnresolved) roleUnresolved.clone();

    // Did it work?
    assertEquals(roleUnresolved.getRoleName(), roleUnresolved2.getRoleName());
    assertEquals(roleUnresolved.getRoleValue(), roleUnresolved2.getRoleValue());
    assertEquals(roleUnresolved.getProblemType(), roleUnresolved2.getProblemType());
    if(roleUnresolved.getRoleValue() == roleUnresolved2.getRoleValue())
      fail("RoleUnresolved.clone() didn't clone");
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
    RoleUnresolved roleUnresolved = new RoleUnresolved("RoleName", roleValue,
                                          RoleStatus.NO_ROLE_WITH_NAME);
    RoleUnresolved roleUnresolved2 = null;

    try
    {
      // Serialize it
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(roleUnresolved);
    
      // Deserialize it
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new ObjectInputStream(bais);
      roleUnresolved2 = (RoleUnresolved) ois.readObject();
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
    assertEquals(roleUnresolved.getRoleName(), roleUnresolved2.getRoleName());
    assertEquals(roleUnresolved.getRoleValue(), roleUnresolved2.getRoleValue());
  }
}
