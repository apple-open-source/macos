/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.console.twiddle.command;

/**
 * Throw to indicate that a requested command does not exist.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class NoSuchCommandException
   extends CommandException
{
   /**
    * Construct a <tt>NoSuchCommandException</tt> with the specified
    * invalid command name.
    *
    * @param name  Command name.
    */
   public NoSuchCommandException(final String name) {
      super("No such command named '" + name + "'");
   }
}
