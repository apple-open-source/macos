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
 * Thrown to indicate to an encapsulated try/catch block that something has
 * happened and it was not harmfull.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class BenignError
   extends NestedError
{
   /**
    * Construct a <tt>BenignError</tt> with the specified 
    * detail message.
    *
    * @param msg  Detail message.
    */
   public BenignError(final String msg) {
      super(msg);
   }

   /**
    * Construct a <tt>BenignError</tt> with the specified
    * detail message and nested <tt>Throwable</tt>.
    *
    * @param msg     Detail message.
    * @param nested  Nested <tt>Throwable</tt>.
    */
   public BenignError(final String msg, final Throwable nested) {
      super(msg, nested);
   }

   /**
    * Construct a <tt>BenignError</tt> with the specified
    * nested <tt>Throwable</tt>.
    *
    * @param nested  Nested <tt>Throwable</tt>.
    */
   public BenignError(final Throwable nested) {
      super(nested);
   }

   /**
    * Construct a <tt>BenignError</tt> with no detail.
    */
   public BenignError() {
      super();
   }
}
