/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.system.server;

import java.net.URL;
import java.net.URLClassLoader;
import java.net.URLStreamHandlerFactory;

/**
 * A URL classloader to avoid URL annotation of classes in RMI
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public class NoAnnotationURLClassLoader
   extends URLClassLoader
{
   /** The value returned by {@link getURLs}. */
   private static final URL[] EMPTY_URL_ARRAY = {};

   /**
    * Construct a <tt>URLClassLoader</tt>
    *
    * @param urls   the URLs to load classes from.
    */
   public NoAnnotationURLClassLoader(URL[] urls)
   {
      super(urls);
   }

   /**
    * Construct a <tt>URLClassLoader</tt>
    *
    * @param urls   the URLs to load classes from.
    * @param parent the parent classloader.
    */
   public NoAnnotationURLClassLoader(URL[] urls, ClassLoader parent)
   {
      super(urls, parent);
   }

   /**
    * Construct a <tt>URLClassLoader</tt>
    *
    * @param urls    the URLs to load classes from.
    * @param parent  the parent classloader.
    * @param factory the url stream factory.
    */
   public NoAnnotationURLClassLoader(URL[] urls, ClassLoader parent,
                                     URLStreamHandlerFactory factory)
   {
      super(urls, parent, factory);
   }

   /**
   * Return all library URLs
   *
   * <p>Do not remove this method without running the WebIntegrationTestSuite
   */
   public URL[] getAllURLs()
   {
      return super.getURLs();
   }

   /**
   * Return an empty URL array to force the RMI marshalling subsystem to
   * use the <tt>java.server.codebase</tt> property as the annotated codebase.
   *
   * <p>Do not remove this method without discussing it on the dev list.
   *
   * @return Empty URL[]
   */
   public URL[] getURLs()
   {
      return EMPTY_URL_ARRAY;
   }
}
