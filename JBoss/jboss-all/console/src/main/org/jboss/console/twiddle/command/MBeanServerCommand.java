/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.console.twiddle.command;

import java.util.Set;

import javax.management.ObjectName;
import javax.management.MBeanServer;
import javax.management.MalformedObjectNameException;


/**
 * An abstract command to opperate on an MBeanServer.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public abstract class MBeanServerCommand
   extends AbstractCommand
{
   protected MBeanServerCommand(final String name, final String desc)
   {
      super(name, desc);
   }

   protected ObjectName createObjectName(final String name)
      throws CommandException
   {
      try {
         return new ObjectName(name);
      }
      catch (MalformedObjectNameException e) {
         throw new CommandException("Invalid object name: " + name);
      }
   }
   
   protected MBeanServer getMBeanServer()
   {
      //
      // jason: put the server in the environemnt
      //
      
      return context.getServer();
   }
   
   protected ObjectName[] queryMBeans(final String query)
      throws Exception
   {
      // query the mbean server
      MBeanServer server = getMBeanServer();
      
      Set matches = server.queryNames(new ObjectName(query), null);
      log.debug("Query matches: " + matches);

      if (matches.size() == 0) {
         throw new CommandException("No MBean matches for query: " + query);
      }

      ObjectName[] names =
         (ObjectName[])matches.toArray(new ObjectName[matches.size()]);

      return names;
   }
}
