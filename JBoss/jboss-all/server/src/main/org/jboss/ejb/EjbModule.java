/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb;

import java.lang.reflect.Constructor;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.LinkedList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.ListIterator;

import javax.ejb.EJBLocalHome;
import javax.management.ObjectName;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.transaction.TransactionManager;

import org.jboss.deployment.DeploymentException;
import org.jboss.deployment.DeploymentInfo;
import org.jboss.logging.Logger;
import org.jboss.metadata.ApplicationMetaData;
import org.jboss.metadata.BeanMetaData;
import org.jboss.metadata.ConfigurationMetaData;
import org.jboss.metadata.EntityMetaData;
import org.jboss.metadata.InvokerProxyBindingMetaData;
import org.jboss.metadata.MetaData;
import org.jboss.metadata.SessionMetaData;
import org.jboss.metadata.XmlLoadable;
import org.jboss.mx.loading.UnifiedClassLoader;
import org.jboss.ejb.plugins.SecurityProxyInterceptor;
import org.jboss.ejb.plugins.StatefulSessionInstancePool;
import org.jboss.security.AuthenticationManager;
import org.jboss.security.RealmMapping;
import org.jboss.system.Registry;
import org.jboss.system.ServiceControllerMBean;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.mx.util.MBeanProxyExt;
import org.jboss.mx.util.ObjectNameFactory;
import org.jboss.util.loading.DelegatingClassLoader;
import org.jboss.web.WebClassLoader;
import org.jboss.web.WebServiceMBean;

import org.w3c.dom.Element;

/**
 * An EjbModule represents a collection of beans that are deployed as a
 * unit.
 *
 * <p>The beans may use the EjbModule to access other beans within the same
 *    deployment unit.
 *
 * @see Container
 * @see EJBDeployer
 *
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @author <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian.Brock</a>
 * @author <a href="mailto:Scott.Stark@jboss.org">Scott Stark</a>
 * @version $Revision: 1.34.2.18 $
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean"
 */
public class EjbModule
   extends ServiceMBeanSupport
   implements EjbModuleMBean
{
   public static final String BASE_EJB_MODULE_NAME = "jboss.j2ee:service=EjbModule";

   public static final ObjectName EJB_MODULE_QUERY_NAME = ObjectNameFactory.create(BASE_EJB_MODULE_NAME + ",*");

   public static String DEFAULT_STATELESS_CONFIGURATION = "Default Stateless SessionBean";
   public static String DEFAULT_STATEFUL_CONFIGURATION = "Default Stateful SessionBean";
   public static String DEFAULT_ENTITY_BMP_CONFIGURATION = "Default BMP EntityBean";
   public static String DEFAULT_ENTITY_CMP_CONFIGURATION = "Default CMP EntityBean";
   public static String DEFAULT_MESSAGEDRIVEN_CONFIGURATION = "Default MesageDriven Bean";

   // Constants uses with container interceptor configurations
   public static final int BMT = 1;
   public static final int CMT = 2;
   public static final int ANY = 3;

   static final String BMT_VALUE = "Bean";
   static final String CMT_VALUE = "Container";
   static final String ANY_VALUE = "Both";

   /** Class logger. */
   private static final Logger log = Logger.getLogger(EjbModule.class);

   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   /** HashMap<ejbName, Container> the containers for this deployment unit. */
   HashMap containers = new HashMap();
   /** The containers in their ApplicationMetaData ordering */
   LinkedList containerOrdering = new LinkedList();
   /** HashMap<ejbName, EJBLocalHome> of local homes */
   HashMap localHomes = new HashMap();

   /** Class loader of this deployment unit. */
   ClassLoader classLoader = null;

   /** Name of this deployment unit, url it was deployed from */
   final String name;

   private DeploymentInfo deploymentInfo;

   private ServiceControllerMBean serviceController;

   private final Map moduleData =
      Collections.synchronizedMap(new HashMap());

   private ObjectName webServiceName;

   private TransactionManager tm;

   public EjbModule(final DeploymentInfo di, TransactionManager tm,
      ObjectName webServiceName)
   {
      this.deploymentInfo = di;
      String name = deploymentInfo.url.toString();
      if (name.endsWith("/"))
      {
         name = name.substring(0, name.length() - 1);
      }
      this.name = name;
      this.tm = tm;
      this.webServiceName = webServiceName;
   }

   public Map getModuleDataMap()
   {
      return moduleData;
   }

   public Object getModuleData(Object key)
   {
      return moduleData.get(key);
   }

   public void putModuleData(Object key, Object value)
   {
      moduleData.put(key, value);
   }

   public void removeModuleData(Object key)
   {
      moduleData.remove(key);
   }

   /**
    * Add a container to this deployment unit.
    *
    * @param   con
    */
   private void addContainer(Container con)
      throws DeploymentException
   {
      String ejbName = con.getBeanMetaData().getEjbName();
      if (containers.containsKey(ejbName))
         throw new DeploymentException("Duplicate ejb-name. Container for "
            + ejbName + " already exists.");
      containers.put(ejbName, con);
      containerOrdering.add(con);
      con.setEjbModule(this);
   }

   /**
    * Remove a container from this deployment unit.
    *
    * @param   con
    */
   public void removeContainer(Container con)
   {
      containers.remove(con.getBeanMetaData().getEjbName());
      containerOrdering.remove(con);
   }

   public void addLocalHome(Container con, EJBLocalHome localHome)
   {
      localHomes.put(con.getBeanMetaData().getEjbName(), localHome);
   }

   public void removeLocalHome(Container con)
   {
      localHomes.remove(con.getBeanMetaData().getEjbName());
   }

   public EJBLocalHome getLocalHome(Container con)
   {
      return (EJBLocalHome) localHomes.get(con.getBeanMetaData().getEjbName());
   }

   /**
    * Get a container from this deployment unit that corresponds to a given name
    *
    * @param   name  ejb-name name defined in ejb-jar.xml
    *
    * @return  container for the named bean, or null if the container was
    *          not found
    */
   public Container getContainer(String name)
   {
      return (Container) containers.get(name);
   }

   /**
    * Get all containers in this deployment unit.
    *
    * @return  a collection of containers for each enterprise bean in this
    *          deployment unit.
    * @jmx:managed-attribute
    */
   public Collection getContainers()
   {
      return containerOrdering;
   }

   /**
    * Get the class loader of this deployment unit.
    *
    * @return
    */
   public ClassLoader getClassLoader()
   {
      return classLoader;
   }

   /**
    * Set the class loader of this deployment unit
    *
    * @param   name
    */
   public void setClassLoader(ClassLoader cl)
   {
      this.classLoader = cl;
   }

   /**
    * Get the URL from which this deployment unit was deployed
    *
    * @return    The URL from which this Application was deployed.
    */
   public URL getURL()
   {
      return deploymentInfo.url;
   }

   // Service implementation ----------------------------------------

   protected void createService() throws Exception
   {
      serviceController = (ServiceControllerMBean)
         MBeanProxyExt.create(ServiceControllerMBean.class,
                              ServiceControllerMBean.OBJECT_NAME,
                              server);

      log.debug("createService, begin");

      //Set up the beans in this module.
      try
      {
         ApplicationMetaData appMetaData = (ApplicationMetaData) deploymentInfo.metaData;
         Iterator beans = appMetaData.getEnterpriseBeans();
         while (beans.hasNext())
         {
            BeanMetaData bean = (BeanMetaData) beans.next();
            log.info("Deploying " + bean.getEjbName());
            Container con = createContainer(bean, deploymentInfo);
            con.setDeploymentInfo(deploymentInfo);
            addContainer(con);
         }

         //only one iteration should be necessary, but we won't sweat it.
         //2 iterations are needed by cmp...jdbc/bridge/JDBCCMRFieldBridge which
         //assumes persistence managers are all set up for every
         //bean in the relationship!
         ListIterator iter = containerOrdering.listIterator();
         while( iter.hasNext() )
         {
            Container con = (Container) iter.next();
            ObjectName jmxName = con.getJmxName();
            /* Add the container mbean to the deployment mbeans so the state
               of the deployment can be tracked.
            */
            server.registerMBean(con, jmxName);
            deploymentInfo.mbeans.add(jmxName);
            BeanMetaData metaData = con.getBeanMetaData();
            Collection depends = metaData.getDepends();
            serviceController.create(jmxName, depends);
            // We keep the hashCode around for fast creation of proxies
            int jmxHash = jmxName.hashCode();
            Registry.bind(new Integer(jmxHash), jmxName);
            log.debug("Bound jmxName=" + jmxName + ", hash=" + jmxHash + "into Registry");
         }
      }
      catch (Exception e)
      {
         destroyService();
         throw e;
      } // end of try-catch

   }

   /**
    * The mbean Service interface <code>start</code> method calls
    * the start method on each contatiner, then the init method on each
    * container.  Conversion to a different registration system with one-phase
    * startup is conceivable.
    *
    * @exception Exception if an error occurs
    */
   protected void startService() throws Exception
   {
      ListIterator iter = containerOrdering.listIterator();
      while( iter.hasNext() )
      {
         Container con = (Container) iter.next();
         log.debug("startService, starting container: " + con.getBeanMetaData().getEjbName());
         serviceController.start(con.getJmxName());
      }
   }

   /**
    * Stops all the containers of this application.
    */
   protected void stopService() throws Exception
   {
      ListIterator iter = containerOrdering.listIterator(containerOrdering.size());
      while( iter.hasPrevious() )
      {
         Container con = (Container) iter.previous();
         try
         {
            // The container may already be destroyed so validate metaData
            BeanMetaData metaData = con.getBeanMetaData();
            String ejbName = metaData != null ? metaData.getEjbName() : "Unknown";
            log.debug("stopService, stopping container: " + ejbName);
            serviceController.stop(con.getJmxName());
         }
         catch (Exception e)
         {
            log.error("unexpected exception stopping Container: " + con.getJmxName(), e);
         } // end of try-catch
      }
   }

   protected void destroyService() throws Exception
   {
      WebServiceMBean webServer = null;
      if (webServiceName != null)
      {
         webServer =
            (WebServiceMBean) MBeanProxyExt.create(WebServiceMBean.class,
                                                   webServiceName);
      }
      ListIterator iter = containerOrdering.listIterator(containerOrdering.size());
      while( iter.hasPrevious() )
      {
         Container con = (Container) iter.previous();
         ObjectName jmxName = con.getJmxName();
         int conState = con.getState();
         boolean destroyContainer = conState == CREATED || conState == STOPPED
            || conState == FAILED; 
         log.debug("Looking to destroy container: " + jmxName
            + ", state: "+con.getStateString()+", destroy: "+destroyContainer);
         // Only destroy containers that have been created or started
         if( destroyContainer )
         {
            int jmxHash = jmxName.hashCode();
            Registry.unbind(new Integer(jmxHash));
            // Unregister the web classloader
            //Removing the wcl should probably be done in stop of the container,
            // but I don't want to look for errors today.
            //Certainly the attempt needs to be before con.destroy
            //where the reference is discarded.
            if (webServer != null)
            {
               ClassLoader wcl = con.getWebClassLoader();
               if (wcl != null)
               {
                  try
                  {
                     webServer.removeClassLoader(wcl);
                  }
                  catch (Throwable e)
                  {
                     log.warn("Failed to unregister webClassLoader", e);
                  }
               }
            } // end of if ()
            try
            {
               serviceController.destroy(jmxName);
            }
            catch (Throwable e)
            {
               log.error("unexpected exception destroying Container: " + jmxName, e);
            } // end of try-catch
        }
        // If the container was registered with the mbeanserver, remove it
        if (destroyContainer || conState == REGISTERED)
        {
            try
            {
               serviceController.remove(jmxName);
            }
            catch (Throwable e)
            {
               log.error("unexpected exception removing Container: " + jmxName, e);
            } // end of try-catch
         }
      }

      this.containers.clear();
      this.localHomes.clear();
      this.containerOrdering.clear();
      this.moduleData.clear();
      this.classLoader = null;
      this.deploymentInfo = null;
      this.serviceController = null;
      this.webServiceName = null;
      this.tm = null;
   }

   // ******************
   // Container Creation
   // ******************

   private Container createContainer(BeanMetaData bean, DeploymentInfo sdi)
      throws Exception
   {
      ClassLoader cl = sdi.ucl;
      ClassLoader localCl = sdi.localCl;

      Container container = null;
      // Added message driven deployment
      if (bean.isMessageDriven())
      {
         container = createMessageDrivenContainer(bean, cl, localCl);
      }
      else if (bean.isSession())   // Is session?
      {
         if (((SessionMetaData) bean).isStateless())   // Is stateless?
         {
            container = createStatelessSessionContainer(bean, cl, localCl);
         }
         else   // Stateful
         {
            container = createStatefulSessionContainer(bean, cl, localCl);
         }
      }
      else   // Entity
      {
         container = createEntityContainer(bean, cl, localCl);
      }
      return container;
   }

   private MessageDrivenContainer createMessageDrivenContainer(BeanMetaData bean,
                                                               ClassLoader cl,
                                                               ClassLoader localCl)
      throws Exception
   {
      // get the container configuration for this bean
      // a default configuration is now always provided
      ConfigurationMetaData conf = bean.getContainerConfiguration();
      // Stolen from Stateless deploy
      // Create container
      MessageDrivenContainer container = new MessageDrivenContainer();
      int transType = bean.isContainerManagedTx() ? CMT : BMT;

      initializeContainer(container, conf, bean, transType, cl, localCl);
      createProxyFactories(bean, container, cl);
      container.setInstancePool(createInstancePool(conf, cl));

      return container;
   }

   private StatelessSessionContainer createStatelessSessionContainer(BeanMetaData bean,
                                                                     ClassLoader cl,
                                                                     ClassLoader localCl)
      throws Exception
   {
      // get the container configuration for this bean
      // a default configuration is now always provided
      ConfigurationMetaData conf = bean.getContainerConfiguration();
      // Create container
      StatelessSessionContainer container = new StatelessSessionContainer();
      int transType = bean.isContainerManagedTx() ? CMT : BMT;
      initializeContainer(container, conf, bean, transType, cl, localCl);
      if (bean.getHome() != null)
      {
         createProxyFactories(bean, container, cl);
      }
      container.setInstancePool(createInstancePool(conf, cl));

      return container;
   }

   private StatefulSessionContainer createStatefulSessionContainer(BeanMetaData bean,
                                                                   ClassLoader cl,
                                                                   ClassLoader localCl)
      throws Exception
   {
      // get the container configuration for this bean
      // a default configuration is now always provided
      ConfigurationMetaData conf = bean.getContainerConfiguration();
      // Create container
      StatefulSessionContainer container = new StatefulSessionContainer();
      int transType = bean.isContainerManagedTx() ? CMT : BMT;
      initializeContainer(container, conf, bean, transType, cl, localCl);
      if (bean.getHome() != null)
      {
         createProxyFactories(bean, container, cl);
      }
      container.setInstanceCache(createInstanceCache(conf, cl));
      // No real instance pool, use the shadow class
      StatefulSessionInstancePool ip = new StatefulSessionInstancePool();
      ip.importXml(conf.getContainerPoolConf());
      container.setInstancePool(ip);
      // Set persistence manager
      container.setPersistenceManager((StatefulSessionPersistenceManager) cl.loadClass(conf.getPersistenceManager()).newInstance());
      //Set the bean Lock Manager
      container.setLockManager(createBeanLockManager(container, false, conf.getLockClass(), cl));

      return container;
   }

   private EntityContainer createEntityContainer(BeanMetaData bean,
                                                 ClassLoader cl,
                                                 ClassLoader localCl)
      throws Exception
   {
      // get the container configuration for this bean
      // a default configuration is now always provided
      ConfigurationMetaData conf = bean.getContainerConfiguration();
      // Create container
      EntityContainer container = new EntityContainer();
      int transType = CMT;
      initializeContainer(container, conf, bean, transType, cl, localCl);
      if (bean.getHome() != null)
      {
         createProxyFactories(bean, container, cl);
      }
      container.setInstanceCache(createInstanceCache(conf, cl));
      container.setInstancePool(createInstancePool(conf, cl));
      //Set the bean Lock Manager
      boolean reentrant = ((EntityMetaData) bean).isReentrant();
      BeanLockManager lockMgr = createBeanLockManager(container, reentrant, conf.getLockClass(), cl);
      container.setLockManager(lockMgr);

      // Set persistence manager
      if (((EntityMetaData) bean).isBMP())
      {
         Class pmClass = cl.loadClass(conf.getPersistenceManager());
         EntityPersistenceManager pm = (EntityPersistenceManager) pmClass.newInstance();
         container.setPersistenceManager(pm);
      }
      else
      {
         // CMP takes a manager and a store
         org.jboss.ejb.plugins.CMPPersistenceManager persistenceManager =
            new org.jboss.ejb.plugins.CMPPersistenceManager();

         //Load the store from configuration
         Class pmClass = cl.loadClass(conf.getPersistenceManager());
         EntityPersistenceStore pm = (EntityPersistenceStore) pmClass.newInstance();
         persistenceManager.setPersistenceStore(pm);
         // Set the manager on the container
         container.setPersistenceManager(persistenceManager);
      }

      return container;
   }

   // **************
   // Helper Methods
   // **************

   /**
    * Perform the common steps to initializing a container.
    */
   private void initializeContainer(Container container,
                                    ConfigurationMetaData conf,
                                    BeanMetaData bean,
                                    int transType,
                                    ClassLoader cl,
                                    ClassLoader localCl)
      throws NamingException, DeploymentException
   {
      // Create local classloader for this container
      // For loading resources that must come from the local jar.  Not for loading classes!
      container.setLocalClassLoader(new URLClassLoader(new URL[0], localCl));
      // Set metadata (do it *before* creating the container's WebClassLoader)
      container.setEjbModule(this);
      container.setBeanMetaData(bean);

      // Create the container's WebClassLoader
      // and register it with the web service.
      String webClassLoaderName = getWebClassLoader(conf, bean);
      log.debug("Creating WebClassLoader of class " + webClassLoaderName);
      WebClassLoader wcl = null;
      try
      {
         Class clazz = cl.loadClass(webClassLoaderName);
         Constructor constructor = clazz.getConstructor(
            new Class[]{ObjectName.class, UnifiedClassLoader.class});
         wcl = (WebClassLoader) constructor.newInstance(
            new Object[]{container.getJmxName(), cl});
      }
      catch (Exception e)
      {
         throw new DeploymentException(
            "Failed to create WebClassLoader of class "
            + webClassLoaderName + ": ", e);
      }
      if (webServiceName != null)
      {

         WebServiceMBean webServer =
            (WebServiceMBean) MBeanProxyExt.create(WebServiceMBean.class,
                                                   webServiceName);
         URL[] codebase = {webServer.addClassLoader(wcl)};


         wcl.setWebURLs(codebase);
      } // end of if ()
      container.setWebClassLoader(wcl);
      // Create classloader for this container
      // Only used to unique the bean ENC and does not augment class loading
      container.setClassLoader(new DelegatingClassLoader(wcl));

      // Set transaction manager
      InitialContext iniCtx = new InitialContext();
      container.setTransactionManager(tm);

      // Set security domain manager
      String securityDomain = bean.getApplicationMetaData().getSecurityDomain();
      String confSecurityDomain = conf.getSecurityDomain();
      // Default the config security to the application security manager
      if (confSecurityDomain == null)
         confSecurityDomain = securityDomain;
      // Check for an empty confSecurityDomain which signifies to disable security
      if (confSecurityDomain != null && confSecurityDomain.length() == 0)
         confSecurityDomain = null;
      if (confSecurityDomain != null)
      {   // Either the application has a security domain or the container has security setup
         try
         {
            log.debug("Setting security domain from: " + confSecurityDomain);
            Object securityMgr = iniCtx.lookup(confSecurityDomain);
            AuthenticationManager ejbS = (AuthenticationManager) securityMgr;
            RealmMapping rM = (RealmMapping) securityMgr;
            container.setSecurityManager(ejbS);
            container.setRealmMapping(rM);
         }
         catch (NamingException e)
         {
            throw new DeploymentException("Could not find the security-domain, name=" + confSecurityDomain, e);
         }
         catch (Exception e)
         {
            throw new DeploymentException("Invalid security-domain specified, name=" + confSecurityDomain, e);
         }
      }

      // Load the security proxy instance if one was configured
      String securityProxyClassName = bean.getSecurityProxy();
      if (securityProxyClassName != null)
      {
         try
         {
            Class proxyClass = cl.loadClass(securityProxyClassName);
            Object proxy = proxyClass.newInstance();
            container.setSecurityProxy(proxy);
            log.debug("setSecurityProxy, " + proxy);
         }
         catch (Exception e)
         {
            throw new DeploymentException("Failed to create SecurityProxy of type: " +
                                          securityProxyClassName, e);
         }
      }

      // Install the container interceptors based on the configuration
      addInterceptors(container, transType, conf.getContainerInterceptorsConf());
   }

   /**
    * Return the name of the WebClassLoader class for this ejb.
    */
   private static String getWebClassLoader(ConfigurationMetaData conf,
                                           BeanMetaData bmd)
      throws DeploymentException
   {
      String webClassLoader = null;
      Iterator it = bmd.getInvokerBindings();
      int count = 0;
      while (it.hasNext())
      {
         String invoker = (String) it.next();
         ApplicationMetaData amd = bmd.getApplicationMetaData();
         InvokerProxyBindingMetaData imd =
            amd.getInvokerProxyBindingMetaDataByName(invoker);
         if (imd == null)
         {
            String msg = "Failed to find InvokerProxyBindingMetaData for: '"
               + invoker + "'. Check the invoker-proxy-binding-name to "
               + "invoker-proxy-binding/name mappings in jboss.xml";
            throw new DeploymentException(msg);
         }

         Element proxyFactoryConfig = imd.getProxyFactoryConfig();
         String webCL = MetaData.getOptionalChildContent(proxyFactoryConfig,
                                                         "web-class-loader");
         if (webCL != null)
         {
            log.debug("Invoker " + invoker
                      + " specified WebClassLoader class" + webCL);
            webClassLoader = webCL;
            count++;
         }
      }
      if (count > 1)
      {
         log.warn(count + " invokers have WebClassLoader specifications.");
         log.warn("Using the last specification seen ("
                  + webClassLoader + ").");
      }
      else if (count == 0)
      {
         webClassLoader = conf.getWebClassLoader();
      }
      return webClassLoader;
   }

   /**
    * Given a container-interceptors element of a container-configuration,
    * add the indicated interceptors to the container depending on the container
    * transcation type and metricsEnabled flag.
    *
    *
    * @todo marcf: frankly the transaction type stuff makes no sense to me, we have externalized
    * the container stack construction in jbossxml and I don't see why or why there would be a
    * type missmatch on the transaction
    *
    * @param container   the container instance to setup.
    * @param transType   one of the BMT, CMT or ANY constants.
    * @param element     the container-interceptors element from the
    *                    container-configuration.
    */
   private void addInterceptors(Container container,
                                int transType,
                                Element element)
      throws DeploymentException
   {
      // Get the interceptor stack(either jboss.xml or standardjboss.xml)
      Iterator interceptorElements = MetaData.getChildrenByTagName(element, "interceptor");
      String transTypeString = stringTransactionValue(transType);
      ClassLoader loader = container.getClassLoader();
      /* First build the container interceptor stack from interceptorElements
         match transType and metricsEnabled values
      */
      ArrayList istack = new ArrayList();
      while (interceptorElements != null && interceptorElements.hasNext())
      {
         Element ielement = (Element) interceptorElements.next();
         /* Check that the interceptor is configured for the transaction mode of the bean
            by comparing its 'transaction' attribute to the string representation
            of transType
            FIXME: marcf, WHY???????
         */
         String transAttr = ielement.getAttribute("transaction");
         if (transAttr == null || transAttr.length() == 0)
            transAttr = ANY_VALUE;
         if (transAttr.equalsIgnoreCase(ANY_VALUE) || transAttr.equalsIgnoreCase(transTypeString))
         {   // The transaction type matches the container bean trans type, check the metricsEnabled
            String metricsAttr = ielement.getAttribute("metricsEnabled");
            boolean metricsInterceptor = metricsAttr.equalsIgnoreCase("true");
            try
            {
               boolean metricsEnabled = ((Boolean) server.getAttribute(EJBDeployerMBean.OBJECT_NAME,
                                                                       "MetricsEnabled")).booleanValue();
               if (metricsEnabled == false && metricsInterceptor == true)
               {
                  continue;
               }
            }
            catch (Exception e)
            {
               throw new DeploymentException("couldn't contace EJBDeployer!", e);
            } // end of try-catch


            String className = null;
            try
            {
               className = MetaData.getElementContent(ielement);
               Class clazz = loader.loadClass(className);
               Interceptor interceptor = (Interceptor) clazz.newInstance();
               istack.add(interceptor);
            }
            catch (Exception e)
            {
               log.warn("Could not load the " + className + " interceptor for this container", e);
            }
         }
      }

      if (istack.size() == 0)
         log.warn("There are no interceptors configured. Check the standardjboss.xml file");

      // Now add the interceptors to the container
      for (int i = 0; i < istack.size(); i++)
      {
         Interceptor interceptor = (Interceptor) istack.get(i);
         container.addInterceptor(interceptor);
      }

      /* If there is a security proxy associated with the container add its
         interceptor just before the container interceptor
      */
      if (container.getSecurityProxy() != null)
         container.addInterceptor(new SecurityProxyInterceptor());

      // Finally we add the last interceptor from the container
      container.addInterceptor(container.createContainerInterceptor());
   }

   private static String stringTransactionValue(int transType)
   {
      String transaction = ANY_VALUE;
      switch (transType)
      {
      case BMT:
         transaction = BMT_VALUE;
         break;
      case CMT:
         transaction = CMT_VALUE;
         break;
      }
      return transaction;
   }

   /**
    * Create all proxy factories for this ejb
    */
   private static void createProxyFactories(BeanMetaData conf, Container container,
                                            ClassLoader cl)
      throws Exception
   {
      Iterator it = conf.getInvokerBindings();
      while (it.hasNext())
      {
         String invoker = (String) it.next();
         String jndiBinding = conf.getInvokerBinding(invoker);
         log.debug("creating binding for " + jndiBinding + ":" + invoker);
         InvokerProxyBindingMetaData imd = conf.getApplicationMetaData().getInvokerProxyBindingMetaDataByName(invoker);
         EJBProxyFactory ci = null;

         // create a ProxyFactory instance
         try
         {
            ci = (EJBProxyFactory) cl.loadClass(imd.getProxyFactory()).newInstance();
            ci.setContainer(container);
            ci.setInvokerMetaData(imd);
            ci.setInvokerBinding(jndiBinding);
            if (ci instanceof XmlLoadable)
            {
               // the container invoker can load its configuration from the jboss.xml element
               ((XmlLoadable) ci).importXml(imd.getProxyFactoryConfig());
            }
            container.addProxyFactory(invoker, ci);
         }
         catch (Exception e)
         {
            throw new DeploymentException("Missing or invalid Container Invoker (in jboss.xml or standardjboss.xml): " + invoker, e);
         }
      }
   }


   private static BeanLockManager createBeanLockManager(Container container, boolean reentrant, String beanLock,
                                                        ClassLoader cl)
      throws Exception
   {
      // The bean lock manager
      BeanLockManager lockManager = new BeanLockManager(container);

      Class lockClass = null;
      try
      {
         lockClass = cl.loadClass(beanLock);
      }
      catch (Exception e)
      {
         throw new DeploymentException("Missing or invalid lock class (in jboss.xml or standardjboss.xml): " + beanLock + " - " + e);
      }

      lockManager.setLockCLass(lockClass);
      lockManager.setReentrant(reentrant);

      return lockManager;
   }

   private static InstancePool createInstancePool(ConfigurationMetaData conf,
                                                  ClassLoader cl)
      throws Exception
   {
      // Set instance pool
      InstancePool ip = null;

      try
      {
         ip = (InstancePool) cl.loadClass(conf.getInstancePool()).newInstance();
      }
      catch (Exception e)
      {
         throw new DeploymentException("Missing or invalid Instance Pool (in jboss.xml or standardjboss.xml)", e);
      }

      if (ip instanceof XmlLoadable)
         ((XmlLoadable) ip).importXml(conf.getContainerPoolConf());

      return ip;
   }

   private static InstanceCache createInstanceCache(ConfigurationMetaData conf,
                                                    ClassLoader cl)
      throws Exception
   {
      // Set instance cache
      InstanceCache ic = null;

      try
      {
         ic = (InstanceCache) cl.loadClass(conf.getInstanceCache()).newInstance();
      }
      catch (Exception e)
      {
         throw new DeploymentException("Missing or invalid Instance Cache (in jboss.xml or standardjboss.xml)", e);
      }

      if (ic instanceof XmlLoadable)
         ((XmlLoadable) ic).importXml(conf.getContainerCacheConf());

      return ic;
   }

}
/*
vim:ts=3:sw=3:et
*/
