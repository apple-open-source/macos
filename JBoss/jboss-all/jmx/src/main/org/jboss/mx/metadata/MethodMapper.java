/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.metadata;

import javax.management.MBeanAttributeInfo;
import javax.management.MBeanOperationInfo;
import javax.management.MBeanParameterInfo;
import javax.management.modelmbean.ModelMBeanAttributeInfo;
import java.lang.reflect.Method;
import java.util.HashMap;

/**
 * Helper class for resolving JMX *Info objects against Method objects. It's typically
 * used during the construction of dispatchers during MBean registration/creation.
 *
 * If you're looking for a fast resolver of JMX operation signatures see AttributeOperationResolver.
 *
 * FIXME - this class is not aware of multiple target objects (as in modelmbeans)
 * however I'm half expecting that feature to disappear in JMX 1.1 anyhow.
 *
 * @author  <a href="mailto:trevor@protocool.com">Trevor Squires</a>.
 */

public class MethodMapper
{
   private HashMap map = null;

   /**
    * Constructs a mapper by reflecting on the class.
    */
   public MethodMapper(Class resourceClass)
   {
      if (null == resourceClass)
      {
         throw new IllegalArgumentException("resourceClass cannot be null");
      }

      map = createMap(resourceClass);
   }

   /**
    * Return a method matching the signature defined in the operation info
    */
   public Method lookupOperation(MBeanOperationInfo info)
   {
      if (null == info)
      {
         throw new IllegalArgumentException("MBeanOperationInfo cannot be null");
      }

      return (Method) map.get(operationSignature(info));
   }

   /**
    * Return a method matching the getter signature expected for an attribute.
    */
   public Method lookupGetter(MBeanAttributeInfo info)
   {
      if (null == info)
      {
         throw new IllegalArgumentException("MBeanAttributeInfo cannot be null");
      }

      return (Method) map.get(getterSignature(info));
   }

   /**
    * Return a method matching the getter signature expected for a ModelMBean attribute.
    */
   public Method lookupGetter(ModelMBeanAttributeInfo info)
   {
      if (null == info)
      {
         throw new IllegalArgumentException("MBeanAttributeInfo cannot be null");
      }

      return (Method) map.get(getterSignature(info));
   }

   /**
    * Return a method matching the setter signature expected for an attribute
    */
   public Method lookupSetter(MBeanAttributeInfo info)
   {
      if (null == info)
      {
         throw new IllegalArgumentException("MBeanAttributeInfo cannot be null");
      }

      return (Method) map.get(setterSignature(info));
   }

   /**
    * Return a method matching the setter signature expected for a ModelMBean attribute
    */
   public Method lookupSetter(ModelMBeanAttributeInfo info)
   {
      if (null == info)
      {
         throw new IllegalArgumentException("ModelMBeanAttributeInfo cannot be null");
      }

      return (Method) map.get(setterSignature(info));
   }

   /**
    * Return a method matching the specified signature
    */
   public Method lookupMethod(String returnType, String name, String[] signature)
   {
      if (null == returnType)
      {
         throw new IllegalArgumentException("returnType cannot be null");
      }

      if (null == name)
      {
         throw new IllegalArgumentException("method name cannot be null");
      }

      return (Method) map.get(methodSignature(returnType, name, signature));
   }

   /**
    * Generates a signature string for an attribute getter method using standard rules
    */
   public static String getterSignature(MBeanAttributeInfo info)
   {
      if (null == info)
      {
         throw new IllegalArgumentException("MBeanAttributeInfo cannot be null");
      }

      String prefix = (info.isIs()) ? "is" : "get";
      return methodSignature(info.getType(), prefix + info.getName(), null);
   }

   /**
    * Generates a getter signature string for a ModelMBean attribute by checking the
    * descriptor for getMethod.
    */
   public static String getterSignature(ModelMBeanAttributeInfo info)
   {
      if (null == info)
      {
         throw new IllegalArgumentException("ModelMBeanAttributeInfo cannot be null");
      }

      String methodName = (String) info.getDescriptor().getFieldValue("getMethod");

      if (null == methodName)
      {
         throw new IllegalArgumentException("getMethod not defined in ModelMBeanAttributeInfo descriptor");
      }

      return methodSignature(info.getType(), methodName, null);
   }

   /**
    * Generates a signature string for an attribute setter method using standard rules
    */
   public static String setterSignature(MBeanAttributeInfo info)
   {
      if (null == info)
      {
         throw new IllegalArgumentException("MBeanAttributeInfo cannot be null");
      }

      return methodSignature(Void.TYPE.getName(), "set" + info.getName(), new String[]{info.getType()});
   }

   /**
    * Generates a setter signature string for a ModelMBean attribute by checking the
    * descriptor for setMethod.
    */
   public static String setterSignature(ModelMBeanAttributeInfo info)
   {
      if (null == info)
      {
         throw new IllegalArgumentException("ModelMBeanAttributeInfo cannot be null");
      }

      String methodName = (String) info.getDescriptor().getFieldValue("setMethod");

      if (null == methodName)
      {
         throw new IllegalArgumentException("setMethod not defined in ModelMBeanAttributeInfo descriptor");
      }

      return methodSignature(Void.TYPE.getName(), methodName, new String[]{info.getType()});
   }

   /**
    * Generates a signature string using the operation info.
    */
   public static String operationSignature(MBeanOperationInfo info)
   {
      if (null == info)
      {
         throw new IllegalArgumentException("MBeanOperationInfo cannot be null");
      }

      MBeanParameterInfo[] params = info.getSignature();
      String[] signature = new String[params.length];
      for (int i = 0; i < signature.length; i++)
      {
         signature[i] = params[i].getType();
      }
      return methodSignature(info.getReturnType(), info.getName(), signature);
   }

   /**
    * Generates a signature string using a Method object.
    */
   public static String methodSignature(Method method)
   {
      if (null == method)
      {
         throw new IllegalArgumentException("Method cannot be null");
      }

      Class[] paramtypes = method.getParameterTypes();
      String[] signature = new String[paramtypes.length];
      for (int i = 0; i < signature.length; i++)
      {
         signature[i] = paramtypes[i].getName();
      }
      return methodSignature(method.getReturnType().getName(), method.getName(), signature);
   }

   /**
    * Generates a signature string using the supplied signature arguments.
    */
   public static String methodSignature(String returnType, String name, String[] signature)
   {
      if (null == returnType)
      {
         throw new IllegalArgumentException("returnType cannot be null");
      }
      if (null == name)
      {
         throw new IllegalArgumentException("method name cannot be null");
      }

      StringBuffer buf = new StringBuffer(returnType).append(';').append(name);
      if (null == signature)
      {
         return buf.toString();
      }

      for (int i = 0; i < signature.length; i++)
      {
         buf.append(';').append(signature[i]); // the ; char ensures uniqueness
      }

      return buf.toString();
   }

   /**
    * creates the signature string to Method HashMap.
    */
   protected HashMap createMap(Class resourceClass)
   {
      HashMap cmap = new HashMap();
      Method[] methods = resourceClass.getMethods();
      for (int i = 0; i < methods.length; i++)
      {
         cmap.put(methodSignature(methods[i]), methods[i]);
      }
      return cmap;
   }
}
