/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 * This software is derived from works developed by the
 * Apache Software Foundation (http://www.apache.org/), and its
 * redistribution and use are further subject to the terms of the
 * Apache Software License (see below), which is herein incorporated
 * by reference.
 *
 * ====================================================================
 *
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 1999-2001 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution, if
 *    any, must include the following acknowlegement:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowlegement may appear in the software itself,
 *    if and wherever such third-party acknowlegements normally appear.
 *
 * 4. The names "The Jakarta Project", "Tomcat", and "Apache Software
 *    Foundation" must not be used to endorse or promote products derived
 *    from this software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * [Additional notices, if required by prior licensing conditions]
 *
 */
package org.jboss.web.tomcat.tc4;


import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.net.JarURLConnection;
import java.net.URL;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.Iterator;
import java.util.MissingResourceException;
import java.util.ResourceBundle;
import java.util.Set;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;

import javax.naming.NameClassPair;
import javax.naming.NamingEnumeration;
import javax.naming.NamingException;
import javax.naming.directory.DirContext;
import javax.servlet.ServletContext;
import javax.servlet.ServletException;

import org.apache.catalina.Authenticator;
import org.apache.catalina.Connector;
import org.apache.catalina.Container;
import org.apache.catalina.Context;
import org.apache.catalina.Engine;
import org.apache.catalina.Host;
import org.apache.catalina.Lifecycle;
import org.apache.catalina.LifecycleEvent;
import org.apache.catalina.LifecycleListener;
import org.apache.catalina.Logger;
import org.apache.catalina.Pipeline;
import org.apache.catalina.Service;
import org.apache.catalina.Valve;
import org.apache.catalina.Wrapper;
import org.apache.catalina.core.ContainerBase;
import org.apache.catalina.core.StandardContext;
import org.apache.catalina.deploy.ApplicationParameter;
import org.apache.catalina.deploy.ErrorPage;
import org.apache.catalina.deploy.FilterDef;
import org.apache.catalina.deploy.FilterMap;
import org.apache.catalina.deploy.LoginConfig;
import org.apache.catalina.deploy.SecurityConstraint;
import org.apache.catalina.startup.Constants;
import org.apache.catalina.startup.ContextConfig;
import org.apache.catalina.startup.TldRuleSet;
import org.apache.catalina.startup.WebRuleSet;
import org.apache.catalina.util.StringManager;
import org.apache.commons.digester.Digester;
import org.xml.sax.InputSource;
import org.xml.sax.SAXParseException;

/**
 * Startup event listener for a <b>Context</b> that configures the properties
 * of that Context, and the associated defined servlets, using by default the
 * authenticator classes found in package
 * <code>org.jboss.web.tomcat.tc4.authenticator</code>.
 * <p>
 * Differs from the standard Tomcat ContextConfig class solely in that it
 * determines any authenticator valve to load by referring to property file
 * <code>org.jboss.web.tomcat.tc4.Authenticators</code> instead of
 * <code>org.apache.catalina.startup.Authenticators</code>.  This difference
 * is implemented in private method <code>authenticatorConfig</code>.
 *
 * @author B Stansberry, based almost entirely on work by Craig R. McClanahan
 *
 * @version $Revison$ $Date: 2003/11/22 08:12:14 $ derived from Tomcat Revision: 1.67
 */

public final class SingleSignOnContextConfig implements LifecycleListener
{


   // ----------------------------------------------------- Instance Variables


   /**
    * The set of Authenticators that we know how to configure.  The key is
    * the name of the implemented authentication method, and the value is
    * the fully qualified Java class name of the corresponding Valve.
    */
   private static ResourceBundle authenticators = null;


   /**
    * The Context we are associated with.
    */
   private Context context = null;


   /**
    * The debugging detail level for this component.
    */
   private int debug = 0;


   /**
    * Track any fatal errors during startup configuration processing.
    */
   private boolean ok = false;


   /**
    * The string resources for this package.
    */
   private static final StringManager sm =
         StringManager.getManager(Constants.Package);


   /**
    * The <code>Digester</code> we will use to process tag library
    * descriptor files.
    */
   private static Digester tldDigester = createTldDigester();


   /**
    * The <code>Digester</code> we will use to process web application
    * deployment descriptor files.
    */
   private static Digester webDigester = createWebDigester();


   // ------------------------------------------------------------- Properties


   /**
    * Return the debugging detail level for this component.
    */
   public int getDebug()
   {

      return (this.debug);

   }


   /**
    * Set the debugging detail level for this component.
    *
    * @param debug The new debugging detail level
    */
   public void setDebug(int debug)
   {

      this.debug = debug;

   }


   // --------------------------------------------------------- Public Methods


   /**
    * Process the START event for an associated Context.
    *
    * @param event The lifecycle event that has occurred
    */
   public void lifecycleEvent(LifecycleEvent event)
   {

      // Identify the context we are associated with
      try
      {
         context = (Context) event.getLifecycle();
         if (context instanceof StandardContext)
         {
            int contextDebug = ((StandardContext) context).getDebug();
            if (contextDebug > this.debug)
               this.debug = contextDebug;
         }
      }
      catch (ClassCastException e)
      {
         log(sm.getString("contextConfig.cce", event.getLifecycle()), e);
         return;
      }

      // Process the event that has occurred
      if (event.getType().equals(Lifecycle.START_EVENT))
         start();
      else if (event.getType().equals(Lifecycle.STOP_EVENT))
         stop();

   }


   // -------------------------------------------------------- Private Methods


   /**
    * Process the application configuration file, if it exists.
    */
   private void applicationConfig()
   {

      // Open the application web.xml file, if it exists
      InputStream stream = null;
      ServletContext servletContext = context.getServletContext();
      if (servletContext != null)
         stream = servletContext.getResourceAsStream
               (Constants.ApplicationWebXml);
      if (stream == null)
      {
         log(sm.getString("contextConfig.applicationMissing"));
         return;
      }

      // Process the application web.xml file
      synchronized (webDigester)
      {
         try
         {
            URL url =
                  servletContext.getResource(Constants.ApplicationWebXml);
            InputSource is = new InputSource(url.toExternalForm());
            is.setByteStream(stream);
            webDigester.setDebug(getDebug());
            if (context instanceof StandardContext)
            {
               ((StandardContext) context).setReplaceWelcomeFiles(true);
            }
            webDigester.clear();
            webDigester.push(context);
            webDigester.parse(is);
         }
         catch (SAXParseException e)
         {
            log(sm.getString("contextConfig.applicationParse"), e);
            log(sm.getString("contextConfig.applicationPosition",
                             "" + e.getLineNumber(),
                             "" + e.getColumnNumber()));
            ok = false;
         }
         catch (Exception e)
         {
            log(sm.getString("contextConfig.applicationParse"), e);
            ok = false;
         }
         finally
         {
            try
            {
               if (stream != null)
               {
                  stream.close();
               }
            }
            catch (IOException e)
            {
               log(sm.getString("contextConfig.applicationClose"), e);
            }
         }
      }

   }


   /**
    * Set up an Authenticator automatically if required, and one has not
    * already been configured.
    * <p>
    * Differs from the standard Tomcat ContextConfig class in that it
    * determines the authenticator to load by referring to property file
    * <code>org.jboss.web.tomcat.tc4.Authenticators</code> instead of
    * <code>org.apache.catalina.startup.Authenticators</code>.
    */
   private synchronized void authenticatorConfig()
   {

      // Does this Context require an Authenticator?
      SecurityConstraint constraints[] = context.findConstraints();
      if ((constraints == null) || (constraints.length == 0))
         return;
      LoginConfig loginConfig = context.getLoginConfig();
      if (loginConfig == null)
      {
         loginConfig = new LoginConfig("NONE", null, null, null);
         context.setLoginConfig(loginConfig);
      }

      // Has an authenticator been configured already?
      if (context instanceof Authenticator)
         return;
      if (context instanceof ContainerBase)
      {
         Pipeline pipeline = ((ContainerBase) context).getPipeline();
         if (pipeline != null)
         {
            Valve basic = pipeline.getBasic();
            if ((basic != null) && (basic instanceof Authenticator))
               return;
            Valve valves[] = pipeline.getValves();
            for (int i = 0; i < valves.length; i++)
            {
               if (valves[i] instanceof Authenticator)
                  return;
            }
         }
      }
      else
      {
         return;     // Cannot install a Valve even if it would be needed
      }

      // Has a Realm been configured for us to authenticate against?
      if (context.getRealm() == null)
      {
         log(sm.getString("contextConfig.missingRealm"));
         ok = false;
         return;
      }

      // Load our mapping properties if necessary
      if (authenticators == null)
      {
         try
         {
            // HERE IS THE DIFFERENCE FROM THE STANDARD TOMCAT VERSION
            authenticators = ResourceBundle.getBundle
                  ("org.jboss.web.tomcat.tc4.Authenticators");
         }
         catch (MissingResourceException e)
         {
            log(sm.getString("contextConfig.authenticatorResources"), e);
            ok = false;
            return;
         }
      }

      // Identify the class name of the Valve we should configure
      String authenticatorName = null;
      try
      {
         authenticatorName =
               authenticators.getString(loginConfig.getAuthMethod());
      }
      catch (MissingResourceException e)
      {
         authenticatorName = null;
      }
      if (authenticatorName == null)
      {
         log(sm.getString("contextConfig.authenticatorMissing",
                          loginConfig.getAuthMethod()));
         ok = false;
         return;
      }

      // Instantiate and install an Authenticator of the requested class
      Valve authenticator = null;
      try
      {
         Class authenticatorClass = Class.forName(authenticatorName);
         authenticator = (Valve) authenticatorClass.newInstance();
         if (context instanceof ContainerBase)
         {
            Pipeline pipeline = ((ContainerBase) context).getPipeline();
            if (pipeline != null)
            {
               ((ContainerBase) context).addValve(authenticator);
               log(sm.getString("contextConfig.authenticatorConfigured",
                                loginConfig.getAuthMethod()));
            }
         }
      }
      catch (Throwable t)
      {
         log(sm.getString("contextConfig.authenticatorInstantiate",
                          authenticatorName), t);
         ok = false;
      }

   }


   /**
    * Create and deploy a Valve to expose the SSL certificates presented
    * by this client, if any.  If we cannot instantiate such a Valve
    * (because the JSSE classes are not available), silently continue.
    * This is only instantiated for those Contexts being served by
    * a Connector with secure set to true.
    */
   private void certificatesConfig()
   {

      // Only install this valve if there is a Connector installed
      // which has secure set to true.
      boolean secure = false;
      Container container = context.getParent();
      if (container instanceof Host)
      {
         container = container.getParent();
      }
      if (container instanceof Engine)
      {
         Service service = ((Engine) container).getService();
         // The service can be null when Tomcat is run in embedded mode
         if (service == null)
         {
            secure = true;
         }
         else
         {
            Connector[] connectors = service.findConnectors();
            for (int i = 0; i < connectors.length; i++)
            {
               secure = connectors[i].getSecure();
               if (secure)
               {
                  break;
               }
            }
         }
      }
      if (!secure)
      {
         return;
      }

      // Validate that the JSSE classes are present
      try
      {
         Class clazz = this.getClass().getClassLoader().loadClass
               ("javax.net.ssl.SSLSocket");
         if (clazz == null)
            return;
      }
      catch (Throwable t)
      {
         return;
      }

      // Instantiate a new CertificatesValve if possible
      Valve certificates = null;
      try
      {
         Class clazz =
               Class.forName("org.apache.catalina.valves.CertificatesValve");
         certificates = (Valve) clazz.newInstance();
      }
      catch (Throwable t)
      {
         return;     // Probably JSSE classes not present
      }

      // Add this Valve to our Pipeline
      try
      {
         if (context instanceof ContainerBase)
         {
            Pipeline pipeline = ((ContainerBase) context).getPipeline();
            if (pipeline != null)
            {
               ((ContainerBase) context).addValve(certificates);
               log(sm.getString
                   ("contextConfig.certificatesConfig.added"));
            }
         }
      }
      catch (Throwable t)
      {
         log(sm.getString("contextConfig.certificatesConfig.error"), t);
         ok = false;
      }

   }


   /**
    * Create (if necessary) and return a Digester configured to process a tag
    * library descriptor, looking for additional listener classes to be
    * registered.
    */
   private static Digester createTldDigester()
   {

      URL url = null;
      Digester tldDigester = new Digester();
      tldDigester.setValidating(true);
      url = ContextConfig.class.getResource(Constants.TldDtdResourcePath_11);
      tldDigester.register(Constants.TldDtdPublicId_11,
                           url.toString());
      url = ContextConfig.class.getResource(Constants.TldDtdResourcePath_12);
      tldDigester.register(Constants.TldDtdPublicId_12,
                           url.toString());
      tldDigester.addRuleSet(new TldRuleSet());
      return (tldDigester);

   }


   /**
    * Create (if necessary) and return a Digester configured to process the
    * web application deployment descriptor (web.xml).
    */
   private static Digester createWebDigester()
   {

      URL url = null;
      Digester webDigester = new Digester();
      webDigester.setValidating(true);
      url = ContextConfig.class.getResource(Constants.WebDtdResourcePath_22);
      webDigester.register(Constants.WebDtdPublicId_22,
                           url.toString());
      url = ContextConfig.class.getResource(Constants.WebDtdResourcePath_23);
      webDigester.register(Constants.WebDtdPublicId_23,
                           url.toString());
      webDigester.addRuleSet(new WebRuleSet());
      return (webDigester);

   }


   /**
    * Process the default configuration file, if it exists.
    */
   private void defaultConfig()
   {

      // Open the default web.xml file, if it exists
      File file = new File(Constants.DefaultWebXml);
      if (!file.isAbsolute())
         file = new File(System.getProperty("catalina.base"),
                         Constants.DefaultWebXml);
      FileInputStream stream = null;
      try
      {
         stream = new FileInputStream(file.getCanonicalPath());
         stream.close();
         stream = null;
      }
      catch (FileNotFoundException e)
      {
         log(sm.getString("contextConfig.defaultMissing"));
         return;
      }
      catch (IOException e)
      {
         log(sm.getString("contextConfig.defaultMissing"), e);
         return;
      }

      // Process the default web.xml file
      synchronized (webDigester)
      {
         try
         {
            InputSource is =
                  new InputSource("file://" + file.getAbsolutePath());
            stream = new FileInputStream(file);
            is.setByteStream(stream);
            webDigester.setDebug(getDebug());
            if (context instanceof StandardContext)
               ((StandardContext) context).setReplaceWelcomeFiles(true);
            webDigester.clear();
            webDigester.push(context);
            webDigester.parse(is);
         }
         catch (SAXParseException e)
         {
            log(sm.getString("contextConfig.defaultParse"), e);
            log(sm.getString("contextConfig.defaultPosition",
                             "" + e.getLineNumber(),
                             "" + e.getColumnNumber()));
            ok = false;
         }
         catch (Exception e)
         {
            log(sm.getString("contextConfig.defaultParse"), e);
            ok = false;
         }
         finally
         {
            try
            {
               if (stream != null)
               {
                  stream.close();
               }
            }
            catch (IOException e)
            {
               log(sm.getString("contextConfig.defaultClose"), e);
            }
         }
      }

   }


   /**
    * Log a message on the Logger associated with our Context (if any)
    *
    * @param message Message to be logged
    */
   private void log(String message)
   {

      Logger logger = null;
      if (context != null)
         logger = context.getLogger();
      if (logger != null)
         logger.log("SingleSignOnContextConfig[" + context.getName() + "]: "
                    + message);
      else
         System.out.println("SingleSignOnContextConfig[" + context.getName()
                            + "]: " + message);

   }


   /**
    * Log a message on the Logger associated with our Context (if any)
    *
    * @param message Message to be logged
    * @param throwable Associated exception
    */
   private void log(String message, Throwable throwable)
   {

      Logger logger = null;
      if (context != null)
         logger = context.getLogger();
      if (logger != null)
         logger.log("SingleSignOnContextConfig[" + context.getName() + "] "
                    + message, throwable);
      else
      {
         System.out.println("SingleSignOnContextConfig[" + context.getName()
                            + "]: " + message);
         System.out.println("" + throwable);
         throwable.printStackTrace(System.out);
      }

   }


   /**
    * Process a "start" event for this Context.
    */
   private synchronized void start()
   {

      if (debug > 0)
         log(sm.getString("contextConfig.start"));
      context.setConfigured(false);
      ok = true;

      // Set properties based on DefaultContext
      Container container = context.getParent();
      if (!context.getOverride())
      {
         if (container instanceof Host)
         {
            ((Host) container).importDefaultContext(context);
            container = container.getParent();
         }
         if (container instanceof Engine)
         {
            ((Engine) container).importDefaultContext(context);
         }
      }

      // Process the default and application web.xml files
      defaultConfig();
      applicationConfig();
      if (ok)
      {
         validateSecurityRoles();
      }

      // Scan tag library descriptor files for additional listener classes
      if (ok)
      {
         try
         {
            tldScan();
         }
         catch (Exception e)
         {
            log(e.getMessage(), e);
            ok = false;
         }
      }

      // Configure a certificates exposer valve, if required
      if (ok)
         certificatesConfig();

      // Configure an authenticator if we need one
      if (ok)
         authenticatorConfig();

      // Dump the contents of this pipeline if requested
      if ((debug >= 1) && (context instanceof ContainerBase))
      {
         log("Pipline Configuration:");
         Pipeline pipeline = ((ContainerBase) context).getPipeline();
         Valve valves[] = null;
         if (pipeline != null)
            valves = pipeline.getValves();
         if (valves != null)
         {
            for (int i = 0; i < valves.length; i++)
            {
               log("  " + valves[i].getInfo());
            }
         }
         log("======================");
      }

      // Make our application available if no problems were encountered
      if (ok)
         context.setConfigured(true);
      else
      {
         log(sm.getString("contextConfig.unavailable"));
         context.setConfigured(false);
      }

   }


   /**
    * Process a "stop" event for this Context.
    */
   private synchronized void stop()
   {

      if (debug > 0)
         log(sm.getString("contextConfig.stop"));

      int i;

      // Removing children
      Container[] children = context.findChildren();
      for (i = 0; i < children.length; i++)
      {
         context.removeChild(children[i]);
      }

      // Removing application listeners
      String[] applicationListeners = context.findApplicationListeners();
      for (i = 0; i < applicationListeners.length; i++)
      {
         context.removeApplicationListener(applicationListeners[i]);
      }

      // Removing application parameters
      ApplicationParameter[] applicationParameters =
            context.findApplicationParameters();
      for (i = 0; i < applicationParameters.length; i++)
      {
         context.removeApplicationParameter
               (applicationParameters[i].getName());
      }

      // Removing security constraints
      SecurityConstraint[] securityConstraints = context.findConstraints();
      for (i = 0; i < securityConstraints.length; i++)
      {
         context.removeConstraint(securityConstraints[i]);
      }

      // Removing Ejbs
      /*
      ContextEjb[] contextEjbs = context.findEjbs();
      for (i = 0; i < contextEjbs.length; i++) {
          context.removeEjb(contextEjbs[i].getName());
      }
      */

      // Removing environments
      /*
      ContextEnvironment[] contextEnvironments = context.findEnvironments();
      for (i = 0; i < contextEnvironments.length; i++) {
          context.removeEnvironment(contextEnvironments[i].getName());
      }
      */

      // Removing errors pages
      ErrorPage[] errorPages = context.findErrorPages();
      for (i = 0; i < errorPages.length; i++)
      {
         context.removeErrorPage(errorPages[i]);
      }

      // Removing filter defs
      FilterDef[] filterDefs = context.findFilterDefs();
      for (i = 0; i < filterDefs.length; i++)
      {
         context.removeFilterDef(filterDefs[i]);
      }

      // Removing filter maps
      FilterMap[] filterMaps = context.findFilterMaps();
      for (i = 0; i < filterMaps.length; i++)
      {
         context.removeFilterMap(filterMaps[i]);
      }

      // Removing instance listeners
      String[] instanceListeners = context.findInstanceListeners();
      for (i = 0; i < instanceListeners.length; i++)
      {
         context.removeInstanceListener(instanceListeners[i]);
      }

      // Removing local ejbs
      /*
      ContextLocalEjb[] contextLocalEjbs = context.findLocalEjbs();
      for (i = 0; i < contextLocalEjbs.length; i++) {
          context.removeLocalEjb(contextLocalEjbs[i].getName());
      }
      */

      // Removing Mime mappings
      String[] mimeMappings = context.findMimeMappings();
      for (i = 0; i < mimeMappings.length; i++)
      {
         context.removeMimeMapping(mimeMappings[i]);
      }

      // Removing parameters
      String[] parameters = context.findParameters();
      for (i = 0; i < parameters.length; i++)
      {
         context.removeParameter(parameters[i]);
      }

      // Removing resource env refs
      /*
      String[] resourceEnvRefs = context.findResourceEnvRefs();
      for (i = 0; i < resourceEnvRefs.length; i++) {
          context.removeResourceEnvRef(resourceEnvRefs[i]);
      }
      */

      // Removing resource links
      /*
      ContextResourceLink[] contextResourceLinks =
          context.findResourceLinks();
      for (i = 0; i < contextResourceLinks.length; i++) {
          context.removeResourceLink(contextResourceLinks[i].getName());
      }
      */

      // Removing resources
      /*
      ContextResource[] contextResources = context.findResources();
      for (i = 0; i < contextResources.length; i++) {
          context.removeResource(contextResources[i].getName());
      }
      */

      // Removing sercurity role
      String[] securityRoles = context.findSecurityRoles();
      for (i = 0; i < securityRoles.length; i++)
      {
         context.removeSecurityRole(securityRoles[i]);
      }

      // Removing servlet mappings
      String[] servletMappings = context.findServletMappings();
      for (i = 0; i < servletMappings.length; i++)
      {
         context.removeServletMapping(servletMappings[i]);
      }

      // FIXME : Removing status pages

      // Removing taglibs
      String[] taglibs = context.findTaglibs();
      for (i = 0; i < taglibs.length; i++)
      {
         context.removeTaglib(taglibs[i]);
      }

      // Removing welcome files
      String[] welcomeFiles = context.findWelcomeFiles();
      for (i = 0; i < welcomeFiles.length; i++)
      {
         context.removeWelcomeFile(welcomeFiles[i]);
      }

      // Removing wrapper lifecycles
      String[] wrapperLifecycles = context.findWrapperLifecycles();
      for (i = 0; i < wrapperLifecycles.length; i++)
      {
         context.removeWrapperLifecycle(wrapperLifecycles[i]);
      }

      // Removing wrapper listeners
      String[] wrapperListeners = context.findWrapperListeners();
      for (i = 0; i < wrapperListeners.length; i++)
      {
         context.removeWrapperListener(wrapperListeners[i]);
      }

      ok = true;

   }


   /**
    * Scan for and configure all tag library descriptors found in this
    * web application.
    *
    * @exception Exception if a fatal input/output or parsing error occurs
    */
   private void tldScan() throws Exception
   {

      // Acquire this list of TLD resource paths to be processed
      Set resourcePaths = tldScanResourcePaths();

      // Scan each accumulated resource paths for TLDs to be processed
      Iterator paths = resourcePaths.iterator();
      while (paths.hasNext())
      {
         String path = (String) paths.next();
         if (path.endsWith(".jar"))
         {
            tldScanJar(path);
         }
         else
         {
            tldScanTld(path);
         }
      }

   }


   /**
    * Scan the JAR file at the specified resource path for TLDs in the
    * <code>META-INF</code> subdirectory, and scan them for application
    * event listeners that need to be registered.
    *
    * @param resourcePath Resource path of the JAR file to scan
    *
    * @exception Exception if an exception occurs while scanning this JAR
    */
   private void tldScanJar(String resourcePath) throws Exception
   {

      if (debug >= 1)
      {
         log(" Scanning JAR at resource path '" + resourcePath + "'");
      }

      JarFile jarFile = null;
      String name = null;
      InputStream inputStream = null;
      try
      {
         URL url = context.getServletContext().getResource(resourcePath);
         if (url == null)
         {
            throw new IllegalArgumentException
                  (sm.getString("contextConfig.tldResourcePath",
                                resourcePath));
         }
         url = new URL("jar:" + url.toString() + "!/");
         JarURLConnection conn =
               (JarURLConnection) url.openConnection();
         conn.setUseCaches(false);
         jarFile = conn.getJarFile();
         Enumeration entries = jarFile.entries();
         while (entries.hasMoreElements())
         {
            JarEntry entry = (JarEntry) entries.nextElement();
            name = entry.getName();
            if (!name.startsWith("META-INF/"))
            {
               continue;
            }
            if (!name.endsWith(".tld"))
            {
               continue;
            }
            if (debug >= 2)
            {
               log("  Processing TLD at '" + name + "'");
            }
            inputStream = jarFile.getInputStream(entry);
            tldScanStream(inputStream);
            inputStream.close();
            inputStream = null;
            name = null;
         }
         // FIXME - Closing the JAR file messes up the class loader???
         //            jarFile.close();
      }
      catch (Exception e)
      {
         if (name == null)
         {
            throw new ServletException
                  (sm.getString("contextConfig.tldJarException",
                                resourcePath), e);
         }
         else
         {
            throw new ServletException
                  (sm.getString("contextConfig.tldEntryException",
                                name, resourcePath), e);
         }
      }
      finally
      {
         if (inputStream != null)
         {
            try
            {
               inputStream.close();
            }
            catch (Throwable t)
            {
               ;
            }
            inputStream = null;
         }
         if (jarFile != null)
         {
            // FIXME - Closing the JAR file messes up the class loader???
            //                try {
            //                    jarFile.close();
            //                } catch (Throwable t) {
            //                    ;
            //                }
            jarFile = null;
         }
      }

   }


   /**
    * Scan the TLD contents in the specified input stream, and register
    * any application event listeners found there.  <b>NOTE</b> - It is
    * the responsibility of the caller to close the InputStream after this
    * method returns.
    *
    * @param resourceStream InputStream containing a tag library descriptor
    *
    * @exception Exception if an exception occurs while scanning this TLD
    */
   private void tldScanStream(InputStream resourceStream)
         throws Exception
   {

      synchronized (tldDigester)
      {
         tldDigester.clear();
         tldDigester.push(context);
         tldDigester.parse(resourceStream);
      }

   }


   /**
    * Scan the TLD contents at the specified resource path, and register
    * any application event listeners found there.
    *
    * @param resourcePath Resource path being scanned
    *
    * @exception Exception if an exception occurs while scanning this TLD
    */
   private void tldScanTld(String resourcePath) throws Exception
   {

      if (debug >= 1)
      {
         log(" Scanning TLD at resource path '" + resourcePath + "'");
      }

      InputStream inputStream = null;
      try
      {
         inputStream =
               context.getServletContext().getResourceAsStream(resourcePath);
         if (inputStream == null)
         {
            throw new IllegalArgumentException
                  (sm.getString("contextConfig.tldResourcePath",
                                resourcePath));
         }
         tldScanStream(inputStream);
         inputStream.close();
         inputStream = null;
      }
      catch (Exception e)
      {
         throw new ServletException
               (sm.getString("contextConfig.tldFileException", resourcePath),
                e);
      }
      finally
      {
         if (inputStream != null)
         {
            try
            {
               inputStream.close();
            }
            catch (Throwable t)
            {
               ;
            }
            inputStream = null;
         }
      }

   }


   /**
    * Accumulate and return a Set of resource paths to be analyzed for
    * tag library descriptors.  Each element of the returned set will be
    * the context-relative path to either a tag library descriptor file,
    * or to a JAR file that may contain tag library descriptors in its
    * <code>META-INF</code> subdirectory.
    *
    * @exception IOException if an input/output error occurs while
    *  accumulating the list of resource paths
    */
   private Set tldScanResourcePaths() throws IOException
   {

      if (debug >= 1)
      {
         log(" Accumulating TLD resource paths");
      }
      Set resourcePaths = new HashSet();

      // Accumulate resource paths explicitly listed in the web application
      // deployment descriptor
      if (debug >= 2)
      {
         log("  Scanning <taglib> elements in web.xml");
      }
      String taglibs[] = context.findTaglibs();
      for (int i = 0; i < taglibs.length; i++)
      {
         String resourcePath = context.findTaglib(taglibs[i]);
         // FIXME - Servlet 2.3 DTD implies that the location MUST be
         // a context-relative path starting with '/'?
         if (!resourcePath.startsWith("/"))
         {
            resourcePath = "/WEB-INF/" + resourcePath;
         }
         if (debug >= 3)
         {
            log("   Adding path '" + resourcePath +
                "' for URI '" + taglibs[i] + "'");
         }
         resourcePaths.add(resourcePath);
      }

      // Scan TLDs in the /WEB-INF subdirectory of the web application
      if (debug >= 2)
      {
         log("  Scanning TLDs in /WEB-INF subdirectory");
      }
      DirContext resources = context.getResources();
      try
      {
         NamingEnumeration items = resources.list("/WEB-INF");
         while (items.hasMoreElements())
         {
            NameClassPair item = (NameClassPair) items.nextElement();
            String resourcePath = "/WEB-INF/" + item.getName();
            // FIXME - JSP 1.2 is not explicit about whether we should
            // scan subdirectories of /WEB-INF for TLDs also
            if (!resourcePath.endsWith(".tld"))
            {
               continue;
            }
            if (debug >= 3)
            {
               log("   Adding path '" + resourcePath + "'");
            }
            resourcePaths.add(resourcePath);
         }
      }
      catch (NamingException e)
      {
         ; // Silent catch: it's valid that no /WEB-INF directory exists
      }

      // Scan JARs in the /WEB-INF/lib subdirectory of the web application
      if (debug >= 2)
      {
         log("  Scanning JARs in /WEB-INF/lib subdirectory");
      }
      try
      {
         NamingEnumeration items = resources.list("/WEB-INF/lib");
         while (items.hasMoreElements())
         {
            NameClassPair item = (NameClassPair) items.nextElement();
            String resourcePath = "/WEB-INF/lib/" + item.getName();
            if (!resourcePath.endsWith(".jar"))
            {
               continue;
            }
            if (debug >= 3)
            {
               log("   Adding path '" + resourcePath + "'");
            }
            resourcePaths.add(resourcePath);
         }
      }
      catch (NamingException e)
      {
         ; // Silent catch: it's valid that no /WEB-INF/lib directory exists
      }

      // Return the completed set
      return (resourcePaths);

   }


   /**
    * Validate the usage of security role names in the web application
    * deployment descriptor.  If any problems are found, issue warning
    * messages (for backwards compatibility) and add the missing roles.
    * (To make these problems fatal instead, simply set the <code>ok</code>
    * instance variable to <code>false</code> as well).
    */
   private void validateSecurityRoles()
   {

      // Check role names used in <security-constraint> elements
      SecurityConstraint constraints[] = context.findConstraints();
      for (int i = 0; i < constraints.length; i++)
      {
         String roles[] = constraints[i].findAuthRoles();
         for (int j = 0; j < roles.length; j++)
         {
            if (!"*".equals(roles[j]) &&
                  !context.findSecurityRole(roles[j]))
            {
               log(sm.getString("contextConfig.role.auth", roles[j]));
               context.addSecurityRole(roles[j]);
            }
         }
      }

      // Check role names used in <servlet> elements
      Container wrappers[] = context.findChildren();
      for (int i = 0; i < wrappers.length; i++)
      {
         Wrapper wrapper = (Wrapper) wrappers[i];
         String runAs = wrapper.getRunAs();
         if ((runAs != null) && !context.findSecurityRole(runAs))
         {
            log(sm.getString("contextConfig.role.runas", runAs));
            context.addSecurityRole(runAs);
         }
         String names[] = wrapper.findSecurityReferences();
         for (int j = 0; j < names.length; j++)
         {
            String link = wrapper.findSecurityReference(names[j]);
            if ((link != null) && !context.findSecurityRole(link))
            {
               log(sm.getString("contextConfig.role.link", link));
               context.addSecurityRole(link);
            }
         }
      }

   }


}
