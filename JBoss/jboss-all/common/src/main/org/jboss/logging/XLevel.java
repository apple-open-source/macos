/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.logging;

import org.apache.log4j.Level;

/** 
 * Provides custom extention levels for use with the Log4j logging framework.
 *
 * <p>
 * Adds a trace level that is below the standard Log4j <tt>DEBUG</tt> level.
 *
 * <p>
 * This is a custom level that is 100 below the {@link Level#DEBUG_INT} 
 * and represents a lower level useful for logging events that should only
 * be displayed when deep debugging is required.
 *
 * @see org.apache.log4j.Level
 *
 * @author  <a href="mailto:Scott.Stark@jboss.org">Scott Stark</a>
 * @version $Revision: 1.3.2.1 $
 */
public class XLevel 
   extends Level
{
   /** The integer representation of the level, ({@link Level#DEBUG_INT} - 100) */
   public static final int TRACE_INT = Level.DEBUG_INT - 100;

   /** The string name of the trace level. */
   public static final String TRACE_STR = "TRACE";
   
   /** The TRACE level object singleton */
   public static final XLevel TRACE = new XLevel(TRACE_INT, TRACE_STR, 7);

   /**
    * Construct a <tt>XLevel</tt>.
    */
   protected XLevel(final int level, final String strLevel, final int syslogEquiv)
   {
      super(level, strLevel, syslogEquiv);
   }
   

   /////////////////////////////////////////////////////////////////////////
   //                            Factory Methods                          //
   /////////////////////////////////////////////////////////////////////////

   /** 
    * Convert an integer passed as argument to a level. If the conversion
    * fails, then this method returns the specified default.
    * 
    * @return the Level object for name if one exists, defaultLevel otherwize.
    */
   public static Level toLevel(final String name, final Level defaultLevel)
   {
      if (name == null)
         return defaultLevel;

      String upper = name.toUpperCase();
      if (upper.equals(TRACE_STR)) {
         return TRACE;
      }

      return Level.toLevel(name, defaultLevel);
   }

   /** 
    * Convert an integer passed as argument to a level.
    * 
    * @return the Level object for name if one exists
    */
   public static Level toLevel(final String name)
   {
      return toLevel(name, TRACE);
   }
   
   /**
    * Convert an integer passed as argument to a priority. If the conversion
    * fails, then this method returns the specified default.
    * @return the Level object for i if one exists, defaultLevel otherwize.
    */
   public static Level toLevel(int i)
   {
      return toLevel(i, TRACE);
   }

   /** 
    * Convert an integer passed as argument to a level. If the conversion
    * fails, then this method returns the specified default.
    * 
    * @return the Level object for i if one exists, defaultLevel otherwize.
    */
   public static Level toLevel(final int i, final Level defaultLevel)
   {
      Level p;
      if (i == TRACE_INT)
         p = TRACE;
      else
         p = Level.toLevel(i);
      return p;
   }
}
