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
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.management.openmbean.CompositeData;
import javax.management.openmbean.CompositeDataSupport;
import javax.management.openmbean.CompositeType;
import javax.management.openmbean.InvalidKeyException;
import javax.management.openmbean.InvalidOpenTypeException;
import javax.management.openmbean.KeyAlreadyExistsException;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;
import javax.management.openmbean.TabularData;
import javax.management.openmbean.TabularDataSupport;
import javax.management.openmbean.TabularType;

/**
 * Tabular data support tests.<p>
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class TabularDataSupportTestCase
  extends TestCase
{
   // Static --------------------------------------------------------------------

   // Attributes ----------------------------------------------------------------

   // Constructor ---------------------------------------------------------------

   /**
    * Construct the test
    */
   public TabularDataSupportTestCase(String s)
   {
      super(s);
   }

   // Tests ---------------------------------------------------------------------

   public void testTabularDataSupport()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      TabularDataSupport data = new TabularDataSupport(tabularType);
      TabularDataSupport data2 = new TabularDataSupport(tabularType, 100, .5f);
   }

   public void testGetTabularType()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      TabularDataSupport data = new TabularDataSupport(tabularType);
      assertTrue("Expected the same tabular type", data.getTabularType().equals(tabularType));
   }

   public void testCalculateIndex()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      TabularDataSupport data = new TabularDataSupport(tabularType);

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);
      Object[] index = data.calculateIndex(compData);

      assertTrue("Expected index element 0 to be value1", index[0].equals("value1"));
      assertTrue("Expected index element 1 to be 2", index[1].equals(new Integer(2)));

      map = new HashMap();
      map.put("name1", "value2");
      map.put("name2", new Integer(3));
      compData = new CompositeDataSupport(rowType, map);
      index = data.calculateIndex(compData);

      assertTrue("Expected index element 0 to be value2", index[0].equals("value2"));
      assertTrue("Expected index element 1 to be 3", index[1].equals(new Integer(3)));
   }

   public void testContainsKeyObject()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      TabularDataSupport data = new TabularDataSupport(tabularType);

      assertTrue("Didn't expect containsKey null", data.containsKey(null) == false);
      assertTrue("Didn't expect containsKey not an Object array", data.containsKey(new Object()) == false);

      Object[] index = new Object[] { "value1", new Integer(2) };
      assertTrue("Didn't expect containsKey on empty data", data.containsKey((Object) index) == false);

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(3));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);
      assertTrue("Didn't expect containsKey on index not present", data.containsKey((Object) index) == false);

      map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      compData = new CompositeDataSupport(rowType, map);
      data.put(compData);
      assertTrue("Expected containsKey", data.containsKey((Object) index));

      map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(3));
      compData = new CompositeDataSupport(rowType, map);
      assertTrue("Didn't expect containsKey on index still not present", 
         data.containsKey((Object) data.calculateIndex(compData)) == false);

      data.remove(index);
      assertTrue("Didn't expect removed data in containsKey", data.containsKey((Object) index) == false);
   }

   public void testContainsKeyObjectArray()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      TabularDataSupport data = new TabularDataSupport(tabularType);

      assertTrue("Didn't expect containsKey null", data.containsKey(null) == false);
      assertTrue("Didn't expect containsKey not an Object array", data.containsKey(new Object()) == false);

      Object[] index = new Object[] { "value1", new Integer(2) };
      assertTrue("Didn't expect containsKey on empty data", data.containsKey(index) == false);

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(3));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);
      assertTrue("Didn't expect containsKey on index not present", data.containsKey(index) == false);

      map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      compData = new CompositeDataSupport(rowType, map);
      data.put(compData);
      assertTrue("Expected containsKey", data.containsKey(index));

      map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(3));
      compData = new CompositeDataSupport(rowType, map);
      assertTrue("Didn't expect containsKey on index still not present",
         data.containsKey(data.calculateIndex(compData)) == false);

      data.remove(index);
      assertTrue("Didn't expect removed data in containsKey", data.containsKey(index) == false);
   }

   public void testContainsValueObject()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      TabularDataSupport data = new TabularDataSupport(tabularType);

      assertTrue("Didn't expect containsValue null", data.containsValue(null) == false);

      itemNames = new String[] { "name1", "name2" };
      itemDescriptions = new String[] { "desc1", "desc2" };
      itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType2 = new CompositeType("rowTypeName2", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport compData2 = new CompositeDataSupport(rowType2, map);

      assertTrue("Didn't expect containsValue wrong composite type", data.containsValue((Object) compData2) == false);

      map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(3));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);
      assertTrue("Didn't expect containsValue on data not present", data.containsValue((Object) compData) == false);

      map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      compData = new CompositeDataSupport(rowType, map);
      data.put(compData);
      assertTrue("Expected containsValue", data.containsValue((Object) compData));

      map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(3));
      compData = new CompositeDataSupport(rowType, map);
      assertTrue("Didn't expect containsValue on value still not present", data.containsValue((Object) compData) == false);

      assertTrue("Didn't expect containsValue still wrong composite type", data.containsValue((Object) compData2) == false);

      data.remove(data.calculateIndex(compData));
      assertTrue("Didn't expect removed data in containsValue", data.containsValue((Object) compData) == false);
   }

   public void testContainsValueCompositeData()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      TabularDataSupport data = new TabularDataSupport(tabularType);

      assertTrue("Didn't expect containsValue null", data.containsValue(null) == false);

      itemNames = new String[] { "name1", "name2" };
      itemDescriptions = new String[] { "desc1", "desc2" };
      itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType2 = new CompositeType("rowTypeName2", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport compData2 = new CompositeDataSupport(rowType2, map);

      assertTrue("Didn't expect containsValue wrong composite type", data.containsValue(compData2) == false);

      map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(3));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);
      assertTrue("Didn't expect containsValue on data not present", data.containsValue(compData) == false);

      map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      compData = new CompositeDataSupport(rowType, map);
      data.put(compData);
      assertTrue("Expected containsValue", data.containsValue(compData));

      map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(3));
      compData = new CompositeDataSupport(rowType, map);
      assertTrue("Didn't expect containsValue on value still not present", data.containsValue(compData) == false);

      assertTrue("Didn't expect containsValue still wrong composite type", data.containsValue(compData2) == false);

      data.remove(data.calculateIndex(compData));
      assertTrue("Didn't expect removed data in containsValue", data.containsValue(compData) == false);
   }

   public void testGetObject()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      TabularDataSupport data = new TabularDataSupport(tabularType);

      Object[] index = new Object[] { "value1", new Integer(3) };
      assertTrue("Expected null for get on data not present", data.get((Object) index) == null);

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);
      index = new Object[] { "value1", new Integer(2) };
      data.put(compData);
      assertTrue("Expected get to return the same value", data.get((Object) index).equals(compData));

      index = new Object[] { "value1", new Integer(3) };
      assertTrue("Didn't expect get on value still not present", data.get((Object) index) == null);

      index = new Object[] { "value1", new Integer(2) };
      data.remove(index);
      assertTrue("Didn't expect removed data in get", data.get((Object) index) == null);
   }

   public void testGetObjectArray()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      TabularDataSupport data = new TabularDataSupport(tabularType);

      Object[] index = new Object[] { "value1", new Integer(3) };
      assertTrue("Expected null for get on data not present", data.get(index) == null);

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);
      index = new Object[] { "value1", new Integer(2) };
      data.put(compData);
      assertTrue("Expected get to return the same value", data.get(index).equals(compData));

      index = new Object[] { "value1", new Integer(3) };
      assertTrue("Didn't expect get on value still not present", data.get(index) == null);

      index = new Object[] { "value1", new Integer(2) };
      data.remove(index);
      assertTrue("Didn't expect removed data in get", data.get(index) == null);
   }

   public void testPutObjectObject()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      TabularDataSupport data = new TabularDataSupport(tabularType);

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);
      Object[] index = new Object[] { "value1", new Integer(2) };
      data.put(index, (Object) compData);
      assertTrue("The data should be present after put", data.get(index).equals(compData));

      HashMap map2 = new HashMap();
      map2.put("name1", "value1");
      map2.put("name2", new Integer(3));
      CompositeDataSupport compData2 = new CompositeDataSupport(rowType, map2);
      index = new Object[] { "value1", new Integer(3) };
      data.put(index, (Object) compData2);
      assertTrue("Another data should be present after put", data.get(index).equals(compData2));

      index = new Object[] { "value1", new Integer(2) };
      assertTrue("The previous data should be present after put", data.get(index).equals(compData));

      data.remove(index);
      data.put(index, compData);
      assertTrue("Data should be present after remove/put", data.get(index).equals(compData));

      HashMap map3 = new HashMap();
      map3.put("name1", "value1");
      map3.put("name2", new Integer(4));
      CompositeDataSupport compData3 = new CompositeDataSupport(rowType, map3);
      index = new Object[] { "value1", new Integer(4) };
      data.put(new Object(), compData3);
      assertTrue("The key should be ignored in put", data.get(index).equals(compData3));
   }

   public void testPutCompositeData()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      TabularDataSupport data = new TabularDataSupport(tabularType);

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);
      Object[] index = new Object[] { "value1", new Integer(2) };
      data.put(compData);
      assertTrue("The data should be present after put", data.get(index).equals(compData));

      HashMap map2 = new HashMap();
      map2.put("name1", "value1");
      map2.put("name2", new Integer(3));
      CompositeDataSupport compData2 = new CompositeDataSupport(rowType, map2);
      index = new Object[] { "value1", new Integer(3) };
      data.put(compData2);
      assertTrue("Another data should be present after put", data.get(index).equals(compData2));

      index = new Object[] { "value1", new Integer(2) };
      assertTrue("The previous data should be present after put", data.get(index).equals(compData));

      data.remove(index);
      data.put(compData);
      assertTrue("Data should be present after remove/put", data.get(index).equals(compData));
   }

   public void testRemoveObject()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      TabularDataSupport data = new TabularDataSupport(tabularType);

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);
      Object[] index = new Object[] { "value1", new Integer(2) };

      assertTrue("Remove on data not present returns null", data.remove((Object) index) == null);

      data.put(compData);
      assertTrue("Remove on data present returns the data", data.remove((Object) index).equals(compData));
   }

   public void testRemoveObjectArray()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      TabularDataSupport data = new TabularDataSupport(tabularType);

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);
      Object[] index = new Object[] { "value1", new Integer(2) };

      assertTrue("Remove on data not present returns null", data.remove(index) == null);

      data.put(compData);
      assertTrue("Remove on data present returns the data", data.remove(index).equals(compData));
   }

   public void testPutAllMap()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      TabularDataSupport data = new TabularDataSupport(tabularType);

      data.putAll((Map) null);
      assertTrue("Put all null is ok", data.isEmpty());

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);

      HashMap toPut = new HashMap();
      toPut.put(new Object(), compData);
      data.putAll(toPut);
      assertTrue("Put all added one", data.size() == 1);
      assertTrue("Put all added the correct data", data.containsValue(compData));

      HashMap map2 = new HashMap();
      map2.put("name1", "value1");
      map2.put("name2", new Integer(3));
      CompositeDataSupport compData2 = new CompositeDataSupport(rowType, map2);

      HashMap map3 = new HashMap();
      map3.put("name1", "value1");
      map3.put("name2", new Integer(4));
      CompositeDataSupport compData3 = new CompositeDataSupport(rowType, map3);

      toPut = new HashMap();
      toPut.put(new Object(), compData2);
      toPut.put(new Object(), compData3);
      data.putAll(toPut);
      assertTrue("Put all added two", data.size() == 3);
      assertTrue("Put all added the correct data", data.containsValue(compData2));
      assertTrue("Put all added the correct data", data.containsValue(compData3));
      assertTrue("Put all original data still present", data.containsValue(compData));
   }

   public void testPutAllCompositeData()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      TabularDataSupport data = new TabularDataSupport(tabularType);

      data.putAll((CompositeData[]) null);
      assertTrue("Put all null is ok", data.isEmpty());

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);

      CompositeData[] toPut = new CompositeData[] { compData };
      data.putAll(toPut);
      assertTrue("Put all added one", data.size() == 1);
      assertTrue("Put all added the correct data", data.containsValue(compData));

      HashMap map2 = new HashMap();
      map2.put("name1", "value1");
      map2.put("name2", new Integer(3));
      CompositeDataSupport compData2 = new CompositeDataSupport(rowType, map2);

      HashMap map3 = new HashMap();
      map3.put("name1", "value1");
      map3.put("name2", new Integer(4));
      CompositeDataSupport compData3 = new CompositeDataSupport(rowType, map3);

      toPut = new CompositeData[] { compData2, compData3 };
      data.putAll(toPut);
      assertTrue("Put all added two", data.size() == 3);
      assertTrue("Put all added the correct data", data.containsValue(compData2));
      assertTrue("Put all added the correct data", data.containsValue(compData3));
      assertTrue("Put all original data still present", data.containsValue(compData));
   }

   public void testClear()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      TabularDataSupport data = new TabularDataSupport(tabularType);

      data.putAll((CompositeData[]) null);
      assertTrue("Put all null is ok", data.isEmpty());

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);

      HashMap map2 = new HashMap();
      map2.put("name1", "value1");
      map2.put("name2", new Integer(3));
      CompositeDataSupport compData2 = new CompositeDataSupport(rowType, map2);

      HashMap map3 = new HashMap();
      map3.put("name1", "value1");
      map3.put("name2", new Integer(4));
      CompositeDataSupport compData3 = new CompositeDataSupport(rowType, map3);

      HashMap toPut = new HashMap();
      toPut.put(new Object(), compData);
      toPut.put(new Object(), compData2);
      toPut.put(new Object(), compData3);
      data.putAll(toPut);

      data.clear();
      assertTrue("Clear should clear the data", data.isEmpty());
   }

   public void testSize()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      TabularDataSupport data = new TabularDataSupport(tabularType);

      assertTrue("Initial size is zero", data.size() == 0);

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);

      CompositeData[] toPut = new CompositeData[] { compData };
      data.putAll(toPut);
      assertTrue("Expected one element", data.size() == 1);

      HashMap map2 = new HashMap();
      map2.put("name1", "value1");
      map2.put("name2", new Integer(3));
      CompositeDataSupport compData2 = new CompositeDataSupport(rowType, map2);

      HashMap map3 = new HashMap();
      map3.put("name1", "value1");
      map3.put("name2", new Integer(4));
      CompositeDataSupport compData3 = new CompositeDataSupport(rowType, map3);

      toPut = new CompositeData[] { compData2, compData3 };
      data.putAll(toPut);
      assertTrue("Expected three elements", data.size() == 3);

      data.remove(new Object[] { "value1", new Integer(4) });
      assertTrue("Expected two elements", data.size() == 2);

      data.clear();
      assertTrue("Expected no elements", data.size() == 0);
   }

   public void testIsEmpty()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      TabularDataSupport data = new TabularDataSupport(tabularType);

      assertTrue("Initially empty", data.isEmpty());

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);

      CompositeData[] toPut = new CompositeData[] { compData };
      data.putAll(toPut);
      assertTrue("Not empty after a put", data.isEmpty() == false);

      data.clear();
      assertTrue("Expected no elements", data.isEmpty());
   }

   /**
    * @todo full test, unmodifiable/iterator
    */
   public void testKeySet()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      TabularDataSupport data = new TabularDataSupport(tabularType);

      data.putAll((CompositeData[]) null);
      assertTrue("Put all null is ok", data.isEmpty());

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);

      HashMap map2 = new HashMap();
      map2.put("name1", "value1");
      map2.put("name2", new Integer(3));
      CompositeDataSupport compData2 = new CompositeDataSupport(rowType, map2);

      HashMap map3 = new HashMap();
      map3.put("name1", "value1");
      map3.put("name2", new Integer(4));
      CompositeDataSupport compData3 = new CompositeDataSupport(rowType, map3);

      HashMap toPut = new HashMap();
      toPut.put(new Object(), compData);
      toPut.put(new Object(), compData2);
      toPut.put(new Object(), compData3);
      data.putAll(toPut);

      Set keySet = data.keySet();
      assertTrue("Key set should contain 3 elements", keySet.size() == 3);
      assertTrue("Key set should contain index [value1, 2]", 
         keySet.contains(Arrays.asList(new Object[] { "value1", new Integer(2) })));
      assertTrue("Key set should contain index [value1, 3]", 
         keySet.contains(Arrays.asList(new Object[] { "value1", new Integer(3) })));
      assertTrue("Key set should contain index [value1, 4]", 
         keySet.contains(Arrays.asList(new Object[] { "value1", new Integer(4) })));
   }

   /**
    * @todo full test, modifiable/iterator
    */
   public void testValues()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      TabularDataSupport data = new TabularDataSupport(tabularType);

      data.putAll((CompositeData[]) null);
      assertTrue("Put all null is ok", data.isEmpty());

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);

      HashMap map2 = new HashMap();
      map2.put("name1", "value1");
      map2.put("name2", new Integer(3));
      CompositeDataSupport compData2 = new CompositeDataSupport(rowType, map2);

      HashMap map3 = new HashMap();
      map3.put("name1", "value1");
      map3.put("name2", new Integer(4));
      CompositeDataSupport compData3 = new CompositeDataSupport(rowType, map3);

      HashMap toPut = new HashMap();
      toPut.put(new Object(), compData);
      toPut.put(new Object(), compData2);
      toPut.put(new Object(), compData3);
      data.putAll(toPut);

      Collection values = data.values();
      assertTrue("Values should contain 3 elements", values.size() == 3);
      assertTrue("Values should contain index compData", values.contains(compData));
      assertTrue("Values should contain index compData2", values.contains(compData2));
      assertTrue("Values should contain index compData3", values.contains(compData3));
   }

   /**
    * @todo this test
    */
   public void testEntrySet()
      throws Exception
   {
   }

   public void testClone()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      TabularDataSupport data = new TabularDataSupport(tabularType);

      data.putAll((CompositeData[]) null);
      assertTrue("Put all null is ok", data.isEmpty());

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);

      HashMap map2 = new HashMap();
      map2.put("name1", "value1");
      map2.put("name2", new Integer(3));
      CompositeDataSupport compData2 = new CompositeDataSupport(rowType, map2);

      HashMap map3 = new HashMap();
      map3.put("name1", "value1");
      map3.put("name2", new Integer(4));
      CompositeDataSupport compData3 = new CompositeDataSupport(rowType, map3);

      HashMap toPut = new HashMap();
      toPut.put(new Object(), compData);
      toPut.put(new Object(), compData2);
      toPut.put(new Object(), compData3);
      data.putAll(toPut);

      TabularDataSupport clone = (TabularDataSupport) data.clone();
      assertTrue("Clone should have the same tabular type", data.getTabularType().equals(clone.getTabularType()));
      assertTrue("Clone should have the same number of elements", data.size() == clone.size());
      CompositeData compDataClone = clone.get(new Object[] {"value1", new Integer(2) });
      assertTrue("Should be a shallow clone", compData == compDataClone);
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

      TabularDataSupport data = new TabularDataSupport(tabularType);

      assertTrue("Null should not be equal", data.equals(null) == false);
      assertTrue("Only TabularData should be equal", data.equals(new Object()) == false);

      assertTrue("An instance should equal itself", data.equals(data));

      TabularDataSupport data2 = new TabularDataSupport(tabularType);

      assertTrue("Two different instances with the same tabular type are equal", data.equals(data2));
      assertTrue("Two different instances with the same tabular type are equal", data2.equals(data));

      TabularType tabularType2 = new TabularType("typeName2", "description", rowType, indexNames);
      data2 = new TabularDataSupport(tabularType2);

      assertTrue("Instances with different tabular type are not equal", data.equals(data2) == false);
      assertTrue("Instances with different tabular type are not equal", data2.equals(data) == false);

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);

      HashMap map2 = new HashMap();
      map2.put("name1", "value1");
      map2.put("name2", new Integer(3));
      CompositeDataSupport compData2 = new CompositeDataSupport(rowType, map2);

      HashMap map3 = new HashMap();
      map3.put("name1", "value1");
      map3.put("name2", new Integer(4));
      CompositeDataSupport compData3 = new CompositeDataSupport(rowType, map3);

      HashMap toPut = new HashMap();
      toPut.put(new Object(), compData);
      toPut.put(new Object(), compData2);
      toPut.put(new Object(), compData3);

      data.putAll(toPut);
      data2 = new TabularDataSupport(tabularType);
      data2.putAll(toPut);
      assertTrue("Instances with the same composite data are equal", data.equals(data2));
      assertTrue("Instances with the same composite data are equal", data2.equals(data));

      toPut = new HashMap();
      toPut.put(new Object(), compData);
      toPut.put(new Object(), compData2);
      data2 = new TabularDataSupport(tabularType);
      data2.putAll(toPut);
      assertTrue("Instances with different composite data are not equal", data.equals(data2) == false);
      assertTrue("Instances with different composite data are not equal", data2.equals(data) == false);
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

      TabularDataSupport data = new TabularDataSupport(tabularType);

      data.putAll((CompositeData[]) null);
      assertTrue("Put all null is ok", data.isEmpty());

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);

      HashMap map2 = new HashMap();
      map2.put("name1", "value1");
      map2.put("name2", new Integer(3));
      CompositeDataSupport compData2 = new CompositeDataSupport(rowType, map2);

      HashMap map3 = new HashMap();
      map3.put("name1", "value1");
      map3.put("name2", new Integer(4));
      CompositeDataSupport compData3 = new CompositeDataSupport(rowType, map3);

      HashMap toPut = new HashMap();
      toPut.put(new Object(), compData);
      toPut.put(new Object(), compData2);
      toPut.put(new Object(), compData3);
      data.putAll(toPut);

      int myHashCode = tabularType.hashCode() + compData.hashCode() + compData2.hashCode() + compData3.hashCode();
      assertTrue("Wrong hash code generated", myHashCode == data.hashCode());
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

      TabularDataSupport data = new TabularDataSupport(tabularType);

      data.putAll((CompositeData[]) null);
      assertTrue("Put all null is ok", data.isEmpty());

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);

      HashMap map2 = new HashMap();
      map2.put("name1", "value1");
      map2.put("name2", new Integer(3));
      CompositeDataSupport compData2 = new CompositeDataSupport(rowType, map2);

      HashMap map3 = new HashMap();
      map3.put("name1", "value1");
      map3.put("name2", new Integer(4));
      CompositeDataSupport compData3 = new CompositeDataSupport(rowType, map3);

      HashMap toPut = new HashMap();
      toPut.put(new Object(), compData);
      toPut.put(new Object(), compData2);
      toPut.put(new Object(), compData3);
      data.putAll(toPut);

      String toString = data.toString();

      assertTrue("toString() should contain the tabular type",
         toString.indexOf(tabularType.toString()) != -1);
      assertTrue("toString() should contain index=compositeData for compData",
         toString.indexOf(Arrays.asList(data.calculateIndex(compData)) + "=" + compData) != -1);
      assertTrue("toString() should contain index=compositeData for compData2",
         toString.indexOf(Arrays.asList(data.calculateIndex(compData2)) + "=" + compData2) != -1);
      assertTrue("toString() should contain index=compositeData for compData3",
         toString.indexOf(Arrays.asList(data.calculateIndex(compData3)) + "=" + compData3) != -1);
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

      TabularDataSupport data = new TabularDataSupport(tabularType);

      data.putAll((CompositeData[]) null);
      assertTrue("Put all null is ok", data.isEmpty());

      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);

      HashMap map2 = new HashMap();
      map2.put("name1", "value1");
      map2.put("name2", new Integer(3));
      CompositeDataSupport compData2 = new CompositeDataSupport(rowType, map2);

      HashMap map3 = new HashMap();
      map3.put("name1", "value1");
      map3.put("name2", new Integer(4));
      CompositeDataSupport compData3 = new CompositeDataSupport(rowType, map3);

      HashMap toPut = new HashMap();
      toPut.put(new Object(), compData);
      toPut.put(new Object(), compData2);
      toPut.put(new Object(), compData3);
      data.putAll(toPut);

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

   public void testErrors()
      throws Exception
   {
      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };

      CompositeType rowType = new CompositeType("rowTypeName", "rowDescription",
         itemNames, itemDescriptions, itemTypes);
      HashMap map = new HashMap();
      map.put("name1", "value1");
      map.put("name2", new Integer(2));
      CompositeDataSupport compData = new CompositeDataSupport(rowType, map);

      CompositeType rowType2 = new CompositeType("rowTypeName2", "rowDescription",
         itemNames, itemDescriptions, itemTypes);
      CompositeDataSupport compData2 = new CompositeDataSupport(rowType2, map);

      String[] indexNames = new String[] { "name1", "name2" };
      TabularType tabularType = new TabularType("typeName", "description", rowType, indexNames);

      boolean caught = false;
      try
      {
         new TabularDataSupport(null);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for null tabular type");

      caught = false;
      try
      {
         new TabularDataSupport(null, 10, .5f);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for null tabular type");

      caught = false;
      try
      {
         new TabularDataSupport(tabularType, -1, .5f);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for negative initial capacity");

      caught = false;
      try
      {
         new TabularDataSupport(tabularType, 10, 0f);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for zero load factor");

      caught = false;
      try
      {
         new TabularDataSupport(tabularType, 10, -0.5f);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected IllegalArgumentException for negative load factor");

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         data.calculateIndex(null);
      }
      catch (NullPointerException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected NullPointerException for calculate index on null object");

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         data.calculateIndex(compData2);
      }
      catch (InvalidOpenTypeException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected InvalidOpenTypeException for calculate index on wrong composite type");

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         data.get((Object) null);
      }
      catch (NullPointerException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected NullPointerException for get((Object) null)");

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         data.get(new Object());
      }
      catch (ClassCastException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected ClassCastException for get(new Object())");

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         data.get((Object) new Object[] { "wrong" });
      }
      catch (InvalidKeyException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected InvalidKeyException for get(Object) wrong");

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         data.get((Object[]) null);
      }
      catch (NullPointerException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected NullPointerException for get((Object[]) null)");

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         data.get(new Object[] { "wrong" });
      }
      catch (InvalidKeyException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected InvalidKeyException for get(Object[]) wrong");

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         data.put(new Object(), null);
      }
      catch (NullPointerException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected NullPointerException for put(Object, Object) with null value");

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         data.put(new Object(), new Object());
      }
      catch (ClassCastException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected ClassCastException for put(Object, Object) with none CompositeData");

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         data.put(new Object(), compData2);
      }
      catch (InvalidOpenTypeException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected InvalidOpenTypeException for put(Object, Object) with wrong CompositeType");

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         data.put(new Object(), compData);
         data.put(new Object(), compData);
      }
      catch (KeyAlreadyExistsException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected KeyAlreadyExistsException for put(Object, Object)");

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         data.put(null);
      }
      catch (NullPointerException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected NullPointerException for put(CompositeData) with null value");

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         data.put(compData2);
      }
      catch (InvalidOpenTypeException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected InvalidOpenTypeException for put(CompositeData) with wrong CompositeType");

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         data.put(compData);
         data.put(compData);
      }
      catch (KeyAlreadyExistsException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected KeyAlreadyExistsException for put(CompositeData)");

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         data.remove((Object) null);
      }
      catch (NullPointerException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected NullPointerException for remove((Object) null)");

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         data.remove(new Object());
      }
      catch (ClassCastException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected ClassCastException for remove(new Object())");

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         data.remove((Object) new Object[] { "wrong" });
      }
      catch (InvalidKeyException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected InvalidKeyException for remove(Object) wrong");

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         data.remove((Object[]) null);
      }
      catch (NullPointerException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected NullPointerException for remove((Object[]) null)");

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         data.remove(new Object[] { "wrong" });
      }
      catch (InvalidKeyException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected InvalidKeyException for remove(Object[]) wrong");

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         HashMap toPut = new HashMap();
         toPut.put(new Object(), compData);
         toPut.put(new Object(), null);
         data.putAll(toPut);
      }
      catch (NullPointerException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected NullPointerException for putAll(Map) null");

      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         HashMap toPut = new HashMap();
         toPut.put(new Object(), compData);
         toPut.put(new Object(), null);
         try
         {
            data.putAll(toPut);
         }
         catch (NullPointerException expected)
         {
         }
         assertTrue("Nothing should be added for NullPointerException putAll(Map)", data.isEmpty());
      }

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         HashMap toPut = new HashMap();
         toPut.put(new Object(), compData);
         toPut.put(new Object(), new Object());
         data.putAll(toPut);
      }
      catch (ClassCastException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected ClassCastException for putAll(Map) non composite data");

      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         HashMap toPut = new HashMap();
         toPut.put(new Object(), compData);
         toPut.put(new Object(), new Object());
         try
         {
            data.putAll(toPut);
         }
         catch (ClassCastException expected)
         {
         }
         assertTrue("Nothing should be added for ClassCastException putAll(Map)", data.isEmpty());
      }

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         HashMap toPut = new HashMap();
         toPut.put(new Object(), compData);
         toPut.put(new Object(), compData2);
         data.putAll(toPut);
      }
      catch (InvalidOpenTypeException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected InvalidOpenTypeException for putAll(Map) wrong composite type");

      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         HashMap toPut = new HashMap();
         toPut.put(new Object(), compData);
         toPut.put(new Object(), compData2);
         try
         {
            data.putAll(toPut);
         }
         catch (InvalidOpenTypeException expected)
         {
         }
         assertTrue("Nothing should be added for InvalidOpenTypeException putAll(Map)", data.isEmpty());
      }

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         HashMap toPut = new HashMap();
         toPut.put(new Object(), compData);
         toPut.put(new Object(), compData);
         data.putAll(toPut);
      }
      catch (KeyAlreadyExistsException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected InvalidOpenTypeException for putAll(Map) with duplicate data");

      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         HashMap toPut = new HashMap();
         toPut.put(new Object(), compData);
         toPut.put(new Object(), compData);
         try
         {
            data.putAll(toPut);
         }
         catch (KeyAlreadyExistsException expected)
         {
         }
         assertTrue("Nothing should be added for KeyAlreadyExistsException duplicates putAll(Map)", data.isEmpty());
      }

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         HashMap toPut = new HashMap();
         toPut.put(new Object(), compData);
         data.putAll(toPut);
         toPut = new HashMap();
         toPut.put(new Object(), compData);
         data.putAll(toPut);
      }
      catch (KeyAlreadyExistsException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected InvalidOpenTypeException for putAll(Map) adding a duplicate");

      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         HashMap toPut = new HashMap();
         toPut.put(new Object(), compData);
         data.putAll(toPut);
         toPut = new HashMap();
         toPut.put(new Object(), compData);
         try
         {
            data.putAll(toPut);
         }
         catch (KeyAlreadyExistsException expected)
         {
         }
         assertTrue("Nothing should be added for KeyAlreadyExistsException already put putAll(Map)", data.size() == 1);
      }

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         CompositeData[] toPut = new CompositeData[] { compData, null };
         data.putAll(toPut);
      }
      catch (NullPointerException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected NullPointerException for putAll(CompositeData[]) null");

      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         CompositeData[] toPut = new CompositeData[] { compData, null };
         try
         {
            data.putAll(toPut);
         }
         catch (NullPointerException expected)
         {
         }
         assertTrue("Nothing should be added for NullPointerException putAll(CompositeData[])", data.isEmpty());
      }

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         CompositeData[] toPut = new CompositeData[] { compData, compData2 };
         data.putAll(toPut);
      }
      catch (InvalidOpenTypeException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected InvalidOpenTypeException for putAll(CompositeData[]) wrong composite type");

      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         CompositeData[] toPut = new CompositeData[] { compData, compData2 };
         try
         {
            data.putAll(toPut);
         }
         catch (InvalidOpenTypeException expected)
         {
         }
         assertTrue("Nothing should be added for InvalidOpenTypeException putAll(CompositeData[])", data.isEmpty());
      }

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         CompositeData[] toPut = new CompositeData[] { compData, compData };
         data.putAll(toPut);
      }
      catch (KeyAlreadyExistsException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected InvalidOpenTypeException for putAll(CompositeData[]) with duplicate data");

      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         CompositeData[] toPut = new CompositeData[] { compData, compData };
         try
         {
            data.putAll(toPut);
         }
         catch (KeyAlreadyExistsException expected)
         {
         }
         assertTrue("Nothing should be added for KeyAlreadyExistsException duplicates putAll(CompositeData[])", data.isEmpty());
      }

      caught = false;
      try
      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         CompositeData[] toPut = new CompositeData[] { compData };
         data.putAll(toPut);
         data.putAll(toPut);
      }
      catch (KeyAlreadyExistsException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Expected InvalidOpenTypeException for putAll(CompositeData[]) adding a duplicate");

      {
         TabularDataSupport data = new TabularDataSupport(tabularType);
         CompositeData[] toPut = new CompositeData[] { compData };
         data.putAll(toPut);
         try
         {
            data.putAll(toPut);
         }
         catch (KeyAlreadyExistsException expected)
         {
         }
         assertTrue("Nothing should be added for KeyAlreadyExistsException already put putAll(CompositeData[])", data.size() == 1);
      }
   }
}
