/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.jaws.metadata;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;
import java.util.StringTokenizer;

/**
 * Provide static method to obtain a private attribute of a value object using
 * its get method, and to set the value of a private attribute using its set
 * method.<p>
 * WARNING : These methods use Reflection so OK to run them at deployment time
 * but at run time, we should better catch the Method get/set at CMPFieldMetaData
 * level.  So that the only invoke() method will be called at run time.  This still
 * needs to be done.
 *
 * @author <a href="mailto:vincent.harcq@hubmethods.com">Vincent Harcq</a>
 * @version $Revision: 1.3 $
 */
public class ValueObjectHelper
{

   // Public --------------------------------------------------------

   public static Object getValue(Object o, String fieldName)
      throws NoSuchMethodException, InvocationTargetException,
         IllegalAccessException
   {
      Method m=o.getClass().getMethod(getGetMethod(fieldName),null);
      return m.invoke(o,null);
   }

   public static void setValue(Object o, String fieldName, Object value)
      throws NoSuchMethodException, InvocationTargetException,
         IllegalAccessException
   {
   	  if (value == null) return;
      Class[] paramTypes= { value.getClass() };
      Method m=o.getClass().getMethod(getSetMethod(fieldName),paramTypes);
      Object[] args = { value };
      m.invoke(o,args);
   }

   /**
    * @param ejbClass the class in which the first occurance of name will be found
    * @param name must be in the form address.line1
    */
   public static Class getNestedFieldType(Class ejbClass, String name)
   throws NoSuchMethodException{
      StringTokenizer st = new StringTokenizer(name,".");
      Class clazz = ejbClass;
      while (st.hasMoreTokens())
      {
         String tmp = st.nextToken();
         try
         {
             // First try to find a public Field
             Field tmpField = clazz.getField(tmp);
             clazz = tmpField.getType();
         }
         catch (NoSuchFieldException e)
         {
             // Else try to find the getMethod
             Method m = clazz.getMethod(getGetMethod(tmp),null);
             // TODO what about primitive types ?
             clazz = m.getReturnType();
         }
      }
      return clazz;
   }

   // Private -------------------------------------------------------

   private static String getGetMethod(String fieldName)
   {
      char firstLetter = Character.toUpperCase(fieldName.charAt(0));
      String then = fieldName.substring(1);
      StringBuffer stBuf = new StringBuffer("get");
      stBuf.append(firstLetter);
      stBuf.append(then);
      return stBuf.toString();
   }

   private static String getSetMethod(String fieldName)
   {
      char firstLetter = Character.toUpperCase(fieldName.charAt(0));
      String then = fieldName.substring(1);
      StringBuffer stBuf = new StringBuffer("set");
      stBuf.append(firstLetter);
      stBuf.append(then);
      return stBuf.toString();
   }

}
