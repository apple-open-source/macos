/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.catalina;

import java.io.InputStream;
import java.io.IOException;
import java.io.File;
import java.io.FileInputStream;
import java.lang.reflect.Method;
import java.net.UnknownHostException;
import java.net.URL;
import java.net.URLClassLoader;
import java.net.MalformedURLException;
import java.net.InetAddress;
import java.security.ProtectionDomain;
import java.util.HashMap;
import java.util.Iterator;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.servlet.ServletContext;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.DocumentBuilder;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.xml.sax.EntityResolver;
import org.xml.sax.InputSource;

import org.jboss.deployment.DeploymentException;
import org.jboss.metadata.WebMetaData;
import org.jboss.security.SecurityDomain;
import org.jboss.util.file.Files;
import org.jboss.util.file.JarUtils;
import org.jboss.web.AbstractWebContainer;
import org.jboss.web.AbstractWebContainer.WebDescriptorParser;
import org.jboss.web.WebApplication;
import org.jboss.web.catalina.security.JBossSecurityMgrRealm;
import org.jboss.web.catalina.security.SSLServerSocketFactory;

import org.apache.log4j.Category;
import org.apache.catalina.Connector;
import org.apache.catalina.Container;
import org.apache.catalina.Context;
import org.apache.catalina.Deployer;
import org.apache.catalina.Engine;
import org.apache.catalina.Host;
import org.apache.catalina.Globals;
import org.apache.catalina.Lifecycle;
import org.apache.catalina.LifecycleEvent;
import org.apache.catalina.LifecycleException;
import org.apache.catalina.LifecycleListener;
import org.apache.catalina.Loader;
import org.apache.catalina.Logger;
import org.apache.catalina.Realm;
import org.apache.catalina.Valve;
import org.apache.catalina.authenticator.AuthenticatorBase;
import org.apache.catalina.core.StandardContext;
import org.apache.catalina.startup.Embedded;
import org.apache.catalina.valves.ValveBase;
import org.jboss.web.catalina.session.ClusterManager;
import org.jboss.web.catalina.session.ClusteringNotSupportedException;
import org.jboss.web.catalina.session.ClusteredSessionValve;
import org.jboss.web.catalina.session.InstantSnapshotManager;
import org.jboss.web.catalina.session.IntervalSnapshotManager;
import org.jboss.web.catalina.session.SnapshotManager;


/** An implementation of the AbstractWebContainer for the Jakarta Tomcat
 4.0 servlet container. This uses the org.apache.catalina.startup.Embedded as
 the integration class. It does not parse the catalina server.xml in the
 catalina distribution. Rather, it parses a subset of the server.xml syntax
 and obtains this configuration information from the Config attribute.
 
 @see org.jboss.web.AbstractWebContainer
 @see org.apache.catalina.startup.Embedded
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.17.2.4 $
 */
public class EmbeddedCatalinaServiceSX extends AbstractWebContainer
   implements EmbeddedCatalinaServiceSXMBean
{
   // Constants -----------------------------------------------------
   public static final String NAME = "EmbeddedCatalinaSX";

   /** The embedded instance used to configure catalina */
   private EmbeddedCatalina catalina;
   /** The catalina debug level */
   private int debugLevel;
   /** The value to use for the catalina.home System property */
   private String catalinaHome;
   /** The value to use for the catalina.base System property */
   private String catalinaBase;
   /** Any extended configuration information specified via a config
    element in the mbean definition.
    */
   private Element extendedConfig;
   /** A flag indicating if the Java2 parent delegation class loading model
    should be used.
    */
   private boolean useParentDelegation = true;
   /** A flag indicating if the working dir for a war deployment should be
    delete when the war is undeployed.
    */
   private boolean deleteWorkDirs = true;
   /** Which snapshot mode should be used in clustered environment?
       Default: instant
    */
   private String snapshotMode = "instant"; // instant or interval

   /** With IntervalSnapshotManager use this interval (in ms) for snapshotting */
   private int snapshotInterval = 1000;

   /** Should only modified sessions be distributed? */
   private boolean economicSnapshotting = false;
   
   public EmbeddedCatalinaServiceSX()
   {
   }

   public String getName()
   {
      return NAME;
   }

   public String getCatalinaHome()
   {
      return this.catalinaHome;
   }
   public void setCatalinaHome(String catalinaHome)
   {
      this.catalinaHome = catalinaHome;
   }

   public String getCatalinaBase()
   {
      return this.catalinaBase;
   }
   public void setCatalinaBase(String catalinaBase)
   {
      this.catalinaBase = catalinaBase;
   }

   /** Flag for the standard Java2 parent delegation class loading model
    rather than the servlet 2.3 load from war first model
    */
   public boolean getJava2ClassLoadingCompliance()
   {
      return this.useParentDelegation;
   }
   /** Enable the standard Java2 parent delegation class loading model
    rather than the servlet 2.3 load from war first model
    */
   public void setJava2ClassLoadingCompliance(boolean compliance)
   {
      this.useParentDelegation = compliance;
   }

   /** Get the delete work dirs on undeployment flag.
    @see #setDeleteWorkDirs(boolean)
    */
   public boolean getDeleteWorkDirs()
   {
      return this.deleteWorkDirs;
   }
   /** Set the delete work dirs on undeployment flag. By default catalina
    does not delete its working directories when a context is stopped and
    this can cause jsp pages in redeployments to not be recompiled if the
    timestap of the file in the war has not been updated. This defaults to true.
    */
   public void setDeleteWorkDirs(boolean flag)
   {
      this.deleteWorkDirs = flag;
   }
   
   /** Set the snapshot mode. Currently supported: instant or interval */
   public void setSnapshotMode(String mode)
   {
      this.snapshotMode=mode;
   }

   /** Get the snapshot mode */
   public String getSnapshotMode()
   {
      return this.snapshotMode;
   }

   /** Set the snapshot interval in milliseconds for snapshot mode = interval */
   public void setSnapshotInterval(int interval)
   {
      this.snapshotInterval=interval;
   }

   /** Get the snapshot interval */
   public int getSnapshotInterval()
   {
      return this.snapshotInterval;
   }

   public void setEconomicSnapshotting(boolean flag)
   {
      economicSnapshotting=flag;
   }

   public boolean getEconomicSnapshotting()
   {
      return this.economicSnapshotting;
   }

   public Element getConfig()
   {
      return this.extendedConfig;
   }
   /** This method is invoked to import an arbitrary XML configuration tree.
     Subclasses should override this method if they support such a configuration
     capability. This implementation does nothing.
    */
   public void setConfig(Element config)
   {
      this.extendedConfig = config;
   }

   public void startService() throws Exception
   {
      // Start create the embeded catalina container but don't let it overwrite the thread class loader
      ClassLoader cl = Thread.currentThread().getContextClassLoader();
      ClassLoader parent = cl;
      while( parent != null )
      {
         log.trace(parent);
         URL[] urls = super.getClassLoaderURLs(parent);
         for(int u = 0; u < urls.length; u ++)
            log.trace("  "+urls[u]);
         parent = parent.getParent();
      }

      // Determine the catalina debug level from the enabled priority
      debugLevel = 0;
      if( log.isTraceEnabled() )
         debugLevel = 2;
      log.debug("Setting catalina debug level to: "+debugLevel);

      try
      {
         // Set the catalina.home property from the Embedded class location
         if( catalinaHome == null )
         {
            ProtectionDomain pd = Embedded.class.getProtectionDomain();
            URL homeURL = pd.getCodeSource().getLocation();
            String homePath = homeURL.getFile();
            File homeDir = new File(homePath, "../../..");
            catalinaHome = homeDir.getCanonicalPath();
         }
         if( catalinaBase == null )
            catalinaBase = catalinaHome;
         log.debug("Setting catalina.home to: " + catalinaHome);
         log.debug("Setting catalina.base to: " + catalinaBase);
         System.setProperty("catalina.home", catalinaHome);
         System.setProperty("catalina.base", catalinaBase);
         initCatalina(cl);
         catalina.start();
      }
      finally
      {
         Thread.currentThread().setContextClassLoader(cl);
      }
      log.info("OK");

      // Invoke the super method to register as a deployer
      super.startService();
   }

   public void stopService() throws Exception
   {
      super.stopService();
      if( catalina != null )
      {
         catalina.stop();
      }
   }

   /** Perform the tomcat specific deployment steps.
    */
   protected void performDeploy(WebApplication appInfo, String warUrl,
      WebDescriptorParser webAppParser) throws Exception
   {
      WebMetaData metaData = appInfo.getMetaData();
      String ctxPath = metaData.getContextRoot();
      if( ctxPath.equals("/") )
      {
         ctxPath = "";
         metaData.setContextRoot(ctxPath);
      }
      log.info("deploy, ctxPath="+ctxPath+", warUrl="+warUrl);

      URL url = new URL(warUrl);
      // Catalina needs a war in a dir so extract the nested war
      if( url.getProtocol().equals("njar") )
      {
         url = org.jboss.net.protocol.njar.Handler.njarToFile(url);
         log.debug("Extracted war from njar, warUrl="+url);
         File warFile = new File(url.getFile());
         String warFileName = warFile.getName();
         warFileName = warFileName.substring(0, warFileName.length()-3);
         warFileName += "war";
         File warDir = new File(warFile.getParent(), warFileName);
         FileInputStream warStream = new FileInputStream(warFile);
         JarUtils.unjar(warStream, warDir);
         warStream.close();
         log.debug("Unpacked war into dir: "+warDir);
         url = warDir.toURL();
      }
      createWebContext(appInfo, url, webAppParser);
      log.debug("Initialized: "+appInfo);
   }

   /** Perform the tomcat specific deployment steps.
    */
   public void performUndeploy(String warUrl) throws Exception
   {
      // find the javax.servlet.ServletContext in the repository
      WebApplication appInfo = getDeployedApp(warUrl);
      if( appInfo == null )
      {
         log.debug("performUndeploy, no WebApplication found for URL "+warUrl);
         return;
      }

      log.info("undeploy, ctxPath="+appInfo.getMetaData().getContextRoot()+", warUrl="+warUrl);
      Context context = (Context) appInfo.getAppData();
      if(context == null)
         throw new DeploymentException("URL " + warUrl + " is not deployed");

      String ctxPath = context.getPath();
      Deployer deployer = (Deployer) context.getParent();
      deployer.remove(ctxPath);
      if( deleteWorkDirs == true )
      {
         File workDir = (File) context.getServletContext().getAttribute(Globals.WORK_DIR_ATTR);
         log.debug("Deleting catalina work dir: "+workDir);
         Files.delete(workDir);
      }
   }

   /** Create and configure a org.apache.catalina.startup.Embedded
    instance. We do not use the server.xml file as we obtain all
    of the required customization from our mbean properties.
    */
   private void initCatalina(ClassLoader parent) throws Exception
   {
      Logger jbossLog = new Log4jLogger(this.log);
      Realm jbossRealm = new JBossSecurityMgrRealm();
      catalina = new EmbeddedCatalina(jbossLog, jbossRealm);
      catalina.setDebug(debugLevel);
      catalina.setUseNaming(false);

      // Apply any extended configuration
      ConfigHandler handler = new ConfigHandler(log);
      handler.applyHostConfig(extendedConfig, catalina, debugLevel > 0);
   }

   private void createWebContext(final WebApplication appInfo, URL warUrl,
      final WebDescriptorParser webAppParser) throws Exception
   {
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      WebMetaData metaData = appInfo.getMetaData();
      String ctxPath = metaData.getContextRoot();
      appInfo.setName(warUrl.getPath());
      appInfo.setClassLoader(loader);
      appInfo.setURL(warUrl);
      final StandardContext context = (StandardContext) catalina.createContext(ctxPath, warUrl.getFile());
      context.setParentClassLoader(loader);
      // Create a web application info object
      appInfo.setAppData(context);

      String hostName = metaData.getVirtualHost();
      Host virtualHost = catalina.findHost(hostName);
     
      if(metaData.getDistributable()) { 
         // Try to initate clustering, fallback to standard if no clustering is available 
         try
         {
            ClusterManager manager = new ClusterManager(context, this.log);
            manager.setContainer(context);
            context.setManager(manager);
            
            // choose the snapshot manager
            SnapshotManager snap = null;
            if(snapshotMode.equals("instant"))
            {
               snap = new InstantSnapshotManager(manager, context, economicSnapshotting);
            }
            else if(snapshotMode.equals("interval"))
            {
               snap = new IntervalSnapshotManager(manager, context, snapshotInterval, economicSnapshotting);
            }
            else
            {
               log.error("Snapshot mode must be 'instant' or 'interval' - using 'instant'");
               snap = new InstantSnapshotManager(manager, context, economicSnapshotting);
            }
            // Adding session snapshot valve
            ValveBase valve = new ClusteredSessionValve(snap);
            valve.setContainer(context);
            context.addValve(valve);
   
            log.info("Enabled clustering support for ctxPath="+ctxPath);
         }
         catch(ClusteringNotSupportedException e)
         {
            log.error("Failed to setup clustering, clustering disabled");
         }
      }
      
      /* We need to establish the JNDI ENC prior to the start of the web container
       so that init on startup servlets are able to interact with their ENC. We
       hook into the context lifecycle events to be notified of the start of the
       context as this occurs before the servlets are started. */
      final org.jboss.logging.Logger theLog = super.log;
      context.addLifecycleListener(new LifecycleListener()
         {
            public void lifecycleEvent(LifecycleEvent event)
            {
               Object source = event.getSource();
               if( source == context && event.getType().equals(Lifecycle.START_EVENT) )
               {
                  theLog.debug("Context.lifecycleEvent, event="+event);
                  contextInit(context, appInfo, webAppParser);
               }
            }
         }
      );
      initENC(appInfo, webAppParser);

      // A debug level of 1 is rather verbose so only enable debugging if trace priority is active
      if( debugLevel <= 1 )
         context.setDebug(0);
      virtualHost.addChild(context);
   }

   private void initENC(WebApplication appInfo, WebDescriptorParser webAppParser)
      throws Exception
   {
      ClassLoader tcl = Thread.currentThread().getContextClassLoader();
      WebMetaData metaData = appInfo.getMetaData();
      webAppParser.parseWebAppDescriptors(tcl, metaData);
   }

   /** Build the web application ENC.
    */
    private void contextInit(Context context, WebApplication appInfo, WebDescriptorParser webAppParser)
    {
        try
        {
           ServletContext servletCtx = context.getServletContext();
           if (servletCtx == null)
              return;

            /* We need to go through the context valves and set the cache flag
             on any AuthenticatorBase to false or else the JBossSecurityMgrRealm
             is not asked to authenticate every request. This can result in
             an authenticated user thread not receiving its authenticated
             Subject and this results in an authorization failure.
             */
            Valve[] valves = ((StandardContext)context).getValves();
            for(int v = 0; v < valves.length; v ++)
            {
               Valve valve = valves[v];
               if( valve instanceof AuthenticatorBase )
               {
                  AuthenticatorBase auth = (AuthenticatorBase) valve;
                  auth.setCache(false);
               }
            }

            // Add all of the classpth elements
            ClassLoader rsrcLoader = Thread.currentThread().getContextClassLoader();
            String[] jspCP = getCompileClasspath(rsrcLoader);
            Loader ctxLoader = context.getLoader();
            for(int u = 0; u < jspCP.length; u ++)
            {
               ctxLoader.addRepository(jspCP[u]);
            }

            // Setup the wep app JNDI java:comp/env namespace
            ClassLoader scl = context.getLoader().getClassLoader();
            // Enable parent delegation class loading
            try
            {
               Class[] signature = {boolean.class};
               Method setDelegate = scl.getClass().getMethod("setDelegate", signature);
               Object[] args = {new Boolean(this.useParentDelegation)};
               setDelegate.invoke(scl, args);
               log.info("Using Java2 parent classloader delegation: "+useParentDelegation);
            }
            catch(Exception e)
            {
               log.warn("Unable to invoke setDelegate on class loader:"+scl);
            }
        }
        catch(Exception e)
        {
            log.error("Failed to setup web application ENC", e);
        }
    }

}
