/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util;

import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.lang.reflect.Array;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/**
 * A collection of <code>Class</code> utilities.
 *
 * @version <tt>$Revision: 1.3.2.2 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author Scott.Stark@jboss.org
 */
public final class Classes
{
   /** The string used to separator packages */
   public static final String PACKAGE_SEPARATOR = ".";

   /** The characther used to separator packages */
   public static final char PACKAGE_SEPARATOR_CHAR = '.';

   /** The default package name. */
   public static final String DEFAULT_PACKAGE_NAME = "<default>";

   /**
    * Get the short name of the specified class by striping off the package
    * name.
    *
    * @param classname  Class name.
    * @return           Short class name.
    */
   public static String stripPackageName(final String classname)
   {
      int idx = classname.lastIndexOf(PACKAGE_SEPARATOR);

      if (idx != -1)
         return classname.substring(idx + 1, classname.length());
      return classname;
   }

   /**
    * Get the short name of the specified class by striping off the package
    * name.
    *
    * @param type    Class name.
    * @return        Short class name.
    */
   public static String stripPackageName(final Class type)
   {
      return stripPackageName(type.getName());
   }

   /**
    * Get the package name of the specified class.
    *
    * @param classname  Class name.
    * @return           Package name or "" if the classname is in the
    *                   <i>default</i> package.
    *
    * @throws EmptyStringException     Classname is an empty string.
    */
   public static String getPackageName(final String classname)
   {
      if (classname.length() == 0)
         throw new EmptyStringException();

      int index = classname.lastIndexOf(PACKAGE_SEPARATOR);
      if (index != -1)
         return classname.substring(0, index);
      return "";
   }

   /**
    * Get the package name of the specified class.
    *
    * @param type    Class.
    * @return        Package name.
    */
   public static String getPackageName(final Class type)
   {
      return getPackageName(type.getName());
   }

   /**
    * Force the given class to be loaded fully.
    *
    * <p>This method attempts to locate a static method on the given class
    *    the attempts to invoke it with dummy arguments in the hope that
    *    the virtual machine will prepare the class for the method call and
    *    call all of the static class initializers.
    *
    * @param type    Class to force load.
    *
    * @throws NullArgumentException    Type is <i>null</i>.
    */
   public static void forceLoad(final Class type)
   {
      if (type == null)
         throw new NullArgumentException("type");
      
      // don't attempt to force primitives to load
      if (type.isPrimitive()) return;

      // don't attempt to force java.* classes to load
      String packageName = Classes.getPackageName(type);
      // System.out.println("package name: " + packageName);

      if (packageName.startsWith("java.") ||
         packageName.startsWith("javax."))
      {
         return;
      }

      // System.out.println("forcing class to load: " + type);

      try
      {
         Method methods[] = type.getDeclaredMethods();
         Method method = null;
         for (int i = 0; i < methods.length; i++)
         {
            int modifiers = methods[i].getModifiers();
            if (Modifier.isStatic(modifiers))
            {
               method = methods[i];
               break;
            }
         }

         if (method != null)
         {
            method.invoke(null, null);
         }
         else
         {
            type.newInstance();
         }
      }
      catch (Exception ignore)
      {
         ThrowableHandler.add(ignore);
      }
   }


   /////////////////////////////////////////////////////////////////////////
   //                               Primitives                            //
   /////////////////////////////////////////////////////////////////////////

   /** Primitive type name -> class map. */
   private static final Map PRIMITIVE_NAME_TYPE_MAP = new HashMap();
   
   /** Setup the primitives map. */
   static
   {
      PRIMITIVE_NAME_TYPE_MAP.put("boolean", Boolean.TYPE);
      PRIMITIVE_NAME_TYPE_MAP.put("byte", Byte.TYPE);
      PRIMITIVE_NAME_TYPE_MAP.put("char", Character.TYPE);
      PRIMITIVE_NAME_TYPE_MAP.put("short", Short.TYPE);
      PRIMITIVE_NAME_TYPE_MAP.put("int", Integer.TYPE);
      PRIMITIVE_NAME_TYPE_MAP.put("long", Long.TYPE);
      PRIMITIVE_NAME_TYPE_MAP.put("float", Float.TYPE);
      PRIMITIVE_NAME_TYPE_MAP.put("double", Double.TYPE);
   }

   /**
    * Get the primitive type for the given primitive name.
    *
    * <p>
    * For example, "boolean" returns Boolean.TYPE and so on...
    *
    * @param name    Primitive type name (boolean, int, byte, ...)
    * @return        Primitive type or null.
    *
    * @exception IllegalArgumentException    Type is not a primitive class
    */
   public static Class getPrimitiveTypeForName(final String name)
   {
      return (Class) PRIMITIVE_NAME_TYPE_MAP.get(name);
   }

   /** Map of primitive types to their wrapper classes */
   private static final Class[] PRIMITIVE_WRAPPER_MAP = {
      Boolean.TYPE, Boolean.class,
      Byte.TYPE, Byte.class,
      Character.TYPE, Character.class,
      Double.TYPE, Double.class,
      Float.TYPE, Float.class,
      Integer.TYPE, Integer.class,
      Long.TYPE, Long.class,
      Short.TYPE, Short.class,
   };

   /**
    * Get the wrapper class for the given primitive type.
    *
    * @param type    Primitive class.
    * @return        Wrapper class for primitive.
    *
    * @exception IllegalArgumentException    Type is not a primitive class
    */
   public static Class getPrimitiveWrapper(final Class type)
   {
      if (!type.isPrimitive())
      {
         throw new IllegalArgumentException("type is not a primitive class");
      }

      for (int i = 0; i < PRIMITIVE_WRAPPER_MAP.length; i += 2)
      {
         if (type.equals(PRIMITIVE_WRAPPER_MAP[i]))
            return PRIMITIVE_WRAPPER_MAP[i + 1];
      }

      // should never get here, if we do then PRIMITIVE_WRAPPER_MAP
      // needs to be updated to include the missing mapping
      throw new UnreachableStatementException();
   }

   /**
    * Check if the given class is a primitive wrapper class.
    *
    * @param type    Class to check.
    * @return        True if the class is a primitive wrapper.
    */
   public static boolean isPrimitiveWrapper(final Class type)
   {
      for (int i = 0; i < PRIMITIVE_WRAPPER_MAP.length; i += 2)
      {
         if (type.equals(PRIMITIVE_WRAPPER_MAP[i + 1]))
         {
            return true;
         }
      }

      return false;
   }

   /**
    * Check if the given class is a primitive class or a primitive 
    * wrapper class.
    *
    * @param type    Class to check.
    * @return        True if the class is a primitive or primitive wrapper.
    */
   public static boolean isPrimitive(final Class type)
   {
      if (type.isPrimitive() || isPrimitiveWrapper(type))
      {
         return true;
      }

      return false;
   }
   /** Check type against boolean, byte, char, short, int, long, float, double.
    * @param The java type name
    * @return true if this is a primative type name.
    */ 
   public static boolean isPrimitive(final String type)
   {
      return PRIMITIVE_NAME_TYPE_MAP.containsKey(type); 
   }

   /////////////////////////////////////////////////////////////////////////
   //                            Class Loading                            //
   /////////////////////////////////////////////////////////////////////////

   /**
    * This method acts equivalently to invoking
    * <code>Thread.currentThread().getContextClassLoader().loadClass(className);</code> but it also
    * supports primitive types and array classes of object types or primitive types.
    *
    * @param className    the qualified name of the class or the name of primitive type or
    *                     array in the same format as returned by the
    *                     <code>java.lang.Class.getName()</code> method.
    * @return             the Class object for the requested className
    *
    * @throws ClassNotFoundException when the <code>classLoader</code> can not find the requested class
    */
   public static Class loadClass(String className) throws ClassNotFoundException
   {
      return loadClass(className, Thread.currentThread().getContextClassLoader());
   }

   /**
    * This method acts equivalently to invoking classLoader.loadClass(className)
    * but it also supports primitive types and array classes of object types or
    * primitive types.
    *
    * @param className the qualified name of the class or the name of primitive
    * type or array in the same format as returned by the
    * java.lang.Class.getName() method.
    * @param classLoader  the ClassLoader used to load classes
    * @return             the Class object for the requested className
    *
    * @throws ClassNotFoundException when the <code>classLoader</code> can not
    * find the requested class
    */
   public static Class loadClass(String className, ClassLoader classLoader)
      throws ClassNotFoundException
   {
      // ClassLoader.loadClass() does not handle primitive types:
      //
      //   B            byte
      //   C            char
      //   D            double
      //   F            float
      //   I            int
      //   J            long
      //   S            short
      //   Z            boolean
      //   V	         void
      //
      if (className.length() == 1)
      {
         char type = className.charAt(0);
         if (type == 'B') return Byte.TYPE;
         if (type == 'C') return Character.TYPE;
         if (type == 'D') return Double.TYPE;
         if (type == 'F') return Float.TYPE;
         if (type == 'I') return Integer.TYPE;
         if (type == 'J') return Long.TYPE;
         if (type == 'S') return Short.TYPE;
         if (type == 'Z') return Boolean.TYPE;
         if (type == 'V') return Void.TYPE;
         // else throw...
         throw new ClassNotFoundException(className);
      }

      // Check for a primative type
      if( isPrimitive(className) == true )
         return (Class) Classes.PRIMITIVE_NAME_TYPE_MAP.get(className);

      // Check for the internal vm format: Lclassname;
      if (className.charAt(0) == 'L' && className.charAt(className.length() - 1) == ';')
         return classLoader.loadClass(className.substring(1, className.length() - 1));
      
      // first try - be optimistic
      // this will succeed for all non-array classes and array classes that have already been resolved
      //
      try
      {
         return classLoader.loadClass(className);
      }
      catch (ClassNotFoundException e)
      {
         // if it was non-array class then throw it
         if (className.charAt(0) != '[')
            throw e;
      }
   
      // we are now resolving array class for the first time
      
      // count opening braces
      int arrayDimension = 0;
      while (className.charAt(arrayDimension) == '[')
         arrayDimension++;
            
      // resolve component type - use recursion so that we can resolve primitive types also
      Class componentType = loadClass(className.substring(arrayDimension), classLoader);
      
      // construct array class
      return Array.newInstance(componentType, new int[arrayDimension]).getClass();
   }

   /**
    * Convert a list of Strings from an Interator into an array of
    * Classes (the Strings are taken as classnames).
    *
    * @param it A java.util.Iterator pointing to a Collection of Strings
    * @param cl The ClassLoader to use
    *
    * @return Array of Classes
    *
    * @throws ClassNotFoundException When a class could not be loaded from
    *         the specified ClassLoader
    */
   public final static Class[] convertToJavaClasses(Iterator it,
      ClassLoader cl)
      throws ClassNotFoundException
   {
      ArrayList classes = new ArrayList();
      while (it.hasNext())
      {
         classes.add(convertToJavaClass((String) it.next(), cl));
      }
      return (Class[]) classes.toArray(new Class[classes.size()]);
   }

   /**
    * Convert a given String into the appropriate Class.
    *
    * @param name Name of class
    * @param cl ClassLoader to use
    *
    * @return The class for the given name
    *
    * @throws ClassNotFoundException When the class could not be found by
    *         the specified ClassLoader
    */
   private final static Class convertToJavaClass(String name,
      ClassLoader cl)
      throws ClassNotFoundException
   {
      int arraySize = 0;
      while (name.endsWith("[]"))
      {
         name = name.substring(0, name.length() - 2);
         arraySize++;
      }

      // Check for a primitive type
      Class c = (Class) PRIMITIVE_NAME_TYPE_MAP.get(name);

      if (c == null)
      {
         // No primitive, try to load it from the given ClassLoader
         try
         {
            c = cl.loadClass(name);
         }
         catch (ClassNotFoundException cnfe)
         {
            throw new ClassNotFoundException("Parameter class not found: " +
               name);
         }
      }

      // if we have an array get the array class
      if (arraySize > 0)
      {
         int[] dims = new int[arraySize];
         for (int i = 0; i < arraySize; i++)
         {
            dims[i] = 1;
         }
         c = Array.newInstance(c, dims).getClass();
      }

      return c;
   }

}

/*
vim:ts=3:sw=3:et
*/
