/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.capability;

import org.jboss.mx.metadata.AttributeOperationResolver;

import javax.management.Attribute;
import javax.management.AttributeList;
import javax.management.AttributeNotFoundException;
import javax.management.DynamicMBean;
import javax.management.InvalidAttributeValueException;
import javax.management.MBeanAttributeInfo;
import javax.management.MBeanConstructorInfo;
import javax.management.MBeanException;
import javax.management.MBeanInfo;
import javax.management.MBeanNotificationInfo;
import javax.management.MBeanOperationInfo;
import javax.management.NotificationBroadcaster;
import javax.management.ReflectionException;
import javax.management.RuntimeErrorException;
import javax.management.RuntimeMBeanException;
import javax.management.RuntimeOperationsException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Iterator;

/**
 * Directs DynamicMBean calls to underlying resource via reflection. It's suitable
 * for use as a StandardMBean or as the resource for a ModelMBean.
 *
 * @author  <a href="mailto:trevor@protocool.com">Trevor Squires</a>.
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>
 */
public class ReflectedMBeanDispatcher implements DynamicMBean
{
   private Object resource = null;

   private AttributeOperationResolver resolver = null;
   private MBeanConstructorInfo[] constructorInfo = null;
   private MBeanAttributeInfo[] attributeInfo = null;
   private MBeanOperationInfo[] operationInfo = null;

   private Method[] operations = null;
   private MethodPair[] attributes = null;

   private boolean isBroadcaster = false;

   private String resourceClassName = null;
   private String resourceDescription = null;

   public ReflectedMBeanDispatcher(MBeanInfo mbinfo, AttributeOperationResolver resolver, Object resource)
   {
      if (null == resource)
      {
         throw new IllegalArgumentException("resource cannot be null");
      }

      if (null == mbinfo)
      {
         throw new IllegalArgumentException("MBeanInfo cannot be null");
      }

      if (null == resolver)
      {
         throw new IllegalArgumentException("AOresolver cannot be null");
      }


      if (resource instanceof NotificationBroadcaster)
      {
         this.isBroadcaster = true;
      }

      this.resource = resource;
      this.resolver = resolver;
      this.resourceClassName = mbinfo.getClassName();
      this.resourceDescription = mbinfo.getDescription();
      this.constructorInfo = mbinfo.getConstructors();
      this.attributeInfo = mbinfo.getAttributes();
      this.operationInfo = mbinfo.getOperations();

      this.operations = new Method[operationInfo.length];
      this.attributes = new MethodPair[attributeInfo.length];
   }

   public void bindOperationAt(int position, Method method)
   {
      try
      {
         operations[position] = method;
      }
      catch (ArrayIndexOutOfBoundsException e)
      {
         throw new IllegalArgumentException("position out of bounds: " + position);
      }
   }

   public void bindAttributeAt(int position, Method getter, Method setter)
   {
      try
      {
         attributes[position] = new MethodPair(getter, setter);
      }
      catch (ArrayIndexOutOfBoundsException e)
      {
         throw new IllegalArgumentException("position out of bounds: " + position);
      }
   }

   public String getResourceClassName()
   {
      return resourceClassName;
   }

   public Object getAttribute(String attribute)
      throws AttributeNotFoundException, MBeanException, ReflectionException
   {
      if (null == attribute)
      {
         throw new RuntimeOperationsException(new IllegalArgumentException("attribute cannot be null"));
      }

      Method m = null;
      try
      {
         m = attributes[resolver.lookup(attribute).intValue()].getter;
         return m.invoke(resource, new Object[0]);
      }
      catch (NullPointerException e)
      {
         throw new AttributeNotFoundException("Readable attribute '" + attribute + "' not found");
      }
      catch (ArrayIndexOutOfBoundsException e)
      {
         throw new AttributeNotFoundException("Readable attribute '" + attribute + "' not found");
      }
      catch (InvocationTargetException e)
      {
         Throwable t = e.getTargetException();
         if (t instanceof RuntimeException)
         {
            throw new RuntimeMBeanException((RuntimeException) t, "RuntimeException in MBean when getting attribute '" + attribute + "'");
         }
         else if (t instanceof Exception)
         {
            throw new MBeanException((Exception) t, "Exception in MBean when getting attribute '" + attribute + "'");
         }
         else // it's an error
         {
            throw new RuntimeErrorException((Error) t, "Error in MBean when getting attribute '" + attribute + "'");
         }
      }
      catch (IllegalArgumentException e)
      {
         throw new AttributeNotFoundException("Readable attribute '" + attribute + "' not found");
      }
      catch (Exception e) // assume all other exceptions are reflection related
      {
         throw new ReflectionException(e, "Exception in AttributeProvider for getting '" + attribute + "'");
      }
      catch (Error e)
      {
         throw new RuntimeErrorException(e, "Error in AttributeProvider for getting '" + attribute + "'");
      }
   }


   public void setAttribute(Attribute attribute)
      throws AttributeNotFoundException, InvalidAttributeValueException,
      MBeanException, ReflectionException
   {
      if (null == attribute)
      {
         throw new RuntimeOperationsException(new IllegalArgumentException("attribute cannot be null"));
      }

      Method m = null;
      try
      {
         m = attributes[resolver.lookup(attribute.getName()).intValue()].setter;
         m.invoke(resource, new Object[]{attribute.getValue()});
      }
      catch (NullPointerException e)
      {
         throw new AttributeNotFoundException("Writable attribute '" + attribute.getName() + "' not found");
      }
      catch (ArrayIndexOutOfBoundsException e)
      {
         throw new AttributeNotFoundException("Writable attribute '" + attribute.getName() + "' not found");
      }
      catch (InvocationTargetException e)
      {
         Throwable t = e.getTargetException();
         if (t instanceof RuntimeException)
         {
            throw new RuntimeMBeanException((RuntimeException) t, "RuntimeException in MBean when setting attribute '" + attribute.getName() + "'");
         }
         else if (t instanceof Exception)
         {
            throw new MBeanException((Exception) t, "Exception in MBean when setting attribute '" + attribute.getName() + "'");
         }
         else // it's an error
         {
            throw new RuntimeErrorException((Error) t, "Error in MBean when setting attribute '" + attribute.getName() + "'");
         }
      }
      catch (IllegalArgumentException e)
      {
         String valueType = (null == attribute.getValue()) ? "<null value>" : attribute.getValue().getClass().getName();
         throw new InvalidAttributeValueException("Attribute value mismatch while setting '" + attribute.getName() + "': " + valueType);
      }
      catch (Exception e) // assume all other exceptions are reflection related
      {
         throw new ReflectionException(e, "Exception in AttributeProvider for setting '" + attribute.getName() + "'");
      }
      catch (Error e)
      {
         throw new RuntimeErrorException(e, "Error in AttributeProvider for setting '" + attribute.getName() + "'");
      }
   }


   public AttributeList getAttributes(String[] attributes)
   {
      if (null == attributes)
      {
         throw new RuntimeOperationsException(new IllegalArgumentException("attributes array cannot be null"));
      }

      AttributeList list = new AttributeList();
      for (int i = 0; i < attributes.length; i++)
      {
         try
         {
            list.add(new Attribute(attributes[i], getAttribute(attributes[i])));
         }
         catch (Throwable e)
         {
            // QUERY - do we *really* just ignore all problems?
         }
      }

      return list;
   }

   public AttributeList setAttributes(AttributeList attributes)
   {
      if (null == attributes)
      {
         throw new RuntimeOperationsException(new IllegalArgumentException("attribute list cannot be null"));
      }

      AttributeList list = new AttributeList();
      for (Iterator iterator = attributes.iterator(); iterator.hasNext();)
      {
         Attribute toSet = (Attribute) iterator.next();
         try
         {
            setAttribute(toSet);
            list.add(toSet);
         }
         catch (Throwable e)
         {
            // QUERY - do we *really* just ignore all problems?
         }
      }
      return list;
   }

   public Object invoke(String actionName,
                        Object[] params,
                        String[] signature)
      throws MBeanException, ReflectionException
   {
      Method m = null;
      try
      {
         m = operations[resolver.lookup(actionName, signature).intValue()];
         return m.invoke(resource, params);
      }
      catch (NullPointerException e)
      {
         throw new ReflectionException(new NoSuchMethodException("Unable to locate MBean operation for: " + opKeyString(actionName, signature)));
      }
      catch (ArrayIndexOutOfBoundsException e)
      {
         throw new ReflectionException(new NoSuchMethodException("Unable to locate MBean operation for: " + opKeyString(actionName, signature)));
      }
      catch (InvocationTargetException e)
      {
         Throwable t = e.getTargetException();
         if (t instanceof RuntimeException)
         {
            throw new RuntimeMBeanException((RuntimeException) t, "RuntimeException in MBean operation '" + opKeyString(actionName, signature) + "'");
         }
         else if (t instanceof Exception)
         {
            throw new MBeanException((Exception) t, "Exception in MBean operation '" + opKeyString(actionName, signature) + "'");
         }
         else // it's an error
         {
            throw new RuntimeErrorException((Error) t, "Error in MBean operation '" + opKeyString(actionName, signature) + "'");
         }
      }
      catch (Exception e) // assume all other exceptions are reflection related
      {
         throw new ReflectionException(e, "Exception when calling method for '" + opKeyString(actionName, signature) + "'");
      }
      catch (Error e)
      {
         throw new RuntimeErrorException(e, "Error when calling method for '" + opKeyString(actionName, signature) + "'");
      }

   }

   public MBeanInfo getMBeanInfo()
   {
      return new MBeanInfo(resourceClassName, resourceDescription,
                           attributeInfo, constructorInfo,
                           operationInfo, (isBroadcaster) ? this.getNotificationInfo() : new MBeanNotificationInfo[0]);

   }

   // Protected -----------------------------------------------------
   protected MBeanNotificationInfo[] getNotificationInfo()
   {
      if (isBroadcaster)
      {
         return ((NotificationBroadcaster) resource).getNotificationInfo();
      }
      else
      {
         throw new RuntimeOperationsException(new UnsupportedOperationException("resource is not a NotificationBroadcaster"));
      }
   }

   protected Object getResourceObject()
   {
      return resource;
   }

   // ONLY used for friendly exceptions!
   protected final String opKeyString(String name, String[] signature)
   {
      StringBuffer buf = new StringBuffer(name).append('(');
      if (null != signature)
      {
         for (int i = 0; i < signature.length-1; i++)
            buf.append(signature[i]).append(',');
         if (signature.length > 0)
            buf.append(signature[signature.length-1]);
      }
      return buf.append(')').toString();
   }

   public static class MethodPair
   {
      public Method getter = null;
      public Method setter = null;

      public MethodPair(Method getter, Method setter)
      {
         this.getter = getter;
         this.setter = setter;
      }
   }
}
