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

import javax.management.NotCompliantMBeanException;
import javax.management.relation.InvalidRoleInfoException;
import javax.management.relation.RelationSupport;
import javax.management.relation.RoleInfo;

import junit.framework.TestCase;

/**
 * Role Info tests.<p>
 *
 * Test it to death.<p>
 *
 * NOTE: The tests use String literals to ensure the comparisons are
 *       not performed on object references.
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class RoleInfoTestCase
  extends TestCase
{
  // Attributes ----------------------------------------------------------------

  // Constructor ---------------------------------------------------------------

  /**
   * Construct the test
   */
  public RoleInfoTestCase(String s)
  {
    super(s);
  }

  // Tests ---------------------------------------------------------------------

  /**
   * Basic tests.
   */
  public void testBasic()
  {
    RoleInfo roleInfo = null;

    // Minimal Constructor
    try
    {
      roleInfo = new RoleInfo("RoleName", RelationSupport.class.getName());
    }
    catch (Exception e)
    {
      fail(e.toString());
    }

    // Did it work?
    assertEquals(roleInfo.getName(), "RoleName");
    assertEquals(roleInfo.getRefMBeanClassName(), RelationSupport.class.getName());
    assertEquals(roleInfo.isReadable(), true);
    assertEquals(roleInfo.isWritable(), true);
    assertEquals(roleInfo.getMinDegree(), 1);
    assertEquals(roleInfo.getMaxDegree(), 1);
    assertEquals(roleInfo.getDescription(), null);

    // Partial Constructor
    try
    {
      roleInfo = new RoleInfo("RoleName", RelationSupport.class.getName(), 
                              false, false);
    }
    catch (Exception e)
    {
      fail(e.toString());
    }

    // Did it work?
    assertEquals(roleInfo.getName(), "RoleName");
    assertEquals(roleInfo.getRefMBeanClassName(), RelationSupport.class.getName());
    assertEquals(roleInfo.isReadable(), false);
    assertEquals(roleInfo.isWritable(), false);
    assertEquals(roleInfo.getMinDegree(), 1);
    assertEquals(roleInfo.getMaxDegree(), 1);
    assertEquals(roleInfo.getDescription(), null);

    // Full Constructor
    try
    {
      roleInfo = new RoleInfo("RoleName", RelationSupport.class.getName(), 
                              false, false, 23, 25, "Description");
    }
    catch (Exception e)
    {
      fail(e.toString());
    }

    // Did it work?
    assertEquals(roleInfo.getName(), "RoleName");
    assertEquals(roleInfo.getRefMBeanClassName(), RelationSupport.class.getName());
    assertEquals(roleInfo.isReadable(), false);
    assertEquals(roleInfo.isWritable(), false);
    assertEquals(roleInfo.getMinDegree(), 23);
    assertEquals(roleInfo.getMaxDegree(), 25);
    assertEquals(roleInfo.getDescription(), "Description");
  }

  /**
   * Test Error Handling.
   */
  public void testErrorHandling()
  {
    RoleInfo roleInfo = null;

    boolean caught = false;
    try
    {
      roleInfo = new RoleInfo(null);
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
      fail("Copy Constructor accepts null role info");

    caught = false;
    try
    {
      roleInfo = new RoleInfo(null, RelationSupport.class.getName());
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
      fail("Constructor accepts null role name (1)");

    caught = false;
    try
    {
      roleInfo = new RoleInfo(null, RelationSupport.class.getName(), true, true);
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
      fail("Constructor accepts null role name (2)");

    caught = false;
    try
    {
      roleInfo = new RoleInfo(null, RelationSupport.class.getName(), true, true,
                              1, 1, "blah");
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
      fail("Constructor accepts null role name (3)");

    caught = false;
    try
    {
      roleInfo = new RoleInfo("RoleName", null);
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
      fail("Constructor accepts null class name (1)");

    caught = false;
    try
    {
      roleInfo = new RoleInfo("RoleName", null, true, true);
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
      fail("Constructor accepts null class name (2)");

    caught = false;
    try
    {
      roleInfo = new RoleInfo("RoleName", null, true, true,
                              1, 1, "blah");
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
      fail("Constructor accepts null class name (3)");

    caught = false;
    try
    {
      roleInfo = new RoleInfo("RoleName", "Inv alid");
    }
    catch (ClassNotFoundException e)
    {
      caught = true;
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    if (caught == false)
      fail("Constructor accepts invalid class name (1)");

    caught = false;
    try
    {
      roleInfo = new RoleInfo("RoleName", "Inv alid", true, true);
    }
    catch (ClassNotFoundException e)
    {
      caught = true;
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    if (caught == false)
      fail("Constructor accepts invalid class name (2)");

    caught = false;
    try
    {
      roleInfo = new RoleInfo("RoleName", "Inv alid", true, true,
                              1, 1, "blah");
    }
    catch (ClassNotFoundException e)
    {
      caught = true;
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    if (caught == false)
      fail("Constructor accepts invalid class name (3)");

    caught = false;
    try
    {
      roleInfo = new RoleInfo("RoleName", RoleInfo.class.getName());
    }
    catch (NotCompliantMBeanException e)
    {
      caught = true;
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    if (caught == false)
      fail("Constructor accepts not compliant mbean (1)");

    caught = false;
    try
    {
      roleInfo = new RoleInfo("RoleName", RoleInfo.class.getName(), true, true);
    }
    catch (NotCompliantMBeanException e)
    {
      caught = true;
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    if (caught == false)
      fail("Constructor accepts not compliant mbean (2)");

    caught = false;
    try
    {
      roleInfo = new RoleInfo("RoleName", RoleInfo.class.getName(), true, true,
                              1, 1, "blah");
    }
    catch (NotCompliantMBeanException e)
    {
      caught = true;
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    if (caught == false)
      fail("Constructor accepts not compliant mbean (3)");
  }

  /**
   * Test constructor cardinality.
   */
  public void testConstructorCardinality()
  {
    // Create the role info
    RoleInfo roleInfo = null;

    // It's allow by the spec?????
    try
    {
      roleInfo = new RoleInfo("RoleName", RelationSupport.class.getName(), 
                            false, false, 0, 0, "Description");
    }
    catch (Exception e)
    {
      fail(e.toString());
    }

    boolean caught = false;
    try
    {
      roleInfo = new RoleInfo("RoleName", RelationSupport.class.getName(), 
                            false, false, 1, 0, "Description");
    }
    catch (InvalidRoleInfoException e)
    {
      caught = true;
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    if (caught == false)
      fail("Shouldn't allow minimum of 1 and maximum of 0");

    caught = false;
    try
    {
      roleInfo = new RoleInfo("RoleName", RelationSupport.class.getName(), 
                            false, false, RoleInfo.ROLE_CARDINALITY_INFINITY,
                            0, "Description");
    }
    catch (InvalidRoleInfoException e)
    {
      caught = true;
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    if (caught == false)
      fail("Shouldn't allow infinite minimum without infinite maximum");
  }

  /**
   * Test the degree checkers.
   */
  public void testCheckDegrees()
  {
    // Create the role info
    RoleInfo roleInfo = null;
    RoleInfo roleInfo2 = null;

    try
    {
      roleInfo = new RoleInfo("RoleName", RelationSupport.class.getName(), 
                            false, false, 23, 25, "Description");
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    assertEquals(true, roleInfo.checkMaxDegree(0));
    assertEquals(true, roleInfo.checkMaxDegree(22));
    assertEquals(true, roleInfo.checkMaxDegree(23));
    assertEquals(true, roleInfo.checkMaxDegree(24));
    assertEquals(true, roleInfo.checkMaxDegree(25));
    assertEquals(false, roleInfo.checkMaxDegree(26));
    assertEquals(false, roleInfo.checkMaxDegree(Integer.MAX_VALUE));

    assertEquals(false, roleInfo.checkMinDegree(0));
    assertEquals(false, roleInfo.checkMinDegree(22));
    assertEquals(true, roleInfo.checkMinDegree(23));
    assertEquals(true, roleInfo.checkMinDegree(24));
    assertEquals(true, roleInfo.checkMinDegree(25));
    assertEquals(true, roleInfo.checkMinDegree(26));
    assertEquals(true, roleInfo.checkMinDegree(Integer.MAX_VALUE));

    try
    {
      roleInfo = new RoleInfo("RoleName", RelationSupport.class.getName(), 
                            false, false, 25, 
                            RoleInfo.ROLE_CARDINALITY_INFINITY, "Description");
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    assertEquals(true, roleInfo.checkMaxDegree(0));
    assertEquals(true, roleInfo.checkMaxDegree(24));
    assertEquals(true, roleInfo.checkMaxDegree(25));
    assertEquals(true, roleInfo.checkMaxDegree(26));
    assertEquals(true, roleInfo.checkMaxDegree(Integer.MAX_VALUE));

    assertEquals(false, roleInfo.checkMinDegree(0));
    assertEquals(false, roleInfo.checkMinDegree(24));
    assertEquals(true, roleInfo.checkMinDegree(25));
    assertEquals(true, roleInfo.checkMinDegree(26));
    assertEquals(true, roleInfo.checkMinDegree(Integer.MAX_VALUE));

    try
    {
      roleInfo = new RoleInfo("RoleName", RelationSupport.class.getName(), 
                            false, false, RoleInfo.ROLE_CARDINALITY_INFINITY, 
                            RoleInfo.ROLE_CARDINALITY_INFINITY, "Description");
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    assertEquals(true, roleInfo.checkMaxDegree(0));
    assertEquals(true, roleInfo.checkMaxDegree(26));
    assertEquals(true, roleInfo.checkMaxDegree(Integer.MAX_VALUE));

    assertEquals(true, roleInfo.checkMinDegree(0));
    assertEquals(true, roleInfo.checkMinDegree(24));
    assertEquals(true, roleInfo.checkMinDegree(Integer.MAX_VALUE));
  }

  /**
   * Test copy constructor.
   */
  public void testCopy()
  {
    // Create the role info
    RoleInfo roleInfo = null;
    RoleInfo roleInfo2 = null;

    try
    {
      roleInfo = new RoleInfo("RoleName", RelationSupport.class.getName(), 
                            false, false, 23, 25, "Description");
      roleInfo2 = new RoleInfo(roleInfo);
    }
    catch (Exception e)
    {
      fail(e.toString());
    }

    // Did it work?
    assertEquals(roleInfo.getName(), roleInfo2.getName());
    assertEquals(roleInfo.getRefMBeanClassName(), roleInfo2.getRefMBeanClassName());
    assertEquals(roleInfo.isReadable(), roleInfo2.isReadable());
    assertEquals(roleInfo.isWritable(), roleInfo2.isWritable());
    assertEquals(roleInfo.getMinDegree(), roleInfo2.getMinDegree());
    assertEquals(roleInfo.getMaxDegree(), roleInfo2.getMaxDegree());
    assertEquals(roleInfo.getDescription(), roleInfo2.getDescription());
  }

  /**
   * Test serialization.
   */
  public void testSerialization()
  {
    // Create the role info
    RoleInfo roleInfo = null;
    RoleInfo roleInfo2 = null;

    try
    {
      roleInfo = new RoleInfo("RoleName", RelationSupport.class.getName(), 
                              false, false, 23, 25, "Description");
      // Serialize it
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(roleInfo);
    
      // Deserialize it
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new ObjectInputStream(bais);
      roleInfo2 = (RoleInfo) ois.readObject();
    }
    catch (Exception e)
    {
      fail(e.toString());
    }

    // Did it work?
    assertEquals(roleInfo.getName(), roleInfo2.getName());
    assertEquals(roleInfo.getRefMBeanClassName(), roleInfo2.getRefMBeanClassName());
    assertEquals(roleInfo.isReadable(), roleInfo2.isReadable());
    assertEquals(roleInfo.isWritable(), roleInfo2.isWritable());
    assertEquals(roleInfo.getMinDegree(), roleInfo2.getMinDegree());
    assertEquals(roleInfo.getMaxDegree(), roleInfo2.getMaxDegree());
    assertEquals(roleInfo.getDescription(), roleInfo2.getDescription());
  }
}
