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
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.JarURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLConnection;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;

import javax.management.Notification;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.system.server.ServerConfig;
import org.jboss.system.server.ServerConfigLocator;
import org.jboss.util.file.JarUtils;
import org.jboss.mx.util.MBeanProxyExt;
import org.jboss.util.stream.Streams;

/**
 * An abstract {@link SubDeployer}.
 *
 * Provides registration with {@link MainDeployer} as well as
 * implementations of init, create, start, stop and destroy that
 * generate JMX notifications on completion of the method.
 *
 * @version <tt>$Revision: 1.8.2.10 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author  Scott.Stark@jboss.org
 */
public abstract class SubDeployerSupport
   extends ServiceMBeanSupport
   implements SubDeployer, SubDeployerMBean
{
   /**
    * Holds the native library <em>suffix</em> for this system.
    *
    * <p>
    * Determined by examining the result of System.mapLibraryName(specialToken).
    * The special token defaults to "XxX", but can be changed by setting the
    * system property: <tt>org.jboss.deployment.SubDeployerSupport.nativeLibToken</tt>.
    */
   protected static final String nativeSuffix;

   /**
    * Holds the native library <em>prefix</em> for this system.
    *
    * @see #nativeSuffix
    */
   protected static final String nativePrefix;

   /** A proxy to the MainDeployer. */
   protected MainDeployerMBean mainDeployer;

   /** The temporary directory into which deployments are unpacked */
   protected File tempDeployDir;

   /** The temporary directory where native libs are unpacked. */
   private File tempNativeDir;

   /**
    * The <code>createService</code> method is one of the ServiceMBean lifecyle operations.
    * (no jmx tag needed from superinterface)
    * @exception Exception if an error occurs
    */
   protected void createService() throws Exception
   {
      // watch the deploy directory, it is a set so multiple adds
      // (start/stop) only one entry is present
      // get the temporary directory to use

      ServerConfig config = ServerConfigLocator.locate();
      File basedir = config.getServerTempDir();

      // ${jboss.server.home.dir}/tmp/native
      tempNativeDir = new File(basedir, "native");
      // ${jboss.server.home.dir}/tmp/deploy
      tempDeployDir = new File(basedir, "deploy");

      // Setup the proxy to mainDeployer
      mainDeployer = (MainDeployerMBean)
         MBeanProxyExt.create(MainDeployerMBean.class,
                           MainDeployerMBean.OBJECT_NAME,
                           server);
   }

   /**
    * Performs SubDeployer registration.
    */
   protected void startService() throws Exception
   {
      // Register with the main deployer
      mainDeployer.addDeployer(this);
   }

   /**
    * Performs SubDeployer deregistration.
    */
   protected void stopService() throws Exception
   {
      // Unregister with the main deployer
      mainDeployer.removeDeployer(this);
   }

   /**
    * Clean up.
    */
   protected void destroyService() throws Exception
   {
      // Help the GC
      mainDeployer = null;
      tempNativeDir = null;
   }

   /**
    * Sub-classes should override this method to provide
    * custom 'init' logic.
    *
    * <p>This method calls the processNestedDeployments(di) method and then
    * issues a JMX notification of type SubDeployer.INIT_NOTIFICATION.
    * This behaviour can overridden by concrete sub-classes.  If further
    * initialization needs to be done, and you wish to preserve the
    * functionality, be sure to call super.init(di) at the end of your
    * implementation.
    */
   public void init(DeploymentInfo di) throws DeploymentException
   {
      processNestedDeployments(di);
      // Issue the SubDeployer.INIT_NOTIFICATION
      Notification msg = new Notification(SubDeployer.INIT_NOTIFICATION,
         this, getNextNotificationSequenceNumber());
      msg.setUserData(di);
      sendNotification(msg);
   }

   /**
    * Sub-classes should override this method to provide
    * custom 'create' logic.
    *
    * This method issues a JMX notification of type SubDeployer.CREATE_NOTIFICATION.
    */
   public void create(DeploymentInfo di) throws DeploymentException
   {
      // Issue the SubDeployer.CREATE_NOTIFICATION
      Notification msg = new Notification(SubDeployer.CREATE_NOTIFICATION,
         this, getNextNotificationSequenceNumber());
      msg.setUserData(di);
      sendNotification(msg);
   }

   /**
    * Sub-classes should override this method to provide
    * custom 'start' logic.
    *
    * This method issues a JMX notification of type SubDeployer.START_NOTIFICATION.
    */
   public void start(DeploymentInfo di) throws DeploymentException
   {
      // Issue the SubDeployer.START_NOTIFICATION
      Notification msg = new Notification(SubDeployer.START_NOTIFICATION,
         this, getNextNotificationSequenceNumber());
      msg.setUserData(di);
      sendNotification(msg);
   }

   /**
    * Sub-classes should override this method to provide
    * custom 'stop' logic.
    *
    * This method issues a JMX notification of type SubDeployer.START_NOTIFICATION.
    */
   public void stop(DeploymentInfo di) throws DeploymentException
   {
      // Issue the SubDeployer.START_NOTIFICATION
      Notification msg = new Notification(SubDeployer.STOP_NOTIFICATION,
         this, getNextNotificationSequenceNumber());
      msg.setUserData(di);
      sendNotification(msg);
   }

   /**
    * Sub-classes should override this method to provide
    * custom 'destroy' logic.
    *
    * This method issues a JMX notification of type SubDeployer.DESTROY_NOTIFICATION.
    */
   public void destroy(DeploymentInfo di) throws DeploymentException
   {
      // Issue the SubDeployer.DESTROY_NOTIFICATION
      Notification msg = new Notification(SubDeployer.DESTROY_NOTIFICATION,
         this, getNextNotificationSequenceNumber());
      msg.setUserData(di);
      sendNotification(msg);
   }

   /**
    * The <code>processNestedDeployments</code> method searches for any nested and
    * deployable elements.  Only Directories and Zipped archives are processed,
    * and those are delegated to the addDeployableFiles and addDeployableJar
    * methods respectively.  This method can be overridden for alternate
    * behaviour.
    */
   protected void processNestedDeployments(DeploymentInfo di) throws DeploymentException
   {
      log.debug("looking for nested deployments in : " + di.url);
      if (di.isXML)
      {
         // no nested archives in an xml file
         return;
      }

      if (di.isDirectory)
      {
         File f = new File(di.url.getFile());
         if (!f.isDirectory())
         {
            // something is screwy
            throw new DeploymentException
               ("Deploy file incorrectly reported as a directory: " + di.url);
         }

         addDeployableFiles(di, f);
      }
      else
      {
         try
         {
            // Obtain a jar url for the nested jar
            URL nestedURL = JarUtils.extractNestedJar(di.localUrl, this.tempDeployDir);
            JarFile jarFile = new JarFile(nestedURL.getFile());
            addDeployableJar(di, jarFile);
         }
         catch (Exception e)
         {
            log.warn("Failed to add deployable jar: " + di.localUrl, e);

            //
            // jason: should probably throw new DeploymentException
            //        ("Failed to add deployable jar: " + jarURLString, e);
            //        rather than make assumptions to what type of deployable
            //        file this was that failed...
            //

            return;
         }
      }
   }

   /**
    * This method returns true if the name is a recognized archive file.
    * This can be overridden for alternate behaviour.
    *
    * (David Jencks) This method should never have been written or used. It makes the deployment scheme non-extensible.
    *
    * @param name The "short-name" of the URL.  It will have any trailing '/'
    *        characters removed, and any directory structure has been removed.
    * @param url The full url.
    *
    * @return true iff the name ends in a known archive extension: jar, sar,
    *         ear, rar, zip, wsr, war, or if the name matches the native
    *         library conventions.
    *
    * @todo Find an extensible way of deciding if a package is
    * deployable based on what deployers exist.
    */
   protected boolean isDeployable(String name, URL url)
   {
      return name.endsWith(".jar")
         || name.endsWith(".sar")
         || name.endsWith(".ear")
         || name.endsWith(".rar")
         || name.endsWith(".zip")
         || name.endsWith(".wsr")
         || name.endsWith(".war")
         || name.endsWith(".bsh")
         || name.endsWith("-ds.xml")
         || name.endsWith(".last")
         || (name.endsWith("-service.xml") && url.getPath().indexOf("META-INF") == -1)
         || (name.endsWith(nativeSuffix) && name.startsWith(nativePrefix));
   }

   /**
    * This method recursively searches the directory structure for any files
    * that are deployable (@see isDeployable).  If a directory is found to
    * be deployable, then its subfiles and subdirectories are not searched.
    *
    * @param di the DeploymentInfo
    * @param dir The root directory to start searching.
    */
   protected void addDeployableFiles(DeploymentInfo di, File dir)
      throws DeploymentException
   {
      File[] files = dir.listFiles();
      for (int i = 0; i < files.length; i++)
      {
         File file = files[i];
         String name = file.getName();
         try
         {
            URL url = file.toURL();
            if (isDeployable(name, url))
            {
               deployUrl(di, url, name);
               // we don't want deployable units processed any further
               continue;
            }
         }
         catch (MalformedURLException e)
         {
            log.warn("File name invalid; ignoring: " + file, e);
         }
         if (file.isDirectory())
         {
            addDeployableFiles(di, file);
         }
      }
   }

   /**
    * This method searches the entire jar file for any deployable files
    * (@see isDeployable).
    *
    * @param di the DeploymentInfo
    * @param jarFile the jar file to process.
    */
   protected void addDeployableJar(DeploymentInfo di, JarFile jarFile)
      throws DeploymentException
   {
      String urlPrefix = "jar:"+di.localUrl.toString()+"!/";
      for (Enumeration e = jarFile.entries(); e.hasMoreElements();)
      {
         JarEntry entry = (JarEntry)e.nextElement();
         String name = entry.getName();
         try
         {
            URL url = new URL(urlPrefix+name);
            if (isDeployable(name, url))
            {
               // Obtain a jar url for the nested jar
               URL nestedURL = JarUtils.extractNestedJar(url, this.tempDeployDir);
               deployUrl(di, nestedURL, name);
            }
         }
         catch (MalformedURLException mue)
         {
            //
            // jason: why are we eating this exception?
            //
            log.warn("Jar entry invalid; ignoring: " + name, mue);
         }
         catch (IOException ex)
         {
            log.warn("Failed to extract nested jar; ignoring: " + name, ex);
         }
      }
   }

   protected void deployUrl(DeploymentInfo di, URL url, String name)
      throws DeploymentException
   {
      log.info("nested deployment: " + url);
      try
      {
         //
         // jason: need better handling for os/arch specific libraries
         //        should be able to have multipule native libs in an archive
         //        one for each supported platform (os/arch), we only want to
         //        load the one for the current platform.
         //
         //        This probably means explitly listing the libraries in a
         //        deployment descriptor, which could probably also be used
         //        to explicitly map the files, as it might be possible to
         //        share a native lib between more than one version, no need
         //        to duplicate the file, metadata can be used to tell us
         //        what needs to be done.
         //
         //        Also need this mapping to get around the different values
         //        which are used by vm vendors for os.arch and such...
         //

         if (name.endsWith(nativeSuffix) && name.startsWith(nativePrefix))
         {
            File destFile = new File(tempNativeDir, name);
            log.info("Loading native library: " + destFile.toString());

            File parent = destFile.getParentFile();
            if (!parent.exists()) {
               parent.mkdirs();
            }

            InputStream in = url.openStream();
            OutputStream out = new FileOutputStream(destFile);
            Streams.copyb(in, out);

            out.flush();
            out.close();
            in.close();

            System.load(destFile.toString());
         }
         else
         {
            DeploymentInfo sub = new DeploymentInfo(url, di, getServer());
         }
      }
      catch (Exception ex)
      {
         throw new DeploymentException
            ("Could not deploy sub deployment "+name+" of deployment "+di.url, ex);
      }
   }

   /////////////////////////////////////////////////////////////////////////
   //                     Class Property Configuration                    //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Static configuration properties for this class.  Allows easy access
    * to change defaults with system properties.
    */
   protected static class ClassConfiguration
      extends org.jboss.util.property.PropertyContainer
   {
      private String nativeLibToken = "XxX";

      public ClassConfiguration()
      {
         // properties will be settable under our enclosing classes group
         super(SubDeployerSupport.class);

         // bind the properties & the access methods
         bindMethod("nativeLibToken");
      }

      public void setNativeLibToken(final String token)
      {
         this.nativeLibToken = token;
      }

      public String getNativeLibToken()
      {
         return nativeLibToken;
      }
   }

   /** The singleton class configuration object for this class. */
   protected static final ClassConfiguration CONFIGURATION = new ClassConfiguration();

   //
   // jason: the following needs to be done after setting up the
   //        class config reference, so it is moved it down here.
   //

   /**
    * Determine the native library suffix and prefix.
    */
   static
   {
      // get the token to use from config, incase the default needs
      // to be changed to resolve problem with a specific platform
      String token = CONFIGURATION.getNativeLibToken();

      // then determine what the prefix and suffixes are for this platform
      String nativex = System.mapLibraryName(token);
      int xPos = nativex.indexOf(token);
      nativePrefix = nativex.substring(0, xPos);
      nativeSuffix = nativex.substring(xPos + 3);
   }
}
