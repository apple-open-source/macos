/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.system;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.ListIterator;
import java.util.Map;
import java.util.Set;
import javax.management.JMException;
import javax.management.MBeanInfo;
import javax.management.MBeanOperationInfo;
import javax.management.MBeanRegistration;
import javax.management.MBeanServer;
import javax.management.Notification;
import javax.management.NotificationFilterSupport;
import javax.management.NotificationListener;
import javax.management.ObjectInstance;
import javax.management.ObjectName;
import javax.management.NotificationBroadcasterSupport;

import org.jboss.deployment.DeploymentException;
import org.jboss.deployment.DeploymentInfo;
import org.jboss.deployment.DeploymentState;
import org.jboss.logging.Logger;
import org.jboss.mx.server.ServerConstants;
import org.jboss.mx.util.JMXExceptionDecoder;
import org.jboss.mx.util.ObjectNameFactory;

import org.w3c.dom.Element;
import org.apache.log4j.NDC;

/**
 * This is the main Service Controller. A controller can deploy a service to a
 * jboss.system It installs by delegating, it configures by delegating
 *
 * @see org.jboss.system.Service
 * 
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @author <a href="mailto:scott.stark@jboss.org">Scott Stark</a>
 * @version $Revision: 1.18.2.10 $ <p>
 * @jmx:mbean name="jboss.system:service=ServiceController"
 */
public class ServiceController
   extends NotificationBroadcasterSupport
   implements ServiceControllerMBean, MBeanRegistration, NotificationListener
{
   public static final ObjectName DEFAULT_LOADER_REPOSITORY = ObjectNameFactory.create(ServerConstants.DEFAULT_LOADER_NAME);
   // Attributes ----------------------------------------------------
   
   /** Class logger. */
   private static final Logger log = Logger.getLogger(ServiceController.class);

   /** A callback to the JMX MBeanServer */
   MBeanServer server;
   
   /** Creator, helper class to instantiate MBeans **/
   protected ServiceCreator creator;
   
   /** Configurator, helper class to configure MBeans **/
   protected ServiceConfigurator configurator;
   
   /** Object Name to Service Proxy map **/
   protected Map nameToServiceMap = Collections.synchronizedMap(new HashMap());

   /** A linked list of services in the order they were created **/
   protected List installedServices = new LinkedList();

   /**
    * A map of classname to list of mbeans whose class is from that 
    * classloader.
    *
    */
   private final Map classNameToMBeansMap = new HashMap();

   /**
    * <code>waitingConfigs</code> is a map between ObjectNames and mbean configurations
    * that cannot be deployed because the mbean class is not available.
    *
    */
   private Map waitingConfigs = new HashMap();

   private long sequenceNo;
   //notification handling
   private final Object CLASSLOADER_ADDED_OBJECT = new Object();
   private final Object CLASS_REMOVED_OBJECT = new Object();

   
   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------
   
   /**
   * Lists the ServiceContexts of deployed mbeans
   *
   * @return the list of ServiceContexts for mbeans deployed through ServiceController.
   * @jmx:managed-operation
   */
   public List listDeployed()
   {
      return new ArrayList(installedServices);
   }

   /**
    * The <code>listIncompletelyDeployed</code> method returns the service contexts for 
    * the mbeans whose status is not RUNNING, and logs the string.
    *
    * @return a <code>List</code> value
   * @jmx:managed-operation
    */
   public List listIncompletelyDeployed()
   {
      List id = new ArrayList();
      for (Iterator i = installedServices.iterator(); i.hasNext();)
      {
         ServiceContext sc = (ServiceContext)i.next();
         if (sc.state != ServiceContext.RUNNING) 
         {
            id.add(sc);
         } // end of if ()
      } // end of for ()

      return id;
   }
   
   /**
   * lists ObjectNames of deployed mbeans deployed through serviceController.
   *
   * @return a list of ObjectNames of deployed mbeans.
   * @jmx:managed-operation
   */
   public List listDeployedNames()
   {
      List names = new ArrayList(installedServices.size());
      for (Iterator i = installedServices.iterator(); i.hasNext();)
      {
         ServiceContext ctx = (ServiceContext) i.next();
         names.add(ctx.objectName);
      } // end of for ()

      return names;
   }
   
   /**
   * Gets the Configuration attribute of the ServiceController object
   *
   * @param objectNames Description of Parameter
   * @return The Configuration value
   * @exception Exception Description of Exception
   * @jmx:managed-operation
   */
   public String listConfiguration(ObjectName[] objectNames) throws Exception
   {
      return configurator.getConfiguration(objectNames);
   }

   /**
    * The <code>listWaitingMBeans</code> method returns a list
    * of the mbeans currently not deployed because their classes 
    * are not available.
    *
    * @return a <code>List</code> value
    * @exception Exception if an error occurs
    * @jmx:managed-operation
    */
   public List listWaitingMBeans() throws Exception
   {
      return new ArrayList(waitingConfigs.keySet());
   }

   /** Go through the mbeans of the DeploymentInfo and validate that they
    * are in a state at least equal to that of the argument state
    *
    * @jmx:managed-operation
    */
   public void validateDeploymentState(DeploymentInfo di, DeploymentState state)
   {
      ArrayList mbeans = new ArrayList(di.mbeans);
      if( di.deployedObject != null )
         mbeans.add(di.deployedObject);
      boolean mbeansStateIsValid = true;
      for(int m = 0; m < mbeans.size(); m ++)
      {
         ObjectName serviceName = (ObjectName) mbeans.get(m);
         ServiceContext ctx = this.getServiceContext(serviceName);
         if( ctx!=null && state == DeploymentState.STARTED )
            mbeansStateIsValid &= ctx.state == ServiceContext.RUNNING;
      }
      if( mbeansStateIsValid == true )
         di.state = state;
   }

   /**
   * Deploy the beans
   *
   * Deploy means "instantiate and configure" so the MBean is created in the MBeanServer
   * You must call "create" and "start" separately on the MBean to affect the service lifecycle
   * deploy doesn't bother with service lifecycle only MBean instanciation/registration/configuration
   *  
   * @param mbeanElement Description of Parameter
   * @return Description of the Returned Value
   * @throws Exception ???
   * @jmx:managed-operation
   */
   public synchronized List install(Element config, ObjectName loaderName)
      throws DeploymentException
   {
      List mbeans = configurator.install(config, loaderName);
      for (Iterator i = mbeans.iterator(); i.hasNext();)
      {
         ObjectName mbean = (ObjectName)i.next();
         installedServices.add(createServiceContext(mbean));
      } // end of for ()
      return mbeans;
   }

   /**
   * #Description of the Method
   *
   * @param serviceName Description of Parameter
   * @exception Exception Description of Exception
   * @jmx:managed-operation
   */
   public synchronized void create(ObjectName serviceName) throws Exception
   {  
      create(serviceName, null);
   }

   /**
   * #Description of the Method
   *
   * @param serviceName Description of Parameter
   * @exception Exception Description of Exception
   * @jmx:managed-operation
   */
   public synchronized void create(ObjectName serviceName, Collection depends)
      throws Exception
   {  
      log.debug("Creating service " + serviceName);
      ServiceContext ctx = createServiceContext(serviceName);
      log.trace("Pushing NDC: " + ctx.objectName.toString());
      NDC.push(ctx.objectName.toString());

      try
      {   
         if (!installedServices.contains(ctx))
            installedServices.add(ctx);

         if (depends != null) 
         {
            log.debug("adding depends in ServiceController.create: " + depends);
            for (Iterator i = depends.iterator(); i.hasNext();)
            {
               registerDependency(serviceName, (ObjectName)i.next());
            } // end of for ()
         } // end of if ()
      
         // Get the fancy service proxy (for the lifecycle API)
         ctx.proxy = getServiceProxy(ctx.objectName, null);
      
         // If we are already created (can happen in dependencies) or failed just return
         if (ctx.state == ServiceContext.CREATED 
             || ctx.state == ServiceContext.RUNNING
             || ctx.state == ServiceContext.FAILED)
         {
            log.debug("Ignoring create request for service: "+ctx.objectName);
            return;
         }
         
         // JSR 77, and to avoid circular dependencies
         int oldState = ctx.state; 
         ctx.state= ServiceContext.CREATED;
      
         // Are all the mbeans I depend on created?   if not just return
         for (Iterator iterator = ctx.iDependOn.iterator(); iterator.hasNext();)
         {
            ServiceContext sc = (ServiceContext) iterator.next();
            int state = sc.state;
         
            // A dependent is not created or running
            if (!(state == ServiceContext.CREATED || state == ServiceContext.RUNNING))
            {
               log.debug("waiting in create of "+ serviceName +
                            " waiting on " + sc.objectName);
               ctx.state=oldState;
               return;
            }
         }

         // Call create on the service Proxy  
         try
         { 
            ctx.proxy.create();
            sequenceNo ++;
            Notification createMsg = new Notification(ServiceMBean.CREATE_EVENT,
               this, sequenceNo);
            createMsg.setUserData(serviceName);
            sendNotification(createMsg);
         }
         catch (Throwable e)
         {
            ctx.state = ServiceContext.FAILED;
            ctx.problem = e;
            log.warn("Problem creating service " + serviceName, e);
            return;
         }

         // Those that depend on me are waiting for my creation, recursively create them
         log.debug("Creating dependent components for: " + serviceName
            + " dependents are: " + ctx.dependsOnMe);
         ArrayList tmp = new ArrayList(ctx.dependsOnMe);
         for(int n = 0; n < tmp.size(); n ++)
         {
            // marcf fixme circular dependencies?
            ServiceContext ctx2 = (ServiceContext) tmp.get(n);
            create(ctx2.objectName);
         }
         tmp.clear();
      }
      finally
      {
         NDC.pop();
         NDC.remove();
      }
   }
   
   /**
   * #Description of the Method
   *
   * @param serviceName Description of Parameter
   * @exception Exception Description of Exception
   * @jmx:managed-operation
   */
   public synchronized void start(ObjectName serviceName) throws Exception
   {   
      log.debug("starting service " + serviceName);

      ServiceContext ctx = createServiceContext(serviceName);

      NDC.push(ctx.objectName.toString());
      
      try
      {   
         if (!installedServices.contains(ctx))
            installedServices.add(ctx);

         // If we are already started (can happen in dependencies) just return
         if (ctx.state == ServiceContext.RUNNING || ctx.state == ServiceContext.FAILED)
         {
            log.debug("Ignoring start request for service: "+ctx.objectName);
            return;
         }
         
         // JSR 77, and to avoid circular dependencies
         int oldState = ctx.state; 
         ctx.state= ServiceContext.RUNNING;
      
         // Are all the mbeans I depend on started?   if not just return
         for (Iterator iterator = ctx.iDependOn.iterator(); iterator.hasNext(); )
         {
            ServiceContext sctx = (ServiceContext) iterator.next();
         
            int state  = sctx.state;
         
            // A dependent is not running
            if (!(state == ServiceContext.RUNNING))
            {
               log.debug("waiting in start "+serviceName+" on "+sctx.objectName);
               ctx.state = oldState;
               return;
            }
         }
      
         // Call start on the service Proxy  
         try
         {
            ctx.proxy.start(); 
         }   
         catch (Throwable e)
         {
            ctx.state = ServiceContext.FAILED;
            ctx.problem = e;
            log.warn("Problem starting service " + serviceName, e);
            return;
         }
         // Those that depend on me are waiting for my start, recursively start them
         log.debug("Starting dependent components for: " + serviceName
            + " dependent components: " + ctx.dependsOnMe);
         ArrayList tmp = new ArrayList(ctx.dependsOnMe);
         for(int n = 0; n < tmp.size(); n ++)
         {
            // marcf fixme circular dependencies?
            ServiceContext ctx2 = (ServiceContext) tmp.get(n);
            start(ctx2.objectName);  
         }
         tmp.clear();
      }
      finally
      {
         NDC.pop();
         NDC.remove();
      }
   }

   /**
   * #Description of the Method
   *
   * @param serviceName Description of Parameter
   * @exception Exception Description of Exception
   * @jmx:managed-operation
   */
   public void stop(ObjectName serviceName) throws Exception
   {
      boolean debug = log.isDebugEnabled();
      
      ServiceContext ctx = (ServiceContext) nameToServiceMap.get(serviceName);
      if (debug)
      {
         log.debug("stopping service: " + serviceName);
      }
      
      if (ctx == null)
      {
         log.warn("Ignoring request to stop nonexistent service: " + serviceName);
         return;
      }
      
      // If we are already stopped (can happen in dependencies) just return
      if (ctx.state != ServiceContext.RUNNING) return;
      
      // JSR 77 and to avoid circular dependencies
      ctx.state = ServiceContext.STOPPED;
      
      if (debug)
      {
         log.debug("stopping dependent services for: " + serviceName
            + " dependent services are: " + ctx.dependsOnMe);
      }
      ArrayList tmp = new ArrayList(ctx.dependsOnMe);
      for(int n = 0; n < tmp.size(); n ++)
      {
         // stop all the mbeans that depend on me
         ServiceContext ctx2 = (ServiceContext) tmp.get(n);
         ObjectName other = ctx2.objectName;
         stop(other);
      }
      tmp.clear();

      // Call stop on the service Proxy
      if( ctx.proxy != null )
      {
         try
         {
            ctx.proxy.stop();
         }
         catch (Throwable e)
         {
            ctx.state = ServiceContext.FAILED;
            ctx.problem = e;
            log.warn("Problem stopping service " + serviceName, e);
         }
      }
   }
   
   /**
   * #Description of the Method
   *
   * @param serviceName Description of Parameter
   * @exception Exception Description of Exception
   * @jmx:managed-operation
   */
   public void destroy(ObjectName serviceName) throws Exception
   {
      boolean debug = log.isDebugEnabled();
      
      ServiceContext ctx = (ServiceContext) nameToServiceMap.get(serviceName);
      if (debug)
         log.debug("destroying service: " + serviceName);

      if (ctx == null)
      {
         log.warn("Ignoring request to destroy nonexistent service: " + serviceName);
         return;
      }
      
      // If we are already destroyed (can happen in dependencies) just return
      if (ctx.state == ServiceContext.DESTROYED ||
          ctx.state == ServiceContext.NOTYETINSTALLED)
         return;
      
      // JSR 77, and to avoid circular dependencies
      ctx.state = ServiceContext.DESTROYED;
      
      if (debug)
      {
         log.debug("destroying dependent services for: " + serviceName
            + " dependent services are: " + ctx.dependsOnMe);
      }
      ArrayList tmp = new ArrayList(ctx.dependsOnMe);
      for(int n = 0; n < tmp.size(); n ++)
      {        
         // destroy all the mbeans that depend on me
         ServiceContext ctx2 = (ServiceContext) tmp.get(n);
         ObjectName other = ctx2.objectName;
         destroy(other);
      }
      tmp.clear();

      // Call destroy on the service Proxy  
      if( ctx.proxy != null )
      {
         try
         {
            ctx.proxy.destroy();
            sequenceNo ++;
            Notification destroyMsg = new Notification(ServiceMBean.DESTROY_EVENT,
               this, sequenceNo);
            destroyMsg.setUserData(serviceName);
            sendNotification(destroyMsg);
         }
         catch (Throwable e)
         {
            ctx.state = ServiceContext.FAILED;
            ctx.problem = e;
            log.warn("Problem destroying service " + serviceName, e);
         }
      }
   }   
   
   /**
   * This MBean is going buh bye
   *
   * @param objectName Description of Parameter
   * @exception Exception Description of Exception
   * @jmx:managed-operation
   */
   public void remove(ObjectName objectName) throws Exception
   {
      boolean debug = log.isDebugEnabled();
      
      ServiceContext ctx = (ServiceContext) nameToServiceMap.get(objectName);
      if (debug)
         log.debug("removing service: " + objectName);

      //if mbean is waiting for its class, remove config element.
      waitingConfigs.remove(objectName);

      if (ctx == null)
      {
         log.warn("Ignoring request to remove nonexistent service: " + objectName);
         return;
      }
      
      // Notify those that think I depend on them
      Iterator iterator = ctx.iDependOn.iterator();
      while (iterator.hasNext())
      {
         ServiceContext iDependOnContext = (ServiceContext) iterator.next();
         iDependOnContext.dependsOnMe.remove(ctx);

         // Remove any context whose only reason for existence is that
         // we depend on it, i.e. it otherwise unknown to the system
         if (iDependOnContext.state == ServiceContext.NOTYETINSTALLED
             && iDependOnContext.dependsOnMe.size() == 0)
         {
            nameToServiceMap.remove(iDependOnContext.objectName);
            if (debug)
               log.debug("Removing context for nonexistent service it is " + 
                      "no longer recording dependencies: " + iDependOnContext);
         }
      }
      //We remove all traces of our dependency configuration, since we
      //don't know what will show up the next time we are deployed.
      ctx.iDependOn.clear();

      // Do we have a deployed MBean?
      if (server.isRegistered(objectName))
      {
         log.debug("removing " + objectName + " from server");

         // Remove the context, unless it is still recording dependencies
         if (ctx.dependsOnMe.size() == 0)
            nameToServiceMap.remove(objectName);
         else
         {
            log.debug("Context not removed, it is recording " + 
                      "dependencies: " + ctx);
         }

         // remove the mbean from the instaled ones
         installedServices.remove(ctx);
         //remove from classname to mbean map

         ObjectInstance oi = server.getObjectInstance(objectName);
         String className = oi.getClassName();
         Set mbeans = (Set)classNameToMBeansMap.get(className);
         if (mbeans != null)
         {
            mbeans.remove(objectName);
         }
         creator.remove(objectName);
      }
      else 
      {
         log.debug("no need to remove " + objectName + " from server");
      }
      // This context is no longer installed, but it may still exist
      // to record dependent services
      ctx.state = ServiceContext.NOTYETINSTALLED;
   }

   /**
    * Describe <code>shutdown</code> method here.
    *
    * @jmx:managed-operation
    */
   public void shutdown()
   {
      log.info("Stopping " + nameToServiceMap.size() + " services");
      
      List servicesCopy = new ArrayList(installedServices);
      
      int serviceCounter = 0;
      ObjectName name = null;
      
      ListIterator i = servicesCopy.listIterator(servicesCopy.size());
      while (i.hasPrevious()) 
      {
         ServiceContext ctx = (ServiceContext) i.previous();
         name = ctx.objectName;

         try
         {
            remove(name);
            serviceCounter++;
         }
         catch (Throwable e)
         {
            log.error("Could not remove " + name, e);
         }
      }
      log.info("Stopped " + serviceCounter + " services");
   }
   
   // MBeanRegistration implementation ----------------------------------------
   
   /**
   * #Description of the Method
   *
   * @param server Description of Parameter
   * @param name Description of Parameter
   * @return Description of the Returned Value
   * @exception Exception Description of Exception
   */
   public ObjectName preRegister(MBeanServer server, ObjectName name)
      throws Exception
   {
      this.server = server;
      
      creator = new ServiceCreator(server);
      configurator = new ServiceConfigurator(server, this, creator);

      NotificationFilterSupport removeFilter = new NotificationFilterSupport();
      removeFilter.enableType(ServerConstants.CLASS_REMOVED);

      server.addNotificationListener(DEFAULT_LOADER_REPOSITORY, 
				     this,
				     removeFilter,
				     CLASS_REMOVED_OBJECT);
      
      NotificationFilterSupport addFilter = new NotificationFilterSupport();
      addFilter.enableType(ServerConstants.CLASSLOADER_ADDED);

      server.addNotificationListener(DEFAULT_LOADER_REPOSITORY, 
				     this,
				     addFilter,
				     CLASSLOADER_ADDED_OBJECT);
      
      log.info("Controller MBean online");
      return name == null ? OBJECT_NAME : name;
   }
   
   public void postRegister(Boolean registrationDone)
   {
      if (!registrationDone.booleanValue())
      {
         log.info( "Registration of ServiceController failed" );
      }
   }
   
   public void preDeregister()
   throws Exception
   {
      server.removeNotificationListener(DEFAULT_LOADER_REPOSITORY, this);
   }
   
   public void postDeregister()
   {
   }
   
   
   /**
   * Get the Service interface through which the mbean given by objectName
   * will be managed.
   *
   * @param objectName
   * @param serviceFactory
   * @return The Service value
   * 
   * @throws ClassNotFoundException
   * @throws InstantiationException
   * @throws IllegalAccessException
   */
   private Service getServiceProxy(ObjectName objectName, String serviceFactory)
      throws ClassNotFoundException, InstantiationException,
      IllegalAccessException, JMException
   {
      Service service = null;
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      if (serviceFactory != null && serviceFactory.length() > 0)
      {
         Class clazz = loader.loadClass(serviceFactory);
         ServiceFactory factory = (ServiceFactory)clazz.newInstance();
         service = factory.createService(server, objectName);
      }
      else
      {
         MBeanInfo info = server.getMBeanInfo(objectName);
         MBeanOperationInfo[] opInfo = info.getOperations();
         Class[] interfaces = { Service.class };
         InvocationHandler handler = new ServiceProxy(objectName, opInfo);
         service = (Service)Proxy.newProxyInstance(loader, interfaces, handler);
      }
      
      return service;
   }
   
   
   /** Lookup the ServiceContext for the given serviceName
    */
   public ServiceContext getServiceContext(ObjectName serviceName)
   {
      ServiceContext ctx = (ServiceContext) nameToServiceMap.get(serviceName);
      return ctx;
   }

   public synchronized void registerMBeanClassName(ObjectInstance instance)
   {
      String className  = instance.getClassName();

      Set mbeans = (Set)classNameToMBeansMap.get(className);
      if (mbeans == null)
      {
	 mbeans = new HashSet();
	 classNameToMBeansMap.put(className, mbeans);
      }
      if (!mbeans.contains(instance.getObjectName()))
      {
	 mbeans.add(instance.getObjectName());
      }
   }

   public void handleNotification(Notification notification, Object handback)
   {
      if (handback == CLASSLOADER_ADDED_OBJECT) 
      {
         newClassLoaderNotification();
      } // end of if ()
      else  if (handback == CLASS_REMOVED_OBJECT) 
      {
         String className = notification.getMessage();
         unregisterClassName(className);
      } // end of if ()
   }

   void unregisterClassName(String className)
   {
      Set mbeans = (Set)classNameToMBeansMap.remove(className);
      if (mbeans != null)
      {
	 for (Iterator i = mbeans.iterator(); i.hasNext(); )
	 {
	    ObjectName mbeanName = (ObjectName)i.next();
	    try
	    {
	       Element mbeanConfig = configurator.getConfiguration(mbeanName);
	       stop(mbeanName);
	       destroy(mbeanName);
	       remove(mbeanName);
	       registerWaitingForClass(mbeanName, mbeanConfig);
	    }
	    catch (Exception e)
	    {
	       log.info("Exception removing mbean: " + mbeanName, e);
	    }
	 }
      }
   }

   void registerWaitingForClass(ObjectName mbeanName, Element mbeanElement)
   {
      log.trace("registering waiting for class: " + mbeanName);
      waitingConfigs.put(mbeanName, mbeanElement);
   }

   // Create a Service Context for the service, or get one if it exists
   synchronized ServiceContext createServiceContext(ObjectName objectName)
   {
      // If it is already there just return it
      if (nameToServiceMap.containsKey(objectName)) 
         return (ServiceContext)nameToServiceMap.get(objectName);
         
      // If not create it, add it and return it
      ServiceContext ctx = new ServiceContext();
      ctx.objectName = objectName;
      
      // we keep track of these here
      nameToServiceMap.put(objectName, ctx);
      
      return ctx;
   }

   /**
    * The <code>newClassLoaderNotification</code> method receives notification of a 
    * new classloader and tries to load all the mbeans waiting for their classes.
    * @todo newClassLoaderNotification will not deal well with more than one concurrent call.
    */
   private void newClassLoaderNotification()
   {
      boolean trace = log.isTraceEnabled();
      if( trace )
         log.trace("Scanning for newly supplied classes for waiting mbeans");
      Map waiting = null;
      synchronized (this)
      {
         waiting = waitingConfigs;
         waitingConfigs = new HashMap();
      }
      for (Iterator i = waiting.values().iterator(); i.hasNext(); )
      {
         Element mbeanElement = (Element)i.next();
         try
         {
            if( trace )
               log.trace("trying to install mbean: " + mbeanElement);
            List mbeans = configurator.install(mbeanElement, null);
            for (Iterator j = mbeans.iterator(); j.hasNext(); )
            {
               ObjectName name = (ObjectName)j.next();
               create(name);
               start(name);
            }
         }
         catch (Exception e)
         {
            log.info("Exception when trying to deploy waiting mbean" + mbeanElement, e);
         }
      }
   }


   void registerDependency(ObjectName needs, ObjectName used)
   {
      log.debug("recording that " + needs + " depends on " + used);
      ServiceContext needsCtx = createServiceContext(needs);
      ServiceContext usedCtx = createServiceContext(used);
               

      if (!needsCtx.iDependOn.contains(usedCtx)) 
      {
         // needsCtx depends on usedCtx
         needsCtx.iDependOn.add(usedCtx);
         // UsedCtx needs to know I depend on him
         usedCtx.dependsOnMe.add(needsCtx);
      } // end of if ()
   }
  
   // Inner classes -------------------------------------------------
   
   /**
   * A mapping from the Service interface method names to the corresponding
   * index into the ServiceProxy.hasOp array.
   */
   private static HashMap serviceOpMap = new HashMap();
   
   /**
   * An implementation of InvocationHandler used to proxy of the Service
   * interface for mbeans. It determines which of the start/stop
   * methods of the Service interface an mbean implements by inspecting its
   * MBeanOperationInfo values. Each Service interface method that has a
   * matching operation is forwarded to the mbean by invoking the method
   * through the MBeanServer object.
   */
   public class ServiceProxy
      implements InvocationHandler
   {
      private boolean[] hasOp = { false, false, false, false };
      private ObjectName objectName;
      
      /**
      * Go through the opInfo array and for each operation that matches on of
      * the Service interface methods set the corresponding hasOp array value
      * to true.
      * 
      * @param objectName
      * @param opInfo
      */
      public ServiceProxy(ObjectName objectName, MBeanOperationInfo[] opInfo)
      {
         this.objectName = objectName;
         
         for (int op = 0; op < opInfo.length; op++)
         {
            MBeanOperationInfo info = opInfo[op];
            String name = info.getName();
            Integer opID = (Integer)serviceOpMap.get(name);
            if (opID == null)
            {
               continue;
            }
            
            // Validate that is a no-arg void return type method
            if (info.getReturnType().equals("void") == false)
            {
               continue;
            }
            if (info.getSignature().length != 0)
            {
               continue;
            }
            
            hasOp[opID.intValue()] = true;
         }
      }
      
      /**
      * Map the method name to a Service interface method index and if the
      * corresponding hasOp array element is true, dispatch the method to the
      * mbean we are proxying.
      *
      * @param proxy
      * @param method
      * @param args
      * @return             Always null.
      * @throws Throwable
      */
      public Object invoke(Object proxy, Method method, Object[] args)
         throws Throwable
      {
         String name = method.getName();
         Integer opID = (Integer)serviceOpMap.get(name);
         
         if (opID != null && hasOp[opID.intValue()] == true)
         {
            // deal with those pesky JMX exceptions
            try
            {
               String[] sig = {};
               server.invoke(objectName, name, args, sig);
            }
            catch (Exception e)
            {
               throw JMXExceptionDecoder.decode(e);
            }
         }
         
         return null;
      }
   }
   
   /**
   * Initialize the service operation map.
   */
   static
   {
      serviceOpMap.put("create", new Integer(0));
      serviceOpMap.put("start", new Integer(1));  
      serviceOpMap.put("destroy", new Integer(2));
      serviceOpMap.put("stop", new Integer(3));
   }
}
