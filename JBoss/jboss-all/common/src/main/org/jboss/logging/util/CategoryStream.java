/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.logging.util;

import java.io.IOException;
import java.io.PrintStream;

import org.apache.log4j.Category;
import org.apache.log4j.Priority;


/**
 * A subclass of PrintStream that redirects its output to a log4j Category.
 * 
 * <p>This class is used to map PrintStream/PrintWriter oriented logging onto
 *    the log4j Categories. Examples include capturing System.out/System.err
 *
 * @deprecated Use {@link LoggerStream} instead.
 *
 * @version <tt>$Revision: 1.3.2.1 $</tt>
 * @author <a href="mailto:Scott.Stark@jboss.org">Scott Stark</a>.
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class CategoryStream
    extends PrintStream
{
   /**
    * Default flag to enable/disable tracing println calls.
    * from the system property <tt>org.jboss.logging.util.CategoryStream.trace</tt>
    * or if not set defaults to <tt>false</tt>.
    */
   public static final boolean TRACE =
      getBoolean(CategoryStream.class.getName() + ".trace", false);

   /** Helper to get boolean value from system property or use default if not set. */
   private static boolean getBoolean(String name, boolean defaultValue)
   {
      String value = System.getProperty(name, null);
      if (value == null)
         return defaultValue;
      return new Boolean(value).booleanValue();
   }
   
   private Category category;
   private Priority priority;
   private boolean issuedWarning;
   
   /**
    * Redirect logging to the indicated category using Priority.INFO
    */
   public CategoryStream(final Category category)
   {
      this(category, Priority.INFO, System.out);
   } 
    
   /**
    * Redirect logging to the indicated category using the given
    * priority. The ps is simply passed to super but is not used.
    */
   public CategoryStream(final Category category,
                         final Priority priority,
                         final PrintStream ps)
   {
      super(ps);
      this.category = category;
      this.priority = priority;
   }
    
   public void println(String msg)
   {
      if( msg == null )
         msg = "null";
      byte[] bytes = msg.getBytes();
      write(bytes, 0, bytes.length);
   }
    
   public void println(Object msg)
   {
      if( msg == null )
         msg = "null";
      byte[] bytes = msg.toString().getBytes();
      write(bytes, 0, bytes.length);
   }
    
   public void write(byte b)
   {
      byte[] bytes = {b};
      write(bytes, 0, 1);
   }

   private ThreadLocal recursiveCheck = new ThreadLocal();
   public void write(byte[] b, int off, int len)
   {
      Boolean recursed = (Boolean)recursiveCheck.get();
      if (recursed != null && recursed.equals(Boolean.TRUE))
      {
         /* There is a configuration error that is causing looping. Most
            likely there are two console appenders so just return to prevent
            spinning.
         */
         if( issuedWarning == false )
         {
            String msg = "ERROR: invalid console appender config detected, console stream is looping";
            try
            {
               out.write(msg.getBytes());
            }
            catch(IOException ignore)
            {
            }
            issuedWarning = true;
         }
         return;
      }

      // Remove the end of line chars
      while( len > 0 && (b[len-1] == '\n' || b[len-1] == '\r') && len > off )
         len --;

      // HACK, something is logging exceptions line by line (including
      // blanks), but I can't seem to find it, so for now just ignore
      // empty lines... they aren't very useful.
      if (len != 0)
      {
         String msg = new String(b, off, len);
         recursiveCheck.set(Boolean.TRUE);
         if (TRACE)
         {
            category.log(priority, msg, new Throwable());
         }
         else
         {
            category.log(priority, msg);
         }
         recursiveCheck.set(Boolean.FALSE);
      }
   }
}
