/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.serialization;

import junit.framework.TestCase;

import java.lang.reflect.Array;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.InvalidObjectException;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Vector;

/**
 * Tests serialization with the RI
 *
 * @todo Proper equality tests instead of toString()
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class SerializeTestCase
   extends TestCase
{
   // Attributes ----------------------------------------------------------------

   // Constructor ---------------------------------------------------------------

   /**
    * Construct the test
    */
   public SerializeTestCase(String s)
   {
      super(s);
   }

   public void testArrayType()
      throws Exception
   {
      if (SerializationSUITE.form < 11)
         return;
      Class clazz = loadClass("javax.management.openmbean.SimpleType");
      Object elementType = clazz.getField("BIGDECIMAL").get(null);
      Object obj = instantiate(
         "javax.management.openmbean.ArrayType",
         new Class[] { Integer.TYPE, 
                       loadClass("javax.management.openmbean.OpenType") },
         new Object[] { new Integer(3), elementType }
      );
      Object result = runTest(obj);
      assertEquals(obj, result);
   }

   public void testAttribute()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.Attribute",
         new Class[] { String.class, Object.class },
         new Object[] { "name", "value" }
      );
      Object result = runTest(obj);
      assertEquals(obj, result);
   }

   public void testAttributeChangeNotification()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.AttributeChangeNotification",
         new Class[] { Object.class, Long.TYPE, Long.TYPE,
                       String.class, String.class, String.class,
                       Object.class, Object.class },
         new Object[] { "source", new Long(1), new Long(2), "message", "name", 
                        "type", "old", "new" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testAttributeChangeNotificationFilter()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.AttributeChangeNotificationFilter",
         new Class[0],
         new Object[0]
      );
      Method method = obj.getClass().getMethod("enableAttribute", 
          new Class[] { String.class });
      method.invoke(obj, new Object[] { "attribute" });
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testAttributeList()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.AttributeList",
         new Class[0],
         new Object[0]
      );
      Object attr = instantiate(
         "javax.management.Attribute",
         new Class[] { String.class, Object.class },
         new Object[] { "name", "value" }
      );
      Method method = obj.getClass().getMethod("add", 
          new Class[] { attr.getClass() });
      method.invoke(obj, new Object[] { attr });
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testAttributeNotFoundException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.AttributeNotFoundException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testAttributeValueExp()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.AttributeValueExp",
         new Class[] { String.class },
         new Object[] { "attr" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testBadAttributeValueExpException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.BadAttributeValueExpException",
         new Class[] { Object.class },
         new Object[] { "value" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testBadBinaryOpValueExpException()
      throws Exception
   {
      Object exp = instantiate(
         "javax.management.AttributeValueExp",
         new Class[] { String.class },
         new Object[] { "attr" }
      );

      Object obj = instantiate(
         "javax.management.BadBinaryOpValueExpException",
         new Class[] { loadClass("javax.management.ValueExp") },
         new Object[] { exp }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testBadStringOperationException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.BadStringOperationException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testCompositeDataSupport()
      throws Exception
   {
      if (SerializationSUITE.form < 11)
         return;
      Class clazz = loadClass("javax.management.openmbean.SimpleType");
      Object openType = clazz.getField("STRING").get(null);

      Class elementClass = loadClass("javax.management.openmbean.OpenType");
      Object array = Array.newInstance(elementClass, 2);
      Array.set(array, 0, openType);
      Array.set(array, 1, openType);

      Object compositeType = instantiate(
         "javax.management.openmbean.CompositeType",
         new Class[] { String.class, String.class, String[].class, String[].class, array.getClass() },
         new Object[] { "typeName", "description", new String[] { "name1", "name2" },
            new String[] { "desc1", "desc2" }, array }
      );
      Object obj = instantiate(
         "javax.management.openmbean.CompositeDataSupport",
         new Class[] { compositeType.getClass(), String[].class, Object[].class },
         new Object[] { compositeType, new String[] { "name1", "name2" },
            new Object[] { "itemValue1", "itemValue2" } }
      );
      Object result = runTest(obj);
      assertEquals(obj, result);
   }

   public void testCompositeType()
      throws Exception
   {
      if (SerializationSUITE.form < 11)
         return;
      Class clazz = loadClass("javax.management.openmbean.SimpleType");
      Object openType = clazz.getField("STRING").get(null);

      Class elementClass = loadClass("javax.management.openmbean.OpenType");
      Object array = Array.newInstance(elementClass, 2);
      Array.set(array, 0, openType);
      Array.set(array, 1, openType);

      Object obj = instantiate(
         "javax.management.openmbean.CompositeType",
         new Class[] { String.class, String.class, String[].class, String[].class, array.getClass() },
         new Object[] { "typeName", "description", new String[] { "name1", "name2" },
            new String[] { "desc1", "desc2" }, array }
      );
      Object result = runTest(obj);
      assertEquals(obj, result);
   }

   public void testDescriptorSupport()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.modelmbean.DescriptorSupport",
         new Class[] { new String[0].getClass(), new Object[0].getClass() },
         new Object[] { new String[] { "name1", "name2"},
                        new Object[] { "value1", "value2" } }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testInstanceAlreadyExistsException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.InstanceAlreadyExistsException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testInstanceNotFoundException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.InstanceNotFoundException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testIntrospectionException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.IntrospectionException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testInvalidApplicationException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.InvalidApplicationException",
         new Class[] { Object.class },
         new Object[] { "value" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testInvalidAttributeValueException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.InvalidAttributeValueException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testInvalidKeyException()
      throws Exception
   {
      if (SerializationSUITE.form < 11)
         return;
      Object obj = instantiate(
         "javax.management.openmbean.InvalidKeyException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testInvalidOpenTypeException()
      throws Exception
   {
      if (SerializationSUITE.form < 11)
         return;
      Object obj = instantiate(
         "javax.management.openmbean.InvalidOpenTypeException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testInvalidRelationIdException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.relation.InvalidRelationIdException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testInvalidRelationServiceException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.relation.InvalidRelationServiceException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testInvalidRelationTypeException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.relation.InvalidRelationTypeException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testInvalidRoleInfoException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.relation.InvalidRoleInfoException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testInvalidRoleValueException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.relation.InvalidRoleValueException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testInvalidTargetObjectTypeException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.modelmbean.InvalidTargetObjectTypeException",
         new Class[] { Exception.class, String.class },
         new Object[] { new Exception("exception"), "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testKeyAlreadyExistsException()
      throws Exception
   {
      if (SerializationSUITE.form < 11)
         return;
      Object obj = instantiate(
         "javax.management.openmbean.KeyAlreadyExistsException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testJMException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.JMException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testJMRuntimeException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.JMRuntimeException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testListenerNotFoundException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.ListenerNotFoundException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testMalformedObjectNameException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.MalformedObjectNameException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testMBeanAttributeInfo()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.MBeanAttributeInfo",
         new Class[] { String.class, String.class, String.class,
                       Boolean.TYPE, Boolean.TYPE, Boolean.TYPE },
         new Object[] { "name", "type", "description", new Boolean(true),
                        new Boolean(true), new Boolean(true)}
      );
      try
      {
         Object result = runTest(obj);
         assertEquals(obj.toString(), result.toString());
      }
      catch (java.io.InvalidClassException e)
      {
         fail("FAILS IN RI 1.1: Wrong serialization for form 1.0");
      }
   }

   public void testMBeanConstructorInfo()
      throws Exception
   {
      Object parm = instantiate(
         "javax.management.MBeanParameterInfo",
         new Class[] { String.class, String.class, String.class },
         new Object[] { "name", "type", "description" }
      );
      Object array = Array.newInstance(parm.getClass(), 1);
      Array.set(array, 0, parm);
      Object obj = instantiate(
         "javax.management.MBeanConstructorInfo",
         new Class[] { String.class, String.class, array.getClass() },
         new Object[] { "name", "description", array }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testMBeanException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.MBeanException",
         new Class[] { Exception.class, String.class },
         new Object[] { new Exception("Cause"), "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testMBeanFeatureInfo()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.MBeanFeatureInfo",
         new Class[] { String.class, String.class },
         new Object[] { "name", "description" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testMBeanInfo()
      throws Exception
   {
      Object parm = instantiate(
         "javax.management.MBeanParameterInfo",
         new Class[] { String.class, String.class, String.class },
         new Object[] { "name", "type", "description" }
      );
      Object parms = Array.newInstance(parm.getClass(), 1);
      Array.set(parms, 0, parm);

      Object att = instantiate(
         "javax.management.MBeanAttributeInfo",
         new Class[] { String.class, String.class, String.class,
                       Boolean.TYPE, Boolean.TYPE, Boolean.TYPE },
         new Object[] { "name", "type", "description", new Boolean(true),
                        new Boolean(true), new Boolean(true)}
      );
      Object atts = Array.newInstance(att.getClass(), 1);
      Array.set(atts, 0, att);

      Object con = instantiate(
         "javax.management.MBeanConstructorInfo",
         new Class[] { String.class, String.class, parms.getClass() },
         new Object[] { "name", "description", parms }
      );
      Object cons = Array.newInstance(con.getClass(), 1);
      Array.set(cons, 0, con);

      Class clazz = loadClass("javax.management.MBeanOperationInfo");
      Integer impact = new Integer(clazz.getField("ACTION").getInt(null));
      Object op = instantiate(
         "javax.management.MBeanOperationInfo",
         new Class[] { String.class, String.class, parms.getClass(),
                       String.class, Integer.TYPE },
         new Object[] { "name", "description", parms, "type", impact }
      );
      Object ops = Array.newInstance(op.getClass(), 1);
      Array.set(ops, 0, op);

      String[] types = { "type1", "type2" };
      Object not = instantiate(
         "javax.management.MBeanNotificationInfo",
         new Class[] { types.getClass(), String.class, String.class },
         new Object[] { types, "name", "description" }
      );
      Object nots = Array.newInstance(not.getClass(), 1);
      Array.set(nots, 0, not);

      Object obj = instantiate(
         "javax.management.MBeanInfo",
         new Class[] { String.class, String.class, atts.getClass(),
                       cons.getClass(), ops.getClass(), nots.getClass() },
         new Object[] { "className", "description", atts, cons, ops, nots }
      );
      try
      {
         Object result = runTest(obj);
         assertEquals(obj.toString(), result.toString());
      }
      catch (java.io.InvalidClassException e)
      {
         fail("FAILS IN RI 1.1: Wrong serialization for form 1.0 " +
              "The real error is in MBeanAttributeInfo");
      }
   }

   public void testMBeanNotificationInfo()
      throws Exception
   {
      String[] types = { "type1", "type2" };
      Object obj = instantiate(
         "javax.management.MBeanNotificationInfo",
         new Class[] { types.getClass(), String.class, String.class },
         new Object[] { types, "name", "description" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testMBeanOperationInfo()
      throws Exception
   {
      Object parm = instantiate(
         "javax.management.MBeanParameterInfo",
         new Class[] { String.class, String.class, String.class },
         new Object[] { "name", "type", "description" }
      );
      Object array = Array.newInstance(parm.getClass(), 1);
      Array.set(array, 0, parm);
      Class clazz = loadClass("javax.management.MBeanOperationInfo");
      Integer impact = new Integer(clazz.getField("ACTION").getInt(null));
      Object obj = instantiate(
         "javax.management.MBeanOperationInfo",
         new Class[] { String.class, String.class, array.getClass(),
                       String.class, Integer.TYPE },
         new Object[] { "name", "description", array, "type", impact }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testMBeanParameterInfo()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.MBeanParameterInfo",
         new Class[] { String.class, String.class, String.class },
         new Object[] { "name", "type", "description" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testMBeanRegistrationException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.MBeanRegistrationException",
         new Class[] { Exception.class, String.class },
         new Object[] { new Exception("Cause"), "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testMBeanServerNotification()
      throws Exception
   {
      Object objectName = instantiate(
         "javax.management.ObjectName",
         new Class[] { String.class },
         new Object[] { "domain:x=y" }
      );

      Class clazz = loadClass("javax.management.MBeanServerNotification");
      String type = (String) clazz.getField("REGISTRATION_NOTIFICATION").get(null);

      Object obj = instantiate(
         "javax.management.MBeanServerNotification",
         new Class[] { String.class, Object.class, Long.TYPE, 
                       objectName.getClass() },
         new Object[] { type, "source", new Long(1), objectName }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testMBeanServerNotificationFilter()
      throws Exception
   {
      Object objectName = instantiate(
         "javax.management.ObjectName",
         new Class[] { String.class },
         new Object[] { "domain:x=y" }
      );
      Object obj = instantiate(
         "javax.management.relation.MBeanServerNotificationFilter",
         new Class[0],
         new Object[0]
      );
      Method method = obj.getClass().getMethod("enableType", 
          new Class[] { String.class });
      method.invoke(obj, new Object[] { "prefix" });
      method = obj.getClass().getMethod("enableObjectName", 
          new Class[] { objectName.getClass() });
      method.invoke(obj, new Object[] { objectName });
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testMBeanServerPermission()
      throws Exception
   {
      if (SerializationSUITE.form < 11)
         return;
      Object obj = instantiate(
         "javax.management.MBeanServerPermission",
         new Class[] { String.class },
         new Object[] { "*" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testModelMBeanAttributeInfo()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.modelmbean.ModelMBeanAttributeInfo",
         new Class[] { String.class, String.class, String.class,
                       Boolean.TYPE, Boolean.TYPE, Boolean.TYPE },
         new Object[] { "name", "type", "description", new Boolean(true),
                        new Boolean(true), new Boolean(false)}
      );
      try
      {
         Object result = runTest(obj);
         assertEquals(obj.toString(), result.toString());
      }
      catch (java.io.InvalidClassException e)
      {
         fail("FAILS IN RI 1.1: Wrong serialization for form 1.0 ");
      }
   }

   /**
    * @todo equals test
    */
   public void testModelMBeanConstructorInfo()
      throws Exception
   {
      Object parm = instantiate(
         "javax.management.MBeanParameterInfo",
         new Class[] { String.class, String.class, String.class },
         new Object[] { "name", "type", "description" }
      );
      Object array = Array.newInstance(parm.getClass(), 1);
      Array.set(array, 0, parm);
      Object obj = instantiate(
         "javax.management.modelmbean.ModelMBeanConstructorInfo",
         new Class[] { String.class, String.class, array.getClass() },
         new Object[] { "name", "description", array }
      );
      Object result = runTest(obj);
   }

   /**
    * @todo equals test
    */
   public void testModelMBeanInfoSupport()
      throws Exception
   {
      Object parm = instantiate(
         "javax.management.MBeanParameterInfo",
         new Class[] { String.class, String.class, String.class },
         new Object[] { "name", "type", "description" }
      );
      Object parms = Array.newInstance(parm.getClass(), 1);
      Array.set(parms, 0, parm);

      Object att = instantiate(
         "javax.management.modelmbean.ModelMBeanAttributeInfo",
         new Class[] { String.class, String.class, String.class,
                       Boolean.TYPE, Boolean.TYPE, Boolean.TYPE },
         new Object[] { "name", "type", "description", new Boolean(true),
                        new Boolean(true), new Boolean(false)}
      );
      Object atts = Array.newInstance(att.getClass(), 1);
      Array.set(atts, 0, att);

      Object con = instantiate(
         "javax.management.modelmbean.ModelMBeanConstructorInfo",
         new Class[] { String.class, String.class, parms.getClass() },
         new Object[] { "name", "description", parms }
      );
      Object cons = Array.newInstance(con.getClass(), 1);
      Array.set(cons, 0, con);

      Class clazz = loadClass("javax.management.modelmbean.ModelMBeanOperationInfo");
      Integer impact = new Integer(clazz.getField("ACTION").getInt(null));
      Object op = instantiate(
         "javax.management.modelmbean.ModelMBeanOperationInfo",
         new Class[] { String.class, String.class, parms.getClass(),
                       String.class, Integer.TYPE },
         new Object[] { "name", "description", parms, "type", impact }
      );
      Object ops = Array.newInstance(op.getClass(), 1);
      Array.set(ops, 0, op);

      String[] types = { "type1", "type2" };
      Object not = instantiate(
         "javax.management.modelmbean.ModelMBeanNotificationInfo",
         new Class[] { types.getClass(), String.class, String.class },
         new Object[] { types, "name", "description" }
      );
      Object nots = Array.newInstance(not.getClass(), 1);
      Array.set(nots, 0, not);

      Object obj = instantiate(
         "javax.management.modelmbean.ModelMBeanInfoSupport",
         new Class[] { String.class, String.class, atts.getClass(),
                       cons.getClass(), ops.getClass(), nots.getClass() },
         new Object[] { "className", "description", atts, cons, ops, nots }
      );
      try
      {
         Object result = runTest(obj);
      }
      catch (java.io.InvalidClassException e)
      {
         fail("FAILS IN RI 1.1: Wrong serialization for form 1.0 ");
      }
   }

   /**
    * @todo equals test
    */
   public void testModelMBeanNotificationInfo()
      throws Exception
   {
      String[] types = { "type1", "type2" };
      Object obj = instantiate(
         "javax.management.modelmbean.ModelMBeanNotificationInfo",
         new Class[] { types.getClass(), String.class, String.class },
         new Object[] { types, "name", "description" }
      );
      try
      {
         Object result = runTest(obj);
      }
      catch (java.io.StreamCorruptedException e)
      {
         fail("FAILS IN RI 1.1: Wrong serialization for form 1.0 ");
      }
   }

   /**
    * @todo equals test
    */
   public void testModelMBeanOperationInfo()
      throws Exception
   {
      Object parm = instantiate(
         "javax.management.MBeanParameterInfo",
         new Class[] { String.class, String.class, String.class },
         new Object[] { "name", "type", "description" }
      );
      Object array = Array.newInstance(parm.getClass(), 1);
      Array.set(array, 0, parm);
      Class clazz = loadClass("javax.management.MBeanOperationInfo");
      Integer impact = new Integer(clazz.getField("ACTION").getInt(null));
      Object obj = instantiate(
         "javax.management.modelmbean.ModelMBeanOperationInfo",
         new Class[] { String.class, String.class, array.getClass(),
                       String.class, Integer.TYPE },
         new Object[] { "name", "description", array, "type", impact }
      );
      try
      {
         Object result = runTest(obj);
         assertEquals(obj.toString(), result.toString());
      }
      catch (java.io.StreamCorruptedException e)
      {
         fail("FAILS IN RI 1.1: Wrong serialization for form 1.0 ");
      }
   }

   /**
    * @todo the constructor is package private
    * Actually tested by temporarily making the constructor public
    */
   public void testMonitorNotification()
      throws Exception
   {
/*****
      Object monitorName = instantiate(
         "javax.management.ObjectName",
         new Class[] { String.class },
         new Object[] { "monitor:x=y" }
      );
      Object objectName = instantiate(
         "javax.management.ObjectName",
         new Class[] { String.class },
         new Object[] { "domain:x=y" }
      );
      Object obj = instantiate(
         "javax.management.monitor.MonitorNotification",
         new Class[] { String.class, Object.class, Long.TYPE, Long.TYPE,
                       String.class, Object.class, String.class, 
                       objectName.getClass(), Object.class },
         new Object[] { "type", monitorName, new Long(1), new Long(2),
                        "message", "derivedGauge", "attribute", objectName,
                        "trigger"}
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
*****/
   }

   public void testMonitorSettingException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.monitor.MonitorSettingException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testNotCompliantMBeanException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.NotCompliantMBeanException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testNotification()
      throws Exception
   {
      Object objectName = instantiate(
         "javax.management.ObjectName",
         new Class[] { String.class },
         new Object[] { "domain:x=y" }
      );
      Object obj = instantiate(
         "javax.management.Notification",
         new Class[] { String.class, Object.class, Long.TYPE, Long.TYPE, 
                       String.class },
         new Object[] { "type", objectName, new Long(1), new Long(2), 
                        "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   /**
    * @todo equals tests
    */
   public void testNotificationFilterSupport()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.NotificationFilterSupport",
         new Class[0],
         new Object[0]
      );
      Method method = obj.getClass().getMethod("enableType", 
          new Class[] { String.class });
      method.invoke(obj, new Object[] { "prefix" });
      Object result = runTest(obj);
   }

   public void testObjectInstance()
      throws Exception
   {
      Object objectName = instantiate(
         "javax.management.ObjectName",
         new Class[] { String.class },
         new Object[] { "domain:x=y" }
      );
      Object obj = instantiate(
         "javax.management.ObjectInstance",
         new Class[] { objectName.getClass(), String.class },
         new Object[] { objectName, "DummyClass" }
      );
      Object result = runTest(obj);
      assertEquals(obj, result);
   }

   public void testObjectName()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.ObjectName",
         new Class[] { String.class },
         new Object[] { "domain:x=y" }
      );
      Object result = runTest(obj);
      assertEquals(obj, result);
   }

   public void testObjectNamePattern()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.ObjectName",
         new Class[] { String.class },
         new Object[] { "domain*:x=y" }
      );
      Object result = runTest(obj);
      assertEquals(obj, result);
   }

   public void testObjectNamePropertyPattern()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.ObjectName",
         new Class[] { String.class },
         new Object[] { "domain:x=y,*" }
      );
      Object result = runTest(obj);
      assertEquals(obj, result);
   }

   public void testObjectNameRawPropertyPattern()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.ObjectName",
         new Class[] { String.class },
         new Object[] { "domain:*" }
      );
      Object result = runTest(obj);
      assertEquals(obj, result);
   }

   public void testOpenDataException()
      throws Exception
   {
      if (SerializationSUITE.form < 11)
         return;
      Object obj = instantiate(
         "javax.management.openmbean.OpenDataException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testOpenMBeanAttributeInfoSupportMinMax()
      throws Exception
   {
      if (SerializationSUITE.form < 11)
         return;
      Class clazz = loadClass("javax.management.openmbean.SimpleType");
      Object elementType = clazz.getField("INTEGER").get(null);

      Object obj = instantiate(
         "javax.management.openmbean.OpenMBeanAttributeInfoSupport",
         new Class[] { String.class, String.class, loadClass("javax.management.openmbean.OpenType"),
                       boolean.class, boolean.class, boolean.class, Object.class, Comparable.class, Comparable.class },
         new Object[] { "name", "description", elementType, new Boolean(true), new Boolean(true), new Boolean(true),
                        new Integer(12), new Integer(11), new Integer(13) }
      );
      Object result = runTest(obj);
      assertEquals(obj, result);
   }

   public void testOpenMBeanAttributeInfoSupportLegal()
      throws Exception
   {
      if (SerializationSUITE.form < 11)
         return;
      Class clazz = loadClass("javax.management.openmbean.SimpleType");
      Object elementType = clazz.getField("INTEGER").get(null);

      Object obj = instantiate(
         "javax.management.openmbean.OpenMBeanAttributeInfoSupport",
         new Class[] { String.class, String.class, loadClass("javax.management.openmbean.OpenType"),
                       boolean.class, boolean.class, boolean.class, Object.class, Object[].class },
         new Object[] { "name", "description", elementType, new Boolean(true), new Boolean(true), new Boolean(true),
                        new Integer(12), new Integer[] { new Integer(12), new Integer(13) }}
      );
      Object result = runTest(obj);
      assertEquals(obj, result);
   }

   public void testOpenMBeanConstructorInfoSupport()
      throws Exception
   {
      if (SerializationSUITE.form < 11)
         return;
      Class clazz = loadClass("javax.management.openmbean.SimpleType");
      Object elementType = clazz.getField("INTEGER").get(null);

      Object parmInfo = instantiate(
         "javax.management.openmbean.OpenMBeanParameterInfoSupport",
         new Class[] { String.class, String.class, loadClass("javax.management.openmbean.OpenType"),
                       Object.class, Object[].class },
         new Object[] { "name", "description", elementType,
                        new Integer(12), new Integer[] {new Integer(12), new Integer(13) }}
      );
      Object array = Array.newInstance(parmInfo.getClass(), 1);
      Array.set(array, 0, parmInfo);

      Object obj = instantiate(
         "javax.management.openmbean.OpenMBeanConstructorInfoSupport",
         new Class[] { String.class, String.class, loadClass("[Ljavax.management.openmbean.OpenMBeanParameterInfo;")},
         new Object[] { "name", "description", array }
      );
      Object result = runTest(obj);
      assertEquals(obj, result);
   }

   public void testOpenMBeanInfoSupport()
      throws Exception
   {
      if (SerializationSUITE.form < 11)
         return;
      Class clazz = loadClass("javax.management.openmbean.SimpleType");
      Object elementType = clazz.getField("INTEGER").get(null);

      Object parmInfo = instantiate(
         "javax.management.openmbean.OpenMBeanParameterInfoSupport",
         new Class[] { String.class, String.class, loadClass("javax.management.openmbean.OpenType"),
                       Object.class, Object[].class },
         new Object[] { "name", "description", elementType,
                        new Integer(12), new Integer[] {new Integer(12), new Integer(13) }}
      );
      Object parmArray = Array.newInstance(parmInfo.getClass(), 1);
      Array.set(parmArray, 0, parmInfo);

      Object attInfo = instantiate(
         "javax.management.openmbean.OpenMBeanAttributeInfoSupport",
         new Class[] { String.class, String.class, loadClass("javax.management.openmbean.OpenType"),
                       boolean.class, boolean.class, boolean.class, Object.class, Object[].class },
         new Object[] { "name", "description", elementType, new Boolean(true), new Boolean(true), new Boolean(true),
                        new Integer(12), new Integer[] { new Integer(12), new Integer(13) }}
      );

      Object conInfo = instantiate(
         "javax.management.openmbean.OpenMBeanConstructorInfoSupport",
         new Class[] { String.class, String.class, loadClass("[Ljavax.management.openmbean.OpenMBeanParameterInfo;")},
         new Object[] { "name", "description", parmArray }
      );

      clazz = loadClass("javax.management.MBeanOperationInfo");
      Object impact = clazz.getField("INFO").get(null);

      Object opInfo = instantiate(
         "javax.management.openmbean.OpenMBeanOperationInfoSupport",
         new Class[] { String.class, String.class, loadClass("[Ljavax.management.openmbean.OpenMBeanParameterInfo;"),
                       loadClass("javax.management.openmbean.OpenType"), int.class },
         new Object[] { "name", "description", parmArray, elementType, impact }
      );

      String[] types = { "type1", "type2" };
      Object notInfo = instantiate(
         "javax.management.MBeanNotificationInfo",
         new Class[] { types.getClass(), String.class, String.class },
         new Object[] { types, "name", "description" }
      );

      Object attArray = Array.newInstance(attInfo.getClass(), 1);
      Array.set(attArray, 0, attInfo);

      Object conArray = Array.newInstance(conInfo.getClass(), 1);
      Array.set(conArray, 0, conInfo);

      Object opArray = Array.newInstance(opInfo.getClass(), 1);
      Array.set(opArray, 0, opInfo);

      Object notArray = Array.newInstance(notInfo.getClass(), 1);
      Array.set(notArray, 0, notInfo);

      Object obj = instantiate(
         "javax.management.openmbean.OpenMBeanInfoSupport",
         new Class[] { String.class, String.class, 
                       loadClass("[Ljavax.management.openmbean.OpenMBeanAttributeInfo;"),
                       loadClass("[Ljavax.management.openmbean.OpenMBeanConstructorInfo;"),
                       loadClass("[Ljavax.management.openmbean.OpenMBeanOperationInfo;"),
                       loadClass("[Ljavax.management.MBeanNotificationInfo;") },
         new Object[] { "classname", "description", attArray, conArray, opArray, notArray }
      );

      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testOpenMBeanOperationInfoSupport()
      throws Exception
   {
      if (SerializationSUITE.form < 11)
         return;
      Class clazz = loadClass("javax.management.openmbean.SimpleType");
      Object elementType = clazz.getField("INTEGER").get(null);

      Object parmInfo = instantiate(
         "javax.management.openmbean.OpenMBeanParameterInfoSupport",
         new Class[] { String.class, String.class, loadClass("javax.management.openmbean.OpenType"),
                       Object.class, Object[].class },
         new Object[] { "name", "description", elementType,
                        new Integer(12), new Integer[] {new Integer(12), new Integer(13) }}
      );
      Object array = Array.newInstance(parmInfo.getClass(), 1);
      Array.set(array, 0, parmInfo);

      clazz = loadClass("javax.management.MBeanOperationInfo");
      Object impact = clazz.getField("INFO").get(null);

      Object obj = instantiate(
         "javax.management.openmbean.OpenMBeanOperationInfoSupport",
         new Class[] { String.class, String.class, loadClass("[Ljavax.management.openmbean.OpenMBeanParameterInfo;"),
                       loadClass("javax.management.openmbean.OpenType"), int.class },
         new Object[] { "name", "description", array, elementType, impact }
      );
      Object result = runTest(obj);
      assertEquals(obj, result);
   }

   public void testOpenMBeanParameterInfoSupportMinMax()
      throws Exception
   {
      if (SerializationSUITE.form < 11)
         return;
      Class clazz = loadClass("javax.management.openmbean.SimpleType");
      Object elementType = clazz.getField("INTEGER").get(null);

      Object obj = instantiate(
         "javax.management.openmbean.OpenMBeanParameterInfoSupport",
         new Class[] { String.class, String.class, loadClass("javax.management.openmbean.OpenType"),
                       Object.class, Comparable.class, Comparable.class },
         new Object[] { "name", "description", elementType,
                        new Integer(12), new Integer(11), new Integer(13) }
      );
      Object result = runTest(obj);
      assertEquals(obj, result);
   }

   public void testOpenMBeanParameterInfoSupportLegal()
      throws Exception
   {
      if (SerializationSUITE.form < 11)
         return;
      Class clazz = loadClass("javax.management.openmbean.SimpleType");
      Object elementType = clazz.getField("INTEGER").get(null);

      Object obj = instantiate(
         "javax.management.openmbean.OpenMBeanParameterInfoSupport",
         new Class[] { String.class, String.class, loadClass("javax.management.openmbean.OpenType"),
                       Object.class, Object[].class },
         new Object[] { "name", "description", elementType,
                         new Integer(12), new Integer[] {new Integer(12), new Integer(13) }}
      );
      Object result = runTest(obj);
      assertEquals(obj, result);
   }

   public void testOperationsException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.OperationsException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testReflectionException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.ReflectionException",
         new Class[] { Exception.class, String.class },
         new Object[] { new Exception("Cause"), "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testRelationException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.relation.RelationException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testRelationNotFoundException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.relation.RelationNotFoundException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testRelationNotification()
      throws Exception
   {
      Object objectName = instantiate(
         "javax.management.ObjectName",
         new Class[] { String.class },
         new Object[] { "domain:x=y" }
      );
      Class clazz = loadClass("javax.management.relation.RelationNotification");
      String type = (String) clazz.getField("RELATION_BASIC_UPDATE").get(null);
      ArrayList newValue = new ArrayList();
      newValue.add(objectName);
      ArrayList oldValue = new ArrayList();
      oldValue.add(objectName);
      Object obj = instantiate(
         "javax.management.relation.RelationNotification",
         new Class[] { String.class, Object.class, Long.TYPE, Long.TYPE, 
                       String.class, String.class, String.class,
                       objectName.getClass(), String.class, List.class, List.class },
         new Object[] { type, objectName, new Long(1), new Long(2), 
                        "message", "relationId", "relationType", objectName,
                        "roleName", newValue, oldValue}
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testRelationServiceNotRegisteredException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.relation.RelationServiceNotRegisteredException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testRelationTypeNotFoundException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.relation.RelationTypeNotFoundException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   /**
    * @todo equals test
    */
   public void testRelationTypeSupport()
      throws Exception
   {
      Object roleInfo = instantiate(
         "javax.management.relation.RoleInfo",
         new Class[] { String.class, String.class, Boolean.TYPE, Boolean.TYPE, 
                       Integer.TYPE, Integer.TYPE, String.class },
         new Object[] { "name", "test.serialization.support.Trivial", 
                        new Boolean(true), new Boolean(true), new Integer(10), 
                        new Integer(20), "descritpion" }
      );
      Object array = Array.newInstance(roleInfo.getClass(), 1);
      Array.set(array, 0, roleInfo);
      Object obj = instantiate(
         "javax.management.relation.RelationTypeSupport",
         new Class[] { String.class, array.getClass() },
         new Object[] { "name", array }
      );
      Object result = runTest(obj);
   }

   public void testRole()
      throws Exception
   {
      Object objectName = instantiate(
         "javax.management.ObjectName",
         new Class[] { String.class },
         new Object[] { "domain:x=y" }
      );
      ArrayList list = new ArrayList();
      list.add(objectName);
      Object obj = instantiate(
         "javax.management.relation.Role",
         new Class[] { String.class, List.class},
         new Object[] { "name", list } 
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testRoleInfo()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.relation.RoleInfo",
         new Class[] { String.class, String.class, Boolean.TYPE, Boolean.TYPE, 
                       Integer.TYPE, Integer.TYPE, String.class },
         new Object[] { "name", "test.serialization.support.Trivial", 
                        new Boolean(true), new Boolean(true), new Integer(10), 
                        new Integer(20), "descritpion" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testRoleInfoNotFoundException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.relation.RoleInfoNotFoundException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testRoleList()
      throws Exception
   {
      Object objectName = instantiate(
         "javax.management.ObjectName",
         new Class[] { String.class },
         new Object[] { "domain:x=y" }
      );
      ArrayList list = new ArrayList();
      list.add(objectName);
      Object role = instantiate(
         "javax.management.relation.Role",
         new Class[] { String.class, List.class},
         new Object[] { "name", list } 
      );
      list = new ArrayList();
      list.add(role);
      Object obj = instantiate(
         "javax.management.relation.RoleList",
         new Class[] { List.class},
         new Object[] { list } 
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testRoleNotFoundException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.relation.RoleNotFoundException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testRoleResult()
      throws Exception
   {
      Object objectName = instantiate(
         "javax.management.ObjectName",
         new Class[] { String.class },
         new Object[] { "domain:x=y" }
      );
      ArrayList list = new ArrayList();
      list.add(objectName);

      Object resolved = instantiate(
         "javax.management.relation.Role",
         new Class[] { String.class, List.class},
         new Object[] { "name", list } 
      );
      list = new ArrayList();
      list.add(resolved);
      Object resolvedList = instantiate(
         "javax.management.relation.RoleList",
         new Class[] { List.class },
         new Object[] { list } 
      );

      Class clazz = loadClass("javax.management.relation.RoleStatus");
      Integer status = new Integer(clazz.getField("ROLE_NOT_READABLE").getInt(null));
      Object unresolved = instantiate(
         "javax.management.relation.RoleUnresolved",
         new Class[] { String.class, List.class, Integer.TYPE},
         new Object[] { "name", list, status } 
      );
      list = new ArrayList();
      list.add(unresolved);
      Object unresolvedList = instantiate(
         "javax.management.relation.RoleUnresolvedList",
         new Class[] { List.class },
         new Object[] { list } 
      );
      Object obj = instantiate(
         "javax.management.relation.RoleResult",
         new Class[] { resolvedList.getClass(), unresolvedList.getClass() },
         new Object[] { resolvedList, unresolvedList } 
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testRoleUnresolved()
      throws Exception
   {
      Object objectName = instantiate(
         "javax.management.ObjectName",
         new Class[] { String.class },
         new Object[] { "domain:x=y" }
      );
      ArrayList list = new ArrayList();
      list.add(objectName);
      Class clazz = loadClass("javax.management.relation.RoleStatus");
      Integer status = new Integer(clazz.getField("ROLE_NOT_READABLE").getInt(null));
      Object obj = instantiate(
         "javax.management.relation.RoleUnresolved",
         new Class[] { String.class, List.class, Integer.TYPE},
         new Object[] { "name", list, status } 
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testRoleUnresolvedList()
      throws Exception
   {
      Object objectName = instantiate(
         "javax.management.ObjectName",
         new Class[] { String.class },
         new Object[] { "domain:x=y" }
      );
      ArrayList list = new ArrayList();
      list.add(objectName);
      Class clazz = loadClass("javax.management.relation.RoleStatus");
      Integer status = new Integer(clazz.getField("ROLE_NOT_READABLE").getInt(null));
      Object unresolved = instantiate(
         "javax.management.relation.RoleUnresolved",
         new Class[] { String.class, List.class, Integer.TYPE},
         new Object[] { "name", list, status } 
      );
      list = new ArrayList();
      list.add(unresolved);
      Object obj = instantiate(
         "javax.management.relation.RoleUnresolvedList",
         new Class[] { List.class},
         new Object[] { list } 
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testRuntimeErrorException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.RuntimeErrorException",
         new Class[] { Error.class, String.class },
         new Object[] { new Error("Cause"), "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testRuntimeMBeanException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.RuntimeMBeanException",
         new Class[] { RuntimeException.class, String.class },
         new Object[] { new RuntimeException("Cause"), "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testRuntimeOperationsException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.RuntimeOperationsException",
         new Class[] { RuntimeException.class, String.class },
         new Object[] { new RuntimeException("Cause"), "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testServiceNotFoundException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.ServiceNotFoundException",
         new Class[] { String.class },
         new Object[] { "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testSimpleType()
      throws Exception
   {
      if (SerializationSUITE.form < 11)
         return;
      Class clazz = loadClass("javax.management.openmbean.SimpleType");
      Object obj = clazz.getField("BIGDECIMAL").get(null);
      Object result = runTest(obj);
      assertTrue("Simple types should resolve to the same object",
                 obj == result);
   }

   public void testStringValueExp()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.StringValueExp",
         new Class[] { String.class },
         new Object[] { "attr" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   public void testTabularDataSupport()
      throws Exception
   {
      if (SerializationSUITE.form < 11)
         return;
      Class clazz = loadClass("javax.management.openmbean.SimpleType");
      Object openType = clazz.getField("STRING").get(null);

      Class elementClass = loadClass("javax.management.openmbean.OpenType");
      Object array = Array.newInstance(elementClass, 2);
      Array.set(array, 0, openType);
      Array.set(array, 1, openType);

      Object compositeType = instantiate(
         "javax.management.openmbean.CompositeType",
         new Class[] { String.class, String.class, String[].class, String[].class, array.getClass() },
         new Object[] { "typeName", "description", new String[] { "name1", "name2" },
            new String[] { "desc1", "desc2" }, array }
      );

      Object tabularType = instantiate(
         "javax.management.openmbean.TabularType",
         new Class[] { String.class, String.class, compositeType.getClass(), String[].class },
         new Object[] { "typeName", "description", compositeType, new String[] { "name1" }}
      );

      Object obj = instantiate(
         "javax.management.openmbean.TabularDataSupport",
         new Class[] { tabularType.getClass() },
         new Object[] { tabularType }
      );
      Object result = runTest(obj);
      assertEquals(obj, result);
   }

   public void testTabularType()
      throws Exception
   {
      if (SerializationSUITE.form < 11)
         return;
      Class clazz = loadClass("javax.management.openmbean.SimpleType");
      Object openType = clazz.getField("STRING").get(null);

      Class elementClass = loadClass("javax.management.openmbean.OpenType");
      Object array = Array.newInstance(elementClass, 2);
      Array.set(array, 0, openType);
      Array.set(array, 1, openType);

      Object compositeType = instantiate(
         "javax.management.openmbean.CompositeType",
         new Class[] { String.class, String.class, String[].class, String[].class, array.getClass() },
         new Object[] { "typeName", "description", new String[] { "name1", "name2" },
            new String[] { "desc1", "desc2" }, array }
      );

      Object obj = instantiate(
         "javax.management.openmbean.TabularType",
         new Class[] { String.class, String.class, compositeType.getClass(), String[].class },
         new Object[] { "typeName", "description", compositeType, new String[] { "name1" }}
      );

      Object result = runTest(obj);
      assertEquals(obj, result);
   }

   /**
    * @todo ?
    */
   public void testTimerAlarmClockNotification()
      throws Exception
   {
   }

   /**
    * @todo the constructor is package private
    * Actually tested by temporarily making the constructor public
    */
   public void testTimerNotification()
      throws Exception
   {
/*****
      Object timerName = instantiate(
         "javax.management.ObjectName",
         new Class[] { String.class },
         new Object[] { "timer:x=y" }
      );
      Object obj = instantiate(
         "javax.management.timer.TimerNotification",
         new Class[] { String.class, Object.class, Long.TYPE, Long.TYPE,
                       String.class, Integer.class, Object.class },
         new Object[] { "type", timerName, new Long(1), new Long(2),
                        "message", new Integer(1), "user data" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
*****/
   }

   public void testXMLParseException()
      throws Exception
   {
      Object obj = instantiate(
         "javax.management.modelmbean.XMLParseException",
         new Class[] { Exception.class, String.class },
         new Object[] { new Exception("exception"), "message" }
      );
      Object result = runTest(obj);
      assertEquals(obj.toString(), result.toString());
   }

   // Support -------------------------------------------------------

   /**
    * Instantiate an object using JBossMX.
    */
   private Object instantiate(String className, Class[] sig, Object[] parms)
      throws Exception
   {
      Constructor cons = loadClass(className).getDeclaredConstructor(sig);
      return cons.newInstance(parms);
   }

   /**
    * Load a class using JBossMX.
    */
   private Class loadClass(String className)
      throws Exception
   {
      return SerializationSUITE.jbossmx.loadClass(className);
   }

   /**
    * Serialize from jbossmx to jmxri.
    * Serialize from jmxri to jbossmx.
    */
   private Object runTest(Object obj)
      throws Exception
   {
      ByteArrayOutputStream os = serializeJBoss(obj);
      Object intermediate = deserializeRI(os);
      os = serializeRI(intermediate);
      return deserializeJBoss(os);
   }

   /**
    * Dummy method wrapper for debugging.
    */
   private ByteArrayOutputStream serializeJBoss(Object obj)
      throws Exception
   {
      return serialize(obj);
   }

   /**
    * Dummy method wrapper for debugging.
    */
   private ByteArrayOutputStream serializeRI(Object obj)
      throws Exception
   {
      return serialize(obj);
   }

   /**
    * Dummy method wrapper for debugging.
    */
   private Object deserializeJBoss(ByteArrayOutputStream os)
      throws Exception
   {
      return deserialize(SerializationSUITE.jbossmx, os);
   }

   /**
    * Dummy method wrapper for debugging.
    */
   private Object deserializeRI(ByteArrayOutputStream os)
      throws Exception
   {
      return deserialize(SerializationSUITE.jmxri, os);
   }

   /**
    * Serialize the object.
    */
   private ByteArrayOutputStream serialize(Object obj)
      throws Exception
   {
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      ObjectOutputStream oos = new ObjectOutputStream(baos);
      oos.writeObject(obj);
      return baos;
   }
    
   /**
    * Deserialize the object.
    */
   private Object deserialize(ClassLoader cl, ByteArrayOutputStream baos)
      throws Exception
   {
      ByteArrayInputStream bais = new ByteArrayInputStream(baos.toByteArray());
      ObjectInputStream ois = new MyObjectInputStream(cl, bais);
      return ois.readObject();
   }

   /**
    * Custom inputstream to override classloading to the relevent
    * jmx implementation
    */
   public class MyObjectInputStream extends ObjectInputStream
   {
      ClassLoader cl;
      public MyObjectInputStream(ClassLoader cl, ByteArrayInputStream is)
         throws IOException
      {
         super(is);
         this.cl = cl;
      }
      protected Class resolveClass(java.io.ObjectStreamClass osc)
         throws IOException, ClassNotFoundException
      {
         return cl.loadClass(osc.getName());
      }
   }
}
