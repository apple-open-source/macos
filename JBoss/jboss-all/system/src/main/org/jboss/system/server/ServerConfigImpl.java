/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.system.server;

import java.util.Properties;

import java.io.File;

import java.net.URL;
import java.net.MalformedURLException;

import org.jboss.util.NestedRuntimeException;
import org.jboss.util.Null;
import org.jboss.util.MuBoolean;

/**
 * A container for the basic configuration elements required to create
 * a Server instance.
 *
 * <p>MalformedURLException are rethrown as NestedRuntimeExceptions, so that
 *    code that needs to access these values does not have to directly
 *    worry about problems with lazy construction of final URL values.
 *
 * <p>Most values are determined durring first call to getter.  All values
 *    when determined will have equivilent system properties set.
 *
 * <p>Clients are not meant to use this class directly.  Instead use
 *    {@link ServerConfigLocator} to get an instance of {@link ServerConfig}
 *    and then use it to get the server's configuration bits.
 *
 * @jmx:mbean name="jboss.system:type=ServerConfig"
 *
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @version <tt>$Revision: 1.6.2.2 $</tt>
 */
public class ServerConfigImpl
      implements ServerConfig, ServerConfigImplMBean
{
   /** The configuration properties to pull data from. */
   private Properties props;

   private File homeDir;
   private URL homeURL;
   private URL libraryURL;

   /**
    * The base URL where patch files will be loaded from. This is
    * typed as an Object to allow its value to contain Null.VALUE
    * or a URL.  If value is Null.VALUE then we have determined
    * that there is no user configuration for this value and it will
    * be passed back as null to the requesting client.
    */
   private Object patchURL;

   private String serverName;
   private File serverBaseDir;
   private File serverHomeDir;
   private File serverTempDir;
   private File serverDataDir;
   private URL serverBaseURL;
   private URL serverHomeURL;
   private URL serverLibraryURL;
   private URL serverConfigURL;

   /** Exit on shutdown flag. */
   private MuBoolean exitOnShutdown;
   private MuBoolean blockingShutdown;
   private MuBoolean requireJBossURLStreamHandlerFactory;

   private String rootDeployableFilename;

   /**
    * Construct a new <tt>ServerConfigImpl</tt> instance.
    *
    * @param props    Configuration properties.
    *
    * @throws Exception    Missing or invalid configuration.
    */
   public ServerConfigImpl(final Properties props) throws Exception
   {
      this.props = props;

      // Must have HOME_DIR
      homeDir = getFile(ServerConfig.HOME_DIR);
      if (homeDir == null)
         throw new Exception("Missing configuration value for: " + ServerConfig.HOME_DIR);
      System.setProperty(ServerConfig.HOME_DIR, homeDir.toString());
      // Setup the SERVER_HOME_DIR system property
      getServerHomeDir();
   }

   /** Breakout the initialization of URLs from the constructor as we need
    * the ServerConfig.HOME_DIR set for log setup, but we cannot create any
    * file URLs prior to the
    */
   public void initURLs()
      throws MalformedURLException
   {
      // If not set then default to homeDir
      homeURL = getURL(ServerConfig.HOME_URL);
      if (homeURL == null)
         homeURL = homeDir.toURL();
      System.setProperty(ServerConfig.HOME_URL, homeURL.toString());
   }

   /////////////////////////////////////////////////////////////////////////
   //                             Typed Access                            //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Get the local home directory which the server is running from.
    *
    * @jmx:managed-attribute
    */
   public File getHomeDir()
   {
      return homeDir;
   }

   /**
    * Get the home URL which the server is running from.
    *
    * @jmx:managed-attribute
    */
   public URL getHomeURL()
   {
      return homeURL;
   }

   /**
    * Get the home URL which the server is running from.
    *
    * @jmx:managed-attribute
    */
   public URL getLibraryURL()
   {
      if (libraryURL == null)
      {
         try
         {
            libraryURL = getURL(ServerConfig.LIBRARY_URL);
            if (libraryURL == null)
            {
               libraryURL = new URL(homeURL, ServerConfig.LIBRARY_URL_SUFFIX);
            }
            System.setProperty(ServerConfig.LIBRARY_URL, libraryURL.toString());
         }
         catch (MalformedURLException e)
         {
            throw new NestedRuntimeException(e);
         }
      }
      return libraryURL;
   }

   /**
    * Get the patch URL for the server.
    *
    * @jmx:managed-attribute
    */
   public URL getPatchURL()
   {
      if (patchURL == null)
      {
         try
         {
            patchURL = getURL(ServerConfig.PATCH_URL);
            if (patchURL == null)
            {
               patchURL = Null.VALUE;
            }
            else
            {
               System.setProperty(ServerConfig.PATCH_URL, patchURL.toString());
            }
         }
         catch (MalformedURLException e)
         {
            throw new NestedRuntimeException(e);
         }
      }

      if (patchURL == Null.VALUE)
         return null;

      return (URL) patchURL;
   }

   /**
    * Get the name of the server.
    *
    * @jmx:managed-attribute
    */
   public String getServerName()
   {
      if (serverName == null)
      {
         serverName = props.getProperty(ServerConfig.SERVER_NAME, ServerConfig.DEFAULT_SERVER_NAME);
         System.setProperty(ServerConfig.SERVER_NAME, serverName);
      }
      return serverName;
   }

   /**
    * Get the base directory for calculating server home directories.
    *
    * @jmx:managed-attribute
    */
   public File getServerBaseDir()
   {
      if (serverBaseDir == null)
      {
         serverBaseDir = getFile(ServerConfig.SERVER_BASE_DIR);
         if (serverBaseDir == null)
         {
            serverBaseDir = new File(homeDir, ServerConfig.SERVER_BASE_DIR_SUFFIX);
            System.setProperty(ServerConfig.SERVER_BASE_DIR, serverBaseDir.toString());
         }
      }
      return serverBaseDir;
   }

   /**
    * Get the server home directory.
    *
    * @jmx:managed-attribute
    */
   public File getServerHomeDir()
   {
      if (serverHomeDir == null)
      {
         serverHomeDir = getFile(ServerConfig.SERVER_HOME_DIR);
         if (serverHomeDir == null)
         {
            serverHomeDir = new File(getServerBaseDir(), getServerName());
            System.setProperty(ServerConfig.SERVER_HOME_DIR, serverHomeDir.toString());
         }
      }
      return serverHomeDir;
   }

   /**
    * Get the directory where temporary files will be stored.
    *
    * @jmx:managed-attribute
    */
   public File getServerTempDir()
   {
      if (serverTempDir == null)
      {
         serverTempDir = getFile(ServerConfig.SERVER_TEMP_DIR);
         if (serverTempDir == null)
         {
            serverTempDir = new File(getServerHomeDir(), ServerConfig.SERVER_TEMP_DIR_SUFFIX);
            System.setProperty(ServerConfig.SERVER_TEMP_DIR, serverTempDir.toString());
         }
      }
      return serverTempDir;
   }

   /**
    * Get the directory where local data will be stored.
    *
    * @jmx:managed-attribute
    */
   public File getServerDataDir()
   {
      if (serverDataDir == null)
      {
         serverDataDir = getFile(ServerConfig.SERVER_DATA_DIR);
         if (serverDataDir == null)
         {
            serverDataDir = new File(getServerHomeDir(), ServerConfig.SERVER_DATA_DIR_SUFFIX);
            System.setProperty(ServerConfig.SERVER_DATA_DIR, serverDataDir.toString());
         }
      }
      return serverDataDir;
   }

   /**
    * Get the base directory for calculating server home URLs.
    *
    * @jmx:managed-attribute
    */
   public URL getServerBaseURL()
   {
      if (serverBaseURL == null)
      {
         try
         {
            serverBaseURL = getURL(ServerConfig.SERVER_BASE_URL);
            if (serverBaseURL == null)
            {
               serverBaseURL = new URL(homeURL, ServerConfig.SERVER_BASE_URL_SUFFIX);
            }
            System.setProperty(ServerConfig.SERVER_BASE_URL, serverBaseURL.toString());
         }
         catch (MalformedURLException e)
         {
            throw new NestedRuntimeException(e);
         }
      }
      return serverBaseURL;
   }

   /**
    * Get the server home URL.
    *
    * @jmx:managed-attribute
    */
   public URL getServerHomeURL()
   {
      if (serverHomeURL == null)
      {
         try
         {
            serverHomeURL = getURL(ServerConfig.SERVER_HOME_URL);
            if (serverHomeURL == null)
            {
               serverHomeURL = new URL(getServerBaseURL(), getServerName() + "/");
            }
            System.setProperty(ServerConfig.SERVER_HOME_URL, serverHomeURL.toString());
         }
         catch (MalformedURLException e)
         {
            throw new NestedRuntimeException(e);
         }
      }
      return serverHomeURL;
   }

   /**
    * Get the server library URL.
    *
    * @jmx:managed-attribute
    */
   public URL getServerLibraryURL()
   {
      if (serverLibraryURL == null)
      {
         try
         {
            serverLibraryURL = getURL(ServerConfig.SERVER_LIBRARY_URL);
            if (serverLibraryURL == null)
            {
               serverLibraryURL = new URL(getServerHomeURL(), ServerConfig.LIBRARY_URL_SUFFIX);
            }
            System.setProperty(ServerConfig.SERVER_LIBRARY_URL, serverLibraryURL.toString());
         }
         catch (MalformedURLException e)
         {
            throw new NestedRuntimeException(e);
         }
      }
      return serverLibraryURL;
   }

   /**
    * Get the server configuration URL.
    *
    * @jmx:managed-attribute
    */
   public URL getServerConfigURL()
   {
      if (serverConfigURL == null)
      {
         try
         {
            serverConfigURL = getURL(ServerConfig.SERVER_CONFIG_URL);
            if (serverConfigURL == null)
            {
               serverConfigURL = new URL(getServerHomeURL(), ServerConfig.SERVER_CONFIG_URL_SUFFIX);
            }
            System.setProperty(ServerConfig.SERVER_CONFIG_URL, serverConfigURL.toString());
         }
         catch (MalformedURLException e)
         {
            throw new NestedRuntimeException(e);
         }
      }
      return serverConfigURL;
   }

   /**
    * Enable or disable exiting the JVM when {@link Server#shutdown} is called.
    * If enabled, then shutdown calls {@link Server#exit}.  If disabled, then
    * only the shutdown hook will be run.
    *
    * @param flag    True to enable calling exit on shutdown.
    *
    * @jmx:managed-attribute
    */
   public void setExitOnShutdown(final boolean flag)
   {
      if (exitOnShutdown == null)
      {
         exitOnShutdown = new MuBoolean(flag);
      }
      else
      {
         exitOnShutdown.set(flag);
      }
   }

   /**
    * Get the current value of the exit on shutdown flag.
    *
    * @return    The current value of the exit on shutdown flag.
    *
    * @jmx:managed-attribute
    */
   public boolean getExitOnShutdown()
   {
      if (exitOnShutdown == null)
      {
         String value = props.getProperty(ServerConfig.EXIT_ON_SHUTDOWN, null);
         if (value == null)
         {
            exitOnShutdown = new MuBoolean(ServerConfig.DEFAULT_EXIT_ON_SHUTDOWN);
         }
         else
         {
            exitOnShutdown = new MuBoolean(value);
         }
      }

      return exitOnShutdown.get();
   }

   /**
    * Enable or disable blocking when {@link Server#shutdown} is
    * called.  If enabled, then shutdown will be called in the current
    * thread.  If disabled, then the shutdown hook will be run
    * ansynchronously in a separate thread.
    *
    * @param flag    True to enable blocking shutdown.
    *
    * @jmx:managed-attribute
    */
   public void setBlockingShutdown(final boolean flag)
   {
      if (blockingShutdown == null)
      {
         blockingShutdown = new MuBoolean(flag);
      }
      else
      {
         blockingShutdown.set(flag);
      }
   }

   /**
    * Get the current value of the blocking shutdown flag.
    *
    * @return    The current value of the blocking shutdown flag.
    *
    * @jmx:managed-attribute
    */
   public boolean getBlockingShutdown()
   {
      if (blockingShutdown == null)
      {
         String value = props.getProperty(ServerConfig.BLOCKING_SHUTDOWN, null);
         if (value == null)
         {
            blockingShutdown = new MuBoolean(ServerConfig.DEFAULT_BLOCKING_SHUTDOWN);
         }
         else
         {
            blockingShutdown = new MuBoolean(value);
         }
      }

      return blockingShutdown.get();
   }


   /**
    * Set the RequireJBossURLStreamHandlerFactory flag.  if false,
    * exceptions when setting the URLStreamHandlerFactory will be
    * logged and ignored.
    *
    * @param flag    True to enable blocking shutdown.
    *
    * @jmx:managed-attribute
    */
   public void setRequireJBossURLStreamHandlerFactory(final boolean flag)
   {
      if (requireJBossURLStreamHandlerFactory == null)
      {
         requireJBossURLStreamHandlerFactory = new MuBoolean(flag);
      }
      else
      {
         requireJBossURLStreamHandlerFactory.set(flag);
      }
   }

   /**
    * Get the current value of the requireJBossURLStreamHandlerFactory flag.
    *
    * @return    The current value of the requireJBossURLStreamHandlerFactory flag.
    *
    * @jmx:managed-attribute
    */
   public boolean getRequireJBossURLStreamHandlerFactory()
   {
      if (requireJBossURLStreamHandlerFactory == null)
      {
         String value = props.getProperty(ServerConfig.REQUIRE_JBOSS_URL_STREAM_HANDLER_FACTORY, null);
         if (value == null)
         {
            requireJBossURLStreamHandlerFactory = new MuBoolean(ServerConfig.DEFAULT_REQUIRE_JBOSS_URL_STREAM_HANDLER_FACTORY);
         }
         else
         {
            requireJBossURLStreamHandlerFactory = new MuBoolean(value);
         }
      }

      return requireJBossURLStreamHandlerFactory.get();
   }

   /**
    * Set the filename of the root deployable that will be used to finalize
    * the bootstrap process.
    *
    * @param filename    The filename of the root deployable.
    *
    * @jmx:managed-attribute
    */
   public void setRootDeploymentFilename(final String filename)
   {
      this.rootDeployableFilename = filename;
   }

   /**
    * Get the filename of the root deployable that will be used to finalize
    * the bootstrap process.
    *
    * @return    The filename of the root deployable.
    *
    * @jmx:managed-attribute
    */
   public String getRootDeploymentFilename()
   {
      if (rootDeployableFilename == null)
      {
         rootDeployableFilename = props.getProperty(ServerConfig.ROOT_DEPLOYMENT_FILENAME,
               ServerConfig.DEFAULT_ROOT_DEPLOYMENT_FILENAME);
      }

      return rootDeployableFilename;
   }

   /**
    * Get a URL from configuration.
    */
   private URL getURL(final String name) throws MalformedURLException
   {
      String value = props.getProperty(name, null);
      if (value != null)
      {
         if (!value.endsWith("/")) value += "/";
         return new URL(value);
      }

      return null;
   }

   /**
    * Get a File from configuration.
    */
   private File getFile(final String name)
   {
      String value = props.getProperty(name, null);
      if (value != null)
      {
         return new File(value);
      }

      return null;
   }
}
