/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.console.twiddle.command;

import org.jboss.util.NestedException;

/**
 * A command exception.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class CommandException
   extends NestedException
{
   //
   // jason: need to expose the source command of the exception
   //
   
   /**
    * Construct a <tt>CommandException</tt> with the specified detail 
    * message.
    *
    * @param msg  Detail message.
    */
   public CommandException(String msg) {
      super(msg);
   }

   /**
    * Construct a <tt>CommandException</tt> with the specified detail 
    * message and nested <tt>Throwable</tt>.
    *
    * @param msg     Detail message.
    * @param nested  Nested <tt>Throwable</tt>.
    */
   public CommandException(String msg, Throwable nested) {
      super(msg, nested);
   }

   /**
    * Construct a <tt>CommandException</tt> with the specified
    * nested <tt>Throwable</tt>.
    *
    * @param nested  Nested <tt>Throwable</tt>.
    */
   public CommandException(Throwable nested) {
      super(nested);
   }

   /**
    * Construct a <tt>CommandException</tt> with no detail.
    */
   public CommandException() {
      super();
   }
}
