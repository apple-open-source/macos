/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.varia.deployment;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import java.net.URL;
import java.net.MalformedURLException;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.List;
import java.util.ListIterator;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Map;
import java.util.jar.JarFile;
import java.util.jar.JarEntry;

import javax.management.MBeanServer;
import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;

import org.jboss.deployment.DeploymentException;
import org.jboss.deployment.DeploymentInfo;
import org.jboss.deployment.MainDeployerMBean;
import org.jboss.deployment.SubDeployer;
import org.jboss.deployment.SubDeployerSupport;

import org.jboss.system.ServiceControllerMBean;
import org.jboss.system.server.ServerConfig;
import org.jboss.system.server.ServerConfigLocator;

import org.jboss.util.Counter;
import org.jboss.util.file.Files;
import org.jboss.util.file.JarUtils;
import org.jboss.mx.util.MBeanProxyExt;

import org.jboss.varia.deployment.convertor.Convertor;

/**
 * This is the deployer for other vendor's applications
 * with dynamic migration of vendor-specific DDs to
 * JBoss specific DDs.
 *
 * @see org.jboss.varia.deployment.convertor.Convertor
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.3.2.6 $
 *
 * @jmx.mbean
 *    name="jboss.system:service=ServiceDeployer"
 *    extends="org.jboss.deployment.SubDeployerMBean"
 */
public class FoeDeployer
   extends SubDeployerSupport
   implements SubDeployer, FoeDeployerMBean
{
   // Attributes ----------------------------------------------------
   /** A proxy to the ServiceControllerDeployer. */
   private ServiceControllerMBean serviceController;

   /** The deployers scratch directory. */
   private File scratchDirectory;
   
   /** Contains the list of available converters */
   private List converterList = new ArrayList();
   
   /** an increment for tmp files */
   private final Counter id = Counter.makeSynchronized(new Counter(0));

   /** map of exploaded deployment destionation in scratch directoy by DeploymentInfo */
   private ThreadLocal destinationByDI = new ThreadLocal() {
      protected Object initialValue()
      {
         return new HashMap();
      }
   };

   // SubDeployerSupport overrides ----------------------------------
   /**
    * Returns true if the there is a converter available to convert
    * the deployment unit.
    *
    * @jmx.managed-operation
    */
   public boolean accepts(DeploymentInfo di)
   {
      // delegate accepts to convertors
      Iterator i = converterList.iterator();
      while(i.hasNext())
      {
         Convertor converter = (Convertor)i.next();
         if(converter.accepts(di.url))
         {
            return true;
         }
      }
      return false;
   }

   /**
    * Returns true if the there is a converter available to convert
    * the deployment unit.
    */
   public boolean accepts(URL url)
   {
      // delegate accepts to convertors
      Iterator i = converterList.iterator();
      while(i.hasNext())
      {
         Convertor converter = (Convertor)i.next();
         if(converter.accepts(url))
         {
            return true;
         }
      }
      return false;
   }

   /**
    * At the init phase the deployment unit and its subdeployment units are unpacked.
    * @jmx.managed-operation
    */
   public void init(DeploymentInfo di)
      throws DeploymentException
   {
      // Determine the destination for unpacking and save it in the ThreadLocal
      Map destinations = (Map)destinationByDI.get();
      File destination = (File)destinations.get(di.parent);
      if(destination == null)
      {
         // Loop until for new destination
         while(destination == null || destination.exists())
            destination = new File(scratchDirectory, id.increment() + "." + di.shortName);
      }
      else
      {
         destination = new File(destination, di.shortName);
      }
      destinations.put(di, destination);
      destinationByDI.set(destinations);

      try
      {
         log.debug("unpacking to " + destination);
         inflateJar(di.localUrl, destination);
      }
      catch(Exception e)
      {
         throw new DeploymentException("Unpacking failed: ", e);
      }

      // invoke super class' initialization
      super.init(di);
   }
   
   /**
    * At the create phase, the conversion and packing is done.
    * @jmx.managed-operation
    */
   public void create(DeploymentInfo di)
      throws DeploymentException
   {
      try
      {
         // fetch the destionation of unpacked deployment from ThreadLocal
         Map destinations = (Map)destinationByDI.get();
         File inflateDest = (File)destinations.get(di);

         // Look for the converter that accepts vendor specific deployment descriptors
         // and let it convert them
         Iterator i = converterList.iterator();
         while(i.hasNext())
         {
            Convertor converter = (Convertor)i.next();
            if(converter.accepts(di.url))
            {
               // Convert them to JBoss specific DDs
               converter.convert(di, inflateDest);
               // Now conversion is done and we can leave
               break;
            }
         }

         // deflate
         File deflateDest = (File)destinations.get(di.parent);
         if(deflateDest == null)
            deflateDest = scratchDirectory;
         String validName;
         if(di.shortName.endsWith(".wl"))
            validName = di.shortName.substring(0, di.shortName.length()-3);
         else
            validName = di.shortName.substring( 0, di.shortName.length() - 4 ) + "jar";
         File convertedUnit = new File(deflateDest, validName);
         log.debug("deflating to " + convertedUnit);
         deflateJar(convertedUnit, inflateDest);

         // remove unpacked deployment unit
         Files.delete(inflateDest);

         // copy the converted app back to the deployment directory
         if(di.parent == null)
            copyFile(convertedUnit, new File(di.url.getFile()).getParentFile());
      }
      catch(Exception e)
      {
         log.error("Conversion error: ", e);
      }
   }

   /**
    * This method stops this deployment because it is not of any
    * use anymore (conversion is done)
    * @jmx.managed-operation
    */
   public void start(DeploymentInfo di)
      throws DeploymentException
   {
      stop(di);
      destroy(di);
   }

   /**
    * @jmx.managed-operation
    */
   public void stop(DeploymentInfo di)
   {
      log.debug("undeploying application: " + di.url);
   }

   /**
    * @jmx.managed-operation
    */
   public void destroy(DeploymentInfo di)
   {
      List services = di.mbeans;
      int lastService = services.size();
      for(ListIterator i = services.listIterator(lastService); i.hasPrevious();)
      {
         ObjectName name = (ObjectName)i.previous();
         log.debug( "destroying mbean " + name );
         try
         {
            serviceController.destroy(name);
         }
         catch(Exception e)
         {
            log.error("Could not destroy mbean: " + name, e);
         }
      }

      for(ListIterator i = services.listIterator(lastService); i.hasPrevious();)
      {
         ObjectName name = (ObjectName)i.previous();
         log.debug("removing mbean " + name);
         try
         {
            serviceController.remove( name );
         }
         catch(Exception e)
         {
            log.error("Could not remove mbean: " + name, e);
         }
      }
   }

   /**
    * This method is called in SubDeployerSupport.processNestedDeployments()
    * The method is overriden to deploy the deployments acceptable by FoeDeployer only.
    */
   protected void addDeployableJar(DeploymentInfo di, JarFile jarFile)
      throws DeploymentException
   {
      String urlPrefix = "jar:" + di.localUrl.toString() + "!/";
      for(Enumeration e = jarFile.entries(); e.hasMoreElements();)
      {
         JarEntry entry = (JarEntry)e.nextElement();
         String name = entry.getName();
         try
         {
            URL url = new URL(urlPrefix + name);
            if(isDeployable(name, url))
            {
               // Obtain a jar url for the nested jar
               // Append the ".wl" suffix to prevent other than FoeDeployer deployers'
               // attempts to deploy the deployment unit
               URL nestedURL = JarUtils.extractNestedJar(url, this.tempDeployDir);
               File file = new File(nestedURL.getFile());
               File wlFile = new File(nestedURL.getFile() + ".wl");
               file.renameTo(wlFile);

               if(accepts(wlFile.toURL()))
               {
                  deployUrl(di, wlFile.toURL(), name + ".wl");
               }
               else
               {
                  // if the deployment isn't accepted rename it back
                  wlFile.renameTo(new File(nestedURL.getFile()));
               }
            }
         }
         catch(MalformedURLException mue)
         {
            log.warn("Jar entry invalid; ignoring: " + name, mue);
         }
         catch(IOException ex)
         {
            log.warn("Failed to extract nested jar; ignoring: " + name, ex);
         }
      }
   }

   /**
    * The startService method
    * - gets the mbeanProxies for MainDeployer and ServiceController;
    * - creates scratch directory for foe work.
    *
    * @exception Exception if an error occurs
    */
   protected void startService()
      throws Exception
   {
      mainDeployer = (MainDeployerMBean) MBeanProxyExt.create(
         MainDeployerMBean.class,
         MainDeployerMBean.OBJECT_NAME,
         server
      );

      // get the controller proxy
      serviceController = (ServiceControllerMBean) MBeanProxyExt.create(
         ServiceControllerMBean.class,
         ServiceControllerMBean.OBJECT_NAME,
         server
      );

      ServerConfig config = ServerConfigLocator.locate();

      // build the scratch directory
      File tempDirectory = config.getServerTempDir();
      scratchDirectory = new File(tempDirectory, "foe");
      if(!scratchDirectory.exists())
         scratchDirectory.mkdirs();

      // Note: this should go the last.
      // scratch directory must be created before this call
      super.startService();
   }

   /**
    * Returns the ObjectName
    */
   protected ObjectName getObjectName(MBeanServer server, ObjectName name)
      throws MalformedObjectNameException
   {
      return name == null ? OBJECT_NAME : name;
   }

   // FoeDeployerMBean implementation -------------------------------
   /**
    * Add a new conveter to the list. If the same converter is
    * added, this new one won't be added, meaning everything stays the same.
    * This method is normally called by a Converter to be
    * called by this deployer to convert.
    *
    * @param converter New Converter to be added
    *
    * @jmx.managed-operation
    */
   public void addConvertor(Convertor converter)
   {
      converterList.add(converter);

      // try to deploy waiting deployment units
      // note: there is no need to synchronize, because MainDeployer
      // returns a copy of waiting deployments
      Collection waitingDeployments = mainDeployer.listWaitingForDeployer();
      if((waitingDeployments != null) && (waitingDeployments.size() > 0))
      {
         for( Iterator iter = waitingDeployments.iterator(); iter.hasNext(); )
         {
            DeploymentInfo di = (DeploymentInfo)iter.next();

            // check whether the converter accepts the deployment
            if(!converter.accepts(di.url))
               continue;
            
            log.debug("trying to deploy with new converter: " + di.shortName);
            try 
            {
               mainDeployer.redeploy(di);
            }
            catch (DeploymentException e)
            {
               log.error("DeploymentException while trying to deploy a package with new converter", e);
            }
         }
      }
   }

   /**
    * Removes a conveter from the list of converters. If the
    * converter does not exist nothing happens.
    * This method is normally called by a Converter to be removed
    * from the list if not serving anymore.
    *
    * @param converter Conveter to be removed from the list
    *
    * @jmx.managed-operation
    */
   public void removeConvertor(Convertor converter)
   {
      converterList.remove(converter);
   }

   // Private --------------------------------------------------------
   /**
    * The <code>inflateJar</code> copies the jar entries
    * from the jar url jarUrl to the directory destDir.
    *
    * @param fileURL URL pointing to the file to be inflated
    * @param destinationDirectory Directory to which the content shall be inflated to
    *
    * @exception DeploymentException if an error occurs
    * @exception IOException if an error occurs
    */
   protected void inflateJar( URL fileURL, File destinationDirectory )
      throws DeploymentException, IOException
   {
      InputStream input = new FileInputStream(fileURL.getFile());
      JarUtils.unjar(input, destinationDirectory);
      // input is closed in unjar();
   }

   /**
    * Deflate a given directory into a JAR file
    *
    * @param jarFile The JAR file to be created
    * @param root Root directory of the files to be included (this directory
    *             will not be included in the path of the JAR content)
    **/
   private void deflateJar( File jarFile, File root )
      throws Exception
   {
      OutputStream output = new FileOutputStream(jarFile);
      JarUtils.jar(output, root.listFiles(), null, null, null);
      output.close();
   }

   /**
    * Copies the given File to a new destination with the same name
    *
    * @param source The source file to be copied
    * @param destinationDirectory File pointing to the destination directory
    **/
   private void copyFile(File source, File destinationDirectory)
      throws Exception
   {
      File target = new File( destinationDirectory, source.getName() );
      // Move may fail if target is used (because it is deployed)
      // Use Files.copy instead
      Files.copy(source, target);
   }
}
