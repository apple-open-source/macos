/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.loading;

import java.net.URL;
import java.net.URLClassLoader;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;

import javax.management.ListenerNotFoundException;
import javax.management.MBeanNotificationInfo;
import javax.management.Notification;
import javax.management.NotificationBroadcaster;
import javax.management.NotificationBroadcasterSupport;
import javax.management.NotificationFilter;
import javax.management.NotificationListener;

import org.jboss.logging.Logger;

import EDU.oswego.cs.dl.util.concurrent.ReentrantLock;

/** An obsolate implementation of the LoaderRepository interface
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author  <a href="mailto:simone.bordet@hp.com">Simone Bordet</a>.
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @deprecated Use the UnifiedLoaderRepository3 version
 * @version $Revision: 1.5.2.14 $
 * just a hint... xdoclet not really used
 * @jmx.name="JMImplementation:service=UnifiedLoaderRepository,name=Default"
 */
public class UnifiedLoaderRepository2 extends LoaderRepository
   implements NotificationBroadcaster, UnifiedLoaderRepository2MBean
{
   // Static --------------------------------------------------------
   private static final Logger log = Logger.getLogger(UnifiedLoaderRepository2.class);
   public static final ReentrantLock repositoryLock = new ReentrantLock();

   // Attributes ----------------------------------------------------

   /**
    * The classloaders we use for loading classes.
    */
   private HashSet classLoaders = new HashSet();

   /** A set used to check for duplicate URLs. Previously this was handled
    by the UCL.equals, but this caused problems with Class.forName(String,
    boolean, ClassLoader) caching.
    */
   private HashSet classLoaderURLs = new HashSet();

   /**
    * The classes loaded, maps names to class objects.
    */
   private HashMap classes = new HashMap();

   /**
    * Maps class loaders to the set of classes they loaded.
    */
   private HashMap loaderToClassesMap = new HashMap();

   /**
    * Maps class loaders to the set of resource names they looked up.
    */
   private HashMap loaderToResourcesMap = new HashMap();

   /**
    * The global resources. Map of name to ResourceInfo
    */
   private HashMap globalResources = new HashMap();

   /** A map of package names to the set of ClassLoaders which
    have classes in the package.
    */
   private HashMap packagesMap = new HashMap();

   /** A map of class loaders to the array of pckages names they serve
    */
   private HashMap loaderToPackagesMap = new HashMap();

   /**
    * The sequenceNumber used to number notifications.
    */
   private long sequenceNumber = 0;

   /**
    * We delegate our notification sending to a support object.
    */
   private final NotificationBroadcasterSupport broadcaster = new NotificationBroadcasterSupport();

   /**
    * The NotificationInfo we emit.
    */
   private MBeanNotificationInfo[] info;


   // Public --------------------------------------------------------

   public static boolean attempt(long waitMS)
   {
      boolean acquired = false;
      // Save and clear the interrupted state of the incoming thread
      boolean threadWasInterrupted = Thread.currentThread().interrupted();
      try
      {
         acquired = repositoryLock.attempt(waitMS); 
      }
      catch(InterruptedException e)
      {
      }
      finally
      {
         // Restore the interrupted state of the thread
         if( threadWasInterrupted )
            Thread.currentThread().interrupt();
      }
      return acquired;
   }
   public static void acquire()
   {
      // Save and clear the interrupted state of the incoming thread
      boolean threadWasInterrupted = Thread.currentThread().interrupted();
      try
      {
         repositoryLock.acquire();
      }
      catch(InterruptedException e)
      {
      }
      finally
      {
         // Restore the interrupted state of the thread
         if( threadWasInterrupted )
            Thread.currentThread().interrupt();
      }
   }
   public static void release()
   {
      repositoryLock.release();
   }

   public UnifiedClassLoader newClassLoader(final URL url, boolean addToRepository)
      throws Exception
   {
      UnifiedClassLoader2 ucl = new UnifiedClassLoader2(url, null, this);
      if( addToRepository )
      {
         UnifiedClassLoader delegate = ucl.getDelegate();
         this.addClassLoader(delegate);
      }
      return ucl;
   }
   public UnifiedClassLoader newClassLoader(final URL url, final URL origURL, boolean addToRepository)
      throws Exception
   {
      UnifiedClassLoader2 ucl = new UnifiedClassLoader2(url, origURL, this);
      if( addToRepository )
      {
         UnifiedClassLoader delegate = ucl.getDelegate();
         this.addClassLoader(delegate);
      }
      return ucl;
   }

   /**
    * Loads a class following the Unified ClassLoader architecture.
    */
   public Class loadClass(String name, boolean resolve, ClassLoader cl)
      throws ClassNotFoundException
   {
      // Try the cache before anything else.
      Class cls = loadClassFromCache(name, cl);

      // Found in cache, we're done
      if (cls != null) {return cls;}

      // Not found in cache, ask the calling classloader
      cls = loadClassFromClassLoader(name, resolve, cl);

      // The calling classloader sees the class, we're done
      if (cls != null) {return cls;}

      // Not visible by the calling classloader, iterate on the other classloaders
      cls = loadClassFromRepository(name, resolve, cl);
      if( cls == null && name.charAt(0) == '[' )
	{
         // First try to load the element class
         String subname = name.substring(2, name.length()-1);
         cls = loadClassFromRepository(subname, resolve, cl);
         if( cls != null )
         {
            // Retry loading the array class since we have the element class
            cls = loadClassFromRepository(name, resolve, cl);
         }
      }

      // Some other classloader sees the class, we're done
      if (cls != null) {return cls;}

      // This class is not visible
      throw new ClassNotFoundException(name);
   }

   private String getResourcePackageName(String rsrcName)
   {
      int index = rsrcName.lastIndexOf('/');
      String pkgName = rsrcName;
      if( index > 0 )
         pkgName = rsrcName.substring(0, index);
      return pkgName.replace('/', '.');
   }
   /** Is the class in a package for which we have a class loader mapping
    */
   private boolean containsClassPackage(String className)
   {
      String pkgName = ClassLoaderUtils.getPackageName(className);
      return packagesMap.containsKey(pkgName);
   }

   private Class loadClassFromCache(String name, ClassLoader cl)
   {
      // Return classes from the global cache
      // In this case the classloader is not used.
      Class cls = (Class)classes.get(name);
      return cls;
   }

   private void cacheLoadedClass(String name, Class cls, ClassLoader cl)
   {
      // Update the global cache
      classes.put(name, cls);

      // Update the cache for this classloader
      // This is used to cycling classloaders
      HashSet classes = (HashSet)loaderToClassesMap.get(cl);
      if (classes == null)
      {
         classes = new HashSet();
         loaderToClassesMap.put(cl, classes);
      }
      classes.add(name);
   }

   private Class loadClassFromClassLoader(String name, boolean resolve, ClassLoader cl)
   {
      if (cl instanceof UnifiedClassLoader)
      {
         try
         {
            Class cls = ((UnifiedClassLoader)cl).loadClassLocally(name, resolve);
            cacheLoadedClass(name, cls, cl);
            return cls;
         }
         catch (ClassNotFoundException x)
         {
            // The class is not visible by the calling classloader
         }
      }
      return null;
   }

   private Class loadClassFromRepository(String name, boolean resolve, ClassLoader cl)
   {
      // Get the set of class loaders from the packages map
      String pkgName = ClassLoaderUtils.getPackageName(name);
      HashSet pkgSet = (HashSet) this.packagesMap.get(pkgName);
      // If no pkg match was found there is no point looking any further
      if( pkgSet == null )
         return null;

      //for (Iterator i = classLoaders.iterator(); i.hasNext();)
      for(Iterator i = pkgSet.iterator(); i.hasNext();)
      {
         ClassLoader classloader = (ClassLoader)i.next();
         if (classloader.equals(cl))
         {
            continue;
         }

         if (classloader instanceof UnifiedClassLoader)
         {
            try
            {
               UnifiedClassLoader ucl = (UnifiedClassLoader) classloader;
               Class cls = ucl.loadClassLocally(name, resolve);
               cacheLoadedClass(name, cls, classloader);
               return cls;
            }
            catch (ClassNotFoundException ignored)
            {
               // Go on with next classloader
            }
         }
      }
      return null;
   }

   /**
    * Loads a resource following the Unified ClassLoader architecture
    */
   public URL getResource(String name, ClassLoader cl)
   {
      // getResource() calls are not synchronized on the classloader from JDK code.
      // First ask the cache (of the calling classloader)
      URL resource = getResourceFromCache(name, cl);

      // The resource was already loaded by the calling classloader, we're done
      if (resource != null) {return resource;}

      // Not found in cache, ask the calling classloader
      resource = getResourceFromClassLoader(name, cl);

      // The calling classloader sees the resource, we're done
      if (resource != null) {return resource;}

      // Not found in classloader, ask the global cache
      resource = getResourceFromGlobalCache(name);

      // The cache has it, we are done
      if (resource != null) {return resource;}

      // Not visible in global cache, iterate on all classloaders 
      resource = getResourceFromRepository(name, cl);

      // Some other classloader sees the resource, we're done
      if (resource != null) {return resource;}

      // This resource is not visible
      return null;
   }

   /** Not implemented
    * @param name
    * @param cl
    * @param urls
    */
   public void getResources(String name, ClassLoader cl, List urls)
   {
   }

   private URL getResourceFromCache(String name, ClassLoader cl)
   {
      // Differently from classes, resource are not looked up in a global cache,
      // as it is possible that 2 classloaders have the same resource name (ejb-jar.xml),
      // a global cache will overwrite. Instead we look in the classloader's cache
      // that we mantain to cycle the classloaders

      if (loaderToResourcesMap.containsKey(cl))
      {
         HashMap resources = (HashMap)loaderToResourcesMap.get(cl);
         return (URL)resources.get(name);
      }
      return null;
   }

   private URL getResourceFromClassLoader(String name, ClassLoader cl)
   {
      if (cl instanceof UnifiedClassLoader)
      {
         URL url = ((UnifiedClassLoader)cl).getResourceLocally(name);
         cacheLoadedResource(name, url, cl);
         return url;
      }
      return null;
   }

   private URL getResourceFromGlobalCache(String name)
   {
      // Invoked after the classloader is asked, caches the
      // results of global lookups
      ResourceInfo ri = (ResourceInfo) globalResources.get(name);
      if (ri != null)
         return ri.url;
      return null;
   }

   private URL getResourceFromRepository(String name, ClassLoader cl)
   {
      // Get the set of class loaders from the packages map
      String pkgName = getResourcePackageName(name);
      HashSet pkgSet = (HashSet) this.packagesMap.get(pkgName);
      Iterator i;
      if( pkgSet != null )
         i = pkgSet.iterator();
      // If no pkg match was found just go through all class loaders
      else
         i = classLoaders.iterator();

      while( i.hasNext() == true )
      {
         ClassLoader classloader = (ClassLoader)i.next();
         if (classloader.equals(cl))
         {
            continue;
         }

         if (classloader instanceof UnifiedClassLoader)
         {
            URL url = ((UnifiedClassLoader)classloader).getResourceLocally(name);
            if (url != null)
            {
               cacheLoadedResource(name, url, classloader);
               cacheGlobalResource(name, url, classloader);
               return url;
            }
            else
            {
               // Do nothing, go on with next classloader
            }
         }
      }
      return null;
   }

   private void cacheLoadedResource(String name, URL url, ClassLoader cl)
   {
      // Differently from classes there is no global cache.

      // Update the cache for this classloader only
      // This is used for cycling classloaders
      HashMap resources = (HashMap)loaderToResourcesMap.get(cl);
      if (resources == null)
      {
         resources = new HashMap();
         loaderToResourcesMap.put(cl, resources);
      }
      resources.put(name, url);
   }

   private void cacheGlobalResource(String name, URL url, ClassLoader cl)
   {
      // Resources looked up from one classloader, found in another.
      globalResources.put(name, new ResourceInfo(url, cl));
   }

   /** This is a utility method a listing of the URL for all UnifiedClassLoaders
    * associated with the repository. It is never called in response to
    * class or resource loading.
    */
   public URL[] getURLs()
   {
      HashSet classpath = new HashSet();
      HashSet tmp = (HashSet) classLoaders.clone();
      for (Iterator iter = tmp.iterator(); iter.hasNext();)
      {
         Object obj = iter.next();
         if (obj instanceof UnifiedClassLoader)
         {
            UnifiedClassLoader cl = (UnifiedClassLoader)obj;
            URL[] urls = cl.getClasspath();
            int length = urls != null ? urls.length : 0;
            for (int u = 0; u < length; u++)
            {
               URL path = urls[u];
               classpath.add(path);
            }
         }
      } // for all ClassLoaders

      URL[] cp = new URL[classpath.size()];
      classpath.toArray(cp);
      return cp;
   }

   // LoaderRepository overrides ------------------------------------

   public Class loadClass(String className) throws ClassNotFoundException
   {
      // if someone comes to us directly through LoaderRepository interface
      // notice that UCL loaders will skip this and invoke
      // loadClass(name, resolve, cl) directly
      ClassLoader scl = Thread.currentThread().getContextClassLoader();
      Class clazz = null;
      UnifiedLoaderRepository2.acquire();

      try
      {
         clazz = loadClass(className, false, scl);
      }
      catch (ClassNotFoundException e)
      {
         // If the TCL is not a UnifiedClassLoader then the scl was not asked
         // if it could load the class. Do so here.
         if ((scl instanceof UnifiedClassLoader) == false)
            clazz = scl.loadClass(className);
         // Else we need to rethrow the CNFE
         else
            throw e;
      }
      finally
      {
         UnifiedLoaderRepository2.release();
      }
      return clazz;
   }

   /** This is a utility method that iterates through the current class loaders
    * and tries to find the given class name. It is never invoked as part of
    * class or resource loading.
    * @return the Class object for name if found, null otherwise.
    *
    * @jmx.managed-operation
    */
   public Class findClass(String name)
   {
      // We have to find the class as a resource as we don't want to invoke
      // loadClass(name) and cause the side-effect of loading new classes.
      String classRsrcName = name.replace('.', '/') + ".class";

      HashSet tmp = (HashSet) classLoaders.clone();
      for (Iterator iter = tmp.iterator(); iter.hasNext();)
      {
         ClassLoader cl = (ClassLoader)iter.next();
         URL classURL = cl.getResource(classRsrcName);
         log.trace("Checking CL for URL: "+classURL);
         if (classURL != null)
         {
            try
            {
               // Since the class was found we can load it which should be a noop
               Class cls = cl.loadClass(name);
               log.trace("Found class in: "+cls.getProtectionDomain());
               return cls;
            }
            catch (ClassNotFoundException e)
            {
               log.debug("Failed to load class: " + name, e);
            }
         }
      }
      log.trace("Class not found");
      return null;
   }

   public Class loadClassWithout(ClassLoader loader, String className)
      throws ClassNotFoundException
   {
      throw new ClassNotFoundException("NYI");
   }

   public void addClassLoader(ClassLoader loader)
   {
      UnifiedLoaderRepository2.acquire();
      try
      {
         // if you come to us as UCL we send you straight to the orbit
         if (loader instanceof UnifiedClassLoader)
            addUnifiedClassLoader((UnifiedClassLoader)loader);

         // if you come to us as URLCL we'll slice you up and
         // orbit UCL per URL
         else if (loader instanceof URLClassLoader)
         {
            URLClassLoader ucl = (URLClassLoader)loader;
            URL[] urls = ucl.getURLs();

            for (int i = 0; i < urls.length; ++i)
            {
               addUnifiedClassLoader(new UnifiedClassLoader(urls[i], this));
            }
         }

         else
         {
            // addNonDelegatingClassLoader(loader);
            // throw new RuntimeException("ULR only allows UCL to be added");
            log.warn("Tried to add non- URLClassLoader.  Ignored");
         } // end of else
      }
      finally
      {
         UnifiedLoaderRepository2.release();
      }
   }

   public boolean addClassLoaderURL(ClassLoader cl, URL url)
   {
      return false;
   }

   private void addUnifiedClassLoader(UnifiedClassLoader cl)
   {
      cl.setRepository(this);
      // See if this URL already exists
      URL url = cl.getURL();
      boolean added = false;
      boolean exists = classLoaderURLs.contains(url);
      // If already present will not be added
      if(!exists)
      {
	 classLoaderURLs.add(url);
         added = classLoaders.add(cl);
      }
      if (added)
      {
         log.debug("Adding "+cl);
         updatePackageMap(cl);
      }
      else
      {
         log.debug("Skipping duplicate "+cl);
      }
   }

   /** Walk through the class loader URL to see what packages it is capable
    of handling
    */
   private void updatePackageMap(UnifiedClassLoader cl)
   {
      try
      {
         String[] pkgNames = ClassLoaderUtils.updatePackageMap(cl, packagesMap);
         loaderToPackagesMap.put(cl, pkgNames);
      }
      catch(Exception e)
      {
         if( log.isTraceEnabled() )
            log.trace("Failed to update pkgs for cl="+cl, e);
         else
            log.debug("Failed to update pkgs for cl="+cl, e);
      }
   }

   public void removeClassLoader(ClassLoader cl)
   {
      UnifiedLoaderRepository2.acquire();
      try
      {
         if( cl instanceof UnifiedClassLoader )
         {
            UnifiedClassLoader ucl = (UnifiedClassLoader) cl;
            URL url = ucl.getURL();
            classLoaderURLs.remove(url);
         }
         boolean removed = classLoaders.remove(cl);
         log.debug("UnifiedLoaderRepository removed("+removed+") " + cl);

         // Take care also of the cycling mapping for classes
         if (loaderToClassesMap.containsKey(cl))
         {
            HashSet loaded = (HashSet)loaderToClassesMap.remove(cl);
            // This classloader has loaded at least one class
            if (loaded != null)
            {
               // Notify that classes are about to be removed
               for (Iterator iter = loaded.iterator(); iter.hasNext();)
               {
                  broadcaster.sendNotification(new Notification(CLASS_REMOVED, this, getNextSequenceNumber(), (String)iter.next()));
               }

               // Remove the classes from the global cache
               for (Iterator i = loaded.iterator(); i.hasNext();)
               {
                  String cls = (String)i.next();
                  this.classes.remove(cls);
               }
            }
         }

         // Take care also of the cycling mapping for resources
         if (loaderToResourcesMap.containsKey(cl))
         {
            HashMap resources = (HashMap)loaderToResourcesMap.remove(cl);

            // Remove the resources from the global cache that are from this classloader
            if (resources != null)
            {
               for (Iterator i = resources.keySet().iterator(); i.hasNext();)
               {
                  String name = (String) i.next();
                  ResourceInfo ri = (ResourceInfo) globalResources.get(name);
                  if (ri != null && ri.cl == cl)
                     globalResources.remove(name);
               }
            }
         }

         // Clean up the package name to class loader mapping
         String[] pkgNames = (String[]) loaderToPackagesMap.remove(cl);
         int length = pkgNames != null ? pkgNames.length : 0;
         for(int p = 0; p < length; p ++)
         {
            String pkgName = pkgNames[p];
            HashSet pkgSet = (HashSet) packagesMap.get(pkgName);
            if( pkgSet != null )
            {
               pkgSet.remove(cl);
               if( pkgSet.isEmpty() )
                  packagesMap.remove(pkgName);
            }
         }
      }
      finally
      {
         UnifiedLoaderRepository2.release();
      }
   }

   /**
    * This method provides an mbean-accessible way to add a
    * UnifiedClassloader, and sends a notification when it is added.
    *
    * @param ucl an <code>UnifiedClassLoader</code> value
    * @return a <code>LoaderRepository</code> value
    *
    * @jmx.managed-operation
    */
   public LoaderRepository registerClassLoader(UnifiedClassLoader ucl)
   {
      addClassLoader(ucl);
      Notification msg = new Notification(CLASSLOADER_ADDED, this, getNextSequenceNumber());
      msg.setUserData(ucl);
      broadcaster.sendNotification(msg);

      return this;
   }

   /**
    * @jmx.managed-operation
    */
   public LoaderRepository getInstance()
   {
      return this;
   }

   // implementation of javax.management.NotificationBroadcaster interface

   /**
    * addNotificationListener delegates to the broadcaster object we hold.
    *
    * @param listener a <code>NotificationListener</code> value
    * @param filter a <code>NotificationFilter</code> value
    * @param handback an <code>Object</code> value
    * @exception IllegalArgumentException if an error occurs
    */
   public void addNotificationListener(NotificationListener listener, NotificationFilter filter, Object handback) throws IllegalArgumentException
   {
      broadcaster.addNotificationListener(listener, filter, handback);
   }

   /**
    *
    * @return <description>
    */
   public MBeanNotificationInfo[] getNotificationInfo()
   {
      if (info == null)
      {
         info = new MBeanNotificationInfo[]{
            new MBeanNotificationInfo(new String[]{"CLASSLOADER_ADDED"},
                                      "javax.management.Notification",
                                      "Notification that a classloader has been added to the extensible classloader"),
            new MBeanNotificationInfo(new String[]{"CLASS_REMOVED"},
                                      "javax.management.Notification",
                                      "Notification that a class has been removed from the extensible classloader")

         };
      }
      return info;
   }

   /**
    * removeNotificationListener delegates to our broadcaster object
    *
    * @param listener a <code>NotificationListener</code> value
    * @exception ListenerNotFoundException if an error occurs
    */
   public void removeNotificationListener(NotificationListener listener) throws ListenerNotFoundException
   {
      broadcaster.removeNotificationListener(listener);
   }

   private long getNextSequenceNumber()
   {
      return sequenceNumber++;
   }
}

