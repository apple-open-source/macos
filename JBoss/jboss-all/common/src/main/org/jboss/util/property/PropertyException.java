/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.property;

import org.jboss.util.NestedRuntimeException;

/**
 * This exception is thrown to indicate a non-fatal problem with the 
 * property system.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class PropertyException
   extends NestedRuntimeException
{
   /**
    * Construct a <tt>PropertyException</tt> with the specified detail 
    * message.
    *
    * @param msg  Detail message.
    */
   public PropertyException(String msg) {
      super(msg);
   }

   /**
    * Construct a <tt>PropertyException</tt> with the specified detail 
    * message and nested <tt>Throwable</tt>.
    *
    * @param msg     Detail message.
    * @param nested  Nested <tt>Throwable</tt>.
    */
   public PropertyException(String msg, Throwable nested) {
      super(msg, nested);
   }

   /**
    * Construct a <tt>PropertyException</tt> with the specified
    * nested <tt>Throwable</tt>.
    *
    * @param nested  Nested <tt>Throwable</tt>.
    */
   public PropertyException(Throwable nested) {
      super(nested);
   }

   /**
    * Construct a <tt>PropertyException</tt> with no detail.
    */
   public PropertyException() {
      super();
   }
}
