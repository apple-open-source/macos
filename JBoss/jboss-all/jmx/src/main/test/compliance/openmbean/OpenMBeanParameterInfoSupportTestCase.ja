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

import javax.management.openmbean.ArrayType;
import javax.management.openmbean.CompositeType;
import javax.management.openmbean.OpenDataException;
import javax.management.openmbean.OpenMBeanParameterInfoSupport;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;
import javax.management.openmbean.TabularDataSupport;
import javax.management.openmbean.TabularType;

/**
 * Open MBean Parameter Info tests.<p>
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class OpenMBeanParameterInfoSupportTestCase
  extends TestCase
{
   // Static --------------------------------------------------------------------

   // Attributes ----------------------------------------------------------------

   // Constructor ---------------------------------------------------------------

   /**
    * Construct the test
    */
   public OpenMBeanParameterInfoSupportTestCase(String s)
   {
      super(s);
   }

   // Tests ---------------------------------------------------------------------

   public void testOpenMBeanParameterInfoSupport()
      throws Exception
   {
      OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.STRING);
      assertEquals("name", info.getName());
      assertEquals("description", info.getDescription());
      assertEquals("java.lang.String", info.getType());

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.STRING, "default");
      assertEquals("name", info.getName());
      assertEquals("description", info.getDescription());
      assertEquals("java.lang.String", info.getType());
      assertEquals("default", info.getDefaultValue());
      assertEquals(true, info.hasDefaultValue());

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.STRING, null);
      assertEquals("name", info.getName());
      assertEquals("description", info.getDescription());
      assertEquals("java.lang.String", info.getType());
      assertEquals(null, info.getDefaultValue());
      assertEquals(false, info.hasDefaultValue());

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(3), new Integer(2), new Integer(4));
      assertEquals("name", info.getName());
      assertEquals("description", info.getDescription());
      assertEquals("java.lang.Integer", info.getType());
      assertEquals(new Integer(3), info.getDefaultValue());
      assertEquals(new Integer(2), info.getMinValue());
      assertEquals(new Integer(4), info.getMaxValue());

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, null, new Integer(2), new Integer(4));
      assertEquals("name", info.getName());
      assertEquals("description", info.getDescription());
      assertEquals("java.lang.Integer", info.getType());
      assertEquals(null, info.getDefaultValue());
      assertEquals(new Integer(2), info.getMinValue());
      assertEquals(new Integer(4), info.getMaxValue());

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(3), null, new Integer(4));
      assertEquals("name", info.getName());
      assertEquals("description", info.getDescription());
      assertEquals("java.lang.Integer", info.getType());
      assertEquals(new Integer(3), info.getDefaultValue());
      assertEquals(null, info.getMinValue());
      assertEquals(new Integer(4), info.getMaxValue());

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(3), new Integer(2), null);
      assertEquals("name", info.getName());
      assertEquals("description", info.getDescription());
      assertEquals("java.lang.Integer", info.getType());
      assertEquals(new Integer(3), info.getDefaultValue());
      assertEquals(new Integer(2), info.getMinValue());
      assertEquals(null, info.getMaxValue());

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, null, null, new Integer(4));
      assertEquals("name", info.getName());
      assertEquals("description", info.getDescription());
      assertEquals("java.lang.Integer", info.getType());
      assertEquals(null, info.getDefaultValue());
      assertEquals(null, info.getMinValue());
      assertEquals(new Integer(4), info.getMaxValue());

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, null, new Integer(2), null);
      assertEquals("name", info.getName());
      assertEquals("description", info.getDescription());
      assertEquals("java.lang.Integer", info.getType());
      assertEquals(null, info.getDefaultValue());
      assertEquals(new Integer(2), info.getMinValue());
      assertEquals(null, info.getMaxValue());

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(3), null, null);
      assertEquals("name", info.getName());
      assertEquals("description", info.getDescription());
      assertEquals("java.lang.Integer", info.getType());
      assertEquals(new Integer(3), info.getDefaultValue());
      assertEquals(null, info.getMinValue());
      assertEquals(null, info.getMaxValue());

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, null, null, null);
      assertEquals("name", info.getName());
      assertEquals("description", info.getDescription());
      assertEquals("java.lang.Integer", info.getType());
      assertEquals(null, info.getDefaultValue());
      assertEquals(null, info.getMinValue());
      assertEquals(null, info.getMaxValue());

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.STRING, "default",
         new Object[] { "legal1", "legal2", "default" });
      assertEquals("name", info.getName());
      assertEquals("description", info.getDescription());
      assertEquals("java.lang.String", info.getType());
      assertEquals("default", info.getDefaultValue());
      assertEquals(3, info.getLegalValues().size());
      assertTrue("legal1 should be a legal value", info.getLegalValues().contains("legal1"));
      assertTrue("legal2 should be a legal value", info.getLegalValues().contains("legal2"));
      assertTrue("default should be a legal value", info.getLegalValues().contains("default"));

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.STRING, null,
         new Object[] { "legal1", "legal2", "default" });
      assertEquals("name", info.getName());
      assertEquals("description", info.getDescription());
      assertEquals("java.lang.String", info.getType());
      assertEquals(null, info.getDefaultValue());
      assertEquals(3, info.getLegalValues().size());
      assertTrue("legal1 should be a legal value", info.getLegalValues().contains("legal1"));
      assertTrue("legal2 should be a legal value", info.getLegalValues().contains("legal2"));
      assertTrue("default should be a legal value", info.getLegalValues().contains("default"));

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.STRING, "default", null);
      assertEquals("name", info.getName());
      assertEquals("description", info.getDescription());
      assertEquals("java.lang.String", info.getType());
      assertEquals("default", info.getDefaultValue());
      assertEquals(null, info.getLegalValues());

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.STRING, null, null);
      assertEquals("name", info.getName());
      assertEquals("description", info.getDescription());
      assertEquals("java.lang.String", info.getType());
      assertEquals(null, info.getDefaultValue());
      assertEquals(null, info.getLegalValues());
   }

   public void testOpenType()
      throws Exception
   {
      OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.STRING);
      assertEquals(SimpleType.STRING, info.getOpenType());
   }

   public void testHas()
      throws Exception
   {
      OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.STRING, "default");
      assertEquals(true, info.hasDefaultValue());

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.STRING, null);
      assertEquals(false, info.hasDefaultValue());

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, null, new Integer(3), null);
      assertEquals(true, info.hasMinValue());

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, null, null, null);
      assertEquals(false, info.hasMinValue());

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, null, null, new Integer(3));
      assertEquals(true, info.hasMaxValue());

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, null, null, null);
      assertEquals(false, info.hasMaxValue());

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, null, new Object[] { new Integer(3) });
      assertEquals(true, info.hasLegalValues());

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, null, null);
      assertEquals(false, info.hasLegalValues());
   }

   public void testIsValue()
      throws Exception
   {
      OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.STRING);
      assertTrue("String should be a value", info.isValue("string"));
      assertTrue("Integer should not be a value", info.isValue(new Integer(3)) == false);
      assertTrue("Null should not be a value", info.isValue(null) == false);
   }

   public void testEquals()
      throws Exception
   {
      OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.STRING);

      assertTrue("Null should not be equal", info.equals(null) == false);
      assertTrue("Only OpenMBeanParameterInfo should be equal", info.equals(new Object()) == false);

      OpenMBeanParameterInfoSupport info2 = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.STRING);

      assertTrue("Different instances of the same data are equal", info.equals(info2));
      assertTrue("Different instances of the same data are equal", info2.equals(info));

      info2 = new OpenMBeanParameterInfoSupport(
         "name", "description2", SimpleType.STRING);

      assertTrue("Different instances with different descriptions are equal", info.equals(info2));
      assertTrue("Different instances with different descritpions are equal", info2.equals(info));

      info2 = new OpenMBeanParameterInfoSupport(
         "name2", "description", SimpleType.STRING);

      assertTrue("Instances with different names are not equal", info.equals(info2) == false);
      assertTrue("Instances with different names are not equal", info2.equals(info) == false);

      info2 = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER);

      assertTrue("Instances with different types are not equal", info.equals(info2) == false);
      assertTrue("Instances with different types are not equal", info2.equals(info) == false);

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(3), new Integer(2), new Integer(4));
      info2 = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(2), new Integer(2), new Integer(4));

      assertTrue("Instances with different default values are not equal", info.equals(info2) == false);
      assertTrue("Instances with different default values are not equal", info2.equals(info) == false);

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(3), new Integer(2), new Integer(4));
      info2 = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(2), null, new Integer(4));

      assertTrue("Instances with different default values are not equal", info.equals(info2) == false);
      assertTrue("Instances with different default values are not equal", info2.equals(info) == false);

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(3));
      info2 = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(3));

      assertTrue("Different instances of the same default value are equal", info.equals(info2));
      assertTrue("Different instances of the same default value are equal", info2.equals(info));

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, null, new Integer(2), null);
      info2 = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, null, new Integer(2), null);

      assertTrue("Different instances of the same minimum are equal", info.equals(info2));
      assertTrue("Different instances of the same minimum are equal", info2.equals(info));

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, null, null, new Integer(2));
      info2 = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, null, null, new Integer(2));

      assertTrue("Different instances of the same maximum are equal", info.equals(info2));
      assertTrue("Different instances of the same maximum are equal", info2.equals(info));

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(3), new Integer(2), new Integer(4));
      info2 = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(3), new Integer(1), new Integer(4));

      assertTrue("Instances with different minimums are not equal", info.equals(info2) == false);
      assertTrue("Instances with different minimums are not equal", info2.equals(info) == false);

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(3), new Integer(2), new Integer(4));
      info2 = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(3), null, new Integer(4));

      assertTrue("Instances with different minimums are not equal", info.equals(info2) == false);
      assertTrue("Instances with different minimums are not equal", info2.equals(info) == false);

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(3), new Integer(2), new Integer(4));
      info2 = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(3), new Integer(2), new Integer(5));

      assertTrue("Instances with different maximums are not equal", info.equals(info2) == false);
      assertTrue("Instances with different maximums are not equal", info2.equals(info) == false);

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(3), new Integer(2), new Integer(4));
      info2 = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(3), new Integer(2), null);

      assertTrue("Instances with different maximums are not equal", info.equals(info2) == false);
      assertTrue("Instances with different maximums are not equal", info2.equals(info) == false);

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, null, new Object[] { new Integer(2), new Integer(3) });
      info2 = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, null, new Object[] { new Integer(2), new Integer(3) });

      assertTrue("Different instances of the same legal values are equal", info.equals(info2));
      assertTrue("Different instances of the same legal values are equal", info2.equals(info));

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, null, new Object[] { new Integer(2), new Integer(3) });
      info2 = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, null, new Object[] { new Integer(2), new Integer(4) });

      assertTrue("Different instances with different legal values are equal", info.equals(info2) == false);
      assertTrue("Different instances with different legal values are equal", info2.equals(info) == false);

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, null, new Object[] { new Integer(2), new Integer(3) });
      info2 = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, null, new Object[] { new Integer(2) });

      assertTrue("Different instances with different legal values are equal", info.equals(info2) == false);
      assertTrue("Different instances with different legal values are equal", info2.equals(info) == false);

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, null, new Object[] { new Integer(2), new Integer(3) });
      info2 = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, null, null);

      assertTrue("Different instances with different legal values are equal", info.equals(info2) == false);
      assertTrue("Different instances with different legal values are equal", info2.equals(info) == false);
   }

   public void testHashCode()
      throws Exception
   {

      OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(3), new Integer(2), new Integer(4));
      int myHash = "name".hashCode() + SimpleType.INTEGER.hashCode() +
         new Integer(3).hashCode() + new Integer(2).hashCode() + new Integer(4).hashCode();
      assertEquals(myHash, info.hashCode());
      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(3), 
         new Object[] { new Integer(2), new Integer(3), new Integer(4) } );
      myHash = "name".hashCode() + SimpleType.INTEGER.hashCode() +
         new Integer(3).hashCode() + new Integer(2).hashCode() + new Integer(3).hashCode() + new Integer(4).hashCode();
      assertEquals(myHash, info.hashCode());
   }

   public void testToString()
      throws Exception
   {
      OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(3), new Integer(2), new Integer(4));

      String toString = info.toString();
      assertTrue("info.toString() should contain the name", toString.indexOf("name") != -1);
      assertTrue("info.toString() should contain the simple type",
         toString.indexOf(SimpleType.INTEGER.toString()) != -1);
      assertTrue("info.toString() should contain the default value",
         toString.indexOf(new Integer(3).toString()) != -1);
      assertTrue("info.toString() should contain the minimum value",
         toString.indexOf(new Integer(2).toString()) != -1);
      assertTrue("info.toString() should contain the maximum value",
         toString.indexOf(new Integer(4).toString()) != -1);

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(3), 
         new Object[] { new Integer(2), new Integer(3), new Integer(4) } );
      assertTrue("info.toString() should contain the legal value 2",
         toString.indexOf(new Integer(2).toString()) != -1);
      assertTrue("info.toString() should contain the legal value 3",
         toString.indexOf(new Integer(3).toString()) != -1);
      assertTrue("info.toString() should contain the legal value 4",
         toString.indexOf(new Integer(4).toString()) != -1);
   }

   public void testSerialization()
      throws Exception
   {
      OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(3), new Integer(2), new Integer(4));

      // Serialize it
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(info);
    
      // Deserialize it
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new ObjectInputStream(bais);
      Object result = ois.readObject();

      assertEquals(info, result);

      info = new OpenMBeanParameterInfoSupport(
         "name", "description", SimpleType.INTEGER, new Integer(3), 
         new Object[] { new Integer(2), new Integer(3), new Integer(4) } );

      // Serialize it
      baos = new ByteArrayOutputStream();
      oos = new ObjectOutputStream(baos);
      oos.writeObject(info);
    
      // Deserialize it
      bais = new ByteArrayInputStream(baos.toByteArray());
      ois = new ObjectInputStream(bais);
      result = ois.readObject();

      assertEquals(info, result);
   }

   public void testErrors1()
      throws Exception
   {
      boolean caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            null, "description", SimpleType.INTEGER);
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
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "", "description", SimpleType.INTEGER);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for an empty name");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", null, SimpleType.INTEGER);
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
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "", SimpleType.INTEGER);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for an empty description");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "", "description", SimpleType.INTEGER);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for an empty name");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", null);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for null simple type");
   }

   public void testErrors2()
      throws Exception
   {
      boolean caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            null, "description", SimpleType.INTEGER, new Integer(3));
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
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "", "description", SimpleType.INTEGER, new Integer(3));
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for an empty name");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", null, SimpleType.INTEGER, new Integer(3));
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
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "", SimpleType.INTEGER, new Integer(3));
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for an empty description");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "", "description", SimpleType.INTEGER, new Integer(3));
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for an empty name");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", null, new Integer(3));
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for null simple type");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.STRING, new Integer(3));
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected OpenDataException for wrong simple type and default value");

      caught = false;
      try
      {
         ArrayType arrayType = new ArrayType(1, SimpleType.STRING);
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", arrayType, new String[0]);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected OpenDataException for array type and default value");

      caught = false;
      try
      {
         ArrayType arrayType = new ArrayType(1, SimpleType.STRING);
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", arrayType, null);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == true)
         fail("Didn't execpt OpenDataException for array type and no default value");

      caught = false;
      try
      {
         String[] itemNames = new String[] { "name1", "name2" };
         String[] itemDescriptions = new String[] { "desc1", "desc2" };
         OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
         CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
            itemNames, itemDescriptions, itemTypes);

         String[] indexNames = new String[] { "name1", "name2" };
         TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);
         TabularDataSupport data = new TabularDataSupport(tabularType);

         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", tabularType, data);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected OpenDataException for tabular type and default value");

      caught = false;
      try
      {
         String[] itemNames = new String[] { "name1", "name2" };
         String[] itemDescriptions = new String[] { "desc1", "desc2" };
         OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
         CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
            itemNames, itemDescriptions, itemTypes);

         String[] indexNames = new String[] { "name1", "name2" };
         TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", tabularType, null);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == true)
         fail("Didn't execpt OpenDataException for tabular type and null default value");
   }

   public void testErrors3()
      throws Exception
   {
      boolean caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            null, "description", SimpleType.INTEGER, new Integer(3), new Integer[] { new Integer(3), new Integer(4) });
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
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "", "description", SimpleType.INTEGER, new Integer(3), new Integer[] { new Integer(3), new Integer(4) });
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for an empty name");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", null, SimpleType.INTEGER, new Integer(3), new Integer[] { new Integer(3), new Integer(4) });
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
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "", SimpleType.INTEGER, new Integer(3), new Integer[] { new Integer(3), new Integer(4) });
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for an empty description");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "", "description", SimpleType.INTEGER, new Integer(3), new Integer[] { new Integer(3), new Integer(4) });
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for an empty name");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", null, new Integer(3), new Integer[] { new Integer(3), new Integer(4) });
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for null simple type");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.STRING, new Integer(3), new Integer[] { new Integer(3), new Integer(4) });
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected OpenDataException for wrong simple type and default value");

      caught = false;
      try
      {
         ArrayType arrayType = new ArrayType(1, SimpleType.STRING);
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", arrayType, new String[0], null);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected OpenDataException for array type and default value");

      caught = false;
      try
      {
         ArrayType arrayType = new ArrayType(1, SimpleType.STRING);
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", arrayType, null, null);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == true)
         fail("Didn't expect OpenDataException for array type and no default value and legals");

      caught = false;
      try
      {
         String[] itemNames = new String[] { "name1", "name2" };
         String[] itemDescriptions = new String[] { "desc1", "desc2" };
         OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
         CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
            itemNames, itemDescriptions, itemTypes);

         String[] indexNames = new String[] { "name1", "name2" };
         TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);
         TabularDataSupport data = new TabularDataSupport(tabularType);

         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", tabularType, data, null);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected OpenDataException for tabular type and default value");

      caught = false;
      try
      {
         String[] itemNames = new String[] { "name1", "name2" };
         String[] itemDescriptions = new String[] { "desc1", "desc2" };
         OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
         CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
            itemNames, itemDescriptions, itemTypes);

         String[] indexNames = new String[] { "name1", "name2" };
         TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", tabularType, null, null);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == true)
         fail("Didn't expect OpenDataException for tabular type and null default value and legals");

      caught = false;
      try
      {
         ArrayType arrayType = new ArrayType(1, SimpleType.STRING);
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", arrayType, null, new String[] { "hello", "goodbye" });
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected OpenDataException for array type and default value");

      caught = false;
      try
      {
         ArrayType arrayType = new ArrayType(1, SimpleType.STRING);
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", arrayType, null, new String[0]);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == true)
         fail("Didn't expect OpenDataException for array type and no default value and empty legals");

      caught = false;
      try
      {
         String[] itemNames = new String[] { "name1", "name2" };
         String[] itemDescriptions = new String[] { "desc1", "desc2" };
         OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
         CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
            itemNames, itemDescriptions, itemTypes);

         String[] indexNames = new String[] { "name1", "name2" };
         TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);
         TabularDataSupport data = new TabularDataSupport(tabularType);

         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", tabularType, null, new TabularDataSupport[] { data });
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected OpenDataException for tabular type and legal values");

      caught = false;
      try
      {
         String[] itemNames = new String[] { "name1", "name2" };
         String[] itemDescriptions = new String[] { "desc1", "desc2" };
         OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
         CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
            itemNames, itemDescriptions, itemTypes);

         String[] indexNames = new String[] { "name1", "name2" };
         TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", tabularType, null, new TabularDataSupport[0]);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == true)
         fail("Didn't expect OpenDataException for tabular type and null default value and empty legals");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.STRING, new Integer(3), new Integer[] { new Integer(2), new Integer(4) });
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected OpenDataException for default value not a legal value");
   }

   public void testErrors4()
      throws Exception
   {
      boolean caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            null, "description", SimpleType.INTEGER, new Integer(3), new Integer(3), new Integer(4));
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
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "", "description", SimpleType.INTEGER, new Integer(3), new Integer(3), new Integer(4));
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for an empty name");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", null, SimpleType.INTEGER, new Integer(3), new Integer(3), new Integer(4));
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
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "", SimpleType.INTEGER, new Integer(3), new Integer(3), new Integer(4));
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for an empty description");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "", "description", SimpleType.INTEGER, new Integer(3), new Integer(3), new Integer(4));
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for an empty name");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", null, new Integer(3), new Integer(3), new Integer(4));
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for null simple type");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.STRING, new Integer(3), null, null);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected OpenDataException for wrong simple type and default value");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.STRING, null, new Integer(3), null);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected OpenDataException for wrong simple type and minimum value");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.STRING, null, null, new Integer(3));
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected OpenDataException for wrong simple type and maximum value");

      caught = false;
      try
      {
         ArrayType arrayType = new ArrayType(1, SimpleType.STRING);
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", arrayType, new String[0], null, null);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected OpenDataException for array type and default value");

      caught = false;
      try
      {
         ArrayType arrayType = new ArrayType(1, SimpleType.STRING);
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", arrayType, null, null, null);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == true)
         fail("Didn't expect OpenDataException for array type and no default value");

      caught = false;
      try
      {
         String[] itemNames = new String[] { "name1", "name2" };
         String[] itemDescriptions = new String[] { "desc1", "desc2" };
         OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
         CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
            itemNames, itemDescriptions, itemTypes);

         String[] indexNames = new String[] { "name1", "name2" };
         TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);
         TabularDataSupport data = new TabularDataSupport(tabularType);

         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", tabularType, data, null, null);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected OpenDataException for tabular type and default value");

      caught = false;
      try
      {
         String[] itemNames = new String[] { "name1", "name2" };
         String[] itemDescriptions = new String[] { "desc1", "desc2" };
         OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
         CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
            itemNames, itemDescriptions, itemTypes);

         String[] indexNames = new String[] { "name1", "name2" };
         TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", tabularType, null, null, null);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == true)
         fail("Didn't expect OpenDataException for tabular type and null default value");

      caught = false;
      try
      {
         ArrayType arrayType = new ArrayType(1, SimpleType.STRING);
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", arrayType, new String[] { "hello", "goodbye" }, null, null);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected OpenDataException for array type and default value");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.STRING, new Integer(3), new Integer(4), new Integer(5));
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected OpenDataException for default value less than minimum value");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.INTEGER, new Integer(4), new Integer(4), new Integer(5));
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == true)
         fail("Didn't expect OpenDataException for default value equal minimum value");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.INTEGER, new Integer(6), new Integer(4), new Integer(5));
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected OpenDataException for default value greater than maximum value");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.INTEGER, new Integer(5), new Integer(4), new Integer(5));
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == true)
         fail("Didn't expect OpenDataException for default value equal maximum value");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.INTEGER, null, new Integer(4), new Integer(3));
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected OpenDataException for minimum greater than maximum value");

      caught = false;
      try
      {
         OpenMBeanParameterInfoSupport info = new OpenMBeanParameterInfoSupport(
            "name", "description", SimpleType.INTEGER, null, new Integer(4), new Integer(4));
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == true)
         fail("Didn't expect OpenDataException for minimum equal maximum value");
   }
}
