/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.loading;

import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Enumeration;
import java.io.IOException;

import javax.management.ListenerNotFoundException;
import javax.management.MBeanNotificationInfo;
import javax.management.Notification;
import javax.management.NotificationBroadcaster;
import javax.management.NotificationBroadcasterSupport;
import javax.management.NotificationFilter;
import javax.management.NotificationListener;

import org.jboss.logging.Logger;
import EDU.oswego.cs.dl.util.concurrent.ConcurrentReaderHashMap;

/** A repository of class loaders that form a flat namespace of classes
 * and resources. This version uses UnifiedClassLoader3 instances. Class
 * and resource loading is synchronized by the acquiring the monitor to the
 * associated repository structure monitor. See the variable javadoc comments
 * for what monitor is used to access a given structure.
 *
 * @author  <a href="mailto:scott.stark@jboss.org">Scott Stark</a>.
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.1.4.21 $
 * just a hint... xdoclet not really used
 * @jmx.name="JMImplementation:service=UnifiedLoaderRepository,name=Default"
 */
public class UnifiedLoaderRepository3 extends LoaderRepository
   implements NotificationBroadcaster, UnifiedLoaderRepository3MBean
{
   // Static --------------------------------------------------------
   private static final Logger log = Logger.getLogger(UnifiedLoaderRepository3.class);
   /** Used to provide a relative ordering of UCLs based on the order in
    * which they are added to the repository */
   private static int addedCount;

   // Attributes ----------------------------------------------------

   /** HashSet<UCL> of classloaders in the repository.
    * Access synchronized via this.classLoaders monitor.
    */
   private HashSet classLoaders = new HashSet();

   /** A HashSet<URL> used to check for duplicate URLs. Previously this was handled
    by the UCL.equals, but this caused problems with Class.forName(String,
    boolean, ClassLoader) caching.
    Access synchronized via this.classLoaders monitor.
    */
   private HashSet classLoaderURLs = new HashSet();

   /** The loaded classes cache, HashMap<String, Class>.
    * Access synchronized via this.classes monitor.
    */
   private ConcurrentReaderHashMap classes = new ConcurrentReaderHashMap();

   /** HashMap<UCL, HashSet<String>> class loaders to the set of class names
    * loaded via the UCL.
    * Access synchronized via this.classes monitor.
    */
   private HashMap loaderToClassesMap = new HashMap();

   /** HashMap<UCL, HashMap<String, URL>> class loaders to the set of
    * resource names they looked up.
    * Access synchronized via this.loaderToResourcesMap monitor.
    */
   private HashMap loaderToResourcesMap = new HashMap();

   /** HashMap<String, ResourceInfo(URL, UCL)> of global resources not unique
    * to a UCL
    * Access synchronized via this.loaderToResourcesMap monitor.
    */
   private HashMap globalResources = new HashMap();

   /** A HashMap<String, HashSet<UCL>> of package names to the set of
    * ClassLoaders which have classes in the package.
    * Access synchronized via this.packagesMap monitor.
    */
   private HashMap packagesMap = new HashMap();

   /** A HashMap<UCL, String[]> of class loaders to the array of pckages names
    * they serve
    * Access synchronized via this.packagesMap monitor.
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

   public UnifiedClassLoader newClassLoader(final URL url, boolean addToRepository)
      throws Exception
   {
      UnifiedClassLoader3 ucl = new UnifiedClassLoader3(url, null, this);
      if( addToRepository )
      {
         this.registerClassLoader(ucl);
      }
      return ucl;
   }
   public UnifiedClassLoader newClassLoader(final URL url, final URL origURL, boolean addToRepository)
      throws Exception
   {
      UnifiedClassLoader3 ucl = new UnifiedClassLoader3(url, origURL, this);
      if( addToRepository )
      {
         this.registerClassLoader(ucl);
      }
      return ucl;
   }

   public int getCacheSize()
   {
      return classes.size();
   }
   public int getClassLoadersSize()
   {
      return classLoaders.size();
   }
   public void flush()
   {
      synchronized( classes )
      {
         classes.clear();
      }
   }

   public Class getCachedClass(String classname)
   {
      return (Class)classes.get(classname);
   }

   /** Unlike other implementations of LoaderRepository, this method does
    * nothing but ask the UnifiedClassLoader3 to load the class as UCL3s
    * do not use this method.
    */
   public Class loadClass(String name, boolean resolve, ClassLoader cl)
      throws ClassNotFoundException
   {
      UnifiedClassLoader3 ucl = (UnifiedClassLoader3) cl;
      return ucl.loadClass(name, resolve);
   }

   /** Called by LoadMgr to obtain all class loaders for the given className
    * @return HashSet<UnifiedClassLoader3>, may be null
    */
   public HashSet getPackageClassLoaders(String className)
   {
      String pkgName = ClassLoaderUtils.getPackageName(className);
      HashSet pkgSet = null;
      synchronized( packagesMap )
      {
         pkgSet = (HashSet) this.packagesMap.get(pkgName);
      }
      return pkgSet;
   }

   private String getResourcePackageName(String rsrcName)
   {
      int index = rsrcName.lastIndexOf('/');
      String pkgName = rsrcName;
      if( index > 0 )
         pkgName = rsrcName.substring(0, index);
      return pkgName.replace('/', '.');
   }

   /** Lookup a Class from the repository cache.
    * @param name the fully qualified class name
    * @return the cached Class if found, null otherwise
    */
   public Class loadClassFromCache(String name)
   {
      Class cls = null;
      synchronized( classes )
      {
         cls = (Class) classes.get(name);
      }
      return cls;
   }

   /** Add a Class to the repository cache.
    * @param name the fully qualified class name
    * @param cls the Class instance
    * @param cl the repository UCL
    */
   public void cacheLoadedClass(String name, Class cls, ClassLoader cl)
   {
      synchronized( classes )
      {
         // Update the global cache
         Object prevClass = classes.put(name, cls);
         if( log.isTraceEnabled() )
         {
            log.trace("cacheLoadedClass, classname: "+name+", class: "+cls
               +", ucl: "+cl);
         }

         // Update the cache for this classloader
         // This is used to cycling classloaders
         HashSet loadedClasses = (HashSet) loaderToClassesMap.get(cl);
         if (loadedClasses == null)
         {
            loadedClasses = new HashSet();
            loaderToClassesMap.put(cl, loadedClasses);
         }
         loadedClasses.add(name);
      }
   }

   Class loadClassFromClassLoader(String name, boolean resolve, UnifiedClassLoader cl)
   {
      if (cl instanceof UnifiedClassLoader)
      {
         try
         {
            Class cls = cl.loadClassLocally(name, resolve);
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

   /**
    * Loads a resource following the Unified ClassLoader architecture
    */
   public URL getResource(String name, ClassLoader cl)
   {
      // getResource() calls are not synchronized on the classloader from JDK code.
      // First ask the cache (of the calling classloader)
      URL resource = getResourceFromCache(name, cl);

      // The resource was already loaded by the calling classloader, we're done
      if (resource != null)
         return resource;

      // Not found in cache, ask the calling classloader
      resource = getResourceFromClassLoader(name, cl);

      // The calling classloader sees the resource, we're done
      if (resource != null)
         return resource;

      // Not found in classloader, ask the global cache
      resource = getResourceFromGlobalCache(name);

      // The cache has it, we are done
      if (resource != null)
         return resource;

      // Not visible in global cache, iterate on all classloaders
      resource = getResourceFromRepository(name, cl);

      // Some other classloader sees the resource, we're done
      if (resource != null)
         return resource;

      // This resource is not visible
      return null;
   }

   /** Find all resource URLs for the given name. This is entails an
    * exhuastive search of the repository and is an expensive operation.
    *
    * @param name the resource name
    * @param cl the requesting class loader
    * @param urls a list into which the located resource URLs will be placed
    */
   public void getResources(String name, ClassLoader cl, List urls)
   {
      // Go through all class loaders
      HashSet tmpClassLoaders;
      synchronized( classLoaders )
      {
         tmpClassLoaders = new HashSet(classLoaders);
      }
      Iterator iter = tmpClassLoaders.iterator();
      while( iter.hasNext() == true )
      {
         ClassLoader nextCL = (ClassLoader) iter.next();
         if (nextCL instanceof UnifiedClassLoader)
         {
            UnifiedClassLoader ucl = (UnifiedClassLoader) nextCL;
            try
            {
               Enumeration resURLs = ucl.findResourcesLocally(name);
               while( resURLs.hasMoreElements() )
               {
                  Object res = resURLs.nextElement();
                  urls.add(res);
               }
            }
            catch(IOException ignore)
            {
            }
         }
      }
   }

   /** As opposed to classes, resource are not looked up in a global cache,
    * since it is possible that 2 classloaders have the same resource name
    * (ejb-jar.xml), a global cache will overwrite. Instead we look in the
    * classloader's cache that we mantain to cycle the classloaders
    * @param name the resource name
    * @param cl the repository classloader
    * @return the resource URL if found, null otherwise
    */
   private URL getResourceFromCache(String name, ClassLoader cl)
   {
      URL resource = null;
      synchronized( loaderToResourcesMap )
      {
         if (loaderToResourcesMap.containsKey(cl))
         {
            HashMap resources = (HashMap)loaderToResourcesMap.get(cl);
            resource = (URL) resources.get(name);
         }
      }
      return resource;
   }

   private URL getResourceFromClassLoader(String name, ClassLoader cl)
   {
      URL resource = null;
      if (cl instanceof UnifiedClassLoader)
      {
         UnifiedClassLoader ucl = (UnifiedClassLoader) cl;
         resource = ucl.getResourceLocally(name);
         cacheLoadedResource(name, resource, cl);
      }
      return resource;
   }

   /** Check for a resource in the global cache
    * Synchronizes access to globalResources using the loaderToResourcesMap monitor
    * @param name
    * @return
    */
   private URL getResourceFromGlobalCache(String name)
   {
      ResourceInfo ri = null;
      synchronized( loaderToResourcesMap )
      {
         ri = (ResourceInfo) globalResources.get(name);
      }
      URL resource = null;
      if (ri != null)
         resource = ri.url;
      return resource;
   }

   private URL getResourceFromRepository(String name, ClassLoader cl)
   {
      // Get the set of class loaders from the packages map
      String pkgName = getResourcePackageName(name);
      Iterator i = null;
      synchronized( packagesMap )
      {
         HashSet pkgSet = (HashSet) this.packagesMap.get(pkgName);
         if( pkgSet != null )
         {
            pkgSet = new HashSet(pkgSet);
            i = pkgSet.iterator();
         }
      }
      if( i == null )
      {
         // If no pkg match was found just go through all class loaders
         synchronized( classLoaders )
         {
            HashSet tmpClassLoaders = new HashSet(classLoaders);
            i = tmpClassLoaders.iterator();
         }
      }

      URL url = null;
      while( i.hasNext() == true )
      {
         ClassLoader classloader = (ClassLoader)i.next();
         if (classloader.equals(cl))
         {
            continue;
         }

         if (classloader instanceof UnifiedClassLoader)
         {
            url = ((UnifiedClassLoader)classloader).getResourceLocally(name);
            if (url != null)
            {
               cacheLoadedResource(name, url, classloader);
               cacheGlobalResource(name, url, classloader);
               break;
            }
            else
            {
               // Do nothing, go on with next classloader
            }
         }
      }
      return url;
   }

   /** Update the loaderToResourcesMap
    * @param name the resource name
    * @param url the resource URL
    * @param cl the UCL
    */
   private void cacheLoadedResource(String name, URL url, ClassLoader cl)
   {
      // Update the cache for this classloader only
      // This is used for cycling classloaders
      synchronized( loaderToResourcesMap )
      {
         HashMap resources = (HashMap) loaderToResourcesMap.get(cl);
         if (resources == null)
         {
            resources = new HashMap();
            loaderToResourcesMap.put(cl, resources);
         }
         resources.put(name, url);
      }
   }

   /** Update cache of resources looked up via one UCL, buf found in another UCL
    * @param name the resource name
    * @param url the resource URL
    * @param cl the UCL
    */
   private void cacheGlobalResource(String name, URL url, ClassLoader cl)
   {
      synchronized( loaderToResourcesMap )
      {
         globalResources.put(name, new ResourceInfo(url, cl));
      }
   }

   /** This is a utility method a listing of the URL for all UnifiedClassLoaders
    * associated with the repository. It is never called in response to
    * class or resource loading.
    */
   public URL[] getURLs()
   {
      HashSet classpath = new HashSet();
      HashSet tmp = null;
      synchronized( classLoaders )
      {
         tmp = (HashSet) classLoaders.clone();
      }
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

   /** A utility method that iterates over all repository class loaders and
    * display the class information for every UCL that contains the given
    * className
    */
   public String displayClassInfo(String className)
   {
      /* We have to find the class as a resource as we don't want to invoke
      loadClass(name) and cause the side-effect of loading new classes.
      */
      String classRsrcName = className.replace('.', '/') + ".class";

      int count = 0;
      Class loadedClass = this.loadClassFromCache(className);
      StringBuffer results = new StringBuffer(className+" Information\n");
      if( loadedClass != null )
      {
         results.append("Repository cache version:");
         ClassLoaderUtils.displayClassInfo(loadedClass, results);
      }
      else
      {
         results.append("Not loaded in repository cache\n");
      }
      HashSet tmp = null;
      synchronized( classLoaders )
      {
         tmp = (HashSet) classLoaders.clone();
      }
      for (Iterator iter = tmp.iterator(); iter.hasNext();)
      {
         URLClassLoader cl = (URLClassLoader) iter.next();
         URL classURL = cl.findResource(classRsrcName);
         if (classURL != null)
         {
            results.append("\n\n### Instance"+count+" found in UCL: "+cl+"\n");
            count ++;
         }
      }

      // Also look to the parent class loaders of the TCL
      ClassLoader tcl = Thread.currentThread().getContextClassLoader();
      URLClassLoader[] stack =  ClassLoaderUtils.getClassLoaderStack(tcl);
      for (int s = 0; s < stack.length; s ++)
      {
         URLClassLoader cl = stack[s];
         URL classURL = cl.findResource(classRsrcName);
         if (classURL != null)
         {
            results.append("\n\n### Instance"+count+" via UCL: "+cl+"\n");
            count ++;
         }
      }

      return results.toString();
   }

   // LoaderRepository overrides ------------------------------------

   /**
    */
   public Class loadClass(String className) throws ClassNotFoundException
   {
      ClassLoader scl = Thread.currentThread().getContextClassLoader();
      Class clazz = null;

      if( (scl instanceof UnifiedClassLoader3) == true )
      {
         clazz = loadClass(className, false, scl);
      }
      else
      {
         clazz = scl.loadClass(className);
      }
      return clazz;
   }

   public Class loadClassWithout(ClassLoader loader, String className)
      throws ClassNotFoundException
   {
      throw new ClassNotFoundException("NYI");
   }

   /** Add a class loader to the repository.
    */
   public void addClassLoader(ClassLoader loader)
   {
      // if you come to us as UCL we send you straight to the orbit
      if (loader instanceof UnifiedClassLoader)
         addUnifiedClassLoader((UnifiedClassLoader3)loader);

      // if you come to us as URLCL we'll slice you up and
      // orbit UCL per URL
      else if (loader instanceof URLClassLoader)
      {
         URLClassLoader ucl = (URLClassLoader)loader;
         URL[] urls = ucl.getURLs();

         for (int i = 0; i < urls.length; ++i)
         {
            UnifiedClassLoader3 ucl3 = new UnifiedClassLoader3(urls[i], urls[i], this);
            addUnifiedClassLoader(ucl3);
         }
      }
      else
      {
         log.warn("Tried to add non-URLClassLoader.  Ignored");
      } // end of else
   }

   public boolean addClassLoaderURL(ClassLoader cl, URL url)
   {
      UnifiedClassLoader ucl = (UnifiedClassLoader) cl;
      boolean added = false;
      synchronized( classLoaders )
      {
         if( classLoaderURLs.contains(url) == false )
         {
            updatePackageMap(ucl, url);
            classLoaderURLs.add(url);
            added = true;
         }
      }
      return added;
   }

   /** Add a UCL to the repository.
    * This sychronizes on classLoaders.
    * @param cl
    */
   private void addUnifiedClassLoader(UnifiedClassLoader3 cl)
   {
      cl.setRepository(this);
      // See if this URL already exists
      URL url = cl.getURL();
      boolean added = false;
      synchronized( classLoaders )
      {
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
            addedCount ++;
            cl.setAddedOrder(addedCount);
            updatePackageMap(cl);
         }
         else
         {
            log.debug("Skipping duplicate "+cl);
         }
      }
   }

   /** Walk through the class loader URL to see what packages it is capable
    of handling
    */
   private void updatePackageMap(UnifiedClassLoader cl)
   {
      try
      {
         synchronized( packagesMap )
         {
            String[] pkgNames = ClassLoaderUtils.updatePackageMap(cl, packagesMap);
            loaderToPackagesMap.put(cl, pkgNames);
         }
      }
      catch(Exception e)
      {
         if( log.isTraceEnabled() )
            log.trace("Failed to update pkgs for cl="+cl, e);
         else
            log.debug("Failed to update pkgs for cl="+cl, e);
      }
   }
   /** Walk through the new URL to update the packages the ClassLoader is
    * capable of handling
    */
   private void updatePackageMap(UnifiedClassLoader cl, URL url)
   {
      try
      {
         synchronized( packagesMap )
         {
            String[] prevPkgNames = (String[]) loaderToPackagesMap.get(cl);
            String[] pkgNames = ClassLoaderUtils.updatePackageMap(cl, packagesMap, url, prevPkgNames);
            loaderToPackagesMap.put(cl, pkgNames);
         }
      }
      catch(Exception e)
      {
         if( log.isTraceEnabled() )
            log.trace("Failed to update pkgs for cl="+cl, e);
         else
            log.debug("Failed to update pkgs for cl="+cl, e);
      }
   }

   /** Remove the class loader from the repository. This synchronizes on the
    * this.classLoaders
    */
   public void removeClassLoader(ClassLoader cl)
   {
      ArrayList removeNotifications = new ArrayList();
      synchronized( classLoaders )
      {
         if( cl instanceof UnifiedClassLoader )
         {
            UnifiedClassLoader ucl = (UnifiedClassLoader) cl;
            URL[] urls = ucl.getClasspath();
            for(int u = 0; u < urls.length; u ++)
               classLoaderURLs.remove(urls[u]);
         }
         boolean removed = classLoaders.remove(cl);
         log.debug("UnifiedLoaderRepository removed("+removed+") " + cl);

         // Take care also of the cycling mapping for classes
         HashSet loadedClasses = null;
         boolean hasLoadedClasses = false;
         synchronized( classes )
         {
            hasLoadedClasses = loaderToClassesMap.containsKey(cl);
            if( hasLoadedClasses )
               loadedClasses = (HashSet) loaderToClassesMap.remove(cl);
            // This classloader has loaded at least one class
            if (loadedClasses != null)
            {
               // Notify that classes are about to be removed
               for (Iterator iter = loadedClasses.iterator(); iter.hasNext();)
               {
                  String className = (String)iter.next();
                  Notification n = new Notification(CLASS_REMOVED, this,
                        getNextSequenceNumber(), className);
                  removeNotifications.add(n);
               }

               // Remove the classes from the global cache
               for (Iterator i = loadedClasses.iterator(); i.hasNext();)
               {
                  String cls = (String)i.next();
                  this.classes.remove(cls);
               }
            }
         }

         // Take care also of the cycling mapping for resources
         synchronized( loaderToResourcesMap )
         {
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
         }

         // Clean up the package name to class loader mapping
         synchronized( packagesMap )
         {
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
      }

      // Send the class removal notfications outside of the synchronized block
      for(int n = 0; n < removeNotifications.size(); n ++)
      {
         Notification msg = (Notification) removeNotifications.get(n);
         broadcaster.sendNotification(msg);
      }

      Notification msg = new Notification(CLASSLOADER_REMOVED, this, getNextSequenceNumber());
      msg.setUserData(cl);
      broadcaster.sendNotification(msg);
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
   public void addNotificationListener(NotificationListener listener,
      NotificationFilter filter, Object handback) throws IllegalArgumentException
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

   private synchronized long getNextSequenceNumber()
   {
      return sequenceNumber++;
   }

}
