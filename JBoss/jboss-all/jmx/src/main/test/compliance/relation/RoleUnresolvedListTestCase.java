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
import javax.management.relation.RoleStatus;
import javax.management.relation.RoleUnresolved;
import javax.management.relation.RoleUnresolvedList;

import junit.framework.TestCase;

/**
 * Role Unresolved lists tests.<p>
 *
 * Test it to death.<p>
 *
 * NOTE: The tests use String literals to ensure the comparisons are
 *       not performed on object references.
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class RoleUnresolvedListTestCase
  extends TestCase
{
  // Attributes ----------------------------------------------------------------

  // Role Unresolveds used in testing
  boolean setUpDone = false;
  RoleUnresolved roleUnresolved1;
  RoleUnresolved roleUnresolved2;

  // Constructor ---------------------------------------------------------------

  /**
   * Construct the test
   */
  public RoleUnresolvedListTestCase(String s)
  {
    super(s);
  }

  // Tests ---------------------------------------------------------------------

  /**
   * Empty Constructors.
   */
  public void testEmptyConstructors()
  {
    setUpRoleUnresolveds();

    // Empty lists
    RoleUnresolvedList empty = new RoleUnresolvedList();
    assertEquals(0, empty.size());
    empty = new RoleUnresolvedList(100);
    assertEquals(0, empty.size());
  }

  /**
   * Basic constructor test.
   */
  public void testBasicConstructor()
  {
    setUpRoleUnresolveds();

    ArrayList roleUnresolveds = new ArrayList();
    roleUnresolveds.add(roleUnresolved1);
    roleUnresolveds.add(roleUnresolved2);
    RoleUnresolvedList full = new RoleUnresolvedList(roleUnresolveds);
    assertEquals(2, full.size());
    assertEquals(roleUnresolved1, full.get(0));
    assertEquals(roleUnresolved2, full.get(1));
    Iterator iterator = full.iterator();
    assertEquals(roleUnresolved1, iterator.next());
    assertEquals(roleUnresolved2, iterator.next());
  }

  /**
   * Basic constructor test, ordering. Do it backwards
   */
  public void testBasicConstructorOrdering()
  {
    setUpRoleUnresolveds();

    ArrayList roleUnresolveds = new ArrayList();
    roleUnresolveds.add(roleUnresolved2);
    roleUnresolveds.add(roleUnresolved1);
    RoleUnresolvedList full = new RoleUnresolvedList(roleUnresolveds);
    assertEquals(2, full.size());
    assertEquals(roleUnresolved2, full.get(0));
    assertEquals(roleUnresolved1, full.get(1));
    Iterator iterator = full.iterator();
    assertEquals(roleUnresolved2, iterator.next());
    assertEquals(roleUnresolved1, iterator.next());
  }

  /**
   * Basic constructor test, allows duplicates
   */
  public void testBasicConstructorDuplicates()
  {
    setUpRoleUnresolveds();

    // Check duplicates allowed
    ArrayList roleUnresolveds = new ArrayList();
    roleUnresolveds.add(roleUnresolved1);
    roleUnresolveds.add(roleUnresolved1);
    RoleUnresolvedList full = new RoleUnresolvedList(roleUnresolveds);
    assertEquals(2, full.size());
    assertEquals(roleUnresolved1, full.get(0));
    assertEquals(roleUnresolved1, full.get(1));
    Iterator iterator = full.iterator();
    assertEquals(roleUnresolved1, iterator.next());
    assertEquals(roleUnresolved1, iterator.next());
  }

  /**
   * Test Error Handling.
   */
  public void testErrorHandling()
  {
    setUpRoleUnresolveds();

    // Shouldn't allow new roleUnresolveds
    ArrayList roleUnresolveds = new ArrayList();
    roleUnresolveds.add(roleUnresolved1);
    roleUnresolveds.add(null);

    // Shouldn't allow null for the name in constructor
    boolean caught = false;
    try
    {
      new RoleUnresolvedList(roleUnresolveds);
    }
    catch (IllegalArgumentException e)
    {
      caught = true;
    }
    if (caught == false)
      fail ("Constructor accepts null roleUnresolveds");

    // Should only allow roleUnresolveds
    roleUnresolveds = new ArrayList();
    roleUnresolveds.add(roleUnresolved1);
    roleUnresolveds.add(new Object());
    caught = false;
    try
    {
      new RoleUnresolvedList(roleUnresolveds);
    }
    catch (IllegalArgumentException e)
    {
      caught = true;
    }
    if (caught == false)
      fail ("Constructor accepts non roleUnresolveds");
  }

  /**
   * Single Append tests.
   */
  public void testSingleAppend()
  {
    setUpRoleUnresolveds();

    // Simple add
    RoleUnresolvedList list = new RoleUnresolvedList();
    list.add(roleUnresolved1);
    assertEquals(1, list.size());
    assertEquals(roleUnresolved1.toString(), list.get(0).toString());
    Iterator iterator = list.iterator();
    assertEquals(roleUnresolved1.toString(), iterator.next().toString());

    // Once more for luck, should append
    list.add(roleUnresolved2);
    assertEquals(2, list.size());
    assertEquals(roleUnresolved1.toString(), list.get(0).toString());
    assertEquals(roleUnresolved2.toString(), list.get(1).toString());
    iterator = list.iterator();
    assertEquals(roleUnresolved1.toString(), iterator.next().toString());
    assertEquals(roleUnresolved2.toString(), iterator.next().toString());

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
      fail ("RoleUnresolved add(null) shouldn't work");
  }

  /**
   * Add single 
   */
  public void testSingleAdd()
  {
    setUpRoleUnresolveds();

    RoleUnresolvedList list = new RoleUnresolvedList();
    list.add(roleUnresolved1);
    list.add(roleUnresolved2);

    // Add one
    list.add(1, roleUnresolved1);
    assertEquals(3, list.size());
    assertEquals(roleUnresolved1.toString(), list.get(0).toString());
    assertEquals(roleUnresolved1.toString(), list.get(1).toString());
    assertEquals(roleUnresolved2.toString(), list.get(2).toString());
    Iterator iterator = list.iterator();
    assertEquals(roleUnresolved1.toString(), iterator.next().toString());
    assertEquals(roleUnresolved1.toString(), iterator.next().toString());
    assertEquals(roleUnresolved2.toString(), iterator.next().toString());

    // Add a roleUnresolved in the wrong place
    boolean caught = false;
    try
    {
      list.add(4, roleUnresolved1);
    }
    catch (IndexOutOfBoundsException e)
    {
      caught = true;
    }
    if (caught == false)
      fail ("Shouldn't be able to add a roleUnresolved outside of valid range");

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
    setUpRoleUnresolveds();

    RoleUnresolvedList list = new RoleUnresolvedList();
    list.add(roleUnresolved1);
    list.add(roleUnresolved2);

    // Add one
    list.set(1, roleUnresolved1);
    assertEquals(2, list.size());
    assertEquals(roleUnresolved1.toString(), list.get(0).toString());
    assertEquals(roleUnresolved1.toString(), list.get(1).toString());
    Iterator iterator = list.iterator();
    assertEquals(roleUnresolved1.toString(), iterator.next().toString());
    assertEquals(roleUnresolved1.toString(), iterator.next().toString());

    // Add a role Unresolved in the wrong place
    boolean caught = false;
    try
    {
      list.set(4, roleUnresolved1);
    }
    catch (IndexOutOfBoundsException e)
    {
      caught = true;
    }
    if (caught == false)
      fail ("Shouldn't be able to set a roleUnresolved outside of valid range");

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
    setUpRoleUnresolveds();

    RoleUnresolvedList list = new RoleUnresolvedList();
    list.add(roleUnresolved1);
    list.add(roleUnresolved1);
    RoleUnresolvedList listToAdd = new RoleUnresolvedList();
    listToAdd.add(roleUnresolved2);
    listToAdd.add(roleUnresolved2);

    // Add all
    list.addAll(listToAdd);
    assertEquals(4, list.size());
    assertEquals(roleUnresolved1.toString(), list.get(0).toString());
    assertEquals(roleUnresolved1.toString(), list.get(1).toString());
    assertEquals(roleUnresolved2.toString(), list.get(2).toString());
    assertEquals(roleUnresolved2.toString(), list.get(3).toString());
    Iterator iterator = list.iterator();
    assertEquals(roleUnresolved1.toString(), iterator.next().toString());
    assertEquals(roleUnresolved1.toString(), iterator.next().toString());
    assertEquals(roleUnresolved2.toString(), iterator.next().toString());
    assertEquals(roleUnresolved2.toString(), iterator.next().toString());

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
    setUpRoleUnresolveds();

    RoleUnresolvedList list = new RoleUnresolvedList();
    list.add(roleUnresolved1);
    list.add(roleUnresolved1);
    RoleUnresolvedList listToAdd = new RoleUnresolvedList();
    listToAdd.add(roleUnresolved2);
    listToAdd.add(roleUnresolved2);

    // Add all
    list.addAll(1, listToAdd);
    assertEquals(4, list.size());
    assertEquals(roleUnresolved1.toString(), list.get(0).toString());
    assertEquals(roleUnresolved2.toString(), list.get(1).toString());
    assertEquals(roleUnresolved2.toString(), list.get(2).toString());
    assertEquals(roleUnresolved1.toString(), list.get(3).toString());
    Iterator iterator = list.iterator();
    assertEquals(roleUnresolved1.toString(), iterator.next().toString());
    assertEquals(roleUnresolved2.toString(), iterator.next().toString());
    assertEquals(roleUnresolved2.toString(), iterator.next().toString());
    assertEquals(roleUnresolved1.toString(), iterator.next().toString());

    // Add a role Unresolved in the wrong place
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
      fail ("Shouldn't be able to addAll a roleUnresolved outside of valid range");

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
    setUpRoleUnresolveds();

    try
    {
      ArrayList roleUnresolveds = new ArrayList();
      roleUnresolveds.add(roleUnresolved1);
      roleUnresolveds.add(roleUnresolved2);
      RoleUnresolvedList full = new RoleUnresolvedList(roleUnresolveds);
      RoleUnresolvedList clone = (RoleUnresolvedList) full.clone();
      assertEquals(2, clone.size());
      assertEquals(roleUnresolved1.toString(), clone.get(0).toString());
      assertEquals(roleUnresolved2.toString(), clone.get(1).toString());
      Iterator iterator = clone.iterator();
      assertEquals(roleUnresolved1.toString(), iterator.next().toString());
      assertEquals(roleUnresolved2.toString(), iterator.next().toString());
    }
    catch (IllegalArgumentException e)
    {
      fail("FAILS IN RI: roleUnresolvedList -> RoleList?");
    }
  }

  /**
   * Test serialization.
   */
  public void testSerialization()
  {
    setUpRoleUnresolveds();

    ArrayList roleUnresolveds = new ArrayList();
    roleUnresolveds.add(roleUnresolved1);
    roleUnresolveds.add(roleUnresolved2);
    RoleUnresolvedList full = new RoleUnresolvedList(roleUnresolveds);
    RoleUnresolvedList copy = null;

    try
    {
      // Serialize it
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(full);
    
      // Deserialize it
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new ObjectInputStream(bais);
      copy = (RoleUnresolvedList) ois.readObject();
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
    assertEquals(roleUnresolved1.toString(), copy.get(0).toString());
    assertEquals(roleUnresolved2.toString(), copy.get(1).toString());
    Iterator iterator = copy.iterator();
    assertEquals(roleUnresolved1.toString(), iterator.next().toString());
    assertEquals(roleUnresolved2.toString(), iterator.next().toString());
  }

  // Tests ---------------------------------------------------------------------

  private void setUpRoleUnresolveds()
  {
    if (setUpDone == true)
      return;    
    try
    {
      // Create the roleUnresolveds
      ArrayList roleValue1 = new ArrayList();
      ObjectName a = new ObjectName(":a=a");
      ObjectName b = new ObjectName(":b=b");
      roleValue1.add(a);
      roleValue1.add(b);
      roleUnresolved1 = new RoleUnresolved("RoleName1", roleValue1,
                                           RoleStatus.ROLE_NOT_READABLE);

      ArrayList roleValue2 = new ArrayList();
      ObjectName c = new ObjectName(":c=c");
      ObjectName d = new ObjectName(":d=d");
      roleValue2.add(c);
      roleValue2.add(d);
      roleUnresolved2 = new RoleUnresolved("RoleName2", roleValue2,
                                           RoleStatus.ROLE_NOT_READABLE);
    }
    catch (MalformedObjectNameException mfone)
    {
      fail(mfone.toString());
    }
    setUpDone = true;
  }
}
