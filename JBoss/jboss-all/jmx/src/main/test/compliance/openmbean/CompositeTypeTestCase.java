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
import java.util.Set;

import javax.management.openmbean.CompositeData;
import javax.management.openmbean.CompositeDataSupport;
import javax.management.openmbean.CompositeType;
import javax.management.openmbean.OpenDataException;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;

/**
 * Composite type tests.<p>
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class CompositeTypeTestCase
  extends TestCase
{
   // Static --------------------------------------------------------------------

   // Attributes ----------------------------------------------------------------

   // Constructor ---------------------------------------------------------------

   /**
    * Construct the test
    */
   public CompositeTypeTestCase(String s)
   {
      super(s);
   }

   // Tests ---------------------------------------------------------------------

   public void testCompositeTypeOpenType()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      assertEquals(CompositeData.class.getName(), compositeType.getClassName());
      assertEquals("description", compositeType.getDescription());
      assertEquals("typeName", compositeType.getTypeName());
      assertTrue("Composite type should not be an array", compositeType.isArray() == false);
   }

   public void testContainsKey()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      assertTrue("Composite type should contain key name1", compositeType.containsKey("name1") == true);
      assertTrue("Composite type should contain key name2", compositeType.containsKey("name2") == true);
      assertTrue("Composite type should not contain key nameX", compositeType.containsKey("nameX") == false);
      assertTrue("Composite type should not contain key null", compositeType.containsKey(null) == false);
      assertTrue("Composite type should not contain key <empty>", compositeType.containsKey("") == false);
   }

   public void testGetDescriptionForItemName()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      assertEquals("desc1", compositeType.getDescription("name1"));
      assertEquals("desc2", compositeType.getDescription("name2"));
   }

   public void testGetTypeForItemName()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      assertEquals(SimpleType.STRING, compositeType.getType("name1"));
      assertEquals(SimpleType.INTEGER, compositeType.getType("name2"));
   }

   public void testKeySet()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      Set keys = compositeType.keySet();
      assertTrue("Should be 2 items", keys.size() == 2);
      assertTrue("Should contain name1", keys.contains("name1"));
      assertTrue("Should contain name2", keys.contains("name2"));
   }

   public void testIsValue()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);

      assertTrue("null is not a value of composite type", compositeType.isValue(null) == false);
      assertTrue("object is not a value of composite type", compositeType.isValue(new Object()) == false);

      Object[] itemValues = new Object[] { "string", new Integer(2) };
      CompositeDataSupport data = new CompositeDataSupport(compositeType, itemNames, itemValues);
      assertTrue("data should be a value of composite type", compositeType.isValue(data));

      CompositeType compositeType2 = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      data = new CompositeDataSupport(compositeType2, itemNames, itemValues);
      assertTrue("data should be a value of composite type, even though not the object instance",
         compositeType.isValue(data));

      OpenType[] itemTypes2 = new OpenType[] { SimpleType.STRING, SimpleType.LONG };
      compositeType2 = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes2);
      Object[] itemValues2 = new Object[] { "string", new Long(2) };
      data = new CompositeDataSupport(compositeType2, itemNames, itemValues2);
      assertTrue("data should not be a value of composite type, it has different types",
         compositeType.isValue(data) == false);

      compositeType2 = new CompositeType("typeName2", "description",
         itemNames, itemDescriptions, itemTypes);
      data = new CompositeDataSupport(compositeType2, itemNames, itemValues);
      assertTrue("data should not be a value of composite type, it has a different type name",
         compositeType.isValue(data) == false);

      String[] itemNames2 = new String[] { "nameX", "name2" };
      compositeType2 = new CompositeType("typeName", "description",
         itemNames2, itemDescriptions, itemTypes);
      data = new CompositeDataSupport(compositeType2, itemNames2, itemValues);
      assertTrue("data should not be a value of composite type, it has different item names",
         compositeType.isValue(data) == false);
   }

   public void testEquals()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);

      assertTrue("null is not equal composite type", compositeType.equals(null) == false);
      assertTrue("object is not equal composite type", compositeType.equals(new Object()) == false);

      CompositeType compositeType2 = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      assertTrue("compositeType2 should be equal composite type, even though not the object instance",
         compositeType.equals(compositeType2));
      assertTrue("compositeType2 should be equal composite type, even though not the object instance",
         compositeType2.equals(compositeType));

      OpenType[] itemTypes2 = new OpenType[] { SimpleType.STRING, SimpleType.LONG };
      compositeType2 = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes2);
      assertTrue("compositeType2 should not be equal composite type, it has different types",
         compositeType.equals(compositeType2) == false);
      assertTrue("compositeType2 should not be equal composite type, it has different types",
         compositeType2.equals(compositeType) == false);

      compositeType2 = new CompositeType("typeName2", "description",
         itemNames, itemDescriptions, itemTypes);
      assertTrue("compositeType2 should not be equal composite type, it has a different type name",
         compositeType.equals(compositeType2) == false);
      assertTrue("compositeType2 should not be equal composite type, it has a different type name",
         compositeType2.equals(compositeType) == false);

      String[] itemNames2 = new String[] { "nameX", "name2" };
      compositeType2 = new CompositeType("typeName", "description",
         itemNames2, itemDescriptions, itemTypes);
      assertTrue("compositeType2 should not be equal composite type, it has different item names",
         compositeType.equals(compositeType2) == false);
      assertTrue("compositeType2 should not be equal composite type, it has different item names",
         compositeType2.equals(compositeType) == false);
   }

   public void testHashCode()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);

      int myHashCode = "typeName".hashCode() + SimpleType.STRING.hashCode() + SimpleType.INTEGER.hashCode()
         + "name1".hashCode() + "name2".hashCode();
      assertTrue("Wrong hash code generated", myHashCode == compositeType.hashCode());
   }

   public void testToString()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);

      String toString = compositeType.toString();

      assertTrue("toString() should contain the composite type class name",
         toString.indexOf(CompositeType.class.getName()) != -1);
      assertTrue("toString() should contain the item name name1",
         toString.indexOf("name1") != -1);
      assertTrue("toString() should contain the item name name2",
         toString.indexOf("name2") != -1);
      assertTrue("toString() should contain " + SimpleType.STRING,
         toString.indexOf(SimpleType.STRING.toString()) != -1);
      assertTrue("toString() should contain " + SimpleType.INTEGER,
         toString.indexOf(SimpleType.INTEGER.toString()) != -1);
   }

   public void testSerialization()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);

      // Serialize it
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(compositeType);
    
      // Deserialize it
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new ObjectInputStream(bais);
      Object result = ois.readObject();

      assertEquals(compositeType, result);
   }

   public void testErrors()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };

      boolean caught = false;
      try
      {
         new CompositeType(null, "description", itemNames, itemDescriptions, itemTypes);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for null typeName");

      caught = false;
      try
      {
         new CompositeType("", "description", itemNames, itemDescriptions, itemTypes);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for empty typeName");

      caught = false;
      try
      {
         new CompositeType("typeName", null, itemNames, itemDescriptions, itemTypes);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for null description");

      caught = false;
      try
      {
         new CompositeType("typeName", "", itemNames, itemDescriptions, itemTypes);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for empty description");

      caught = false;
      try
      {
         new CompositeType("typeName", "description", null, itemDescriptions, itemTypes);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for null item names");

      caught = false;
      try
      {
         new CompositeType("typeName", "description", itemNames, null, itemTypes);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for null item descriptions");

      caught = false;
      try
      {
         new CompositeType("typeName", "description", itemNames, itemDescriptions, null);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for null item types");

      String[] nullItemNames = new String[] { "name1", null };
      caught = false;
      try
      {
         new CompositeType("typeName", "description", nullItemNames, itemDescriptions, itemTypes);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for null element of item names");

      String[] nullItemDescriptions = new String[] { "desc1", null };
      caught = false;
      try
      {
         new CompositeType("typeName", "description", itemNames, nullItemDescriptions, itemTypes);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for null element of item descriptions");

      OpenType[] nullItemTypes = new OpenType[] { SimpleType.STRING, null };
      caught = false;
      try
      {
         new CompositeType("typeName", "description", itemNames, itemDescriptions, nullItemTypes);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for null element of item types");

      String[] wrongItemNames = new String[] { "name1" };
      caught = false;
      try
      {
         new CompositeType("typeName", "description", wrongItemNames, itemDescriptions, itemTypes);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for wrong number of elements for item names");

      String[] wrongItemDescriptions = new String[] { "desc1"};
      caught = false;
      try
      {
         new CompositeType("typeName", "description", itemNames, wrongItemDescriptions, itemTypes);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for wrong number of elements for item descriptions");

      OpenType[] wrongItemTypes = new OpenType[] { SimpleType.STRING };
      caught = false;
      try
      {
         new CompositeType("typeName", "description", itemNames, itemDescriptions, wrongItemTypes);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for wrong number of elements for item types");

      String[] duplicateItemNames = new String[] { "desc1", "desc1" };
      caught = false;
      try
      {
         new CompositeType("typeName", "description", duplicateItemNames, itemDescriptions, itemTypes);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted OpenDataException for duplicate item names");

      duplicateItemNames = new String[] { "desc1", " desc1 " };
      caught = false;
      try
      {
         new CompositeType("typeName", "description", duplicateItemNames, itemDescriptions, itemTypes);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted OpenDataException for duplicate item names");
   }

   // Support -------------------------------------------------------------------
}
