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
import javax.management.openmbean.CompositeDataSupport;
import javax.management.openmbean.CompositeType;
import javax.management.openmbean.OpenDataException;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;
import javax.management.openmbean.TabularType;
import javax.management.openmbean.TabularDataSupport;

/**
 * Array type tests.<p>
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class ArrayTypeTestCase
  extends TestCase
{
   // Static --------------------------------------------------------------------

   // Attributes ----------------------------------------------------------------

   // Constructor ---------------------------------------------------------------

   /**
    * Construct the test
    */
   public ArrayTypeTestCase(String s)
   {
      super(s);
   }

   // Tests ---------------------------------------------------------------------

   public void testArrayTypeOpenType()
      throws Exception
   {
      ArrayType arrayType = new ArrayType(3, SimpleType.STRING);
      assertEquals("[[[Ljava.lang.String;", arrayType.getClassName());
      assertEquals("3-dimension array of java.lang.String", arrayType.getDescription());
      assertEquals("[[[Ljava.lang.String;", arrayType.getTypeName());
      assertTrue("Composite type should be an array", arrayType.isArray());
   }

   public void testGetDimension()
      throws Exception
   {
      ArrayType arrayType = new ArrayType(3, SimpleType.STRING);
      assertTrue("Dimension should be 3", arrayType.getDimension() == 3);
   }

   public void testElementOpenType()
      throws Exception
   {
      ArrayType arrayType = new ArrayType(3, SimpleType.STRING);
      assertTrue("Element OpenType should be " + SimpleType.STRING, arrayType.getElementOpenType().equals(SimpleType.STRING));
   }

   public void testIsValue()
      throws Exception
   {
      ArrayType arrayType = new ArrayType(3, SimpleType.STRING);

      assertTrue("null is not a value of array type", arrayType.isValue(null) == false);
      assertTrue("object is not a value of array type", arrayType.isValue(new Object()) == false);

      String[][][] data = new String[1][2][3];
      assertTrue("data should be a value of array type", arrayType.isValue(data));

      String[][] data2 = new String[1][2];
      assertTrue("data should not be a value of array type, wrong number of dimensions", arrayType.isValue(data2) == false);

      Object[][][] data3 = new Object[1][2][3];
      assertTrue("data should not be a value of array type, wrong element type", arrayType.isValue(data3) == false);

      String[] itemNames = new String[] { "name1", "name2" };
      String[] itemDescriptions = new String[] { "desc1", "desc2" };
      OpenType[] itemTypes = new OpenType[] { SimpleType.STRING, SimpleType.INTEGER };
      CompositeType compositeType = new CompositeType("typeName", "description",
         itemNames, itemDescriptions, itemTypes);
      Object[] itemValues = new Object[] { "string", new Integer(1) };
      CompositeDataSupport cds = new CompositeDataSupport(compositeType, itemNames, itemValues);
      CompositeDataSupport[][] compData1 = new CompositeDataSupport[][]
      {
         { cds, null }, { cds, cds }
      };

      ArrayType compArrayType1 = new ArrayType(2, compositeType);
      assertTrue("compData1 should be a value of array type", compArrayType1.isValue(compData1));

      ArrayType compArrayType2 = new ArrayType(1, compositeType);
      assertTrue("compData1 should not be a value of array type, wrong dimension", compArrayType2.isValue(compData1) == false);

      CompositeType compositeType2 = new CompositeType("typeName2", "description",
         itemNames, itemDescriptions, itemTypes);
      ArrayType compArrayType3 = new ArrayType(2, compositeType2);
      assertTrue("compData1 should not be a value of array type, wrong element type", compArrayType3.isValue(compData1) == false);

      TabularType tabularType = new TabularType("typeName", "description", compositeType, new String[] { "name1" });
      TabularDataSupport tds = new TabularDataSupport(tabularType);
      TabularDataSupport[][] tabData1 = new TabularDataSupport[][]
      {
         { tds, null }, { tds, tds }
      };

      ArrayType tabArrayType1 = new ArrayType(2, tabularType);
      assertTrue("tabData1 should be a value of array type", tabArrayType1.isValue(tabData1));

      ArrayType tabArrayType2 = new ArrayType(1, tabularType);
      assertTrue("tabData1 should not be a value of array type, wrong number of dimensions", tabArrayType2.isValue(tabData1) == false);

      TabularType tabularType2 = new TabularType("typeName2", "description", compositeType, new String[] { "name1" });
      ArrayType tabArrayType3 = new ArrayType(2, tabularType2);
      assertTrue("tabData1 should not be a value of array type, wrong element type", tabArrayType3.isValue(tabData1) == false);
   }

   public void testEquals()
      throws Exception
   {
      ArrayType arrayType = new ArrayType(3, SimpleType.STRING);
      assertTrue("null is not an array type", arrayType.equals(null) == false);
      assertTrue("object is not an array type", arrayType.equals(new Object()) == false);

      assertTrue("should be equal", arrayType.equals(arrayType));

      ArrayType arrayType2 = new ArrayType(3, SimpleType.STRING);
      assertTrue("should be equal, even though different instances", arrayType.equals(arrayType2));
      assertTrue("should be equal, even though different instances", arrayType2.equals(arrayType));

      arrayType2 = new ArrayType(2, SimpleType.STRING);
      assertTrue("should not be equal, wrong number of dimensions", arrayType.equals(arrayType2) == false);
      assertTrue("should not be equal, wrong number of dimensions", arrayType2.equals(arrayType) == false);

      arrayType2 = new ArrayType(3, SimpleType.INTEGER);
      assertTrue("should not be equal, wrong element type", arrayType.equals(arrayType2) == false);
      assertTrue("should not be equal, wrong element type", arrayType2.equals(arrayType) == false);
   }

   public void testHashCode()
      throws Exception
   {
      ArrayType arrayType = new ArrayType(3, SimpleType.STRING);

      int myHashCode = 3 + SimpleType.STRING.hashCode();
      assertTrue("Wrong hash code generated", myHashCode == arrayType.hashCode());
   }

   public void testToString()
      throws Exception
   {
      ArrayType arrayType = new ArrayType(3, SimpleType.STRING);

      String toString = arrayType.toString();

      assertTrue("toString() should contain the array type class name",
         toString.indexOf(ArrayType.class.getName()) != -1);
      assertTrue("toString() should contain the dimension",
         toString.indexOf("3") != -1);
      assertTrue("toString() should contain the element type",
         toString.indexOf(SimpleType.STRING.toString()) != -1);
   }

   public void testSerialization()
      throws Exception
   {
      ArrayType arrayType = new ArrayType(3, SimpleType.STRING);

      // Serialize it
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(arrayType);
    
      // Deserialize it
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new ObjectInputStream(bais);
      Object result = ois.readObject();

      assertEquals(arrayType, result);
   }

   public void testErrors()
      throws Exception
   {
      boolean caught = false;
      try
      {
         new ArrayType(-1, SimpleType.STRING);
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for negative dimension");

      caught = false;
      try
      {
         new ArrayType(1, new ArrayType(2, SimpleType.STRING));
      }
      catch (OpenDataException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted OpenDataException for ArrayType element type");
   }

   public void testErrors2()
      throws Exception
   {
      boolean caught = false;
      try
      {
         new ArrayType(1, null);
      }
      catch (NullPointerException e)
      {
         fail("FAILS IN RI: expected IllegalArgumentException for null element type");
      }
      catch (IllegalArgumentException e)
      {
         caught = true;
      }
      if (caught == false)
         fail("Excepted IllegalArgumentException for null element type");
   }
}
