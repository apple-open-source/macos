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
import java.util.Iterator;

import javax.management.ObjectName;
import javax.management.MalformedObjectNameException;
import javax.management.relation.Role;
import javax.management.relation.RoleList;

import junit.framework.TestCase;

/**
 * Role lists tests.<p>
 *
 * Test it to death.<p>
 *
 * NOTE: The tests use String literals to ensure the comparisons are
 *       not performed on object references.
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class RoleListTestCase
  extends TestCase
{
  // Attributes ----------------------------------------------------------------

  // Roles used in testing
  boolean setUpDone = false;
  Role role1;
  Role role2;

  // Constructor ---------------------------------------------------------------

  /**
   * Construct the test
   */
  public RoleListTestCase(String s)
  {
    super(s);
  }

  // Tests ---------------------------------------------------------------------

  /**
   * Empty Constructors.
   */
  public void testEmptyConstructors()
  {
    setUpRoles();

    // Empty lists
    RoleList empty = new RoleList();
    assertEquals(0, empty.size());
    empty = new RoleList(100);
    assertEquals(0, empty.size());
  }

  /**
   * Basic constructor test.
   */
  public void testBasicConstructor()
  {
    setUpRoles();

    ArrayList roles = new ArrayList();
    roles.add(role1);
    roles.add(role2);
    RoleList full = new RoleList(roles);
    assertEquals(2, full.size());
    assertEquals(role1, full.get(0));
    assertEquals(role2, full.get(1));
    Iterator iterator = full.iterator();
    assertEquals(role1, iterator.next());
    assertEquals(role2, iterator.next());
  }

  /**
   * Basic constructor test, ordering. Do it backwards
   */
  public void testBasicConstructorOrdering()
  {
    setUpRoles();

    ArrayList roles = new ArrayList();
    roles.add(role2);
    roles.add(role1);
    RoleList full = new RoleList(roles);
    assertEquals(2, full.size());
    assertEquals(role2, full.get(0));
    assertEquals(role1, full.get(1));
    Iterator iterator = full.iterator();
    assertEquals(role2, iterator.next());
    assertEquals(role1, iterator.next());
  }

  /**
   * Basic constructor test, allows duplicates
   */
  public void testBasicConstructorDuplicates()
  {
    setUpRoles();

    // Check duplicates allowed
    ArrayList roles = new ArrayList();
    roles.add(role1);
    roles.add(role1);
    RoleList full = new RoleList(roles);
    assertEquals(2, full.size());
    assertEquals(role1, full.get(0));
    assertEquals(role1, full.get(1));
    Iterator iterator = full.iterator();
    assertEquals(role1, iterator.next());
    assertEquals(role1, iterator.next());
  }

  /**
   * Test Error Handling.
   */
  public void testErrorHandling()
  {
    setUpRoles();

    // Shouldn't allow new roles
    ArrayList roles = new ArrayList();
    roles.add(role1);
    roles.add(null);

    // Shouldn't allow null for the name in constructor
    boolean caught = false;
    try
    {
      new RoleList(roles);
    }
    catch (IllegalArgumentException e)
    {
      caught = true;
    }
    if (caught == false)
      fail ("Constructor accepts null roles");

    // Should only allow roles
    roles = new ArrayList();
    roles.add(role1);
    roles.add(new Object());
    caught = false;
    try
    {
      new RoleList(roles);
    }
    catch (IllegalArgumentException e)
    {
      caught = true;
    }
    if (caught == false)
      fail ("Constructor accepts non roles");
  }

  /**
   * Single Append tests.
   */
  public void testSingleAppend()
  {
    setUpRoles();

    // Simple add
    RoleList list = new RoleList();
    list.add(role1);
    assertEquals(1, list.size());
    assertEquals(role1.toString(), list.get(0).toString());
    Iterator iterator = list.iterator();
    assertEquals(role1.toString(), iterator.next().toString());

    // Once more for luck, should append
    list.add(role2);
    assertEquals(2, list.size());
    assertEquals(role1.toString(), list.get(0).toString());
    assertEquals(role2.toString(), list.get(1).toString());
    iterator = list.iterator();
    assertEquals(role1.toString(), iterator.next().toString());
    assertEquals(role2.toString(), iterator.next().toString());

    // Add a null, shouldn't work
    boolean caught = false;
    try
    {
      list.add(null);
    }
    catch (IllegalArgumentException e)
    {
      caught = true;
    }
    if (caught == false)
      fail ("addRole(null) shouldn't work");
  }

  /**
   * Add single 
   */
  public void testSingleAdd()
  {
    setUpRoles();

    // Set up a role list
    RoleList list = new RoleList();
    list.add(role1);
    list.add(role2);

    // Add one
    list.add(1, role1);
    assertEquals(3, list.size());
    assertEquals(role1.toString(), list.get(0).toString());
    assertEquals(role1.toString(), list.get(1).toString());
    assertEquals(role2.toString(), list.get(2).toString());
    Iterator iterator = list.iterator();
    assertEquals(role1.toString(), iterator.next().toString());
    assertEquals(role1.toString(), iterator.next().toString());
    assertEquals(role2.toString(), iterator.next().toString());

    // Add a role in the wrong place
    boolean caught = false;
    try
    {
      list.add(4, role1);
    }
    catch (IndexOutOfBoundsException e)
    {
      caught = true;
    }
    if (caught == false)
      fail ("Shouldn't be able to add a role outside of valid range");

    // Add a null should not work
    caught = false;
    try
    {
      list.add(1, null);
    }
    catch (IllegalArgumentException e)
    {
      caught = true;
    }
    if (caught == false)
      fail ("Shouldn't be able to add a null at an index");
  }

  /**
   * Set single 
   */
  public void testSingleSet()
  {
    setUpRoles();

    // Set up a role list
    RoleList list = new RoleList();
    list.add(role1);
    list.add(role2);

    // Add one
    list.set(1, role1);
    assertEquals(2, list.size());
    assertEquals(role1.toString(), list.get(0).toString());
    assertEquals(role1.toString(), list.get(1).toString());
    Iterator iterator = list.iterator();
    assertEquals(role1.toString(), iterator.next().toString());
    assertEquals(role1.toString(), iterator.next().toString());

    // Add a role in the wrong place
    boolean caught = false;
    try
    {
      list.set(4, role1);
    }
    catch (IndexOutOfBoundsException e)
    {
      caught = true;
    }
    if (caught == false)
      fail ("Shouldn't be able to set a role outside of valid range");

    // set a null should not work
    caught = false;
    try
    {
      list.add(1, null);
    }
    catch (IllegalArgumentException e)
    {
      caught = true;
    }
    if (caught == false)
      fail ("Shouldn't be able to set a null at an index");
  }

  /**
   * Add multiple
   */
  public void testMultipleAdd()
  {
    setUpRoles();

    // Set up a role list
    RoleList list = new RoleList();
    list.add(role1);
    list.add(role1);
    RoleList listToAdd = new RoleList();
    listToAdd.add(role2);
    listToAdd.add(role2);

    // Add all
    list.addAll(listToAdd);
    assertEquals(4, list.size());
    assertEquals(role1.toString(), list.get(0).toString());
    assertEquals(role1.toString(), list.get(1).toString());
    assertEquals(role2.toString(), list.get(2).toString());
    assertEquals(role2.toString(), list.get(3).toString());
    Iterator iterator = list.iterator();
    assertEquals(role1.toString(), iterator.next().toString());
    assertEquals(role1.toString(), iterator.next().toString());
    assertEquals(role2.toString(), iterator.next().toString());
    assertEquals(role2.toString(), iterator.next().toString());

    // Add a null should work (not very standard)
    boolean caught = false;
    try
    {
      list.addAll(null);
    }
    catch (Exception e)
    {
      caught = true;
    }
    if (caught == true)
      fail ("Should be able to addAll a null");
  }

  /**
   * Add multiple at a location
   */
  public void testMultipleLocationAdd()
  {
    setUpRoles();

    // Set up a role list
    RoleList list = new RoleList();
    list.add(role1);
    list.add(role1);
    RoleList listToAdd = new RoleList();
    listToAdd.add(role2);
    listToAdd.add(role2);

    // Add all
    list.addAll(1, listToAdd);
    assertEquals(4, list.size());
    assertEquals(role1.toString(), list.get(0).toString());
    assertEquals(role2.toString(), list.get(1).toString());
    assertEquals(role2.toString(), list.get(2).toString());
    assertEquals(role1.toString(), list.get(3).toString());
    Iterator iterator = list.iterator();
    assertEquals(role1.toString(), iterator.next().toString());
    assertEquals(role2.toString(), iterator.next().toString());
    assertEquals(role2.toString(), iterator.next().toString());
    assertEquals(role1.toString(), iterator.next().toString());

    // Add a role in the wrong place
    boolean caught = false;
    try
    {
      list.addAll(6, listToAdd);
    }
    catch (IndexOutOfBoundsException e)
    {
      caught = true;
    }
    if (caught == false)
      fail ("Shouldn't be able to addAll a role outside of valid range");

    // Add a null should not work
    caught = false;
    try
    {
      list.addAll(1, null);
    }
    catch (IllegalArgumentException e)
    {
      caught = true;
    }
    if (caught == false)
      fail ("Shouldn't be able to addAll a null at an index");
  }

  /**
   * Test clone.
   */
  public void testClone()
  {
    setUpRoles();

    ArrayList roles = new ArrayList();
    roles.add(role1);
    roles.add(role2);
    RoleList full = new RoleList(roles);
    RoleList clone = (RoleList) full.clone();
    assertEquals(2, clone.size());
    assertEquals(role1.toString(), clone.get(0).toString());
    assertEquals(role2.toString(), clone.get(1).toString());
    Iterator iterator = clone.iterator();
    assertEquals(role1.toString(), iterator.next().toString());
    assertEquals(role2.toString(), iterator.next().toString());
  }

  /**
   * Test serialization.
   */
  public void testSerialization()
  {
    setUpRoles();

    ArrayList roles = new ArrayList();
    roles.add(role1);
    roles.add(role2);
    RoleList full = new RoleList(roles);
    RoleList copy = null;

    try
    {
      // Serialize it
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(full);
    
      // Deserialize it
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new ObjectInputStream(bais);
      copy = (RoleList) ois.readObject();
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
    assertEquals(2, copy.size());
    assertEquals(role1.toString(), copy.get(0).toString());
    assertEquals(role2.toString(), copy.get(1).toString());
    Iterator iterator = copy.iterator();
    assertEquals(role1.toString(), iterator.next().toString());
    assertEquals(role2.toString(), iterator.next().toString());
  }

  // Tests ---------------------------------------------------------------------

  private void setUpRoles()
  {
    if (setUpDone == true)
      return;    
    try
    {
      // Create the roles
      ArrayList roleValue1 = new ArrayList();
      ObjectName a = new ObjectName(":a=a");
      ObjectName b = new ObjectName(":b=b");
      roleValue1.add(a);
      roleValue1.add(b);
      role1 = new Role("RoleName1", roleValue1);

      ArrayList roleValue2 = new ArrayList();
      ObjectName c = new ObjectName(":c=c");
      ObjectName d = new ObjectName(":d=d");
      roleValue2.add(c);
      roleValue2.add(d);
      role2 = new Role("RoleName2", roleValue2);
    }
    catch (MalformedObjectNameException mfone)
    {
      fail(mfone.toString());
    }
    setUpDone = true;
  }
}
