/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.deployment.cache;

import java.net.URL;

import javax.management.ObjectName;
import javax.management.MBeanServer;

import org.jboss.system.ServiceMBeanSupport;
import org.jboss.system.MissingAttributeException;

import org.jboss.deployment.Deployer;
import org.jboss.deployment.DeploymentException;

import org.jboss.util.NullArgumentException;
import org.jboss.mx.util.MBeanProxyExt;
import org.jboss.mx.util.MBeanProxyInstance;

/**
 * A Deployer-like service which intercepts deploy/undeploy calls
 * to MainDeployer and provides local caching of target URLs using 
 * local disk.
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean, org.jboss.deployment.DeployerMBean"
 *
 * @todo clean up stale cache members
 *
 * @version <tt>$Revision: 1.5.4.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class DeploymentCache
   extends ServiceMBeanSupport
   implements Deployer, DeploymentCacheMBean
{
   /** A proxy to the deployer we are using. */
   protected Deployer deployer;

   /** A proxy to the deployment store we are using. */
   protected DeploymentStore store;


   /////////////////////////////////////////////////////////////////////////
   //                               Pluggables                            //
   /////////////////////////////////////////////////////////////////////////

   /**
    * @jmx:managed-attribute
    */
   public void setDeployer(final ObjectName deployerName)
   {
      if (deployerName == null)
         throw new NullArgumentException("deployerName");

      deployer = (Deployer)
         MBeanProxyExt.create(Deployer.class, deployerName, server);
   }

   /**
    * @jmx:managed-attribute
    */
   public ObjectName getDeployer()
   {
      return ((MBeanProxyInstance)deployer).getMBeanProxyObjectName();
   }

   /**
    * @jmx:managed-attribute
    */
   public void setStore(final ObjectName storeName)
   {
      if (storeName == null)
         throw new NullArgumentException("storeName");

      store = (DeploymentStore)
         MBeanProxyExt.create(DeploymentStore.class, storeName, server);
   }

   /**
    * @jmx:managed-attribute
    */
   public ObjectName getStore()
   {
      return ((MBeanProxyInstance)store).getMBeanProxyObjectName();
   }


   /////////////////////////////////////////////////////////////////////////
   //                               Deployer                              //
   /////////////////////////////////////////////////////////////////////////

   protected boolean isInvalid(final URL orig, final URL stored)
      throws Exception
   {
      boolean trace = log.isTraceEnabled();

      long omod = orig.openConnection().getLastModified();
      long smod = stored.openConnection().getLastModified();

      if (trace) {
         log.trace("Modfication times (orig, stored): " + omod + ", " + smod);
      }
      
      return omod > smod;
   }

   public void deploy(final URL url) throws DeploymentException
   {
      boolean debug = log.isDebugEnabled();

      try {
         URL storedURL = store.get(url);
         if (storedURL != null) {
            // it's in the cache, is it still valid ?

            if (isInvalid(url, storedURL)) {
               // the stored version is old, get the new version
               log.info("Cached deployment is invalid; refreshing store for URL: " + url);
               storedURL = store.put(url);
            }
            else {
               if (debug) {
                  log.debug("Using cached deployment URL: " + storedURL);
               }
            }
         }
         else {
            // not in the cache, put it there
            log.info("Deployment not in cache; adding URL to store: " + url);
            storedURL = store.put(url);
         }

         // invoke the chained deployer with the stored URL
         deployer.deploy(storedURL);
      }
      catch (Exception e) {
         throw new DeploymentException(e);
      }
   }

   public void undeploy(final URL url) throws DeploymentException
   {
      boolean debug = log.isDebugEnabled();

      try {
         URL storedURL = store.get(url);
         if (storedURL != null) {
            // invoke undeploy on target deployer using local cache url
            deployer.undeploy(storedURL);
         }
         else {
            if (debug) {
               log.debug("Not found in store; ignoring URL: " + url);
            }
         }
      }
      catch (Exception e) {
         throw new DeploymentException(e);
      }
   }

   public boolean isDeployed(final URL url)
   {
      try {
         URL storedURL = store.get(url);

         // if the stored url is not null then ask the target deployer
         // else it is not deployed
         return storedURL != null && deployer.isDeployed(url);
      }
      catch (Exception e) {
         return false;
      }
   }


   /////////////////////////////////////////////////////////////////////////
   //                     Service/ServiceMBeanSupport                     //
   /////////////////////////////////////////////////////////////////////////

   protected void createService() throws Exception
   {
      // create stale deployemnt timer/scanner/whatever
   }
   
   protected void startService() throws Exception 
   {
      if (deployer == null)
         throw new MissingAttributeException("Deployer");
      if (store == null)
         throw new MissingAttributeException("Store");

      // start stale deployemnt timer/scanner/whatever
   }
   
   protected void stopService() throws Exception 
   {
      // stop stale deployemnt timer/scanner/whatever
   }
   
   protected void destroyService() throws Exception 
   {
      deployer = null;
      store = null;
   }
}
