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
import java.io.Serializable;

import java.util.ArrayList;
import java.util.Date;

import javax.management.ObjectName;
import javax.management.relation.RelationNotification;
import javax.management.relation.RelationService;

import junit.framework.TestCase;

/**
 * Relation Notification Tests
 *
 * NOTE: These tests use relation service for the source, but it is not
 * tested for compliance. JBossMX uses the relation service's object name
 * to ensure the notification is serializable
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class RelationNotificationTestCase
  extends TestCase
  implements Serializable
{

  // Constants -----------------------------------------------------------------

  static String[] types = new String[]
  {
    RelationNotification.RELATION_BASIC_CREATION,
    RelationNotification.RELATION_MBEAN_CREATION,
    RelationNotification.RELATION_BASIC_UPDATE,
    RelationNotification.RELATION_MBEAN_UPDATE,
    RelationNotification.RELATION_BASIC_REMOVAL,
    RelationNotification.RELATION_MBEAN_REMOVAL
  };

  // Attributes ----------------------------------------------------------------

  // Constructor ---------------------------------------------------------------

  /**
   * Construct the test
   */
  public RelationNotificationTestCase(String s)
  {
    super(s);
  }

  // Tests ---------------------------------------------------------------------

  /**
   * Make sure all the constants are different
   */
  public void testDifferent()
  {
    for (int i = 0; i < (types.length - 1); i++)
    {
      for (int j = i + 1; j < types.length; j++)
        if (types[i].equals(types[j]))
          fail("Relation Notifications types not unique");
    }
  }

  /**
   * Test Basic Creation
   */
  public void testBasicCreation()
  {
    RelationNotification rn = null;
    try
    {
      rn = new RelationNotification(RelationNotification.RELATION_BASIC_CREATION,
             new RelationService(true), 21, 23, "message", "relationId", 
             "relationTypeName", null, null);
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    assertEquals(RelationNotification.RELATION_BASIC_CREATION, rn.getType());
    assertEquals(21, rn.getSequenceNumber());
    assertEquals(23, rn.getTimeStamp());
    assertEquals("message", rn.getMessage());
    assertEquals("relationId", rn.getRelationId());
    assertEquals("relationTypeName", rn.getRelationTypeName());
    assertEquals(null, rn.getObjectName());
    assertEquals(0, rn.getMBeansToUnregister().size());
  }

  /**
   * Test Basic Removal
   */
  public void testBasicRemoval()
  {
    RelationNotification rn = null;
    ObjectName objectName = null;
    ArrayList unregs = new ArrayList();
    try
    {
      rn = new RelationNotification(RelationNotification.RELATION_BASIC_REMOVAL,
             new RelationService(true), 21, 23, "message", "relationId", 
             "relationTypeName", null, unregs);
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    assertEquals(RelationNotification.RELATION_BASIC_REMOVAL, rn.getType());
    assertEquals(21, rn.getSequenceNumber());
    assertEquals(23, rn.getTimeStamp());
    assertEquals("message", rn.getMessage());
    assertEquals("relationId", rn.getRelationId());
    assertEquals("relationTypeName", rn.getRelationTypeName());
    assertEquals(null, rn.getObjectName());
    assertEquals(unregs, rn.getMBeansToUnregister());
  }

  /**
   * Test MBean Creation
   */
  public void testMBeanCreation()
  {
    RelationNotification rn = null;
    ObjectName objectName = null;
    try
    {
      objectName = new ObjectName(":a=a");
      rn = new RelationNotification(RelationNotification.RELATION_MBEAN_CREATION,
             new RelationService(true), 21, 23, "message", "relationId", 
             "relationTypeName", objectName, null);
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    assertEquals(RelationNotification.RELATION_MBEAN_CREATION, rn.getType());
    assertEquals(21, rn.getSequenceNumber());
    assertEquals(23, rn.getTimeStamp());
    assertEquals("message", rn.getMessage());
    assertEquals("relationId", rn.getRelationId());
    assertEquals("relationTypeName", rn.getRelationTypeName());
    assertEquals(objectName, rn.getObjectName());
    assertEquals(0, rn.getMBeansToUnregister().size());
  }

  /**
   * Test MBean Removal
   */
  public void testMBeanRemoval()
  {
    RelationNotification rn = null;
    ObjectName objectName = null;
    ArrayList unregs = new ArrayList();
    try
    {
      objectName = new ObjectName(":a=a");
      rn = new RelationNotification(RelationNotification.RELATION_MBEAN_REMOVAL,
             new RelationService(true), 21, 23, "message", "relationId", 
             "relationTypeName", objectName, unregs);
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    assertEquals(RelationNotification.RELATION_MBEAN_REMOVAL, rn.getType());
    assertEquals(21, rn.getSequenceNumber());
    assertEquals(23, rn.getTimeStamp());
    assertEquals("message", rn.getMessage());
    assertEquals("relationId", rn.getRelationId());
    assertEquals("relationTypeName", rn.getRelationTypeName());
    assertEquals(objectName, rn.getObjectName());
    assertEquals(unregs, rn.getMBeansToUnregister());
  }

  /**
   * Test Basic Update
   */
  public void testBasicUpdate()
  {
    RelationNotification rn = null;
    ArrayList newRoles = new ArrayList();
    ArrayList oldRoles = new ArrayList();
    try
    {
      rn = new RelationNotification(RelationNotification.RELATION_BASIC_UPDATE,
             new RelationService(true), 21, 23, "message", "relationId", 
             "relationTypeName", null, "roleName", newRoles, oldRoles);
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    assertEquals(RelationNotification.RELATION_BASIC_UPDATE, rn.getType());
    assertEquals(21, rn.getSequenceNumber());
    assertEquals(23, rn.getTimeStamp());
    assertEquals("message", rn.getMessage());
    assertEquals("relationId", rn.getRelationId());
    assertEquals("relationTypeName", rn.getRelationTypeName());
    assertEquals(null, rn.getObjectName());
    assertEquals("roleName", rn.getRoleName());
    assertEquals(0, rn.getNewRoleValue().size());
    assertEquals(0, rn.getOldRoleValue().size());
  }

  /**
   * Test MBean Update
   */
  public void testMBeanUpdate()
  {
    RelationNotification rn = null;
    ObjectName objectName = null;
    ArrayList newRoles = new ArrayList();
    ArrayList oldRoles = new ArrayList();
    try
    {
      objectName = new ObjectName(":a=a");
      rn = new RelationNotification(RelationNotification.RELATION_MBEAN_UPDATE,
             new RelationService(true), 21, 23, "message", "relationId", 
             "relationTypeName", objectName, "roleName", newRoles, oldRoles);
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    assertEquals(RelationNotification.RELATION_MBEAN_UPDATE, rn.getType());
    assertEquals(21, rn.getSequenceNumber());
    assertEquals(23, rn.getTimeStamp());
    assertEquals("message", rn.getMessage());
    assertEquals("relationId", rn.getRelationId());
    assertEquals("relationTypeName", rn.getRelationTypeName());
    assertEquals(objectName, rn.getObjectName());
    assertEquals("roleName", rn.getRoleName());
    assertEquals(0, rn.getNewRoleValue().size());
    assertEquals(0, rn.getOldRoleValue().size());
  }

  /**
   * Test Creation/Removal Error Handling
   */
  public void testCreationRemovalErrors()
  {
    RelationNotification rn = null;
    ObjectName objectName = null;
    try
    {
      objectName = new ObjectName(":a=a");
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    ArrayList unregs = new ArrayList();

    boolean caught = false;
    try
    {
      rn = new RelationNotification("blah",
             new RelationService(true), 21, 23, "message", "relationId", 
             "relationTypeName", objectName, unregs);
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
      fail("Creation/Removal accepts an invalid type");

    caught = false;
    try
    {
      rn = new RelationNotification(RelationNotification.RELATION_BASIC_UPDATE,
             new RelationService(true), 21, 23, "message", "relationId", 
             "relationTypeName", objectName, unregs);
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
      fail("Creation/Removal accepts basic update");

    caught = false;
    try
    {
      rn = new RelationNotification(RelationNotification.RELATION_MBEAN_UPDATE,
             new RelationService(true), 21, 23, "message", "relationId", 
             "relationTypeName", objectName, unregs);
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
      fail("Creation/Removal accepts mean update");

    caught = false;
    try
    {
      rn = new RelationNotification(RelationNotification.RELATION_BASIC_CREATION,
             null, 21, 23, "message", "relationId", 
             "relationTypeName", objectName, unregs);
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
      fail("Creation/Removal accepts null source");

    caught = false;
    try
    {
      rn = new RelationNotification(RelationNotification.RELATION_BASIC_CREATION,
             new RelationService(true), 21, 23, "message", null, 
             "relationTypeName", objectName, unregs);
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
      fail("Creation/Removal accepts null relation id");

    caught = false;
    try
    {
      rn = new RelationNotification(RelationNotification.RELATION_BASIC_CREATION,
             new RelationService(true), 21, 23, "message", "relation id", 
             null, objectName, unregs);
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
      fail("Creation/Removal accepts null relation type name");
  }

  /**
   * Test Creation/Removal Error Handling
   */
  public void testCreationRemovalErrors2()
  {
    RelationNotification rn = null;
    ObjectName objectName = null;
    try
    {
      objectName = new ObjectName(":a=a");
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    ArrayList unregs = new ArrayList();

    boolean caught = false;
    try
    {
      rn = new RelationNotification(null,
             new RelationService(true), 21, 23, "message", "relationId", 
             "relationTypeName", objectName, unregs);
    }
    catch (NullPointerException e)
    {
      fail("FAILS IN RI: Throws the wrong exception type");
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
      fail("Creation/Removal accepts an a null type");
  }

  /**
   * Test Update Error Handling
   */
  public void testUpdateErrors()
  {
    RelationNotification rn = null;
    ObjectName objectName = null;
    try
    {
      objectName = new ObjectName(":a=a");
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    ArrayList newRoles = new ArrayList();
    ArrayList oldRoles = new ArrayList();

    boolean caught = false;
    try
    {
      rn = new RelationNotification("blah",
             new RelationService(true), 21, 23, "message", "relationId", 
             "relationTypeName", objectName, "roleInfo", newRoles, oldRoles);
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
      fail("Update accepts an invalid type");

    caught = false;
    try
    {
      rn = new RelationNotification(RelationNotification.RELATION_BASIC_CREATION,
             new RelationService(true), 21, 23, "message", "relationId", 
             "relationTypeName", objectName, "roleInfo", newRoles, oldRoles);
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
      fail("Update accepts basic create");

    caught = false;
    try
    {
      rn = new RelationNotification(RelationNotification.RELATION_MBEAN_CREATION,
             new RelationService(true), 21, 23, "message", "relationId", 
             "relationTypeName", objectName, "roleInfo", newRoles, oldRoles);
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
      fail("Creation/Removal accepts mean create");

    caught = false;
    try
    {
      rn = new RelationNotification(RelationNotification.RELATION_BASIC_REMOVAL,
             new RelationService(true), 21, 23, "message", "relationId", 
             "relationTypeName", objectName, "roleInfo", newRoles, oldRoles);
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
      fail("Update accepts basic remove");

    caught = false;
    try
    {
      rn = new RelationNotification(RelationNotification.RELATION_MBEAN_REMOVAL,
             new RelationService(true), 21, 23, "message", "relationId", 
             "relationTypeName", objectName, "roleInfo", newRoles, oldRoles);
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
      fail("Update accepts mean remove");

    caught = false;
    try
    {
      rn = new RelationNotification(RelationNotification.RELATION_BASIC_UPDATE,
             null, 21, 23, "message", "relationId", 
             "relationTypeName", objectName, "roleInfo", newRoles, oldRoles);
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
      fail("Update accepts null source");

    caught = false;
    try
    {
      rn = new RelationNotification(RelationNotification.RELATION_BASIC_UPDATE,
             new RelationService(true), 21, 23, "message", null, 
             "relationTypeName", objectName, "roleInfo", newRoles, oldRoles);
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
      fail("Update accepts null relation id");

    caught = false;
    try
    {
      rn = new RelationNotification(RelationNotification.RELATION_BASIC_UPDATE,
             new RelationService(true), 21, 23, "message", "relation id", 
             null, objectName, "roleInfo", newRoles, oldRoles);
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
      fail("Update accepts null relation type name");

    caught = false;
    try
    {
      rn = new RelationNotification(RelationNotification.RELATION_BASIC_UPDATE,
             new RelationService(true), 21, 23, "message", "relation id", 
             null, objectName, null, newRoles, oldRoles);
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
      fail("Update accepts null role info");

    caught = false;
    try
    {
      rn = new RelationNotification(RelationNotification.RELATION_BASIC_UPDATE,
             new RelationService(true), 21, 23, "message", "relation id", 
             "relationTypeName", objectName, "roleInfo", null, oldRoles);
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
      fail("Creation/Removal accepts null new role value");

    caught = false;
    try
    {
      rn = new RelationNotification(RelationNotification.RELATION_BASIC_UPDATE,
             new RelationService(true), 21, 23, "message", "relation id", 
             "relationTypeName", objectName, "roleInfo", newRoles, null);
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
      fail("Update accepts null old role value");
  }

  /**
   * Test Update Error Handling
   */
  public void testUpdateErrors2()
  {
    RelationNotification rn = null;
    ObjectName objectName = null;
    try
    {
      objectName = new ObjectName(":a=a");
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    ArrayList newRoles = new ArrayList();
    ArrayList oldRoles = new ArrayList();

    boolean caught = false;
    try
    {
      rn = new RelationNotification(null,
             new RelationService(true), 21, 23, "message", "relationId", 
             "relationTypeName", objectName, "roleInfo", newRoles, oldRoles);
    }
    catch (NullPointerException e)
    {
      fail("FAILS IN RI: Throws the wrong exception type");
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
      fail("Update accepts an a null type");
  }

  /**
   * Test serialization.
   */
/*  public void testSerializationBasicCreation()
  {
    RelationNotification orig = null;
    RelationNotification rn = null;
    try
    {
      orig = new RelationNotification(RelationNotification.RELATION_BASIC_CREATION,
             new RelationService(true), 21, 23, "message", "relationId", 
             "relationTypeName", null, null);

      // Serialize it
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(orig);
    
      // Deserialize it
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new ObjectInputStream(bais);
      rn = (RelationNotification) ois.readObject();
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    assertEquals(RelationNotification.RELATION_BASIC_CREATION, rn.getType());
    assertEquals(21, rn.getSequenceNumber());
    assertEquals(23, rn.getTimeStamp());
    assertEquals("message", rn.getMessage());
    assertEquals("relationId", rn.getRelationId());
    assertEquals("relationTypeName", rn.getRelationTypeName());
    assertEquals(null, rn.getObjectName());
    assertEquals(0, rn.getMBeansToUnregister().size());
  }
*/
  /**
   * Test serialization.
   */
/*  public void testSerializationBasicRemoval()
  {
    RelationNotification orig = null;
    RelationNotification rn = null;
    ObjectName objectName = null;
    ArrayList unregs = new ArrayList();
    try
    {
      orig = new RelationNotification(RelationNotification.RELATION_BASIC_REMOVAL,
             new RelationService(true), 21, 23, "message", "relationId", 
             "relationTypeName", null, unregs);

      // Serialize it
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(orig);
    
      // Deserialize it
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new ObjectInputStream(bais);
      rn = (RelationNotification) ois.readObject();
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    assertEquals(RelationNotification.RELATION_BASIC_REMOVAL, rn.getType());
    assertEquals(21, rn.getSequenceNumber());
    assertEquals(23, rn.getTimeStamp());
    assertEquals("message", rn.getMessage());
    assertEquals("relationId", rn.getRelationId());
    assertEquals("relationTypeName", rn.getRelationTypeName());
    assertEquals(null, rn.getObjectName());
    assertEquals(unregs, rn.getMBeansToUnregister());
  }
*/

  /**
   * Test serialization.
   */
/*  public void testSerializationMBeanCreation()
  {
    RelationNotification orig = null;
    RelationNotification rn = null;
    ObjectName objectName = null;
    try
    {
      objectName = new ObjectName(":a=a");
      orig = new RelationNotification(RelationNotification.RELATION_MBEAN_CREATION,
             new RelationService(true), 21, 23, "message", "relationId", 
             "relationTypeName", objectName, null);

      // Serialize it
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(orig);
    
      // Deserialize it
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new ObjectInputStream(bais);
      rn = (RelationNotification) ois.readObject();
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    assertEquals(RelationNotification.RELATION_MBEAN_CREATION, rn.getType());
    assertEquals(21, rn.getSequenceNumber());
    assertEquals(23, rn.getTimeStamp());
    assertEquals("message", rn.getMessage());
    assertEquals("relationId", rn.getRelationId());
    assertEquals("relationTypeName", rn.getRelationTypeName());
    assertEquals(objectName, rn.getObjectName());
    assertEquals(0, rn.getMBeansToUnregister().size());
  }
*/

  /**
   * Test serialization.
   */
/*  public void testSerializationMBeanRemoval()
  {
    RelationNotification orig = null;
    RelationNotification rn = null;
    ObjectName objectName = null;
    ArrayList unregs = new ArrayList();
    try
    {
      objectName = new ObjectName(":a=a");
      orig = new RelationNotification(RelationNotification.RELATION_MBEAN_REMOVAL,
             new RelationService(true), 21, 23, "message", "relationId", 
             "relationTypeName", objectName, unregs);

      // Serialize it
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(orig);
    
      // Deserialize it
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new ObjectInputStream(bais);
      rn = (RelationNotification) ois.readObject();
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    assertEquals(RelationNotification.RELATION_MBEAN_REMOVAL, rn.getType());
    assertEquals(21, rn.getSequenceNumber());
    assertEquals(23, rn.getTimeStamp());
    assertEquals("message", rn.getMessage());
    assertEquals("relationId", rn.getRelationId());
    assertEquals("relationTypeName", rn.getRelationTypeName());
    assertEquals(objectName, rn.getObjectName());
    assertEquals(unregs, rn.getMBeansToUnregister());
  }
*/

  /**
   * Test serialization.
   */
/*  public void testSerializationBasicUpdate()
  {
    RelationNotification orig = null;
    RelationNotification rn = null;
    ArrayList newRoles = new ArrayList();
    ArrayList oldRoles = new ArrayList();
    try
    {
      orig = new RelationNotification(RelationNotification.RELATION_BASIC_UPDATE,
             new RelationService(true), 21, 23, "message", "relationId", 
             "relationTypeName", null, "roleName", newRoles, oldRoles);

      // Serialize it
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(orig);
    
      // Deserialize it
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new ObjectInputStream(bais);
      rn = (RelationNotification) ois.readObject();
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    assertEquals(RelationNotification.RELATION_BASIC_UPDATE, rn.getType());
    assertEquals(21, rn.getSequenceNumber());
    assertEquals(23, rn.getTimeStamp());
    assertEquals("message", rn.getMessage());
    assertEquals("relationId", rn.getRelationId());
    assertEquals("relationTypeName", rn.getRelationTypeName());
    assertEquals(null, rn.getObjectName());
    assertEquals("roleName", rn.getRoleName());
    assertEquals(0, rn.getNewRoleValue().size());
    assertEquals(0, rn.getOldRoleValue().size());
  }
*/

  /**
   * Test serialization.
   */
/*  public void testSerializationMBeanUpdate()
  {
    RelationNotification orig = null;
    RelationNotification rn = null;
    ObjectName objectName = null;
    ArrayList newRoles = new ArrayList();
    ArrayList oldRoles = new ArrayList();
    try
    {
      objectName = new ObjectName(":a=a");
      orig = new RelationNotification(RelationNotification.RELATION_MBEAN_UPDATE,
             new RelationService(true), 21, 23, "message", "relationId", 
             "relationTypeName", objectName, "roleName", newRoles, oldRoles);

      // Serialize it
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(orig);
    
      // Deserialize it
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new ObjectInputStream(bais);
      rn = (RelationNotification) ois.readObject();
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    assertEquals(RelationNotification.RELATION_MBEAN_UPDATE, rn.getType());
    assertEquals(21, rn.getSequenceNumber());
    assertEquals(23, rn.getTimeStamp());
    assertEquals("message", rn.getMessage());
    assertEquals("relationId", rn.getRelationId());
    assertEquals("relationTypeName", rn.getRelationTypeName());
    assertEquals(objectName, rn.getObjectName());
    assertEquals("roleName", rn.getRoleName());
    assertEquals(0, rn.getNewRoleValue().size());
    assertEquals(0, rn.getOldRoleValue().size());
  }
*/
}
