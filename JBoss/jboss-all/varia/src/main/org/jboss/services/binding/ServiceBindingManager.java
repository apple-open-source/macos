/*
 *  JBoss, the OpenSource J2EE webOS
 *
 *  Distributable under LGPL license.
 *  See terms of license at gnu.org.
 */
package org.jboss.services.binding;

import java.net.URL;
import javax.management.MBeanRegistration;
import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.jboss.logging.Logger;
import org.jboss.system.server.ServerConfigLocator;

/** The services configuration binding manager mbean implementation.
 *
 * <p>The ServiceBindingManager enables the centralized management
 * of ports, by service. The port configuration store is abstracted out
 * using the ServicesStore and ServicesStoreFactory interfaces. Note that
 * this class does not implement the JBoss services lifecycle methods
 * and hook its behavior off of those because this service is used to
 * configure other services before any of the lifecycle methods are invoked.
 *
 * @version $Revision: 1.5.2.1 $
 * @author  <a href="mailto:bitpushr@rochester.rr.com">Mike Finn</a>.
 * @author Scott.Stark@jboss.org
 *
 * @jmx:mbean
 */
public class ServiceBindingManager
   implements MBeanRegistration, ServiceBindingManagerMBean
{
   private static Logger log = Logger.getLogger(ServiceBindingManager.class);

   private MBeanServer server;
   /** The name of the server this manager is associated with. This is a
    logical name used to lookup ServiceConfigs from the ServicesStore.
    */
   private String serverName;
   /** The name of the class implementation the ServicesStoreFatory. The
    default value is org.jboss.services.binding.XMLServicesStoreFactory
    */
   private String storeFactoryClassName = "org.jboss.services.binding.XMLServicesStoreFactory";
   /** The ServiceConfig store instance
    */
   private ServicesStore store;
   /** The URL of the configuration store
    */
   private URL storeURL;

   /**
    * @jmx:managed-attribute
    */
   public String getServerName()
   {
      return this.serverName;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setServerName(String serverName)
   {
      this.serverName = serverName;
   }

   /**
    * @jmx:managed-attribute
    */
   public String getStoreFactoryClassName()
   {
      return this.storeFactoryClassName;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setStoreFactoryClassName(String storeFactoryClassName)
   {
      this.storeFactoryClassName = storeFactoryClassName;
   }

   /**
    * @jmx:managed-attribute
    */
   public URL getStoreURL()
   {
      return storeURL;
   }

   /** Set the string representation of the store URL
    * @jmx:managed-attribute
    */
   public void setStoreURL(URL storeURL)
   {
      this.storeURL = storeURL;
   }

   public ObjectName preRegister(MBeanServer server, ObjectName name)
      throws Exception
   {
      this.server = server;
      return name;
   }
   public void postRegister(Boolean registrationDone)
   {
   }
   public void preDeregister()
      throws Exception
   {
      if( store != null )
         store.store(storeURL);
   }
   public void postDeregister()
   {
   }

   /**
    * Looks up the service config for the given service using the
    * server name bound to this mbean.
    *
    * @jmx:managed-operation
    *
    * @param  serviceName  the JMX ObjectName of the service
    * @return ServiceConfig instance if found, null otherwise
    */
   public ServiceConfig getServiceConfig(ObjectName serviceName)
      throws Exception
   {
      if( store == null )
         initStore();

      log.debug("getServiceConfig, server:"+serverName+";serviceName:" + serviceName);

      // Look up the service config
      ServiceConfig svcConfig = store.getService(serverName, serviceName);
      ServiceConfig configCopy = null;
      if( svcConfig != null )
         configCopy = (ServiceConfig) svcConfig.clone();
      return configCopy;
   }

   /**
    * Looks up the service config for the requested service using the
    * server name bound to this mbean and invokes the configuration delegate
    * to apply the bindings to the service. If no config if found then this
    * method is a noop.
    *
    * @jmx:managed-operation
    *
    * @param  serviceName  the JMX ObjectName of the service
    * @exception Exception, thrown on failure to apply an existing configuration
    */
   public void applyServiceConfig(ObjectName serviceName)
      throws Exception
   {
      if( store == null )
         initStore();

      log.debug("applyServiceConfig, server:"+serverName+";serviceName:" + serviceName);

      // Look up the service config
      ServiceConfig svcConfig = store.getService(serverName, serviceName);
      if( svcConfig != null )
      {
         ClassLoader loader = Thread.currentThread().getContextClassLoader();
         String delegateName = svcConfig.getServiceConfigDelegateClassName();
         if( delegateName != null )
         {
            Class delegateClass = loader.loadClass(delegateName);
            ServicesConfigDelegate delegate = (ServicesConfigDelegate) delegateClass.newInstance();
            delegate.applyConfig(svcConfig, server);
         }
      }
   }

   /** Create and load the services store
    */
   private void initStore() throws Exception
   {
      log.info("Initializing store");
      // If no store url identified, use the ServerConfigURL
      if( this.storeURL == null )
      {
         this.storeURL = ServerConfigLocator.locate().getServerConfigURL();
      }
      log.info("Using StoreURL: "+storeURL);

      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      Class factoryClass = loader.loadClass(storeFactoryClassName);
      ServicesStoreFactory storeFactory = (ServicesStoreFactory) factoryClass.newInstance();

      // Create and load the store
      store = storeFactory.newInstance();
      store.load(storeURL);
   }
}
