/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.console.twiddle.command;

import org.jboss.util.NullArgumentException;

import org.jboss.logging.Logger;

/**
 * An abstract command.
 *
 * @version <tt>$Revision: 1.2.2.1 $</tt>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author Scott.Stark@jboss.org
 */
public abstract class AbstractCommand
   implements Command
{
   protected Logger log = Logger.getLogger(getClass());

   protected final String desc;
   
   protected final String name;

   protected CommandContext context;

   protected AbstractCommand(final String name, final String desc)
   {
      this.name = name;
      this.desc = desc;
   }
   
   public String getName()
   {
      return name;
   }

   public String getDescription()
   {
      return desc;
   }

   public void setCommandContext(final CommandContext context)
   {
      if (context == null)
         throw new NullArgumentException("context");
      
      this.context = context;
   }

   public void unsetCommandContext()
   {
      this.context = null;
   }
   
   /**
    * Return a cloned copy of this command.
    *
    * @return   Cloned command.
    */
   public Object clone()
   {
      try
      {
         return super.clone();
      }
      catch (CloneNotSupportedException e)
      {
         throw new InternalError();
      }
   }
}
