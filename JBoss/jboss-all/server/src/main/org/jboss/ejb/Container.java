/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb;


import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.URL;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;
import javax.ejb.EJBException;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanException;
import javax.management.ObjectName;
import javax.naming.*;
import javax.transaction.TransactionManager;
import org.jboss.deployment.DeploymentException;
import org.jboss.deployment.DeploymentInfo;
import org.jboss.ejb.BeanLockManager;
import org.jboss.ejb.plugins.local.BaseLocalProxyFactory;
import org.jboss.invocation.Invocation;
import org.jboss.invocation.InvocationType;
import org.jboss.invocation.MarshalledInvocation;
import org.jboss.invocation.InvocationStatistics;
import org.jboss.logging.Logger;
import org.jboss.metadata.ApplicationMetaData;
import org.jboss.metadata.BeanMetaData;
import org.jboss.metadata.EjbLocalRefMetaData;
import org.jboss.metadata.EjbRefMetaData;
import org.jboss.metadata.EnvEntryMetaData;
import org.jboss.metadata.ResourceEnvRefMetaData;
import org.jboss.metadata.ResourceRefMetaData;
import org.jboss.naming.ENCThreadLocalKey;
import org.jboss.naming.Util;
import org.jboss.security.AuthenticationManager;
import org.jboss.security.RealmMapping;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.util.NestedError;
import org.jboss.mx.util.ObjectNameFactory;
import org.jboss.mx.util.ObjectNameConverter;

/**
 * This is the base class for all EJB-containers in JBoss. A Container
 * functions as the central hub of all metadata and plugins. Through this
 * the container plugins can get hold of the other plugins and any metadata
 * they need.
 *
 * <p>The EJBDeployer creates instances of subclasses of this class
 *    and calls the appropriate initialization methods.
 *
 * <p>A Container does not perform any significant work, but instead delegates
 *    to the plugins to provide for all kinds of algorithmic functionality.
 *
 * @see EJBDeployer
 *
 * @author <a href="mailto:rickard.oberg@jboss.org">Rickard Öberg</a>
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:Scott.Stark@jboss.org">Scott Stark</a>.
 * @author <a href="bill@burkecentral.com">Bill Burke</a>
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version $Revision: 1.96.2.16 $
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean"
 */
public abstract class Container
   extends ServiceMBeanSupport
   implements ContainerMBean
{
   public final static String BASE_EJB_CONTAINER_NAME =
         "jboss.j2ee:service=EJB";

   public final static ObjectName EJB_CONTAINER_QUERY_NAME =
         ObjectNameFactory.create(BASE_EJB_CONTAINER_NAME + ",*");

   /** This is the application that this container is a part of */
   protected EjbModule ejbModule;

   /**
    * This is the local classloader of this container. Used for loading
    * resources that must come from the local jar file for the container.
    * NOT for loading classes!
    */
   protected ClassLoader localClassLoader;

   /**
    * This is the classloader of this container. All classes and resources that
    * the bean uses will be loaded from here. By doing this we make the bean
    * re-deployable
    */
   protected ClassLoader classLoader;

   /** The class loader for remote dynamic classloading */
   protected ClassLoader webClassLoader;

   /**
    * Externally supplied configuration data
    */
   private DeploymentInfo di;

   /**
    * This is the new metadata. it includes information from both ejb-jar and
    * jboss.xml the metadata for the application can be accessed trough
    * metaData.getApplicationMetaData()
    */
   protected BeanMetaData metaData;

   /** This is the EnterpriseBean class */
   protected Class beanClass;

   /** This is the Home interface class */
   protected Class homeInterface;

   /** This is the Remote interface class */
   protected Class remoteInterface;

   /** The local home interface class */
   protected Class localHomeInterface;

   /** The local inteface class */
   protected Class localInterface;

   /** This is the TransactionManager */
   protected TransactionManager tm;

   /** This is the SecurityManager */
   protected AuthenticationManager sm;

   /** This is the realm mapping */
   protected RealmMapping rm;

   /** The custom security proxy used by the SecurityInterceptor */
   protected Object securityProxy;

   /** This is the bean lock manager that is to be used */
   protected BeanLockManager lockManager;

   /** ??? */
   protected LocalProxyFactory localProxyFactory =
      new BaseLocalProxyFactory();

   /** This is a cache for method permissions */
   private HashMap methodPermissionsCache = new HashMap();

   /** Maps for MarshalledInvocation mapping */
   protected Map marshalledInvocationMapping = new HashMap();

   /** This Container's codebase, a sequence of URLs separated by spaces */
   //protected String codebase = "";

   /** ObjectName of Container */
   private ObjectName jmxName;

   protected HashMap proxyFactories = new HashMap();

   /**
    * The Proxy factory is set in the Invocation.  This TL is used
    * for methods that do not have access to the Invocation.
    */
   protected ThreadLocal proxyFactoryTL = new ThreadLocal();

   /** The number of create invocations that have been made */
   protected long createCount;
   /** The number of create invocations that have been made */
   protected long removeCount;
   /** Time statistics for the invoke(Invocation) methods */
   protected InvocationStatistics invokeStats = new InvocationStatistics();

   /**
    * boolean <code>started</code> indicates if this container is currently
    * started. if not, calls to non lifecycle methods will raise exceptions.
    */
   private boolean started = false;

   // Public --------------------------------------------------------

   public Class getLocalClass()
   {
      return localInterface;
   }

   public Class getLocalHomeClass()
   {
      return localHomeInterface;
   }

   /**
    * Sets a transaction manager for this container.
    *
    * @see javax.transaction.TransactionManager
    *
    * @param tm
    */
   public void setTransactionManager(final TransactionManager tm)
   {
      this.tm = tm;
   }

   /**
    * Returns this container's transaction manager.
    *
    * @return    A concrete instance of javax.transaction.TransactionManager
    */
   public TransactionManager getTransactionManager()
   {
      return tm;
   }

   public void setSecurityManager(AuthenticationManager sm)
   {
      this.sm = sm;
   }

   public AuthenticationManager getSecurityManager()
   {
      return sm;
   }

   public BeanLockManager getLockManager()
   {
      return lockManager;
   }

   public void setLockManager(final BeanLockManager lockManager)
   {
      this.lockManager = lockManager;
      lockManager.setContainer(this);
   }

   public void addProxyFactory(String invokerBinding, EJBProxyFactory factory)
   {
      proxyFactories.put(invokerBinding, factory);
   }

   public void setRealmMapping(final RealmMapping rm)
   {
      this.rm = rm;
   }

   public RealmMapping getRealmMapping()
   {
      return rm;
   }

   public void setSecurityProxy(Object proxy)
   {
      this.securityProxy = proxy;
   }

   public Object getSecurityProxy()
   {
      return securityProxy;
   }

   public EJBProxyFactory getProxyFactory()
   {
      return (EJBProxyFactory)proxyFactoryTL.get();
   }

   public void setProxyFactory(Object factory)
   {
      proxyFactoryTL.set(factory);
   }

   public EJBProxyFactory lookupProxyFactory(String binding)
   {
      return (EJBProxyFactory)proxyFactories.get(binding);
   }

   /**
    * Gets the DeploymentInfo for this Container
    *
    * @return The DeploymentInfo for this Container
    */
   public final DeploymentInfo getDeploymentInfo()
   {
      return di;
   }

   /**
    * Sets the DeploymentInfo of this Container
    *
    * @param di The new DeploymentInfo to be used
    */
   public final void setDeploymentInfo( DeploymentInfo di )
   {
      this.di = di;
   }

   /**
    * Sets the application deployment unit for this container. All the bean
    * containers within the same application unit share the same instance.
    *
    * @param   app     application for this container
    */
   public void setEjbModule(EjbModule app)
   {
      if (app == null)
         throw new IllegalArgumentException("Null EjbModule");

      ejbModule = app;
   }

   /**
    * Gets the application deployment unit for this container. All the bean
    * containers within the same application unit share the same instance.
    * @jmx:managed-attribute
    */
   public EjbModule getEjbModule()
   {
      return ejbModule;
   }

   /**
    * Gets the number of create invocations that have been made
    * @jmx:managed-attribute
    */
   public long getCreateCount()
   {
      return createCount;
   }
   /**
    * Gets the number of remove invocations that have been made
    * @jmx:managed-attribute
    */
   public long getRemoveCount()
   {
      return removeCount;
   }

   /** Gets the invocation statistics collection
    * @jmx:managed-attribute
    */
   public InvocationStatistics getInvokeStats()
   {
      return invokeStats;
   }

   /**
    * Sets the local class loader for this container.
    * Used for loading resources from the local jar file for this container.
    * NOT for loading classes!
    *
    * @param   cl
    */
   public void setLocalClassLoader(ClassLoader cl)
   {
      this.localClassLoader = cl;
   }

   /**
    * Returns the local classloader for this container.
    *
    * @return   The local classloader for this container.
    */
   public ClassLoader getLocalClassLoader()
   {
      return localClassLoader;
   }

   /**
    * Sets the class loader for this container. All the classes and resources
    * used by the bean in this container will use this classloader.
    *
    * @param   cl
    */
   public void setClassLoader(ClassLoader cl)
   {
      this.classLoader = cl;
   }

   /**
    * Returns the classloader for this container.
    *
    * @return
    */
   public ClassLoader getClassLoader()
   {
      return classLoader;
   }

   /** Get the class loader for dynamic class loading via http.
    */
   public ClassLoader getWebClassLoader()
   {
      return webClassLoader;
   }
   /** Set the class loader for dynamic class loading via http.
    */
   public void setWebClassLoader(final ClassLoader webClassLoader)
   {
      this.webClassLoader = webClassLoader;
   }

   /**
    * Sets the meta data for this container. The meta data consists of the
    * properties found in the XML descriptors.
    *
    * @param metaData
    */
   public void setBeanMetaData(final BeanMetaData metaData)
   {
      this.metaData = metaData;
   }

   /**
    * Returns the metadata of this container.
    *
    * @jmx:managed-attribute
    * @return metaData;
    */
   public BeanMetaData getBeanMetaData()
   {
      return metaData;
   }

   /**
    * Returns the permissions for a method. (a set of roles)
    *
    * @return assemblyDescriptor;
    */
   public Set getMethodPermissions(Method m, InvocationType iface)
   {
      Set permissions;

      if (methodPermissionsCache.containsKey(m))
      {
         permissions = (Set) methodPermissionsCache.get( m );
      }
      else
      {
         String name = m.getName();
         Class[] sig = m.getParameterTypes();
         permissions = getBeanMetaData().getMethodPermissions(name, sig, iface);
         methodPermissionsCache.put(m, permissions);
      }

      return permissions;
   }

   /**
    * Returns the bean class instance of this container.
    *
    * @return    instance of the Enterprise bean class.
    */
   public Class getBeanClass()
   {
      return beanClass;
   }

   /**
    * Returns a new instance of the bean class or a subclass of the bean class.
    * This factory style method is speciffically used by a container to supply
    * an implementation of the abstract accessors in EJB2.0, but could be
    * usefull in other situations. This method should ALWAYS be used instead
    * of getBeanClass().newInstance();
    *
    * @return    the new instance
    *
    * @see java.lang.Class#newInstance
    */
   public Object createBeanClassInstance() throws Exception {
      return getBeanClass().newInstance();
   }

   /**
    * Sets the codebase of this container.
    *
    * @param   codebase a possibly empty, but non null String with
    *                   a sequence of URLs separated by spaces
    * /
   public void setCodebase(final String codebase)
   {
      if (codebase != null)
         this.codebase = codebase;
   }
   */
   /**
    * Gets the codebase of this container.
    *
    * @return    this container's codebase String, a sequence of URLs
    *            separated by spaces
    * /
   public String getCodebase()
   {
      return codebase;
   }
   */
   /** Build a JMX name using the pattern jboss.j2ee:service=EJB,jndiName=[jndiName]
      where the [jndiName] is either the bean remote home JNDI binding, or
      the local home JNDI binding if the bean has no remote interfaces.
   */
   public ObjectName getJmxName()
   {
      if (jmxName == null)
      {
         BeanMetaData beanMetaData = getBeanMetaData();
         if (beanMetaData == null)
         {
            throw new IllegalStateException("Container metaData is null");
         }

         String jndiName = beanMetaData.getContainerObjectNameJndiName();
         if (jndiName == null)
         {
            throw new IllegalStateException("Container jndiName is null");
         }
 
         // The name must be escaped since the jndiName may be arbitrary
         String name = BASE_EJB_CONTAINER_NAME + ",jndiName=" + jndiName;
         try
         {
            jmxName = ObjectNameConverter.convert(name);
         }
         catch(MalformedObjectNameException e)
         {
            throw new RuntimeException("Failed to create ObjectName, msg="+e.getMessage());
         }
      }
      return jmxName;
   }

   /**
    * The EJBDeployer calls this method.  The EJBDeployer has set
    * all the plugins and interceptors that this bean requires and now proceeds
    * to initialize the chain.  The method looks for the standard classes in
    * the URL, sets up the naming environment of the bean. The concrete
    * container classes should override this method to introduce
    * implementation specific initialization behaviour.
    *
    * @throws Exception    if loading the bean class failed
    *                      (ClassNotFoundException) or setting up "java:"
    *                      naming environment failed (DeploymentException)
    */
   protected void createService() throws Exception
   {
      // Acquire classes from CL
      beanClass = classLoader.loadClass(metaData.getEjbClass());

      if (metaData.getLocalHome() != null)
         localHomeInterface = classLoader.loadClass(metaData.getLocalHome());
      if (metaData.getLocal() != null)
         localInterface = classLoader.loadClass(metaData.getLocal());

      localProxyFactory.setContainer( this );
      localProxyFactory.create();
      if (localHomeInterface != null)
         ejbModule.addLocalHome(this, localProxyFactory.getEJBLocalHome() );
   }

   /**
    * A default implementation of starting the container service.
    * The container registers it's dynamic MBean interface in the JMX base.
    *
    * The concrete container classes should override this method to introduce
    * implementation specific start behaviour.
    *
    * @todo implement the service lifecycle methods in an xmbean interceptor so
    * non lifecycle managed ops are blocked when mbean is not started.
    *
    * @throws Exception    An exception that occured during start
    */
   protected void startService() throws Exception
   {
      // Setup "java:comp/env" namespace
      setupEnvironment();
      started = true;
      localProxyFactory.start();
   }

   /**
    * A default implementation of stopping the container service (no-op). The
    * concrete container classes should override this method to introduce
    * implementation specific stop behaviour.
    */
   protected void stopService() throws Exception
   {
      started = false;
      localProxyFactory.stop();
      teardownEnvironment();
   }

   /**
    * A default implementation of destroying the container service (no-op).
    * The concrete container classes should override this method to introduce
    * implementation specific destroy behaviour.
    */
   protected void destroyService() throws Exception
   {
      localProxyFactory.destroy();
      ejbModule.removeLocalHome( this );
      this.classLoader = null;
      this.webClassLoader = null;
      this.localClassLoader = null;
      this.ejbModule = null;

      // this.lockManager = null; Setting this to null causes AbstractCache
      // to fail on undeployment
      di = null;
      metaData = null;
      beanClass = null;
      homeInterface = null;
      remoteInterface = null;
      localHomeInterface = null;
      localInterface = null;
      tm = null;
      sm = null;
      rm = null;
      securityProxy = null;
      methodPermissionsCache.clear();
      proxyFactories.clear();
      invokeStats.resetStats();
      marshalledInvocationMapping.clear();
      proxyFactories.clear();
      proxyFactoryTL = null;
   }

   /**
    * This method is called when a method call comes
    * in on the Home object.  The Container forwards this call to the
    * interceptor chain for further processing.
    *
    * @param mi   the object holding all info about this invocation
    * @return     the result of the home invocation
    *
    * @throws Exception
    */
   public abstract Object internalInvokeHome(Invocation mi)
      throws Exception;

   /**
    * This method is called when a method call comes
    * in on an EJBObject.  The Container forwards this call to the interceptor
    * chain for further processing.
    *
    * @param id        the id of the object being invoked. May be null
    *                  if stateless
    * @param method    the method being invoked
    * @param args      the parameters
    * @return          the result of the invocation
    *
    * @throws Exception
    */
   public abstract Object internalInvoke(Invocation mi)
      throws Exception;

   abstract Interceptor createContainerInterceptor();

   public abstract void addInterceptor(Interceptor in);

   /**
    * @jmx:managed-operation
    *
    * @param mi
    * @return
    * @throws Exception
    */
   public Object invoke(Invocation mi)
      throws Exception
   {
      Thread currentThread = Thread.currentThread();
      ClassLoader callerClassLoader = currentThread.getContextClassLoader();
      long start = System.currentTimeMillis();
      Method m = null;

      Object type = null;

      try
      {
         currentThread.setContextClassLoader(this.classLoader);
         // Check against home, remote, localHome, local, getHome,
         // getRemote, getLocalHome, getLocal
         type = mi.getType();

         // stat gathering: concurrent calls
         this.invokeStats.callIn();

         if(type == InvocationType.REMOTE ||
               type == InvocationType.LOCAL)
         {
            if (mi instanceof MarshalledInvocation)
            {
               ((MarshalledInvocation) mi).setMethodMap(
                     marshalledInvocationMapping);

               if (log.isTraceEnabled())
               {
                  log.trace("METHOD REMOTE INVOKE "+
                        mi.getObjectName()+"||"+
                        mi.getMethod().getName()+"||");
               }
            }
            m = mi.getMethod();
            return internalInvoke(mi);
         }
         else if(type == InvocationType.HOME ||
               type == InvocationType.LOCALHOME)
         {
            if (mi instanceof MarshalledInvocation)
            {

               ((MarshalledInvocation) mi).setMethodMap(
                     marshalledInvocationMapping);

               if (log.isTraceEnabled())
               {
                  log.trace("METHOD HOME INVOKE " +
                        mi.getObjectName() + "||"+
                        mi.getMethod().getName() + "||"+
                        mi.getArguments().toString());
               }
            }
            m = mi.getMethod();
            return internalInvokeHome(mi);
         }
         else
         {
            throw new MBeanException(new IllegalArgumentException(
                     "Unknown invocation type: " + type));
         }
      }
      finally
      {
         if( m != null )
         {
            long end = System.currentTimeMillis();
            long elapsed = end - start;
            this.invokeStats.updateStats(m, elapsed);
         }

         // stat gathering: concurrent calls
         this.invokeStats.callOut();

         currentThread.setContextClassLoader(callerClassLoader);
      }
   }

   // Private -------------------------------------------------------

   /**
    * This method sets up the naming environment of the bean.
    * We create the java:comp/env namespace with properties, EJB-References,
    * and DataSource ressources.
    */
   private void setupEnvironment() throws Exception
   {
      boolean debug = log.isDebugEnabled();
      BeanMetaData beanMetaData = getBeanMetaData();

      if (debug)
      {
         log.debug("Begin java:comp/env for EJB: "+beanMetaData.getEjbName());
         log.debug("TCL: "+Thread.currentThread().getContextClassLoader());
      }

      // Since the BCL is already associated with this thread we can start
      // using the java: namespace directly
      Context ctx = (Context) new InitialContext().lookup("java:comp");
      Context envCtx = ctx.createSubcontext("env");

      // Bind environment properties
      {
         Iterator enum = beanMetaData.getEnvironmentEntries();
         while(enum.hasNext())
         {
            EnvEntryMetaData entry = (EnvEntryMetaData)enum.next();
            if (debug) {
               log.debug("Binding env-entry: "+entry.getName()+" of type: "+
                         entry.getType()+" to value:"+entry.getValue());
            }

            EnvEntryMetaData.bindEnvEntry(envCtx, entry);
         }
      }

      // Bind EJB references
      {
         Iterator enum = beanMetaData.getEjbReferences();
         while(enum.hasNext())
         {
            EjbRefMetaData ref = (EjbRefMetaData)enum.next();
            if (debug)
               log.debug("Binding an EJBReference "+ref.getName());

            if (ref.getLink() != null)
            {
               // Internal link
               String linkName = ref.getLink();
               String jndiName = EjbUtil.findEjbLink(server, di, linkName);
               if (debug)
               {
                  log.debug("Binding "+ref.getName()+
                        " to ejb-link: "+linkName+" -> "+jndiName);
               }
               if( jndiName == null )
               {
                  String msg = "Failed to resolve ejb-link: "+linkName
                     +" make by ejb-name: "+ref.getName();
                  throw new DeploymentException(msg);
               }

               Util.bind(envCtx,
                     ref.getName(),
                     new LinkRef(jndiName));

            }
            else
            {
               // Get the invoker specific ejb-ref mappings
               Iterator it = beanMetaData.getInvokerBindings();
               Reference reference = null;
               while (it.hasNext())
               {
                  String invokerBinding = (String)it.next();
                  // Check for an invoker level jndi-name
                  String name = ref.getInvokerBinding(invokerBinding);
                  // Check for an global jndi-name
                  if (name == null)
                     name = ref.getJndiName();
                  if (name == null)
                  {
                     throw new DeploymentException
                        ("ejb-ref "+ref.getName()+
                         ", expected either ejb-link in ejb-jar.xml or " +
                         "jndi-name in jboss.xml");
                  }

                  StringRefAddr addr = new StringRefAddr(invokerBinding, name);
                  log.debug("adding " + invokerBinding + ":" + name +
                        " to Reference");

                  if (reference == null)
                  {
                     reference = new Reference("javax.naming.LinkRef",
                           ENCThreadLocalKey.class.getName(),
                           null);
                  }
                  reference.add(addr);
               }

               // If there were invoker bindings create bind the reference 
               if (reference != null)
               {
                  if (ref.getJndiName() != null)
                  {
                     // Add default for the bean level ejb-ref/jndi-name
                     StringRefAddr addr =
                           new StringRefAddr("default", ref.getJndiName());
                     reference.add(addr);
                  }
                  if ( reference.size() == 1 && reference.get("default") == null )
                  {
                     /* There is only one invoker binding and its not default so
                     create a default binding to allow the link to have a value
                     when accessed without an invoker active.
                     */
                     StringRefAddr addr = (StringRefAddr) reference.get(0);
                     String target = (String) addr.getContent();
                     StringRefAddr addr1 = new StringRefAddr("default", target);
                     reference.add(addr1);
                  }
                  Util.bind(envCtx, ref.getName(), reference);
               }
               else
               {
                  // Bind the bean level ejb-ref/jndi-name
                  if (ref.getJndiName() == null)
                  {
                     throw new DeploymentException("ejb-ref " + ref.getName()+
                         ", expected either ejb-link in ejb-jar.xml " +
                         "or jndi-name in jboss.xml");
                  }
                  Util.bind(envCtx,
                        ref.getName(),
                        new LinkRef(ref.getJndiName()));
               }
            }
         }
      }

      // Bind Local EJB references
      {
         Iterator enum = beanMetaData.getEjbLocalReferences();
         // unique key name
         String localJndiName = beanMetaData.getLocalJndiName();
         while(enum.hasNext())
         {
            EjbLocalRefMetaData ref = (EjbLocalRefMetaData)enum.next();
            String refName = ref.getName();
            log.debug("Binding an EJBLocalReference "+ref.getName());

            if (ref.getLink() != null)
            {
               // Internal link
               log.debug("Binding "+refName+" to bean source: "+ref.getLink());

               String jndiName = EjbUtil.findLocalEjbLink(server, di,
                  ref.getLink());

               Util.bind(envCtx,
                     ref.getName(),
                     new LinkRef(jndiName));
            }
            else
            { 
                // Bind the bean level ejb-local-ref/local-jndi-name
                if (ref.getJndiName() == null)
                {
                    throw new DeploymentException("ejb-local-ref " + ref.getName()+
                                ", expected either ejb-link in ejb-jar.xml " +
                                "or local-jndi-name in jboss.xml");
                }
                Util.bind(envCtx,
                          ref.getName(),
                          new LinkRef(ref.getJndiName()));
            }
         }
      }

      // Bind resource references
      {
         Iterator enum = beanMetaData.getResourceReferences();

         // let's play guess the cast game ;)  New metadata should fix this.
         ApplicationMetaData application =
               beanMetaData.getApplicationMetaData();

         while(enum.hasNext())
         {
            ResourceRefMetaData ref = (ResourceRefMetaData)enum.next();

            String resourceName = ref.getResourceName();
            String finalName = application.getResourceByName(resourceName);
            // If there was no resource-manager specified then an immeadiate
            // jndi-name or res-url name should have been given
            if (finalName == null)
               finalName = ref.getJndiName();

            if (finalName == null)
            {
               // the application assembler did not provide a resource manager
               // if the type is javax.sql.Datasoure use the default one

               if (ref.getType().equals("javax.sql.DataSource"))
               {
                  // Go through JNDI and look for DataSource - use the first one
                  Context dsCtx = new InitialContext();
                  try
                  {
                     // Check if it is available in JNDI
                     dsCtx.lookup("java:/DefaultDS");
                     finalName = "java:/DefaultDS";
                  }
                  catch (Exception e)
                  {
                     if (debug)
                        log.debug("failed to lookup DefaultDS; ignoring", e);
                  }
                  finally
                  {
                     dsCtx.close();
                  }
               }

               // Default failed? Warn user and move on
               // POTENTIALLY DANGEROUS: should this be a critical error?
               if (finalName == null)
               {
                  log.warn("No resource manager found for " +
                        ref.getResourceName());
                  continue;
               }
            }

            if (ref.getType().equals("java.net.URL"))
            {
               // URL bindings
               if (debug)
                  log.debug("Binding URL: " + ref.getRefName() +
                        " to JDNI ENC as: " + finalName);
               Util.bind(envCtx, ref.getRefName(), new URL(finalName));
            }
            else
            {
               // Resource Manager bindings, should validate the type...
               if (debug) {
                  log.debug("Binding resource manager: "+ref.getRefName()+
                            " to JDNI ENC as: " +finalName);
               }

               Util.bind(envCtx, ref.getRefName(), new LinkRef(finalName));
            }
         }
      }

      // Bind resource env references
      {
         Iterator enum = beanMetaData.getResourceEnvReferences();
         while( enum.hasNext() )
         {
            ResourceEnvRefMetaData resRef =
                  (ResourceEnvRefMetaData) enum.next();
            String encName = resRef.getRefName();
            String jndiName = resRef.getJndiName();
            // Should validate the type...
            if (debug)
               log.debug("Binding env resource: " + encName +
                     " to JDNI ENC as: " +jndiName);
            Util.bind(envCtx, encName, new LinkRef(jndiName));
         }
      }

      // Create a java:comp/env/security/security-domain link to the container
      // or application security-domain if one exists so that access to the
      // security manager can be made without knowing the global jndi name.

      String securityDomain =
            metaData.getContainerConfiguration().getSecurityDomain();
      if( securityDomain == null )
         securityDomain = metaData.getApplicationMetaData().getSecurityDomain();
      if( securityDomain != null )
      {
         if (debug) {
            log.debug("Binding securityDomain: "+securityDomain+
                      " to JDNI ENC as: security/security-domain");
         }

         Util.bind(
               envCtx,
               "security/security-domain",
               new LinkRef(securityDomain));
         Util.bind(
               envCtx,
               "security/subject",
               new LinkRef(securityDomain+"/subject"));
      }

      if (debug)
         log.debug("End java:comp/env for EJB: "+beanMetaData.getEjbName());
   }

   /**
    *The <code>teardownEnvironment</code> method unbinds everything from
    * the comp/env context.  It would be better do destroy the env context
    * but destroyContext is not currently implemented..
    *
    * @exception Exception if an error occurs
    */
   private void teardownEnvironment() throws Exception
   {
      Context ctx = (Context)new InitialContext().lookup("java:comp");
      ctx.unbind("env");
      log.debug("Removed bindings from java:comp/env for EJB: "+getBeanMetaData().getEjbName());
   }


   /**
    * The base class for container interceptors.
    *
    * <p>
    * All container interceptors perform the same basic functionality
    * and only differ slightly.
    */
   protected abstract class AbstractContainerInterceptor
      implements Interceptor
   {
      protected final Logger log = Logger.getLogger(this.getClass());

      public void setContainer(Container con) {}

      public void setNext(Interceptor interceptor) {}

      public Interceptor getNext() { return null; }

      public void create() {}

      public void start() {}

      public void stop() {}

      public void destroy() {}

      protected void rethrow(Exception e)
         throws Exception
      {
         if (e instanceof IllegalAccessException) {
            // Throw this as a bean exception...(?)
            throw new EJBException(e);
         }
         else if(e instanceof InvocationTargetException) {
            Throwable t = ((InvocationTargetException)e).getTargetException();

            if (t instanceof EJBException) {
               throw (EJBException)t;
            }
            else if (t instanceof Exception) {
               throw (Exception)t;
            }
            else if (t instanceof Error) {
               throw (Error)t;
            }
            else {
               throw new NestedError("Unexpected Throwable", t);
            }
         }

         throw e;
      }

      // Monitorable implementation ------------------------------------

      public void sample(Object s)
      {
         // Just here to because Monitorable request it but will be removed soon
      }

      public Map retrieveStatistic()
      {
         return null;
      }

      public void resetStatistic()
      {
      }

   }
}
/*
vim:ts=3:sw=3:et
*/
