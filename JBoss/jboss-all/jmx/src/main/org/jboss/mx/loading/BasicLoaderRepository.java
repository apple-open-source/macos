/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.loading;

import java.net.URL;
import java.util.Enumeration;
import java.util.Iterator;
import java.util.List;

/**
 * Implements a simple classloader repository for the MBean server. The basic
 * loader repository uses an unordered list of classloaders to try and load
 * the required class. There is no attempt made to resolve conflicts between
 * classes loaded by different classloaders. <p>
 *
 * A thread's context class loader is always searched first. Context class loader
 * is not required to be registered to the repository.
 *
 * @see org.jboss.mx.loading.LoaderRepository
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.6.4.3 $
 */   
public class BasicLoaderRepository 
   extends LoaderRepository
{

   // Public --------------------------------------------------------
   /**
    * Attempts to load a class using the first found instance of a <tt>ClassLoader</tt>
    * with a given class description.
    *
    * @param   className   fully qualified name of the class to load
    * @return  loaded class instance
    * @throws  ClassNotFoundException if the class was not found by any of the
    *          registered class loaders
    */
   public Class loadClass(String className) throws ClassNotFoundException
   {
      return loadClassWithout(null, className);
   }

   /**
    * Attempts to load a class using the first found instance of a <tt>ClassLoader</tt>
    * with a given class description. If a non-null reference is passed as a
    * <tt>skipLoader</tt> parameter then this class loader will not be used to
    * load the requested class from the repository. <p>
    *
    * @param   skipLoader  this class loader instance will be skipped when attempting
    *                      to load the given class.
    * @param   className   fully qualified name of the class to load
    * @return  loaded class instance
    * @throws  ClassNotFoundException if the class was not found by any of the
    *          registered class loaders
    */
   public Class loadClassWithout(ClassLoader skipLoader, String className)
      throws ClassNotFoundException
   {
   
      // REVIEW: Is this the correct place for this or should it only
      //         go where signatures are checked?  
      // JPL:    looks ok to me
      // JPL:    however there is not guarantee that the BasicLR is installed
      //         as the loader repository for all server instances.
      
      // Check for native classes
      Class clazz = (Class) nativeClassBySignature.get(className);
      if (clazz != null)
        return clazz;
      
      // try ctx cl first
      ClassLoader ctxLoader = Thread.currentThread().getContextClassLoader();

      if (ctxLoader != skipLoader)
      {
         try
         {
            clazz = ctxLoader.loadClass(className);
         }
         catch (ClassNotFoundException e)
         {
            // ignore and move on to the loader list
         }
      }

      if (clazz != null)
         return clazz;

      Iterator it = loaders.iterator();
      while (it.hasNext())
      {
         ClassLoader cl = (ClassLoader)it.next();

         if (cl != skipLoader)
         {
            try
            {
               clazz = cl.loadClass(className);
               return clazz;
            }
            catch (ClassNotFoundException ignored)
            {
               // go on and try the next loader
            }
         }
      }

      throw new ClassNotFoundException(className);
   }

   public void addClassLoader(ClassLoader cl)
   {
      loaders.add(cl);
   }

   public boolean addClassLoaderURL(ClassLoader cl, URL url)
   {
      // This is a noop here
      return false;
   }

   public void removeClassLoader(ClassLoader cl)
   {
      loaders.remove(cl);
   }

   public UnifiedClassLoader newClassLoader(final URL url, boolean addToRepository)
      throws Exception
   {
      UnifiedClassLoader ucl = new UnifiedClassLoader(url);
      if( addToRepository )
         this.addClassLoader(ucl);
      return ucl;
   }
   public UnifiedClassLoader newClassLoader(final URL url, final URL origURL, boolean addToRepository)
      throws Exception
   {
      UnifiedClassLoader ucl = new UnifiedClassLoader(url, origURL);
      if( addToRepository )
         this.addClassLoader(ucl);
      return ucl;
   }
   public Class loadClass(String name, boolean resolve, ClassLoader cl)
      throws ClassNotFoundException
   {
      throw new ClassNotFoundException("loadClass(String,boolean,ClassLoader) not supported");
   }
   public URL getResource(String name, ClassLoader cl)
   {
      URL res = null;
      if( cl instanceof UnifiedClassLoader )
      {
         UnifiedClassLoader ucl = (UnifiedClassLoader) cl;
         res = ucl.getResourceLocally(name);
      }
      else
      {
         res = cl.getResource(name);
      }
      return res;
   }
   public void getResources(String name, ClassLoader cl, List urls)
   {
      Enumeration resURLs = null;
      try
      {
         if( cl instanceof UnifiedClassLoader )
         {
            UnifiedClassLoader ucl = (UnifiedClassLoader) cl;
            resURLs = ucl.findResourcesLocally(name);
         }
         else
         {
            resURLs = cl.getResources(name);
         }
         while( resURLs.hasMoreElements() )
            urls.add(resURLs.nextElement());
      }
      catch(Exception e)
      {
      }
   }
}
