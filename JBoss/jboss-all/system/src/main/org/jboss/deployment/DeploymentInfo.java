/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.deployment;

import java.io.File;
import java.io.FileInputStream;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Date;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Iterator;
import java.util.List;
import java.util.Set;
import java.util.jar.JarFile;
import java.util.jar.Manifest;

import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.jboss.logging.Logger;
import org.jboss.mx.loading.UnifiedClassLoader;
import org.jboss.mx.loading.LoaderRepositoryFactory;
import org.jboss.mx.loading.LoaderRepositoryFactory.LoaderRepositoryConfig;
import org.jboss.util.file.Files;

import org.w3c.dom.Document;

/**
 * Service Deployment Info .
 *
 * Every deployment (even the J2EE ones) should be seen at some point as
 * Service Deployment info
 *
 * @see org.jboss.system.Service
 *
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:David.Maplesden@orion.co.nz">David Maplesden</a>
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @author <a href="mailto:daniel.schulze@telkel.com">Daniel Schulze</a>
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @author <a href="mailto:scott.stark@jboss.org">Scott Stark</a>
 * @version   $Revision: 1.14.2.14 $ <p>
 */
public class DeploymentInfo
{
   private static final Logger log = Logger.getLogger(DeploymentInfo.class);

   // Variables ------------------------------------------------------------

   /** The initial construction timestamp */
   public Date date = new Date();

   /** the URL identifing this SDI **/
   public URL url;
   /** An optional URL to a local copy of the deployment */
   public URL localUrl;
   /** The URL used to watch for changes when the deployment is unpacked */
   public URL watch;
   /** The suffix of the deployment url */
   public String shortName;
   /** The last system time the deployment inited by the MainDeployer */
   public long lastDeployed = 0;
   /** use for "should we redeploy failed" */
   public long lastModified = 0;

   // A free form status for the "state" can be Deployed/failed etc etc
   public String status;
   /** The current state of the deployment */
   public DeploymentState state = DeploymentState.CONSTRUCTED;
   /** The subdeployer that handles the deployment */
   public SubDeployer deployer;

   /** Unified CL is a global scope class loader **/
   public UnifiedClassLoader ucl;

   /** local Cl is a CL that is used for metadata loading, if ejb-jar.xml is
    left in the parent CL through old deployments, this makes ensures that we
    use the local version. You must use the URLClassLoader.findResource method
    to restrict loading to the deployment URL.
    */
   public URLClassLoader localCl;

   /** The classpath declared by this xml descriptor, needs <classpath> entry **/
   final public Collection classpath = new ArrayList();

   // The mbeans deployed
   final public List mbeans = new ArrayList();

   // Anyone can have subdeployments
   final public Set subDeployments = new HashSet();

   // And the subDeployments have a parent
   public DeploymentInfo parent = null;

   /** the web root context in case of war file */
   public String webContext;

   /** the manifest entry of the deployment (if any)
   *  manifest is not serializable ... is only needed
   *  at deployment time, so we mark it transient
   */
   public Manifest manifest;

   // Each Deployment is really mapping one to one to a XML document, here in its parsed form
   public Document document;

   /** We can hold "typed" metadata, really an interpretation of the bare XML document */
   public Object metaData;
   /** Is this a stand-alone service descriptor */
   public boolean isXML;
   public boolean isScript;
   /** Does the deployment url point to a directory */
   public boolean isDirectory;

   /**
    * The variable <code>deployedObject</code> can contain the MBean that
    * is created through the deployment.  for instance, deploying an ejb-jar
    * results in an EjbModule mbean, which is stored here.
    */
   public ObjectName deployedObject;
   /** The configuration of the loader repository for this deployment */
   public LoaderRepositoryConfig repositoryConfig;

   private MBeanServer server;

   public DeploymentInfo(final URL url, final DeploymentInfo parent, final MBeanServer server)
      throws DeploymentException
   {
      this.server = server;
      // The key url the deployment comes from
      this.url = url;

      // this may be changed by deployers in case of directory and xml file following
      this.watch = url;

      // Whether we are part of a subdeployment or not
      this.parent = parent;

      // Is it a directory?
      if (url.getProtocol().startsWith("file") && new File(url.getFile()).isDirectory())
         this.isDirectory = true;

      // Does it even exist?
      if (!isDirectory)
      {
         try
         {
            url.openStream().close();
         }
         catch (Exception e)
         {
            throw new DeploymentException("url " + url + " could not be opened, does it exist?");
         }
      }

      if (parent != null)
      {
         parent.subDeployments.add(this);
         // Use the repository of our topmost parent
         repositoryConfig = getTopRepositoryConfig();
      }

      // The "short name" for the deployment http://myserver/mybean.ear should yield "mybean.ear"
      shortName = getShortName(url.getFile());
      // Do we have an XML descriptor only deployment?
      isXML = shortName.toLowerCase().endsWith("xml");
      isScript = shortName.toLowerCase().endsWith("bsh");
   }

   public MBeanServer getServer()
   {
      return server;
   }
   public void setServer(MBeanServer server)
   {
      this.server = server;
   }

   /** Create a UnifiedClassLoader for the deployment that loads from
    the localUrl and uses its parent deployments url as its orignal
    url. Previously xml descriptors simply used the TCL but since the UCLs
    are now registered as mbeans each must be unique.
    */
   public void createClassLoaders() throws Exception
   {
      // create a local classloader for local files, don't go with the UCL for ejb-jar.xml
      if( localCl == null )
         localCl = new URLClassLoader(new URL[] {localUrl});

      /* Walk the deployment tree to find the parent deployment and obtain its
       url to use as our URL from which this deployment unit originated. This
       is used to determine permissions using the original URL namespace.
       Also pick up the LoaderRepository from the topmost ancestor.
      */
      URL origUrl = url;
      DeploymentInfo current = this;
      while (current.parent != null)
      {
         current = current.parent;
      }
      origUrl = current.url;
      repositoryConfig = current.repositoryConfig;
      if( parent == null )
      {
         if( repositoryConfig == null )
            repositoryConfig = new LoaderRepositoryConfig();
         // Make sure the specified LoaderRepository exists.
         LoaderRepositoryFactory.createLoaderRepository(server, repositoryConfig);
         log.debug("createLoaderRepository from config: "+repositoryConfig);
         // the classes are passed to a UCL that will share the classes with the whole base
         Object[] args = {localUrl, origUrl, Boolean.TRUE};
         String[] sig = {"java.net.URL", "java.net.URL", "boolean"};
         ucl = (UnifiedClassLoader) server.invoke(repositoryConfig.repositoryName,
            "newClassLoader",args, sig);
      }
      else
      {
         // Add a reference to the LoaderRepository
         LoaderRepositoryFactory.createLoaderRepository(server, repositoryConfig);
         // Add the deployment URL to the parent UCL
         ucl = parent.ucl;
         ucl.addURL(localUrl);
      }
      // Add any library jars seen before the UCL was created
      if( classpath.size() > 0 )
      {
         Iterator jars = classpath.iterator();
         while( jars.hasNext() )
         {
            URL jar = (URL) jars.next();
            ucl.addURL(jar);
         }
      }
   }

   /** Set the UnifiedLoaderRepository info for the deployment. This can only
    * be called for the parent deployment, and must be done early in the
    * Subdeployer init(DeploymentInfo) method prior to any class loading.
    * @param repositoryClassName the name of the UnifiedLoaderRepository instance
    * to use.
    * @param repositoryName the JMX name of the repository which must be unique
    * @throws Exception
    */
   public void setRepositoryInfo(LoaderRepositoryConfig config)
       throws Exception
   {
      if( parent != null )
      {
         log.warn("Only the root deployment can set the loader repository, "
            +  "ingoring config="+config);
         return;
      }

      this.repositoryConfig = config;
      // Recreate the ucl if it exists
      if( ucl != null )
      {
         ucl.unregister();
         // Make sure the specified LoaderRepository exists.
         LoaderRepositoryFactory.createLoaderRepository(server, repositoryConfig);
         log.debug("createLoaderRepository from config: "+repositoryConfig);
         // the classes are passed to a UCL that will share the classes with the whole base
         Object[] args = {localUrl, url, Boolean.TRUE};
         String[] sig = {"java.net.URL", "java.net.URL", "boolean"};
         ucl = (UnifiedClassLoader) server.invoke(repositoryConfig.repositoryName,
            "newClassLoader",args, sig);
      }
   }

   /** All library jars referenced through either the manifest references
    * or sar classpaths are added to the root DeploymentInfo class loader. This
    * is neccessary to avoid IllegalAccessErrors due to classes in a pkg
    * being split across jars
    */
   public void addLibraryJar(URL libJar)
   {
      DeploymentInfo current = this;
      while (current.parent != null)
      {
         current = current.parent;
      }
      /* If the UCL exists add the jar to it else use the classpath to
      indicate that the jars need to be added when the ucl is created.
      */
      if( current.ucl != null )
         current.ucl.addURL(libJar);
      else
         classpath.add(libJar);
   }

   /** The the class loader repository name of the top most DeploymentInfo
    */
   public LoaderRepositoryConfig getTopRepositoryConfig()
   {
      LoaderRepositoryConfig topConfig = repositoryConfig;
      DeploymentInfo info = this;
      while( info.parent != null )
      {
         info = info.parent;
         topConfig = info.repositoryConfig;
      }
      return topConfig;
   }

   /**
    * getManifest returns (if present) the deployment's manifest
    * it is lazy loaded to work from the localURL
    */
   public Manifest getManifest()
   {
      try
      {
         if (manifest == null)
         {
            File file = new File(localUrl.getFile());

            if (file.isDirectory())
            {
               FileInputStream fis = new FileInputStream(new File(file, "META-INF/MANIFEST.MF"));
               manifest = new Manifest(fis);
               fis.close();
            }
            else // a jar
               manifest = new JarFile(file).getManifest();

         }

         return manifest;
      }
      // It is ok to barf at any time in the above, means no manifest
      catch (Exception ignored) { return null;}
   }

   public void cleanup()
   {
      // Remove the deployment UCL
      if (ucl != null)
         ucl.unregister();
      ucl = null;

      // Remove any deployment specific repository
      if ( repositoryConfig != null )
      {
         LoaderRepositoryFactory.destroyLoaderRepository(server,
            repositoryConfig.repositoryName);
      }

      subDeployments.clear();
      mbeans.clear();
      if (localUrl == null || localUrl.equals(url))
      {
         log.debug("Not deleting localUrl, it is null or not a copy: " + localUrl);
      }
      else if (Files.delete(localUrl.getFile()))
      {
         log.debug("Cleaned Deployment: " + localUrl);
      }
      else
      {
         log.warn("Could not delete " + localUrl + " restart will delete it");
      }

      // Clean up references to other objects
      localUrl = null;
      repositoryConfig = null;
      watch = null;
      parent = null;
      manifest = null;
      document = null;
      metaData = null;
      server = null;
      classpath.clear();
      state = DeploymentState.DESTROYED;
      // Who is using this after clear?
      // deployer = null;
   }

   public int hashCode()
   {
      return url.hashCode();
   }

   public boolean equals(Object other)
   {
      if (other instanceof DeploymentInfo)
      {
         return ((DeploymentInfo) other).url.equals(this.url);
      }
      return false;
   }

   public String toString()
   {
      StringBuffer s = new StringBuffer(super.toString());
      s.append(" { url=" + url + " }\n");
      s.append("  deployer: " + deployer + "\n");
      s.append("  status: " + status + "\n");
      s.append("  state: " + state + "\n");
      s.append("  watch: " + watch + "\n");
      s.append("  lastDeployed: " + lastDeployed + "\n");
      s.append("  lastModified: " + lastModified + "\n");
      s.append("  mbeans:\n");
      for (Iterator i = mbeans.iterator(); i.hasNext(); )
      {
         ObjectName o = (ObjectName)i.next();
         try
         {
            String state = (String)server.getAttribute(o, "StateString");
            s.append("    " + o + " state: " + state + "\n");
         }
         catch (Exception e)
         {
            s.append("    " + o + " (state not available)\n");
         } // end of try-catch

      } // end of for ()

      return s.toString();
   }

   public static String getShortName(String name)
   {
      if (name.endsWith("/")) name = name.substring(0, name.length() - 1);

      name = name.substring(name.lastIndexOf("/") + 1);
      return name;
   }
}
