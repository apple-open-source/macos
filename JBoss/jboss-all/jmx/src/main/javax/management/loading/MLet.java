/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.loading;


import javax.management.MBeanRegistration;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.ServiceNotFoundException;

import java.net.URL;
import java.net.URLStreamHandlerFactory;
import java.net.URLClassLoader;
import java.net.MalformedURLException;

import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.BufferedReader;
import java.io.IOException;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Iterator;
import java.util.NoSuchElementException;
import java.util.Set;
import java.util.StringTokenizer;
import java.util.List;

import java.text.ParseException;

import org.jboss.logging.Logger;
import org.jboss.mx.loading.MBeanFileParser;
import org.jboss.mx.loading.MLetParser;
import org.jboss.mx.loading.MBeanElement;
import org.jboss.mx.loading.LoaderRepository;
import org.jboss.mx.loading.UnifiedClassLoader;
import org.jboss.mx.server.ServerConstants;
import org.jboss.mx.util.MBeanInstaller;

/**
 * URL classloader capable of parsing an MLet text file adhering to the file
 * format defined in the JMX specification (v1.0).
 *
 * @see javax.management.loading.MLetMBean
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.10.4.4 $  
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>20020313 Juha Lindfors:</b>
 * <ul>
 * <li> Added MLet text file ARG tag support </li>
 * </ul>
 *
 * <p><b>20020317 Juha Lindfors:</b>
 * <ul>
 * <li> Unified Loader Repository support </li>
 * 
 * <li> We need to intercept addURL() call since MLet CL exposes this as a
 *      public method (unlike URL CL). This means the set of URLs in this
 *      classloaders scope may change after it has been registered to the
 *      repository. </li>
 *
 * <li> We override loadClass() to delegate class loading directly to the
 *      repository in case a Unified Loader Repository is installed. This means
 *      in case of ULR this classloader is never used to load classes. The ULR
 *      has a set of Unified CLs that match the URLs added to this CL. </li>
 * </ul>
 *
 * <p><b>20020501 Fusayuki Minamoto:</b>
 * <ul>
 * <li> Added MLet text file VERSION tag support </li>
 * </ul>
 */
public class MLet 
   extends URLClassLoader 
   implements MLetMBean, MBeanRegistration
{

   // FIXME: (RI javadoc) Note -  The MLet class loader uses the DefaultLoaderRepository
   //        to load classes that could not be found in the loaded jar files.
   //
   // IOW we need to override findClass for this cl...
   // I think we can avoid the ugly dlr field hack from RI

   
   // Attributes ----------------------------------------------------
   /** Reference to the MBean server this loader is registered to. */
   private MBeanServer server    = null;
   /** Object name of this loader. */
   private ObjectName objectName = null;
   /** MBean installer based on MLet version. */
   private MBeanInstaller installer = null;
   
   /** 
    * A delegate classloader in the loader repository
    * used when we've been added to the unified loader repository
    */
   private UnifiedClassLoader ucl = null;

   // Static --------------------------------------------------------

   private static final Logger log = Logger.getLogger(MLet.class);

   /**
    * Is the classloader repository some form of JBoss unified loader repository
    */
   private static final boolean isUnifiedRepository;

   // Determine whether we are doing Unified Repository Support
   static
   {
      if (System.getProperty(ServerConstants.LOADER_REPOSITORY_CLASS_PROPERTY).equals(
            ServerConstants.UNIFIED_LOADER_REPOSITORY_CLASS))
         isUnifiedRepository = true;
      else
         isUnifiedRepository = false;
   }

   // Constructors --------------------------------------------------
   public MLet()
   {
      super(new URL[0], Thread.currentThread().getContextClassLoader());
   }

   public MLet(URL[] urls)
   {
      super(urls, Thread.currentThread().getContextClassLoader());
   }

   public MLet(URL[] urls, ClassLoader parent)
   {
      super(urls, parent);
   }

   public MLet(URL[] urls, ClassLoader parent, URLStreamHandlerFactory factory)
   {
      super(urls, parent, factory);
   }

   // MBeanRegistration implementation ------------------------------
   public ObjectName preRegister(MBeanServer server, ObjectName name) throws  Exception
   {
      if (name == null)
         name = new ObjectName(":type=MLet");

      this.objectName = name;
      this.server     = server;
      this.installer  = new MBeanInstaller(server, this, name);

      return name;
   }

   public void postRegister(Boolean registrationDone)
   {
      if (registrationDone.booleanValue() == false)
         tidyUp();
      else
      {
         // FIXME: 3.2 does not follow the spec, the MLET is not added to the loader
         // repository by the registry, do it here.
         LoaderRepository ulr = LoaderRepository.getDefaultLoaderRepository();
         if (isUnifiedRepository)
         {
            try
            {
               URL[] urls = super.getURLs();
               if (urls.length != 0)
               {
                  ucl = ulr.newClassLoader(urls[0], true);
                  for (int i = 1; i < urls.length; ++i)
                     ulr.addClassLoaderURL(ucl, urls[i]);
               }
            }
            catch (Exception e)
            {
               log.warn("Failed to create classloader for MLet", e);
            }
         }
         else
            ulr.addClassLoader(this);
      }
   }

   public void preDeregister() throws Exception
   {
      // FIXME: 3.2 does not follow the spec, the MLET is not removed from the loader
      // repository by the registry, do it here.
      LoaderRepository ulr = LoaderRepository.getDefaultLoaderRepository();
      if (isUnifiedRepository)
      {
         ulr.removeClassLoader(ucl);
         ucl = null;
      }
      else
      {
         ulr.removeClassLoader(this);
      }
   }

   public void postDeregister()
   {
      tidyUp();
   }

   // MLetMBean implementation --------------------------------------
   public Set getMBeansFromURL(String url) throws ServiceNotFoundException
   {
      try
      {
         return getMBeansFromURL(new URL(url));
      }
      catch (MalformedURLException e) 
      {
         throw new ServiceNotFoundException("Malformed URL:" + url);
      }
   }

   public Set getMBeansFromURL(URL url) throws ServiceNotFoundException
   {
      if (server == null)
         throw new ServiceNotFoundException("Loader must be registered to the server before loading the MBeans.");

      HashSet mbeans        = new HashSet();
      MBeanElement element  = null;

      try 
      {
         MBeanFileParser parser = new MLetParser();
         Set mlets              = parser.parseMBeanFile(url);
         
         if (mlets.size() == 0)
            throw new ServiceNotFoundException("The specified URL '" + url + "' does not contain MLET tags.");
            
         Iterator it = mlets.iterator();
         while (it.hasNext())
         {
            element = (MBeanElement)it.next();
            String codebase = element.getCodebase();
            
            // if no codebase is specified then the url of the mlet text file is used
            if (codebase == null)
               codebase = url.toString().substring(0, url.toString().lastIndexOf('/'));

            String codebaseURL = null;

            List archives  = element.getArchives();

            for (int i = 0; i <archives.size(); i++)
            {
               try
               {
                  codebaseURL = codebase + ((codebase.endsWith("/")) ? "" : "/") + archives.get(i);
                  addURL(new URL(codebaseURL));
               }
               catch (MalformedURLException e)
               {
                  log.error("MLET ERROR: malformed codebase URL: '" + codebaseURL + "'");
               }
            }
               
            try
            {
               // FIXME: see the note at the beginning... we use an explicit loader
               //        in the createMBean() call to force this classloader to
               //        be used first to load all MLet classes. Normally this form
               //        of createMBean() call will not delegate to DLR even though
               //        the javadoc requires it. Therefore the findClass() should
               //        be overridden to delegate to the repository. 
               /*
               mbeans.add(server.createMBean(
                     element.getCode(),
                     (element.getName() != null) ? new ObjectName(element.getName()) : null,
                     objectName,
                     element.getConstructorValues(),
                     element.getConstructorTypes())
               );
               */

               // installer creates or upgrades this mbean based on the MLet version
               mbeans.add(installer.installMBean(element));
            }
            catch (Throwable t)
            {
               // if mbean can't be created, throwable is added to the return set
               mbeans.add(t);

               log.error("MLET ERROR: can't create MBean: ", t);
            }
         }
      }
      catch (ParseException e)
      {
         throw new ServiceNotFoundException(e.getMessage());
      }

      return mbeans;
   }

   public void addURL(URL url)
   {
      if (isUnifiedRepository && server != null)
      {

         // since we don't have the URLs til getMBeansFromURL() is called we
         // need to add these UCLs late, after the MBean registration
         LoaderRepository ulr = LoaderRepository.getDefaultLoaderRepository();
         try
         {
            if (ucl == null)
               ucl = ulr.newClassLoader(url, true);

            ucl.addURL(url);
         }
         catch (Exception e)
         {
            // REVIEW: Adrian - throw a runtime exception?
            log.warn("Failed to add url " + url, e);
         }
      }

      else
      {
         // will probably override to add individual URL CL to repository as well
         // in the same style as with ULR. This would allow findClass() to safely
         // delegate to the BLR without having to deal with infinite looping.
         //
         // ... or use loadClassWithout(cl)   [JPL]
         super.addURL(url);
      }
   }

   public void addURL(String url) throws ServiceNotFoundException
   {
      try
      {
         this.addURL(new URL(url));
      }
      catch (MalformedURLException e)
      {
         throw new ServiceNotFoundException("Malformed URL: " + url);
      }
   }

   public String getLibraryDirectory()
   {
      // FIXME
      throw new Error("NYI");
   }

   public void setLibraryDirectory(String libdir)
   {
      // FIXME
      throw new Error("NYI");
   }

   // Classloader overrides -----------------------------------------
   public Class loadClass(String name, boolean resolve) throws ClassNotFoundException
   {
      if (isUnifiedRepository && ucl != null)
      {
         // if its ULR we can safely delegate the load to it because only a single
         // definition of a given class exists in the repository. This cl has
         // conflicting definitions and therefore we skip it altogether.
         return ucl.loadClass(name, resolve);
      }

      else
      {
         // with BLR multiple class definitions by different classloaders can
         // exist... therefore try loadClass with this CL first, if it fails
         // delegate to loader repository
         return super.loadClass(name, resolve);
      }
   }

   private void tidyUp()
   {
      objectName = null;
      server = null;
      installer = null;
   }
}

