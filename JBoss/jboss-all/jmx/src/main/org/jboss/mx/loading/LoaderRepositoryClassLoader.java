/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.mx.loading;

import java.net.URLStreamHandlerFactory;

import org.jboss.util.loading.DelegatingClassLoader;

/**
 * A delegating classloader that first peeks in the loader
 * repository's cache.
 *
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public class LoaderRepositoryClassLoader
   extends DelegatingClassLoader
{
   /** The loader repository */
   protected LoaderRepository repository;

   /**
    * Constructor
    *
    * @param parent the parent classloader, cannot be null.
    * @param repository the loader repository, cannot be null.
    */
   public LoaderRepositoryClassLoader(ClassLoader parent, LoaderRepository repository)
   {
      super(parent);
      if (repository == null)
         throw new IllegalArgumentException("No repository");
      this.repository = repository;
   }

   /**
    * Constructor
    *
    * @param parent, the parent classloader, cannot be null.
    * @param factory the url stream factory.
    */
   public LoaderRepositoryClassLoader(ClassLoader parent, LoaderRepository repository, URLStreamHandlerFactory factory)
   {
      super(parent);
      if (repository == null)
         throw new IllegalArgumentException("No repository");
      this.repository = repository;
   }

   /**
    * Load a class, first peek in the loader repository cache then
    * ask the parent.
    *
    * @param className the class name to load
    * @param resolve whether to link the class
    * @return the loaded class
    * @throws ClassNotFoundException when the class could not be found
    */
   protected Class loadClass(String className, boolean resolve)
      throws ClassNotFoundException
   {
      Class clazz = repository.getCachedClass(className);
      if (clazz != null)
      {
         if (resolve)
            resolveClass(clazz);
         return clazz;
      }

      // Delegate
      return super.loadClass(className, resolve);
   }
}
