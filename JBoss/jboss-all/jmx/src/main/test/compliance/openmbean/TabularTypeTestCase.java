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
import java.util.Iterator;
import java.util.List;

import javax.management.openmbean.CompositeType;
import javax.management.openmbean.OpenDataException;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;
import javax.management.openmbean.TabularData;
import javax.management.openmbean.TabularDataSupport;
import javax.management.openmbean.TabularType;

/**
 * Tabular type tests.<p>
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class TabularTypeTestCase
  extends TestCase
{
   // Static --------------------------------------------------------------------

   // Attributes ----------------------------------------------------------------

   // Constructor ---------------------------------------------------------------

   /**
    * Construct the test
    */
   public TabularTypeTestCase(String s)
   {
      super(s);
   }

   // Tests ---------------------------------------------------------------------

   public void testTabularTypeOpenType()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      assertEquals(TabularData.class.getName(), tabularType.getClassName());
      assertEquals("description", tabularType.getDescription());
      assertEquals("typeName", tabularType.getTypeName());
      assertTrue("Tabular type should not be an array", tabularType.isArray() == false);
   }

   public void testGetRowType()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      assertEquals(rowType, tabularType.getRowType());
   }

   public void testIndexNames()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      List indexList = tabularType.getIndexNames();
      assertTrue("wrong number of index names", indexList.size() == 2);
      assertTrue("index list should contain name1", indexList.contains("name1"));
      assertTrue("index list should contain name2", indexList.contains("name2"));
      Iterator i = indexList.iterator();
      assertTrue("first index is name1", i.next().equals("name1"));
      assertTrue("second index is name2", i.next().equals("name2"));
   }

   public void testIsValue()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      assertTrue("null is not a value of tabular type", tabularType.isValue(null) == false);
      assertTrue("object is not a value of tabular type", tabularType.isValue(new Object()) == false);

      TabularDataSupport data = new TabularDataSupport(tabularType);
      assertTrue("data should is a value", tabularType.isValue(data));

      TabularType tabularType2 = new TabularType("typeName", "description", rowType, indexNames);
      data = new TabularDataSupport(tabularType2);
      assertTrue("data is a value, even though the tabular type is a different instance",
                 tabularType.isValue(data));

      tabularType2 = new TabularType("typeName2", "description", rowType, indexNames);
      data = new TabularDataSupport(tabularType2);
      assertTrue("data should not be a value, they have different type names",
                 tabularType.isValue(data) == false);

      CompositeType rowType2 = new CompositeType("rowTypeName2", "rowDescription",
         itemNames, itemDescriptions, itemTypes);
      tabularType2 = new TabularType("typeName", "description", rowType2, indexNames);
      data = new TabularDataSupport(tabularType2);
      assertTrue("data should not be a value, they have different row types",
                 tabularType.isValue(data) == false);

      String[] indexNames2 = new String[] { "name2", "name1" };
      tabularType2 = new TabularType("typeName", "description", rowType, indexNames2);
      data = new TabularDataSupport(tabularType2);
      assertTrue("data should not be a value, they have different index names",
                 tabularType.isValue(data) == false);
   }

   public void testEquals()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      assertTrue("null is not equal to tabular type", tabularType.equals(null) == false);
      assertTrue("object is not a equal to tabular type", tabularType.equals(new Object()) == false);

      TabularType tabularType2 = new TabularType("typeName", "description", rowType, indexNames);
      assertTrue("Should be equal, even though the tabular type is a different instance",
                 tabularType.equals(tabularType2));
      assertTrue("Should be equal, even though the tabular type is a different instance",
                 tabularType2.equals(tabularType));

      tabularType2 = new TabularType("typeName2", "description", rowType, indexNames);
      assertTrue("should not be equal, they have different type names",
                 tabularType.equals(tabularType2) == false);
      assertTrue("should not be equal, they have different type names",
                 tabularType2.equals(tabularType) == false);

      CompositeType rowType2 = new CompositeType("rowTypeName2", "rowDescription",
         itemNames, itemDescriptions, itemTypes);
      tabularType2 = new TabularType("typeName", "description", rowType2, indexNames);
      assertTrue("should not be a equal, they have different row types",
                 tabularType.equals(tabularType2) == false);
      assertTrue("should not be a equal, they have different row types",
                 tabularType2.equals(tabularType) == false);

      String[] indexNames2 = new String[] { "name2", "name1" };
      tabularType2 = new TabularType("typeName", "description", rowType, indexNames2);
      assertTrue("should not be equal, they have different index names",
                 tabularType.equals(tabularType2) == false);
      assertTrue("should not be equal, they have different index names",
                 tabularType2.equals(tabularType) == false);
   }

   public void testHashCode()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      int myHashCode = "typeName".hashCode() + rowType.hashCode()
         + "name1".hashCode() + "name2".hashCode();
      assertTrue("Wrong hash code generated", myHashCode == tabularType.hashCode());
   }

   public void testToString()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      String toString = tabularType.toString();

      assertTrue("toString() should contain the tabular type class name",
         toString.indexOf(TabularType.class.getName()) != -1);
      assertTrue("toString() should contain the type name",
         toString.indexOf("typeName") != -1);
      assertTrue("toString() should contain the row type " + rowType,
         toString.indexOf(rowType.toString()) != -1);
      assertTrue("toString() should contain the index name1",
         toString.indexOf("name1") != -1);
      assertTrue("toString() should contain the index name2",
         toString.indexOf("name2") != -1);
   }

   public void testSerialization()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      // Serialize it
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(tabularType);
    
      // Deserialize it
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new ObjectInputStream(bais);
      Object result = ois.readObject();

      assertEquals(tabularType, result);
   }

   public void testErrors()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };

      boolean caught = false;
      try
      {
         TabularType tabularType = new TabularType(null, "description", rowType, indexNames);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for null type name");

      caught = false;
      try
      {
         TabularType tabularType = new TabularType("", "description", rowType, indexNames);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for empty type name");

      caught = false;
      try
      {
         TabularType tabularType = new TabularType("typeName", null, rowType, indexNames);
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
         TabularType tabularType = new TabularType("typeName", "", rowType, indexNames);
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
         TabularType tabularType = new TabularType("typeName", "description", null, indexNames);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for null row type");

      caught = false;
      try
      {
         TabularType tabularType = new TabularType("typeName", "description", rowType, null);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for null index names");

      caught = false;
      try
      {
         TabularType tabularType = new TabularType("typeName", "description", rowType, new String[0]);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for empty index names");

      caught = false;
      try
      {
         TabularType tabularType = new TabularType("typeName", "description", rowType, new String[] { "name1", null });
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for null index name element");

      caught = false;
      try
      {
         TabularType tabularType = new TabularType("typeName", "description", rowType, new String[] { "name1", "" });
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for empty index name element");

      caught = false;
      try
      {
         TabularType tabularType = new TabularType("typeName", "description", rowType, new String[] { "name1", "nameX" });
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected OpenDataException for invalid index name");
   }

   // Support -------------------------------------------------------------------
}
