/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.resource;

import java.io.PrintWriter;
import java.io.PrintStream;
import java.lang.reflect.UndeclaredThrowableException;

import org.jboss.util.NestedThrowable;

/**
 * Thrown to indicate a problem with a resource related operation.
 *
 * <p>
 * Properly displays linked exception (ie. nested exception)
 * when printing the stack trace.
 *
 * @version <tt>$Revision: 1.1.4.3 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class JBossResourceException
   extends javax.resource.ResourceException
   implements NestedThrowable
{

   /**
    * Construct a <tt>JBossResourceException</tt> with the specified detail
    * message.
    *
    * @param msg  Detail message.
    */
   public JBossResourceException(final String msg)
   {
      super(msg);
   }

   /**
    * Construct a <tt>JBossResourceException</tt> with the specified detail
    * message and error code.
    *
    * @param msg   Detail message.
    * @param code  Error code.
    */
   public JBossResourceException(final String msg, final String code)
   {
      super(msg, code);
   }

   /**
    * Construct a <tt>JBossResourceException</tt> with the specified detail
    * message, error code and linked <tt>Exception</tt>.
    *
    * @param msg     Detail message.
    * @param code    Error code.
    * @param linked  Linked <tt>Exception</tt>.
    */
   public JBossResourceException(final String msg, final String code, final Throwable linked)
   {
      super(msg, code);
      setLinkedException(process(linked));
   }

   /**
    * Construct a <tt>JBossResourceException</tt> with the specified detail
    * message and linked <tt>Exception</tt>.
    *
    * @param msg     Detail message.
    * @param linked  Linked <tt>Exception</tt>.
    */
   public JBossResourceException(final String msg, final Throwable linked)
   {
      super(msg);
      setLinkedException(process(linked));
   }

   /**
    * Construct a <tt>JBossResourceException</tt> with the specified
    * linked <tt>Exception</tt>.
    *
    * @param linked  Linked <tt>Exception</tt>.
    */
   public JBossResourceException(final Throwable linked)
   {
      this(linked.getMessage(), linked);
   }

   /**
    * Return the nested <tt>Throwable</tt>.
    *
    * @return  Nested <tt>Throwable</tt>.
    */
   public Throwable getNested()
   {
      return getLinkedException();
   }

   /**
    * Return the nested <tt>Throwable</tt>.
    *
    * <p>For JDK 1.4 compatibility.
    *
    * @return  Nested <tt>Throwable</tt>.
    */
   public Throwable getCause()
   {
      return getLinkedException();
   }

   /**
    * Returns the composite throwable message.
    *
    * @return  The composite throwable message.
    */
   public String getMessage()
   {
      return NestedThrowable.Util.getMessage(super.getMessage(), getLinkedException());
   }

   /**
    * Prints the composite message and the embedded stack trace to the
    * specified print stream.
    *
    * @param stream  Stream to print to.
    */
   public void printStackTrace(final PrintStream stream)
   {
      Exception linked = getLinkedException();
      if (linked == null || NestedThrowable.PARENT_TRACE_ENABLED)
      {
         super.printStackTrace(stream);
      }
      NestedThrowable.Util.print(linked, stream);
   }

   /**
    * Prints the composite message and the embedded stack trace to the
    * specified print writer.
    *
    * @param writer  Writer to print to.
    */
   public void printStackTrace(final PrintWriter writer)
   {
      Exception linked = getLinkedException();
      if (linked == null || NestedThrowable.PARENT_TRACE_ENABLED)
      {
         super.printStackTrace(writer);
      }
      NestedThrowable.Util.print(linked, writer);
   }

   /**
    * Prints the composite message and the embedded stack trace to
    * <tt>System.err</tt>.
    */
   public void printStackTrace()
   {
      printStackTrace(System.err);
   }

   private Exception process(Throwable t)
   {
      if (t instanceof Exception)
      {
         return (Exception)t;
      } // end of if ()
      return new UndeclaredThrowableException(t);
   }
}
