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

/**
 * A better NoSuchMethodException which can take a Method object
 * and formats the detail message based on in.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class NoSuchMethodException
   extends java.lang.NoSuchMethodException
{
   /**
    * Construct a <tt>NoSuchMethodException</tt> with the specified detail 
    * message.
    *
    * @param msg  Detail message.
    */
   public NoSuchMethodException(String msg) {
      super(msg);
   }

   /**
    * Construct a <tt>NoSuchMethodException</tt> using the given method
    * object to construct the detail message.
    *
    * @param method  Method to determine detail message from.
    */
   public NoSuchMethodException(Method method) {
      super(format(method));
   }

   /**
    * Construct a <tt>NoSuchMethodException</tt> using the given method
    * object to construct the detail message.
    *
    * @param msg     Detail message prefix.
    * @param method  Method to determine detail message suffix from.
    */
   public NoSuchMethodException(String msg, Method method) {
      super(msg + format(method));
   }
   
   /**
    * Construct a <tt>NoSuchMethodException</tt> with no detail.
    */
   public NoSuchMethodException() {
      super();
   }

   /**
    * Return a string representation of the given method object.
    */
   public static String format(Method method)
   {
      StringBuffer buffer = new StringBuffer();
      buffer.append(method.getName()).append("(");
      Class[] paramTypes = method.getParameterTypes();
      for (int count = 0; count < paramTypes.length; count++) {
         if (count > 0) {
            buffer.append(",");
         }
         buffer.
            append(paramTypes[count].getName().substring(paramTypes[count].getName().lastIndexOf(".")+1));
      }
      buffer.append(")");

      return buffer.toString();
   }
}
