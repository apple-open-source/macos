/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.varia;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;

import java.util.Vector;

import junit.framework.TestCase;

import javax.management.Notification;
import javax.management.NotificationFilterSupport;

/**
 * Notification Filter Support tests.<p>
 *
 * Test it to death.<p>
 *
 * NOTE: The tests use String literals to ensure the comparisons are
 *       not performed on object references.
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class NotificationFilterSupportTestCase
  extends TestCase
{
  // Attributes ----------------------------------------------------------------

  /**
   * Test notifications
   */
  Notification n1 = new Notification("type1", new Object(), 1);
  Notification n2 = new Notification("type1", new Object(), 2);
  Notification n3 = new Notification("type1plus", new Object(), 3);
  Notification n4 = new Notification("type2", new Object(), 4);
  Notification n5 = new Notification("type2", new Object(), 5);

  // Constructor ---------------------------------------------------------------

  /**
   * Construct the test
   */
  public NotificationFilterSupportTestCase(String s)
  {
    super(s);
  }

  // Tests ---------------------------------------------------------------------

  /**
   * By default all types are disabled.
   */
  public void testDefault()
  {
    NotificationFilterSupport nfs = new NotificationFilterSupport();
    assertEquals(false, nfs.isNotificationEnabled(n1));
    assertEquals(false, nfs.isNotificationEnabled(n2));
    assertEquals(false, nfs.isNotificationEnabled(n3));
    assertEquals(false, nfs.isNotificationEnabled(n4));
    assertEquals(false, nfs.isNotificationEnabled(n5));
  }

  /**
   * Enable a single type, all others should be disabled.
   */
  public void testEnableType()
  {
    NotificationFilterSupport nfs = new NotificationFilterSupport();
    nfs.enableType("type1plus");
    assertEquals(false, nfs.isNotificationEnabled(n1));
    assertEquals(false, nfs.isNotificationEnabled(n2));
    assertEquals(true, nfs.isNotificationEnabled(n3));
    assertEquals(false, nfs.isNotificationEnabled(n4));
    assertEquals(false, nfs.isNotificationEnabled(n5));
  }

  /**
   * Enable some types then disable everyting.
   */
  public void testDisableAllTypes()
  {
    NotificationFilterSupport nfs = new NotificationFilterSupport();
    nfs.enableType("type1");
    nfs.enableType("type2");
    nfs.disableAllTypes();
    assertEquals(false, nfs.isNotificationEnabled(n1));
    assertEquals(false, nfs.isNotificationEnabled(n2));
    assertEquals(false, nfs.isNotificationEnabled(n3));
    assertEquals(false, nfs.isNotificationEnabled(n4));
    assertEquals(false, nfs.isNotificationEnabled(n5));
  }

  /**
   * Enable some types the disable one of them.
   */
  public void testDisableType()
  {
    NotificationFilterSupport nfs = new NotificationFilterSupport();
    nfs.enableType("type1");
    nfs.enableType("type2");
    nfs.disableType("type1");
    assertEquals(false, nfs.isNotificationEnabled(n1));
    assertEquals(false, nfs.isNotificationEnabled(n2));
    assertEquals(false, nfs.isNotificationEnabled(n3));
    assertEquals(true, nfs.isNotificationEnabled(n4));
    assertEquals(true, nfs.isNotificationEnabled(n5));
  }

  /**
   * Enable a prefix that matched multiple types.
   */
  public void testPrefix()
  {
    NotificationFilterSupport nfs = new NotificationFilterSupport();
    nfs.enableType("type1");
    assertEquals(true, nfs.isNotificationEnabled(n1));
    assertEquals(true, nfs.isNotificationEnabled(n2));
    assertEquals(true, nfs.isNotificationEnabled(n3));
    assertEquals(false, nfs.isNotificationEnabled(n4));
    assertEquals(false, nfs.isNotificationEnabled(n5));
  }

  /**
   * Test the get enabled types.
   */
  public void testGetEnabledTypes()
  {
    Vector v;
    NotificationFilterSupport nfs = new NotificationFilterSupport();

    // By default should contain nothing
    assertEquals(0, nfs.getEnabledTypes().size());

    // Add two
    nfs.enableType("type1");
    nfs.enableType("type2");
    v = nfs.getEnabledTypes();
    assertEquals(2, v.size());
    assertEquals(true, v.contains("type1"));
    assertEquals(true, v.contains("type2"));

    // Remove one
    nfs.disableType("type1");
    v = nfs.getEnabledTypes();
    assertEquals(1, v.size());
    assertEquals(false, v.contains("type1"));
    assertEquals(true, v.contains("type2"));

    // Remove all
    nfs.enableType("type2");
    nfs.disableAllTypes();
    v = nfs.getEnabledTypes();
    assertEquals(0, v.size());

    // Test duplication
    nfs.enableType("type1");
    nfs.enableType("type1");
    v = nfs.getEnabledTypes();
    assertEquals(1, v.size());

    // Test duplication removal
    nfs.disableType("type1");
    v = nfs.getEnabledTypes();
    assertEquals(0, v.size());
  }

  /**
   * Test serialization.
   */
  public void testSerialization()
  {
    NotificationFilterSupport nfs = new NotificationFilterSupport();
    NotificationFilterSupport nfs2 = null;

    // Add two
    nfs.enableType("type1");
    nfs.enableType("type2");

    try
    {
      // Serialize it
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(nfs);
    
      // Deserialize it
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new ObjectInputStream(bais);
      nfs2 = (NotificationFilterSupport) ois.readObject();
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
    Vector v = nfs2.getEnabledTypes();
    assertEquals(2, v.size());
    assertEquals(true, v.contains("type1"));
    assertEquals(true, v.contains("type2"));
  }
}
