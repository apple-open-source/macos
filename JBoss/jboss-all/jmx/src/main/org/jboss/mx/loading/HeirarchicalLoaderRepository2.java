/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.loading;

import java.net.URL;
import java.util.HashMap;
import java.util.Iterator;
import javax.management.AttributeNotFoundException;
import javax.management.InstanceNotFoundException;
import javax.management.MBeanException;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.ReflectionException;

import org.jboss.logging.Logger;
import org.jboss.mx.server.ServerConstants;

/**
 * A simple extension of UnifiedLoaderRepository2 that adds the notion of a
 * parent UnifiedLoaderRepository. Classes and resources are loaded from child
 * first and then the parent.
 *
 * @author  <a href="mailto:scott.stark@jboss.org">Scott Stark</a>.
 * @version $Revision: 1.2.4.2 $
 */
public class HeirarchicalLoaderRepository2 extends UnifiedLoaderRepository2
{
   // Attributes ----------------------------------------------------
   private static final Logger log = Logger.getLogger(HeirarchicalLoaderRepository2.class);
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

   /**
    * Maps resource names of resources looked up here to the URLs used to
    * load them.
    */
   private LoaderRepository parentRepository;

   public HeirarchicalLoaderRepository2(MBeanServer server)
      throws AttributeNotFoundException, InstanceNotFoundException, MBeanException, ReflectionException
   {
      this(server, DEFAULT_LOADER_NAME);
   }
   public HeirarchicalLoaderRepository2(MBeanServer server, ObjectName parentName)
      throws AttributeNotFoundException, InstanceNotFoundException, MBeanException, ReflectionException
   {
      this.parentRepository = (LoaderRepository) server.getAttribute(parentName,
                    "Instance");
   }

   // Public --------------------------------------------------------

   /**
    * Add a class to this repository. Allows a class to be added in
    * byte code form stored inside this repository.
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
      
      // Try this repository
      try
      {
         foundClass = super.loadClass(name, resolve, scl);
      }
      catch(ClassNotFoundException e)
      {
         // Next try our parent repository
         if( foundClass == null )
            foundClass = parentRepository.loadClass(name, resolve, scl);
      }
      if( foundClass != null )
         return foundClass;

      // If we reach here, all of the classloaders currently in the VM don't know about the class
      throw new ClassNotFoundException(name);
   }

   /**
    * Find a resource from this repository.
    *
    * @param name The name of the resource
    * @param scl The asking class loader
    * @return An URL for reading the resource, or <code>null</code> if the
    *          resource could not be found.
    */
   public URL getResource(String name, ClassLoader scl)
   {
      URL resource = null;
      
      // Try this repository
      resource = super.getResource(name, scl);
      // Next try our parent repository
      if( resource == null )
         resource = parentRepository.getResource(name, scl);
      
      return resource;
   }

   /** Obtain a listing of the URL for all UnifiedClassLoaders associated with
    *the ServiceLibraries
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
  
}
