/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util;

import java.io.PrintStream;
import java.io.ByteArrayOutputStream;

/**
 * A collection of Throwable utilities.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public final class Throwables
{
   /**
    * Return a string that consists of the stack trace of the given 
    * <code>Throwable</code>.
    *
    * @param t    <code>Throwable</code> to get stack trace from.
    * @return     <code>Throwable</code> stack trace as a string.
    */
   public static String toString(final Throwable t) {
      ByteArrayOutputStream output = new ByteArrayOutputStream();
      PrintStream stream = new PrintStream(output);
      t.printStackTrace(stream);

      return output.toString();
   }
}
