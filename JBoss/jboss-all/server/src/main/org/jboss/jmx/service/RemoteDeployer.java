/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.jmx.service;

import java.net.URL;
import java.net.MalformedURLException;

import java.util.List;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.Hashtable;

import javax.management.ObjectName;
import javax.naming.Context;
import javax.naming.InitialContext;
import gnu.getopt.Getopt;
import gnu.getopt.LongOpt;

import org.jboss.jmx.adaptor.rmi.RMIAdaptor;
import org.jboss.jmx.connector.rmi.RMIConnectorImpl;
import org.jboss.jmx.connector.RemoteMBeanServer;

import org.jboss.deployment.MainDeployerMBean;
import org.jboss.deployment.Deployer;
import org.jboss.deployment.DeployerMBean;
import org.jboss.deployment.DeploymentException;

import org.jboss.mx.util.MBeanProxyExt;
import org.jboss.util.Strings;

import org.jboss.logging.Logger;

/**
 * A JMX client to deploy an application into a running JBoss server via RMI.
 *
 * @version <tt>$Revision: 1.4.2.3 $</tt>
 * @author  <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author  <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @author  <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class RemoteDeployer
   implements Deployer, DeployerMBean
{
   /** Class logger. */
   private static final Logger log = Logger.getLogger(Deployer.class);
   
   /** Default adapter name */
   private static final String DEFAULT_ADAPTER_NAME = "jmx/rmi/RMIAdaptor";
   
   /** A proxy to the deployer instance on the remote server. */
   protected Deployer deployer;
   
   /**
    * Construct a new <tt>RemoteDeployer</tt>.
    */
   public RemoteDeployer(ObjectName deployerName, Hashtable env, String adapterName)
      throws Exception
   {
      init(deployerName, env, adapterName);
   }
   
   /**
    * Construct a new <tt>RemoteDeployer</tt>.
    *
    * @param url   The URL of the JNDI provider or null to use the default.
    */
   public RemoteDeployer(ObjectName deployerName, String url, String adapterName) throws Exception
   {
      Hashtable env = null;
      
      if (url != null)
      {
         env = new Hashtable();
         env.put(Context.PROVIDER_URL, url);
      }
      
      init(deployerName, env, adapterName);
   }
   
   /**
    * Construct a new <tt>RemoteDeployer</tt>.
    *
    * <p>Uses MainDeployer and the given url for Context.PROVIDER_URL.
    */
   public RemoteDeployer(String url, String adapterName) throws Exception
   {
      this(MainDeployerMBean.OBJECT_NAME, url, adapterName);
   }

   /**
    * Construct a new <tt>RemoteDeployer</tt>.
    *
    * <p>Uses MainDeployer.
    */
   public RemoteDeployer() throws Exception
   {
      this(MainDeployerMBean.OBJECT_NAME, (Hashtable)null, DEFAULT_ADAPTER_NAME);
   }

   protected void init(ObjectName deployerName, Hashtable env, String adapterName)
      throws Exception
   {
      RemoteMBeanServer server = lookupRemoteMBeanServer(env, adapterName);
      deployer = (Deployer)MBeanProxyExt.create(Deployer.class, deployerName, server);
   }
   
   /**
    * Lookup the RemoteMBeanServer which will be used to invoke methods on.
    *
    * @param env            The initial context environment or null to use default.
    * @param adapterName    The JNDI name of the RMI adapter or null for the default.
    *
    * @throws Exception   Failed to lookup connector reference or retruned reference
    *                     was not of type {@link RMIAdapter}.
    */
   protected RemoteMBeanServer lookupRemoteMBeanServer(Hashtable env, String adapterName)
      throws Exception
   {
      RemoteMBeanServer server = null;
      InitialContext ctx;
      if (env == null)
      {
         ctx = new InitialContext();
      }
      else
      {
         ctx = new InitialContext(env);
      }
      
      if (adapterName == null)
      {
         adapterName = DEFAULT_ADAPTER_NAME;
      }      
      
      try
      {
         Object obj = ctx.lookup( adapterName );
         log.debug("RMI Adapter: " + obj);
         
         if (!(obj instanceof RMIAdaptor))
         {
            throw new RuntimeException("Object not of type: RMIAdaptorImpl, but: " +
            (obj == null ? "not found" : obj.getClass().getName()));
         }
         
         server = new RMIConnectorImpl((RMIAdaptor) obj);
         log.debug("Remote MBean Server : " + server);
      }
      finally
      {
         ctx.close();
      }
      
      return server;
   }
   
   /**
    * Deploys the given url on the remote server.
    *
    * @param url    The url of the application to deploy.
    *
    * @throws DeploymentException   Failed to deploy application.
    */
   public void deploy(final URL url) throws DeploymentException
   {
      deployer.deploy(url);
   }
   
   /**
    * Deploys the given url on the remote server.
    *
    * @param url    The url of the application to deploy.
    *
    * @throws DeploymentException      Failed to deploy application.
    * @throws MalformedURLException    Invalid URL.
    */
   public void deploy(final String url) throws MalformedURLException, DeploymentException
   {
      deployer.deploy(Strings.toURL(url));
   }
   
   /**
    * Undeploys the application specifed by the given url on the remote server.
    *
    * @param url    The url of the application to undeploy.
    *
    * @throws DeploymentException   Failed to undeploy application.
    */
   public void undeploy(final URL url) throws DeploymentException
   {
      deployer.undeploy(url);
   }
   
   /**
    * Undeploys the application specifed by the given url on the remote server.
    *
    * @param url    The url of the application to undeploy.
    *
    * @throws DeploymentException      Failed to undeploy application.
    * @throws MalformedURLException    Invalid URL.
    */
   public void undeploy(final String url) throws MalformedURLException, DeploymentException
   {
      deployer.undeploy(Strings.toURL(url));
   }
   
   /**
    * Check if the given url is deployed on thr remote server.
    *
    * @param url    The url of the application to check.
    * @return       True if the application is deployed.
    */
   public boolean isDeployed(final URL url)
   {
      return deployer.isDeployed(url);
   }
   
   /**
    * Check if the given url is deployed on thr remote server.
    *
    * @param url    The url of the application to check.
    * @return       True if the application is deployed.
    *
    * @throws DeploymentException      Failed to determine if application is deployed.
    * @throws MalformedURLException    Invalid URL.
    */
   public boolean isDeployed(final String url) throws MalformedURLException
   {
      return deployer.isDeployed(Strings.toURL(url));
   }
   
   
   /////////////////////////////////////////////////////////////////////////
   //                         Command Line Support                        //
   /////////////////////////////////////////////////////////////////////////
   
   public static final String PROGRAM_NAME = System.getProperty("program.name", "deployer");
   
   /**
    * Switches equate to commands for the desired deploy/undeploy operation to execute;
    * this is the base class for those commands.
    */
   protected static abstract class DeployerCommand
   {
      protected URL url;
      
      public DeployerCommand(String url) throws Exception
      {
         this.url = Strings.toURL(url);
      }
      
      public abstract void execute(Deployer deployer) throws Exception;
   }
   
   protected static void displayUsage()
   {
      System.out.println("usage: " + PROGRAM_NAME + " [options] (operation)+");
      System.out.println();
      System.out.println("options:");
      System.out.println("    -h, --help                Show this help message");
      System.out.println("    -D<name>[=<value>]        Set a system property");
      System.out.println("    --                        Stop processing options");
      System.out.println("    -s, --server=<url>        Specify the JNDI URL of the remote server");
      System.out.println("    -a, --adapter=<name>      Specify JNDI name of the RMI adapter to use [" + DEFAULT_ADAPTER_NAME + "]");
      System.out.println();
      System.out.println("operations:");
      System.out.println("    -d, --deploy=<url>        Deploy a URL into the remote server");
      System.out.println("    -u, --undeploy=<url>      Undeploy a URL from the remote server");
      System.out.println("    -r, --redeploy=<url>      Redeploy a URL from the remote server");
      System.out.println("    -i, --isdeployed=<url>    Check if a URL is deployed on the remote server");
      System.out.println();
   }
   
   public static void main(final String[] args) throws Exception
   {
      if (args.length == 0)
      {
         displayUsage();
         System.exit(0);
      }
      
      String sopts = "-:hD:s:d:u:i:r:a:";
      LongOpt[] lopts =
      {
         new LongOpt("help", LongOpt.NO_ARGUMENT, null, 'h'),
         new LongOpt("server", LongOpt.REQUIRED_ARGUMENT, null, 's'),
         new LongOpt("adapter", LongOpt.REQUIRED_ARGUMENT, null, 'a'),
         new LongOpt("deploy", LongOpt.REQUIRED_ARGUMENT, null, 'd'),
         new LongOpt("undeploy", LongOpt.REQUIRED_ARGUMENT, null, 'u'),
         new LongOpt("isdeployed", LongOpt.REQUIRED_ARGUMENT, null, 'i'),
         new LongOpt("redeploy", LongOpt.REQUIRED_ARGUMENT, null, 'r'),
      };
      
      Getopt getopt = new Getopt(PROGRAM_NAME, args, sopts, lopts);
      int code;
      String arg;
      
      List commands = new ArrayList();
      String serverURL = null;
      String adapterName = null;
      
      while ((code = getopt.getopt()) != -1)
      {
         switch (code)
         {
            case ':':
            case '?':
               // for now both of these should exit with error status
               System.exit(1);
               break; // for completeness
               
            case 1:
               // this will catch non-option arguments
               // (which we don't currently care about)
               System.err.println(PROGRAM_NAME + ": unused non-option argument: " +
               getopt.getOptarg());
               break; // for completeness
               
            case 'h':
               // show command line help
               displayUsage();
               System.exit(0);
               break; // for completeness
               
            case 'D':
            {
               // set a system property
               arg = getopt.getOptarg();
               String name, value;
               int i = arg.indexOf("=");
               if (i == -1)
               {
                  name = arg;
                  value = "true";
               }
               else
               {
                  name = arg.substring(0, i);
                  value = arg.substring(i + 1, arg.length());
               }
               System.setProperty(name, value);
               break;
            }
            
            case 's':
            {
               serverURL = getopt.getOptarg();
               break;
            }
            
            case 'a':
            {
               adapterName = getopt.getOptarg();
               break;
            }
            
            case 'd':
            {
               commands.add(new DeployerCommand(getopt.getOptarg())
               {
                  public void execute(Deployer deployer) throws Exception
                  {
                     deployer.deploy(url);
                     System.out.println(url + " has been deployed.");
                  }
               });
               break;
            }
            
            case 'u':
            {
               commands.add(new DeployerCommand(getopt.getOptarg())
               {
                  public void execute(Deployer deployer) throws Exception
                  {
                     deployer.undeploy(url);
                     System.out.println(url + " has been undeployed.");
                  }
               });
               break;
            }
            
            case 'r':
            {
               commands.add(new DeployerCommand(getopt.getOptarg())
               {
                  public void execute(Deployer deployer) throws Exception
                  {
                     if (deployer.isDeployed(url))
                     {
                        deployer.undeploy(url);
                     }
                     deployer.deploy(url);
                     System.out.println(url + " has been redeployed.");
                  }
               });
               break;
            }
            
            case 'i':
            {
               commands.add(new DeployerCommand(getopt.getOptarg())
               {
                  public void execute(Deployer deployer) throws Exception
                  {
                     boolean deployed = deployer.isDeployed(url);
                     System.out.println(url + " is " + (deployed ? "deployed." : "not deployed."));
                  }
               });
               break;
            }
            
            default:
               // this should not happen,
               // if it does throw an error so we know about it
               throw new Error("unhandled option code: " + code);
         }
      }
      
      // setup the deployer
      Deployer deployer = new RemoteDeployer(serverURL, adapterName);
      
      // now execute all of the commands
      Iterator iter = commands.iterator();
      while (iter.hasNext())
      {
         DeployerCommand command = (DeployerCommand)iter.next();
         command.execute(deployer);
      }
   }
}
