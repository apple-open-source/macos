/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.loading;

import java.net.URL;
import java.util.List;
import java.util.Iterator;
import javax.management.AttributeNotFoundException;
import javax.management.InstanceNotFoundException;
import javax.management.MBeanException;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.ReflectionException;

import org.jboss.logging.Logger;
import org.jboss.mx.server.ServerConstants;

/** A simple extension of UnifiedLoaderRepository3 that adds the notion of a
 * parent UnifiedLoaderRepository. Classes and resources are loaded from child
 * first and then the parent depending on the java2ParentDelegaton flag.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class HeirarchicalLoaderRepository4 extends UnifiedLoaderRepository4
{
   // Attributes ----------------------------------------------------
   private static final Logger log = Logger.getLogger(HeirarchicalLoaderRepository3.class);
   private static ObjectName DEFAULT_LOADER_NAME;

   static
   {
      try
      {
         DEFAULT_LOADER_NAME = new ObjectName(ServerConstants.DEFAULT_LOADER_NAME);
      }
      catch(Exception e)
      {
         log.error("Failed to initialize default loader name", e);
      }
   }

   /** A ClassLoader override that prevents a child class loader from looking
    * beyond its URLs for classes.
    */
   static class NoParentClassLoader extends ClassLoader
   {
      protected synchronized Class loadClass(String name, boolean resolve)
            throws ClassNotFoundException
      {
         throw new ClassNotFoundException("NoParentClassLoader has no classed");
      }

      protected Class findClass(String name) throws ClassNotFoundException
      {
         throw new ClassNotFoundException("NoParentClassLoader has no classed");
      }
   }
   static class UClWrapper extends UnifiedClassLoader4
   {
      HeirarchicalLoaderRepository4 ulr;
      UClWrapper(HeirarchicalLoaderRepository4 ulr)
      {
         super(null, null, new NoParentClassLoader(), ulr);
         this.ulr = ulr;
      }

      protected Class findClass(String name) throws ClassNotFoundException
      {
         Class clazz = null;
         UnifiedClassLoader4 ucl = ulr.internalGetClassLoader(name);
         UnifiedClassLoader4 parentUCL = null;
         try
         {
            if( ucl != null )
               clazz = ucl.findClass(name);
            else
            {
               parentUCL = ulr.internalGetParentClassLoader();
               parentUCL.findClass(name);
            }
         }
         catch(ClassNotFoundException e)
         {
            if( parentUCL != null )
               clazz = parentUCL.loadClass(name);
            else
               throw e;
         }
         return clazz;
      }
   }

   /** The repository to which we delegate if requested classes or resources
    are not available from this repository.
    */
   private UnifiedLoaderRepository4 parentRepository;
   /** A flag indicating if the standard parent delegation loading where the
    parent repository is used before this repository.
    */
   private boolean java2ParentDelegaton;

   public HeirarchicalLoaderRepository4(MBeanServer server)
      throws AttributeNotFoundException, InstanceNotFoundException, MBeanException, ReflectionException
   {
      this(server, DEFAULT_LOADER_NAME);
   }
   public HeirarchicalLoaderRepository4(MBeanServer server, ObjectName parentName)
      throws AttributeNotFoundException, InstanceNotFoundException, MBeanException, ReflectionException
   {
      this.parentRepository = (UnifiedLoaderRepository4) server.getAttribute(parentName,
                    "Instance");
   }
   public HeirarchicalLoaderRepository4(UnifiedLoaderRepository4 parentRepository)
   {
      this.parentRepository = parentRepository;
   }

   // Public --------------------------------------------------------

   public UnifiedClassLoader newClassLoader(final URL url, boolean addToRepository)
      throws Exception
   {
      UnifiedClassLoader4 ucl = null;
      if( java2ParentDelegaton == false )
         ucl = new UnifiedClassLoader4(url, null, new NoParentClassLoader(), this);
      else
         ucl = new UnifiedClassLoader4(url, null, this);

      if( addToRepository )
      {
         this.registerClassLoader(ucl);
      }
      return ucl;
   }
   public UnifiedClassLoader newClassLoader(final URL url, final URL origURL, boolean addToRepository)
      throws Exception
   {
      UnifiedClassLoader4 ucl = null;
      if( java2ParentDelegaton == false )
         ucl = new UnifiedClassLoader4(url, origURL, new NoParentClassLoader(), this);
      else
         ucl = new UnifiedClassLoader4(url, origURL, this);

      if( addToRepository )
      {
         this.registerClassLoader(ucl);
      }
      return ucl;
   }

   /** Get the use parent first flag. This indicates whether the parent
    * repository is consulted first for resource and class loading or if the
    * HeirchicalLoaderRepository is consulted first.
    *
    * @return true if the parent repository is consulted first, false if the
    * HeirchicalLoaderRepository is consulted first.
    */
   public boolean getUseParentFirst()
   {
      return java2ParentDelegaton;
   }
   /** Set the use parent first flag. This indicates whether the parent
    * repository is consulted first for resource and class loading or if the
    * HeirchicalLoaderRepository is consulted first.
    *
    * @param flag true if the parent repository is consulted first, false if the
    * HeirchicalLoaderRepository is consulted first.
    */
   public void setUseParentFirst(boolean flag)
   {
      java2ParentDelegaton = flag;
   }

   /** Load a class using the repository class loaders.
    *
    * @param name The name of the class
    * @param resolve If <code>true</code>, the class will be resolved
    * @param scl The asking class loader
    * @return The loaded class
    * @throws ClassNotFoundException If the class could not be found.
    */
   public Class loadClass(String name, boolean resolve, ClassLoader scl)
      throws ClassNotFoundException
   {
      Class foundClass = null;

      if( java2ParentDelegaton == true )
      {
         try
         {
            // Try the parent repository first
            foundClass = parentRepository.loadClass(name, resolve, scl);
         }
         catch(ClassNotFoundException e)
         {
            // Next try our repository
            if( foundClass == null )
               foundClass = super.loadClass(name, resolve, scl);
         }
      }
      else
      {
         try
         {
            // Try this repository first
            foundClass = super.loadClass(name, resolve, scl);
         }
         catch(ClassNotFoundException e)
         {
            // Next try our parent repository
            if( foundClass == null )
               foundClass = parentRepository.loadClass(name, resolve, scl);
         }
      }

      if( foundClass != null )
         return foundClass;

      /* If we reach here, all of the classloaders currently in the VM don't
         know about the class
      */
      throw new ClassNotFoundException(name);
   }

   /** Find a resource from this repository. This first looks to this
    * repository and then the parent repository.
    * @param name The name of the resource
    * @param scl The asking class loader
    * @return An URL for reading the resource, or <code>null</code> if the
    *          resource could not be found.
    */
   public URL getResource(String name, ClassLoader scl)
   {
      URL resource = null;

      if( java2ParentDelegaton == true )
      {
         // Try this repository
         resource = parentRepository.getResource(name, scl);
         // Next try our parent repository
         if( resource == null )
            resource = super.getResource(name, scl);
      }
      else
      {
         // Try this repository
         resource = super.getResource(name, scl);
         // Next try our parent repository
         if( resource == null )
            resource = parentRepository.getResource(name, scl);
      }

      return resource;
   }

   /** Find all resource URLs for the given name. This is entails an
    * exhuastive search of this and the parent repository and is an expensive
    * operation.
    *
    * @param name the resource name
    * @param cl the requesting class loader
    * @param urls a list into which the located resource URLs will be placed
    */
   public void getResources(String name, ClassLoader cl, List urls)
   {
      if( java2ParentDelegaton == true )
      {
         // Get the parent repository resources
         parentRepository.getResources(name, cl, urls);
         // Next get this repositories resources
         super.getResources(name, cl, urls);
      }
      else
      {
         // Get this repositories resources
         super.getResources(name, cl, urls);
         // Next get the parent repository resources
         parentRepository.getResources(name, cl, urls);
      }
   }

   /** Obtain a listing of the URLs for all UnifiedClassLoaders associated with
    *the repository
    */
   public URL[] getURLs()
   {
      URL[] ourURLs = super.getURLs();
      URL[] parentURLs = parentRepository.getURLs();
      int size = ourURLs.length + parentURLs.length;
      URL[] urls = new URL[size];
      System.arraycopy(ourURLs, 0, urls, 0, ourURLs.length);
      System.arraycopy(parentURLs, 0, urls, ourURLs.length, parentURLs.length);
      return urls;
   }

   /** Called by LoadMgr to locate a previously loaded class. This looks
    * first to this repository and then the parent repository.
    *@return the cached class if found, null otherwise
    */
   public Class loadClassFromCache(String name)
   {
      Class foundClass = null;

      if( java2ParentDelegaton == true )
      {
         // Try this repository
         foundClass = parentRepository.loadClassFromCache(name);
         // Next try our parent repository
         if( foundClass == null )
            foundClass = super.loadClassFromCache(name);
      }
      else
      {
         // Try this repository
         foundClass = super.loadClassFromCache(name);
         /* We do not try the parent repository cache as this does not allow
         the child repository to override classes in the parent
         */
      }
      return foundClass;
   }

   /** Called by LoadMgr to obtain all class loaders. This returns a set of
    * PkgClassLoader with the HeirarchicalLoaderRepository3 ordered ahead of
    * the parent repository pkg class loaders
    *@return UnifiedClassLoader4 for name
    */
   public UnifiedClassLoader4 getClassLoader(String name)
   {
      return new UClWrapper(this);
   }

   UnifiedClassLoader4 internalGetClassLoader(String name)
   {
      UnifiedClassLoader4 thisUCL = super.getClassLoader(name);
      UnifiedClassLoader4 ucl = thisUCL;

      if( ucl == null )
      {
         UnifiedClassLoader4 parentUCL = parentRepository.getClassLoader(name);
         ucl = parentUCL;
      }
      return ucl;
   }
   UnifiedClassLoader4 internalGetParentClassLoader()
   {
      UnifiedClassLoader4 ucl = null;
      Iterator iter = parentRepository.getClassLoaders().iterator();
      if( iter.hasNext() )
         ucl = (UnifiedClassLoader4) iter.next();
      else
      {
         ClassLoader loader = getClass().getClassLoader();
         if( loader instanceof UnifiedClassLoader4 )
            ucl = (UnifiedClassLoader4) loader;
         else
            ucl = new UnifiedClassLoader4(null, null, loader, this);
      }
      return ucl;
   }
}
