/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.mq;

import java.io.PrintWriter;
import java.io.PrintStream;

import javax.jms.TransactionRolledBackException;

import org.jboss.util.NestedThrowable;
import org.jboss.util.NestedException;

/**
 * A TransactionRolledBackException with
 * a nested <tt>Throwable</tt> detail object.
 *
 * @version <tt>$Revision: 1.1.2.1 $</tt>
 * @author  <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 */
public class SpyTransactionRolledBackException
   extends TransactionRolledBackException
   implements NestedThrowable
{
   /** The nested throwable */
   protected Throwable nested;

   /**
    * Construct a <tt>SpyTransactionRolledBackException</tt> with the specified detail 
    * message.
    *
    * @param msg  Detail message.
    */
   public SpyTransactionRolledBackException(final String msg)
   {
      super(msg);
      this.nested = null;
   }
   
   /**
    * Construct a <tt>SpyTransactionRolledBackException</tt> with the specified detail 
    * message and nested <tt>Throwable</tt>.
    *
    * @param msg     Detail message.
    * @param nested  Nested <tt>Throwable</tt>.
    */
   public SpyTransactionRolledBackException(final String msg, final Throwable nested)
   {
      super(msg);
      this.nested = nested;
      NestedThrowable.Util.checkNested(this, nested);
   }

   /**
    * Construct a <tt>SpyTransactionRolledBackException</tt> with the specified
    * nested <tt>Throwable</tt>.
    *
    * @param nested  Nested <tt>Throwable</tt>.
    */
   public SpyTransactionRolledBackException(final Throwable nested)
   {
      this(nested.getMessage(), nested);
   }

   /**
    * Return the nested <tt>Throwable</tt>.
    *
    * @return  Nested <tt>Throwable</tt>.
    */
   public Throwable getNested()
   {
      return nested;
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
      return nested;
   }

   public void setLinkedException(final Exception e)
   {
      this.nested = e;
   }

   public Exception getLinkedException()
   {
      //
      // jason: this is bad, but whatever... the jms folks should have had more insight
      //
      if (nested == null)
         return this;      
      if (nested instanceof Exception) {
         return (Exception)nested;
      }
      return new NestedException(nested);
   }
   
   /**
    * Returns the composite throwable message.
    *
    * @return  The composite throwable message.
    */
   public String getMessage()
   {
      return NestedThrowable.Util.getMessage(super.getMessage(), nested);
   }

   /**
    * Prints the composite message and the embedded stack trace to the
    * specified print stream.
    *
    * @param stream  Stream to print to.
    */
   public void printStackTrace(final PrintStream stream)
   {
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
   public void printStackTrace(final PrintWriter writer)
   {
      if (nested == null || NestedThrowable.PARENT_TRACE_ENABLED) {
         super.printStackTrace(writer);
      }
      NestedThrowable.Util.print(nested, writer);
   }

   /**
    * Prints the composite message and the embedded stack trace to 
    * <tt>System.err</tt>.
    */
   public void printStackTrace()
   {
      printStackTrace(System.err);
   }
}
