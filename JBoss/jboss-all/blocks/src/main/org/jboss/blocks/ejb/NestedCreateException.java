/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.blocks.ejb;

import javax.ejb.CreateException;

import org.jboss.util.NestedThrowable;

/**
 * A nested <em>EJB</em> create exception.
 *
 * @version <tt>$Revision: 1.2 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class NestedCreateException
   extends CreateException
   implements NestedThrowable
{
   /** The nested throwable */
   protected final Throwable nested;

   /**
    * Construct a <tt>NestedCreateException</tt> with the specified detail 
    * message.
    *
    * @param msg  Detail message.
    */
   public NestedCreateException(final String msg) {
      super(msg);
      this.nested = null;
   }

   /**
    * Construct a <tt>NestedCreateException</tt> with the specified detail 
    * message and nested <tt>Throwable</tt>.
    *
    * @param msg     Detail message.
    * @param nested  Nested <tt>Throwable</tt>.
    */
   public NestedCreateException(final String msg, final Throwable nested) {
      super(msg);
      this.nested = nested;
   }

   /**
    * Construct a <tt>NestedCreateException</tt> with the specified
    * nested <tt>Throwable</tt>.
    *
    * @param nested  Nested <tt>Throwable</tt>.
    */
   public NestedCreateException(final Throwable nested) {
      this(nested.getMessage(), nested);
   }

   /**
    * Construct a <tt>NestedCreateException</tt> with no detail.
    */
   public NestedCreateException() {
      super();
      this.nested = null;
   }

   /**
    * Return the nested <tt>Throwable</tt>.
    *
    * @return  Nested <tt>Throwable</tt>.
    */
   public final Throwable getNested() {
      return nested;
   }

   public final Throwable getCause() {
      return nested;
   }
   
   /**
    * Returns the composite throwable message.
    *
    * @return  The composite throwable message.
    */
   public String getMessage() {
      return NestedThrowable.Util.getMessage(super.getMessage(), nested);
   }

   /**
    * Prints the composite message and the embedded stack trace to the
    * specified print stream.
    *
    * @param stream  Stream to print to.
    */
   public void printStackTrace(final java.io.PrintStream stream) {
      if (nested == null || NestedThrowable.PARENT_TRACE_ENABLED) {
         super.printStackTrace(stream);
      }
      NestedThrowable.Util.print(nested, stream);
   }

   /**
    * Prints the composite message and the embedded stack trace to the
    * specified print writer.
    *
    * @param writer  Writer to print to.
    */
   public void printStackTrace(final java.io.PrintWriter writer) {
      if (nested == null || NestedThrowable.PARENT_TRACE_ENABLED) {
         super.printStackTrace(writer);
      }
      NestedThrowable.Util.print(nested, writer);
   }

   /**
    * Prints the composite message and the embedded stack trace to 
    * <tt>System.err</tt>.
    */
   public void printStackTrace() {
      printStackTrace(System.err);
   }
}
