/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Array;

import java.lang.ref.Reference;

import java.io.IOException;
import java.io.ObjectOutputStream;
import java.io.ObjectInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ByteArrayInputStream;
import java.io.Serializable;

import org.jboss.util.coerce.CoercionHandler;
import org.jboss.util.stream.Streams;

/**
 * A collection of <code>Object</code> utilities.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public final class Objects
{
   /////////////////////////////////////////////////////////////////////////
   //                           Coercion Methods                          //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Get a compatible constructor for the given value type
    *
    * @param type       Class to look for constructor in
    * @param valueType  Argument type for constructor
    * @return           Constructor or null
    */
   public static Constructor getCompatibleConstructor(final Class type,
                                                      final Class valueType)
   {
      // first try and find a constructor with the exact argument type
      try {
         return type.getConstructor(new Class[] { valueType });
      }
      catch (Exception ignore) {
         // if the above failed, then try and find a constructor with
         // an compatible argument type

         // get an array of compatible types
         Class[] types = type.getClasses();

         for (int i=0; i<types.length; i++) {
            try {
               return type.getConstructor(new Class[] { types[i] });
            }
            catch (Exception ignore2) {}
         }
      }

      // if we get this far, then we can't find a compatible constructor
      return null;
   }

   /**
    * Coerce the given value into the specified type.
    *
    * @param value   Value to coerce.
    * @param type    Class type to coerce to.
    * @return        Coerced object.
    *
    * <p>Primative classes will be translated into their respective
    *    wrapper class as needed.
    *
    * @exception NotCoercibleException    Value is not corecible.
    * @exception CoercionException        Failed to coerce.
    */
   public static Object coerce(final Object value, final Class type)
      throws CoercionException
   {
      // get the class for the given value
      Class valueType = value.getClass();

      // if value typeis assignable (aka castable) from type then return value
      if (type.isAssignableFrom(valueType)) {
         return value;
      }

      // if the object is Coercible, then let it do the work
      if (value instanceof Coercible) {
         return ((Coercible)value).coerce(type);
      }

      // find a handler that can take a type object, let it decide if it
      // can actually coerce the correct object from value
      if (CoercionHandler.isInstalled(type)) {
         CoercionHandler handler = CoercionHandler.create(type);
         return handler.coerce(value, type);
      }

      // see if type has a construct that takes a value object
      // 
      // NOTE: Just because the target type has a compatible constructor
      //       does not nessicarily mean that by creating that object
      //       with the source value will be the proper coercion.
      //
      Constructor c = getCompatibleConstructor(type, valueType);
      if (c != null) {
         try {
            return c.newInstance(new Object[] { value });
         }
         catch (InvocationTargetException e) {
            // include the target exception as detail
            Throwable t = e.getTargetException();
            if (t instanceof CoercionException)
               throw (CoercionException)t;
            throw new CoercionException(t);
         }
         catch (Exception e) {
            if (e instanceof CoercionException)
               throw (CoercionException)e;
            throw new CoercionException(e);
         }
      }

      // if type is a primitive, then get its wrapper, and recurse
      if (type.isPrimitive()) {
         return coerce(value, Classes.getPrimitiveWrapper(type));
      }

      // if the object was not coerced by now, throw an exception
      throw new NotCoercibleException(value);
   }

   /**
    * Coerce the given values into the specified type.
    *
    * <p>If type is an array, then an array of that type is returned
    *    else the first element from values is used.
    *
    * <p>Coerce will handle primative array types correctly by using
    *    the reflection mechanism to unwrap primative values from their
    *    wrapper classes.
    *
    * @param values  Values to coerce.
    * @param type    Class type to coerce object to.
    * @return        Coerced object.
    *
    * @exception NotCoercibleException    Value is not corecible.
    * @exception CoercionException        Failed to coerce.
    * @exception IllegalArgumentException Indexed value is <i>null</i>
    *                                     (values contains a null element).
    */
   public static Object coerce(Object values[], Class type) 
      throws CoercionException
   {
      // if the desired type is not an array, the use the first element
      // from the values list
      if (! type.isArray())
         return coerce(values[0], type);

      // create a new array that can hold objects of the desired type
      type = type.getComponentType();
      Object array = Array.newInstance(type, values.length);

      // coerce each element in the values array into the new array
      for (int i=0; i<values.length; i++) {
         // complain if any items in the list are null
         if (values[i] == null)
            throw new IllegalArgumentException("values[" + i + "] is null");
         
         // attempt to coerce the value to the specified type
         Object coerced = coerce(values[i], type);

         // will unwrap any classes to primatives as needed
         Array.set(array, i, coerced);
      }

      return array;
   }


   /////////////////////////////////////////////////////////////////////////
   //                            Cloning Methods                          //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Copy an serializable object deeply.
    *
    * @param obj  Object to copy.
    * @return     Copied object.
    *
    * @throws IOException
    * @throws ClassCastException
    */
   public static Object copy(final Serializable obj)
      throws IOException, ClassNotFoundException
   {
      ObjectOutputStream out = null;
      ObjectInputStream in = null;
      Object copy = null;
      
      try {
         // write the object
         ByteArrayOutputStream baos = new ByteArrayOutputStream();
         out = new ObjectOutputStream(baos);
         out.writeObject(obj);
         out.flush();

         // read in the copy
         byte data[] = baos.toByteArray();
         ByteArrayInputStream bais = new ByteArrayInputStream(data);
         in = new ObjectInputStream(bais);
         copy = in.readObject();
      }
      finally {
         Streams.close(out);
         Streams.close(in);
      }

      return copy;
   }
   

   /////////////////////////////////////////////////////////////////////////
   //                              Misc Methods                           //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Dereference the given object if it is <i>non-null</i> and is an
    * instance of <code>Reference</code>.  If the object is <i>null</i>
    * then <i>null</i> is returned.  If the object is not an instance of
    * <code>Reference</code>, then the object is returned.
    *
    * @param obj  Object to dereference.
    * @return     Dereferenced object.
    */
   public static Object deref(final Object obj) {
      if (obj != null && obj instanceof Reference) {
         Reference ref = (Reference)obj;
         return ref.get();
      }

      return obj;
   }
      
   /**
    * Check if the given object is an array (primitve or native).
    *
    * @param obj  Object to test.
    * @return     True of the object is an array.
    */
   public static boolean isArray(final Object obj) {
      if (obj != null)
         return obj.getClass().isArray();
      return false;
   }

   /**
    * Return an Object array for the given object.
    *
    * @param obj  Object to convert to an array.  Converts primitive
    *             arrays to Object arrays consisting of their wrapper
    *             classes.  If the object is not an array (object or primitve)
    *             then a new array of the given type is created and the
    *             object is set as the sole element.
    */
   public static Object[] toArray(final Object obj) {
      // if the object is an array, the cast and return it.
      if (obj instanceof Object[]) {
         return (Object[])obj;
      }

      // if the object is an array of primitives then wrap the array
      Class type = obj.getClass();
      Object array; 
      if (type.isArray()) {
         int length = Array.getLength(obj);
         Class componentType = type.getComponentType();
         array = Array.newInstance(componentType, length);
         for (int i=0; i<length; i++) {
            Array.set(array, i, Array.get(obj, i));
         }
      }
      else {
         array = Array.newInstance(type, 1);
         Array.set(array, 0, obj);
      }

      return (Object[])array;
   }

   /**
    * Test the equality of two object arrays.
    *
    * @param a       The first array.
    * @param b       The second array.
    * @param deep    True to traverse elements which are arrays.
    * @return        True if arrays are equal.
    */
   public static boolean equals(final Object[] a, final Object[] b,
                                final boolean deep)
   {
      if (a == b) return true;
      if (a == null || b == null) return false;
      if (a.length != b.length) return false;

      for (int i=0; i<a.length; i++) {
         Object x = a[i];
         Object y = b[i];

         if (x != y) return false;
         if (x == null || y == null) return false;
         if (deep) {
            if (x instanceof Object[] && y instanceof Object[]) {
               if (! equals((Object[])x, (Object[])y, true)) return false;
            }
            else {
               return false;
            }
         }
         if (! x.equals(y)) return false;
      }

      return true;
   }

   /**
    * Test the equality of two object arrays.
    *
    * @param a    The first array.
    * @param b    The second array.
    * @return     True if arrays are equal.
    */
   public static boolean equals(final Object[] a, final Object[] b) {
      return equals(a, b, true);
   }
}
