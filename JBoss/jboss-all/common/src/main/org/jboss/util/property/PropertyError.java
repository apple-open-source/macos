/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.property;

import org.jboss.util.NestedError;

/**
 * Thrown to indicate a fatal problem with the property system.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class PropertyError
   extends NestedError
{
   /**
    * Construct a <tt>PropertyError</tt> with the specified detail 
    * message.
    *
    * @param msg  Detail message.
    */
   public PropertyError(String msg) {
      super(msg);
   }

   /**
    * Construct a <tt>PropertyError</tt> with the specified detail 
    * message and nested <tt>Throwable</tt>.
    *
    * @param msg     Detail message.
    * @param nested  Nested <tt>Throwable</tt>.
    */
   public PropertyError(String msg, Throwable nested) {
      super(msg, nested);
   }

   /**
    * Construct a <tt>PropertyError</tt> with the specified
    * nested <tt>Throwable</tt>.
    *
    * @param nested  Nested <tt>Throwable</tt>.
    */
   public PropertyError(Throwable nested) {
      super(nested);
   }

   /**
    * Construct a <tt>PropertyError</tt> with no detail.
    */
   public PropertyError() {
      super();
   }
}
