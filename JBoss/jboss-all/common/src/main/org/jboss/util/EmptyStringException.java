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
 * Thrown to indicate that a string was empty (aka. <code>""</code>)
 * where it must <b>not</b> be.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class EmptyStringException
   extends IllegalArgumentException
{
   /**
    * Construct a <tt>EmptyStringException</tt>.
    *
    * @param msg  Exception message.
    */
   public EmptyStringException(final String msg) {
      super(msg);
   }

   /**
    * Construct a <tt>EmptyStringException</tt>.
    */
   public EmptyStringException() {
      super();
   }
}
