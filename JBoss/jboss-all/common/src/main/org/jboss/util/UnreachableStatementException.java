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
 * Thrown to indicate that section of code that should never have been
 * reachable, has just been reached.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class UnreachableStatementException 
   extends RuntimeException
{
   /**
    * Construct a <tt>UnreachableStatementException</tt> with a detail message.
    *
    * @param msg  Detail message.
    */
   public UnreachableStatementException(final String msg) {
      super(msg);
   }

   /**
    * Construct a <tt>UnreachableStatementException</tt> with no detail.
    */
   public UnreachableStatementException() {
      super();
   }
}
