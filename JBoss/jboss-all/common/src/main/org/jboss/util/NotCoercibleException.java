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
 * This exception is thrown to indicate that an object was not coercible.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class NotCoercibleException
   extends CoercionException
{
   /**
    * Construct a <tt>NotCoercibleException</tt> with the specified detail 
    * message.
    *
    * @param msg  Detail message.
    */
   public NotCoercibleException(String msg) {
      super(msg);
   }

   /**
    * Construct a <tt>NotCoercibleException</tt> with the specified detail 
    * message and nested <tt>Throwable</tt>.
    *
    * @param msg     Detail message.
    * @param nested  Nested <tt>Throwable</tt>.
    */
   public NotCoercibleException(String msg, Throwable nested) {
      super(msg, nested);
   }

   /**
    * Construct a <tt>NotCoercibleException</tt> with the specified
    * nested <tt>Throwable</tt>.
    *
    * @param nested  Nested <tt>Throwable</tt>.
    */
   public NotCoercibleException(Throwable nested) {
      super(nested);
   }

   /**
    * Construct a <tt>NotCoercibleException</tt> with no detail.
    */
   public NotCoercibleException() {
      super();
   }

   /**
    * Construct a <tt>NotCoercibleException</tt> with an object detail.
    *
    * @param obj     Object detail.
    */
   public NotCoercibleException(Object obj) {
      super(String.valueOf(obj));
   }
}
