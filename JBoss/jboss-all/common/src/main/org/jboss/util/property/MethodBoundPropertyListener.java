/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.property;

import java.beans.Introspector;
import java.beans.IntrospectionException;
import java.beans.PropertyDescriptor;
import java.beans.BeanInfo;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

import org.jboss.util.ThrowableHandler;
import org.jboss.util.Objects;

/**
 * Binds property values to class methods.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class MethodBoundPropertyListener
   extends BoundPropertyAdapter
{
   /** Property name which we are bound to */
   protected final String propertyName;

   /** Instance object that contains setter method */
   protected final Object instance;

   /** Property setter method */
   protected final Method setter;

   /** Property descriptor */
   protected final PropertyDescriptor descriptor;

   /**
    * Construct a MethodBoundPropertyListener.
    *
    * @param instance         Instance object that contains setter method.
    * @param propertyName     The name of the property which will be bound.
    * @param beanPropertyName The name of the property setter method.
    *
    * @throws PropertyException
    */
   public MethodBoundPropertyListener(final Object instance,
                                      final String propertyName,
                                      final String beanPropertyName)
   {
      this.instance = instance;
      this.propertyName = propertyName;

      try {
         descriptor = getPropertyDescriptor(beanPropertyName);
         if (descriptor == null) {
            throw new PropertyException
               ("missing method for: " + beanPropertyName);
         }

         setter = descriptor.getWriteMethod();
         if (setter == null) {
            throw new PropertyException
               ("missing setter method for: " + beanPropertyName);
         }
         try {
            setter.setAccessible(true);
         }
         catch (SecurityException e) {
            ThrowableHandler.add(e);
         }
      }
      catch (IntrospectionException e) {
         throw new PropertyException(e);
      }
   }

   /**
    * Get the <tt>PropertyDescriptor</tt> for the given bean property name.
    *
    * @param beanPropertyName    Bean property name.
    * @return                    <tt>PropertyDescriptor</tt>.
    */
   private PropertyDescriptor getPropertyDescriptor(final String beanPropertyName)
      throws IntrospectionException
   {
      Class instanceType = instance.getClass();
      BeanInfo beanInfo = Introspector.getBeanInfo(instanceType);
      PropertyDescriptor descriptors[] = beanInfo.getPropertyDescriptors();
      PropertyDescriptor descriptor = null;

      for (int i=0; i<descriptors.length; i++) {
         if (descriptors[i].getName().equals(beanPropertyName)) {
            descriptor = descriptors[i];
            break;
         }
      }

      return descriptor;
   }

   /**
    * Construct a MethodBoundPropertyListener.
    *
    * @param instance         Instance object that contains setter method.
    * @param propertyName     The name of the property which will be bound.
    */
   public MethodBoundPropertyListener(final Object instance,
                                      final String propertyName)
   {
      this(instance, propertyName, propertyName);
   }

   /**
    * Get the property name which this listener is bound to.
    *
    * @return     Property name.
    */
   public final String getPropertyName() {
      return propertyName;
   }

   /**
    * Coerce and invoke the property setter method on the instance.
    *
    * @param value   Method value.
    *
    * @throws PropertyException     Failed to invoke setter method.
    */
   protected void invokeSetter(String value) {
      try {
         // coerce value to field type
         Class type = descriptor.getPropertyType();
         Object coerced = Objects.coerce(value, type);
         // System.out.println("type: " + type);
         // System.out.println("coerced: " + coerced);

         // invoke the setter method
         setter.invoke(instance, new Object[] { coerced });
      }
      catch (InvocationTargetException e) {
         Throwable target = e.getTargetException();
         if (target instanceof PropertyException) {
            throw (PropertyException)target;
         }
         else {
            throw new PropertyException(target);
         }
      }
      catch (Exception e) {
         throw new PropertyException(e);
      }
   }

   /**
    * Notifies that a property has been added.
    *
    * @param event   Property event.
    */
   public void propertyAdded(final PropertyEvent event) {
      invokeSetter(event.getPropertyValue());
   }

   /**
    * Notifies that a property has changed.
    *
    * @param event   Property event.
    */
   public void propertyChanged(final PropertyEvent event) {
      invokeSetter(event.getPropertyValue());
   }

   /**
    * Notifies that this listener was bound to a property.
    *
    * @param map     PropertyMap which contains property bound to.
    */
   public void propertyBound(final PropertyMap map) {
      // only set the field if the map contains the property already
      if (map.containsProperty(propertyName)) {
         invokeSetter(map.getProperty(propertyName));
      }
   }
}
