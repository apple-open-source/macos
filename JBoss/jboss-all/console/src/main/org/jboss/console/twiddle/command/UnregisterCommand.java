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

import java.util.List;
import java.util.ArrayList;
import java.util.Iterator;

import javax.management.ObjectName;
import javax.management.MBeanServer;

import gnu.getopt.Getopt;
import gnu.getopt.LongOpt;

import org.jboss.util.Strings;

/**
 * Unregister one or more MBeans.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class UnregisterCommand
   extends MBeanServerCommand
{
   private List names = new ArrayList();
   
   public UnregisterCommand()
   {
      super("unregister", "Unregister one or more MBeans");
   }
   
   public void displayHelp()
   {
      PrintWriter out = context.getWriter();
      
      out.println(desc);
      out.println();
      out.println("usage: " + name + " [options] (<name>)+");
      out.println();
      out.println("options:");
      out.println("    --    Stop processing options");

      out.flush();
   }
   
   private void processArguments(final String[] args)
      throws CommandException
   {
      log.debug("processing arguments: " + Strings.join(args, ","));

      if (args.length == 0) {
         throw new CommandException("Command requires arguments");
      }
      
      String sopts = "-:";
      LongOpt[] lopts =
      {
         // new LongOpt("count", LongOpt.NO_ARGUMENT, null, 'c'),
      };

      Getopt getopt = new Getopt(null, args, sopts, lopts);
      getopt.setOpterr(false);
      
      int code;
      int argidx = 0;
      
      while ((code = getopt.getopt()) != -1)
      {
         switch (code)
         {
            case ':':
               throw new CommandException
                  ("Option requires an argument: "+ args[getopt.getOptind() - 1]);

            case '?':
               throw new CommandException
                  ("Invalid (or ambiguous) option: " + args[getopt.getOptind() - 1]);

            // non-option arguments
            case 1:
            {
               String arg = getopt.getOptarg();
               
               switch (argidx++) {
                  default:
                     names.add(createObjectName(arg));
                     break;
               }
               break;
            }
         }
      }
   }

   public void execute(String[] args) throws Exception
   {
      processArguments(args);

      log.debug("object names: " + names);

      if (names.size() == 0)
         throw new CommandException("At least one object name is required");

      MBeanServer server = getMBeanServer();

      Iterator iter = names.iterator();
      while (iter.hasNext()) {
         ObjectName name = (ObjectName)iter.next();
         server.unregisterMBean(name);
      }
   }
}
