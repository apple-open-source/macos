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
 * Thrown to indicate that a method has not been implemented yet.
 * 
 * <p>This exception is used to help stub out implementations.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class NotImplementedException
   extends RuntimeException
{
   /**
    * Construct a <tt>NotImplementedException</tt> with a detail message.
    *
    * @param msg  Detail message.
    */
   public NotImplementedException(final String msg) {
      super(msg);
   }

   /**
    * Construct a <tt>NotImplementedException</tt> with no detail.
    */
   public NotImplementedException() {
      super();
   }
}
