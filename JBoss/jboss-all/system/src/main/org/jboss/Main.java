/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss;

import java.io.File;

import java.net.URL;
import java.net.MalformedURLException;

import java.util.Properties;
import java.util.List;
import java.util.LinkedList;
import java.util.Iterator;

import gnu.getopt.Getopt;
import gnu.getopt.LongOpt;

import org.jboss.system.server.Server;
import org.jboss.system.server.ServerConfig;
import org.jboss.system.server.ServerLoader;

/**
 * Provides a command line interface to start the JBoss server.
 *
 * <p>
 * To enable debug or trace messages durring boot change the Log4j 
 * configuration to use either <tt>log4j-debug.properties</tt>
 * <tt>log4j-trace.properties</tt> by setting the system property 
 * <tt>log4j.configuration</tt>:
 *
 * <pre>
 *   ./run.sh -Dlog4j.configuration=log4j-debug.properties
 * </pre>
 *
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author <a href="mailto:adrian.brock@happeningtimes.com">Adrian Brock</a>
 * @version $Revision: 1.17.2.9 $
 */
public class Main
{
   /** The JAXP library to use. */
   private String jaxpLibs = "xercesImpl.jar,xml-apis.jar,xalan.jar";

   /** The JMX library to use. */
   private String jmxLibs = "jboss-jmx.jar,gnu-regexp.jar";

   private String jdomLib = "jdom.jar";

   private String concurrentLib = "concurrent.jar";

   /** Extra libraries to load the server with .*/
   private List extraLibraries = new LinkedList();

   /** Extra classpath URLS to load the server with .*/
   private List extraClasspath = new LinkedList();

   /**
    * Server properties.  This object holds all of the required 
    * information to get the server up and running. Use System 
    * properties for defaults.
    */
   private Properties props = new Properties(System.getProperties());

   /**
    * Explicit constructor.
    */
   public Main() 
   {
      super();
   }

   /**
    * Boot up JBoss.
    *
    * @param args   The command line arguments.
    *
    * @throws Exception    Failed to boot.
    */
   public void boot(final String[] args) throws Exception
   {
      // First process the command line to pickup custom props/settings
      processCommandLine(args);

      // Auto set HOME_DIR to ../bin/run.jar if not set
      String homeDir = props.getProperty(ServerConfig.HOME_DIR);
      if (homeDir == null)
      {
         String path = Main.class.getProtectionDomain().getCodeSource().getLocation().getFile();
         /* The 1.4 JDK munges the code source file with URL encoding so run
          * this path through the decoder so that is JBoss starts in a path with
          * spaces we don't come crashing down.
         */
         path = java.net.URLDecoder.decode(path);
         File runJar = new File(path);
         File homeFile = runJar.getParentFile().getParentFile();
         homeDir = homeFile.getCanonicalPath();
      }
      props.setProperty(ServerConfig.HOME_DIR, homeDir);

      // Setup HOME_URL too, ServerLoader needs this
      String homeURL = props.getProperty(ServerConfig.HOME_URL);
      if (homeURL == null)
      {
         File file = new File(homeDir);
         homeURL = file.toURL().toString();
         props.setProperty(ServerConfig.HOME_URL, homeURL);
      }

      // Load the server instance
      ServerLoader loader = new ServerLoader(props);

      // Add JAXP and JMX libs
      loader.addLibraries(jaxpLibs);
      loader.addLibraries(jmxLibs);
      // xmbean needs jdom
      loader.addLibrary(jdomLib);
      // jmx UnifiedLoaderRepository needs a concurrent class...
      loader.addLibrary(concurrentLib);

      // Add any extra libraries
      for (int i = 0; i < extraLibraries.size(); i++)
      {
         loader.addLibrary((String)extraLibraries.get(i));
      }

      // Add any extra classapth
      for (int i = 0; i < extraClasspath.size(); i++)
      {
         loader.addURL((URL)extraClasspath.get(i));
      }

      // Load the server
      ClassLoader parentCL = Thread.currentThread().getContextClassLoader();
      Server server = loader.load(parentCL);

      // Initialize & make sure that shutdown exits the VM
      server.init(props);
      ServerConfig config = server.getConfig();
      config.setExitOnShutdown(true);
      
      // Start 'er up mate!
      server.start();
   }

   private URL makeURL(String urlspec) throws MalformedURLException
   {
      urlspec = urlspec.trim();
      
      URL url;
      
      try
      {
         url = new URL(urlspec);
         if (url.getProtocol().equals("file"))
         {
            // make sure the file is absolute & canonical file url
            File file = new File(url.getFile()).getCanonicalFile();
            url = file.toURL();
         }
      }
      catch (Exception e)
      {
         // make sure we have a absolute & canonical file url
         try
         {
            File file = new File(urlspec).getCanonicalFile();
            url = file.toURL();
         }
         catch (Exception n)
         {
            throw new MalformedURLException(n.toString());
         }
      }
      
      return url;
   }
   
   /** Process the command line... */
   private void processCommandLine(final String[] args) throws Exception
   {
      // set this from a system property or default to jboss
      String programName = System.getProperty("program.name", "jboss");
      String sopts = "-:hD:p:n:c:Vj:L:C:P:b:";
      LongOpt[] lopts =
      {
         new LongOpt("help", LongOpt.NO_ARGUMENT, null, 'h'),
         new LongOpt("patchdir", LongOpt.REQUIRED_ARGUMENT, null, 'p'),
         new LongOpt("netboot", LongOpt.REQUIRED_ARGUMENT, null, 'n'),
         new LongOpt("configuration", LongOpt.REQUIRED_ARGUMENT, null, 'c'),
         new LongOpt("version", LongOpt.NO_ARGUMENT, null, 'V'),
         new LongOpt("jaxp", LongOpt.REQUIRED_ARGUMENT, null, 'j'),
         new LongOpt("library", LongOpt.REQUIRED_ARGUMENT, null, 'L'),
         new LongOpt("classpath", LongOpt.REQUIRED_ARGUMENT, null, 'C'),
         new LongOpt("properties", LongOpt.REQUIRED_ARGUMENT, null, 'P'),
         new LongOpt("host", LongOpt.REQUIRED_ARGUMENT, null, 'b'),
      };
      
      Getopt getopt = new Getopt(programName, args, sopts, lopts);
      int code;
      String arg;
      props.setProperty(ServerConfig.SERVER_BIND_ADDRESS, "0.0.0.0");
      System.setProperty(ServerConfig.SERVER_BIND_ADDRESS, "0.0.0.0");
      while ((code = getopt.getopt()) != -1)
      {
         switch (code)
         {
            case ':':
            case '?':
               // for now both of these should exit with error status
               System.exit(1);
               break; // for completeness
               
            case 1:
               // this will catch non-option arguments
               // (which we don't currently care about)
               System.err.println(programName +
                                  ": unused non-option argument: " +
                                  getopt.getOptarg());
               break; // for completeness
               
            case 'h':
               // show command line help
               System.out.println("usage: " + programName + " [options]");
               System.out.println();
               System.out.println("options:");
               System.out.println("    -h, --help                    Show this help message");
               System.out.println("    -V, --version                 Show version information");
               System.out.println("    --                            Stop processing options");
               System.out.println("    -D<name>[=<value>]            Set a system property");
               System.out.println("    -p, --patchdir=<dir>          Set the patch directory; Must be absolute");
               System.out.println("    -n, --netboot=<url>           Boot from net with the given url as base");
               System.out.println("    -c, --configuration=<name>    Set the server configuration name");
               System.out.println("    -j, --jaxp=<type>             Set the JAXP impl type (ie. crimson)");
               System.out.println("    -L, --library=<filename>      Add an extra library to the loaders classpath");
               System.out.println("    -C, --classpath=<url>         Add an extra url to the loaders classpath");
               System.out.println("    -P, --properties=<url>        Load system properties from the given url");
               System.out.println("    -b, --host=<host or ip>       Bind address for all JBoss services");
               System.out.println();
               System.exit(0);
               break; // for completeness
               
            case 'D':
            {
               // set a system property
               arg = getopt.getOptarg();
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
               
            case 'p':
            {
               // set the patch URL
               URL url = makeURL(getopt.getOptarg());
               props.put(ServerConfig.PATCH_URL, url.toString());

               break;
            }
               
            case 'n':
               // set the net boot url
               arg = getopt.getOptarg();
               
               // make sure there is a trailing '/'
               if (!arg.endsWith("/")) arg += "/";
               
               props.put(ServerConfig.HOME_URL, new URL(arg).toString());
               break;
               
            case 'c':
               // set the server name
               arg = getopt.getOptarg();
               props.put(ServerConfig.SERVER_NAME, arg);
               break;
               
            case 'V':
            {
               // Package information for org.jboss
               Package jbossPackage = Package.getPackage("org.jboss");

               // show version information
               System.out.println("JBoss " + jbossPackage.getImplementationVersion());
               System.out.println();
               System.out.println("Distributable under LGPL license.");
               System.out.println("See terms of license at gnu.org.");
               System.out.println();
               System.exit(0);
               break; // for completness
            }

            case 'j':
            {
               // set the JAXP impl type
               arg = getopt.getOptarg().toLowerCase();
               String domFactoryType, saxFactoryType;
               
               if (arg.equals("crimson"))
               {
                  domFactoryType = "org.apache.crimson.jaxp.DocumentBuilderFactoryImpl";
                  saxFactoryType = "org.apache.crimson.jaxp.SAXParserFactoryImpl";
                  jaxpLibs = "crimson.jar";
               }
               else if (arg.equals("xerces"))
               {
                  domFactoryType = "org.apache.xerces.jaxp.DocumentBuilderFactoryImpl";
                  saxFactoryType = "org.apache.xerces.jaxp.SAXParserFactoryImpl";
                  jaxpLibs = "xmlParserAPIs.jar,xercesImpl.jar,xml-apis.jar";
               }
               else
               {
                  System.err.println("Invalid JAXP type: " + arg + " (Expected 'crimson' or 'xerces')");
                  // don't continue, user needs to fix this!
                  System.exit(1);
                  
                  // trick the compiler, so it does not complain that
                  // the above variables might not being set
                  break;
               }
               
               // set the controlling properties
               System.setProperty("javax.xml.parsers.DocumentBuilderFactory", domFactoryType);
               System.setProperty("javax.xml.parsers.SAXParserFactory", saxFactoryType);
                                  
               break;
            }

            case 'L':
               arg = getopt.getOptarg();
               extraLibraries.add(arg);
               break;

            case 'C':
            {
               URL url = makeURL(getopt.getOptarg());
               extraClasspath.add(url);
               break;
            }
       
            case 'P': 
            {
               // Set system properties from url/file
               URL url = makeURL(getopt.getOptarg());
               Properties props = System.getProperties();
               props.load(url.openConnection().getInputStream());
               break;
            }
            case 'b':
               arg = getopt.getOptarg();
               props.put(ServerConfig.SERVER_BIND_ADDRESS, arg);
               System.setProperty(ServerConfig.SERVER_BIND_ADDRESS, arg);
               break;
            default:
               // this should not happen,
               // if it does throw an error so we know about it
               throw new Error("unhandled option code: " + code);
         }
      }
   }
   
   /**
    * This is where the magic begins.
    *
    * <P>Starts up inside of a "jboss" thread group to allow better
    *    identification of JBoss threads.
    *
    * @param args    The command line arguments.
    */
   public static void main(final String[] args) throws Exception
   {
      Runnable worker = new Runnable() {   
            public void run()
            {
               try
               {
                  Main main = new Main();
                  main.boot(args);
               }
               catch (Exception e)
               {
                  System.err.println("Failed to boot JBoss:");
                  e.printStackTrace();
               }
            }
   
         };

      ThreadGroup threads = new ThreadGroup("jboss");
      new Thread(threads, worker, "main").start();
   }
   
   /**
    * This method is here so that if JBoss is running under
    * Alexandria (An NT Service Installer), Alexandria can shutdown
    * the system down correctly.
    */
   public static void systemExit(String argv[])
   {
      System.exit(0);
   }
}
