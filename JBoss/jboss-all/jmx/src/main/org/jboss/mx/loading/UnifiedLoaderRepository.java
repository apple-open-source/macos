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

/** An obsolate implementation of the LoaderRepository interface
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author  <a href="mailto:simone.bordet@hp.com">Simone Bordet</a>.
 * @deprecated Use the UnifiedLoaderRepository3 version
 * @version $Revision: 1.17.2.4 $
 * just a hint... xdoclet not really used
 * @jmx.name="JMImplementation:service=UnifiedLoaderRepository,name=Default"
 */
public class UnifiedLoaderRepository
        extends LoaderRepository
        implements NotificationBroadcaster, UnifiedLoaderRepositoryMBean
{
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

   // Used to properly synchronize class loading
   private Thread currentThread;
   private ReentrantLock reentrantLock = new ReentrantLock();
   private Object lock = new Object();
   private int threadsCount;

   // Static --------------------------------------------------------

   private static final Logger log = Logger.getLogger(UnifiedLoaderRepository.class);

   // Public --------------------------------------------------------

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


   /**
    * Loads a class following the Unified ClassLoader architecture.
    */
   public Class loadClass(String name, boolean resolve, ClassLoader cl) throws ClassNotFoundException
   {
      try
      {
         try
         {
            // Only one thread at a time can load classes
            // Pass the classloader to release its lock when blocking the thread
            // We cannot use synchronized (this), as we MUST release the lock
            // on the classloader. Change this only after discussion on the
            // developer's list !
            synchronize(cl);

            // This syncronized block is necessary to synchronize with add/removeClassLoader
            // See comments in add/removeClassLoader; we iterate on the classloaders, must avoid
            // someone removes or adds a classloader in the meanwhile.
            synchronized (this)
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

               // Some other classloader sees the class, we're done
               if (cls != null) {return cls;}

               // This class is not visible
               throw new ClassNotFoundException(name);
            }
         }
         finally
         {
            unsynchronize(cl);
         }
      }
      catch (ClassCircularityError x)
      {
         // Another thread is asking for the same class with the same classloader
         // from a loadClassInternal() call, so the current thread throws
         // ClassCircularityError. We ask it to poll the cache until the other thread
         // has loaded the class. We have released all locks, we just ping
         // the cache from time to time.
         // PENDING: maybe it can be done in a more efficient way using wait()/notifyAll()
         // on 'lock'
         String circularityClassName = x.getMessage();
         circularityClassName = circularityClassName.replace('/', '.');

         Class cls = null;
         while (cls == null)
         {
            try
            {
               Thread.sleep(137);
            }
            catch (InterruptedException ignored)
            {
            }

            synchronized (this)
            {
               cls = loadClassFromCache(circularityClassName, cl);
            }
         }
         return cls;
      }
   }


   private void synchronize(ClassLoader cl)
   {
      // This method
      // 1- must allow only one thread at a time,
      // 2- must allow a re-entrant thread,
      // 3- must unlock the given classloader waiting on it,
      // 4- must not hold any other lock.
      // If these 4 are not done, deadlock will happen.
      // Point 3 is necessary to fix Jung's RFE#4670071

      // Beware also that is possible that a classloader arrives here already locked
      // (for example via loadClassInternal()) and here we cannot synchronize on 'this'
      // otherwise we deadlock in loadClass() where we first synchronize on 'this' and
      // then on the classloader (resource ordering problem).
      // We solve this by using a reentrant lock.

      // Save and clear the interrupted state of the incoming thread
      boolean threadWasInterrupted = Thread.currentThread().interrupted();
      try
      {
         // Only one thread can pass this barrier
         // Other will accumulate here and let passed one at a time to wait on the classloader,
         // like a dropping sink
         reentrantLock.acquire();

         while (!isThreadAllowed(Thread.currentThread()))
         {
            // This thread is not allowed to run (another one is already running)
            // so I release() to let another thread to enter (will come here again)
            // and they will wait on the classloader to release its lock.
            // It is important that the wait below is not wait(0) since it may be
            // possible that a notifyAll arrives before the wait.
            // It is also important that this release() is outside the sync block on
            // the classloader, to avoid deadlock with threads that triggered
            // loadClassInternal(), locking the classloader
            reentrantLock.release();

            synchronized (cl)
            {
               // Threads will wait here on the classloader object.
               // Waiting on the classloader is fundamental to workaround Jung's RFE#4670071
               // However, we cannot wait(0), since it is possible that 2 threads will try to load
               // classes with different classloaders, so one will enter, the other wait, but
               // since they're using different classloaders, nobody will wake up the waiting one.
               // So we wait for some time and then try again.
               try {cl.wait(137);}
               catch (InterruptedException ignored) {}
            }

            // A notifyAll() has been issued, all threads will accumulate here
            // and only one at a time will pass, exactly equal to the barrier
            // before the 'while' statement (dropping sink).
            // Must be outside the synchronized block on the classloader to avoid that
            // waiting on the reentrant lock will hold the lock on the classloader
            try
            {
               reentrantLock.acquire();
            }
            catch (InterruptedException ignored)
            {
            }
         }
      }
      catch(InterruptedException ignored)
      {
      }
      finally
      {
         // I must keep track of the threads that entered, also of the reentrant ones,
         // see unsynchronize()
         increaseThreadsCount();

         // I release the lock, allowing another thread to enter.
         // This new thread will not be allowed and will wait() on the classloader object,
         // releasing its lock.
         reentrantLock.release();

         // Restore the interrupted state of the thread
         if( threadWasInterrupted )
            Thread.currentThread().interrupt(); 
      }
   }

   private void unsynchronize(ClassLoader cl)
   {
      // Save and clear the interrupted state of the incoming thread
      boolean threadWasInterrupted = Thread.currentThread().interrupted();
      try
      {
         reentrantLock.acquire();

         // Reset the current thread only if we're not reentrant
         if (decreaseThreadsCount() == 0)
         {
            setCurrentThread(null);
         }
      }
      catch (InterruptedException ignored)
      {
      }
      finally
      {
         reentrantLock.release();

         // Notify all threads waiting on this classloader
         // This notification must be after the reentrantLock's release() to avoid this scenario:
         // - Thread A is loading a class in the ULR
         // - Thread B triggers a loadClassInternal which locks the UCL
         // - Thread A calls unsynchronize, locks the reentrantLock and waits to acquire the lock on the UCL
         // - Thread B calls synchronize and waits to lock the reentrantLock
         synchronized (cl)
         {
            cl.notifyAll();
         }

         // Restore the interrupted state of the thread
         if( threadWasInterrupted )
            Thread.currentThread().interrupt(); 
      }
   }

   private boolean isThreadAllowed(Thread thread)
   {
      synchronized (lock)
      {
         Thread current = getCurrentThread();
         if (current == null)
         {
            // Nobody is running, set this one
            setCurrentThread(thread);
            return true;
         }
         else if (current == thread)
         {
            // Reentrant thread
            return true;
         }
         else
         {
            // Another thread
            return false;
         }
      }
   }

   private void setCurrentThread(Thread t)
   {
      // Must be synchronized to allow other threads to see the variable
      synchronized (lock)
      {
         currentThread = t;
      }
   }

   private Thread getCurrentThread()
   {
      // Must be sinchronized to ensure all threads see the increment of the variable
      synchronized (lock)
      {
         return currentThread;
      }
   }

   private int increaseThreadsCount()
   {
      // Must be sinchronized to ensure all threads see the increment of the variable
      synchronized (lock)
      {
         ++threadsCount;
         return getThreadsCount();
      }
   }

   private int decreaseThreadsCount()
   {
      // Must be sinchronized to ensure all threads see the increment of the variable
      synchronized (lock)
      {
         --threadsCount;
         return getThreadsCount();
      }
   }

   private int getThreadsCount()
   {
      synchronized (lock)
      {
         return threadsCount;
      }
   }

   /** Parse a class name into its package prefix
    */
   private String getPackageName(String className)
   {
      int index = className.lastIndexOf('.');
      String pkgName = className;
      if( index > 0 )
         pkgName = className.substring(0, index);
      return pkgName;
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
      return packagesMap.containsKey(getPackageName(className));
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
      String pkgName = getPackageName(name);
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

      try
      {
         synchronize(cl);

         // This syncronized block is necessary to synchronize on the cache
         // Looking for resources requires iterating on classloaders, we synchronize
         // to ensure that nobody removes or adds a classloader in the meanwhile
         synchronized (this)
         {
            // First ask the cache (of the calling classloader)
            URL resource = getResourceFromCache(name, cl);

            // The resource was already loaded by the calling classloader, we're done
            if (resource != null) {return resource;}

            // Not found in cache, ask the calling classloader
            resource = getResourceFromClassLoader(name, cl);

            // The calling classloader sees the resource, we're done
            if (resource != null) {return resource;}

            // Not visible by the calling classloader, iterate on the other classloaders
            resource = getResourceFromRepository(name, cl);

            // Some other classloader sees the resource, we're done
            if (resource != null) {return resource;}
         }
      }
      finally
      {
         unsynchronize(cl);
      }

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

   /**
    * Iterates through the current class loaders and tries to find the
    * given class name.
    * @return the Class object for name if found, null otherwise.
    *
    * @jmx.managed-operation
    */
   public Class findClass(String name)
   {
      // We have to find the class as a resource as we don't want to invoke
      // loadClass(name) and cause the side-effect of loading new classes.
      String classRsrcName = name.replace('.', '/') + ".class";

      // I have to synchronize as I will iterate on the classloaders
      // This avoid someone else calls add/removeClassLoader while I iterate here
      synchronized (this)
      {
         for (Iterator iter = classLoaders.iterator(); iter.hasNext();)
         {
            ClassLoader cl = (ClassLoader)iter.next();
            URL classURL = cl.getResource(classRsrcName);
            if (classURL != null)
            {
               try
               {
                  // Since the class was found we can load it which should be a noop
                  Class cls = cl.loadClass(name);
                  return cls;
               }
               catch (ClassNotFoundException e)
               {
                  log.debug("Failed to load class: " + name, e);
               }
            }
         }
      }

      return null;
   }

   /**
    * Obtain a listing of the URL for all UnifiedClassLoaders associated with
    * the ServiceLibraries
    */
   public URL[] getURLs()
   {
      HashSet classpath = new HashSet();

      // I have to synchronize as I will iterate on the classloaders
      // This avoid someone else calls add/removeClassLoader while I iterate here
      synchronized (this)
      {
         for (Iterator iter = classLoaders.iterator(); iter.hasNext();)
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
      }

      return (URL[])classpath.toArray(new URL[classpath.size()]);
   }

   // LoaderRepository overrides ------------------------------------

   public Class loadClass(String className) throws ClassNotFoundException
   {
      // if someone comes to us directly through LoaderRepository interface
      // notice that UCL loaders will skip this and invoke
      // loadClass(name, resolve, cl) directly
      ClassLoader scl = Thread.currentThread().getContextClassLoader();
      Class clazz = null;
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
      return clazz;
   }

   public Class loadClassWithout(ClassLoader loader, String className) throws ClassNotFoundException
   {
      throw new Error("NYI");
   }


   public void addClassLoader(ClassLoader loader)
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

   public boolean addClassLoaderURL(ClassLoader cl, URL url)
   {
      // This is a noop here
      return false;
   }

   private void addUnifiedClassLoader(UnifiedClassLoader cl)
   {
      try
      {
         synchronize(cl);

         // This method must be synchronized on 'this', as the classloaders cannot be added while loading a class
         // See also removeClassLoader and loadClass(String, boolean, ClassLoader) comments
         synchronized (this)
         {
            cl.setRepository(this);
            // See if this URL already exists
            URL url = cl.getURL();
            boolean exists = classLoaderURLs.contains(url);
            // If already present will not be added
            boolean added = false;
            if(!exists)
            {
               added = classLoaders.add(cl);
               classLoaderURLs.add(url);
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
      }
      finally
      {
         unsynchronize(cl);
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
      try
      {
         synchronize(cl);

         // This method must be synchronized on this, as the classloaders cannot be removed while loading a class
         // See also addClassLoader and loadClass(String, boolean, ClassLoader) comments
         synchronized (this)
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
            // There is no global cache for resources
            if (loaderToResourcesMap.containsKey(cl))
            {
               HashMap resources = (HashMap)loaderToResourcesMap.remove(cl);
            }

            // Clean up the package name to class loader mapping
            String[] pkgNames = (String[]) loaderToPackagesMap.get(cl);
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
      finally
      {
         unsynchronize(cl);
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

   private synchronized long getNextSequenceNumber()
   {
      return sequenceNumber++;
   }


   // EDU.oswego.cs.dl.util.concurrent.ReentrantLock;
   // I copied this class here from Doug Lea's concurrent package to avoid
   // dependencies of the JMX implementation to the concurrent package
   public static class ReentrantLock
   {
      private int holds;
      private Thread owner;

      public void acquire() throws InterruptedException
      {
         if (Thread.interrupted()) throw new InterruptedException();
         Thread caller = Thread.currentThread();
         synchronized (this)
         {
            if (caller == owner)
            {
               ++holds;
            }
            else
            {
               try
               {
                  while (owner != null)
                  {
                     wait();
                  }
                  owner = caller;
                  holds = 1;
               }
               catch (InterruptedException ex)
               {
                  notify();
                  throw ex;
               }
            }
         }
      }

      public synchronized void release()
      {
         Thread t = Thread.currentThread();
         if ( t != owner )
         {
            throw new Error("Illegal Lock usage, t="+t+", owner="+owner);
         }

         if (--holds == 0)
         {
            owner = null;
            notify();
         }
      }

      public synchronized int holds()
      {
         if (Thread.currentThread() != owner)
         {
            return 0;
         }
         return holds;
      }
   }
}
