/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.logging;

import java.io.PrintStream;
import java.io.IOException;

import java.net.URL;
import java.net.URLConnection;
import java.net.MalformedURLException;

import java.util.Timer;
import java.util.TimerTask;
import java.util.StringTokenizer;

import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.MalformedObjectNameException;
   
import org.apache.log4j.Level;
import org.apache.log4j.PropertyConfigurator;
import org.apache.log4j.helpers.LogLog;
import org.apache.log4j.xml.DOMConfigurator;

import org.jboss.logging.util.LoggerStream;
import org.jboss.logging.util.OnlyOnceErrorHandler;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.util.ThrowableHandler;
import org.jboss.util.ThrowableListener;
import org.jboss.util.Strings;
import org.jboss.util.stream.Streams;

/**
 * Initializes the Log4j logging framework.  Supports XML and standard
 * configuration file formats.  Defaults to using 'log4j.xml' read
 * from a system resource.
 *
 * <p>Sets up a {@link ThrowableListener} to adapt unhandled
 *    throwables to a logger.
 *
 * <p>Installs {@link LoggerStream} adapters for <tt>System.out</tt> and 
 *    <tt>System.err</tt> to catch and redirect calls to Log4j.
 *
 * @jmx:mbean name="jboss.system:type=Log4jService,service=Logging"
 *            extends="org.jboss.system.ServiceMBean"
 *
 * @version <tt>$Revision: 1.25.2.3 $</tt>
 * @author <a href="mailto:phox@galactica.it">Fulco Muriglio</a>
 * @author <a href="mailto:Scott_Stark@displayscape.com">Scott Stark</a>
 * @author <a href="mailto:davidjencks@earthlink.net">David Jencks</a>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class Log4jService
   extends ServiceMBeanSupport
   implements Log4jServiceMBean
{
   /**
    * The default url for the configuration file.  Reads the value
    * from the system property <tt>org.jboss.logging.Log4jService.configURL</tt>
    * or if that is not set defaults to <tt>resource:log4j.xml</tt>.
    */
   public static final String DEFAULT_URL =
      System.getProperty(Log4jService.class.getName() + ".configURL", "resource:log4j.xml");

   /**
    * Default flag to enable/disable cacthing System.out.  Reads the value
    * from the system property <tt>org.jboss.logging.Log4jService.catchSystemOut</tt>
    * or if not set defaults to <tt>true</tt>.
    */
   public static final boolean CATCH_SYSTEM_OUT =      
      getBoolean(Log4jService.class.getName() + ".catchSystemOut", true);

   /**
    * Default flag to enable/disable cacthing System.err.  Reads the value
    * from the system property <tt>org.jboss.logging.Log4jService.catchSystemErr</tt>
    * or if not set defaults to <tt>true</tt>.
    */
   public static final boolean CATCH_SYSTEM_ERR =
      getBoolean(Log4jService.class.getName() + ".catchSystemErr", true);

   /** Helper to get boolean value from system property or use default if not set. */
   private static boolean getBoolean(String name, boolean defaultValue)
   {
      String value = System.getProperty(name, null);
      if (value == null)
         return defaultValue;
      return new Boolean(value).booleanValue();
   }
   
   /** The URL to the configuration file. */
   private URL configURL;

   /** The time in seconds between checking for new config. */
   private int refreshPeriod;
   
   private ThrowableListenerLoggingAdapter throwableAdapter;

   /** The previous value of System.out. */
   private PrintStream out;

   /** The previous value of System.err. */
   private PrintStream err;

   /**
    * Flag to enable/disable adapting <tt>System.out</tt> to the
    * <tt>STDOUT</tt> logger.
    */   
   private boolean catchSystemOut = CATCH_SYSTEM_OUT;

   /**
    * Flag to enable/disable adapting <tt>System.out</tt> to the
    * <tt>STDERR</tt> logger.
    */   
   private boolean catchSystemErr = CATCH_SYSTEM_ERR;

   /** The org.apache.log4j.helpers.LogLog.setQuietMode flag setting */
   private boolean log4jQuietMode = true;

   /** The URL watch timer (in daemon mode). */
   private Timer timer = new Timer(true);

   /** The specialized timer task to watch our config file. */
   private URLWatchTimerTask timerTask;

   /**
    * A flag to enable start/stop to work as expected,
    * but still use create to init early.
    */
   private boolean initialized;
  
   /**
    * Uses defaults.
    *
    * @jmx:managed-constructor
    *
    * @throws MalformedURLException    Could not create URL from default (propbably
    *                                  a problem with overridden properties).
    */
   public Log4jService() throws MalformedURLException
   {
      this(DEFAULT_URL, 60);
   }
   
   /**
    * @jmx:managed-constructor
    * 
    * @param url    The configuration URL.
    */
   public Log4jService(final URL url)
   {
      this(url, 60);
   }

   /**
    * @jmx:managed-constructor
    * 
    * @param url    The configuration URL.
    */
   public Log4jService(final String url) throws MalformedURLException
   {
      this(Strings.toURL(url), 60);
   }
   
   /**
    * @jmx:managed-constructor
    * 
    * @param url              The configuration URL.
    * @param refreshPeriod    The refreshPeriod in seconds to wait between each check.
    */
   public Log4jService(final String url, final int refreshPeriod)
      throws MalformedURLException
   {
      this(Strings.toURL(url), refreshPeriod);
   }

   /**
    * @jmx:managed-constructor
    * 
    * @param url              The configuration URL.
    * @param refreshPeriod    The refreshPeriod in seconds to wait between each check.
    */
   public Log4jService(final URL url, final int refreshPeriod)
   {
      this.configURL = url;
      this.refreshPeriod = refreshPeriod;
   }

   /**
    * Set the catch <tt>System.out</tt> flag.
    *
    * @jmx:managed-attribute
    *
    * @param flag    True to enable, false to disable.
    */
   public void setCatchSystemOut(final boolean flag)
   {
      this.catchSystemOut = flag;
   }

   /**
    * Get the catch <tt>System.out</tt> flag.
    *
    * @jmx:managed-attribute
    *
    * @return  True if enabled, false if disabled.
    */
   public boolean getCatchSystemOut()
   {
      return catchSystemOut;
   }

   /**
    * Set the catch <tt>System.err</tt> flag.
    *
    * @jmx:managed-attribute
    *
    * @param flag    True to enable, false to disable.
    */
   public void setCatchSystemErr(final boolean flag)
   {
      this.catchSystemErr = flag;
   }

   /**
    * Get the catch <tt>System.err</tt> flag.
    *
    * @jmx:managed-attribute
    *
    * @return  True if enabled, false if disabled.
    */
   public boolean getCatchSystemErr()
   {
      return catchSystemErr;
   }

   /**
    * Get the org.apache.log4j.helpers.LogLog.setQuietMode flag
    *
    * @jmx:managed-attribute
    *
    * @return  True if enabled, false if disabled.
    */
   public boolean getLog4jQuietMode()
   {
      return log4jQuietMode;
   }
   /**
    * Set the org.apache.log4j.helpers.LogLog.setQuietMode flag
    *
    * @jmx:managed-attribute
    *
    * @return  True if enabled, false if disabled.
    */
   public void setLog4jQuietMode(boolean flag)
   {
      this.log4jQuietMode = flag;
   }

   /**
    * Get the refresh period.
    *
    * @jmx:managed-attribute
    */
   public int getRefreshPeriod()
   {
      return refreshPeriod;
   }

   /**
    * Set the refresh period.
    *
    * @jmx:managed-attribute
    */
   public void setRefreshPeriod(final int refreshPeriod)
   {
      this.refreshPeriod = refreshPeriod;
   }
   
   /**
    * Get the Log4j configuration URL.
    *
    * @jmx:managed-attribute
    */
   public URL getConfigurationURL()
   {
      return configURL;
   }
   
   /**
    * Set the Log4j configuration URL.
    *
    * @jmx:managed-attribute
    */
   public void setConfigurationURL(final URL url)
   {
      this.configURL = url;
   }

   /**
    * Sets the level for a logger of the give name.
    *
    * <p>Values are trimmed before used.
    *
    * @jmx:managed-operation
    *
    * @param name        The name of the logger to change level
    * @param levelName   The name of the level to change the logger to.
    */
   public void setLoggerLevel(final String name, final String levelName)
   {
      org.apache.log4j.Logger logger = org.apache.log4j.Logger.getLogger(name.trim());
      Level level = XLevel.toLevel(levelName.trim());

      logger.setLevel(level);
      log.info("Level set to " + level + " for " + name);
   }

   /**
    * Sets the levels of each logger specified by the given comma
    * seperated list of logger names.
    *
    * @jmx:managed-operation
    *
    * @see #setLoggerLevel
    *
    * @param list        A comma seperated list of logger names.
    * @param levelName   The name of the level to change the logger to.
    */
   public void setLoggerLevels(final String list, final String levelName)
   {
      StringTokenizer stok = new StringTokenizer(list, ",");
      
      while (stok.hasMoreTokens()) {
         String name = stok.nextToken();
         setLoggerLevel(name, levelName);
      }
   }
   
   /**
    * Gets the level of the logger of the give name.
    *
    * @jmx:managed-operation
    *
    * @param name       The name of the logger to inspect.
    */
   public String getLoggerLevel(final String name)
   {
      org.apache.log4j.Logger logger = org.apache.log4j.Logger.getLogger(name);
      Level level = logger.getLevel();

      if (level != null) 
         return level.toString();

      return null;
   }

   /**
    * Force the logging system to reconfigure.
    *
    * @jmx:managed-operation
    */
   public void reconfigure() throws IOException
   {
      if (timerTask == null)
         throw new IllegalStateException("Service stopped or not started");
      
      timerTask.reconfigure();
   }

   /**
    * Hack to reconfigure and change the URL.  This is needed until we
    * have a JMX HTML Adapter that can use PropertyEditor to coerce.
    *
    * @jmx:managed-operation
    *
    * @param url   The new configuration url
    */
   public void reconfigure(final String url) throws IOException, MalformedURLException
   {
      setConfigurationURL(Strings.toURL(url));
      reconfigure();
   }
   
   private void installSystemAdapters()
   {
      org.apache.log4j.Logger logger;

      // Install catchers
      if (catchSystemOut)
      {
         logger = org.apache.log4j.Logger.getLogger("STDOUT");
         out = System.out;
         System.setOut(new LoggerStream(logger, Level.INFO, out));
         log.debug("Installed System.out adapter");
      }
      
      if (catchSystemErr)
      {
         logger = org.apache.log4j.Logger.getLogger("STDERR");
         err = System.err;
         OnlyOnceErrorHandler.setOutput(err);
         System.setErr(new LoggerStream(logger, Level.ERROR, err));
         log.debug("Installed System.err adapter");
      }
   }

   private void uninstallSystemAdapters()
   {
      // Remove System adapters
      if (out != null)
      {
         System.out.flush();
         System.setOut(out);
         log.debug("Removed System.out adapter");
         out = null;
      }
      
      if (err != null)
      {
         System.err.flush();
         System.setErr(err);
         log.debug("Removed System.err adapter");
         err = null;
      }
   }


   ///////////////////////////////////////////////////////////////////////////
   //                       Concrete Service Overrides                      //
   ///////////////////////////////////////////////////////////////////////////

   protected ObjectName getObjectName(MBeanServer server, ObjectName name)
      throws MalformedObjectNameException
   {
      return name == null ? OBJECT_NAME : name;
   }

   private void setup() throws Exception
   {
      if (initialized) return;
      
      timerTask = new URLWatchTimerTask();
      timerTask.run();
      timer.schedule(timerTask, 1000 * refreshPeriod, 1000 * refreshPeriod);

      // Make sure the root Logger has loaded
      org.apache.log4j.Logger.getRootLogger();
      
      // Install listener for unhandled throwables to turn them into log messages
      throwableAdapter = new ThrowableListenerLoggingAdapter();
      ThrowableHandler.addThrowableListener(throwableAdapter);
      log.debug("Added ThrowableListener: " + throwableAdapter);

      initialized = true;
   }

   protected void createService() throws Exception
   {
      setup();
   }

   protected void startService() throws Exception
   {
      setup();
   }
   
   protected void stopService() throws Exception
   {
      timerTask.cancel();
      timerTask = null;
      
      // Remove throwable adapter
      ThrowableHandler.removeThrowableListener(throwableAdapter);
      throwableAdapter = null;

      uninstallSystemAdapters();

      // allow start to re-initalize
      initialized = false;
   }


   ///////////////////////////////////////////////////////////////////////////
   //                       ThrowableListener Adapter                      //
   ///////////////////////////////////////////////////////////////////////////
   
   /**
    * Adapts ThrowableHandler to the Loggger interface.  Using nested 
    * class instead of anoynmous class for better logger naming.
    */
   private static class ThrowableListenerLoggingAdapter
      implements ThrowableListener
   {
      private Logger log = Logger.getLogger(ThrowableListenerLoggingAdapter.class);
      
      public void onThrowable(int type, Throwable t)
      {
         switch (type)
         {
             default:
                // if type is not valid then make it any error
             
             case ThrowableHandler.Type.ERROR:
                log.error("Unhandled Throwable", t);
                break;
             
             case ThrowableHandler.Type.WARNING:
                log.warn("Unhandled Throwable", t);
                break;
             
             case ThrowableHandler.Type.UNKNOWN:
                // these could be red-herrings, so log them as trace
                log.trace("Ynhandled Throwable; status is unknown", t);
                break;
         }
      }
   }


   ///////////////////////////////////////////////////////////////////////////
   //                         URL Watching Timer Task                       //
   ///////////////////////////////////////////////////////////////////////////
   
   /**
    * A timer task to check when a URL changes (based on 
    * last modified time) and reconfigure Log4j.
    */
   private class URLWatchTimerTask
      extends TimerTask
   {
      private Logger log = Logger.getLogger(URLWatchTimerTask.class);

      private long lastConfigured = -1;

      public void run()
      {
         log.trace("Checking if configuration changed");

         boolean trace = log.isTraceEnabled();
         
         try
         {
            URLConnection conn = configURL.openConnection();
            if (trace)
               log.trace("connection: " + conn);

            long lastModified = conn.getLastModified();
            if (trace)
            {
               log.trace("last configured: " + lastConfigured);
               log.trace("last modified: " + lastModified);
            }

            if (lastConfigured < lastModified)
            {
               reconfigure(conn);
            }
         }
         catch (Exception e)
         {
            log.warn("Failed to check URL: " + configURL, e);
         }
      }

      public void reconfigure() throws IOException
      {
         URLConnection conn = configURL.openConnection();
         reconfigure(conn);
      }

      private void reconfigure(final URLConnection conn) 
      {
         log.info("Configuring from URL: " + configURL);
         
         boolean xml = false;
         boolean trace = log.isTraceEnabled();
         
         // check if the url is xml
         String contentType = conn.getContentType();
         if (trace)
            log.trace("content type: " + contentType);
         
         if (contentType == null)
         {
            String filename = configURL.getFile().toLowerCase();
            if (trace) log.trace("filename: " + filename);

            xml = filename.endsWith(".xml");
         }
         else
         {
            xml = contentType.equalsIgnoreCase("text/xml");
            xml |= contentType.equalsIgnoreCase("application/xml");
         }
         if (trace)
            log.trace("reconfiguring; xml=" + xml);

         // Dump our config if trace is enabled
         if (trace)
         {
            try
            {
               java.io.InputStream is = conn.getInputStream();
               Streams.copy(is, System.out);
            }
            catch (Exception e)
            {
               log.error("Failed to dump config", e);
            }
         }

         // need to uninstall adapters to avoid problems
         uninstallSystemAdapters();

         if (xml)
         {
            DOMConfigurator.configure(configURL);
         }
         else
         {
            PropertyConfigurator.configure(configURL);
         }

         /* Set the LogLog.QuietMode. As of log4j1.2.8 this needs to be set to
         avoid deadlock on exception at the appender level. See bug#696819.
         */
         LogLog.setQuietMode(log4jQuietMode);

         // but make sure they get reinstalled again
         installSystemAdapters();

         // and then remeber when we were last changed
         lastConfigured = System.currentTimeMillis();
      }
   }
}
