/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util;

/**
 * Thrown to indicate that a Throwable was caught but was not expected.
 * This is typical when catching Throwables to handle and rethrow Exceptions
 * and Errors.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class UnexpectedThrowable
   extends NestedError
{
   /**
    * Construct a <tt>UnexpectedThrowable</tt> with the specified 
    * detail message.
    *
    * @param msg  Detail message.
    */
   public UnexpectedThrowable(final String msg) {
      super(msg);
   }

   /**
    * Construct a <tt>UnexpectedThrowable</tt> with the specified
    * detail message and nested <tt>Throwable</tt>.
    *
    * @param msg     Detail message.
    * @param nested  Nested <tt>Throwable</tt>.
    */
   public UnexpectedThrowable(final String msg, final Throwable nested) {
      super(msg, nested);
   }

   /**
    * Construct a <tt>UnexpectedThrowable</tt> with the specified
    * nested <tt>Throwable</tt>.
    *
    * @param nested  Nested <tt>Throwable</tt>.
    */
   public UnexpectedThrowable(final Throwable nested) {
      super(nested);
   }

   /**
    * Construct a <tt>UnexpectedThrowable</tt> with no detail.
    */
   public UnexpectedThrowable() {
      super();
   }
}
