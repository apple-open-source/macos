/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.loading;

import java.net.URL;
import java.util.HashMap;
import java.util.List;
import java.util.Vector;

import org.jboss.mx.server.ServerConstants;

/**
 *
 * @see javax.management.loading.DefaultLoaderRepository
 * @see org.jboss.mx.loading.BasicLoaderRepository
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.4.4.4 $  
 */
public abstract class LoaderRepository
   implements ServerConstants
{

   // Attributes ----------------------------------------------------
   protected static Vector loaders = new Vector();
   protected static LoaderRepository instance = null;

   /**
    * Native signature to class map
    */
   protected static HashMap nativeClassBySignature;
   
   // Static --------------------------------------------------------
   /**
    * Construct the native class map
    */
   static
   {
     nativeClassBySignature = new HashMap();
     nativeClassBySignature.put(boolean.class.getName(),
                                Boolean.TYPE);
     nativeClassBySignature.put(byte.class.getName(), 
                                Byte.TYPE);
     nativeClassBySignature.put(char.class.getName(), 
                                Character.TYPE);
     nativeClassBySignature.put(double.class.getName(), 
                                Double.TYPE);
     nativeClassBySignature.put(float.class.getName(), 
                                Float.TYPE);
     nativeClassBySignature.put(int.class.getName(), 
                                Integer.TYPE);
     nativeClassBySignature.put(long.class.getName(), 
                                Long.TYPE);
     nativeClassBySignature.put(short.class.getName(), 
                                Short.TYPE);
     nativeClassBySignature.put(void.class.getName(), 
                                Void.TYPE);
   }

   public synchronized static LoaderRepository getDefaultLoaderRepository()
   {
      
      if (instance != null)
         return instance;
         
      ClassLoader cl = Thread.currentThread().getContextClassLoader();
      String className = System.getProperty(LOADER_REPOSITORY_CLASS_PROPERTY, DEFAULT_LOADER_REPOSITORY_CLASS);
      System.setProperty(LOADER_REPOSITORY_CLASS_PROPERTY, className);

      try 
      {
         Class repository = cl.loadClass(className);
         instance = (LoaderRepository)repository.newInstance();
         
         return instance;
      }
      catch (ClassNotFoundException e)
      {
         throw new Error("Cannot instantiate default loader repository class. Class " + className + " not found.");
      }
      catch (ClassCastException e) 
      {
         throw new Error("Cannot instantiate default loader repository class. The target class is not an instance of LoaderRepository interface.");
      }
      catch (Exception e) 
      {
         throw new Error("Error creating default loader repository: " + e.toString());      
      }
   }
   
   // Public --------------------------------------------------------   
   public Vector getLoaders()
   {
      return loaders;
   }

   public URL[] getURLs()
   {
      return null;
   }

   public Class getCachedClass(String classname)
   {
      throw new RuntimeException("NOT IMPLEMENTED!!!  THIS METHOD NEEDS TO BE IMPLEMENTED");
   }

   /** Create UnifiedClassLoader and optionally add it to the repository
    * @param url the URL to use for class loading
    * @param addToRepository a flag indicating if the UCL should be added to
    *    the repository
    * @return the UCL instance
    * @throws Exception
    */
   public abstract UnifiedClassLoader newClassLoader(final URL url, boolean addToRepository)
      throws Exception;
   /** Create UnifiedClassLoader and optionally add it to the repository
    * @param url the URL to use for class loading
    * @param origURL an orignal URL to use as the URL for the UCL CodeSource.
    * This is useful when the url is a local copy that is difficult to use for
    * security policy writing.
    * @param addToRepository a flag indicating if the UCL should be added to
    *    the repository
    * @return the UCL instance
    * @throws Exception
    */
   public abstract UnifiedClassLoader newClassLoader(final URL url, final URL origURL,
      boolean addToRepository)
      throws Exception;

   /** Load the given class from the repository.
    * @param className
    * @return
    * @throws ClassNotFoundException
    */
   public abstract Class loadClass(String className) throws ClassNotFoundException;
   /** Load the given class from the repository
    * @param name
    * @param resolve
    * @param cl
    * @return
    * @throws ClassNotFoundException
    */
   public abstract Class loadClass(String name, boolean resolve, ClassLoader cl)
      throws ClassNotFoundException;

   /** Find a resource URL for the given name
    *
    * @param name the resource name
    * @param cl the requesting class loader
    * @return The resource URL if found, null otherwise
    */
   public abstract URL getResource(String name, ClassLoader cl);
   /** Find all resource URLs for the given name. Since this typically
    * entails an exhuastive search of the repository it can be a relatively
    * slow operation.
    *
    * @param name the resource name
    * @param cl the requesting class loader
    * @param urls a list into which the located resource URLs will be placed
    */
   public abstract void getResources(String name, ClassLoader cl, List urls);

   /** Not used.
    * @param loader
    * @param className
    * @return
    * @throws ClassNotFoundException
    */
   public abstract Class loadClassWithout(ClassLoader loader, String className)
      throws ClassNotFoundException;
   /** Add a class loader to the repository
    */
   public abstract void addClassLoader(ClassLoader cl);
   /** Update the set of URLs known to be associated with a previously added
    * class loader.
    *
    * @param cl
    * @param url
    */
   public abstract boolean addClassLoaderURL(ClassLoader cl, URL url);
   /** Remove a cladd loader from the repository.
    * @param cl
    */
   public abstract void removeClassLoader(ClassLoader cl);
}
