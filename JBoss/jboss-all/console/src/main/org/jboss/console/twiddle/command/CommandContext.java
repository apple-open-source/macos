/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.console.twiddle.command;

import java.io.PrintWriter;

import javax.management.MBeanServer;

/**
 * ???
 *
 * @version <tt>$Revision: 1.2 $</tt>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public interface CommandContext
{
   boolean isQuiet();

   //
   // jason: add really quiet or terse, or change quiet to silent or something
   //
   
   PrintWriter getWriter();

   PrintWriter getErrorWriter();

   //
   // jason: add reader
   //

   //
   // jason: abstract this into a map based command environment
   //
   
   MBeanServer getServer();
}
