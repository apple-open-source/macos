/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: AxisService.java,v 1.25.2.7 2003/11/06 15:36:05 cgjung Exp $

package org.jboss.net.axis.server;

import org.jboss.net.axis.XMLResourceProvider;
import org.jboss.net.axis.AttacheableService;
import org.jboss.net.axis.Deployment;

import org.jboss.net.DefaultResourceBundle;

import org.jboss.deployment.DeploymentException;
import org.jboss.deployment.DeploymentInfo;
import org.jboss.deployment.MainDeployerMBean;
import org.jboss.deployment.SubDeployer;
import org.jboss.deployment.SubDeployerSupport;
import org.jboss.logging.Log4jLoggerPlugin;
import org.jboss.mx.loading.UnifiedClassLoader;
import org.jboss.web.WebApplication;

import org.apache.log4j.Category;
import org.apache.log4j.Priority;

import org.apache.axis.MessageContext;
import org.apache.axis.utils.XMLUtils;
import org.apache.axis.AxisFault;
import org.apache.axis.server.AxisServer;
import org.apache.axis.AxisProperties;
import org.apache.axis.deployment.wsdd.WSDDProvider;
import org.apache.axis.deployment.wsdd.WSDDUndeployment;
import org.apache.axis.client.Service;
import org.apache.axis.EngineConfiguration;
import org.apache.axis.configuration.EngineConfigurationFactoryFinder;
import org.apache.axis.EngineConfigurationFactory;

import org.jboss.naming.Util;
import org.jboss.metadata.MetaData;
import org.jboss.system.server.ServerConfigLocator;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Attr;
import org.w3c.dom.Node;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.NodeList;

import javax.management.MBeanRegistration;
import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectName;
import javax.management.MalformedObjectNameException;

import javax.naming.InitialContext;
import javax.naming.LinkRef;
import javax.naming.Context;
import javax.naming.NamingException;

import java.io.FilenameFilter;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.FileOutputStream;
import java.io.PrintStream;

import java.net.URL;
import java.net.URLClassLoader;
import java.net.MalformedURLException;

import javax.xml.parsers.ParserConfigurationException;

import java.util.Map;
import java.util.Iterator;
import java.util.Collection;

import java.lang.reflect.Proxy;

/**
 * A deployer service that installs Axis and manages Web-Services 
 * within JMX.
 * @created 27. September 2001
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.25.2.7 $
 */

public class AxisService
   extends SubDeployerSupport
   implements AxisServiceMBean, MBeanRegistration
{

   // 
   // Attributes
   //

   /**
    * A map of current deployment names to the
    * wsdd docs that created them.
    */
   protected Map deployments = new java.util.HashMap();

   /** this is where the axis "web-application" has been installed */
   protected DeploymentInfo myDeploymentInfo = null;

   /** the engine belonging to this service */
   protected AxisServer axisServer;

   /** the client configuration belonging to this service*/
   protected XMLResourceProvider clientConfiguration;

   /** the server configuration belonging to this service*/
   protected XMLResourceProvider serverConfiguration;

   /** the web deployer that hosts our servlet */
   protected SubDeployer webDeployer;

   /** the initial context into which we bind external web-service proxies */
   protected InitialContext initialContext;

   //
   // Constructors
   //

   /** default */
   public AxisService()
   {
      // we fake internationalisation if it is not globally catered
      Category cat = ((Log4jLoggerPlugin) log.getLoggerPlugin()).getCategory();
      if (cat.getResourceBundle() == null)
         cat.setResourceBundle(new DefaultResourceBundle());
   }

   //
   // Some helper methods
   //

   /**
    * deploy an external web service reference
    */

   protected synchronized void deployExternalWebService(Element deployElement)
      throws DeploymentException
   {

      try
      {
         if (initialContext == null)
         {
            initialContext = new InitialContext();
         }

         NamedNodeMap attributes = deployElement.getAttributes();

         String jndiName = attributes.getNamedItem("jndiName").getNodeValue();
         String serviceClassName =
            attributes.getNamedItem("serviceImplClass").getNodeValue();
         Object factory =
            new AttacheableService(
               serviceClassName,
               serviceName.getCanonicalName());
         initialContext.bind(jndiName, factory);
      }
      catch (NamingException e)
      {
         throw new DeploymentException(
            "Could not deploy item " + deployElement,
            e);
      }
   }

   /** undeploys an external web service reference */
   protected synchronized void undeployExternalWebService(Element deployElement)
   {
      try
      {
         if (initialContext == null)
         {
            initialContext = new InitialContext();
         }

         NamedNodeMap attributes = deployElement.getAttributes();

         String jndiName = attributes.getNamedItem("jndiName").getNodeValue();

         if (jndiName != null)
         {
            initialContext.unbind(jndiName);
         }
      }
      catch (NamingException e)
      {

         // what?

         ((Log4jLoggerPlugin) log.getLoggerPlugin()).getCategory().l7dlog(
            Priority.WARN,
            "Could not undeploy " + deployElement,
            new Object[0],
            e);
      }
   }

   //----------------------------------------------------------------------------
   // 'service' interface
   //----------------------------------------------------------------------------

   /**
    * start service means
    *  - initialise axis engine
    *  - register Axis servlet in WebContainer
    *  - contact the maindeployer
    */
   protected void startService() throws Exception
   {

      // find the global config file in classpath
      URL resource =
         getClass().getClassLoader().getResource(
            Constants.AXIS_CONFIGURATION_FILE);

      Category cat = ((Log4jLoggerPlugin) log.getLoggerPlugin()).getCategory();
      if (resource == null)
      {
         cat.l7dlog(
            Priority.WARN,
            Constants.COULD_NOT_FIND_AXIS_CONFIGURATION_0,
            new Object[]{Constants.AXIS_CONFIGURATION_FILE},
            null);
         throw new Exception(Constants.COULD_NOT_FIND_AXIS_CONFIGURATION_0);
      }

      serverConfiguration = new XMLResourceProvider(resource);

      axisServer = new AxisServer(serverConfiguration);
      
      // we annotate the server configuration with our serviceName
      serverConfiguration.getGlobalOptions().put(
         org.jboss.net.axis.Constants.CONFIGURATION_CONTEXT,
         serviceName.toString());

      // find the client config file in classpath
      resource =
         getClass().getClassLoader().getResource(
            Constants.AXIS_CLIENT_CONFIGURATION_FILE);

      if (resource == null)
      {
         cat.l7dlog(
            Priority.WARN,
            Constants.COULD_NOT_FIND_AXIS_CONFIGURATION_0,
            new Object[]{Constants.AXIS_CONFIGURATION_FILE},
            null);
         throw new Exception(Constants.COULD_NOT_FIND_AXIS_CONFIGURATION_0);
      }

      clientConfiguration = new XMLResourceProvider(resource);

      clientConfiguration.buildDeployment();

      clientConfiguration.getGlobalOptions().put(
         org.jboss.net.axis.Constants.CONFIGURATION_CONTEXT,
         serviceName.toString());

      // make sure that Axis/Discovery wont register any application classloaders for system lookup
      AxisProperties.getNameDiscoverer();
      // register our client configuration there 
      Class initializeThisFuckingStaticStuff =
         EngineConfigurationFactoryFinder.class;
      System.setProperty(
         EngineConfigurationFactory.SYSTEM_PROPERTY_NAME,
         JMXEngineConfigurationFactory.class.getName());
      super.startService();
   }

   /** what to do to stop axis temporarily --> undeploy the servlet */
   protected void stopService() throws Exception
   {

      super.stopService();

      // tear down all running web services
      //Is this really what you want to do? Not leave services running anyway? 
      for (Iterator apps =
         new java.util.ArrayList(deployments.values()).iterator();
           apps.hasNext();
         )
      {
         DeploymentInfo info = (DeploymentInfo) apps.next();
         try
         {
            //unregister through server so it's bookeeping is up to date.
            server.invoke(
               MainDeployerMBean.OBJECT_NAME,
               "undeploy",
               new Object[]{info},
               new String[]{"org.jboss.deployment.DeploymentInfo"});
         }
         catch (Exception e)
         {
            log.error("Could not undeploy deployment " + info, e);
         }
      }

      axisServer.stop();
      super.stopService();
      myDeploymentInfo = null;
   }

   //----------------------------------------------------------------------------
   // Deployer interface
   //----------------------------------------------------------------------------

   /**
    * Provides a filter that decides whether a file can be deployed by
    * this deployer based on the filename.  This is for the benefit of
    * the {@link org.jboss.deployer.MainDeployer} service.
    *
    * @return a <tt>FilenameFilter</tt> that only
    *         <tt>accept</tt>s files with names that can be
    *         deployed by this deployer
    */
   public boolean accepts(DeploymentInfo sdi)
   {
      if (sdi.shortName.endsWith("-axis.xml")
         || sdi.localCl.getResource(Constants.WEB_SERVICE_DESCRIPTOR) != null)
      {
         return true;
      }
      return false;
   }

   /*
    * Init a deployment
    *
    * parse the XML MetaData.  Init and deploy are separate steps allowing for subDeployment
    * in between.
    *
    * @param url    The URL to deploy.
    *
    * @throws MalformedURLException    Invalid URL
    * @throws IOException              Failed to fetch content
    * @throws DeploymentException      Failed to deploy
    */

   public void init(DeploymentInfo sdi) throws DeploymentException
   {
      super.init(sdi);

      try
      {
         URL metaInfos = null;

         if (sdi.metaData == null)
         {
            metaInfos =
               sdi.localCl.getResource(Constants.WEB_SERVICE_DESCRIPTOR);
         }
         else
         {
            metaInfos = (URL) sdi.metaData;
         }

         sdi.metaData = XMLUtils.newDocument(metaInfos.openStream());

         // Resolve what to watch
         if (sdi.url.getProtocol().equals("file"))
         {
            sdi.watch = metaInfos;
         }
         else
         {
            // We watch the top only, no directory support
            sdi.watch = sdi.url;
         }
      }
      catch (Exception e)
      {
         throw new DeploymentException(e);
      }
   }

   /**
    * Describe <code>create</code> method here.
    *
    * This step should include deployment steps that expose the existence of the unit being 
    * deployed to other units.
    *
    * @param sdi a <code>DeploymentInfo</code> value
    * @exception DeploymentException if an error occurs
    */
   public void create(DeploymentInfo sdi) throws DeploymentException
   {
      ((Log4jLoggerPlugin) log.getLoggerPlugin()).getCategory().l7dlog(
         Priority.DEBUG,
         Constants.ABOUT_TO_CREATE_AXIS_0,
         new Object[]{sdi},
         null);

      if (deployments.containsKey(sdi.url))
      {
         throw new DeploymentException(
            "attempting to redeploy a depoyed module! " + sdi.url);
      }
      else
      {
         deployments.put(sdi.url, sdi);
      }

   }

   /**
    * Describe <code>start</code> method here.
    *
    * This should only include deployment activities that refer to resources
    * outside the unit being deployed.
    *
    * @param sdi a <code>DeploymentInfo</code> value
    * @exception DeploymentException if an error occurs
    */
   public void start(DeploymentInfo sdi) throws DeploymentException
   {
      ((Log4jLoggerPlugin) log.getLoggerPlugin()).getCategory().l7dlog(
         Priority.DEBUG,
         Constants.ABOUT_TO_START_AXIS_0,
         new Object[]{sdi},
         null);

      // remember old classloader
      ClassLoader previous = Thread.currentThread().getContextClassLoader();

      // build new classloader for naming purposes
      URLClassLoader serviceLoader =
         URLClassLoader.newInstance(new URL[0], sdi.ucl);

      try
      {
         InitialContext iniCtx = new InitialContext();
         Context envCtx = null;

         // create a new naming context java:comp/env
         try
         {
            // enter the apartment
            Thread.currentThread().setContextClassLoader(serviceLoader);
            envCtx = (Context) iniCtx.lookup("java:comp");
            envCtx = envCtx.createSubcontext("env");
         }
         finally
         {
            // enter the apartment
            Thread.currentThread().setContextClassLoader(previous);
         }

         Document doc = (Document) sdi.metaData;
         // the original command
         Element root = doc.getDocumentElement();
         // the deployment command document
         Document deployDoc = XMLUtils.newDocument();
         // the client deployment command document
         Document deployClientDoc = XMLUtils.newDocument();
         // create command
         Element deploy =
            deployDoc.createElementNS(root.getNamespaceURI(), "deployment");
         // create command
         Element deployClient =
            deployClientDoc.createElementNS(
               root.getNamespaceURI(),
               "deployment");

         NamedNodeMap attributes = root.getAttributes();
         for (int count = 0; count < attributes.getLength(); count++)
         {
            Attr attribute = (Attr) attributes.item(count);
            deploy.setAttributeNodeNS(
               (Attr) deployDoc.importNode(attribute, true));
            deployClient.setAttributeNodeNS(
               (Attr) deployClientDoc.importNode(attribute, true));
         }

         // and insert the nodes from the original document
         // and sort out the ejb-ref extensions
         NodeList children = root.getChildNodes();
         for (int count = 0; count < children.getLength(); count++)
         {
            Node actNode = children.item(count);
            if (actNode instanceof Element)
            {
               Element actElement = (Element) actNode;

               if (actElement.getTagName().equals("ejb-ref"))
               {

                  String refName =
                     MetaData.getElementContent(
                        MetaData.getUniqueChild(
                           (Element) actNode,
                           "ejb-ref-name"));
                  String linkName =
                     MetaData.getElementContent(
                        MetaData.getUniqueChild((Element) actNode, "ejb-link"));

                  log.warn(
                     "Web Service Deployment "
                     + sdi
                     + " makes use of the deprecated ejb-ref feature. "
                     + "Please adjust any ejb-providing service tag inside your web-service.xml pointing to "
                     + refName
                     + " to use the absolute "
                     + linkName
                     + " instead.");

                  if (refName == null)
                     throw new DeploymentException(
                        Constants.EJB_REF_MUST_HAVE_UNIQUE_NAME);
                  if (linkName == null)
                     throw new DeploymentException(
                        Constants.EJB_REF_MUST_HAVE_UNIQUE_LINK);

                  Util.bind(envCtx, refName, new LinkRef(linkName));
               }
               else if (actElement.getTagName().equals("ext-service"))
               {
                  deployExternalWebService(actElement);
               }
               else
               {
                  if (!actElement.getTagName().equals("service"))
                  {
                     deployClient.appendChild(
                        deployClientDoc.importNode(actNode, true));
                  }
                  deploy.appendChild(deployDoc.importNode(actNode, true));
               }
            }
            else
            {
               deployClient.appendChild(
                  deployClientDoc.importNode(actNode, true));
               deploy.appendChild(deployDoc.importNode(actNode, true));
            }
         }

         // insert command into document
         deployDoc.appendChild(deploy);
         deployClientDoc.appendChild(deployClient);

         try
         {
            Thread.currentThread().setContextClassLoader(serviceLoader);
            new Deployment(deploy).deployToRegistry(
               ((XMLResourceProvider) axisServer.getConfig()).getDeployment());
            new Deployment(deployClient).deployToRegistry(
               clientConfiguration.buildDeployment());
            axisServer.refreshGlobalOptions();
            axisServer.saveConfiguration();
         }
         catch (Exception e)
         {
            throw new DeploymentException(
               Constants.COULD_NOT_DEPLOY_DESCRIPTOR,
               e);
         }
         finally
         {
            Thread.currentThread().setContextClassLoader(previous);
         }
      }
      catch (NamingException e)
      {
         throw new DeploymentException(
            Constants.COULD_NOT_DEPLOY_DESCRIPTOR,
            e);
      }
      catch (ParserConfigurationException parserError)
      {
         throw new DeploymentException(Constants.COULD_NOT_DEPLOY_DESCRIPTOR,
            parserError);
      }

   }

   /** 
    * this tiny helper copies all children of the given element that
    * are elements and match the given name to the other element
    */
   protected void copyChildren(Document sourceDoc, Element source, String match, Element target)
   {
      NodeList children = source.getChildNodes();
      for (int count = 0; count < children.getLength(); count++)
      {
         Node actNode = children.item(count);
         if (actNode instanceof Element)
         {
            if (((Element) actNode).getLocalName().equals(match))
            {
               target.appendChild(sourceDoc.importNode(actNode, true));
            }
         }
      }
   }

   /** stop a given deployment */
   public void stop(DeploymentInfo sdi) throws DeploymentException
   {
      ((Log4jLoggerPlugin) log.getLoggerPlugin()).getCategory().l7dlog(
         Priority.DEBUG,
         Constants.ABOUT_TO_STOP_AXIS_0,
         new Object[]{sdi},
         null);
      if (!deployments.containsKey(sdi.url))
      {
         throw new DeploymentException(
            "Attempting to undeploy a not-deployed unit! " + sdi.url);
      }
      // this was the deployment command
      Element root = (Element) ((Document) sdi.metaData).getDocumentElement();
      // from which we extract an undeployment counterpart
      Document undeployDoc = null;
      try
      {
         undeployDoc = XMLUtils.newDocument();
      }
      catch (ParserConfigurationException parserError)
      {
         throw new DeploymentException(Constants.COULD_NOT_DEPLOY_DESCRIPTOR,
            parserError);
      }
      Element undeploy =
         undeployDoc.createElementNS(root.getNamespaceURI(), "undeployment");
      
      // copy over administrational attributes
      NamedNodeMap attributes = root.getAttributes();
      for (int count = 0; count < attributes.getLength(); count++)
      {
         Attr attribute = (Attr) attributes.item(count);
         undeploy.setAttributeNodeNS(
            (Attr) undeployDoc.importNode(attribute, true));
      }

      // external services are just a matter of us
      NodeList children = root.getElementsByTagName("ext-service");
      for (int count = 0; count < children.getLength(); count++)
      {
         Element actNode = (Element) children.item(count);
         undeployExternalWebService(actNode);
      }

      // all service and handler entries are copied in the
      // undeployment document
      copyChildren(undeployDoc, root, "service", undeploy);
      copyChildren(undeployDoc, root, "handler", undeploy);
      copyChildren(undeployDoc, root, "typemapping", undeploy);
      copyChildren(undeployDoc, root, "beanmapping", undeploy);

      // put command into document
      undeployDoc.appendChild(undeploy);

      try
      {
         // and call the administrator
         new WSDDUndeployment(undeploy).undeployFromRegistry(
            ((XMLResourceProvider) axisServer.getConfig()).getDeployment());
         // and call the administrator
         new WSDDUndeployment(undeploy).undeployFromRegistry(
            clientConfiguration.buildDeployment());
         axisServer.refreshGlobalOptions();
         axisServer.saveConfiguration();
      }
      catch (Exception e)
      {
         throw new DeploymentException(Constants.COULD_NOT_UNDEPLOY, e);
      }
   }

   /** destroy a given deployment */
   public void destroy(DeploymentInfo sdi) throws DeploymentException
   {
      ((Log4jLoggerPlugin) log.getLoggerPlugin()).getCategory().l7dlog(
         Priority.DEBUG,
         Constants.ABOUT_TO_DESTROY_AXIS_0,
         new Object[]{sdi},
         null);
      deployments.remove(sdi.url);
   }

   /** return the associated client configuration */
   public EngineConfiguration getClientEngineConfiguration()
   {
      return clientConfiguration;
   }

   /** return the associated server configuration */
   public EngineConfiguration getServerEngineConfiguration()
   {
      return serverConfiguration;
   }

   /** return the associated server */
   public AxisServer getAxisServer()
   {
      return axisServer;
   }

}
