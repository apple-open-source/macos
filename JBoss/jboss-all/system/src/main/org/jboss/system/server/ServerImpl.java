/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.system.server;

import java.io.File;
import java.net.URL;

import java.util.List;
import java.util.ArrayList;
import java.util.Date;
import java.util.Iterator;
import java.util.Properties;

import javax.management.Attribute;
import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.NotificationBroadcasterSupport;
import javax.management.ObjectName;
import javax.management.Notification;
import javax.management.NotificationListener;
import javax.management.NotificationFilter;
import javax.management.ListenerNotFoundException;
import javax.management.MBeanNotificationInfo;
import javax.management.NotificationBroadcaster;

import org.jboss.Version;
import org.jboss.deployment.MainDeployerMBean;
import org.jboss.deployment.IncompleteDeploymentException;
import org.jboss.logging.Logger;
import org.jboss.mx.loading.UnifiedClassLoader;
import org.jboss.mx.server.ServerConstants;
import org.jboss.net.protocol.URLStreamHandlerFactory;
import org.jboss.util.StopWatch;
import org.jboss.util.file.Files;
import org.jboss.util.file.FileSuffixFilter;
import org.jboss.mx.util.JMXExceptionDecoder;
import org.jboss.mx.util.ObjectNameFactory;
import org.jboss.mx.util.MBeanProxyExt;
import org.jboss.system.ServiceControllerMBean;


/**
 * The main container component of a JBoss server instance.
 *
 * <h3>Concurrency</h3>
 * This class is <b>not</b> thread-safe.
 *
 * @jmx:mbean name="jboss.system:type=Server"
 *
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.25.2.18 $
 */
public class ServerImpl
   implements Server, ServerImplMBean, NotificationBroadcaster
{
   private final static ObjectName DEFAULT_LOADER_NAME =
      ObjectNameFactory.create(ServerConstants.DEFAULT_LOADER_NAME);

   /** Instance logger. */
   private Logger log;

   /** Container for version information. */
   private final Version version = Version.getInstance();

   /** Package information for org.jboss */
   private final Package jbossPackage = Package.getPackage("org.jboss");

   /** The basic configuration for the server. */
   private ServerConfigImpl config;

   /** The JMX MBeanServer which will serve as our communication bus. */
   private MBeanServer server;

   /** When the server was started. */
   private Date startDate;

   /** Flag to indicate if we are started. */
   private boolean started;

   /** The JVM shutdown hook */
   private ShutdownHook shutdownHook;

   /** The JBoss Life Thread */
   private LifeThread lifeThread;

   /** The NotificationBroadcaster implementation delegate */
   private NotificationBroadcasterSupport broadcasterSupport;

   /**
    * No-arg constructor for {@link ServerLoader}.
    */
   public ServerImpl()
   {
   }

   /**
    * Initialize the Server instance.
    *
    * @param props     The configuration properties for the server.
    * @return          Typed server configuration object.
    *
    * @throws IllegalStateException    Already initialized.
    * @throws Exception                Failed to initialize.
    */
   public void init(final Properties props) throws IllegalStateException, Exception
   {
      if (props == null)
         throw new IllegalArgumentException("props is null");
      if (config != null)
         throw new IllegalStateException("already initialized");

      ClassLoader oldCL = Thread.currentThread().getContextClassLoader();

      try
      {
         Thread.currentThread().setContextClassLoader(getClass().getClassLoader());
         doInit(props);
      }
      finally
      {
         Thread.currentThread().setContextClassLoader(oldCL);
      }
   }

   /** Actually does the init'ing... */
   private void doInit(final Properties props) throws Exception
   {
      // Create a new config object from the give properties
      this.config = new ServerConfigImpl(props);

      // Create the NotificationBroadcaster delegate
      broadcasterSupport = new NotificationBroadcasterSupport();

      // Set the VM temp directory to the server tmp dir
      boolean overrideTmpDir = Boolean.getBoolean("jboss.server.temp.dir.overrideJavaTmpDir");
      if( overrideTmpDir )
      {
         File serverTmpDir = config.getServerTempDir();
         System.setProperty("java.io.tmpdir", serverTmpDir.getCanonicalPath());
      }

      // masqurade as Server.class (cause Impl is not really important)
      log = Logger.getLogger(Server.class);

      // Setup URL handlers - do this before initializing the ServerConfig
      initURLHandlers();
      config.initURLs();

      log.info("Starting JBoss (MX MicroKernel)...");

      // Show what release this is...
      log.info("Release ID: " +
               jbossPackage.getImplementationTitle() + " " +
               jbossPackage.getImplementationVersion());

      log.debug("Using config: " + config);

      // make sure our impl type is exposed
      log.debug("Server type: " + getClass());

      // Log the basic configuration elements
      log.info("Home Dir: " + config.getHomeDir());
      log.info("Home URL: " + config.getHomeURL());
      log.info("Library URL: " + config.getLibraryURL());
      log.info("Patch URL: " + config.getPatchURL());
      log.info("Server Name: " + config.getServerName());
      log.info("Server Home Dir: " + config.getServerHomeDir());
      log.info("Server Home URL: " + config.getServerHomeURL());
      log.info("Server Data Dir: " + config.getServerDataDir());
      log.info("Server Temp Dir: " + config.getServerTempDir());
      log.info("Server Config URL: " + config.getServerConfigURL());
      log.info("Server Library URL: " + config.getServerLibraryURL());
      log.info("Root Deployment Filename: " + config.getRootDeploymentFilename());
   }

   /**
    * The <code>initURLHandlers</code> method calls
    * internalInitURLHandlers.  if requireJBossURLStreamHandlers is
    * false, any exceptions are logged and ignored.
    *
    */
   private void initURLHandlers()
   {
      if (config.getRequireJBossURLStreamHandlerFactory())
      {
         internalInitURLHandlers();
      } // end of if ()
      else
      {
         try
         {
            internalInitURLHandlers();
         }
         catch (SecurityException e)
         {
            log.warn("You do not have permissions to set URLStreamHandlerFactory", e);
         } // end of try-catch
         catch (Error e)
         {
            log.warn("URLStreamHandlerFactory already set", e);
         } // end of catch
      } // end of else
   }

   /**
    * Set up our only URLStreamHandlerFactory.
    * This is needed to ensure Sun's version is not used (as it leaves files
    * locked on Win2K/WinXP platforms.
    */
   private void internalInitURLHandlers()
   {
      // Install a URLStreamHandlerFactory that uses the TCL
      URL.setURLStreamHandlerFactory(new URLStreamHandlerFactory());

      // Preload JBoss URL handlers
      URLStreamHandlerFactory.preload();

      // Include the default JBoss protocol handler package
      String handlerPkgs = System.getProperty("java.protocol.handler.pkgs");
      if (handlerPkgs != null)
      {
         handlerPkgs += "|org.jboss.net.protocol";
      }
      else
      {
         handlerPkgs = "org.jboss.net.protocol";
      }
      System.setProperty("java.protocol.handler.pkgs", handlerPkgs);
   }

   /**
    * Get the typed server configuration object which the
    * server has been initalized to use.
    *
    * @return          Typed server configuration object.
    *
    * @throws IllegalStateException    Not initialized.
    */
   public ServerConfig getConfig() throws IllegalStateException
   {
      if (config == null)
         throw new IllegalStateException("not initialized");

      return config;
   }

   /**
    * Check if the server is started.
    *
    * @return   True if the server is started, else false.
    * @jmx:managed-attribute
    */
   public boolean isStarted()
   {
      return started;
   }

   /**
    * Start the Server instance.
    *
    * @throws IllegalStateException    Already started or not initialized.
    * @throws Exception                Failed to start.
    */
   public void start() throws IllegalStateException, Exception
   {
      // make sure we are initialized
      getConfig();

      // make sure we aren't started yet
      if (started)
         throw new IllegalStateException("already started");

      ClassLoader oldCL = Thread.currentThread().getContextClassLoader();

      try
      {
         Thread.currentThread().setContextClassLoader(getClass().getClassLoader());

         // Deal with those pesky JMX throwables
         try
         {
            doStart();
         }
         catch (Exception e)
         {
            JMXExceptionDecoder.rethrow(e);
         }
      }
      catch (Throwable t)
      {
         log.debug("Failed to start", t);

         if (t instanceof Exception)
            throw (Exception)t;
         if (t instanceof Error)
            throw (Error)t;

         throw new org.jboss.util.UnexpectedThrowable(t);
      }
      finally
      {
         Thread.currentThread().setContextClassLoader(oldCL);
      }
   }

   /** Actually does the starting... */
   private void doStart() throws Exception
   {
      // See how long it takes us to start up
      StopWatch watch = new StopWatch(true);

      // remeber when we we started
      startDate = new Date();

      log.info("Starting General Purpose Architecture (GPA)...");

      // Create the MBeanServer
      server = MBeanServerFactory.createMBeanServer("jboss");
      log.debug("Created MBeanServer: " + server);

      // Register server components
      server.registerMBean(this, ServerImplMBean.OBJECT_NAME);
      server.registerMBean(config, ServerConfigImplMBean.OBJECT_NAME);

      // Initialize spine boot libraries
      UnifiedClassLoader ucl = initBootLibraries();

      // Set ServiceClassLoader as classloader for the construction of
      // the basic system
      Thread.currentThread().setContextClassLoader(ucl);

      // General Purpose Architecture information
      createMBean("org.jboss.system.server.ServerInfo");

      // Service Controller
      ObjectName controller = createMBean("org.jboss.system.ServiceController");

      // Main Deployer
      ObjectName mainDeployer = startBootService(controller, "org.jboss.deployment.MainDeployer");
      server.setAttribute(mainDeployer,
                          new Attribute("ServiceController", controller));

      // Install the shutdown hook
      shutdownHook = new ShutdownHook(controller, mainDeployer);
      shutdownHook.setDaemon(true);

      try
      {
         Runtime.getRuntime().addShutdownHook(shutdownHook);
         log.debug("Shutdown hook added");
      }
      catch (Exception e)
      {
         log.warn("Failed to add shutdown hook; ignoring", e);
      }

      // JARDeployer, required to process <classpath>
      startBootService(controller, "org.jboss.deployment.JARDeployer");

      // SARDeployer, required to process *-service.xml
      startBootService(controller, "org.jboss.deployment.SARDeployer");

      log.info("Core system initialized");

      // TODO: Split up init (ie. create) from start ops so we can expose more control
      //       to embeded clients.

      // Ok, now deploy the root deployable to finish the job

      MainDeployerMBean md = (MainDeployerMBean)
         MBeanProxyExt.create(MainDeployerMBean.class, mainDeployer, server);

      try
      {
         md.deploy(config.getServerConfigURL() + config.getRootDeploymentFilename());
      }
      catch (IncompleteDeploymentException e) {
         log.error("Root deployment has missing dependencies; continuing", e);
      }

      lifeThread = new LifeThread();
      lifeThread.start();

      watch.stop();
      // Tell the world how fast it was =)
      log.info("JBoss (MX MicroKernel) [" + jbossPackage.getImplementationVersion() +
               "] Started in " + watch);
      started = true;

      // Send a notification that the startup is complete
      Notification msg = new Notification(START_NOTIFICATION_TYPE, this, 1);
      msg.setUserData(new Long(watch.getLapTime()));
      sendNotification(msg);
   }

   /**
    * Instantiate and register a service for the given classname into the MBean server.
    */
   private ObjectName createMBean(final String classname)
      throws Exception
   {
      ObjectName name = server.createMBean(classname, null).getObjectName();
      log.debug("Created system MBean: " + name);

      return name;
   }

   /**
    * Instantiate/register, create and start a service for the given classname.
    */
   private ObjectName startBootService(final ObjectName controllerName, final String classname)
      throws Exception
   {
      ObjectName name = createMBean(classname);

      // now go through the create/start sequence on the new service

      Object[] args = { name };
      String[] sig = { ObjectName.class.getName() };

      server.invoke(controllerName, "create", args, sig);
      server.invoke(controllerName, "start", args, sig);

      return name;
   }

   /**
    * Initialize the boot libraries.
    */
   private UnifiedClassLoader initBootLibraries() throws Exception
   {
      boolean debug = log.isDebugEnabled();

      // Build the list of URL for the spine to boot
      List list = new ArrayList();

      // Add the patch URL.  If the url protocol is file, then
      // add the contents of the directory it points to
      URL patchURL = config.getPatchURL();
      if (patchURL != null)
      {
         if (patchURL.getProtocol().equals("file"))
         {
            File dir = new File(patchURL.getFile());
            if (dir.exists())
            {
               // Add the local file patch directory
               list.add(dir.toURL());

               // Add the contents of the directory too
               File[] jars = dir.listFiles(new FileSuffixFilter(new String[] { ".jar", ".zip" }, true));

               for (int j = 0; jars != null && j < jars.length; j++)
               {
                  list.add(jars[j].getCanonicalFile().toURL());
               }
            }
         }
         else
         {
            list.add(patchURL);
         }
      }

      // Add the server configuration directory to be able to load config files as resources
      list.add(config.getServerConfigURL());

      // Not needed, ServerImpl will have the basics on its classpath from ServerLoader
      // may want to bring this back at some point if we want to have reloadable core
      // components...

      // URL libraryURL = config.getLibraryURL();
      // list.add(new URL(libraryURL, "jboss-spine.jar"));

      log.debug("Boot url list: " + list);

      // Create loaders for each URL
      UnifiedClassLoader loader = null;
      for (Iterator iter = list.iterator(); iter.hasNext();)
      {
         URL url = (URL)iter.next();
         if (debug)
         {
            log.debug("Creating loader for URL: " + url);
         }

         // This is a boot URL, so key it on itself.
         Object[] args = {url, Boolean.TRUE};
         String[] sig = {"java.net.URL", "boolean"};
         loader = (UnifiedClassLoader) server.invoke(DEFAULT_LOADER_NAME, "newClassLoader", args, sig);
      }
      return loader;
   }

   /**
    * Shutdown the Server instance and run shutdown hooks.
    *
    * <p>If the exit on shutdown flag is true, then {@link #exit}
    *    is called, else only the shutdown hook is run.
    *
    * @jmx:managed-operation
    *
    * @throws IllegalStateException    No started.
    */
   public void shutdown() throws IllegalStateException
   {
      if (!started)
         throw new IllegalStateException("not started");

      final ServerImpl server = this;

      log.info("Shutting down");

      boolean exitOnShutdown = config.getExitOnShutdown();
      boolean blockingShutdown = config.getBlockingShutdown();
      if (log.isDebugEnabled())
      {
         log.debug("exitOnShutdown: " + exitOnShutdown);
         log.debug("blockingShutdown: " + blockingShutdown);
      }

      lifeThread.interrupt();

      if (exitOnShutdown)
      {
         server.exit(0);
      }
      else if (blockingShutdown)
      {
         shutdownHook.shutdown();
      } // end of if ()
      else
      {
         // start in new thread to give positive
         // feedback to requesting client of success.
         new Thread()
         {
            public void run()
            {
               // just run the hook, don't call System.exit, as we may
               // be embeded in a vm that would not like that very much
               shutdownHook.shutdown();
            }
         }.start();
      }
   }

   /**
    * Shutdown the server, the JVM and run shutdown hooks.
    *
    * @jmx:managed-operation
    *
    * @param exitcode   The exit code returned to the operating system.
    */
   public void exit(final int exitcode)
   {
      // start in new thread so that we might have a chance to give positive
      // feed back to requesting client of success.
      new Thread()
      {
         public void run()
         {
            log.info("Shutting down the JVM now!");
            Runtime.getRuntime().exit(exitcode);
         }
      }.start();
   }

   /**
    * Shutdown the server, the JVM and run shutdown hooks.  Exits with
    * code 1.
    *
    * @jmx:managed-operation
    */
   public void exit()
   {
      exit(1);
   }

   /**
    * Forcibly terminates the currently running Java virtual machine.
    *
    * @param exitcode   The exit code returned to the operating system.
    *
    * @jmx:managed-operation
    */
   public void halt(final int exitcode)
   {
      // Send a notification that the startup is complete
      Notification msg = new Notification(STOP_NOTIFICATION_TYPE, this, 2);
      sendNotification(msg);

      // start in new thread so that we might have a chance to give positive
      // feed back to requesting client of success.
      new Thread()
      {
         public void run()
         {
            System.err.println("Halting the system now!");
            Runtime.getRuntime().halt(exitcode);
         }
      }.start();
   }

   /**
    * Forcibly terminates the currently running Java virtual machine.
    * Exits with code 1.
    *
    * @jmx:managed-operation
    */
   public void halt()
   {
      halt(1);
   }


   ///////////////////////////////////////////////////////////////////////////
   //                            Runtime Access                             //
   ///////////////////////////////////////////////////////////////////////////

   /** A simple helper used to log the Runtime memory information. */
   private void logMemoryUsage(final Runtime rt)
   {
      log.info("Total/free memory: " + rt.totalMemory() + "/" + rt.freeMemory());
   }

   /**
    * Hint to the JVM to run the garbage collector.
    *
    * @jmx:managed-operation
    */
   public void runGarbageCollector()
   {
      Runtime rt = Runtime.getRuntime();

      logMemoryUsage(rt);
      rt.gc();
      log.info("Hinted to the JVM to run garbage collection");
      logMemoryUsage(rt);
   }

   /**
    * Hint to the JVM to run any pending object finailizations.
    *
    * @jmx:managed-operation
    */
   public void runFinalization()
   {
      Runtime.getRuntime().runFinalization();
      log.info("Hinted to the JVM to run any pending object finalizations");
   }

   /**
    * Enable or disable tracing method calls at the Runtime level.
    *
    * @jmx:managed-operation
    */
   public void traceMethodCalls(final Boolean flag)
   {
      Runtime.getRuntime().traceMethodCalls(flag.booleanValue());
   }

   /**
    * Enable or disable tracing instructions the Runtime level.
    *
    * @jmx:managed-operation
    */
   public void traceInstructions(final Boolean flag)
   {
      Runtime.getRuntime().traceInstructions(flag.booleanValue());
   }


   ///////////////////////////////////////////////////////////////////////////
   //                          Server Information                           //
   ///////////////////////////////////////////////////////////////////////////

   /**
    * @jmx:managed-attribute
    */
   public Date getStartDate()
   {
      return startDate;
   }

   /**
    * @jmx:managed-attribute
    */
   public String getVersion()
   {
      return version.toString();
   }

   /**
    * @jmx:managed-attribute
    */
   public String getVersionName()
   {
      return version.getName();
   }

   /**
    * @jmx:managed-attribute
    */
   public String getBuildNumber()
   {
      return version.getBuildNumber();
   }

   /**
    * @jmx:managed-attribute
    */
   public String getBuildID()
   {
      return version.getBuildID();
   }

   /**
    * @jmx:managed-attribute
    */
   public String getBuildDate()
   {
      return version.getBuildDate();
   }

// Begin NotificationBroadcaster interface methods
   public void addNotificationListener(NotificationListener listener, NotificationFilter filter, Object handback)
   {
      broadcasterSupport.addNotificationListener(listener, filter, handback);
   }

   public void removeNotificationListener(NotificationListener listener) throws ListenerNotFoundException
   {
      broadcasterSupport.removeNotificationListener(listener);
   }

   public MBeanNotificationInfo[] getNotificationInfo()
   {
      return broadcasterSupport.getNotificationInfo();
   }

   public void sendNotification(Notification notification)
   {
      broadcasterSupport.sendNotification(notification);
   }
// End NotificationBroadcaster interface methods

   ///////////////////////////////////////////////////////////////////////////
   //                             Life Thread                               //
   ///////////////////////////////////////////////////////////////////////////

   private class LifeThread
      extends Thread
   {
      Object lock = new Object();

      public void run()
      {
         synchronized (lock)
         {
            try
            {
               lock.wait();
            }
            catch (InterruptedException ignore)
            {
            }
         }
         log.info("LifeThread.run exits!");
      }
   }

   ///////////////////////////////////////////////////////////////////////////
   //                             Shutdown Hook                             //
   ///////////////////////////////////////////////////////////////////////////

   private class ShutdownHook
      extends Thread
   {
      /** The ServiceController which we will ask to shut things down with. */
      private ObjectName controller;

      /** The MainDeployer which we will ask to undeploy everything. */
      private ObjectName mainDeployer;

      private boolean forceHalt = true;

      public ShutdownHook(final ObjectName controller, final ObjectName mainDeployer)
      {
         super("JBoss Shutdown Hook");

         this.controller = controller;
         this.mainDeployer = mainDeployer;

         String value = System.getProperty("jboss.shutdown.forceHalt", null);
         if (value != null) {
            forceHalt = new Boolean(value).booleanValue();
         }
      }

      public void run()
      {
         shutdown();

         // later bitch
         if (forceHalt) {
            System.out.println("Halting VM");
            Runtime.getRuntime().halt(0);
         }
      }

      public void shutdown()
      {
         log.info("JBoss SHUTDOWN: Undeploying all packages");
         shutdownDeployments();

         log.info("Shutting down all services");
         System.out.println("Shutting down");

         // Make sure all services are down properly
         shutdownServices();

         // Make sure all mbeans are unregistered
         removeMBeans();

         log.debug("Deleting server tmp/deploy directory");
         File tmp = config.getServerTempDir();
         File tmpDeploy = new File(tmp, "deploy");
         Files.delete(tmpDeploy);

         log.info("Shutdown complete");
         System.out.println("Shutdown complete");
      }

      protected void shutdownDeployments()
      {
         try
         {
            // get the deployed objects from ServiceController
            server.invoke(mainDeployer,
                          "shutdown",
                          new Object[0],
                          new String[0]);
         }
         catch (Exception e)
         {
            Throwable t = JMXExceptionDecoder.decode(e);
            log.error("Failed to shutdown deployer", t);
         }
      }

      /**
       * The <code>shutdownServices</code> method calls the one and only
       * ServiceController to shut down all the mbeans registered with it.
       */
      protected void shutdownServices()
      {
         try
         {
            // get the deployed objects from ServiceController
            server.invoke(controller,
                          "shutdown",
                          new Object[0],
                          new String[0]);
         }
         catch (Exception e)
         {
            Throwable t = JMXExceptionDecoder.decode(e);
            log.error("Failed to shutdown services", t);
         }
      }

      /**
       * The <code>removeMBeans</code> method uses the mbean server to unregister
       * all the mbeans registered here.
       */
      protected void removeMBeans()
      {
         try
         {
            server.unregisterMBean(ServiceControllerMBean.OBJECT_NAME);
            server.unregisterMBean(ServerConfigImplMBean.OBJECT_NAME);
            server.unregisterMBean(ServerImplMBean.OBJECT_NAME);
         }
         catch (Exception e)
         {
            Throwable t = JMXExceptionDecoder.decode(e);
            log.error("Failed to unregister mbeans", t);
         }
         try
         {
            MBeanServerFactory.releaseMBeanServer(server);
         }
         catch (Exception e)
         {
            Throwable t = JMXExceptionDecoder.decode(e);
            log.error("Failed to release mbean server", t);
         }
      }
   }
}
