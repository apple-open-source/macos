/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.console.twiddle;

import java.io.PrintWriter;
import java.io.InputStream;
import java.io.File;
import java.net.URL;
import java.net.MalformedURLException;
import java.util.Properties;
import java.util.Map;
import java.util.HashMap;
import java.util.List;
import java.util.ArrayList;
import java.util.Iterator;

import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;

import javax.management.MBeanServer;

import gnu.getopt.Getopt;
import gnu.getopt.LongOpt;

import org.jboss.logging.Logger;

import org.jboss.jmx.adaptor.rmi.RMIAdaptor;
import org.jboss.jmx.connector.rmi.RMIConnectorImpl;
import org.jboss.jmx.connector.RemoteMBeanServer;

import org.jboss.util.Strings;

import org.jboss.console.twiddle.command.Command;
import org.jboss.console.twiddle.command.CommandContext;
import org.jboss.console.twiddle.command.CommandException;
import org.jboss.console.twiddle.command.NoSuchCommandException;

/**
 * A command to invoke an operation on an MBean (or MBeans).
 *
 *
 * todo Add set command
 *
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.7.2.5 $
 */
public class Twiddle
{
   public static final String PROGRAM_NAME = System.getProperty("program.name", "twiddle");
   public static final String CMD_PROPERTIES = "/org/jboss/console/twiddle/commands.properties";
   public static final String DEFAULT_JNDI_NAME = "jmx/invoker/RMIAdaptor";
   private static final Logger log = Logger.getLogger(Twiddle.class);
   // Command Line Support
   private static Twiddle twiddle = new Twiddle(new PrintWriter(System.out, true),
      new PrintWriter(System.err, true));
   private static String commandName;
   private static String[] commandArgs;
   private static boolean commandHelp;
   private static URL cmdProps;

   private List commandProtoList = new ArrayList();
   private Map commandProtoMap = new HashMap();
   private PrintWriter out;
   private PrintWriter err;
   private String serverURL;
   private String adapterName;
   private boolean quiet;
   private RemoteMBeanServer server;

   public Twiddle(final PrintWriter out, final PrintWriter err)
   {
      this.out = out;
      this.err = err;
   }

   public void setServerURL(final String url)
   {
      this.serverURL = url;
   }

   public void setAdapterName(final String name)
   {
      this.adapterName = name;
   }

   public void setQuiet(final boolean flag)
   {
      this.quiet = flag;
   }

   public void addCommandPrototype(final Command proto)
   {
      String name = proto.getName();

      log.debug("Adding command '" + name + "'; proto: " + proto);

      commandProtoList.add(proto);
      commandProtoMap.put(name, proto);
   }

   private CommandContext createCommandContext()
   {
      return new CommandContext()
      {
         public boolean isQuiet()
         {
            return quiet;
         }

         public PrintWriter getWriter()
         {
            return out;
         }

         public PrintWriter getErrorWriter()
         {
            return err;
         }

         public MBeanServer getServer()
         {
            try
            {
               connect();
            }
            catch (Exception e)
            {
               throw new org.jboss.util.NestedRuntimeException(e);
            }

            return server;
         }
      };
   }

   public Command createCommand(final String name)
      throws NoSuchCommandException, Exception
   {
      //
      // jason: need to change this to accept unique substrings on command names
      //

      Command proto = (Command) commandProtoMap.get(name);
      if (proto == null)
      {
         throw new NoSuchCommandException(name);
      }

      Command command = (Command) proto.clone();
      command.setCommandContext(createCommandContext());

      return command;
   }

   private int getMaxCommandNameLength()
   {
      int max = 0;

      Iterator iter = commandProtoList.iterator();
      while (iter.hasNext())
      {
         Command command = (Command) iter.next();
         String name = command.getName();
         if (name.length() > max)
         {
            max = name.length();
         }
      }

      return max;
   }

   public void displayCommandList()
   {
      if( commandProtoList.size() == 0 )
      {
         try
         {
            loadCommands();
         }
         catch(Exception e)
         {
            System.err.println("Failed to load commnads from: "+cmdProps);
            e.printStackTrace();
         }
      }
      Iterator iter = commandProtoList.iterator();

      out.println(PROGRAM_NAME + " commands: ");

      int maxNameLength = getMaxCommandNameLength();
      log.debug("max command name length: " + maxNameLength);

      while (iter.hasNext())
      {
         Command proto = (Command) iter.next();
         String name = proto.getName();
         String desc = proto.getDescription();

         out.print("    ");
         out.print(name);

         // an even pad, so things line up correctly
         out.print(Strings.pad(" ", maxNameLength - name.length()));
         out.print("    ");

         out.println(desc);
      }

      out.flush();
   }

   private RemoteMBeanServer createRemoteMBeanServer()
      throws NamingException
   {
      InitialContext ctx;

      if (serverURL == null)
      {
         ctx = new InitialContext();
      }
      else
      {
         Properties props = new Properties(System.getProperties());
         props.put(Context.PROVIDER_URL, serverURL);
         ctx = new InitialContext(props);
      }

      // if adapter is null, the use the default
      if (adapterName == null)
      {
         adapterName = DEFAULT_JNDI_NAME;
      }

      Object obj = ctx.lookup(adapterName);
      ctx.close();

      if (!(obj instanceof RMIAdaptor))
      {
         throw new ClassCastException
            ("Object not of type: RMIAdaptorImpl, but: " +
            (obj == null ? "not found" : obj.getClass().getName()));
      }

      return new RMIConnectorImpl((RMIAdaptor) obj);
   }

   private void connect()
      throws NamingException
   {
      if (server == null)
      {
         server = createRemoteMBeanServer();
      }
   }

   public static void main(final String[] args)
   {
      Command command = null;

      try
      {
         // Prosess global options
         processArguments(args);
         loadCommands();

         // Now execute the command
         if (commandName == null)
         {
            // Display program help
            displayHelp();
         }
         else
         {
            command = twiddle.createCommand(commandName);

            if (commandHelp)
            {
               command.displayHelp();
            }
            else
            {
               // Execute the command
               command.execute(commandArgs);
            }
         }

         System.exit(0);
      }
      catch (CommandException e)
      {
         System.err.println(PROGRAM_NAME + ": " + e.getMessage());

         if (e instanceof NoSuchCommandException)
         {
            System.err.println();
            twiddle.displayCommandList();
         }
         else
         {
            System.err.println();
            if (command != null)
            {
               command.displayHelp();
            }
         }
      }
      catch (Exception e)
      {
         System.err.println(PROGRAM_NAME + ": " + e);
      }
   }

   private static void loadCommands() throws Exception
   {
      // load command protos from property definitions
      if( cmdProps == null )
         cmdProps = Twiddle.class.getResource(CMD_PROPERTIES);
      if (cmdProps == null)
         throw new IllegalStateException("Failed to find: " + CMD_PROPERTIES);
      InputStream input = cmdProps.openStream();
      log.debug("command proto type properties: " + cmdProps);
      Properties props = new Properties();
      props.load(input);
      input.close();

      Iterator iter = props.keySet().iterator();
      while (iter.hasNext())
      {
         String name = (String) iter.next();
         String typeName = props.getProperty(name);
         Class type = Class.forName(typeName);

         twiddle.addCommandPrototype((Command) type.newInstance());
      }      
   }

   private static void displayHelp()
   {
      java.io.PrintStream out = System.out;

      out.println("A JMX client to 'twiddle' with a remote JBoss server.");
      out.println();
      out.println("usage: " + PROGRAM_NAME + " [options] <command> [command_arguments]");
      out.println();
      out.println("options:");
      out.println("    -h, --help                Show this help message");
      out.println("        --help-commands       Show a list of commands");
      out.println("    -H=<command>              Show command specific help");
      out.println("    -c=command.properties     Specify the command.properties file to use");
      out.println("    -D<name>[=<value>]        Set a system property");
      out.println("    --                        Stop processing options");
      out.println("    -s, --server=<url>        The JNDI URL of the remote server");
      out.println("    -a, --adapter=<name>      The JNDI name of the RMI adapter to use");
      out.flush();
   }

   private static void processArguments(final String[] args) throws Exception
   {
      for(int a = 0; a < args.length; a ++)
      {
         log.debug("args["+a+"]="+args[a]);
      }
      String sopts = "-:hH:c:D:s:a:q";
      LongOpt[] lopts =
         {
            new LongOpt("help", LongOpt.NO_ARGUMENT, null, 'h'),
            new LongOpt("help-commands", LongOpt.NO_ARGUMENT, null, 0x1000),
            new LongOpt("server", LongOpt.REQUIRED_ARGUMENT, null, 's'),
            new LongOpt("adapter", LongOpt.REQUIRED_ARGUMENT, null, 'a'),
            new LongOpt("quiet", LongOpt.NO_ARGUMENT, null, 'q'),
         };

      Getopt getopt = new Getopt(PROGRAM_NAME, args, sopts, lopts);
      int code;

      PROCESS_ARGUMENTS:

        while ((code = getopt.getopt()) != -1)
        {
           switch (code)
           {
              case ':':
              case '?':
                 // for now both of these should exit with error status
                 System.exit(1);
                 break; // for completeness

                 // non-option arguments
              case 1:
                 {
                    // create the command
                    commandName = getopt.getOptarg();
                    log.debug("Command name: " + commandName);

                    // pass the remaining arguments (if any) to the command for processing
                    int i = getopt.getOptind();

                    if (args.length > i)
                    {
                       commandArgs = new String[args.length - i];
                       System.arraycopy(args, i, commandArgs, 0, args.length - i);
                    }
                    else
                    {
                       commandArgs = new String[0];
                    }

                    // Log the command options
                    if (log.isDebugEnabled())
                    {
                       log.debug("Command arguments: " + Strings.join(commandArgs, ","));
                    }

                    // We are done, execute the command
                    break PROCESS_ARGUMENTS;
                 }

                 // show command line help
              case 'h':
                 displayHelp();
                 System.exit(0);
                 break; // for completeness

                 // Show command help
              case 'H':
                 commandName = getopt.getOptarg();
                 commandHelp = true;
                 break PROCESS_ARGUMENTS;

                 // help-commands
              case 0x1000:
                 twiddle.displayCommandList();
                 System.exit(0);
                 break; // for completeness

              case 'c':
                 // Try value as a URL
                 String props = getopt.getOptarg();
                 try
                 {
                    cmdProps = new URL(props);
                 }
                 catch (MalformedURLException e)
                 {
                    log.debug("Failed to use cmd props as url", e);
                    File path = new File(props);
                    if( path.exists() == false )
                    {
                       String msg = "Failed to locate command props: " + props
                          + " as URL or file";
                       throw new IllegalArgumentException(msg);
                    }
                    cmdProps = path.toURL();
                 }
                 break;
                 // set a system property
              case 'D':
                 {
                    String arg = getopt.getOptarg();
                    String name, value;
                    int i = arg.indexOf("=");
                    if (i == -1)
                    {
                       name = arg;
                       value = "true";
                    }
                    else
                    {
                       name = arg.substring(0, i);
                       value = arg.substring(i + 1, arg.length());
                    }
                    System.setProperty(name, value);
                    break;
                 }

                 // Set the JNDI server URL
              case 's':
                 twiddle.setServerURL(getopt.getOptarg());
                 break;

                 // Set the adapter JNDI name
              case 'a':
                 twiddle.setAdapterName(getopt.getOptarg());
                 break;

                 // Enable quiet operations
              case 'q':
                 twiddle.setQuiet(true);
                 break;
           }
        }
   }
}
