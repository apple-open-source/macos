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
import java.util.Collection;
import java.util.List;
import java.util.HashMap;

import javax.management.openmbean.CompositeData;
import javax.management.openmbean.CompositeDataSupport;
import javax.management.openmbean.CompositeType;
import javax.management.openmbean.InvalidKeyException;
import javax.management.openmbean.OpenDataException;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;

/**
 * Composite data support tests.<p>
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class CompositeDataSupportTestCase
  extends TestCase
{
   // Static --------------------------------------------------------------------

   // Attributes ----------------------------------------------------------------

   // Constructor ---------------------------------------------------------------

   /**
    * Construct the test
    */
   public CompositeDataSupportTestCase(String s)
   {
      super(s);
   }

   // Tests ---------------------------------------------------------------------

   public void testCompositeDataSupport()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      new CompositeDataSupport(compositeType, map);
      new CompositeDataSupport(compositeType, new String[] { "name1", "name2" }, new Object[] { "value1", new Integer(2) });
   }

   public void testGetCompositeType()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport data = new CompositeDataSupport(compositeType, map);
      assertEquals(compositeType, data.getCompositeType());
   }

   public void testGet()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport data = new CompositeDataSupport(compositeType, map);
      assertEquals("value1", data.get("name1"));
      assertEquals(new Integer(2), data.get("name2"));
   }

   public void testGetAll()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport data = new CompositeDataSupport(compositeType, map);
      Object[] result = data.getAll(new String[] { "name1", "name2" });
      assertEquals("value1", result[0]);
      assertEquals(new Integer(2), result[1]);
      result = data.getAll(new String[] { "name2", "name1" });
      assertEquals("value1", result[1]);
      assertEquals(new Integer(2), result[0]);
      result = data.getAll(new String[] { "name1" });
      assertEquals("value1", result[0]);
      result = data.getAll(new String[] { "name2" });
      assertEquals(new Integer(2), result[0]);
   }

   /**
    * REVIEW: getAll(null) is not in the spec/javadoc, the RI returns an empty array
    */   
   public void testGetAllNotSpecified()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport data = new CompositeDataSupport(compositeType, map);
      List result = Arrays.asList(data.getAll(null));

      assertTrue("getAll(null) has no elements", result.isEmpty());
   }

   public void testContainsKey()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport data = new CompositeDataSupport(compositeType, map);
      assertTrue("data should contain key name1", data.containsKey("name1") == true);
      assertTrue("data should contain key name2", data.containsKey("name2") == true);
      assertTrue("data should not contain key nameX", data.containsKey("nameX") == false);
      assertTrue("data should not contain key null", data.containsKey(null) == false);
      assertTrue("data should not contain key <empty>", data.containsKey("") == false);
   }

   public void testContainsValue()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport data = new CompositeDataSupport(compositeType, map);
      assertTrue("data should contain value value1", data.containsValue("value1") == true);
      assertTrue("data should contain value 2", data.containsValue(new Integer(2)) == true);
      assertTrue("data should not contain value name1", data.containsValue("name1") == false);
      assertTrue("data should not contain key null", data.containsValue(null) == false);
      assertTrue("data should not contain key <empty>", data.containsValue("") == false);

      map.clear();
      map.put("name1", "value1");
      map.put("name2", null);
      data = new CompositeDataSupport(compositeType, map);
      assertTrue("data should contain value null", data.containsValue(null) == true);
   }

   public void testValues()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport data = new CompositeDataSupport(compositeType, map);
      Collection values = data.values();
      assertTrue("data values contain 2 elements", values.size() == 2);
      assertTrue("data values should have value1", values.contains("value1"));
      assertTrue("data values should have 2", values.contains(new Integer(2)));
      assertTrue("data values should not have name1", values.contains("name1") == false);
      assertTrue("data values should not have null", values.contains(null) == false);
      assertTrue("data values should not have <empty>", values.contains("") == false);

      map.clear();
      map.put("name1", "value1");
      map.put("name2", null);
      data = new CompositeDataSupport(compositeType, map);
      values = data.values();
      assertTrue("data values should contain value null", values.contains(null) == true);
   }

   public void testEquals()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport data = new CompositeDataSupport(compositeType, map);

      assertTrue("data should equal itself", data.equals(data));
      assertTrue("data should not equal null", data.equals(null) == false);
      assertTrue("data should not equal non CompositeData", data.equals(new Object()) == false);

      String[] itemNames2 = new String[] { "name1", "name2" };
      String[] itemDescriptions2 = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes2 = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType2 = new CompositeType("typeName", "description",
         itemNames2, itemDescriptions2, itemTypes2);
      HashMap map2 = new HashMap();
      map2.put("name1", "value1");
      map2.put("name2", new Integer(2));
      CompositeDataSupport data2 = new CompositeDataSupport(compositeType2, map2);

      assertTrue("data should equal with data2 with different instance of the same composite type", data.equals(data2));
      assertTrue("data2 should equal with data with different instance of the same composite type", data2.equals(data));

      compositeType2 = new CompositeType("typeName2", "description",
         itemNames2, itemDescriptions2, itemTypes2);
      data2 = new CompositeDataSupport(compositeType2, map2);

      assertTrue("data should not be equal with data2 with different composite type", data.equals(data2) == false);
      assertTrue("data2 should not be equal with data with different composite type", data2.equals(data) == false);

      map2 = new HashMap();
      map2.put("name1", "value1");
      map2.put("name2", new Integer(3));
      data2 = new CompositeDataSupport(compositeType, map2);

      assertTrue("data should not be equal with data2 with different values", data.equals(data2) == false);
      assertTrue("data2 should not be equal with data with different value", data2.equals(data) == false);
   }

   public void testHashCode()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport data = new CompositeDataSupport(compositeType, map);

      int myHashCode = compositeType.hashCode() + "value1".hashCode() + new Integer(2).hashCode();
      assertTrue("Wrong hash code generated", myHashCode == data.hashCode());
   }

   public void testToString()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport data = new CompositeDataSupport(compositeType, map);

      String toString = data.toString();

      assertTrue("toString() should contain the composite type",
         toString.indexOf(compositeType.toString()) != -1);
      assertTrue("toString() should contain name1=value1",
         toString.indexOf("name1=value1") != -1);
      assertTrue("toString() should contain name2=" + new Integer(2),
         toString.indexOf("name2=" + new Integer(2)) != -1);
   }

   public void testSerialization()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport data = new CompositeDataSupport(compositeType, map);

      // Serialize it
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(data);
    
      // Deserialize it
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new ObjectInputStream(bais);
      Object result = ois.readObject();

      assertEquals(data, result);
   }

   public void testErrorsArray()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      Object[] itemValues = new Object[] { "value1", new Integer(2) };

      boolean caught = false;
      try
      {
         new CompositeDataSupport(null, itemNames, itemValues);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for null composite type");

      caught = false;
      try
      {
         new CompositeDataSupport(compositeType, null, itemValues);
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
         new CompositeDataSupport(compositeType, new String[0], itemValues);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for empty item names");

      caught = false;
      try
      {
         new CompositeDataSupport(compositeType, itemNames, null);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for null item values");

      caught = false;
      try
      {
         new CompositeDataSupport(compositeType, itemNames, new Object[0]);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for empty item values");

      caught = false;
      try
      {
         new CompositeDataSupport(compositeType, new String[] { "name1", null }, itemValues);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for a null item name");

      caught = false;
      try
      {
         new CompositeDataSupport(compositeType, new String[] { "name1", "" }, itemValues);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for an empty item name");

      caught = false;
      try
      {
         new CompositeDataSupport(compositeType, itemNames, new Object[] { "wrong" });
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for mismatch in number of itemNames/itemValues");

      caught = false;
      try
      {
         new CompositeDataSupport(compositeType, new String[] { "name1" }, new Object[] { "value1" });
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted OpenDataException for mismatch in number of itemNames for CompositeType/CompositeData");

      caught = false;
      try
      {
         new CompositeDataSupport(compositeType, new String[] { "name1", "wrongName" }, itemValues);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted OpenDataException for an item name not in the composite type");

      caught = false;
      try
      {
         new CompositeDataSupport(compositeType, itemNames, new Object[] { "value1", "wrong" });
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted OpenDataException for an item value of the wrong type");

      new CompositeDataSupport(compositeType, itemNames, new Object[] { "value1", null });
   }

   public void testErrorsMap()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));

      boolean caught = false;
      try
      {
         new CompositeDataSupport(null, map);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for null composite type");

      caught = false;
      try
      {
         new CompositeDataSupport(compositeType, null);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for null map");

      caught = false;
      try
      {
         new CompositeDataSupport(compositeType, new HashMap());
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for empty map");

      caught = false;
      try
      {
         HashMap map2 = new HashMap();
         map2.put("name1", "value1");
         map2.put(null, new Integer(2));
         new CompositeDataSupport(compositeType, map2);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for a null key in map");

      caught = false;
      try
      {
         HashMap map2 = new HashMap();
         map2.put("name1", "value1");
         map2.put("", new Integer(2));
         new CompositeDataSupport(compositeType, map2);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for an empty key in map");

      caught = false;
      try
      {
         HashMap map2 = new HashMap();
         map2.put("name1", "value1");
         new CompositeDataSupport(compositeType, map2);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted OpenDataException for mismatch in number of items for CompositeType/CompositeData");

      caught = false;
      try
      {
         HashMap map2 = new HashMap();
         map2.put("name1", "value1");
         map2.put("wrongName", new Integer(2));
         new CompositeDataSupport(compositeType, map2);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted OpenDataException for an item name not in the composite type");

      caught = false;
      try
      {
         HashMap map2 = new HashMap();
         map2.put("name1", "value1");
         map2.put("name2", "wrong");
         new CompositeDataSupport(compositeType, map2);
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted OpenDataException for an item value of the wrong type");

      caught = false;
      try
      {
         HashMap map2 = new HashMap();
         map2.put("name1", "value1");
         map2.put(new Integer(2), new Integer(2));
         new CompositeDataSupport(compositeType, map2);
      }
      catch (ArrayStoreException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted ArrayStoreException for a non String key in map");

      HashMap map2 = new HashMap();
      map2.put("name1", "value1");
      map2.put("name2", null);
      new CompositeDataSupport(compositeType, map2);
   }

   public void testErrors()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport data = new CompositeDataSupport(compositeType, map);

      boolean caught = false;
      try
      {
         data.get(null);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for get and a null key");

      caught = false;
      try
      {
         data.get("");
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for get and an empty key");

      caught = false;
      try
      {
         data.get("wrong");
      }
      catch (InvalidKeyException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted InvalidKeyException for get and a wrong key");

      caught = false;
      try
      {
         data.getAll(new String[] { "name1", null });
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for getAll and a null key");

      caught = false;
      try
      {
         data.getAll(new String[] { "name1", "" });
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for getAll and an empty key");

      caught = false;
      try
      {
         data.getAll(new String[] { "name1", "wrong" });
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted InvalidKeyException for getAll and an invalid key");
   }
}
