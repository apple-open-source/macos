/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.modelmbean;

import junit.framework.TestCase;
import junit.framework.AssertionFailedError;

import test.compliance.modelmbean.support.Resource;

import javax.management.*;
import javax.management.modelmbean.*;


public class ModelMBeanInfoSupportTEST extends TestCase
{
   public ModelMBeanInfoSupportTEST(String s)
   {
      super(s);
   }

   public void testSetDescriptors() throws Exception
   {
      final boolean READABLE = true;
      final boolean WRITABLE = true;
      final boolean ISIS     = true;
      
      RequiredModelMBean mbean = new RequiredModelMBean();
      
      ModelMBeanAttributeInfo attr1 = new ModelMBeanAttributeInfo(
            "Kissa",
            String.class.getName(),
            "Some attribute description",
            !READABLE, !WRITABLE, !ISIS
      );
      
      ModelMBeanAttributeInfo attr2 = new ModelMBeanAttributeInfo(
            "Koira",
            String.class.getName(),
            "Another attribute description",
            !READABLE, !WRITABLE, !ISIS
      );
      
      ModelMBeanConstructorInfo constr1 = new ModelMBeanConstructorInfo(
            "First Constructor",
            "Description of the first constructor",
            null
      );
      
      ModelMBeanConstructorInfo constr2 = new ModelMBeanConstructorInfo(
            "Second Constructor",
            "Description of the second constructor",
            null
      );
      
      ModelMBeanConstructorInfo constr3 = new ModelMBeanConstructorInfo(
            "Third Constructor",
            "Description of the 3rd constructor",
            null
      );
      
      ModelMBeanOperationInfo operation = new ModelMBeanOperationInfo(
            "AnOperation",
            "The description",
            null,
            null,
            MBeanOperationInfo.ACTION
      );
      
      ModelMBeanInfoSupport info = new ModelMBeanInfoSupport(
            mbean.getClass().getName(),
            "some description",
            new ModelMBeanAttributeInfo[]    { attr1, attr2 },
            new ModelMBeanConstructorInfo[]  { constr1, constr2, constr3 },
            new ModelMBeanOperationInfo[]    { operation },
            null
      );
            
      Descriptor descr1 = info.getDescriptor("Second Constructor", "constructor");
      
      assertTrue(descr1.getFieldValue("name").equals("Second Constructor"));
      assertTrue(descr1.getFieldValue("role").equals("constructor"));
      
      Descriptor descr2 = null;
      
      Descriptor[] descr3 = info.getDescriptors("operation");
      
      assertTrue(descr3[0].getFieldValue("descriptorType").equals("operation"));
      assertTrue(descr3[0].getFieldValue("name").equals("AnOperation"));
      
      descr1.setField("someField", "someValue");
      descr3[0].setField("Yksi", "Kaksi");
      
      info.setDescriptors(new Descriptor[] { descr1, descr2, descr3[0] });
      
      descr1 = info.getDescriptor("Second Constructor", "constructor");
      assertTrue(descr1.getFieldValue("name").equals("Second Constructor"));
      assertTrue(descr1.getFieldValue("role").equals("constructor"));
      assertTrue(descr1.getFieldValue("someField").equals("someValue"));
      
      descr1 = info.getDescriptor("AnOperation", "operation");
      
      assertTrue(descr1.getFieldValue("name").equals("AnOperation"));
      assertTrue(descr1.getFieldValue("Yksi").equals("Kaksi"));
      
   }
   
   public void testGetDescriptor() throws Exception
   {
      final boolean READABLE = true;
      final boolean WRITABLE = true;
      final boolean ISIS     = true;
      
      RequiredModelMBean mbean = new RequiredModelMBean();
      
      ModelMBeanAttributeInfo attr1 = new ModelMBeanAttributeInfo(
            "Kissa",
            String.class.getName(),
            "Some attribute description",
            !READABLE, !WRITABLE, !ISIS
      );
      
      ModelMBeanAttributeInfo attr2 = new ModelMBeanAttributeInfo(
            "Koira",
            String.class.getName(),
            "Another attribute description",
            !READABLE, !WRITABLE, !ISIS
      );
      
      ModelMBeanConstructorInfo constr1 = new ModelMBeanConstructorInfo(
            "First Constructor",
            "Description of the first constructor",
            null
      );
      
      ModelMBeanConstructorInfo constr2 = new ModelMBeanConstructorInfo(
            "Second Constructor",
            "Description of the second constructor",
            null
      );
      
      ModelMBeanConstructorInfo constr3 = new ModelMBeanConstructorInfo(
            "Third Constructor",
            "Description of the 3rd constructor",
            null
      );
      
      ModelMBeanOperationInfo operation = new ModelMBeanOperationInfo(
            "AnOperation",
            "The description",
            null,
            null,
            MBeanOperationInfo.ACTION
      );
      
      ModelMBeanInfoSupport info = new ModelMBeanInfoSupport(
            mbean.getClass().getName(),
            "some description",
            new ModelMBeanAttributeInfo[]    { attr1, attr2 },
            new ModelMBeanConstructorInfo[]  { constr1, constr2, constr3 },
            new ModelMBeanOperationInfo[]    { operation },
            null
      );

      Descriptor descr = info.getDescriptor("Second Constructor", "constructor");

      try
      {
         assertTrue(descr.getFieldValue("descriptorType").equals("operation"));
      }
      catch (AssertionFailedError e) 
      {
         throw new AssertionFailedError(
               "FAILS IN JBOSSMX: We incorrectly return descriptor type " +
               "'constructor' here -- should be 'operation'"
         );
      }
      
   }
   
   
   public void testClone() throws Exception 
   {
      final boolean READABLE = true;
      final boolean WRITABLE = true;
      final boolean ISIS     = true;
      
      RequiredModelMBean mbean = new RequiredModelMBean();
      
      ModelMBeanAttributeInfo attr1 = new ModelMBeanAttributeInfo(
            "Kissa",
            String.class.getName(),
            "Some attribute description",
            !READABLE, !WRITABLE, !ISIS
      );
      
      ModelMBeanAttributeInfo attr2 = new ModelMBeanAttributeInfo(
            "Koira",
            String.class.getName(),
            "Another attribute description",
            !READABLE, !WRITABLE, !ISIS
      );
      
      ModelMBeanConstructorInfo constr1 = new ModelMBeanConstructorInfo(
            "First Constructor",
            "Description of the first constructor",
            null
      );
      
      ModelMBeanConstructorInfo constr2 = new ModelMBeanConstructorInfo(
            "Second Constructor",
            "Description of the second constructor",
            null
      );
      
      ModelMBeanConstructorInfo constr3 = new ModelMBeanConstructorInfo(
            "Third Constructor",
            "Description of the 3rd constructor",
            null
      );
      
      ModelMBeanOperationInfo operation = new ModelMBeanOperationInfo(
            "AnOperation",
            "The description",
            null,
            null,
            MBeanOperationInfo.ACTION
      );
      
      ModelMBeanInfoSupport info = new ModelMBeanInfoSupport(
            mbean.getClass().getName(),
            "some description",
            new ModelMBeanAttributeInfo[]    { attr1, attr2 },
            new ModelMBeanConstructorInfo[]  { constr1, constr2, constr3 },
            new ModelMBeanOperationInfo[]    { operation },
            null
      );

      ModelMBeanInfo clone = (ModelMBeanInfo)info.clone();      
      
      assertTrue(clone.getDescriptors(null).length == info.getDescriptors(null).length);
      
      // FIXME: equality not implemented to match field, value pairs
      //assertTrue(clone.getDescriptor("First Constructor", "constructor")
      //               .equals(
      //           info.getDescriptor("First Constructor", "constructor"))
      //);
      
      assertTrue(
            clone.getDescriptor("AnOperation", "operation")
            .getFieldValue("descriptorType")
            .equals(
            info.getDescriptor("AnOperation", "operation")
            .getFieldValue("descriptorType"))
      );
      
      assertTrue(
            clone.getDescriptor("AnOperation", "operation")
            .getFieldValue("name")
            .equals(
            info.getDescriptor("AnOperation", "operation")
            .getFieldValue("name"))
      );
      
   }
   
}
