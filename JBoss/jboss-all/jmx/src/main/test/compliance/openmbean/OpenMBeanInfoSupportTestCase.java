/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.openmbean;

import junit.framework.TestCase;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

import javax.management.MBeanParameterInfo;
import javax.management.MBeanNotificationInfo;
import javax.management.openmbean.OpenMBeanAttributeInfo;
import javax.management.openmbean.OpenMBeanAttributeInfoSupport;
import javax.management.openmbean.OpenMBeanConstructorInfo;
import javax.management.openmbean.OpenMBeanConstructorInfoSupport;
import javax.management.openmbean.OpenMBeanInfoSupport;
import javax.management.openmbean.OpenMBeanParameterInfo;
import javax.management.openmbean.OpenMBeanParameterInfoSupport;
import javax.management.openmbean.OpenMBeanOperationInfo;
import javax.management.openmbean.OpenMBeanOperationInfoSupport;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;

/**
 * Open MBean Info tests.<p>
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class OpenMBeanInfoSupportTestCase
  extends TestCase
{
   // Static --------------------------------------------------------------------

   // Attributes ----------------------------------------------------------------

   // Constructor ---------------------------------------------------------------

   /**
    * Construct the test
    */
   public OpenMBeanInfoSupportTestCase(String s)
   {
      super(s);
   }

   // Tests ---------------------------------------------------------------------

   public void testOpenMBeanInfoSupport()
      throws Exception
   {
      OpenMBeanInfoSupport info = new OpenMBeanInfoSupport(
         "name", "description", null, null, null, null);
      assertEquals("name", info.getClassName());
      assertEquals("description", info.getDescription());
      assertEquals(0, info.getAttributes().length);
      assertEquals(0, info.getConstructors().length);
      assertEquals(0, info.getNotifications().length);
      assertEquals(0, info.getOperations().length);

      info = new OpenMBeanInfoSupport(
         "name", "description", new OpenMBeanAttributeInfoSupport[0], new OpenMBeanConstructorInfoSupport[0],
         new OpenMBeanOperationInfoSupport[0], new MBeanNotificationInfo[0]);
      assertEquals("name", info.getClassName());
      assertEquals("description", info.getDescription());
      assertEquals(0, info.getAttributes().length);
      assertEquals(0, info.getConstructors().length);
      assertEquals(0, info.getNotifications().length);
      assertEquals(0, info.getOperations().length);

      OpenMBeanParameterInfoSupport[] parms = new OpenMBeanParameterInfoSupport[]
      {
         new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.STRING)
      };

      OpenMBeanAttributeInfoSupport[] attributes = new OpenMBeanAttributeInfoSupport[]
      {
         new OpenMBeanAttributeInfoSupport(
         "name", "description", SimpleType.STRING, true, true, false)
      };
      OpenMBeanConstructorInfoSupport[] constructors = new OpenMBeanConstructorInfoSupport[]
      {
         new OpenMBeanConstructorInfoSupport(
         "name", "description", parms)
      };
      OpenMBeanOperationInfoSupport[] operations = new OpenMBeanOperationInfoSupport[]
      {
         new OpenMBeanOperationInfoSupport(
         "name", "description", parms,
         SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO)
      };
      MBeanNotificationInfo[] notifications = new MBeanNotificationInfo[]
      {
         new MBeanNotificationInfo(new String[] { "type1", "type" }, "name", "description")
      };
      info = new OpenMBeanInfoSupport(
         "name", "description", attributes, constructors,
         operations, notifications);
      assertEquals("name", info.getClassName());
      assertEquals("description", info.getDescription());
      assertEquals(1, info.getAttributes().length);
      assertEquals(1, info.getConstructors().length);
      assertEquals(1, info.getNotifications().length);
      assertEquals(1, info.getOperations().length);
   }

   public void testEquals()
      throws Exception
   {
      OpenMBeanParameterInfoSupport[] parms = new OpenMBeanParameterInfoSupport[]
      {
         new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.STRING)
      };

      OpenMBeanAttributeInfoSupport[] attributes = new OpenMBeanAttributeInfoSupport[]
      {
         new OpenMBeanAttributeInfoSupport(
         "name", "description", SimpleType.STRING, true, true, false)
      };
      OpenMBeanConstructorInfoSupport[] constructors = new OpenMBeanConstructorInfoSupport[]
      {
         new OpenMBeanConstructorInfoSupport(
         "name", "description", parms)
      };
      OpenMBeanOperationInfoSupport[] operations = new OpenMBeanOperationInfoSupport[]
      {
         new OpenMBeanOperationInfoSupport(
         "name", "description", parms,
         SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO)
      };
      MBeanNotificationInfo[] notifications = new MBeanNotificationInfo[]
      {
         new MBeanNotificationInfo(new String[] { "type1", "type" }, "name", "description")
      };
      OpenMBeanInfoSupport info = new OpenMBeanInfoSupport(
         "name", "description", attributes, constructors,
         operations, notifications);

      assertTrue("Null is not equal to any instance", info.equals(null) == false);
      assertTrue("Instance is only equal another OpenMBeanInfo instance", info.equals(new Object()) == false);
      assertTrue("Instance should equal itself", info.equals(info));

      OpenMBeanInfoSupport info2 = new OpenMBeanInfoSupport(
         "name", "description", attributes, constructors,
         operations, notifications);
      assertTrue("Instances with same values should be equal", info.equals(info2));
      assertTrue("Instances with same values should be equal", info2.equals(info));

      info2 = new OpenMBeanInfoSupport(
         "name2", "description", attributes, constructors,
         operations, notifications);
      assertTrue("Instances with different class names are not equal", info.equals(info2) == false);
      assertTrue("Instances with different class names are not equal", info2.equals(info) == false);

      info2 = new OpenMBeanInfoSupport(
         "name", "description2", attributes, constructors,
         operations, notifications);
      assertTrue("Instances with different descriptions are equal", info.equals(info2));
      assertTrue("Instances with different descriptions are equal", info2.equals(info));

      OpenMBeanAttributeInfoSupport[] attributes2 = new OpenMBeanAttributeInfoSupport[]
      {
         new OpenMBeanAttributeInfoSupport(
         "name2", "description", SimpleType.STRING, true, true, false)
      };

      info2 = new OpenMBeanInfoSupport(
         "name", "description", attributes2, constructors,
         operations, notifications);
      assertTrue("Instances with different attributes are not equal", info.equals(info2) == false);
      assertTrue("Instances with different attributes are not equal", info2.equals(info) == false);

      attributes2 = new OpenMBeanAttributeInfoSupport[]
      {
         new OpenMBeanAttributeInfoSupport(
         "name2", "description", SimpleType.STRING, true, true, false),
         new OpenMBeanAttributeInfoSupport(
         "name3", "description", SimpleType.STRING, true, true, false)
      };

      info2 = new OpenMBeanInfoSupport(
         "name", "description", attributes2, constructors,
         operations, notifications);
      assertTrue("Instances with different numbers of attributes are not equal", info.equals(info2) == false);
      assertTrue("Instances with different numbers of attributes are not equal", info2.equals(info) == false);

      info2 = new OpenMBeanInfoSupport(
         "name", "description", null, constructors,
         operations, notifications);
      assertTrue("Instances with and without attributes are not equal", info.equals(info2) == false);
      assertTrue("Instances with and without attributes are not equal", info2.equals(info) == false);

      OpenMBeanConstructorInfoSupport[] constructors2 = new OpenMBeanConstructorInfoSupport[]
      {
         new OpenMBeanConstructorInfoSupport(
         "name2", "description", parms)
      };

      info2 = new OpenMBeanInfoSupport(
         "name", "description", attributes, constructors2,
         operations, notifications);
      assertTrue("Instances with different constructors are not equal", info.equals(info2) == false);
      assertTrue("Instances with different constructors are not equal", info2.equals(info) == false);

      constructors2 = new OpenMBeanConstructorInfoSupport[]
      {
         new OpenMBeanConstructorInfoSupport(
         "name2", "description", parms),
         new OpenMBeanConstructorInfoSupport(
         "name3", "description", parms)
      };

      info2 = new OpenMBeanInfoSupport(
         "name", "description", attributes, constructors2,
         operations, notifications);
      assertTrue("Instances with different numbers of constructors are not equal", info.equals(info2) == false);
      assertTrue("Instances with different numbers of constructors are not equal", info2.equals(info) == false);

      info2 = new OpenMBeanInfoSupport(
         "name", "description", attributes, null,
         operations, notifications);
      assertTrue("Instances with and without constructors are not equal", info.equals(info2) == false);
      assertTrue("Instances with and without constructors are not equal", info2.equals(info) == false);

      OpenMBeanOperationInfoSupport[] operations2 = new OpenMBeanOperationInfoSupport[]
      {
         new OpenMBeanOperationInfoSupport(
         "name2", "description", parms,
         SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO)
      };

      info2 = new OpenMBeanInfoSupport(
         "name", "description", attributes, constructors,
         operations2, notifications);
      assertTrue("Instances with different operations are not equal", info.equals(info2) == false);
      assertTrue("Instances with different operations are not equal", info2.equals(info) == false);

      operations2 = new OpenMBeanOperationInfoSupport[]
      {
         new OpenMBeanOperationInfoSupport(
         "name2", "description", parms,
         SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO),
         new OpenMBeanOperationInfoSupport(
         "name3", "description", parms,
         SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO)
      };

      info2 = new OpenMBeanInfoSupport(
         "name", "description", attributes, constructors,
         operations2, notifications);
      assertTrue("Instances with different numbers of operations are not equal", info.equals(info2) == false);
      assertTrue("Instances with different numbers of operations are not equal", info2.equals(info) == false);

      info2 = new OpenMBeanInfoSupport(
         "name", "description", attributes, constructors,
         null, notifications);
      assertTrue("Instances with and without operations are not equal", info.equals(info2) == false);
      assertTrue("Instances with and without operations are not equal", info2.equals(info) == false);

      MBeanNotificationInfo[] notifications2 = new MBeanNotificationInfo[]
      {
         new MBeanNotificationInfo(new String[] { "type", "type" }, "name2", "description")
      };

      info2 = new OpenMBeanInfoSupport(
         "name", "description", attributes, constructors,
         operations, notifications2);
      assertTrue("Instances with different notifications are not equal", info.equals(info2) == false);
      assertTrue("Instances with different notifications are not equal", info2.equals(info) == false);

      notifications2 = new MBeanNotificationInfo[]
      {
         new MBeanNotificationInfo(new String[] { "type", "type" }, "name2", "description"),
         new MBeanNotificationInfo(new String[] { "type", "type" }, "name3", "description")
      };

      info2 = new OpenMBeanInfoSupport(
         "name", "description", attributes, constructors,
         operations, notifications2);
      assertTrue("Instances with different numbers of notifications are not equal", info.equals(info2) == false);
      assertTrue("Instances with different numbers of notifications are not equal", info2.equals(info) == false);

      info2 = new OpenMBeanInfoSupport(
         "name", "description", attributes, constructors,
         operations, null);
      assertTrue("Instances with and without notifications are not equal", info.equals(info2) == false);
      assertTrue("Instances with and without notifications are not equal", info2.equals(info) == false);
   }

   public void testHashCode()
      throws Exception
   {
      OpenMBeanParameterInfoSupport[] parms = new OpenMBeanParameterInfoSupport[]
      {
         new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.STRING)
      };

      OpenMBeanAttributeInfoSupport[] attributes = new OpenMBeanAttributeInfoSupport[]
      {
         new OpenMBeanAttributeInfoSupport(
         "name", "description", SimpleType.STRING, true, true, false)
      };
      OpenMBeanConstructorInfoSupport[] constructors = new OpenMBeanConstructorInfoSupport[]
      {
         new OpenMBeanConstructorInfoSupport(
         "name", "description", parms)
      };
      OpenMBeanOperationInfoSupport[] operations = new OpenMBeanOperationInfoSupport[]
      {
         new OpenMBeanOperationInfoSupport(
         "name", "description", parms,
         SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO)
      };
      MBeanNotificationInfo[] notifications = new MBeanNotificationInfo[]
      {
         new MBeanNotificationInfo(
         new String[] { "type1", "type" }, "name", "description")
      };
      OpenMBeanInfoSupport info = new OpenMBeanInfoSupport(
         "name", "description", attributes, constructors,
         operations, notifications);

      int myHash = "name".hashCode() +
                   new HashSet(Arrays.asList(attributes)).hashCode() +
                   new HashSet(Arrays.asList(constructors)).hashCode() +
                   new HashSet(Arrays.asList(operations)).hashCode() +
                   new HashSet(Arrays.asList(notifications)).hashCode();
      assertEquals(myHash, info.hashCode());
   }

   public void testToString()
      throws Exception
   {
      OpenMBeanParameterInfoSupport[] parms = new OpenMBeanParameterInfoSupport[]
      {
         new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.STRING)
      };

      OpenMBeanAttributeInfoSupport[] attributes = new OpenMBeanAttributeInfoSupport[]
      {
         new OpenMBeanAttributeInfoSupport(
         "name", "description", SimpleType.STRING, true, true, false)
      };
      OpenMBeanConstructorInfoSupport[] constructors = new OpenMBeanConstructorInfoSupport[]
      {
         new OpenMBeanConstructorInfoSupport(
         "name", "description", parms)
      };
      OpenMBeanOperationInfoSupport[] operations = new OpenMBeanOperationInfoSupport[]
      {
         new OpenMBeanOperationInfoSupport(
         "name", "description", parms,
         SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO)
      };
      MBeanNotificationInfo[] notifications = new MBeanNotificationInfo[]
      {
         new MBeanNotificationInfo(
         new String[] { "type1", "type" }, "name", "description")
      };
      OpenMBeanInfoSupport info = new OpenMBeanInfoSupport(
         "NAME", "DESCRIPTION", attributes, constructors,
         operations, notifications);

      String toString = info.toString();

      assertTrue("info.toString() should contain the name",
         toString.indexOf("NAME") != -1);
      assertTrue("info.toString() should contain the attributes",
         toString.indexOf(Arrays.asList(attributes).toString()) != -1);
      assertTrue("info.toString() should contain the constructors",
         toString.indexOf(Arrays.asList(constructors).toString()) != -1);
      assertTrue("info.toString() should contain the operations",
         toString.indexOf(Arrays.asList(operations).toString()) != -1);
      assertTrue("info.toString() should contain the notifications",
         toString.indexOf(Arrays.asList(notifications).toString()) != -1);
   }

   public void testSerialization()
      throws Exception
   {
      OpenMBeanParameterInfoSupport[] parms = new OpenMBeanParameterInfoSupport[]
      {
         new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.STRING)
      };

      OpenMBeanAttributeInfoSupport[] attributes = new OpenMBeanAttributeInfoSupport[]
      {
         new OpenMBeanAttributeInfoSupport(
         "name", "description", SimpleType.STRING, true, true, false)
      };
      OpenMBeanConstructorInfoSupport[] constructors = new OpenMBeanConstructorInfoSupport[]
      {
         new OpenMBeanConstructorInfoSupport(
         "name", "description", parms)
      };
      OpenMBeanOperationInfoSupport[] operations = new OpenMBeanOperationInfoSupport[]
      {
         new OpenMBeanOperationInfoSupport(
         "name", "description", parms,
         SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO)
      };
      MBeanNotificationInfo[] notifications = new MBeanNotificationInfo[]
      {
         new MBeanNotificationInfo(
         new String[] { "type1", "type" }, "name", "description")
      };
      OpenMBeanInfoSupport info = new OpenMBeanInfoSupport(
         "name", "description", attributes, constructors,
         operations, notifications);
      // Serialize it
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(info);
    
      // Deserialize it
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new ObjectInputStream(bais);
      OpenMBeanInfoSupport result = (OpenMBeanInfoSupport) ois.readObject();

      assertEquals(info.getClassName(), result.getClassName());
      assertEquals(Arrays.asList(info.getAttributes()), Arrays.asList(result.getAttributes()));
      assertEquals(Arrays.asList(info.getConstructors()), Arrays.asList(result.getConstructors()));
      assertEquals(Arrays.asList(info.getOperations()), Arrays.asList(result.getOperations()));

      // UGLY!
      MBeanNotificationInfo origNotification = info.getNotifications()[0];
      MBeanNotificationInfo resultNotification = result.getNotifications()[0];
      assertEquals(origNotification.getName(), resultNotification.getName());
      assertEquals(origNotification.getDescription(), resultNotification.getDescription());
      assertEquals(Arrays.asList(origNotification.getNotifTypes()), Arrays.asList(resultNotification.getNotifTypes()));
   }

   public void testErrors()
      throws Exception
   {
      boolean caught = false;
      try
      {
         MyOpenMBeanAttributeInfoSupport[] infos = new MyOpenMBeanAttributeInfoSupport[]
         {
            new MyOpenMBeanAttributeInfoSupport()
         };
         OpenMBeanInfoSupport info = new OpenMBeanInfoSupport(
            "className", "description", infos, null, null, null);
      }
      catch (ArrayStoreException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected ArrayStoreException for attributes not MBeanAttributeInfo");

      caught = false;
      try
      {
         MyOpenMBeanConstructorInfoSupport[] infos = new MyOpenMBeanConstructorInfoSupport[]
         {
            new MyOpenMBeanConstructorInfoSupport()
         };
         OpenMBeanInfoSupport info = new OpenMBeanInfoSupport(
            "className", "description", null, infos, null, null);
      }
      catch (ArrayStoreException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected ArrayStoreException for attributes not MBeanConstructorInfo");

      caught = false;
      try
      {
         MyOpenMBeanOperationInfoSupport[] infos = new MyOpenMBeanOperationInfoSupport[]
         {
            new MyOpenMBeanOperationInfoSupport()
         };
         OpenMBeanInfoSupport info = new OpenMBeanInfoSupport(
            "className", "description", null, null, infos, null);
      }
      catch (ArrayStoreException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected ArrayStoreException for attributes not MBeanOperationInfo");
   }

   public static class MyOpenMBeanParameterInfo
      implements OpenMBeanParameterInfo
   {
      public boolean equals(Object o) { return false; }
      public Object getDefaultValue() { return null; }
      public String getDescription() { return null; }
      public Set getLegalValues() { return null; }
      public Comparable getMaxValue() { return null; }
      public Comparable getMinValue() { return null; }
      public String getName() { return null; }
      public OpenType getOpenType() { return null; }
      public boolean hasDefaultValue() { return false; }
      public boolean hasLegalValues() { return false; }
      public int hashCode() { return 0; }
      public boolean hasMaxValue() { return false; }
      public boolean hasMinValue() { return false; }
      public boolean isValue(Object o) { return false; }
      public String toString() { return null; }
   }

   public static class MyOpenMBeanAttributeInfoSupport
      extends MyOpenMBeanParameterInfo
      implements OpenMBeanAttributeInfo
   {
      public boolean isIs() { return false; }
      public boolean isReadable() { return false; }
      public boolean isWritable() { return false; }
   }

   public static class MyOpenMBeanConstructorInfoSupport
      implements OpenMBeanConstructorInfo
   {
      public boolean equals(Object o) { return false; }
      public String getDescription() { return null; }
      public String getName() { return null; }
      public MBeanParameterInfo[] getSignature() { return null; }
      public int hashCode() { return 0; }
      public String toString() { return null; }
   }

   public static class MyOpenMBeanOperationInfoSupport
      implements OpenMBeanOperationInfo
   {
      public boolean equals(Object o) { return false; }
      public String getDescription() { return null; }
      public int getImpact() { return 0; }
      public String getName() { return null; }
      public OpenType getReturnOpenType() { return null; }
      public String getReturnType() { return null; }
      public MBeanParameterInfo[] getSignature() { return null; }
      public int hashCode() { return 0; }
      public String toString() { return null; }
   }
}
