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
 * This exception is thrown to indicate that a problem has occured while
 * trying to coerce an object.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class CoercionException
   extends NestedRuntimeException
{
   /**
    * Construct a <tt>CoercionException</tt> with the specified detail 
    * message.
    *
    * @param msg  Detail message.
    */
   public CoercionException(String msg) {
      super(msg);
   }

   /**
    * Construct a <tt>CoercionException</tt> with the specified detail 
    * message and nested <tt>Throwable</tt>.
    *
    * @param msg     Detail message.
    * @param nested  Nested <tt>Throwable</tt>.
    */
   public CoercionException(String msg, Throwable nested) {
      super(msg, nested);
   }

   /**
    * Construct a <tt>CoercionException</tt> with the specified
    * nested <tt>Throwable</tt>.
    *
    * @param nested  Nested <tt>Throwable</tt>.
    */
   public CoercionException(Throwable nested) {
      super(nested);
   }

   /**
    * Construct a <tt>CoercionException</tt> with no detail.
    */
   public CoercionException() {
      super();
   }
}
