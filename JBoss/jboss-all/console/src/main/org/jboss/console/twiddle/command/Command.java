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
 * ???
 *
 * @version <tt>$Revision: 1.2.2.1 $</tt>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public interface Command
   extends Cloneable
{
   String getName();

   String getDescription();

   void displayHelp();
   
   void setCommandContext(CommandContext context);

   void unsetCommandContext();

   void execute(String[] args) throws Exception;
   
   Object clone();
}
