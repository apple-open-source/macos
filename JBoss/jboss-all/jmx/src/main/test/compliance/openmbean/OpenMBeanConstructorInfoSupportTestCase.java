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
import javax.management.openmbean.OpenMBeanConstructorInfoSupport;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;

/**
 * Open MBean Constructor Info tests.<p>
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class OpenMBeanConstructorInfoSupportTestCase
  extends TestCase
{
   // Static --------------------------------------------------------------------

   // Attributes ----------------------------------------------------------------

   // Constructor ---------------------------------------------------------------

   /**
    * Construct the test
    */
   public OpenMBeanConstructorInfoSupportTestCase(String s)
   {
      super(s);
   }

   // Tests ---------------------------------------------------------------------

   public void testOpenMBeanConstructorInfoSupport()
      throws Exception
   {
      OpenMBeanConstructorInfoSupport info = new OpenMBeanConstructorInfoSupport(
         "name", "description", null);
      assertEquals("name", info.getName());
      assertEquals("description", info.getDescription());
      assertEquals(0, info.getSignature().length);

      info = new OpenMBeanConstructorInfoSupport(
         "name", "description", new OpenMBeanParameterInfoSupport[0]);
      assertEquals("name", info.getName());
      assertEquals("description", info.getDescription());
      assertEquals(0, info.getSignature().length);

      OpenMBeanParameterInfoSupport[] parms = new OpenMBeanParameterInfoSupport[]
      {
         new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.STRING)
      };
      info = new OpenMBeanConstructorInfoSupport(
         "name", "description", parms);
      assertEquals("name", info.getName());
      assertEquals("description", info.getDescription());
      assertEquals(1, info.getSignature().length);
   }

   public void testEquals()
      throws Exception
   {
      OpenMBeanConstructorInfoSupport info = new OpenMBeanConstructorInfoSupport(
         "name", "description", null);

      assertTrue("Null should not be equal", info.equals(null) == false);
      assertTrue("Only OpenMBeanConstructorInfo should be equal", info.equals(new Object()) == false);

      OpenMBeanConstructorInfoSupport info2 = new OpenMBeanConstructorInfoSupport(
         "name", "description", null);

      assertTrue("Different instances of the same data are equal", info.equals(info2));
      assertTrue("Different instances of the same data are equal", info2.equals(info));

      info2 = new OpenMBeanConstructorInfoSupport(
         "name", "description2", null);

      assertTrue("Different instances with different descriptions are equal", info.equals(info2));
      assertTrue("Different instances with different descritpions are equal", info2.equals(info));

      info2 = new OpenMBeanConstructorInfoSupport(
         "name2", "description", null);

      assertTrue("Instances with different names are not equal", info.equals(info2) == false);
      assertTrue("Instances with different names are not equal", info2.equals(info) == false);

      OpenMBeanParameterInfoSupport param1 = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.STRING);
      OpenMBeanParameterInfoSupport param2 = new OpenMBeanParameterInfoSupport(
         "name2", "description", SimpleType.STRING);

      info = new OpenMBeanConstructorInfoSupport(
         "name", "description", new OpenMBeanParameterInfoSupport[] { param1, param2 });
      info2 = new OpenMBeanConstructorInfoSupport(
         "name", "description", new OpenMBeanParameterInfoSupport[] { param1, param2 });

      assertTrue("Different instances with the same parameters are equal", info.equals(info2));
      assertTrue("Different instances with the same parameters are equal", info2.equals(info));

      info = new OpenMBeanConstructorInfoSupport(
         "name", "description", new OpenMBeanParameterInfoSupport[] { param1, param2 });
      info2 = new OpenMBeanConstructorInfoSupport(
         "name", "description", new OpenMBeanParameterInfoSupport[] { param2, param1 });

      assertTrue("Different instances with the same signature but different parameters are not equal",
         info.equals(info2) == false);
      assertTrue("Different instances with the same signature but different parameters are not equal",
         info2.equals(info) == false);

      param2 = new OpenMBeanParameterInfoSupport(
         "name2", "description", SimpleType.INTEGER);
      info2 = new OpenMBeanConstructorInfoSupport(
         "name", "description", new OpenMBeanParameterInfoSupport[] { param1, param2 });

      assertTrue("Different instances with different signatures are not equal",
         info.equals(info2) == false);
      assertTrue("Different instances with different signatures are not equal",
         info2.equals(info) == false);

      info2 = new OpenMBeanConstructorInfoSupport(
         "name", "description", new OpenMBeanParameterInfoSupport[] { param1 });

      assertTrue("Different instances with different numbers of paramters are not equal",
         info.equals(info2) == false);
      assertTrue("Different instances with different numbers of parameters are not equal",
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
      OpenMBeanConstructorInfoSupport info = new OpenMBeanConstructorInfoSupport(
         "name", "description", parms);

      int myHash = "name".hashCode() + Arrays.asList(parms).hashCode();
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
      OpenMBeanConstructorInfoSupport info = new OpenMBeanConstructorInfoSupport(
         "NAME", "DESCRIPTION", parms);

      String toString = info.toString();

      assertTrue("info.toString() should contain NAME",
         toString.indexOf("NAME") != -1);
      assertTrue("info.toString() should contain the parameters",
         toString.indexOf(Arrays.asList(parms).toString()) != -1);
   }

   public void testSerialization()
      throws Exception
   {
      OpenMBeanParameterInfoSupport[] parms = new OpenMBeanParameterInfoSupport[]
      {
         new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.STRING)
      };
      OpenMBeanConstructorInfoSupport info = new OpenMBeanConstructorInfoSupport(
         "name", "description", parms);

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
         OpenMBeanConstructorInfoSupport info = new OpenMBeanConstructorInfoSupport(
            null, "description", parms);
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
         OpenMBeanConstructorInfoSupport info = new OpenMBeanConstructorInfoSupport(
            "", "description", parms);
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
         OpenMBeanConstructorInfoSupport info = new OpenMBeanConstructorInfoSupport(
            "name", null, parms);
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
         OpenMBeanConstructorInfoSupport info = new OpenMBeanConstructorInfoSupport(
            "name", "", parms);
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
         OpenMBeanConstructorInfoSupport info = new OpenMBeanConstructorInfoSupport(
            "name", "description", new MyOpenMBeanParameterInfo[] { new MyOpenMBeanParameterInfo() });
      }
      catch (ArrayStoreException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected ArrayStoreException for non MBeanParameterInfo array");
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
