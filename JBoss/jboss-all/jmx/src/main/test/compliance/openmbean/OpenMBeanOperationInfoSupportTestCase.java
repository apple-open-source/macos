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
import java.util.Set;

import javax.management.openmbean.OpenMBeanParameterInfo;
import javax.management.openmbean.OpenMBeanParameterInfoSupport;
import javax.management.openmbean.OpenMBeanOperationInfoSupport;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;

/**
 * Open MBean Operation Info tests.<p>
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class OpenMBeanOperationInfoSupportTestCase
  extends TestCase
{
   // Static --------------------------------------------------------------------

   // Attributes ----------------------------------------------------------------

   // Operation ---------------------------------------------------------------

   /**
    * Construct the test
    */
   public OpenMBeanOperationInfoSupportTestCase(String s)
   {
      super(s);
   }

   // Tests ---------------------------------------------------------------------

   public void testOpenMBeanOperationInfoSupport()
      throws Exception
   {
      OpenMBeanOperationInfoSupport info = new OpenMBeanOperationInfoSupport(
         "name", "description", null, SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO);
      assertEquals("name", info.getName());
      assertEquals("description", info.getDescription());
      assertEquals(0, info.getSignature().length);
      assertEquals("java.lang.String", info.getReturnType());
      assertEquals(SimpleType.STRING, info.getReturnOpenType());
      assertEquals(OpenMBeanOperationInfoSupport.ACTION_INFO, info.getImpact());

      info = new OpenMBeanOperationInfoSupport(
         "name", "description", new OpenMBeanParameterInfoSupport[0],
         SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO);
      assertEquals("name", info.getName());
      assertEquals("description", info.getDescription());
      assertEquals(0, info.getSignature().length);
      assertEquals("java.lang.String", info.getReturnType());
      assertEquals(SimpleType.STRING, info.getReturnOpenType());
      assertEquals(OpenMBeanOperationInfoSupport.ACTION_INFO, info.getImpact());

      OpenMBeanParameterInfoSupport[] parms = new OpenMBeanParameterInfoSupport[]
      {
         new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.STRING)
      };
      info = new OpenMBeanOperationInfoSupport(
         "name", "description", parms,
         SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO);
      assertEquals("name", info.getName());
      assertEquals("description", info.getDescription());
      assertEquals(1, info.getSignature().length);
      assertEquals("java.lang.String", info.getReturnType());
      assertEquals(SimpleType.STRING, info.getReturnOpenType());
      assertEquals(OpenMBeanOperationInfoSupport.ACTION_INFO, info.getImpact());
   }

   public void testReturnOpenType()
      throws Exception
   {
      OpenMBeanParameterInfoSupport[] parms = new OpenMBeanParameterInfoSupport[]
      {
         new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.STRING)
      };
      OpenMBeanOperationInfoSupport info = new OpenMBeanOperationInfoSupport(
         "name", "description", parms,
         SimpleType.BOOLEAN, OpenMBeanOperationInfoSupport.ACTION_INFO);

      assertEquals(SimpleType.BOOLEAN, info.getReturnOpenType());
   }

   public void testEquals()
      throws Exception
   {
      OpenMBeanOperationInfoSupport info = new OpenMBeanOperationInfoSupport(
         "name", "description", null, SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO);

      assertTrue("Null should not be equal", info.equals(null) == false);
      assertTrue("Only OpenMBeanOperationInfo should be equal", info.equals(new Object()) == false);

      OpenMBeanOperationInfoSupport info2 = new OpenMBeanOperationInfoSupport(
         "name", "description", null, SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO);

      assertTrue("Different instances of the same data are equal", info.equals(info2));
      assertTrue("Different instances of the same data are equal", info2.equals(info));

      info2 = new OpenMBeanOperationInfoSupport(
         "name", "description2", null, SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO);

      assertTrue("Different instances with different descriptions are equal", info.equals(info2));
      assertTrue("Different instances with different descritpions are equal", info2.equals(info));

      info2 = new OpenMBeanOperationInfoSupport(
         "name2", "description", null, SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO);

      assertTrue("Instances with different names are not equal", info.equals(info2) == false);
      assertTrue("Instances with different names are not equal", info2.equals(info) == false);

      OpenMBeanParameterInfoSupport param1 = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.STRING);
      OpenMBeanParameterInfoSupport param2 = new OpenMBeanParameterInfoSupport(
         "name2", "description", SimpleType.STRING);

      info = new OpenMBeanOperationInfoSupport(
         "name", "description", new OpenMBeanParameterInfoSupport[] { param1, param2 },
         SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO);
      info2 = new OpenMBeanOperationInfoSupport(
         "name", "description", new OpenMBeanParameterInfoSupport[] { param1, param2 },
         SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO);

      assertTrue("Different instances with the same parameters are equal", info.equals(info2));
      assertTrue("Different instances with the same parameters are equal", info2.equals(info));

      info = new OpenMBeanOperationInfoSupport(
         "name", "description", new OpenMBeanParameterInfoSupport[] { param1, param2 },
         SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO);
      info2 = new OpenMBeanOperationInfoSupport(
         "name", "description", new OpenMBeanParameterInfoSupport[] { param2, param1 },
         SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO);

      assertTrue("Different instances with the same signature but different parameters are not equal",
         info.equals(info2) == false);
      assertTrue("Different instances with the same signature but different parameters are not equal",
         info2.equals(info) == false);

      param2 = new OpenMBeanParameterInfoSupport(
         "name2", "description", SimpleType.INTEGER);
      info2 = new OpenMBeanOperationInfoSupport(
         "name", "description", new OpenMBeanParameterInfoSupport[] { param1, param2 },
         SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO);

      assertTrue("Different instances with different signatures are not equal",
         info.equals(info2) == false);
      assertTrue("Different instances with different signatures are not equal",
         info2.equals(info) == false);

      info2 = new OpenMBeanOperationInfoSupport(
         "name", "description", new OpenMBeanParameterInfoSupport[] { param1 },
         SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO);

      assertTrue("Different instances with different numbers of paramters are not equal",
         info.equals(info2) == false);
      assertTrue("Different instances with different numbers of parameters are not equal",
         info2.equals(info) == false);

      info2 = new OpenMBeanOperationInfoSupport(
         "name", "description", new OpenMBeanParameterInfoSupport[] { param1, param2 },
         SimpleType.INTEGER, OpenMBeanOperationInfoSupport.ACTION_INFO);

      assertTrue("Different instances with different return types are not equal",
         info.equals(info2) == false);
      assertTrue("Different instances with different return types are not equal",
         info2.equals(info) == false);

      info2 = new OpenMBeanOperationInfoSupport(
         "name", "description", new OpenMBeanParameterInfoSupport[] { param1, param2 },
         SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION);

      assertTrue("Different instances with different impacts are not equal",
         info.equals(info2) == false);
      assertTrue("Different instances with different impacts are not equal",
         info2.equals(info) == false);
   }

   public void testHashCode()
      throws Exception
   {
      OpenMBeanParameterInfoSupport[] parms = new OpenMBeanParameterInfoSupport[]
      {
         new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.STRING)
      };
      OpenMBeanOperationInfoSupport info = new OpenMBeanOperationInfoSupport(
         "name", "description", parms,
         SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO);

      int myHash = "name".hashCode() + Arrays.asList(parms).hashCode()
                   + SimpleType.STRING.hashCode() + OpenMBeanOperationInfoSupport.ACTION_INFO;
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
      OpenMBeanOperationInfoSupport info = new OpenMBeanOperationInfoSupport(
         "NAME", "DESCRIPTION", parms,
         SimpleType.INTEGER, OpenMBeanOperationInfoSupport.ACTION_INFO);

      String toString = info.toString();

      assertTrue("info.toString() should contain NAME",
         toString.indexOf("NAME") != -1);
      assertTrue("info.toString() should contain the parameters",
         toString.indexOf(Arrays.asList(parms).toString()) != -1);
      assertTrue("info.toString() should contain the simple type",
         toString.indexOf(SimpleType.INTEGER.toString()) != -1);
   }

   public void testSerialization()
      throws Exception
   {
      OpenMBeanParameterInfoSupport[] parms = new OpenMBeanParameterInfoSupport[]
      {
         new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.STRING)
      };
      OpenMBeanOperationInfoSupport info = new OpenMBeanOperationInfoSupport(
         "name", "description", parms,
         SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO);

      // Serialize it
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(info);
    
      // Deserialize it
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new ObjectInputStream(bais);
      Object result = ois.readObject();

      assertEquals(info, result);
   }

   public void testErrors()
      throws Exception
   {
      boolean caught = false;
      try
      {
         OpenMBeanParameterInfoSupport[] parms = new OpenMBeanParameterInfoSupport[]
         {
            new OpenMBeanParameterInfoSupport(
               "name", "description", SimpleType.STRING)
         };
         OpenMBeanOperationInfoSupport info = new OpenMBeanOperationInfoSupport(
            null, "description", parms,
            SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for null name");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport[] parms = new OpenMBeanParameterInfoSupport[]
         {
            new OpenMBeanParameterInfoSupport(
               "name", "description", SimpleType.STRING)
         };
         OpenMBeanOperationInfoSupport info = new OpenMBeanOperationInfoSupport(
            "", "description", parms,
            SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for empty name");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport[] parms = new OpenMBeanParameterInfoSupport[]
         {
            new OpenMBeanParameterInfoSupport(
               "name", "description", SimpleType.STRING)
         };
         OpenMBeanOperationInfoSupport info = new OpenMBeanOperationInfoSupport(
            "name", null, parms,
            SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for null description");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport[] parms = new OpenMBeanParameterInfoSupport[]
         {
            new OpenMBeanParameterInfoSupport(
               "name", "description", SimpleType.STRING)
         };
         OpenMBeanOperationInfoSupport info = new OpenMBeanOperationInfoSupport(
            "name", "", parms,
            SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for empty description");

      caught = false;
      try
      {
         OpenMBeanOperationInfoSupport info = new OpenMBeanOperationInfoSupport(
            "name", "description", new MyOpenMBeanParameterInfo[] { new MyOpenMBeanParameterInfo() },
            SimpleType.STRING, OpenMBeanOperationInfoSupport.ACTION_INFO);
      }
      catch (ArrayStoreException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected ArrayStoreException for non MBeanParameterInfo array");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport[] parms = new OpenMBeanParameterInfoSupport[]
         {
            new OpenMBeanParameterInfoSupport(
               "name", "description", SimpleType.STRING)
         };
         OpenMBeanOperationInfoSupport info = new OpenMBeanOperationInfoSupport(
            "name", "description", parms,
            null, OpenMBeanOperationInfoSupport.ACTION_INFO);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for null return type");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport[] parms = new OpenMBeanParameterInfoSupport[]
         {
            new OpenMBeanParameterInfoSupport(
               "name", "description", SimpleType.STRING)
         };
         OpenMBeanOperationInfoSupport info = new OpenMBeanOperationInfoSupport(
            "name", "description", parms,
            SimpleType.VOID, OpenMBeanOperationInfoSupport.ACTION_INFO);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == true)
         fail("Didn't expect IllegalArgumentException for VOID return type");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport[] parms = new OpenMBeanParameterInfoSupport[]
         {
            new OpenMBeanParameterInfoSupport(
               "name", "description", SimpleType.STRING)
         };
         OpenMBeanOperationInfoSupport info = new OpenMBeanOperationInfoSupport(
            "name", "description", parms,
            SimpleType.STRING, 1234567);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentExecption for invalid action");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport[] parms = new OpenMBeanParameterInfoSupport[]
         {
            new OpenMBeanParameterInfoSupport(
               "name", "description", SimpleType.STRING)
         };
         OpenMBeanOperationInfoSupport info = new OpenMBeanOperationInfoSupport(
            "name", "description", parms,
            SimpleType.STRING, OpenMBeanOperationInfoSupport.UNKNOWN);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentExecption for UNKNOWN action");
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
}
