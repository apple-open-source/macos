/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.tomcat.tc4;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.lang.reflect.Method;
import java.net.URL;
import java.security.ProtectionDomain;
import java.util.Iterator;
import javax.servlet.ServletContext;
import javax.management.ObjectName;
import javax.naming.InitialContext;

import org.w3c.dom.Element;

import org.jboss.deployment.DeploymentException;
import org.jboss.deployment.DeploymentInfo;
import org.jboss.metadata.WebMetaData;
import org.jboss.util.file.Files;
import org.jboss.web.AbstractWebContainer;
import org.jboss.web.WebApplication;
import org.jboss.web.tomcat.tc4.ConfigHandler;
import org.jboss.web.tomcat.tc4.EmbeddedCatalina;
import org.jboss.web.tomcat.security.JBossSecurityMgrRealm;

import org.apache.catalina.Container;
import org.apache.catalina.Context;
import org.apache.catalina.Deployer;
import org.apache.catalina.Host;
import org.apache.catalina.Globals;
import org.apache.catalina.Lifecycle;
import org.apache.catalina.LifecycleEvent;
import org.apache.catalina.LifecycleListener;
import org.apache.catalina.Loader;
import org.apache.catalina.Logger;
import org.apache.catalina.Realm;
import org.apache.catalina.Valve;
import org.apache.catalina.valves.ValveBase;
import org.apache.catalina.authenticator.AuthenticatorBase;
import org.apache.catalina.core.StandardContext;
import org.apache.catalina.core.StandardWrapper;
import org.apache.catalina.startup.Embedded;

import org.jboss.web.tomcat.mbean.ServletInfo;
import org.jboss.web.tomcat.session.ClusterManager;
import org.jboss.web.tomcat.session.ClusteredSessionValve;
import org.jboss.web.tomcat.session.ClusteringNotSupportedException;
import org.jboss.web.tomcat.session.InstantSnapshotManager;
import org.jboss.web.tomcat.session.IntervalSnapshotManager;
import org.jboss.web.tomcat.session.SnapshotManager;
import org.jboss.web.tomcat.tc4.statistics.ContainerStatsValve;
import org.jboss.web.tomcat.statistics.InvocationStatistics;
import org.jboss.web.tomcat.Log4jLogger;


/** An implementation of the AbstractWebContainer for the Jakarta Tomcat
 4.1 servlet container. This uses the org.apache.catalina.startup.Embedded as
 the integration class. It does not parse the catalina server.xml in the
 catalina distribution. Rather, it parses a subset of the server.xml syntax
 and obtains this configuration information from the Config attribute.

 @see org.jboss.web.AbstractWebContainer
 @see org.apache.catalina.startup.Embedded

 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.1.1.2.5 $
 */
public class EmbeddedTomcatService extends AbstractWebContainer
   implements EmbeddedTomcatServiceMBean
{
   // Constants -----------------------------------------------------
   public static final String NAME = "EmbeddedCatalina4.1.x";

   /** The embedded instance used to configure catalina */
   private EmbeddedCatalina catalina;
   /** The catalina debug level */
   private int debugLevel;
   /** The value to use for the catalina.home System property */
   private String catalinaHome;
   /** The value to use for the catalina.base System property */
   private String catalinaBase;
   /** A flag indicating if the JBoss Loader should be used */
   private boolean useJBossWebLoader = true;
   /** Any extended configuration information specified via a config
    element in the mbean definition.
    */
   private Element extendedConfig;
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
   /** Get the request attribute name under which the JAAS Subject is store */
   private String subjectAttributeName = null;

   /** The web ctx invocation statistics */
   private InvocationStatistics stats = new InvocationStatistics();

   public EmbeddedTomcatService()
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

   public boolean getUseJBossWebLoader()
   {
      return useJBossWebLoader;
   }

   public void setUseJBossWebLoader(boolean flag)
   {
      this.useJBossWebLoader = flag;
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
      this.snapshotMode = mode;
   }
   /** Get the snapshot mode */
   public String getSnapshotMode()
   {
      return this.snapshotMode;
   }

   /** Set the snapshot interval in milliseconds for snapshot mode = interval */
   public void setSnapshotInterval(int interval)
   {
      this.snapshotInterval = interval;
   }
   /** Get the snapshot interval */
   public int getSnapshotInterval()
   {
      return this.snapshotInterval;
   }

   public String getSubjectAttributeName()
   {
      return this.subjectAttributeName;
   }
   public void setSubjectAttributeName(String name)
   {
      this.subjectAttributeName = name;
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

   /** Get the active thread count */
   public int getActiveThreadCount()
   {
      return stats.concurrentCalls;
   }   

   /** Get the maximal active thread count */
   public int getMaxActiveThreadCount()
   {
      return stats.maxConcurrentCalls;
   }

   public InvocationStatistics getStats()
   {
      return stats;
   }

   public void resetStats()
   {
      stats.resetStats();
   }

   public void startService() throws Exception
   {
      // Start create the embeded catalina container but don't let it overwrite the thread class loader
      ClassLoader cl = Thread.currentThread().getContextClassLoader();
      ClassLoader parent = cl;
      while (parent != null)
      {
         log.trace(parent);
         URL[] urls = super.getClassLoaderURLs(parent);
         for (int u = 0; u < urls.length; u++)
            log.trace("  " + urls[u]);
         parent = parent.getParent();
      }

      // Determine the catalina debug level from the enabled priority
      debugLevel = 0;
      if (log.isTraceEnabled())
         debugLevel = 3;
      log.debug("Setting catalina debug level to: " + debugLevel);

      try
      {
         // Set the catalina.home property from the Embedded class location
         if (catalinaHome == null)
         {
            ProtectionDomain pd = Embedded.class.getProtectionDomain();
            URL homeURL = pd.getCodeSource().getLocation();
            String homePath = homeURL.getFile();
            File homeDir = new
               File(homePath).getParentFile().getParentFile().getParentFile();
            catalinaHome = homeDir.getCanonicalPath();
         }
         if (catalinaBase == null)
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
      if (catalina != null)
      {
         catalina.stop();
      }
   }

   /** Perform the tomcat specific deployment steps.
    */
   protected void performDeploy(WebApplication appInfo, String warUrl,
      AbstractWebContainer.WebDescriptorParser webAppParser) throws Exception
   {
      WebMetaData metaData = appInfo.getMetaData();
      String ctxPath = metaData.getContextRoot();
      if (ctxPath.equals("/"))
      {
         ctxPath = "";
         metaData.setContextRoot(ctxPath);
      }
      log.info("deploy, ctxPath=" + ctxPath + ", warUrl=" + warUrl);

      URL url = new URL(warUrl);
      createWebContext(appInfo, url, webAppParser);
      log.debug("Initialized: " + appInfo);
   }

   /** Perform the tomcat specific deployment steps.
    */
   public void performUndeploy(String warUrl) throws Exception
   {
      // find the javax.servlet.ServletContext in the repository
      WebApplication appInfo = getDeployedApp(warUrl);
      if (appInfo == null)
      {
         log.debug("performUndeploy, no WebApplication found for URL " + warUrl);
         return;
      }
      log.info("undeploy, ctxPath=" + appInfo.getMetaData().getContextRoot() + ", warUrl=" + warUrl);

      // Unreqister the servlet mbeans
      DeploymentInfo di = appInfo.getDeploymentInfo();
      Iterator iter = di.mbeans.iterator();
      while (iter.hasNext())
      {
         ObjectName name = (ObjectName) iter.next();
         try
         {
            server.unregisterMBean(name);
         }
         catch (Exception ignore)
         {
         }
      }

      StandardContext context = (StandardContext) appInfo.getAppData();
      if (context == null)
         throw new DeploymentException("URL " + warUrl + " is not deployed");

      File workDir = (File) context.getServletContext().getAttribute(Globals.WORK_DIR_ATTR);
      String ctxPath = context.getPath();
      Deployer deployer = (Deployer) context.getParent();
      deployer.remove(ctxPath);

      // Cleanup the webapp resources
      Container[] children = context.findChildren();
      for(int i = 0; i < children.length; i ++)
         context.removeChild(children[i]);
      LifecycleListener[] listeners = context.findLifecycleListeners();
      for(int i = 0; i < listeners.length; i ++)
         context.removeLifecycleListener(listeners[i]);

      // Unbind the ENC now that the web app is shutdown
      ClassLoader currentLoader = Thread.currentThread().getContextClassLoader();
      ClassLoader encLoader = appInfo.getMetaData().getENCLoader();
      Thread.currentThread().setContextClassLoader(encLoader);
      InitialContext ctx = new InitialContext();
      ctx.unbind("java:comp/env");
      Thread.currentThread().setContextClassLoader(currentLoader);

      context.setLoader(null);
      context.setLogger(null);
      context.setManager(null);
      context.setParent(null);
      context.setParentClassLoader(null);
      context.setRealm(null);
      context.setResources(null);

      if (workDir != null && deleteWorkDirs == true)
      {
         log.debug("Deleting catalina work dir: " + workDir);
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
      JBossSecurityMgrRealm jbossRealm = new JBossSecurityMgrRealm();
      jbossRealm.setSubjectAttributeName(subjectAttributeName);
      catalina = new EmbeddedCatalina(jbossLog, jbossRealm);
      catalina.setDebug(debugLevel);
      catalina.setUseNaming(false);

      // Deploy default web.xml to catalina.home, if running from a .sar
      try
      {
         ClassLoader cl = Thread.currentThread().getContextClassLoader();
         InputStream is = cl.getResourceAsStream("web.xml");
         if (is != null)
         {
            File confDir = new File(catalinaHome, "conf");
            confDir.mkdirs();
            File webXml = new File(catalinaHome, "conf/web.xml");
            FileOutputStream os = new FileOutputStream(webXml);
            byte[] buf = new byte[512];
            while (true)
            {
               int n = is.read(buf);
               if (n < 0)
               {
                  break;
               }
               os.write(buf, 0, n);
            }
            os.close();
            is.close();
         }
         else
         {
            log.info("Assuming Tomcat standalone");
         }
      }
      catch (Exception e)
      {
         // Ignore
      }

      // Apply any extended configuration
      ConfigHandler handler = new ConfigHandler(log);
      handler.applyHostConfig(extendedConfig, catalina, debugLevel > 0);
   }

   private void createWebContext(final WebApplication appInfo, URL warUrl,
      final AbstractWebContainer.WebDescriptorParser webAppParser) throws Exception
   {
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      WebMetaData metaData = appInfo.getMetaData();
      String ctxPath = metaData.getContextRoot();
      appInfo.setName(warUrl.getPath());
      appInfo.setClassLoader(loader);
      appInfo.setURL(warUrl);
      final StandardContext context = (StandardContext) catalina.createContext(ctxPath, warUrl.getFile());
      JBossSecurityMgrRealm jbossRealm = new JBossSecurityMgrRealm();
      jbossRealm.setSubjectAttributeName(subjectAttributeName);
      context.setRealm(jbossRealm);
      if (useJBossWebLoader == true)
      {
         WebCtxLoader webLoader = new WebCtxLoader(loader);
         webLoader.setWarURL(warUrl);
         context.setLoader(webLoader);
      }
      else
      {
         context.setParentClassLoader(loader);
      }

      // Create a web application info object
      appInfo.setAppData(context);

      String hostName = metaData.getVirtualHost();
      Host virtualHost = catalina.findHost(hostName);

      if (metaData.getDistributable())
      {
         // Try to initate clustering, fallback to standard if no clustering is available
         try
         {
            ClusterManager manager =
               new ClusterManager(virtualHost,
                                  context,
                                  this.log,
                                  metaData);
            manager.setContainer(context);
            context.setManager(manager);

            // choose the snapshot manager
            SnapshotManager snap = null;
            if (snapshotMode.equals("instant"))
            {
               snap = new InstantSnapshotManager(manager, context);
            }
            else if (snapshotMode.equals("interval"))
            {
               snap = new IntervalSnapshotManager(manager, context, snapshotInterval);
            }
            else
            {
               log.error("Snapshot mode must be 'instant' or 'interval' - using 'instant'");
               snap = new InstantSnapshotManager(manager, context);
            }
            // Adding session snapshot valve
            ValveBase valve = new ClusteredSessionValve(snap);
            valve.setContainer(context);
            context.addValve(valve);

            log.info("Enabled clustering support for ctxPath=" + ctxPath);
         }
         catch (ClusteringNotSupportedException e)
         {
            log.error("Failed to setup clustering, clustering disabled");
         }
      }

      // Add the statistics valve to the context
      ContainerStatsValve valve = new ContainerStatsValve(stats);
      valve.setContainer(context);
      context.addValve(valve);

      // Set the session cookies flag according to metadata
      switch(metaData.getSessionCookies())
      {
         case WebMetaData.SESSION_COOKIES_ENABLED:
            context.setCookies(true);
            log.debug("Enabling session cookies");
            break;
         case WebMetaData.SESSION_COOKIES_DISABLED:
            context.setCookies(false);
            log.debug("Disabling session cookies");
            break;
         default:
            log.debug("Using session cookies default setting");
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
            if (source == context && event.getType().equals(Lifecycle.START_EVENT))
            {
               theLog.debug("Context.lifecycleEvent, event=" + event);
               contextInit(context, appInfo, webAppParser);
            }
         }
      }
      );
      initENC(appInfo, webAppParser);

      // A debug level of 1 is rather verbose so only enable debugging if trace priority is active
      if (debugLevel <= 1)
         context.setDebug(0);
      else
         context.setDebug(debugLevel);
      virtualHost.addChild(context);

      // Create mbeans for the servlets
      DeploymentInfo di = webAppParser.getDeploymentInfo();
      Container[] children = context.findChildren();
      for (int n = 0; n < children.length; n++)
      {
         if (children[n] instanceof StandardWrapper)
         {
            StandardWrapper servlet = (StandardWrapper) children[n];
            String name = servlet.getName();
            try
            {
               ObjectName oname = ServletInfo.createServletMBean(servlet, ctxPath,
                  virtualHost.getName(), getServiceName(), getServer());
               di.mbeans.add(oname);
            }
            catch (Exception e)
            {
               log.debug("Failed to create mbean for servlet: " + name, e);
            }
         }
      }
   }

   private void initENC(WebApplication appInfo, AbstractWebContainer.WebDescriptorParser webAppParser)
      throws Exception
   {
      ClassLoader tcl = Thread.currentThread().getContextClassLoader();
      WebMetaData metaData = appInfo.getMetaData();
      webAppParser.parseWebAppDescriptors(tcl, metaData);
   }

   /** Build the web application ENC.
    */
   private void contextInit(Context context, WebApplication appInfo,
      AbstractWebContainer.WebDescriptorParser webAppParser)
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
         StandardContext stdctx = (StandardContext) context;
         Valve[] valves = stdctx.getValves();
         for (int v = 0; v < valves.length; v++)
         {
            Valve valve = valves[v];
            if (valve instanceof AuthenticatorBase)
            {
               AuthenticatorBase auth = (AuthenticatorBase) valve;
               auth.setCache(false);
            }
         }
         // Install the JBossSecurityMgrRealm as valve to clear the SecurityAssociation
         Realm realm = stdctx.getRealm();
         if (realm instanceof Valve)
            stdctx.addValve((Valve) realm);

         // Add all of the classpth elements
         ClassLoader rsrcLoader = Thread.currentThread().getContextClassLoader();
         String[] jspCP = getCompileClasspath(rsrcLoader);
         Loader ctxLoader = context.getLoader();
         for (int u = 0; u < jspCP.length; u++)
         {
            ctxLoader.addRepository(jspCP[u]);
         }

         // Enable parent delegation class loading
         ClassLoader scl = context.getLoader().getClassLoader();
         try
         {
            Class[] signature = {boolean.class};
            Method setDelegate = scl.getClass().getMethod("setDelegate", signature);
            Boolean parentDelegation = new Boolean(appInfo.getMetaData().getJava2ClassLoadingCompliance());
            Object[] args = {parentDelegation};
            setDelegate.invoke(scl, args);
            log.info("Using Java2 parent classloader delegation: " + parentDelegation);
         }
         catch (Exception e)
         {
            log.warn("Unable to invoke setDelegate on class loader:" + scl);
         }
      }
      catch (Exception e)
      {
         log.error("Failed to setup web application ENC", e);
      }
   }

}
