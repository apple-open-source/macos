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

import java.util.Vector;

import junit.framework.TestCase;

import javax.management.MBeanServerNotification;
import javax.management.ObjectName;

import javax.management.relation.MBeanServerNotificationFilter;

/**
 * MBean Server Notification Filter tests.<p>
 *
 * Test it to death.<p>
 *
 * NOTE: The tests use String literals to ensure the comparisons are
 *       not performed on object references.<p>
 *
 * WARNING!! WARNING!! The spec says the MBeanServerNotificationFilter
 * accepts everything by default. The RI does exactly the opposite.
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class MBeanServerNotificationFilterTestCase
  extends TestCase
{
  // Attributes ----------------------------------------------------------------

  MBeanServerNotificationFilter mbsnf;
  ObjectName on1;
  ObjectName on2;

  MBeanServerNotification n1;
  MBeanServerNotification n2;

  // Constructor ---------------------------------------------------------------

  /**
   * Construct the test
   */
  public MBeanServerNotificationFilterTestCase(String s)
  {
    super(s);
  }

  // Tests ---------------------------------------------------------------------

  /**
   * By default all names are enabled.
   */
  public void testDefault()
  {
    setUpTest();
    mbsnf.enableObjectName(on1);
    mbsnf.enableObjectName(on2);
    assertEquals(true, mbsnf.isNotificationEnabled(n1));
    assertEquals(true, mbsnf.isNotificationEnabled(n2));
  }

  /**
   * Enable all
   */
  public void testEnableAll()
  {
    setUpTest();
    mbsnf.enableAllObjectNames();
    assertEquals(true, mbsnf.isNotificationEnabled(n1));
    assertEquals(true, mbsnf.isNotificationEnabled(n2));
  }

  /**
   * Enable one
   */
  public void testEnableOne()
  {
    setUpTest();
    mbsnf.enableObjectName(on2);
    assertEquals(false, mbsnf.isNotificationEnabled(n1));
    assertEquals(true, mbsnf.isNotificationEnabled(n2));
  }

  /**
   * Disable all
   */
  public void testDisableAll()
  {
    setUpTest();
    mbsnf.enableObjectName(on1);
    mbsnf.disableAllObjectNames();
    assertEquals(false, mbsnf.isNotificationEnabled(n1));
    assertEquals(false, mbsnf.isNotificationEnabled(n2));
  }

  /**
   * Disable one
   */
  public void testDisableOne()
  {
    setUpTest();
    mbsnf.enableAllObjectNames();
    mbsnf.disableObjectName(on2);
    assertEquals(true, mbsnf.isNotificationEnabled(n1));
    assertEquals(false, mbsnf.isNotificationEnabled(n2));
  }

  /**
   * Test getters
   */
  public void testGetters()
  {
    setUpTest();

    try
    {

      // By default Everything disabled
      assertEquals(0, mbsnf.getEnabledObjectNames().size());
      assertEquals(null, mbsnf.getDisabledObjectNames());

      // Enabled everything
      mbsnf.enableAllObjectNames();
      assertEquals(null, mbsnf.getEnabledObjectNames());
      assertEquals(0, mbsnf.getDisabledObjectNames().size());

      // Disable one
      mbsnf.disableObjectName(on1);
      assertEquals(null, mbsnf.getEnabledObjectNames());
      assertEquals(1, mbsnf.getDisabledObjectNames().size());
      assertEquals(on1, mbsnf.getDisabledObjectNames().elementAt(0));

      // Disable everything
      mbsnf.disableAllObjectNames();
      assertEquals(0, mbsnf.getEnabledObjectNames().size());
      assertEquals(null, mbsnf.getDisabledObjectNames());

      // Enable one
      mbsnf.enableObjectName(on1);
      assertEquals(1, mbsnf.getEnabledObjectNames().size());
      assertEquals(null, mbsnf.getDisabledObjectNames());
      assertEquals(on1, mbsnf.getEnabledObjectNames().elementAt(0));
    }
    catch (NullPointerException e)
    {
      fail("FAILS IN RI: " + e.toString());
    }
  }

  /**
   * Test serialization.
   */
  public void testSerialization()
  {
    setUpTest();

    // Enable only one
    mbsnf.enableObjectName(on2);

    MBeanServerNotificationFilter mbsnf2 = null;
    try
    {
      // Serialize it
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(mbsnf);
    
      // Deserialize it
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new ObjectInputStream(bais);
      mbsnf2 = (MBeanServerNotificationFilter) ois.readObject();
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
    assertEquals(false, mbsnf.isNotificationEnabled(n1));
    assertEquals(true, mbsnf.isNotificationEnabled(n2));
  }

  // Support -------------------------------------------------------------------

  private void setUpTest()
  {
    mbsnf = new MBeanServerNotificationFilter();
    mbsnf.enableType(MBeanServerNotification.REGISTRATION_NOTIFICATION);
    try
    {
      on1 = new ObjectName(":a=a");
      on2 = new ObjectName(":b=b");
    }
    catch (Exception e)
    {
      fail(e.toString());
    }
    n1 = new MBeanServerNotification(MBeanServerNotification.REGISTRATION_NOTIFICATION,
                                     new Object(), 1, on1);
    n2 = new MBeanServerNotification(MBeanServerNotification.REGISTRATION_NOTIFICATION,
                                     new Object(), 2, on2);
  }
}
