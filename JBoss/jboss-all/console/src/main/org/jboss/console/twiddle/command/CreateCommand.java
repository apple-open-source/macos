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

import javax.management.ObjectName;
import javax.management.ObjectInstance;
import javax.management.MBeanServer;
import javax.management.MalformedObjectNameException;

import gnu.getopt.Getopt;
import gnu.getopt.LongOpt;

import org.jboss.util.Strings;

/**
 * Create an MBean.
 *
 * @version <tt>$Revision: 1.2 $</tt>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class CreateCommand
   extends MBeanServerCommand
{
   private String className;

   private ObjectName objectName;

   private ObjectName loaderName;

   public CreateCommand()
   {
      super("create", "Create an MBean");
   }
   
   public void displayHelp()
   {
      PrintWriter out = context.getWriter();
      
      out.println(desc);
      out.println();
      out.println("usage: " + name + " [options] <class> [<name>]");
      out.println();
      out.println("options:");
      out.println("    -l, --loader=<name>    Treat object name as a query");
      out.println("    --                     Stop processing options");

      out.flush();
   }
   
   private boolean processArguments(final String[] args)
      throws CommandException
   {
      log.debug("processing arguments: " + Strings.join(args, ","));

      if (args.length == 0) {
         throw new CommandException("Command requires arguments");
      }
      
      String sopts = "-:l:";
      LongOpt[] lopts =
      {
         new LongOpt("loader", LongOpt.OPTIONAL_ARGUMENT, null, 'l'),
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
                  case 0:
                     className = arg;
                     log.debug("class name: " + className);
                     break;

                  case 1:
                  {
                     try {
                        objectName = new ObjectName(arg);
                        log.debug("mbean name: " + objectName);
                     }
                     catch (MalformedObjectNameException e) {
                        throw new CommandException("Invalid object name: " + arg);
                     }
                     break;
                  }
                  
                  default:
                     throw new CommandException("Unused argument: " + arg);
               }
               break;
            }

            // Set the loader name
            case 'l':
            {
               String arg = getopt.getOptarg();
               
               try {
                  loaderName = new ObjectName(arg);
                  log.debug("loader name: " + loaderName);
               }
               catch (MalformedObjectNameException e) {
                  throw new CommandException("Invalid loader object name: " + arg);
               }
               break;
            }
         }
      }

      return true;
   }
   
   public void execute(String[] args) throws Exception
   {
      processArguments(args);
      
      if (className == null)
         throw new CommandException("Missing class name");

      MBeanServer server = getMBeanServer();

      ObjectInstance obj;

      if (loaderName == null) {
         obj = server.createMBean(className, objectName);
      }
      else {
         obj = server.createMBean(className, objectName, loaderName);
      }

      if (!context.isQuiet()) {
         PrintWriter out = context.getWriter();
         out.println(obj.getObjectName());
         out.flush();
      }
   }
}
