/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.server;

/**
 * Server related constant variables. These are constants that are used internally
 * by the MBean server implementation or are used to configure the MBean server.
 * Different JMX service specific constants should be added to the <tt>ServiceConstants</tt>
 * interface.
 *
 * @see org.jboss.mx.service.ServiceConstants
 * @see org.jboss.mx.server.MBeanServerImpl
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.14.2.4 $
 *   
 */
public interface ServerConstants
{
   // Constants -----------------------------------------------------

   /**
    * The name of the protected implementation domain
    * Pass this object to the registry in the values map as the key and value
    * to register in this domain
    */
   final static String JMI_DOMAIN                         = "JMImplementation";
   
   /**
    * String representation of the MBean server delegate MBean object name.
    */
   final static String MBEAN_SERVER_DELEGATE              = JMI_DOMAIN + ":type=MBeanServerDelegate";
   
   /**
    * String representation of the MBean registry mbean object name.
    */
   final static String MBEAN_REGISTRY                     = JMI_DOMAIN + ":type=MBeanRegistry";
   
   /**
    * The default domain name for the MBean server. If a default domain is not specified
    * when the server is created, this value (<tt>"DefaultDomain"</tt>) is used.
    *
    * @see  javax.management.MBeanServerFactory
    */
   final static String DEFAULT_DOMAIN                     = "DefaultDomain";

   
   // MBean Server Delegate -----------------------------------------

   /**
    * The specification name of the implementation. This value can be retrieved from the MBean server delegate.
    */
   final static String SPECIFICATION_NAME                 = "Java Management Extensions Instrumentation and Agent Specification";
   
   /**
    * The specification version of the implementation. This value can be retrieved from the MBean server delegate.
    */
   final static String SPECIFICATION_VERSION              = "1.0";
   
   /**
    * The specification vendor name. This value can be retrieved from the MBean server delegate.
    */
   final static String SPECIFICATION_VENDOR               = "Sun Microsystems, Inc.";
   
   /**
    * The name of the implementation. This value can be retrieved from the MBean server delegate.
    */
   final static String IMPLEMENTATION_NAME                = "JBossMX";
   
   /**
    * The version of the implementation. This value can be retrieved from the MBean server delegate.
    */
   final static String IMPLEMENTATION_VERSION             =  "1.1.0";
   
   /**
    * The vendor of the implementation. This value can be retrieved from the MBean server delegate.
    */
   final static String IMPLEMENTATION_VENDOR              = "JBoss Organization";
   
   
   // System properties ---------------------------------------------
   
    /**
    * This property can be used to configure which Model MBean implementation is
    * used for the MBean agent's required Model MBean 
    * (see {@link javax.management.modelmbean.RequiredModelMBean}). The required
    * Model MBean implementation will delegate all calls to the class specified
    * with this property, e.g 
    * <tt>-Djbossmx.required.modelmbean.class=org.jboss.mx.modelmbean.XMBean</tt>
    * would instantiate and delegate all calls made to <tt>RequiredModelMBean</tt>
    * instance to JBossMX XMBean implementation.
    */
   final static String REQUIRED_MODELMBEAN_CLASS_PROPERTY = "jbossmx.required.modelmbean.class";
   
   /**
    * This constant defines the default Model MBean implementation used for spec
    * required Model MBean (<tt>javax.management.modelmbean.RequiredModelMBean</tt>)
    * instance. Defaults to {@link org.jboss.mx.modelmbean.XMBean} implementation.
    */
   final static String DEFAULT_REQUIRED_MODELMBEAN_CLASS  = "org.jboss.mx.modelmbean.XMBean";
   
   /**
    * This property can be used to configure the default class loader repository
    * implementation for the JVM.
    */   
   final static String LOADER_REPOSITORY_CLASS_PROPERTY   = "jbossmx.loader.repository.class";
   final static String DEFAULT_LOADER_REPOSITORY_CLASS    = "org.jboss.mx.loading.UnifiedLoaderRepository3";
   final static String UNIFIED_LOADER_REPOSITORY_CLASS    = "org.jboss.mx.loading.UnifiedLoaderRepository3";
   final static String DEFAULT_SCOPED_REPOSITORY_CLASS    = "org.jboss.mx.loading.HeirarchicalLoaderRepository3";
   final static String DEFAULT_SCOPED_REPOSITORY_PARSER_CLASS    = "org.jboss.mx.loading.HeirarchicalLoaderRepository3ConfigParser";

   final static String MBEAN_REGISTRY_CLASS_PROPERTY      = "jbossmx.mbean.registry.class";
   final static String DEFAULT_MBEAN_REGISTRY_CLASS       = "org.jboss.mx.server.registry.BasicMBeanRegistry";
   
   final static String MBEAN_SERVER_CLASS_PROPERTY        = "jbossmx.mbean.server.class";
   final static String DEFAULT_MBEAN_SERVER_CLASS         = "org.jboss.mx.server.MBeanServerImpl";
   
   final static String OPTIMIZE_REFLECTED_DISPATCHER      = "jbossmx.optimized.dispatcher";

   /**
    * This property can be used to configure the logging adapter factory
    */   
   final static String LOGGER_FACTORY_CLASS_PROPERTY   = "jbossmx.logging.adapter.factory.class";
   final static String DEFAULT_LOGGER_FACTORY_CLASS    = "org.jboss.mx.logging.log4j.Log4jAdapterFactory";

   //added for UnifiedLoaderRepository becoming an mbean that issues notifications
   final static String DEFAULT_LOADER_NAME = JMI_DOMAIN + ":service=LoaderRepository,name=Default";

   final static String CLASSLOADER_ADDED = "jboss.mx.classloader.added";
   final static String CLASSLOADER_REMOVED = "jboss.mx.classloader.removed";
   final static String CLASS_REMOVED = "jboss.mx.class.removed";

   /**
    * The key for the context classloader for an MBean registration
    */
   final static String CLASSLOADER                        = "org.jboss.mx.classloader";

}
      



