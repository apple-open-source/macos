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
import java.util.StringTokenizer;
import java.util.List;
import java.util.LinkedList;

import java.net.URL;
import java.net.URLClassLoader;
import java.net.MalformedURLException;

/**
 * A helper class to load a JBoss server instance.
 *
 * <p>Basic usage is something like this:
 * <pre>
 *    // setup the basic server config properties
 *    Properties props = new Properties(System.getProperties());
 *    props.put(ServerConfig.SERVER_LIBRARY_URL, "http://myserver.com/myjboss/lib/");
 *    // set some more properties
 *
 *    // create a new loader to do the dirty work
 *    ServerLoader loader = new ServerLoader(props);
 *
 *    // add the jaxp & jmx library to use
 *    loader.addLibrary("crimson.jar");
 *    loader.addLibrary("jboss-jmx-core.jar");
 *
 *    // load and initialize the server instance
 *    ClassLoader parent = Thread.currentThread().getContextClassLoader();
 *    Server server = loader.load(parent);
 *    server.init(props);
 *
 *    // start up the server
 *    server.start();
 *
 *    // go make some coffee, drink a beer or play GTA3
 *    // ...
 *
 *    // shutdown and go to sleep
 *    server.shutdown();
 * </pre>
 * @version <tt>$Revision: 1.8.2.5 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author <a href="mailto:adrian.brock@happeningtimes.com">Adrian Brock</a>
 * @author Scott.Stark@jboss.org
 */
public class ServerLoader
{
   /**
    * The default list of boot libraries.  Does not include
    * the JAXP or JMX impl, users of this class should add the
    * proper libraries.
    */
   public static final String DEFAULT_BOOT_LIBRARY_LIST =
      "jaxp.jar,log4j-boot.jar,jboss-common.jar,jboss-system.jar";

   /** The default server type. */
   public static final String DEFAULT_SERVER_TYPE = "org.jboss.system.server.ServerImpl";

   /**
    * Configuration properties.
    */
   protected Properties props;

   /**
    * The URL where libraries are read from.
    */
   protected URL libraryURL;

   /**
    * A list of extra URLs to add to the classpath when loading
    * the server.
    */
   protected List extraClasspath = new LinkedList();

   /**
    * Construct a <tt>ServerLoader</tt>.
    *
    * @param props    Configuration properties.
    *
    * @throws Exception    Invalid configuration
    */
   public ServerLoader(final Properties props) throws Exception
   {
      if (props == null)
         throw new IllegalArgumentException("props is null");

      this.props = props;

      // must have HOME_URL, or we can't continue
      URL homeURL = getURL(ServerConfig.HOME_URL);
      if (homeURL == null)
      {
         throw new Exception("Missing configuration value for: "
            + ServerConfig.HOME_URL);
      }

      libraryURL = getURL(ServerConfig.LIBRARY_URL);
      if (libraryURL == null)
      {
         // need libraray url to make boot urls list
         libraryURL = new URL(homeURL, ServerConfig.LIBRARY_URL_SUFFIX);
      }

      // If the home URL begins with http add the webav and httpclient jars
      if( homeURL.getProtocol().startsWith("http") == true )
      {
         this.addLibrary("webdavlib.jar");
         this.addLibrary("commons-httpclient.jar");
         this.addLibrary("commons-logging.jar");
      }
   }

   /**
    * Add an extra library to the end of list of libraries
    * which will be loaded from the library URL when loading
    * the Server class.
    *
    * @param filename   A filename (no directory parts)
    *
    * @throws MalformedURLException   Could not generate URL from library URL + filename
    */
   public void addLibrary(final String filename) throws MalformedURLException
   {
      if (filename == null)
         throw new IllegalArgumentException("filename is null");

      URL jarURL = new URL(libraryURL, filename);
      extraClasspath.add(jarURL);
   }

   /**
    * Add a list of comma seperated library file names.
    *
    * @param filenames   A list of comma seperated filenames (with no directory parts)
    *
    * @throws MalformedURLException   Could not generate URL from library URL + filename
    */
   public void addLibraries(final String filenames) throws MalformedURLException
   {
      if (filenames == null)
         throw new IllegalArgumentException("filenames is null");

      StringTokenizer stok = new StringTokenizer(filenames, ",");
      while (stok.hasMoreElements())
      {
         addLibrary(stok.nextToken().trim());
      }
   }

   /**
    * Add an extra URL to the classpath used to load the server.
    *
    * @param url    A URL to add to the classpath.
    */
   public void addURL(final URL url)
   {
      if (url == null)
         throw new IllegalArgumentException("url is null");

      extraClasspath.add(url);
   }

   /**
    * Get a URL from configuration or system properties.
    */
   protected URL getURL(final String name) throws MalformedURLException
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
    * Retruns an array of URLs which will be used to load the
    * core system and construct a new Server object instance.
    */
   protected URL[] getBootClasspath() throws MalformedURLException
   {
      List list = new LinkedList();

      // prepend users classpath to allow for overrides
      list.addAll(extraClasspath);

      String value = props.getProperty(ServerConfig.BOOT_LIBRARY_LIST, DEFAULT_BOOT_LIBRARY_LIST);

      StringTokenizer stok = new StringTokenizer(value, ",");
      while (stok.hasMoreElements())
      {
         URL url = new URL(libraryURL, stok.nextToken().trim());
         list.add(url);
      }

      return (URL[]) list.toArray(new URL[list.size()]);
   }

   /**
    * Load a {@link Server} instance.
    *
    * @parent    The parent of any class loader created during boot.
    * @return    An uninitialized (and unstarted) Server instance.
    *
    * @throws Exception   Failed to load or create Server instance.
    */
   public Server load(final ClassLoader parent) throws Exception
   {
      Server server;
      ClassLoader oldCL = Thread.currentThread().getContextClassLoader();

      try
      {
         // get the boot lib list
         URL[] urls = getBootClasspath();
         URLClassLoader classLoader = new NoAnnotationURLClassLoader(urls, parent);
         Thread.currentThread().setContextClassLoader(classLoader);

         // construct a new Server instance
         String typename = props.getProperty(ServerConfig.SERVER_TYPE, DEFAULT_SERVER_TYPE);
         server = createServer(typename, classLoader);
      }
      finally
      {
         Thread.currentThread().setContextClassLoader(oldCL);
      }

      // thats all folks, have fun
      return server;
   }

   /**
    * Construct a new instance of Server, loading all required classes from
    * the given ClossLoader.
    */
   protected Server createServer(final String typename, final ClassLoader classLoader) throws Exception
   {
      // load the class first
      Class type = classLoader.loadClass(typename);

      // and then create a new instance
      Server server = (Server) type.newInstance();

      // here ya go
      return server;
   }
}
