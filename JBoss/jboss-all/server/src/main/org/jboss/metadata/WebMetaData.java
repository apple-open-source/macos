/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.metadata;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.net.URL;
import java.io.InputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ByteArrayInputStream;

import org.w3c.dom.Element;
import org.w3c.dom.Document;

import org.jboss.deployment.DeploymentException;
import org.jboss.logging.Logger;
import org.jboss.mx.util.ObjectNameFactory;

/** A representation of the web.xml and jboss-web.xml deployment
 * descriptors as used by the AbstractWebContainer web container integration
 * support class.
 *
 * @see XmlLoadable
 * @see org.jboss.web.AbstractWebContainer
 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.9.2.7 $
 */
public class WebMetaData implements XmlLoadable
{
   private static Logger log = Logger.getLogger(WebMetaData.class);

   /** The web.xml resource-refs */
   private HashMap resourceReferences = new HashMap();
   /** The web.xml resource--env-refs */
   private HashMap resourceEnvReferences = new HashMap();
   /** web.xml env-entrys */
   private ArrayList environmentEntries = new ArrayList();
   /** web.xml ejb-refs */
   private HashMap ejbReferences = new HashMap();
   /** web.xml ejb-local-refs */
   private HashMap ejbLocalReferences = new HashMap();
   /** Currently unused as security-roles are not used */
   private ArrayList securityRoleReferences = new ArrayList();
   /** The web.xml distributable flag */
   private boolean distributable = false;
   /** The jboss-web.xml class-loading.java2ClassLoadingCompliance flag */
   private boolean java2ClassLoadingCompliance = false;
   /** The war context root as specified at the jboss-web.xml
    descriptor level. */
   private String contextRoot;
   /** The jboss-web.xml server container virtual host the war should be
      deployed into */
   private String virtualHost;
   /** The jboss-web.xml JNDI name of the security domain implementation
    */
   private String securityDomain;

   public static final int SESSION_INVALIDATE_SET_AND_GET =0;
   public static final int SESSION_INVALIDATE_SET_AND_NON_PRIMITIVE_GET =1;
   public static final int SESSION_INVALIDATE_SET =2;

   private int invalidateSessionPolicy = SESSION_INVALIDATE_SET_AND_NON_PRIMITIVE_GET;

   public static final int REPLICATION_TYPE_SYNC = 0;
   public static final int REPLICATION_TYPE_ASYNC = 1;

   private int replicationType = REPLICATION_TYPE_SYNC;

   /** The web context class loader used to create the java:comp context */
   private ClassLoader encLoader;
   private ArrayList depends = new ArrayList();

   /** Should the context use session cookies or use default */
   private int sessionCookies=SESSION_COOKIES_DEFAULT;

   public static final int SESSION_COOKIES_DEFAULT=0;
   public static final int SESSION_COOKIES_ENABLED=1;
   public static final int SESSION_COOKIES_DISABLED=2;

   public WebMetaData()
   {
   }

   /** Return an iterator of the env-entry mappings.
    @return Iterator of EnvEntryMetaData objects.
    */
   public Iterator getEnvironmentEntries()
   {
      return environmentEntries.iterator();
   }
   /** Return an iterator of the ejb-ref mappings.
    @return Iterator of EjbRefMetaData objects.
    */
   public Iterator getEjbReferences()
   {
      return ejbReferences.values().iterator();
   }
   /** Return an iterator of the ejb-local-ref mappings.
    @return Iterator of EjbLocalRefMetaData objects.
    */
   public Iterator getEjbLocalReferences()
   {
      return ejbLocalReferences.values().iterator();
   }

   /** Return an iterator of the resource-ref mappings.
    @return Iterator of ResourceRefMetaData objects.
    */
   public Iterator getResourceReferences()
   {
      return resourceReferences.values().iterator();
   }
   /** Return an iterator of the resource-ref mappings.
    @return Iterator of ResourceEnvRefMetaData objects.
    */
   public Iterator getResourceEnvReferences()
   {
      return resourceEnvReferences.values().iterator();
   }

   /** This the the jboss-web.xml descriptor context-root and it
    *is only meaningful if a war is deployed outside of an ear.
    */
   public String getContextRoot()
   {
      return contextRoot;
   }
   public void setContextRoot(String contextRoot)
   {
      this.contextRoot = contextRoot;
   }

   /** Return the optional security-domain jboss-web.xml element.
    @return The jndiName of the security manager implementation that is
    responsible for security of the web application. May be null if
    there was no security-domain specified in the jboss-web.xml
    descriptor.
    */
   public String getSecurityDomain()
   {
      return securityDomain;
   }

   /** The servlet container virtual host the war should be deployed into. If
    null then the servlet container default host should be used.
    */
   public String getVirtualHost()
   {
      return virtualHost;
   }

   /**
     The distributable flag.
     @return true if the web-app is marked distributable
    */
   public boolean getDistributable()
   {
      return distributable;
   }

   /** Access the web application depends
    * @return Iterator of JMX ObjectNames the web app depends on.
    */ 
   public Iterator getDepends()
   {
      return depends.iterator();
   }

   /** A flag indicating if the normal Java2 parent first class loading model
    * should be used over the servlet 2.3 web container first model.
    * @return true for parent first, false for the servlet 2.3 model
    */
   public boolean getJava2ClassLoadingCompliance()
   {
      return java2ClassLoadingCompliance;
   }
   public void setJava2ClassLoadingCompliance(boolean flag)
   {
      java2ClassLoadingCompliance = flag;
   }

   public ClassLoader getENCLoader()
   {
      return encLoader;
   }
   public void setENCLoader(ClassLoader encLoader)
   {
      this.encLoader = encLoader;
   }

   public int getSessionCookies()
   {
      return this.sessionCookies;
   }

   public int getInvalidateSessionPolicy()
   {
      return this.invalidateSessionPolicy;
   }

   public int getReplicationType()
   {
      return replicationType;
   }

   public void importXml(Element element) throws Exception
   {
      String rootTag = element.getOwnerDocument().getDocumentElement().getTagName();
      if( rootTag.equals("web-app") )
      {
         importWebXml(element);
      }
      else if( rootTag.equals("jboss-web") )
      {
         importJBossWebXml(element);
      }
   }
   
   /** Parse the elements of the web-app element used by the integration layer.
    */
   protected void importWebXml(Element webApp) throws Exception
   {
      // Parse the web-app/resource-ref elements
      Iterator iterator = MetaData.getChildrenByTagName(webApp, "resource-ref");
      while( iterator.hasNext() )
      {
         Element resourceRef = (Element) iterator.next();
         ResourceRefMetaData resourceRefMetaData = new ResourceRefMetaData();
         resourceRefMetaData.importEjbJarXml(resourceRef);
         resourceReferences.put(resourceRefMetaData.getRefName(), resourceRefMetaData);
      }

      // Parse the resource-env-ref elements
      iterator = MetaData.getChildrenByTagName(webApp, "resource-env-ref");
      while (iterator.hasNext())
      {
         Element resourceRef = (Element) iterator.next();
         ResourceEnvRefMetaData refMetaData = new ResourceEnvRefMetaData();
         refMetaData.importEjbJarXml(resourceRef);
         resourceEnvReferences.put(refMetaData.getRefName(), refMetaData);
      }

      // Parse the web-app/env-entry elements
      iterator = MetaData.getChildrenByTagName(webApp, "env-entry");
      while( iterator.hasNext() )
      {
         Element envEntry = (Element) iterator.next();
         EnvEntryMetaData envEntryMetaData = new EnvEntryMetaData();
         envEntryMetaData.importEjbJarXml(envEntry);
         environmentEntries.add(envEntryMetaData);
      }

      // Parse the web-app/ejb-ref elements
      iterator = MetaData.getChildrenByTagName(webApp, "ejb-ref");
      while( iterator.hasNext() )
      {
         Element ejbRef = (Element) iterator.next();
         EjbRefMetaData ejbRefMetaData = new EjbRefMetaData();
         ejbRefMetaData.importEjbJarXml(ejbRef);
         ejbReferences.put(ejbRefMetaData.getName(), ejbRefMetaData);
      }

      // Parse the web-app/ejb-local-ref elements
      iterator = MetaData.getChildrenByTagName(webApp, "ejb-local-ref");
      while( iterator.hasNext() )
      {
         Element ejbRef = (Element) iterator.next();
         EjbLocalRefMetaData ejbRefMetaData = new EjbLocalRefMetaData();
         ejbRefMetaData.importEjbJarXml(ejbRef);
         ejbLocalReferences.put(ejbRefMetaData.getName(), ejbRefMetaData);
      }

      // Is the web-app marked distributable?
      iterator = MetaData.getChildrenByTagName(webApp, "distributable");
      if(iterator.hasNext())
      {
         distributable=true;
      }
   }

   /** Parse the elements of the jboss-web element used by the integration layer.
    */
   protected void importJBossWebXml(Element jbossWeb) throws Exception
   {
      // Parse the jboss-web/root-context element
      Element contextRootElement = MetaData.getOptionalChild(jbossWeb, "context-root");
      if( contextRootElement != null )
         contextRoot = MetaData.getElementContent(contextRootElement);

      // Parse the jboss-web/security-domain element
      Element securityDomainElement = MetaData.getOptionalChild(jbossWeb, "security-domain");
      if( securityDomainElement != null )
         securityDomain = MetaData.getElementContent(securityDomainElement);

      // Parse the jboss-web/virtual-host element
      Element virtualHostElement = MetaData.getOptionalChild(jbossWeb, "virtual-host");
      if( virtualHostElement != null )
         virtualHost = MetaData.getElementContent(virtualHostElement);

      // Parse the jboss-web/resource-ref elements
      Iterator iterator = MetaData.getChildrenByTagName(jbossWeb, "resource-ref");
      while( iterator.hasNext() )
      {
         Element resourceRef = (Element) iterator.next();
         String resRefName = MetaData.getElementContent(MetaData.getUniqueChild(resourceRef, "res-ref-name"));
         ResourceRefMetaData refMetaData = (ResourceRefMetaData) resourceReferences.get(resRefName);
         if( refMetaData == null )
         {
            throw new DeploymentException("resource-ref " + resRefName
               + " found in jboss-web.xml but not in web.xml");
         }
         refMetaData.importJbossXml(resourceRef);
      }

      // Parse the jboss-web/resource-env-ref elements
      iterator = MetaData.getChildrenByTagName(jbossWeb, "resource-env-ref");
      while( iterator.hasNext() )
      {
         Element resourceRef = (Element) iterator.next();
         String resRefName = MetaData.getElementContent(MetaData.getUniqueChild(resourceRef, "resource-env-ref-name"));
         ResourceEnvRefMetaData refMetaData = (ResourceEnvRefMetaData) resourceEnvReferences.get(resRefName);
         if( refMetaData == null )
         {
            throw new DeploymentException("resource-env-ref " + resRefName
               + " found in jboss-web.xml but not in web.xml");
         }
         refMetaData.importJbossXml(resourceRef);
      }

      // Parse the jboss-web/ejb-ref elements
      iterator = MetaData.getChildrenByTagName(jbossWeb, "ejb-ref");
      while( iterator.hasNext() )
      {
         Element ejbRef = (Element) iterator.next();
         String ejbRefName = MetaData.getElementContent(MetaData.getUniqueChild(ejbRef, "ejb-ref-name"));
         EjbRefMetaData ejbRefMetaData = (EjbRefMetaData) ejbReferences.get(ejbRefName);
         if( ejbRefMetaData == null )
         {
            throw new DeploymentException("ejb-ref " + ejbRefName
               + " found in jboss-web.xml but not in web.xml");
         }
         ejbRefMetaData.importJbossXml(ejbRef);
      }

      // Parse the jboss-web/ejb-local-ref elements
      iterator = MetaData.getChildrenByTagName(jbossWeb, "ejb-local-ref");
      while( iterator.hasNext() )
      {
         Element ejbLocalRef = (Element) iterator.next();
         String ejbLocalRefName = MetaData.getElementContent(MetaData.getUniqueChild(ejbLocalRef, "ejb-ref-name"));
         EjbLocalRefMetaData ejbLocalRefMetaData = (EjbLocalRefMetaData) ejbLocalReferences.get(ejbLocalRefName);
         if( ejbLocalRefMetaData == null )
         {
            throw new DeploymentException("ejb-local-ref " + ejbLocalRefName
               + " found in jboss-web.xml but not in web.xml");
         }
         ejbLocalRefMetaData.importJbossXml(ejbLocalRef);
      }

      // Parse the jboss-web/depends elements
      for( Iterator dependsElements = MetaData.getChildrenByTagName(jbossWeb, "depends");
         dependsElements.hasNext();)
      {
         Element dependsElement = (Element)dependsElements.next();
         String dependsName = MetaData.getElementContent(dependsElement);
         depends.add(ObjectNameFactory.create(dependsName));
      } // end of for ()

      // Parse the jboss-web/use-session-cookies element
      iterator = MetaData.getChildrenByTagName(jbossWeb, "use-session-cookies");
      if ( iterator.hasNext() )
      {
         Element useCookiesElement = (Element) iterator.next();
         String useCookiesElementContent = MetaData.getElementContent(useCookiesElement);
         Boolean useCookies=Boolean.valueOf(useCookiesElementContent);
         
         if (useCookies.booleanValue())
         {
            sessionCookies=SESSION_COOKIES_ENABLED;
         }
         else
         {
            sessionCookies=SESSION_COOKIES_DISABLED;
         }
      }

      // Parse the jboss-web/session-replication element


      Element sessionReplicationRootElement = MetaData.getOptionalChild(jbossWeb, "replication-config");
      if( sessionReplicationRootElement != null )
      {
         // manage "replication-trigger" first ...
         //
         Element replicationTriggerElement = MetaData.getOptionalChild(sessionReplicationRootElement, "replication-trigger");
         if (replicationTriggerElement != null)
         {
            String repMethod = MetaData.getElementContent(replicationTriggerElement);
            if ("SET_AND_GET".equalsIgnoreCase(repMethod))
               this.invalidateSessionPolicy = SESSION_INVALIDATE_SET_AND_GET;
            else if ("SET_AND_NON_PRIMITIVE_GET".equalsIgnoreCase(repMethod))
               this.invalidateSessionPolicy = SESSION_INVALIDATE_SET_AND_NON_PRIMITIVE_GET;
            else if ("SET".equalsIgnoreCase(repMethod))
               this.invalidateSessionPolicy = SESSION_INVALIDATE_SET;
            else
               throw new DeploymentException("replication-trigger value set to a non-valid value: '" + repMethod
                  + "' (should be ['SET_AND_GET', 'SET_AND_NON_PRIMITIVE_GET', 'SET']) in jboss-web.xml");
         }

         // ... then manage "replication-type".
         //
         Element replicationTypeElement = MetaData.getOptionalChild(sessionReplicationRootElement, "replication-type");
         if (replicationTypeElement != null)
         {
            String repType = MetaData.getElementContent(replicationTypeElement);
            if ("SYNC".equalsIgnoreCase(repType))
               this.replicationType = REPLICATION_TYPE_SYNC;
            else if ("ASYNC".equalsIgnoreCase(repType))
               this.replicationType = REPLICATION_TYPE_ASYNC;
            else
               throw new DeploymentException("replication-type value set to a non-valid value: '" + repType
                  + "' (should be ['SYNC', 'ASYNC']) in jboss-web.xml");
         }

      }

      /* The jboss-web/class-loading.java2ClassLoadingCompliance attribute is
      parsed by the AbstractWebContainer since it can be overriden at that
      level and the loader-repository config is needed early
      */
   }

   /** This method creates a context-root string from either the
      WEB-INF/jboss-web.xml context-root element is one exists, or the
      filename portion of the warURL. It is called if the DeploymentInfo
      webContext value is null which indicates a standalone war deployment.
      A war name of ROOT.war is handled as a special case of a war that
      should be installed as the default web context.
    */
   public void parseMetaData(String ctxPath, URL warURL)
      throws DeploymentException
   {
      InputStream jbossWebIS = null;
      InputStream webIS = null;

      // Parse the war deployment descriptors, web.xml and jboss-web.xml
      try
      {
         // See if the warUrl is a directory
         File warDir = new File(warURL.getFile());
         if( warURL.getProtocol().equals("file") && warDir.isDirectory() == true )
         {
            File webDD = new File(warDir, "WEB-INF/web.xml");
            if( webDD.exists() == true )
               webIS = new FileInputStream(webDD);
            File jbossWebDD = new File(warDir, "WEB-INF/jboss-web.xml");
            if( jbossWebDD.exists() == true )
               jbossWebIS = new FileInputStream(jbossWebDD);
         }
         else
         {
            // First check for a WEB-INF/web.xml and a WEB-INF/jboss-web.xml
            InputStream warIS = warURL.openStream();
            java.util.zip.ZipInputStream zipIS = new java.util.zip.ZipInputStream(warIS);
            java.util.zip.ZipEntry entry;
            byte[] buffer = new byte[512];
            int bytes;
            while( (entry = zipIS.getNextEntry()) != null )
            {
               if( entry.getName().equals("WEB-INF/web.xml") )
               {
                  ByteArrayOutputStream baos = new ByteArrayOutputStream();
                  while( (bytes = zipIS.read(buffer)) > 0 )
                  {
                     baos.write(buffer, 0, bytes);
                  }
                  webIS = new ByteArrayInputStream(baos.toByteArray());
               }
               else if( entry.getName().equals("WEB-INF/jboss-web.xml") )
               {
                  ByteArrayOutputStream baos = new ByteArrayOutputStream();
                  while( (bytes = zipIS.read(buffer)) > 0 )
                  {
                     baos.write(buffer, 0, bytes);
                  }
                  jbossWebIS = new ByteArrayInputStream(baos.toByteArray());
               }
            }
            zipIS.close();
         }

         XmlFileLoader xmlLoader = new XmlFileLoader();
         String warURI = warURL.toExternalForm();
         try
         {
            if( webIS != null )
            {
               Document webDoc = xmlLoader.getDocument(webIS, warURI+"/WEB-INF/web.xml");
               Element web = webDoc.getDocumentElement();
               this.importXml(web);
            }
         }
         catch(Exception e)
         {
            throw new DeploymentException("Failed to parse WEB-INF/web.xml", e);
         }
         try
         {
            if( jbossWebIS != null )
            {
               Document jbossWebDoc = xmlLoader.getDocument(jbossWebIS, warURI+"/WEB-INF/jboss-web.xml");
               Element jbossWeb = jbossWebDoc.getDocumentElement();
               this.importXml(jbossWeb);
            }
         }
         catch(Exception e)
         {
            throw new DeploymentException("Failed to parse WEB-INF/jboss-web.xml", e);
         }

      }
      catch(Exception e)
      {
         log.warn("Failed to parse descriptors for war("+warURL+")", e);
      }

      // Build a war root context from the war name if one was not specified
      String webContext = ctxPath;
      if( webContext == null )
         webContext = this.getContextRoot();
      if( webContext == null )
      {
         // Build the context from the war name, strip the .war suffix
         webContext = warURL.getFile();
         webContext = webContext.replace('\\', '/');
         if( webContext.endsWith("/") )
            webContext = webContext.substring(0, webContext.length()-1);
         int prefix = webContext.lastIndexOf('/');
         if( prefix > 0 )
            webContext = webContext.substring(prefix+1);
         int suffix = webContext.lastIndexOf(".war");
         if( suffix > 0 )
            webContext = webContext.substring(0, suffix);
          // Strip any '<int-value>.' prefix
          int index = 0;
          for(; index < webContext.length(); index ++)
          {
             char c = webContext.charAt(index);
             if( Character.isDigit(c) == false && c != '.' )
                break;
          }
          webContext = webContext.substring(index);
      }

      // Servlet containers are anal about the web context starting with '/'
      if( webContext.length() > 0 && webContext.charAt(0) != '/' )
         webContext = "/" + webContext;
      // And also the default root context must be an empty string, not '/'
      else if( webContext.equals("/") )
         webContext = "";
      this.setContextRoot(webContext);
   }

}
