/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util;

import java.io.PrintWriter;
import java.io.PrintStream;
import java.io.Serializable;

import org.jboss.logging.Logger;

import org.jboss.util.platform.Java;


/**
 * Interface which is implemented by all the nested throwable flavors.
 *
 * @version <tt>$Revision: 1.6 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public interface NestedThrowable
   extends Serializable
{
   /**
    * A system wide flag to enable or disable printing of the
    * parent throwable traces.
    *
    * <p>
    * This value is set from the system property
    * <tt>org.jboss.util.NestedThrowable.parentTraceEnabled</tt>
    * or if that is not set defaults to <tt>true</tt>.
    */
   boolean PARENT_TRACE_ENABLED = Util.getBoolean("parentTraceEnabled", true);

   /**
    * A system wide flag to enable or disable printing of the
    * nested detail throwable traces.
    *
    * <p>
    * This value is set from the system property
    * <tt>org.jboss.util.NestedThrowable.nestedTraceEnabled</tt>
    * or if that is not set defaults to <tt>true</tt> unless
    * using JDK 1.4 with {@link #PARENT_TRACE_ENABLED} set to false,
    * then <tt>false</tt> since there is a native mechansim for this there.
    *
    * <p>
    * Note then when running under 1.4 is is not possible to disable
    * the nested trace output, since that is handled by java.lang.Throwable
    * which we delegate the parent printing to.
    */
   boolean NESTED_TRACE_ENABLED = Util.getBoolean("nestedTraceEnabled",
                                                  (Java.isCompatible(Java.VERSION_1_4) &&
                                                   !PARENT_TRACE_ENABLED) ||
                                                  !Java.isCompatible(Java.VERSION_1_4));

   /**
    * A system wide flag to enable or disable checking of parent and child
    * types to detect uneeded nesting
    *
    * <p>
    * This value is set from the system property
    * <tt>org.jboss.util.NestedThrowable.detectDuplicateNesting</tt>
    * or if that is not set defaults to <tt>true</tt>.
    */
   boolean DETECT_DUPLICATE_NESTING = Util.getBoolean("detectDuplicateNesting", true);
   
   /**
    * Return the nested throwable.
    *
    * @return  Nested throwable.
    */
   Throwable getNested();

   /**
    * Return the nested <tt>Throwable</tt>.
    *
    * <p>For JDK 1.4 compatibility.
    *
    * @return  Nested <tt>Throwable</tt>.
    */
   Throwable getCause();


   /////////////////////////////////////////////////////////////////////////
   //                      Nested Throwable Utilities                     //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Utilitiy methods for the various flavors of
    * <code>NestedThrowable</code>.
    */
   final class Util 
   {
      // Can not be final due to init bug, see getLogger() for details
      private static Logger log = Logger.getLogger(NestedThrowable.class);

      /**
       * Something is very broken with class nesting, which can sometimes
       * leave log uninitialized durring one of the following method calls.
       *
       * <p>
       * This is a HACK to keep those methods from NPE until this problem
       * can be resolved.
       */
      private static Logger getLogger()
      {
         if (log == null)
            log = Logger.getLogger(NestedThrowable.class);

         return log;
      }
      
      /** A helper to get a boolean property. */
      protected static boolean getBoolean(String name, boolean defaultValue)
      {
         name = NestedThrowable.class.getName() + "." + name;
         String value = System.getProperty(name, String.valueOf(defaultValue));

         // HACK see getLogger() for details
         log = getLogger();
         
         log.debug(name + "=" + value);

         return new Boolean(value).booleanValue();
      }

      /**
       * Check and possibly warn if the nested exception type is the same
       * as the parent type (duplicate nesting).
       */
      public static void checkNested(final NestedThrowable parent,
                                     final Throwable child)
      {
         if (!DETECT_DUPLICATE_NESTING || parent == null || child == null) return;

         Class parentType = parent.getClass();
         Class childType = child.getClass();

         //
         // This might be backwards... I always get this confused
         //
         
         if (parentType.isAssignableFrom(childType)) {
            // HACK see getLogger() for details
            log = getLogger();

            log.warn("Duplicate throwable nesting of same base type: " +
                     parentType + " is assignable from: " + childType);
         }
      }
      
      /**
       * Returns a formated message for the given detail message
       * and nested <code>Throwable</code>.
       *
       * @param msg     Detail message.
       * @param nested  Nested <code>Throwable</code>.
       * @return        Formatted message.
       */
      public static String getMessage(final String msg,
                                      final Throwable nested)
      {
         StringBuffer buff = new StringBuffer(msg == null ? "" : msg);

         if (nested != null) {
            buff.append(msg == null ? "- " : "; - ")
               .append("nested throwable: (")
               .append(nested)
               .append(")");
         }
 
         return buff.toString();
      }

      /**
       * Prints the nested <code>Throwable</code> to the given stream.
       *
       * @param nested  Nested <code>Throwable</code>.
       * @param stream  Stream to print to.
       */
      public static void print(final Throwable nested,
                               final PrintStream stream)
      {
         if (stream == null)
            throw new NullArgumentException("stream");

         if (NestedThrowable.NESTED_TRACE_ENABLED && nested != null) {
            synchronized (stream) {
               if (NestedThrowable.PARENT_TRACE_ENABLED) {
                  stream.print(" + nested throwable: ");
               }
               else {
                  stream.print("[ parent trace omitted ]: ");
               }
               
               nested.printStackTrace(stream);
            }
         }
      }

      /**
       * Prints the nested <code>Throwable</code> to the given writer.
       *
       * @param nested  Nested <code>Throwable</code>.
       * @param writer  Writer to print to.
       */
      public static void print(final Throwable nested,
                               final PrintWriter writer)
      {
         if (writer == null)
            throw new NullArgumentException("writer");

         if (NestedThrowable.NESTED_TRACE_ENABLED && nested != null) {
            synchronized (writer) {
               if (NestedThrowable.PARENT_TRACE_ENABLED) {
                  writer.print(" + nested throwable: ");
               }
               else {
                  writer.print("[ parent trace omitted ]: ");
               }
               
               nested.printStackTrace(writer);
            }
         }
      }
   }
}
