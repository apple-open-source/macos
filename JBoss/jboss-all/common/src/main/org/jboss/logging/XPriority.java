/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.logging;

import org.apache.log4j.Priority;

/** 
 * Provides custom extention priorites for use with the
 * Log4j logging framework.
 *
 * Adds a trace priority that is below the standard log4j DEBUG priority.
 * This is a custom priority that is 100 below the Priority.DEBUG_INT and
 * represents a lower priority useful for logging events that should only
 * be displayed when deep debugging is required.
 *
 * @deprecated Use {@link XLevel} instead.
 * 
 * @see org.apache.log4j.Category
 * @see org.apache.log4j.Priority
 *
 * @author  <a href="mailto:Scott.Stark@jboss.org">Scott Stark</a>
 * @version $Revision: 1.2 $
 */
public class XPriority 
   extends Priority
{
   /** The integer representation of the priority, (Priority.DEBUG_INT - 100) */
   public static final int TRACE_INT = Priority.DEBUG_INT - 100;

   /** The string name of the trace priority. */
   public static String TRACE_STR = "TRACE";
   
   /** The TRACE priority object singleton */
   public static final XPriority TRACE = new XPriority(TRACE_INT, TRACE_STR, 7);

   /**
    * Construct a <tt>XPriority</tt>.
    */
   protected XPriority(int level, String strLevel, int syslogEquiv)
   {
      super(level, strLevel, syslogEquiv);
   }
   

   /////////////////////////////////////////////////////////////////////////
   //                            Factory Methods                          //
   /////////////////////////////////////////////////////////////////////////

   /** 
    * Convert an integer passed as argument to a priority. If the conversion
    * fails, then this method returns the specified default.
    * @return the Priority object for name if one exists, defaultPriority otherwize.
    */
   public static Priority toPriority(String name, Priority defaultPriority)
   {
      if (name == null)
         return defaultPriority;

      String upper = name.toUpperCase();
      if (upper.equals(TRACE_STR)) {
         return TRACE;
      }

      return Priority.toPriority(name, defaultPriority);
   }

   /** 
    * Convert an integer passed as argument to a priority.
    * 
    * @return the Priority object for name if one exists
    */
   public static Priority toPriority(String name)
   {
      return toPriority(name, TRACE);
   }
   
   /** 
    * Convert an integer passed as argument to a priority. If the conversion
    * fails, then this method returns the specified default.
    * @return the Priority object for i if one exists, defaultPriority otherwize.
    */
   public static Priority toPriority(int i, Priority defaultPriority)
   {
      Priority p;
      if (i == TRACE_INT)
         p = TRACE;
      else
         p = Priority.toPriority(i);
      return p;
   }
}
