/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.deployment;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Iterator;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;

import org.jboss.metadata.MetaData;
import org.jboss.metadata.XmlFileLoader;
import org.jboss.mx.loading.LoaderRepositoryFactory;
import org.jboss.mx.loading.LoaderRepositoryFactory.LoaderRepositoryConfig;
import org.jboss.util.file.JarUtils;

import org.w3c.dom.Element;

/**
 * Enterprise Archive Deployer.
 *
 * @jmx:mbean name="jboss.j2ee:service=EARDeployer"
 *            extends="org.jboss.deployment.SubDeployerMBean"
 *
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.19.2.10 $
 */
public class EARDeployer
   extends SubDeployerSupport
   implements EARDeployerMBean
{
   public EARDeployer()
   {
      super();
   }
   
   public boolean accepts(DeploymentInfo di) 
   {
      String urlStr = di.url.getFile();
      return urlStr.endsWith("ear") || urlStr.endsWith("ear/");
   }
   
   public void init(DeploymentInfo di)
      throws DeploymentException
   {
      try
      {
         log.info("Init J2EE application: " + di.url);

         InputStream in = di.localCl.getResourceAsStream("META-INF/application.xml");
         if( in == null )
            throw new DeploymentException("No META-INF/application.xml found");

         /* Don't require validation of application.xml since an ear may
         just contain a jboss sar specified in the jboss-app.xml descriptor.
         */
         XmlFileLoader xfl = new XmlFileLoader(false);
         Element root = xfl.getDocument(in, "META-INF/application.xml").getDocumentElement();
         J2eeApplicationMetaData metaData = new J2eeApplicationMetaData(root);
         di.metaData = metaData;
         in.close();

         // Check for a jboss-app.xml descriptor
         in = di.localCl.getResourceAsStream("META-INF/jboss-app.xml");
         if( in != null )
         {
            Element jbossApp = xfl.getDocument(in, "META-INF/jboss-app.xml").getDocumentElement();
            in.close();
            // Import module/service archives to metadata
            metaData.importXml(jbossApp, true);
            // Check for a loader-repository for scoping
            Element loader = MetaData.getOptionalChild(jbossApp, "loader-repository");
            initLoaderRepository(di, loader);
         }

         // resolve the watch
         if (di.url.getProtocol().equals("file"))
         {
            File file = new File(di.url.getFile());
            
            // If not directory we watch the package
            if (!file.isDirectory())
            {
               di.watch = di.url;
            }
            // If directory we watch the xml files
            else
            {
               di.watch = new URL(di.url, "META-INF/application.xml");
            }
         }
         else
         {
            // We watch the top only, no directory support
            di.watch = di.url;
         }
         
         // Obtain the sub-deployment list
         File parentDir = null;
         HashMap extractedJars = new HashMap();

         if (di.isDirectory) 
         {
            parentDir = new File(di.localUrl.getFile());
         } 
         else
         {
            /* Extract each entry so that deployment modules can be processed
             and any manifest entries referenced by the ear modules are located
             in the same unpacked directory structure.
            */
            String urlPrefix = "jar:" + di.localUrl + "!/";
            JarFile jarFile = new JarFile(di.localUrl.getFile());
            // For each entry, test if deployable, if so
            // extract it and store the related URL in map
            for (Enumeration e = jarFile.entries(); e.hasMoreElements();)
            {
               JarEntry entry = (JarEntry)e.nextElement();
               String name = entry.getName();
               try 
               {
                  URL url = new URL(urlPrefix + name);
                  if (isDeployable(name, url))
                  {
                     // Obtain a jar url for the nested jar
                    URL nestedURL = JarUtils.extractNestedJar(url, this.tempDeployDir);
                    // and store in it in map
                    extractedJars.put(name, nestedURL);
                  }
               }
               catch (MalformedURLException mue)
               {
                  log.warn("Jar entry invalid. Ignoring: " + name, mue);
               }
               catch (IOException ex)
               {
                  log.warn("Failed to extract nested jar. Ignoring: " + name, ex);
               }
            }
         }

         // Create subdeployments for the ear modules
         for (Iterator iter = metaData.getModules(); iter.hasNext(); )
         {
            J2eeModuleMetaData mod = (J2eeModuleMetaData)iter.next();
            String fileName = mod.getFileName();
            if (fileName != null && (fileName = fileName.trim()).length() > 0)
            {
               DeploymentInfo sub = null;
               if (di.isDirectory)
               {
                  File f = new File(parentDir, fileName);
                  sub = new DeploymentInfo(f.toURL(), di, getServer());
               }
               else
               {
                  // The nested jar url was placed into extractedJars above
                  URL nestedURL = (URL) extractedJars.get(fileName);
                  if( nestedURL == null )
                     throw new DeploymentException("Failed to find module file: "+fileName);
                  sub = new DeploymentInfo(nestedURL, di, getServer());
               }
               // Set the context-root on web modules
               if( mod.isWeb() )
                  sub.webContext = mod.getWebContext();
               log.debug("Deployment Info: " + sub + ", isDirectory: " + sub.isDirectory);
            }
         }
      }
      catch (DeploymentException e)
      {
         throw e;
      }
      catch (Exception e)
      {
         throw new DeploymentException("Error in accessing application metadata: ", e);
      }

      super.init(di);
   }
   
   public void start(DeploymentInfo di)
      throws DeploymentException
   {
      super.start (di);
      log.info ("Started J2EE application: " + di.url);
   }


   /**
    * Describe <code>destroy</code> method here.
    *
    * @param di a <code>DeploymentInfo</code> value
    * @exception DeploymentException if an error occurs
    */
   public void destroy(DeploymentInfo di) throws DeploymentException
   {
      log.info("Undeploying J2EE application, destroy step: " + di.url);
      super.destroy(di);
   }

   /** Build the ear scoped repository
    *
    * @param di the deployment info passed to deploy
    * @param loader the jboss-app/loader-repository element
    * @throws Exception
    */
   protected void initLoaderRepository(DeploymentInfo di, Element loader)
      throws Exception
   {
      if( loader == null )
         return;

      LoaderRepositoryConfig config = LoaderRepositoryFactory.parseRepositoryConfig(loader);
      di.setRepositoryInfo(config);
   }

   /**
    * Add -ds.xml and -service.xml as legitimate deployables.
    */
   protected boolean isDeployable(String name, URL url)
   {
      return super.isDeployable(name, url) || name.endsWith("-ds.xml") ||
         name.endsWith("-service.xml");
   }

   /** Override the default behavior of looking into the archive for deployables
    * as only those explicitly listed in the application.xml and jboss-app.xml
    * should be deployed.
    *
    * @param di
    */
   protected void processNestedDeployments(DeploymentInfo di)
   {
   }
}
