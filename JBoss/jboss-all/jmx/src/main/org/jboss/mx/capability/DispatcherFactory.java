/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.capability;

import org.jboss.mx.metadata.AttributeOperationResolver;
import org.jboss.mx.metadata.MethodMapper;

import org.jboss.mx.server.ServerConstants;

import javax.management.DynamicMBean;
import javax.management.IntrospectionException;
import javax.management.MBeanAttributeInfo;
import javax.management.MBeanInfo;
import javax.management.MBeanOperationInfo;
import javax.management.Descriptor;
import javax.management.modelmbean.ModelMBeanAttributeInfo;
import java.lang.reflect.Method;

/**
 * Creates and binds a dispatcher
 *
 * @author  <a href="mailto:trevor@protocool.com">Trevor Squires</a>.
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>
 */
public class DispatcherFactory
      implements ServerConstants
{

   /**
    * Creates a Dispatcher for an arbitrary resource.  Useful for when you don't care
    * about the AttributeOperationResolver.
    */
   public static DynamicMBean create(MBeanInfo info, Object resource) throws IntrospectionException
   {
      return create(info, resource, new AttributeOperationResolver(info));
   }

   /**
    * Creates a dispatcher for an arbitrary resource using the named AttributeOperationResolver.
    */
   public static DynamicMBean create(MBeanInfo info, Object resource, AttributeOperationResolver resolver) throws IntrospectionException
   {
      if (null == info)
      {
         throw new IllegalArgumentException("info cannot be null");
      }

      if (null == resolver)
      {
         throw new IllegalArgumentException("resolver cannot be null");
      }

      if (null == resource)
      {
         throw new IllegalArgumentException("resource cannot be null");
      }

      MethodMapper mmap = new MethodMapper(resource.getClass());
      ReflectedMBeanDispatcher dispatcher = new ReflectedMBeanDispatcher(info, resolver, resource);

      if (System.getProperty(OPTIMIZE_REFLECTED_DISPATCHER, "false").equalsIgnoreCase("true"))
      {
         // FIXME: subclassing for now so I can rely on the reflection based implementation for the parts
         // that aren't implemented yet
         dispatcher = OptimizedMBeanDispatcher.create(info, resource /*, parent classloader */);
      }

      MBeanAttributeInfo[] attributes = info.getAttributes();
      for (int i = 0; i < attributes.length; i++)
      {
         MBeanAttributeInfo attribute = attributes[i];
         Method getter = null;
         Method setter = null;

         if (attribute.isReadable())
         {
            if (attribute instanceof ModelMBeanAttributeInfo)
            {
               ModelMBeanAttributeInfo mmbAttribute = (ModelMBeanAttributeInfo) attribute;
               Descriptor desc = mmbAttribute.getDescriptor();
               if (desc != null && desc.getFieldValue("getMethod") != null)
               {
                  getter = mmap.lookupGetter(mmbAttribute);
                  if (getter == null)
                  {
                     throw new IntrospectionException("no getter method found for attribute: " + attribute.getName());
                  }
               }
            }
            else
            {
               getter = mmap.lookupGetter(attribute);
               if (getter == null)
               {
                  throw new IntrospectionException("no getter method found for attribute: " + attribute.getName());
               }
            }
         }

         if (attribute.isWritable())
         {
            if (attribute instanceof ModelMBeanAttributeInfo)
            {
               ModelMBeanAttributeInfo mmbAttribute = (ModelMBeanAttributeInfo) attribute;
               Descriptor desc = mmbAttribute.getDescriptor();
               if (desc != null && desc.getFieldValue("setMethod") != null)
               {
                  setter = mmap.lookupSetter(mmbAttribute);
                  if (setter == null)
                  {
                     throw new IntrospectionException("no setter method found for attribute: " + attribute.getName());
                  }
               }
            }
            else
            {
               setter = mmap.lookupSetter(attribute);
               if (setter == null)
               {
                  throw new IntrospectionException("no setter method found for attribute: " + attribute.getName());
               }
            }
         }

         dispatcher.bindAttributeAt(i, getter, setter);
      }

      MBeanOperationInfo[] operations = info.getOperations();
      for (int i = 0; i < operations.length; i++)
      {
         MBeanOperationInfo operation = operations[i];
         Method method = mmap.lookupOperation(operation);
         if (method == null)
         {
            throw new IntrospectionException("no method found for operation: " + operation.getName()); // FIXME better error!
         }

         dispatcher.bindOperationAt(i, method);
      }

      return dispatcher;
   }
}
