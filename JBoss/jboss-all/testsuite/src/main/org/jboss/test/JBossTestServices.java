/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test;

import java.io.File;
import java.net.URL;
import java.net.MalformedURLException;
import java.util.Hashtable;

import javax.management.ObjectName;
import javax.management.MalformedObjectNameException;
import javax.naming.InitialContext;
import javax.naming.Context;
import javax.security.auth.login.LoginContext;

import org.apache.log4j.Category;

import org.jboss.jmx.adaptor.rmi.RMIAdaptor;
import org.jboss.test.util.AppCallbackHandler;

/**
 * This is provides services for jboss junit test cases and TestSetups. It supplies
 * access to log4j logging, the jboss jmx server, jndi, and a method for
 * deploying ejb packages. You may supply the JNDI name under which the
 * RMIAdaptor interface is located via the system property jbosstest.server.name
 * default (jmx/rmi/RMIAdaptor) and the directory for deployable packages with
 * the system property jbosstest.deploy.dir (default ../lib).
 *
 * Should be subclassed to derive junit support for specific services integrated
 * into jboss.
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @author <a href="mailto:christoph.jung@jboss.org">Christoph G. Jung</a>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.14.2.8 $
 */
public class JBossTestServices
{
   // Constants -----------------------------------------------------
   public final static String DEPLOYER_NAME = "jboss.system:service=MainDeployer";
   public final static String DEFAULT_USERNAME = "jduke";
   public final static String DEFAULT_PASSWORD = "theduke";
   public final static String DEFAULT_LOGIN_CONFIG = "other";
   public final static int DEFAULT_THREADCOUNT = 10;
   public final static int DEFAULT_ITERATIONCOUNT = 1000;
   public final static int DEFAULT_BEANCOUNT = 100;

   // Attributes ----------------------------------------------------
   protected RMIAdaptor server;
   protected Category log;
   protected InitialContext initialContext;
   protected Hashtable jndiEnv;   
   protected LoginContext lc;

   // Static --------------------------------------------------------
   // Constructors --------------------------------------------------
   /**
    * Constructor for the JBossTestCase object
    *
    * @param name  Test case name
    */
   public JBossTestServices(String className)
   {
      log = Category.getInstance(className);
      log.debug("JBossTestServices(), className="+className);
   }
   
   // Public --------------------------------------------------------
   
   
   /**
    * The JUnit setup method
    *
    * @exception Exception  Description of Exception
    */
   public void setUp() throws Exception
   {
      log.debug("JBossTestServices.setUp()");
      init();
      log.info("jbosstest.beancount: " + System.getProperty("jbosstest.beancount"));
      log.info("jbosstest.iterationcount: " + System.getProperty("jbosstest.iterationcount"));
      log.info("jbosstest.threadcount: " + System.getProperty("jbosstest.threadcount"));
      log.info("jbosstest.nodeploy: " + System.getProperty("jbosstest.nodeploy"));
      log.info("jbosstest.jndiurl: " + this.getJndiURL());
      log.info("jbosstest.jndifactory: " + this.getJndiInitFactory());
   }

   /**
    * The teardown method for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void tearDown() throws Exception
   {
      // server = null;
      log.debug("JBossTestServices.tearDown()");
   }
   
   
   //protected---------
   
   /**
    * Gets the InitialContext attribute of the JBossTestCase object
    *
    * @return   The InitialContext value
    */
   InitialContext getInitialContext() throws Exception
   {
      return initialContext;
   }
   
   /**
    * Gets the Server attribute of the JBossTestCase object
    *
    * @return   The Server value
    */
   RMIAdaptor getServer () throws Exception
   {
      return server;
   }
   
   /**
    * Gets the Log attribute of the JBossTestCase object
    *
    * @return   The Log value
    */
   Category getLog()
   {
      return log;
   }
   
   /**
    * Gets the Main Deployer Name attribute of the JBossTestCase object
    *
    * @return                                  The Main DeployerName value
    * @exception MalformedObjectNameException  Description of Exception
    */
   ObjectName getDeployerName() throws MalformedObjectNameException
   {
      return new ObjectName(DEPLOYER_NAME);
   }
   
   
   /**
    * Returns the deployment directory to use. This does it's best to figure out
    * where you are looking. If you supply a complete url, it returns it.
    * Otherwise, it looks for jbosstest.deploy.dir or if missing ../lib. Then it
    * tries to construct a file url or a url.
    *
    * @param filename                   name of the file/url you want
    * @return                           A more or less canonical string for the
    *      url.
    * @exception MalformedURLException  Description of Exception
    */
   protected String getDeployURL(final String filename) throws MalformedURLException
   {
      //First see if it is already a complete url.
      try
      {
         return new URL(filename).toString();
      }
      catch (MalformedURLException mue)
      {
      }
      //OK, lets see if we can figure out what it might be.
      String deployDir = System.getProperty("jbosstest.deploy.dir");
      if (deployDir == null)
      {
         deployDir = "../lib";
      }
      String url = deployDir + "/" + filename;
      //try to canonicalize the strings a bit.
      if (new File(url).exists())
      {
         return new File(url).toURL().toString();
      }
      else
      {
         return new URL(url).toString();
      }
   }
   
   /**
    * invoke wraps an invoke call to the mbean server in a lot of exception
    * unwrapping.
    *
    * @param name           ObjectName of the mbean to be called
    * @param method         mbean method to be called
    * @param args           Object[] of arguments for the mbean method.
    * @param sig            String[] of types for the mbean methods parameters.
    * @return               Object returned by mbean method invocation.
    * @exception Exception  Description of Exception
    */
   protected Object invoke(ObjectName name, String method, Object[] args, String[] sig) throws Exception
   {
      return invoke (getServer(), name, method, args, sig);
   }
   
   protected Object invoke (RMIAdaptor server, ObjectName name, String method, Object[] args, String[] sig)
      throws Exception
   {
      try
      {
         return server.invoke(name, method, args, sig);
      }
      catch (javax.management.MBeanException e)
      {
         log.error("MbeanException", e.getTargetException());
         throw e.getTargetException();
      }
      catch (javax.management.ReflectionException e)
      {
         log.error("ReflectionException", e.getTargetException());
         throw e.getTargetException();
      }
      catch (javax.management.RuntimeOperationsException e)
      {
         log.error("RuntimeOperationsException", e.getTargetException());
         throw e.getTargetException();
      }
      catch (javax.management.RuntimeMBeanException e)
      {
         log.error("RuntimeMbeanException", e.getTargetException());
         throw e.getTargetException();
      }
      catch (javax.management.RuntimeErrorException e)
      {
         log.error("RuntimeErrorException", e.getTargetError());
         throw e.getTargetError();
      }
   }
   

   /**
    * Deploy a package with the main deployer. The supplied name is
    * interpreted as a url, or as a filename in jbosstest.deploy.lib or ../lib.
    *
    * @param name           filename/url of package to deploy.
    * @exception Exception  Description of Exception
    */
   public void deploy(String name) throws Exception
   {
      if( Boolean.getBoolean("jbosstest.nodeploy") == true )
      {
         log.debug("Skipping deployment of: "+name);
         return;
      }

      String deployURL = getDeployURL(name);
      log.debug("Deploying "+name+", url="+deployURL);
      invoke(getDeployerName(),
         "deploy",
         new Object[] {deployURL},
         new String[] {"java.lang.String"});
   }
   public void redeploy(String name) throws Exception
   {
      if( Boolean.getBoolean("jbosstest.nodeploy") == true )
      {
         log.debug("Skipping redeployment of: "+name);
         return;
      }

      String deployURL = getDeployURL(name);
      log.debug("Deploying "+name+", url="+deployURL);
      invoke(getDeployerName(),
         "redeploy",
         new Object[] {deployURL},
         new String[] {"java.lang.String"});
   }

   /** Do a JAAS login with the current username, password and login config.
    * @throws Exception
    */ 
   public void login() throws Exception
   {
      flushAuthCache();
      String username = getUsername();
      String pass = getPassword();
      String config = getLoginConfig();
      char[] password = null;
      if( pass != null )
         password = pass.toCharArray();
      AppCallbackHandler handler = new AppCallbackHandler(username, password);
      getLog().debug("Creating LoginContext("+config+")");
      lc = new LoginContext(config, handler);
      lc.login();
      getLog().debug("Created LoginContext, subject="+lc.getSubject());
   }

   public void logout()
   {
      try
      {
         if( lc != null )
            lc.logout();
      }
      catch(Exception e)
      {
         getLog().error("logout error: ", e);
      }
   }

   /**
    * Undeploy a package with the main deployer. The supplied name is
    * interpreted as a url, or as a filename in jbosstest.deploy.lib or ../lib.
    *
    * @param name           filename/url of package to undeploy.
    * @exception Exception  Description of Exception
    */
   public void undeploy(String name) throws Exception
   {
      if( Boolean.getBoolean("jbosstest.nodeploy") == true )
         return;
      String deployName = getDeployURL(name);
      Object[] args = {deployName};
      String[] sig = {"java.lang.String"}; 
      invoke(getDeployerName(), "undeploy", args, sig);
   }

   /** Flush all authentication credentials for the java:/jaas/other security
    domain
   */
   void flushAuthCache() throws Exception
   {
      ObjectName jaasMgr = new ObjectName("jboss.security:service=JaasSecurityManager");
      Object[] params = {"other"};
      String[] signature = {"java.lang.String"};
      invoke(jaasMgr, "flushAuthenticationCache", params, signature);
   }

   void restartDBPool() throws Exception
   {
      ObjectName dbPool = new ObjectName("jboss.jca:service=ManagedConnectionPool,name=DefaultDS");
      Object[] params = {};
      String[] signature = {};
      invoke(dbPool, "stop", params, signature);
      invoke(dbPool, "start", params, signature);
   }

   boolean isSecure()
   {
      return Boolean.getBoolean("jbosstest.secure");
   }
   String getUsername()
   {
      return System.getProperty("jbosstest.username", DEFAULT_USERNAME);
   }
   String getPassword()
   {
      return System.getProperty("jbosstest.password", DEFAULT_PASSWORD);
   }
   String getLoginConfig()
   {
      return System.getProperty("jbosstest.loginconfig", DEFAULT_LOGIN_CONFIG);
   }
   String getJndiURL()
   {
      String url = (String) jndiEnv.get(Context.PROVIDER_URL);
      return url;
   }
   String getJndiInitFactory()
   {
      String factory = (String) jndiEnv.get(Context.INITIAL_CONTEXT_FACTORY);
      return factory;
   }

   int getThreadCount()
   {
      return Integer.getInteger("jbosstest.threadcount", DEFAULT_THREADCOUNT).intValue();
   }

   int getIterationCount()
   {
      return Integer.getInteger("jbosstest.iterationcount", DEFAULT_ITERATIONCOUNT).intValue();
   }

   int getBeanCount()
   {
      return Integer.getInteger("jbosstest.beancount", DEFAULT_BEANCOUNT).intValue();
   }

   //private methods--------------
   
   /** Lookup the RMIAdaptor interface from JNDI. By default this is bound
    * under "jmx/rmi/RMIAdaptor" and this may be overriden with the
    * jbosstest.server.name system property.
    */
   protected void init() throws Exception
   {
      if (initialContext == null)
      {
         initialContext = new InitialContext();
         jndiEnv = initialContext.getEnvironment();
      }
      if (server == null)
      {
         String adaptorName = System.getProperty("jbosstest.server.name");
         if( adaptorName == null )
            adaptorName = "jmx/invoker/RMIAdaptor";
         server = (RMIAdaptor) initialContext.lookup(adaptorName);
      }
   }
   
}
