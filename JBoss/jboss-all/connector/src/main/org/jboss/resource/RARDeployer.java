/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.resource;

import java.io.File;
import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;
import javax.management.ObjectName;
import org.jboss.deployment.DeploymentException;
import org.jboss.deployment.DeploymentInfo;
import org.jboss.deployment.SubDeployerSupport;
import org.jboss.metadata.XmlFileLoader;
import org.jboss.system.ServiceControllerMBean;
import org.jboss.mx.util.MBeanProxyExt;
import org.w3c.dom.Document;
import org.w3c.dom.Element;

/**
 * Service that deploys ".rar" files containing resource adapters. Deploying
 * the RAR file is the first step in making the resource adapter available to
 * application components; once it is deployed, one or more connection
 * factories must be configured and bound into JNDI, a task performed by the
 * <code>ConnectionFactoryLoader</code> service.
 *
 * @jmx:mbean
 *      name="jboss.jca:service=RARDeployer"
 *      extends="org.jboss.deployment.SubDeployerMBean"
 *
 * @author     Toby Allsopp (toby.allsopp@peace.com)
 * @author     <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version    $Revision: 1.31.2.3 $
 * @see        org.jboss.resource.ConnectionFactoryLoader <p>
 */
public class RARDeployer
      extends SubDeployerSupport
      implements RARDeployerMBean
{
   /** A proxy to the ServiceController. */
   private ServiceControllerMBean serviceController;

   /**
    *  Gets the DeployableFilter attribute of the RARDeployer object
    *
    * @return    The DeployableFilter value
    */
   public boolean accepts(DeploymentInfo sdi)
   {
      String urlStr = sdi.url.toString();
      return urlStr.endsWith("rar") || urlStr.endsWith("rar/");
   }

   /**
    * Once registration has finished, create a proxy to the ServiceController
    * for later use.
    */
   public void postRegister(Boolean done)
   {
      super.postRegister(done);

      serviceController = (ServiceControllerMBean)
            MBeanProxyExt.create(ServiceControllerMBean.class,
                  ServiceControllerMBean.OBJECT_NAME,
                  server);
   }

   // RARDeployerMBean implementation -------------------------------

   public void init(DeploymentInfo rdi)
         throws DeploymentException
   {
      URL raUrl = rdi.localCl.findResource("META-INF/ra.xml");

      Document dd = XmlFileLoader.getDocument(raUrl);
      Element root = dd.getDocumentElement();
      RARMetaData metadata = new RARMetaData();
      metadata.importXml(root);

      metadata.setClassLoader(rdi.ucl);
      rdi.metaData = metadata;

      // resolve the watch
      if (rdi.url.getProtocol().equals("file"))
      {
         File file = new File(rdi.url.getFile());

         // If not directory we watch the package
         if (!file.isDirectory())
            rdi.watch = rdi.url;

         // If directory we watch the xml files
         else
         {
            try
            {
               rdi.watch = new URL(rdi.url, "META-INF/ra.xml");
            }
            catch (MalformedURLException mue)
            {
               throw new DeploymentException("Could not watch ra.xml file", mue);
            } // end of try-catch
         } // end of else
      }
      else
      {
         // We watch the top only, no directory support
         rdi.watch = rdi.url;
      }
      // invoke super-class initialization
      super.init(rdi);
   }

   /**
    * The <code>deploy</code> method deploys a rar at the given url.
    *
    * @param url The <code>URL</code> location of the rar to deploy.
    * @return an <code>Object</code> to identify this deployment.
    * @exception IOException if an error occurs
    * @exception DeploymentException if an error occurs
    */
   public void create(DeploymentInfo rdi)
         throws DeploymentException
   {
      log.debug("Attempting to deploy RAR at '" + rdi.url + "'");

      try
      {
         RARMetaData metaData = (RARMetaData) rdi.metaData;

         //set up the RARDeployment mbean for dependency management.
         rdi.deployedObject =
               new ObjectName("jboss.jca:service=RARDeployment,name=" + metaData.getDisplayName());
         server.createMBean("org.jboss.resource.RARDeployment",
               rdi.deployedObject,
               new Object[]{metaData},
               new String[]{"org.jboss.resource.RARMetaData"});

         serviceController.create(rdi.deployedObject);

      }
      catch (Exception e)
      {
         throw new DeploymentException(e);
      }
      super.create(rdi);
   }

   public void start(DeploymentInfo rdi) throws DeploymentException
   {
      try
      {
         serviceController.start(rdi.deployedObject);
      }
      catch (Exception e)
      {
         throw new DeploymentException(e);
      }
      super.start(rdi);
   }

   public void stop(DeploymentInfo rdi)
         throws DeploymentException
   {
      if (log.isDebugEnabled())
      {
         log.debug("Undeploying RAR at '" + rdi.url + "'");
      }

      try
      {
         log.info("About to undeploy RARDeploymentMBean, objectname: " + rdi.deployedObject);
         serviceController.stop(rdi.deployedObject);
      }
      catch (Exception e)
      {
         throw new DeploymentException(e);
      }
      super.stop(rdi);
   }

   public void destroy(DeploymentInfo rdi)
         throws DeploymentException
   {
      try
      {
         serviceController.destroy(rdi.deployedObject);
         serviceController.remove(rdi.deployedObject);
      }
      catch (Exception e)
      {
         throw new DeploymentException(e);
      }

      ((RARMetaData) rdi.metaData).setClassLoader(null);
      super.destroy(rdi);
   }
}
