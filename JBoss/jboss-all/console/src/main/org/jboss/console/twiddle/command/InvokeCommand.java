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

import java.beans.PropertyEditor;

import javax.management.ObjectName;
import javax.management.MBeanInfo;
import javax.management.MBeanOperationInfo;
import javax.management.MBeanServer;
import javax.management.MBeanParameterInfo;

import gnu.getopt.Getopt;
import gnu.getopt.LongOpt;

import org.jboss.util.Strings;
import org.jboss.util.propertyeditor.PropertyEditors;

/**
 * Invoke an operation on an MBean.
 *
 * @version <tt>$Revision: 1.3.2.2 $</tt>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class InvokeCommand
   extends MBeanServerCommand
{
   public static final int QUERY_FIRST = 0;
   public static final int QUERY_ALL = 1;
   
   private int type = QUERY_FIRST;

   private String query;

   private String opName;
   
   private List opArgs = new ArrayList(5); // probably not more than 5 args
   
   public InvokeCommand()
   {
      super("invoke", "Invoke an operation on an MBean");
   }
   
   public void displayHelp()
   {
      PrintWriter out = context.getWriter();
      
      out.println(desc);
      out.println();
      out.println("usage: " + name + " [options] <query> <operation> (<arg>)*");
      out.println();
      out.println("options:");
      out.println("    -q, --query-type[=<type>]    Treat object name as a query");
      out.println("    --                           Stop processing options");
      out.println();
      out.println("query type:");      
      out.println("    f[irst]    Only invoke on the first matching name [default]");
      out.println("    a[ll]      Invoke on all matching names");
   }
   
   private void processArguments(final String[] args)
      throws CommandException
   {
      log.debug("processing arguments: " + Strings.join(args, ","));

      if (args.length == 0) {
         throw new CommandException("Command requires arguments");
      }
      
      String sopts = "-:q:";
      LongOpt[] lopts =
      {
         new LongOpt("query-type", LongOpt.OPTIONAL_ARGUMENT, null, 'q'),
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
                     query = arg;
                     log.debug("query: " + query);
                     break;

                  case 1:
                     opName = arg;
                     log.debug("operation name: " + opName);
                     break;

                  default:
                     opArgs.add(arg);
                     break;
               }
               break;
            }

            // Set the query type
            case 'q':
            {
               String arg = getopt.getOptarg();

               //
               // jason: need a uniqueness mapper, like getopt uses for options...
               //

               if (arg.equals("f") || arg.equals("first")) {
                  type = QUERY_FIRST;
               }
               else if (arg.equals("a") || arg.equals("all")) {
                  type = QUERY_ALL;
               }
               else {
                  throw new CommandException("Invalid query type: " + arg);
               }

               log.debug("Query type: " + type);
               
               break;
            }
         }
      }
   }

   private void invoke(final ObjectName name)
      throws Exception
   {
      log.debug("Invoke " + name);

      MBeanServer server = getMBeanServer();
      
      // get mbean info for this mbean
      MBeanInfo info = server.getMBeanInfo(name);
      
      // does it even have an operation of this name?
      MBeanOperationInfo[] ops = info.getOperations();
      MBeanParameterInfo[] sigInfo = null;

      for (int i=0; i<ops.length; i++) {
         if (ops[i].getName().equals(opName)) {
            MBeanParameterInfo[] temp = ops[i].getSignature();
            log.debug("found op with sig: " + Strings.join(temp, ","));
            
            // for now first op with correct param count wins

            //
            // jason: add options to specify the sig later
            //
            
            if (temp.length == opArgs.size()) {
               sigInfo = temp;
            }
            else {
               //throw new CommandException("MBean has operation, but signature invalid");
            }
         }
      }

      if (sigInfo == null) {
         throw new CommandException("MBean has such operation named: " + opName);
      }
      
      // construct operation signature array
      String[] sig = new String[sigInfo.length];
      for (int i=0; i<sigInfo.length; i++) {
         sig[i] = sigInfo[i].getType();
      }
      log.debug("Using signature: " + Strings.join(sig, ","));
      
      // convert parameters with PropertyEditor
      Object[] params = new Object[sig.length];
      for (int i=0; i<sig.length; i++) {
         PropertyEditor editor = PropertyEditors.getEditor(sig[i]);
         editor.setAsText((String)opArgs.get(i));
         params[i] = editor.getValue();
      }
      log.debug("Using params: " + Strings.join(params, ","));
      
      // invoke the operation
      Object result = server.invoke(name, opName, params, sig);
      log.debug("Raw result: " + result);

      if (!context.isQuiet()) {
         // Translate the result to text
         String resultText = null;
         
         if (result != null)
         {
            PropertyEditor editor = PropertyEditors.getEditor(result.getClass());
            editor.setValue(result);
            resultText = editor.getAsText();
            log.debug("Converted result: " + resultText);
         }
         else
         {
            resultText = "'null'";
         }
      
         // render results to out
         PrintWriter out = context.getWriter();
         out.println(resultText);
         out.flush();
      }
   }
   
   public void execute(String[] args) throws Exception
   {
      processArguments(args);
      
      if (query == null)
         throw new CommandException("Missing MBean query");

      if (opName == null)
         throw new CommandException("Missing operation name");

      log.debug("operation arguments: " + opArgs);
      
      // get the list of object names to work with
      ObjectName[] names = queryMBeans(query);
      if (type == QUERY_FIRST) {
         names = new ObjectName[] { names[0] };
      }

      for (int i=0; i<names.length; i++) {
         invoke(names[i]);
      }
   }
}
