/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.proxy.ejb;


import java.rmi.Remote;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import javax.ejb.EJBHome;
import javax.ejb.EJBMetaData;
import javax.ejb.EJBObject;
import javax.management.ObjectName;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.naming.Reference;
import javax.naming.StringRefAddr;
import javax.rmi.PortableRemoteObject;
import org.jboss.ejb.Container;
import org.jboss.ejb.EJBProxyFactory;
import org.jboss.ejb.EJBProxyFactoryContainer;
import org.jboss.iiop.CorbaNamingService;
import org.jboss.iiop.CorbaORBService;
import org.jboss.iiop.codebase.CodebasePolicy;
import org.jboss.iiop.rmi.AttributeAnalysis;
import org.jboss.iiop.rmi.InterfaceAnalysis;
import org.jboss.iiop.rmi.OperationAnalysis;
import org.jboss.iiop.rmi.ir.InterfaceRepository;
import org.jboss.iiop.rmi.marshal.strategy.SkeletonStrategy;
import org.jboss.invocation.iiop.ReferenceFactory;
import org.jboss.invocation.iiop.ServantRegistries;
import org.jboss.invocation.iiop.ServantRegistry;
import org.jboss.invocation.iiop.ServantRegistryKind;
import org.jboss.invocation.iiop.ServantWithMBeanServer;
import org.jboss.logging.Logger;
import org.jboss.metadata.EntityMetaData;
import org.jboss.metadata.InvokerProxyBindingMetaData;
import org.jboss.metadata.MetaData;
import org.jboss.metadata.SessionMetaData;
import org.jboss.naming.Util;
import org.jboss.system.Registry;
import org.omg.CORBA.Any;
import org.omg.CORBA.InterfaceDef;
import org.omg.CORBA.InterfaceDefHelper;
import org.omg.CORBA.ORB;
import org.omg.CORBA.Policy;
import org.omg.CORBA.Repository;
import org.omg.CosNaming.NameComponent;
import org.omg.CosNaming.NamingContext;
import org.omg.CosNaming.NamingContextExt;
import org.omg.CosNaming.NamingContextExtHelper;
import org.omg.CosNaming.NamingContextHelper;
import org.omg.CosNaming.NamingContextPackage.CannotProceed;
import org.omg.CosNaming.NamingContextPackage.InvalidName;
import org.omg.CosNaming.NamingContextPackage.NotFound;
import org.omg.PortableServer.Current;
import org.omg.PortableServer.CurrentHelper;
import org.omg.PortableServer.POA;
import org.w3c.dom.Element;
import java.net.URL;
import org.jboss.web.WebClassLoader;

/**
 * This is an IIOP "proxy factory" for <code>EJBHome</code>s and
 * <code>EJBObject</code>s. Rather than creating Java proxies (as the JRMP
 * proxy factory does), this factory creates CORBA IORs.
 *<p>
 * An <code>IORFactory</code> is associated to a given enterprise bean. It
 * registers with the IIOP invoker two CORBA servants: an
 * <code>EjbHomeCorbaServant</code> for the bean's
 * <code>EJBHome</code> and an <code>EjbObjectCorbaServant</code> for the
 * bean's <code>EJBObject</code>s.
 *
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.5.2.1 $
 */
public class IORFactory
   implements EJBProxyFactory
{

   // Constants  --------------------------------------------------------------

   private static final Logger staticLogger =
                           Logger.getLogger(IORFactory.class);

   // Attributes -------------------------------------------------------------

   /**
    * A reference for the ORB.
    */
   private ORB orb;

   /**
    * This <code>IORFactory</code>'s container.
    */
   private Container container;

   /**
    * <code>JNDI</code> name of the enterprise bean in the container.
    */
   private String jndiName;

   /**
    * True if the <code>EJBHome</code> should be bound to a JNP/JNDI context
    * in addition to being bound to a CORBA naming context.
    */
   private boolean useJNPContext;

   /**
    * The JNP/JNDI name the <code>EJBHome</code> should be bound to.
    */
   private String jnpName;

   /**
    * <code>EJBMetaData</code> the enterprise bean in the container.
    */
   private EJBMetaDataImplIIOP ejbMetaData;

   /**
    * Mapping from bean methods to <code>SkeletonStrategy</code> instances.
    */
   private Map beanMethodMap;

   /**
    * Mapping from home methods to <code>SkeletonStrategy</code> instances.
    */
   private Map homeMethodMap;

   /**
    * CORBA repository ids of the RMI-IDL interfaces implemented by the bean
    * (<code>EJBObject</code> instance).
    */
   private String[] beanRepositoryIds;

   /**
    * CORBA repository ids of the RMI-IDL interfaces implemented by the bean's
    * home (<code>EJBHome</code> instance).
    */
   private String[] homeRepositoryIds;

   /**
    * <code>ServantRegistry</code> for the container's <code>EJBHome</code>.
    */
   private ServantRegistry homeServantRegistry;

   /**
    * <code>ServantRegistry</code> for the container's <code>EJBObject</code>s.
    */
   private ServantRegistry beanServantRegistry;

   /**
    * <code>ReferenceFactory</code> for the container's <code>EJBHome</code>.
    */
   private ReferenceFactory homeReferenceFactory;

   /**
    * <code>ReferenceFactory</code> for <code>EJBObject</code>s.
    */
   private ReferenceFactory beanReferenceFactory;

   /**
    * Thread-local <code>Current</code> object from which we get the target oid
    * in an incoming IIOP request.
    */
   private Current poaCurrent;

   /**
    * The container's <code>CodebasePolicy</code>.
    */
   private Policy codebasePolicy;

   /**
    * The container's <code>EJBHome</code>.
    */
   private EJBHome ejbHome;

   /**
    * Invoker metadata, which includes the invoker JMX name.
    */
   private InvokerProxyBindingMetaData invokerMetaData;

   /**
    * A reference to the invoker, used for servant registration.
    */
   private ServantRegistries servantRegistries;

   /**
    * The enterprise bean's interface repository implementation.
    */
   private InterfaceRepository iri;

   /**
    * POA for the enterprise bean's interface repository.
    */
   private POA irPoa;

   /**
    * This <code>IORFactory</code>'s logger. Initialized with a
    * per-class logger. Once the enterprise bean's JNDI name is known, the
    * per-class logger will be replaced by a per-instance logger whose name
    * includes the JNDI name.
    */
   private Logger logger = staticLogger;

   // Implementation of the interface ContainerPlugin -------------------------

   public void setContainer(Container container)
   {
      this.container = container;
      if (container != null) {
         String loggerName = IORFactory.class.getName() + '.'
                                   + container.getBeanMetaData().getJndiName();
         logger = Logger.getLogger(loggerName);
      }
   }

   public void create() throws Exception
   {
      // Get orb and irPoa references
      try {
         orb = (ORB)new InitialContext().lookup("java:/"
                                                + CorbaORBService.ORB_NAME);
      }
      catch (NamingException e) {
         throw new Exception("Cannot lookup java:/"
                             + CorbaORBService.ORB_NAME + ": " + e);
      }
      try {
         irPoa = (POA)new InitialContext().lookup("java:/"
                                                + CorbaORBService.IR_POA_NAME);
      }
      catch (NamingException e) {
         throw new Exception("Cannot lookup java:/"
                             + CorbaORBService.IR_POA_NAME + ": " + e);
      }

      // Create a CORBA interface repository for the enterprise bean
      iri = new InterfaceRepository(orb, irPoa, jndiName);

      // Add bean interface info to the interface repository
      iri.mapClass(((EJBProxyFactoryContainer)container).getRemoteClass());
      iri.mapClass(((EJBProxyFactoryContainer)container).getHomeClass());
      iri.finishBuild();

      logger.info("CORBA interface repository for " + jndiName + ": "
                  + orb.object_to_string(iri.getReference()));

      // Create bean method mappings for container invoker
      logger.debug("Bean methods:");

      InterfaceAnalysis interfaceAnalysis =
            InterfaceAnalysis.getInterfaceAnalysis(
                  ((EJBProxyFactoryContainer)container).getRemoteClass());

      beanMethodMap = new HashMap();

      AttributeAnalysis[] attrs = interfaceAnalysis.getAttributes();
      for (int i = 0; i < attrs.length; i++) {
         OperationAnalysis op = attrs[i].getAccessorAnalysis();

         logger.debug("    " + op.getJavaName()
                      + "\n                " + op.getIDLName());
         beanMethodMap.put(op.getIDLName(),
                           new SkeletonStrategy(op.getMethod()));
         op = attrs[i].getMutatorAnalysis();
         if (op != null) {
            logger.debug("    " + op.getJavaName()
                         + "\n                " + op.getIDLName());
            beanMethodMap.put(op.getIDLName(),
                              new SkeletonStrategy(op.getMethod()));
         }
      }

      OperationAnalysis[] ops = interfaceAnalysis.getOperations();
      for (int i = 0; i < ops.length; i++) {
         logger.debug("    " + ops[i].getJavaName()
                      + "\n                " + ops[i].getIDLName());
         beanMethodMap.put(ops[i].getIDLName(),
                           new SkeletonStrategy(ops[i].getMethod()));
      }

      // Initialize repository ids of remote interface
      beanRepositoryIds = interfaceAnalysis.getAllTypeIds();

      // Create home method mappings for container invoker
      logger.debug("Home methods:");

      interfaceAnalysis =
            InterfaceAnalysis.getInterfaceAnalysis(
                  ((EJBProxyFactoryContainer)container).getHomeClass());

      homeMethodMap = new HashMap();

      attrs = interfaceAnalysis.getAttributes();
      for (int i = 0; i < attrs.length; i++) {
         OperationAnalysis op = attrs[i].getAccessorAnalysis();

         logger.debug("    " + op.getJavaName()
                      + "\n                " + op.getIDLName());
         homeMethodMap.put(op.getIDLName(),
                           new SkeletonStrategy(op.getMethod()));
         op = attrs[i].getMutatorAnalysis();
         if (op != null) {
            logger.debug("    " + op.getJavaName()
                         + "\n                " + op.getIDLName());
            homeMethodMap.put(op.getIDLName(),
                              new SkeletonStrategy(op.getMethod()));
         }
      }

      ops = interfaceAnalysis.getOperations();
      for (int i = 0; i < ops.length; i++) {
         logger.debug("    " + ops[i].getJavaName()
                      + "\n                " + ops[i].getIDLName());
         homeMethodMap.put(ops[i].getIDLName(),
                           new SkeletonStrategy(ops[i].getMethod()));
      }

      // Initialize repository ids of home interface
      homeRepositoryIds = interfaceAnalysis.getAllTypeIds();

      // Create codebasePolicy containing the container's codebase string
      logger.debug("container classloader: " + container.getClassLoader()
                   + "\ncontainer parent classloader: "
                   + container.getClassLoader().getParent());
      WebClassLoader wcl = (WebClassLoader)container.getWebClassLoader();
      Any codebase = orb.create_any();
      codebase.insert_string(wcl.getCodebaseString());
      codebasePolicy = orb.create_policy(CodebasePolicy.TYPE, codebase);
      logger.debug("codebasePolicy: " + codebasePolicy);

      // Get the POACurrent object
      poaCurrent = CurrentHelper.narrow(
                  orb.resolve_initial_references("POACurrent"));
   }

   public void start() throws Exception
   {
      // Lookup the invoker in the object registry. This typically cannot
      // be done until our start method as the invokers may need to be started
      // themselves.
      ObjectName oname = new ObjectName(invokerMetaData.getInvokerMBean());
      servantRegistries = (ServantRegistries)Registry.lookup(oname);
      if (servantRegistries == null)
         throw new Exception("invoker is null: " + oname);

      // Read POA usage model from proxy factory config.
      ServantRegistryKind registryWithTransientPOA;
      ServantRegistryKind registryWithPersistentPOA;
      Element proxyFactoryConfig = invokerMetaData.getProxyFactoryConfig();
      String poaUsageModel =
         MetaData.getOptionalChildContent(proxyFactoryConfig, "poa");
      if (poaUsageModel == null || poaUsageModel.equals("shared")) {
         registryWithTransientPOA = ServantRegistryKind.SHARED_TRANSIENT_POA;
         registryWithPersistentPOA = ServantRegistryKind.SHARED_PERSISTENT_POA;
      }
      else if (poaUsageModel.equals("per-servant")) {
         registryWithTransientPOA =
            ServantRegistryKind.TRANSIENT_POA_PER_SERVANT;
         registryWithPersistentPOA =
            ServantRegistryKind.PERSISTENT_POA_PER_SERVANT;
      }
      else {
         throw new Exception("invalid poa element in proxy factory config: "
                             + poaUsageModel);
      }

      // Get CORBA reference to the bean's interface repository
      Repository ir = iri.getReference();

      // Instantiate home servant, bind it to the servant registry, and
      // create CORBA reference to the EJBHome.
      homeServantRegistry =
            servantRegistries.getServantRegistry(registryWithPersistentPOA);

      InterfaceDef homeInterfaceDef =
         InterfaceDefHelper.narrow(ir.lookup_id(homeRepositoryIds[0]));

      String homeServantLoggerName =
         EjbHomeCorbaServant.class.getName() + '.'+ jndiName;

      ServantWithMBeanServer homeServant =
         new EjbHomeCorbaServant(container.getJmxName(),
                                 container.getClassLoader(),
                                 homeMethodMap,
                                 homeRepositoryIds,
                                 homeInterfaceDef,
                                 Logger.getLogger(homeServantLoggerName));

      homeReferenceFactory =
         homeServantRegistry.bind(homeServantName(jndiName),
                                  homeServant,
                                  new Policy[] { codebasePolicy });

      org.omg.CORBA.Object corbaRef =
         homeReferenceFactory.createReference(homeRepositoryIds[0]);
      ((EjbHomeCorbaServant)homeServant).setHomeHandle(
                                             new HomeHandleImplIIOP(corbaRef));
      ejbHome = (EJBHome)PortableRemoteObject.narrow(corbaRef, EJBHome.class);

      // Initialize beanPOA and create metadata depending on the kind of bean
      if (container.getBeanMetaData() instanceof EntityMetaData) {

         // This is an entity bean (lifespan: persistent)
         beanServantRegistry =
            servantRegistries.getServantRegistry(registryWithPersistentPOA);

         Class pkClass;
         EntityMetaData metaData = (EntityMetaData)container.getBeanMetaData();
         String pkClassName = metaData.getPrimaryKeyClass();
         try {
            if (pkClassName != null)
               pkClass = container.getClassLoader().loadClass(pkClassName);
            else
               pkClass = container.getClassLoader().loadClass(
                     metaData.getEjbClass()).getField(
                           metaData.getPrimKeyField()).getClass();
         }
         catch (NoSuchFieldException e) {
            logger.error("Unable to identify Bean's Primary Key class! "
                         + "Did you specify a primary key class and/or field? "
                         + "Does that field exist?");
            throw new Exception("Primary Key Problem");
         }
         catch (NullPointerException e) {
            logger.error("Unable to identify Bean's Primary Key class! "
                         + "Did you specify a primary key class and/or field? "
                         + "Does that field exist?");
            throw new Exception("Primary Key Problem");
         }

         ejbMetaData = new EJBMetaDataImplIIOP(
               ((EJBProxyFactoryContainer)container).getRemoteClass(),
               ((EJBProxyFactoryContainer)container).getHomeClass(),
               pkClass,
               false, // Session
               false, // Stateless
               ejbHome);
      }
      else {

         // This is a session bean (lifespan: transient)
         beanServantRegistry =
            servantRegistries.getServantRegistry(registryWithTransientPOA);

         if (((SessionMetaData)container.getBeanMetaData()).isStateless()) {

            // Stateless session bean
            ejbMetaData = new EJBMetaDataImplIIOP(
                  ((EJBProxyFactoryContainer)container).getRemoteClass(),
                  ((EJBProxyFactoryContainer)container).getHomeClass(),
                  null, // No PK
                  true, // Session
                  true, // Stateless
                  ejbHome);

         }
         else {

            // Stateful session bean
            ejbMetaData = new EJBMetaDataImplIIOP(
                  ((EJBProxyFactoryContainer)container).getRemoteClass(),
                  ((EJBProxyFactoryContainer)container).getHomeClass(),
                  null,  // No PK
                  true,  // Session
                  false, // Stateless
                  ejbHome);
         }
      }

      InterfaceDef beanInterfaceDef =
         InterfaceDefHelper.narrow(ir.lookup_id(beanRepositoryIds[0]));

      String beanServantLoggerName =
         EjbObjectCorbaServant.class.getName() + '.'+ jndiName;

      ServantWithMBeanServer beanServant =
         new EjbObjectCorbaServant(container.getJmxName(),
                                   container.getClassLoader(),
                                   poaCurrent,
                                   beanMethodMap,
                                   beanRepositoryIds,
                                   beanInterfaceDef,
                                   Logger.getLogger(beanServantLoggerName));
      beanReferenceFactory =
         beanServantRegistry.bind(beanServantName(jndiName),
                                  beanServant,
                                  new Policy[] { codebasePolicy });

      // Just for testing
      logger.info("EJBHome reference for " + jndiName + ":\n"
                  + orb.object_to_string((org.omg.CORBA.Object)ejbHome));

      // Get JNP usage info from proxy factory config.
      useJNPContext = MetaData.getOptionalChildBooleanContent(
                           proxyFactoryConfig, "register-ejbs-in-jnp-context");

      Context initialContext = new InitialContext();

      if (useJNPContext) {
         String jnpContext =
            MetaData.getOptionalChildContent(proxyFactoryConfig,
                                             "jnp-context");
         if (jnpContext != null && !jnpContext.equals("")) {
            jnpName = jnpContext + "/" + jndiName;
         }
         else {
            jnpName = jndiName;
         }

         try {
            // Bind the bean home in the JNDI initial context
            Util.rebind(initialContext,
                        jnpName,
                        new Reference(
                              "javax.ejb.EJBHome",
                              new StringRefAddr("IOR",
                                       orb.object_to_string(
                                               (org.omg.CORBA.Object)ejbHome)),
                              IIOPHomeFactory.class.getName(),
                              null));
            logger.info("Home IOR for " + container.getBeanMetaData().getEjbName()
                        + " bound to " + jnpName + " in JNP naming service");
         }
         catch (NamingException e) {
            throw new Exception("Cannot bind EJBHome in JNDI:\n" + e);
         }
      }

      NamingContextExt corbaContext = null;
      try {
         // Obtain local (in-VM) CORBA naming context
         corbaContext = NamingContextExtHelper.narrow((org.omg.CORBA.Object)
               initialContext.lookup("java:/" +
                                     CorbaNamingService.NAMING_NAME));


      }
      catch (NamingException e) {
         throw new Exception("Cannot lookup java:/" +
                             CorbaNamingService.NAMING_NAME + ":\n" + e);
      }

      try {
         // Register bean home in local CORBA naming context
         rebind(corbaContext, jndiName, (org.omg.CORBA.Object)ejbHome);
         logger.info("Home IOR for " + container.getBeanMetaData().getEjbName()
                     + " bound to " + jndiName + " in CORBA naming service");
      }
      catch (Exception e) {
         logger.error("Cannot bind EJBHome in CORBA naming service:", e);
         throw new Exception("Cannot bind EJBHome in CORBA naming service:\n"
                             + e);
      }

   }

   public void stop()
   {
      try {
         // Get initial JNP/JNDI context
         Context initialContext = new InitialContext();

         if (useJNPContext) {
            // Unbind bean home from the JNDI initial context
            try {
               initialContext.unbind(jnpName);
            }
            catch (NamingException namingException) {
               logger.error("Cannot unbind EJBHome from JNDI", namingException);
            }
         }

         // Get local (in-VM) CORBA naming context
         NamingContextExt corbaContext =
               NamingContextExtHelper.narrow((org.omg.CORBA.Object)
                     initialContext.lookup("java:/"
                                   + CorbaNamingService.NAMING_NAME));

         // Unregister bean home from local CORBA naming context
         try {
            NameComponent[] name = corbaContext.to_name(jndiName);
            corbaContext.unbind(name);
         }
         catch (InvalidName invalidName) {
            logger.error("Cannot unregister EJBHome from CORBA naming service",
                         invalidName);
         }
         catch (NotFound notFound) {
            logger.error("Cannot unregister EJBHome from CORBA naming service",
                         notFound);
         }
         catch (CannotProceed cannotProceed) {
            logger.error("Cannot unregister EJBHome from CORBA naming service",
                         cannotProceed);
         }
      }
      catch (NamingException namingException) {
         logger.error("Unexpected error in JNDI lookup", namingException);
      }

      // Deactivate the home servant and the bean servant
      try {
         homeServantRegistry.unbind(homeServantName(jndiName));
      }
      catch (Exception e) {
         logger.error("Cannot deactivate home servant", e);
      }
      try {
         beanServantRegistry.unbind(beanServantName(jndiName));
      }
      catch (Exception e) {
         logger.error("Cannot deactivate bean servant", e);
      }

      // Deactivate the interface repository
      iri.shutdown();
   }

   public void destroy()
   {
   }

   // Implementation of the interface EJBProxyFactory -------------------------

   public void setInvokerMetaData(InvokerProxyBindingMetaData imd)
   {
      invokerMetaData = imd;
   }

   public void setInvokerBinding(String binding)
   {
      jndiName = binding;
   }

   public EJBMetaData getEJBMetaData()
   {
      return ejbMetaData;
   }

   public Object getEJBHome()
   {
      return ejbHome;
   }

   public Object getStatelessSessionEJBObject()
   {
      try {
         return (EJBObject)PortableRemoteObject.narrow(
               beanReferenceFactory.createReference(beanRepositoryIds[0]),
               EJBObject.class);
      }
      catch (Exception e) {
         throw new RuntimeException("Unable to create reference to EJBObject\n"
                                    + e);
      }
   }

   public Object getStatefulSessionEJBObject(Object id)
   {
      try {
         return (EJBObject)PortableRemoteObject.narrow(
               beanReferenceFactory.createReferenceWithId(
                                                   id,
                                                   beanRepositoryIds[0]),
               EJBObject.class);
      }
      catch (Exception e) {
         throw new RuntimeException("Unable to create reference to EJBObject\n"
                                    + e);
      }
   }

   public Object getEntityEJBObject(Object id)
   {
      if (logger.isTraceEnabled()) {
         logger.trace("getEntityEJBObject(), id class is "
                      + id.getClass().getName());
      }
      try {
         return (EJBObject)PortableRemoteObject.narrow(
               beanReferenceFactory.createReferenceWithId(
                                                   id,
                                                   beanRepositoryIds[0]),
               EJBObject.class);
      }
      catch (Exception e) {
         throw new RuntimeException("Unable to create reference to EJBObject\n"
                                    + e);
      }
   }

   public Collection getEntityCollection(Collection ids)
   {
      if (logger.isTraceEnabled()) {
         logger.trace("entering getEntityCollection()");
      }
      Collection collection = new ArrayList(ids.size());
      Iterator idEnum = ids.iterator();
      while(idEnum.hasNext()) {
         collection.add(getEntityEJBObject(idEnum.next()));
      }
      if (logger.isTraceEnabled()) {
         logger.trace("leaving getEntityCollection()");
      }
      return collection;
   }

   // Static methods ----------------------------------------------------------

   /**
    * Returns the CORBA repository id of a given the RMI-IDL interface.
    */
   public static String rmiRepositoryId(Class clz)
   {
      return "RMI:" + clz.getName() + ":0000000000000000";
   }

   /**
    * (Re)binds an object to a name in a given CORBA naming context, creating
    * any non-existent intermediate contexts along the way.
    */
   public static void rebind(NamingContextExt ctx,
                             String strName, org.omg.CORBA.Object obj)
          throws Exception
   {
      NameComponent[] name = ctx.to_name(strName);
      NamingContext intermediateCtx = ctx;

      for (int i = 0; i < name.length - 1; i++ ) {
         NameComponent[] relativeName = new NameComponent[] { name[i] };
         try {
            intermediateCtx = NamingContextHelper.narrow(
                  intermediateCtx.resolve(relativeName));
         }
         catch (NotFound e) {
            intermediateCtx = intermediateCtx.bind_new_context(relativeName);
         }
      }
      intermediateCtx.rebind(new NameComponent[] { name[name.length - 1] },
                             obj);
   }

   /**
    * Returns the name of a home servant for an EJB with the given jndiName.
    * The home servant will be bound to this name in a ServantRegistry.
    */
   private static String homeServantName(String jndiName)
   {
      return "EJBHome/" + jndiName;
   }

   /**
    * Returns the name of a bean servant for an EJB with the given jndiName.
    * The bean servant will be bound to this name in a ServantRegistry.
    */
   private static String beanServantName(String jndiName)
   {
      return "EJBObject/" + jndiName;
   }

}
