/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.net.protocol;

import java.net.URL;
import java.net.URLStreamHandler;
import java.util.Collections;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.StringTokenizer;

import org.jboss.logging.Logger;

/**
 * A factory for loading URL protocol handlers.  This is based
 * on Sun's URL mechanism, in that <tt>Handler</tt> classes will be
 * searched for in the packages specified by the java.protocol.handler.pkgs
 * property are searched for classes matching the protocol + ".Handler"
 * classname. The default JBoss package "org.jboss.net.protocol" is searched
 * even if not specified in the java.protocol.handler.pkgs property.
 *
 * <p>This factory is installed by the default server implementaion
 * to ensure that protocol handlers not in the system classpath are
 * located. The thread context class is used first followed by the
 * Class.forName method.
 * </p>
 *
 * <p>Use {@link preload} to force the URL handler map to load the
 *    handlers for each protocol listed in {@link #PROTOCOLS}.
 *
 * @version <tt>$Revision: 1.4.2.3 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author Scott.Stark@jboss.org
 */
public class URLStreamHandlerFactory
   implements java.net.URLStreamHandlerFactory
{
   /** Class logger. */
   private static final Logger log = Logger.getLogger(URLStreamHandlerFactory.class);
   
   /** The package prefix where JBoss protocol handlers live. */
   public static final String PACKAGE_PREFIX = "org.jboss.net.protocol";
   /** A map of protocol names to handlers. Since there can only be one
    URLStreamHandlerFactory installed, this is a static map that may be
    cleared.
    */
   private static Map handlerMap = Collections.synchronizedMap(new HashMap());

   /** This thread local is used to prevent recursion in the
    * createURLStreamHandler method. Resolving the protocol handler
    * class can end up creating a new URL which can loop back into
    * this factory with a stack like:
    * <pre>
      URLStreamHandlerFactory that use the TCL. See bug#669043
      createURLStreamHandler():146, URLStreamHandlerFactory.java
      getURLStreamHandler():1057, URL.java
      <init>():405, URL.java
      <init>():329, URL.java
      <init>():321, URL.java
      <init>():540, URLClassPath.java
      run():319, URLClassPath.java
      doPrivileged():-1, AccessController.java
      getLoader():308, URLClassPath.java
      getLoader():285, URLClassPath.java
      findResource():136, URLClassPath.java
      run():351, URLClassLoader.java
      doPrivileged():-1, AccessController.java
      findResource():348, URLClassLoader.java
      getResource():780, ClassLoader.java
      getResourceLocally():250, UnifiedClassLoader.java
      getResourceFromClassLoader():333, UnifiedLoaderRepository3.java
      getResource():243, UnifiedLoaderRepository3.java
      getResource():228, UnifiedClassLoader3.java
     </pre>
    So we detect recursion based on the protocol value matches the current
    createURLStreamHandlerProtocol setting.
   */
   private static ThreadLocal createURLStreamHandlerProtocol = new ThreadLocal();

   /** The current packages prefixes determined from the java.protocol.handler.pkgs
    property + the org.jboss.net.protocol default package.
    */
   private String[] handlerPkgs = {PACKAGE_PREFIX};
   /** The last java.protocol.handler.pkgs value. Used to determine if the
    java.protocol.handler.pkgs property has changed since handlerPkgs was
    last built.
    */
   private String lastHandlerPkgs = PACKAGE_PREFIX;

   /** A list of JBoss specific protocols for preloading. */
   public static final String PROTOCOLS[] = {
      "resource",
      "file"
   };

   /**
    * Preload the JBoss specific protocol handlers, so that URL knows about
    * them even if the handler factory is changed.
    */
   public static void preload()
   {
      for (int i = 0; i < PROTOCOLS.length; i ++)
      {
         try
         {
            URL url = new URL(PROTOCOLS[i], "", -1, "");
            log.trace("Loaded protocol: " + PROTOCOLS[i]);
         }
         catch (Exception e)
         {
            log.warn("Failed to load protocol: " + PROTOCOLS[i], e);
         }
      }
   }

   /** Clear the current protocol to handler map. The map will be rebuilt
    as protocol handlers are requested.
    */
   public static void clear()
   {
      handlerMap.clear();
   }

   /** Search the handlerPkgs for URLStreamHandler classes matching the
    * pkg + protocol + ".Handler" naming convention.
    *
    * @see #checkHandlerPkgs()
    * @param protocol The protocol to create a stream handler for
    * @return The protocol handler or null if not found
    */
   public URLStreamHandler createURLStreamHandler(final String protocol)
   {
      // Check the handler map
      URLStreamHandler handler = (URLStreamHandler) handlerMap.get(protocol);
      if( handler != null )
         return handler;

      // Validate that createURLStreamHandler is not recursing
      String prevProtocol = (String) createURLStreamHandlerProtocol.get();
      if( prevProtocol != null && prevProtocol.equals(protocol) )
         return null;
      createURLStreamHandlerProtocol.set(protocol);

      // See if the handler pkgs definition has changed
      checkHandlerPkgs();

      // Search the handlerPkgs for a matching protocol handler
      ClassLoader ctxLoader = Thread.currentThread().getContextClassLoader();
      for(int p = 0; p < handlerPkgs.length; p ++)
      {
         try
         {
            // Form the standard protocol handler class name
            String classname = handlerPkgs[p] + "." + protocol + ".Handler";
            Class type = null;

            try
            {
               type = ctxLoader.loadClass(classname);
            }
            catch(ClassNotFoundException e)
            {
               // Try our class loader
               type = Class.forName(classname);
            }

            if( type != null )
            {
               handler = (URLStreamHandler) type.newInstance();
               handlerMap.put(protocol, handler);
               log.trace("Found protocol:"+protocol+" handler:"+handler);
            }
         }
         catch (Throwable ignore)
         {
         }
      }

      createURLStreamHandlerProtocol.set(null);
      return handler;
   }

   /** See if the java.protocol.handler.pkgs system property has changed
    and if it has, parse it to update the handlerPkgs array.
    */
   private synchronized void checkHandlerPkgs()
   {
      String handlerPkgsProp = System.getProperty("java.protocol.handler.pkgs");
      if( handlerPkgsProp != null && handlerPkgsProp.equals(lastHandlerPkgs) == false )
      {
         // Update the handlerPkgs[] from the handlerPkgsProp
         StringTokenizer tokeninzer = new StringTokenizer(handlerPkgsProp, "|");
         ArrayList tmp = new ArrayList();
         while( tokeninzer.hasMoreTokens() )
         {
            String pkg = tokeninzer.nextToken().intern();
            if( tmp.contains(pkg) == false )
               tmp.add(pkg);
         }
         // Include the JBoss default protocol handler pkg
         if( tmp.contains(PACKAGE_PREFIX) == false )
            tmp.add(PACKAGE_PREFIX);
         handlerPkgs = new String[tmp.size()];
         tmp.toArray(handlerPkgs);
         lastHandlerPkgs = handlerPkgsProp;
      }
   }

}
