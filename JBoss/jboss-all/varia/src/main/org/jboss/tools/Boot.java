/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.tools;

import java.net.*;
import java.lang.reflect.*;
import java.io.*;
import java.util.*;

/**
 * Starts multiple applications using seperate classloaders.
 * This allows multiple applications to co-exist even if they typicaly could not due to 
 * class version problems.  Each application is started in it's own thread.
 * 
 * Usage is Boot [-debug] -cp app-classpath app-class-name app-arguments ( , -cp app-classpath app-class-name app-arguments )*
 * 
 * Where:
 * 	app-classpath is a comma seperated URL form classpath to the application classes.
 * 	app-class-name is the class that will be started
 * 	app-arguments will be the String[] that will be passed to the main method of the application class
 * 
 * Jboss + Another Application boot example:
 *     Boot -cp file:run.jar org.jboss.Main default , -cp file:./myapp.jar,file:./util.jar test.App2TEST arg1 arg2
 * Would start the JBoss Server using the default configuration and it would
 * start the test.App2TEST application.  
 * Important Note: Notice that there are spaces before and after the ","!!!
 * 
 * You can now boot other applications via ths Boot class from withing one of the applications 
 * that was booted by the Boot.
 * 
 * Example usage:
 * <code>
 * Boot b = Boot.getInstance();
 * Boot.ApplicationBoot ab = b.createApplicationBoot();
 * ab.applicationClass = "org.jboss.Main"
 * ab.classpath.add(new URL("file:run.jar"));
 * ab.args.add("default");
 * 
 * // this would start the application in a new thread.
 * b.startApplication( ab );
 * 
 * // Would boot the appp in the current thread.
 * ab.boot();
 * 
 * </code>
 * 
 * @author <a href="mailto:cojonudo14@hotmail.com">Hiram Chirino</a>
 */
public class Boot
{
   /**
    * Indicates whether this instance is running in debug mode.
    */
   protected boolean verbose = false;

   /**
    * For each booted application, we will store a ApplicationBoot object in this linked list.
    */
   protected LinkedList applicationBoots = new LinkedList();
	static Boot instance;
	ThreadGroup bootThreadGroup;

	/**
	 * If boot is accessed via an API, force the use of the getInstance() method.
	 */
	protected Boot () {
	   instance = this;
	   bootThreadGroup = Thread.currentThread().getThreadGroup();
	}
	
	public static Boot getInstance() {
	   if( instance == null )
		   instance = new Boot();
	   return instance;
	}

   /**
    * Represents an application that can be booted.
    */
   public class ApplicationBoot implements Runnable
   {
		/** LinkedList of URL that will be used to load the application's classes and resources */
      public LinkedList classpath = new LinkedList();
		/** The applications class that will be executed. */
      public String applicationClass;
		/** The aruments that will appsed to the application. */
      public LinkedList args = new LinkedList();
      
      protected URLClassLoader classloader;
      protected Thread bootThread;

      /**
       * This is what actually loads the application classes and
       * invokes the main method.  We send any unhandled exceptions to 
       * System.err
       */
      public void run()
      {
         try
         {
            boot();
         }
         catch (Throwable e)
         {
            System.err.println("Exception durring " + applicationClass + " application run: ");
            e.printStackTrace(System.err);
         }
      }

      /**
       * This is what actually loads the application classes and
       * invokes the main method.
       */
      public void boot()
         throws ClassNotFoundException, NoSuchMethodException, IllegalAccessException, InvocationTargetException
      {
	   	verbose("Booting: "+applicationClass);
	   	verbose("Classpath: "+classpath);
	   	verbose("Arguments: "+args);
	   	
         bootThread = Thread.currentThread();
         URL urls[] = new URL[classpath.size()];
         urls = (URL[]) classpath.toArray(urls);

         String passThruArgs[] = new String[args.size()];
         passThruArgs = (String[]) args.toArray(passThruArgs);

			// Save the current loader so we can restore it.
			ClassLoader oldCL = Thread.currentThread().getContextClassLoader();
			
         try {
            
            classloader = new URLClassLoader(urls); // The parent is the system CL
            Thread.currentThread().setContextClassLoader(classloader);
            
            Class appClass = classloader.loadClass(applicationClass);
            Method mainMethod = appClass.getMethod("main", new Class[] { String[].class });

            mainMethod.invoke(null, new Object[] { passThruArgs });
         }
         catch (InvocationTargetException e)
         {
            if (e.getTargetException() instanceof Error)
               throw (Error) e.getTargetException();
            else
               throw e;
         }
         finally
         {
            // Restore the previous classloader ( in case we were called directly 
            // an not via a Thread.start() )
            Thread.currentThread().setContextClassLoader(oldCL);
         }
      }
   }
   
   /**
    * This can be used to boot another application 
    * From within another one.
    * 
    * @returns ApplicationBoot data structure that must be configured before the application can be booted.
    */
   public ApplicationBoot createApplicationBoot() {
      return new ApplicationBoot();
   } 

   /**
    * Boots the application in a new threadgroup and thread.
    * 
    * @param bootData the application to boot.
    * @exception thrown if a problem occurs during launching
    */
   synchronized public void startApplication(ApplicationBoot bootData) throws Exception
   {
      if( bootData == null )
      	throw new NullPointerException("Invalid argument: bootData argument was null");
   
      applicationBoots.add(bootData);
      ThreadGroup threads = new ThreadGroup(bootThreadGroup, bootData.applicationClass);
      new Thread(threads, bootData, "main").start();
   }
   
   synchronized public ApplicationBoot[] getStartedApplications() {
      ApplicationBoot rc[] = new ApplicationBoot[applicationBoots.size()];
      return (ApplicationBoot[])applicationBoots.toArray(rc);
   }
      
	/** logs verbose message to the console */   
   protected void verbose(String msg) {
      if( verbose )
      	System.out.println("[Boot] "+msg);
   }
   
	//////////////////////////////////////////////////////////////////////
	//
	// THE FOLLOWING SET OF FUNCTIONS ARE RELATED TO PROCESSING COMMAND LINE
	// ARGUMENTS.
	//	
	//////////////////////////////////////////////////////////////////////
   protected static final String HELP = "-help";
   protected static final String VERBOSE = "-verbose";
   protected static final String BOOT_APP_SEPERATOR = System.getProperty("org.jboss.Boot.APP_SEPERATOR", ",");
   protected static final String CP = "-cp";

   protected static class InvalidCommandLineException extends Exception {
      /**
       * Constructor for InvalidCommandLineException.
       * @param s
       */
      public InvalidCommandLineException(String s)
      {
         super(s);
      }
	}
   
   /**
    * Main entry point when called from the command line
    * @param args the command line arguments
    */
   public static void main(String[] args)
   {
      Boot boot = Boot.getInstance();
      
      // Put the args in a linked list since it easier to work with.
      LinkedList llargs = new LinkedList();
      for (int i = 0; i < args.length; i++)
         llargs.add(args[i]);

		try {
         
         LinkedList ab = boot.processCommandLine(llargs);
         Iterator i = ab.iterator();
         while (i.hasNext())
         {
            ApplicationBoot bootData = (ApplicationBoot) i.next();
            boot.startApplication(bootData);
         }
         
		} catch ( InvalidCommandLineException e ) {
		   System.err.println("Invalid Usage: "+e.getMessage());
		   System.err.println();
		   showUsage();
		   System.exit(1);
		} catch ( Throwable e ) {
         System.err.println("Failure occured while executing application: ");
         e.printStackTrace(System.err);
		   System.exit(1);
		}
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

   /**
    * Processes the Boot class's command line arguments
    * 
    * @return a linked list with ApplicationBoot objects
    * @param args the command line arguments
    */
   protected LinkedList processCommandLine(LinkedList args) throws Exception
   {
      LinkedList rc = new LinkedList();

      processBootOptions(args);
      while (args.size() > 0)
      {
         ApplicationBoot d = processAppBootCommandLine(args);
         if (d != null)
            rc.add(d);
      }

      if (rc.size() == 0)
      {
         throw new InvalidCommandLineException("An application class name must be provided.");
      }

      return rc;
   }

   /**
    * Processes to global options.
    * 
    * @param args the command line arguments
    */
   protected void processBootOptions(LinkedList args) throws Exception
   {
      Iterator i = args.iterator();
      while (i.hasNext())
      {
         String arg = (String) i.next();
         if (arg.equalsIgnoreCase(VERBOSE))
         {
            verbose = true;
            i.remove();
            continue;
         }
         if (arg.equalsIgnoreCase(HELP))
         {
            showUsage();
            System.exit(0);
         }

         // Didn't recognize it a boot option, then we must have started the application 
         // boot options.
         return;
      }
   }

   protected static void showUsage()
   {
      String programName = System.getProperty("org.jboss.Boot.proces-name", "boot");
      
      System.out.println("usage: " + programName + " [boot-options] [app-options] class [args..]");
      System.out.println("       to execute a class");
      System.out.println("   or  " + programName + " [boot-options] [app-options] class-1 [args..] , ... , [app-options] class-n [args..]");
      System.out.println("       to execute multiple classes");
      System.out.println();
      System.out.println("boot-options:");
      System.out.println("    -help         show this help message");
      System.out.println("    -verbose      display detail messages regarding the boot process.");
      System.out.println("app-options:");
      System.out.println("    -cp <directories and zip/jar urls separated by ,> ");
      System.out.println("                  set search path for application classes and resources");
      System.out.println();
   }

   /**
    * Processes the command line argumenst for the next application on the command line.
    * 
    * @param args the command line arguments
    */
   protected ApplicationBoot processAppBootCommandLine(LinkedList args) throws Exception
   {
      ApplicationBoot rc = new ApplicationBoot();
      Iterator i = args.iterator();

      while (i.hasNext())
      {
         String arg = (String) i.next();
         i.remove();

         if (rc.applicationClass == null)
         {
            if (arg.equalsIgnoreCase(CP))
            {
               if (!i.hasNext())
                  throw new InvalidCommandLineException("Invalid option: classpath missing after the " + CP + " option.");
               String cp = (String) i.next();
               i.remove();

               StringTokenizer st = new StringTokenizer(cp, ",", false);
               while (st.hasMoreTokens())
               {
                  String t = st.nextToken();
                  if (t.length() == 0)
                     continue;
                  try
                  {
                     URL u = new URL(t);
                     rc.classpath.add(u);
                  }
                  catch (MalformedURLException e)
                  {
                     throw new InvalidCommandLineException("Application classpath value was invalid: " + e.getMessage());
                  }
               }
               continue;
            }

            rc.applicationClass = arg;
            continue;
         }
         else
         {
            if (arg.equalsIgnoreCase(BOOT_APP_SEPERATOR))
            {
               break;
            }
            rc.args.add(arg);
         }

      }

      if (rc.applicationClass == null)
         return null;

      return rc;
   }

}
