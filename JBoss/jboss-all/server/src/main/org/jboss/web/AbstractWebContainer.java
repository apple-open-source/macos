/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.web;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.InputStream;
import java.lang.reflect.Method;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.LinkRef;
import javax.naming.NamingException;

import org.jboss.deployment.DeploymentException;
import org.jboss.deployment.DeploymentInfo;
import org.jboss.deployment.SubDeployerSupport;
import org.jboss.ejb.EjbUtil;
import org.jboss.metadata.EjbLocalRefMetaData;
import org.jboss.metadata.EjbRefMetaData;
import org.jboss.metadata.EnvEntryMetaData;
import org.jboss.metadata.ResourceEnvRefMetaData;
import org.jboss.metadata.ResourceRefMetaData;
import org.jboss.metadata.WebMetaData;
import org.jboss.metadata.XmlFileLoader;
import org.jboss.metadata.MetaData;
import org.jboss.mx.loading.LoaderRepositoryFactory;
import org.jboss.mx.loading.LoaderRepositoryFactory.LoaderRepositoryConfig;
import org.jboss.naming.Util;
import org.jboss.security.plugins.NullSecurityManager;
import org.jboss.util.file.JarUtils;

import org.w3c.dom.Document;
import org.w3c.dom.Element;

/** A template pattern class for web container integration into JBoss. This class
should be subclasses by web container providers wishing to integrate their
container into a JBoss server.

It provides support for mapping the following web-app.xml/jboss-web.xml elements
into the JBoss server JNDI namespace:
- env-entry
- resource-ref
- resource-env-ref
- ejb-ref
- ejb-local-ref
- security-domain

Subclasses need to implement the {@link #performDeploy(WebApplication, String,
 WebDescriptorParser) performDeploy()}
and {@link #performUndeploy(String) performUndeploy()} methods to perform the
container specific steps and return the web application info required by the
AbstractWebContainer class.

Integration with the JBossSX security framework is based on the establishment
of a java:comp/env/security context as described in the
{@link linkSecurityDomain(String, Context) linkSecurityDomain } comments.
The security context provides access to the JBossSX security mgr interface
implementations for use by subclass request interceptors. A outline of the
steps for authenticating a user is:
<code>
   // Get the username & password from the request context...
   String username = f(request);
   String password = f(request);
   // Get the JBoss security manager from the ENC context
   InitialContext iniCtx = new InitialContext();
   SecurityManager securityMgr = (SecurityManager) iniCtx.lookup("java:comp/env/security/securityMgr");
   SimplePrincipal principal = new SimplePrincipal(username);
   if( securityMgr.isValid(principal, password) )
   {
   // Indicate the user is allowed access to the web content...

   // Propagate the user info to JBoss for any calls into made by the servlet
   SecurityAssociation.setPrincipal(principal);
   SecurityAssociation.setCredential(password.toCharArray());
   }
   else
   {
   // Deny access...
   }
</code>

An outline of the steps for authorizing the user is:
<code>
   // Get the username & required roles from the request context...
   String username = f(request);
   String[] roles = f(request);
   // Get the JBoss security manager from the ENC context
   InitialContext iniCtx = new InitialContext();
   RealmMapping securityMgr = (RealmMapping) iniCtx.lookup("java:comp/env/security/realmMapping");
   SimplePrincipal principal = new SimplePrincipal(username);
   Set requiredRoles = new HashSet(Arrays.asList(roles));
   if( securityMgr.doesUserHaveRole(principal, requiredRoles) )
   {
   // Indicate the user has the required roles for the web content...
   }
   else
   {
   // Deny access...
   }
</code>

The one thing to be aware of is the relationship between the thread context
class loader and the JNDI ENC context. Any method that attempts to access
the JNDI ENC context must have the ClassLoader in the WebApplication returned
from the {@link #performDeploy(String, String) performDeploy} as its thread
context ClassLoader or else the lookup for java:comp/env will fail with a
name not found exception, or worse, it will receive some other web application
ENC context. If your adapting a web container that is trying be compatible with
both 1.1 and 1.2 Java VMs this is something you need to pay special attention
to. For example, I have seen problems a request interceptor that was handling
the authentication/authorization callouts in tomcat3.2.1 not having the same
thread context ClassLoader as was used to dispatch the http service request.

For a complete example see the
{@link org.jboss.web.catalina.EmbeddedCatalinaServiceSX EmbeddedCatalinaServiceSX}
in the catalina module.

@see #performDeploy(WebApplication webApp, String warUrl,
        WebDescriptorParser webAppParser)
@see #performUndeploy(String)
@see #parseWebAppDescriptors(ClassLoader, WebMetaData)
@see #linkSecurityDomain(String, Context)
@see org.jboss.security.SecurityManager;
@see org.jboss.security.RealmMapping;
@see org.jboss.security.SimplePrincipal;
@see org.jboss.security.SecurityAssociation;

@jmx:mbean extends="org.jboss.deployment.SubDeployerMBean"

@author  Scott.Stark@jboss.org
@version $Revision: 1.51.2.25 $
*/
public abstract class AbstractWebContainer
   extends SubDeployerSupport
   implements AbstractWebContainerMBean
{
   public static interface WebDescriptorParser
   {
      /** This method is called as part of subclass performDeploy() method implementations
      to parse the web-app.xml and jboss-web.xml deployment descriptors from a
      war deployment. The method creates the ENC(java:comp/env) env-entry,
      resource-ref, ejb-ref, etc element values. The creation of the env-entry
      values does not require a jboss-web.xml descriptor. The creation of the
      resource-ref and ejb-ref elements does require a jboss-web.xml descriptor
      for the JNDI name of the deployed resources/EJBs.

      Because the ENC context is private to the web application, the web
      application class loader is used to identify the ENC. The class loader
      is used because each war typically requires a unique class loader to
      isolate the web application classes/resources. This means that the
      ClassLoader passed to this method must be the thread context ClassLoader
      seen by the server/jsp pages during init/destroy/service/etc. method
      invocations if these methods interace with the JNDI ENC context.

      @param loader, the ClassLoader for the web application. May not be null.
      @param metaData, the WebMetaData from the WebApplication object passed to
       the performDeploy method.
      */
      public void parseWebAppDescriptors(ClassLoader loader, WebMetaData metaData) throws Exception;

      /** Get the DeploymentInfo for the war the triggered the deployment process.
       * The returned reference may be updated to affect the state of the
       * JBoss DeploymentInfo object. This can be used to assign ObjectNames
       * of MBeans created by the container.
       * @return The DeploymentInfo for the war being deployed.
       */
      public DeploymentInfo getDeploymentInfo();
   }

   /** A mapping of deployed warUrl strings to the WebApplication object */
   protected HashMap deploymentMap = new HashMap();
   /** The parent class loader first model flag */
   protected boolean java2ClassLoadingCompliance = false;
   /** A flag indicating if war archives should be unpacked */
   protected boolean unpackWars = true;

   /** If true, ejb-links that don't resolve don't cause an error (fallback to jndi-name) */
   protected boolean lenientEjbLink = false;

   public AbstractWebContainer()
   {
   }

   /** Get the flag indicating if the normal Java2 parent first class loading
    * model should be used over the servlet 2.3 web container first model.
    * @return true for parent first, false for the servlet 2.3 model
    * @jmx:managed-attribute
    */
   public boolean getJava2ClassLoadingCompliance()
   {
      return java2ClassLoadingCompliance;
   }
   /** Set the flag indicating if the normal Java2 parent first class loading
    * model should be used over the servlet 2.3 web container first model.
    * @param flag true for parent first, false for the servlet 2.3 model
    * @jmx:managed-attribute
    */
   public void setJava2ClassLoadingCompliance(boolean flag)
   {
      java2ClassLoadingCompliance = flag;
   }

   /** Set the flag indicating if war archives should be unpacked. This may
    * need to be set to false as long extraction paths under deploy can
    * show up as deployment failures on some platforms.
    * 
    * @jmx:managed-attribute
    * @return true is war archives should be unpacked
    */ 
   public boolean getUnpackWars()
   {
      return unpackWars;
   }
   /** Get the flag indicating if war archives should be unpacked. This may
    * need to be set to false as long extraction paths under deploy can
    * show up as deployment failures on some platforms.
    * 
    * @jmx:managed-attribute
    * @param flag , true is war archives should be unpacked
    */ 
   public void setUnpackWars(boolean flag)
   {
      this.unpackWars = flag;
   }


    /**
     * Get the flag indicating if ejb-link errors should be ignored
     * in favour of trying the jndi-name in jboss-web.xml
     * @return a <code>boolean</code> value
     *    
     * @jmx:managed-attribute
     */
    public boolean getLenientEjbLink ()
    {
        return lenientEjbLink;
    }
    
    /**
     * Set the flag indicating if ejb-link errors should be ignored
     * in favour of trying the jndi-name in jboss-web.xml
     * @return a <code>boolean</code> value
     *    
     * @jmx:managed-attribute
     */    
    public void setLenientEjbLink (boolean flag)
    {
        lenientEjbLink = flag;
    }
    
    public boolean accepts(DeploymentInfo sdi)
    {
        String warFile = sdi.url.getFile();
        return warFile.endsWith("war") || warFile.endsWith("war/");
    }

   public synchronized void init(DeploymentInfo di)
      throws DeploymentException
   {
      log.debug("Begin init");
      try
      {
         if (di.url.getPath().endsWith("/"))
         {
            // the URL is a unpacked collection, watch the deployment descriptor
            di.watch = new URL(di.url, "WEB-INF/web.xml");
         }
         else
         {
            // just watch the original URL
            di.watch = di.url;
         }

         // Make sure the war is unpacked if unpackWars is true
         File warFile = new File(di.localUrl.getFile());
         if( warFile.isDirectory() == false && unpackWars == true )
         {
            File tmp = new File(warFile.getAbsolutePath()+".tmp");
            if( warFile.renameTo(tmp) == false )
               throw new DeploymentException("Was unable to move war to: "+tmp);
            if( warFile.mkdir() == false )
               throw new DeploymentException("Was unable to mkdir: "+warFile);            
            log.debug("Unpacking war to: "+warFile);
            FileInputStream fis = new FileInputStream(tmp);
            JarUtils.unjar(fis, warFile);
            fis.close();
            log.debug("Replaced war with unpacked contents");
            if( tmp.delete() == false )
               log.debug("Was unable to delete war tmp file");
            else
               log.debug("Deleted war archive");
            // Reset the localUrl to end in a '/'
            di.localUrl = warFile.toURL();
            // Reset the localCl to point to the file
            URL[] localCP = {di.localUrl};
            di.localCl = new URLClassLoader(localCP);
         }

         WebMetaData metaData = new WebMetaData();
         metaData.setJava2ClassLoadingCompliance(this.java2ClassLoadingCompliance);
         di.metaData = metaData;
         // Check for a loader-repository
         XmlFileLoader xfl = new XmlFileLoader();
         InputStream in = di.localCl.getResourceAsStream("WEB-INF/jboss-web.xml");
         if( in != null )
         {
            Element jbossWeb = xfl.getDocument(in, "WEB-INF/jboss-web.xml").getDocumentElement();
            in.close();
            // Check for a war level class loading config
            Element classLoading = MetaData.getOptionalChild(jbossWeb, "class-loading");
            if( classLoading != null )
            {
               String flagString = classLoading.getAttribute("java2ClassLoadingCompliance");
               if( flagString.length() == 0 )
                  flagString = "true";
               boolean flag = Boolean.valueOf(flagString).booleanValue();
               metaData.setJava2ClassLoadingCompliance(flag);
               // Check for a loader-repository for scoping
               Element loader = MetaData.getOptionalChild(classLoading, "loader-repository");
               if( loader != null )
               {
                  LoaderRepositoryConfig config = LoaderRepositoryFactory.parseRepositoryConfig(loader);
                  di.setRepositoryInfo(config);
               }
            }
         }

         // Generate an event for the initialization
         super.init(di);
      }
      catch (Exception e)
      {
         log.error("Problem in init ", e);
         throw new DeploymentException(e);
      }

      log.debug("End init");
   }

   /** WARs do not have nested deployments
    * @param di
    */
   protected void processNestedDeployments(DeploymentInfo di)
   {
   }

   /** A template pattern implementation of the deploy() method. This method
   calls the {@link #performDeploy(String, String) performDeploy()} method to
   perform the container specific deployment steps and registers the
   returned WebApplication in the deployment map. The steps performed are:

      ClassLoader appClassLoader = thread.getContextClassLoader();
      URLClassLoader warLoader = URLClassLoader.newInstance(empty, appClassLoader);
      thread.setContextClassLoader(warLoader);
      WebDescriptorParser webAppParser = ...;
      WebMetaData metaData = di.metaData;
      parseMetaData(ctxPath, warUrl, metaData);
      WebApplication warInfo = new WebApplication(metaData);
      performDeploy(warInfo, warUrl, webAppParser);
      deploymentMap.put(warUrl, warInfo);
      thread.setContextClassLoader(appClassLoader);

   The subclass performDeploy() implementation needs to invoke
   webAppParser.parseWebAppDescriptors(loader, warInfo) to have the JNDI
   java:comp/env namespace setup before any web app component can access
   this namespace.

    Also, an MBean for each servlet deployed should be created and its
    JMX ObjectName placed into the DeploymentInfo.mbeans list so that the
    JSR77 layer can create the approriate model view. The servlet MBean
    needs to provide access to the min, max and total time in milliseconds.
    Expose this information via MinServiceTime, MaxServiceTime and TotalServiceTime
    attributes to integrate seemlessly with the JSR77 factory layer.

   @param di, The deployment info that contains the context-root element value
    from the J2EE application/module/web application.xml descriptor. This may
    be null if war was is not being deployed as part of an enterprise application.
    It also contains the URL of the web application war.
   */
   public synchronized void start(DeploymentInfo di) throws DeploymentException
   {
      Thread thread = Thread.currentThread();
      ClassLoader appClassLoader = thread.getContextClassLoader();
      try
      {
         // Create a classloader for the war to ensure a unique ENC
         URL[] empty = {};
         URLClassLoader warLoader = URLClassLoader.newInstance(empty, di.ucl);
         thread.setContextClassLoader(warLoader);
         WebDescriptorParser webAppParser = new DescriptorParser(di);
         String webContext = di.webContext;
         if( webContext != null )
         {
            if( webContext.length() > 0 && webContext.charAt(0) != '/' )
               webContext = "/" + webContext;
         }
         // Get the war URL
         URL warURL = di.localUrl != null ? di.localUrl : di.url;

         if (log.isDebugEnabled())
         {
            log.debug("webContext: " + webContext);
            log.debug("warURL: " + warURL);
            log.debug("webAppParser: " + webAppParser);
         }

         // Parse the web.xml and jboss-web.xml descriptors
         WebMetaData metaData = (WebMetaData) di.metaData;
         parseMetaData(webContext, warURL, di.shortName, metaData);
         WebApplication warInfo = new WebApplication(metaData);
         warInfo.setDeploymentInfo(di);
         performDeploy(warInfo, warURL.toString(), webAppParser);
         deploymentMap.put(warURL.toString(), warInfo);

         // Generate an event for the startup
         super.start(di);
      }
      catch(DeploymentException e)
      {
         throw e;
      }
      catch(Exception e)
      {
         throw new DeploymentException("Error during deploy", e);
      }
      finally
      {
         thread.setContextClassLoader(appClassLoader);
      }
   }

   /** This method is called by the deploy() method template and must be overriden by
   subclasses to perform the web container specific deployment steps.
   @param webApp, The web application information context. This contains the
    metadata such as the context-root element value from the J2EE
   application/module/web application.xml descriptor and virtual-host.
   @param warUrl, The string for the URL of the web application war.
   @param webAppParser, The callback interface the web container should use to
   setup the web app JNDI environment for use by the web app components. This
   needs to be invoked after the web app class loader is known, but before
   and web app components attempt to access the java:comp/env JNDI namespace.
   @return WebApplication, the web application information required by the
   AbstractWebContainer class to track the war deployment status.
   */
   protected abstract void performDeploy(WebApplication webApp, String warUrl,
      WebDescriptorParser webAppParser) throws Exception;

   /** A template pattern implementation of the undeploy() method. This method
   calls the {@link #performUndeploy(String) performUndeploy()} method to
   perform the container specific undeployment steps and unregisters the
   the warUrl from the deployment map.
   */
   public synchronized void stop(DeploymentInfo di)
      throws DeploymentException
   {
      URL warURL = di.localUrl != null ? di.localUrl : di.url;
      String warUrl = warURL.toString();
      try
      {
         performUndeploy(warUrl);
         // Remove the web application ENC...
         deploymentMap.remove(warUrl);

         // Generate an event for the stop
         super.stop(di);
      }
      catch(DeploymentException e)
      {
         throw e;
      }
      catch(Exception e)
      {
         throw new DeploymentException("Error during deploy", e);
      }
   }

   /** Called as part of the undeploy() method template to ask the
   subclass for perform the web container specific undeployment steps.
   */
   protected abstract void performUndeploy(String warUrl) throws Exception;

   /** See if a war is deployed.
     @jmx:managed-operation
   */
   public boolean isDeployed(String warUrl)
   {
      return deploymentMap.containsKey(warUrl);
   }

   /** Get the WebApplication object for a deployed war.
   @param warUrl, the war url string as originally passed to deploy().
   @return The WebApplication created during the deploy step if the
   warUrl is valid, null if no such deployment exists.
   */
   public WebApplication getDeployedApp(String warUrl)
   {
      WebApplication appInfo = (WebApplication) deploymentMap.get(warUrl);
      return appInfo;
   }

   /** Returns the applications deployed by the web container subclasses.
   @jmx:managed-attribute
   @return An Iterator of WebApplication objects for the deployed wars.
   */
   public Iterator getDeployedApplications()
   {
      return deploymentMap.values().iterator();
   }

   /** An accessor for any configuration element set via setConfig. This
   method always returns null and must be overriden by subclasses to
   return a valid value.
   @jmx:managed-attribute
   */
   public Element getConfig()
   {
      return null;
   }
   /** This method is invoked to import an arbitrary XML configuration tree.
   Subclasses should override this method if they support such a configuration
   capability. This implementation does nothing.
   @jmx:managed-attribute
   */
   public void setConfig(Element config)
   {
   }

   /** This method is invoked from within subclass performDeploy() method
   implementations when they invoke WebDescriptorParser.parseWebAppDescriptors().

   @param loader, the ClassLoader for the web application. May not be null.
   @param metaData, the WebMetaData from the WebApplication object passed to
    the performDeploy method.
   */
   protected void parseWebAppDescriptors(DeploymentInfo di, ClassLoader loader,
      WebMetaData metaData)
      throws Exception
   {
      log.debug("AbstractWebContainer.parseWebAppDescriptors, Begin");
      InitialContext iniCtx = new InitialContext();
      Context envCtx = null;
      Thread currentThread = Thread.currentThread();
      ClassLoader currentLoader = currentThread.getContextClassLoader();
      try
      {
         // Create a java:comp/env environment unique for the web application
         log.debug("Creating ENC using ClassLoader: "+loader);
         ClassLoader parent = loader.getParent();
         while( parent != null )
         {
            log.debug(".."+parent);
            parent = parent.getParent();
         }
         currentThread.setContextClassLoader(loader);
         metaData.setENCLoader(loader);
         envCtx = (Context) iniCtx.lookup("java:comp");

         // Add a link to the global transaction manager
         envCtx.bind("UserTransaction", new LinkRef("UserTransaction"));
         log.debug("Linked java:comp/UserTransaction to JNDI name: UserTransaction");
         envCtx = envCtx.createSubcontext("env");
      }
      finally
      {
         currentThread.setContextClassLoader(currentLoader);
      }

      Iterator envEntries = metaData.getEnvironmentEntries();
      log.debug("addEnvEntries");
      addEnvEntries(envEntries, envCtx);
      Iterator resourceEnvRefs = metaData.getResourceEnvReferences();
      log.debug("linkResourceEnvRefs");
      linkResourceEnvRefs(resourceEnvRefs, envCtx);
      Iterator resourceRefs = metaData.getResourceReferences();
      log.debug("linkResourceRefs");
      linkResourceRefs(resourceRefs, envCtx);
      Iterator ejbRefs = metaData.getEjbReferences();
      log.debug("linkEjbRefs");
      linkEjbRefs(ejbRefs, envCtx, di);
      Iterator ejbLocalRefs = metaData.getEjbLocalReferences();
      log.debug("linkEjbLocalRefs");
      linkEjbLocalRefs(ejbLocalRefs, envCtx, di);
      String securityDomain = metaData.getSecurityDomain();
      log.debug("linkSecurityDomain");
      linkSecurityDomain(securityDomain, envCtx);
      log.debug("AbstractWebContainer.parseWebAppDescriptors, End");
   }

   protected void addEnvEntries(Iterator envEntries, Context envCtx)
      throws ClassNotFoundException, NamingException
   {
      while( envEntries.hasNext() )
      {
         EnvEntryMetaData entry = (EnvEntryMetaData) envEntries.next();
            log.debug("Binding env-entry: "+entry.getName()+" of type: " +
                      entry.getType()+" to value:"+entry.getValue());
         EnvEntryMetaData.bindEnvEntry(envCtx, entry);
      }
   }

   protected void linkResourceEnvRefs(Iterator resourceEnvRefs, Context envCtx)
      throws NamingException
   {
      while( resourceEnvRefs.hasNext() )
      {
         ResourceEnvRefMetaData ref = (ResourceEnvRefMetaData) resourceEnvRefs.next();
         String resourceName = ref.getJndiName();
         String refName = ref.getRefName();
         if( ref.getType().equals("java.net.URL") )
         {
             try
             {
                 log.debug("Binding '"+refName+"' to URL: "+resourceName);
                 URL url = new URL(resourceName);
                 Util.bind(envCtx, refName, url);
             }
             catch(MalformedURLException e)
             {
                 throw new NamingException("Malformed URL:"+e.getMessage());
             }
         }
         else if( resourceName != null )
         {
            log.debug("Linking '"+refName+"' to JNDI name: "+resourceName);
            Util.bind(envCtx, refName, new LinkRef(resourceName));
         }
         else
         {
            throw new NamingException("resource-env-ref: "+refName
               +" has no valid JNDI binding. Check the jboss-web/resource-env-ref.");
         }
      }
   }

   protected void linkResourceRefs(Iterator resourceRefs, Context envCtx)
      throws NamingException
   {
      while( resourceRefs.hasNext() )
      {
         ResourceRefMetaData ref = (ResourceRefMetaData) resourceRefs.next();
         String jndiName = ref.getJndiName();
         String refName = ref.getRefName();
         if( ref.getType().equals("java.net.URL") )
         {
             try
             {
                 log.debug("Binding '"+refName+"' to URL: "+jndiName);
                 URL url = new URL(jndiName);
                 Util.bind(envCtx, refName, url);
             }
             catch(MalformedURLException e)
             {
                 throw new NamingException("Malformed URL:"+e.getMessage());
             }
         }
         else if( jndiName != null )
         {
             log.debug("Linking '"+refName+"' to JNDI name: "+jndiName);
             Util.bind(envCtx, refName, new LinkRef(jndiName));
         }
         else
         {
            throw new NamingException("resource-ref: "+refName
               +" has no valid JNDI binding. Check the jboss-web/resource-ref.");
         }
      }
   }

   protected void linkEjbRefs(Iterator ejbRefs, Context envCtx, DeploymentInfo di)
      throws NamingException
   {
      while( ejbRefs.hasNext() )
      {
         EjbRefMetaData ejb = (EjbRefMetaData) ejbRefs.next();
         String name = ejb.getName();
         String linkName = ejb.getLink();
         String jndiName = null;

         //use ejb-link if it is specified
         if ( linkName != null )
         {
             jndiName = EjbUtil.findEjbLink(server, di, linkName);
             
             //if flag does not allow misconfigured ejb-links, it is an error
             if ( ( jndiName == null ) && !(getLenientEjbLink()) )
                 throw new NamingException("ejb-ref: "+name+", no ejb-link match");
         }

         
         //fall through to the jndiName
         if ( jndiName == null )
         { 
             jndiName = ejb.getJndiName();
             if (jndiName == null )
                 throw new NamingException("ejb-ref: "+name+", no ejb-link in web.xml and no jndi-name in jboss-web.xml");
         }
         
         log.debug("Linking ejb-ref: "+name+" to JNDI name: "+jndiName);
         Util.bind(envCtx, name, new LinkRef(jndiName));
      }
   }

   protected void linkEjbLocalRefs(Iterator ejbRefs, Context envCtx, DeploymentInfo di)
      throws NamingException
   {
      while( ejbRefs.hasNext() )
      {
         EjbLocalRefMetaData ejb = (EjbLocalRefMetaData) ejbRefs.next();
         String name = ejb.getName();
         String linkName = ejb.getLink();
         String jndiName = null;

         //use the ejb-link field if it is specified
         if ( linkName != null )
         {
             jndiName = EjbUtil.findLocalEjbLink(server, di, linkName);
             
             //if flag does not allow misconfigured ejb-links, it is an error    
             if ( ( jndiName == null ) && !(getLenientEjbLink()) )
                 throw new NamingException("ejb-ref: "+name+", no ejb-link match");
         }

         
         if (jndiName == null)
         {
             jndiName = ejb.getJndiName();
             if ( jndiName == null )
             {
                String msg = null;
                if( linkName == null )
                {
                  msg = "ejb-local-ref: '"+name+"', no ejb-link in web.xml and "
                   + "no local-jndi-name in jboss-web.xml";
                }
                else
                {
                   msg = "ejb-local-ref: '"+name+"', with web.xml ejb-link: '"
                   + linkName + "' failed to resolve to an ejb with a LocalHome";
                }
                throw new NamingException(msg);
             }
         }

         log.debug("Linking ejb-local-ref: "+name+" to JNDI name: "+jndiName);
         Util.bind(envCtx, name, new LinkRef(jndiName));
      }
   }

   /** This creates a java:comp/env/security context that contains a
   securityMgr binding pointing to an AuthenticationManager implementation
   and a realmMapping binding pointing to a RealmMapping implementation.
   If the jboss-web.xml descriptor contained a security-domain element
   then the bindings are LinkRefs to the jndi name specified by the
   security-domain element. If there was no security-domain element then
   the bindings are to NullSecurityManager instance which simply allows
   all access.
   */
   protected void linkSecurityDomain(String securityDomain, Context envCtx)
      throws NamingException
   {
      if( securityDomain == null )
      {
         log.debug("Binding security/securityMgr to NullSecurityManager");
         Object securityMgr = new NullSecurityManager("java:/jaas/null");
         Util.bind(envCtx, "security/securityMgr", securityMgr);
         Util.bind(envCtx, "security/realmMapping", securityMgr);
         Util.bind(envCtx, "security/security-domain", new LinkRef("java:/jaas/null"));
         Util.bind(envCtx, "security/subject", new LinkRef("java:/jaas/null/subject"));
      }
      else
      {
         log.debug("Linking security/securityMgr to JNDI name: "+securityDomain);
         Util.bind(envCtx, "security/securityMgr", new LinkRef(securityDomain));
         Util.bind(envCtx, "security/realmMapping", new LinkRef(securityDomain));
         Util.bind(envCtx, "security/security-domain", new LinkRef(securityDomain));
         Util.bind(envCtx, "security/subject", new LinkRef(securityDomain+"/subject"));
      }
   }

   /** A utility method that searches the given loader for the
    resources: "javax/servlet/resources/web-app_2_3.dtd",
    "org/apache/jasper/resources/jsp12.dtd", and "javax/ejb/EJBHome.class"
    and returns an array of URL strings. Any jar: urls are reduced to the
    underlying <url> portion of the 'jar:<url>!/{entry}' construct.
    */
   public String[] getStandardCompileClasspath(ClassLoader loader)
   {
      String[] jspResources = {
         "javax/servlet/resources/web-app_2_3.dtd",
         "org/apache/jasper/resources/jsp12.dtd",
         "javax/ejb/EJBHome.class"
      };
      ArrayList tmp = new ArrayList();
      for(int j = 0; j < jspResources.length; j ++)
      {
         URL rsrcURL = loader.getResource(jspResources[j]);
         if( rsrcURL != null )
         {
            String url = rsrcURL.toExternalForm();
            if( rsrcURL.getProtocol().equals("jar") )
            {
               // Parse the jar:<url>!/{entry} URL
               url = url.substring(4);
               int seperator = url.indexOf('!');
               url = url.substring(0, seperator);
            }
            tmp.add(url);
         }
         else
         {
            log.warn("Failed to fin jsp rsrc: "+jspResources[j]);
         }
      }
      log.trace("JSP StandardCompileClasspath: " + tmp);
      String[] cp = new String[tmp.size()];
      tmp.toArray(cp);
      return cp;
   }

   /** A utility method that walks up the ClassLoader chain starting at
    the given loader and queries each ClassLoader for a 'URL[] getURLs()'
    method from which a complete classpath of URL strings is built.
    */
   public String[] getCompileClasspath(ClassLoader loader)
   {
      HashSet tmp = new HashSet();
      ClassLoader cl = loader;
      while( cl != null )
      {
         URL[] urls = getClassLoaderURLs(cl);
         addURLs(tmp, urls);
         cl = cl.getParent();
      }
      try
      {
         URL[] globalUrls = (URL[])getServer().getAttribute(LoaderRepositoryFactory.DEFAULT_LOADER_REPOSITORY,
                                                         "URLs");
         addURLs(tmp, globalUrls);
      }
      catch (Exception e)
      {
         log.warn("Could not get global URL[] from default loader repository!");
      } // end of try-catch
      log.trace("JSP CompileClasspath: " + tmp);
      String[] cp = new String[tmp.size()];
      tmp.toArray(cp);
      return cp;
   }

   private void addURLs(Set urlSet, URL[] urls)
   {
      for(int u = 0; u < urls.length; u ++)
      {
         URL url = urls[u];
         urlSet.add(url.toExternalForm());
      }
   }

   /** Use reflection to access a URL[] getURLs method so that non-URLClassLoader
    *class loaders that support this method can provide info.
    */
   protected URL[] getClassLoaderURLs(ClassLoader cl)
   {
      URL[] urls = {};
      try
      {
         Class returnType = urls.getClass();
         Class[] parameterTypes = {};
         Method getURLs = cl.getClass().getMethod("getURLs", parameterTypes);
         if( returnType.isAssignableFrom(getURLs.getReturnType()) )
         {
            Object[] args = {};
            urls = (URL[]) getURLs.invoke(cl, args);
         }
         if( urls == null || urls.length == 0 )
         {
            getURLs = cl.getClass().getMethod("getAllURLs", parameterTypes);
            if( returnType.isAssignableFrom(getURLs.getReturnType()) )
            {
               Object[] args = {};
               urls = (URL[]) getURLs.invoke(cl, args);
            }
         }
      }
      catch(Exception ignore)
      {
      }
      return urls;
   }

   /** This method creates a context-root string from either the
      WEB-INF/jboss-web.xml context-root element is one exists, or the
      filename portion of the warURL. It is called if the DeploymentInfo
      webContext value is null which indicates a standalone war deployment.
      A war name of ROOT.war is handled as a special case of a war that
      should be installed as the default web context.
    */
   protected void parseMetaData(String ctxPath, URL warURL, String warName,
      WebMetaData metaData)
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
               metaData.importXml(web);
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
               metaData.importXml(jbossWeb);
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
         webContext = metaData.getContextRoot();
      if( webContext == null )
      {
         // Build the context from the war name, strip the .war suffix
         webContext = warName;
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
      metaData.setContextRoot(webContext);
   }

   /** An inner class that maps the WebDescriptorParser.parseWebAppDescriptors()
   onto the protected parseWebAppDescriptors() AbstractWebContainer method.
   */
   private class DescriptorParser implements WebDescriptorParser
   {
      DeploymentInfo di;
      DescriptorParser(DeploymentInfo di)
      {
         this.di = di;
      }
      public void parseWebAppDescriptors(ClassLoader loader, WebMetaData metaData) throws Exception
      {
         AbstractWebContainer.this.parseWebAppDescriptors(di, loader, metaData);
      }
      public DeploymentInfo getDeploymentInfo()
      {
         return di;
      }
   }
}
