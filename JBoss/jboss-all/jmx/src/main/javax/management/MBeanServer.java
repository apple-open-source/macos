/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.io.ObjectInputStream;
import java.util.Set;

/**
 * The interface used to access the MBean server instances.
 *
 * @see javax.management.MBeanServerFactory
 * @see javax.management.loading.DefaultLoaderRepository
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.4 $
 */
public interface MBeanServer
{

   /**
    * Instantiates an object using the default loader repository and default
    * no-args constructor.
    *
    * @see  javax.management.loading.DefaultLoaderRepository
    *
    * @param className Class to instantiate. Must have a public no-args
    *        constructor. Cannot contain a <tt>null</tt> reference.
    *
    * @return  instantiated object
    *
    * @throws ReflectionException If there was an error while trying to invoke
    *         the class's constructor or the given class was not found. This 
    *         exception wraps the actual exception thrown.
    * @throws MBeanException If the object constructor threw a checked exception
    *         during the initialization. This exception wraps the actual
    *         exception thrown.
    * @throws RuntimeMBeanException If the class constructor threw a runtime
    *         exception. This exception wraps the actual exception thrown.
    * @throws RuntimeErrorException If the class constructor threw an error.
    *         This exception wraps the actual error thrown.
    * @throws RuntimeOperationsException If the <tt>className</tt> is <tt>null</tt>.
    *         Wraps an <tt>IllegalArgumentException</tt> instance.
    */
   public Object instantiate(String className) 
            throws ReflectionException, MBeanException;

   /**
    * Instantiates an object using the given class loader. If the loader name contains
    * a <tt>null</tt> reference, the class loader of the MBean server implementation
    * will be used. The object must have a default, no-args constructor.
    *
    * @param   className   Class to instantiate. Must have a public no args constructor.
    *                      Cannot contain a <tt>null</tt> reference.
    * @param   loaderName  Object name of a class loader that has been registered to the server.
    *                      If <tt>null</tt>, the class loader of the MBean server is used.
    * @return  instantiated object
    *
    * @throws ReflectionException If there was an error while trying to invoke
    *         the class's constructor or the given class was not found. This
    *         exception wraps the actual exception thrown.
    * @throws MBeanException If the object constructor threw a checked exception
    *         during the initialization. This exception wraps the actual exception
    *         thrown.
    * @throws InstanceNotFoundException if the specified class loader was not
    *         registered to the agent
    * @throws RuntimeMBeanException If the class constructor raised a runtime
    *         exception. This exception wraps the actual exception thrown.
    * @throws RuntimeErrorException If the class constructor raised an error.
    *         This exception wraps the actual error thrown.
    * @throws RuntimeOperationsException if the <tt>className</tt> is <tt>null</tt>.
    *         Wraps an <tt>IllegalArgumentException</tt> instance.
    */
   public Object instantiate(String className, ObjectName loaderName) 
            throws ReflectionException, MBeanException, InstanceNotFoundException;
   
   /**
    * Instantiates an object using the default loader repository and a given constructor.
    * The class being instantiated must contain a constructor that matches the 
    * signature given as an argument to this method call.
    *
    * @see javax.management.loading.DefaultLoaderRepository
    *
    * @param className  class to instantiate
    * @param params     argument values for the constructor call
    * @param signature  signature of the constructor as fully qualified class names
    *
    * @return instantiated object
    *
    * @throws ReflectionException If there was an error while trying to invoke
    *         the class's constructor or the given class was not found. This
    *         exception wraps the actual exception thrown.
    * @throws MBeanException If the object constructor raised a checked exception
    *         during the initialization. This exception wraps the actual exception
    *         thrown.
    * @throws RuntimeMBeanException If the class constructor raised a runtime
    *         exception. This exception wraps the actual exception thrown.
    * @throws RuntimeErrorException If the class constructor raised an error.
    *         This exception wraps the actual error thrown.
    * @throws RuntimeOperationsException if the <tt>className</tt> is <tt>null</tt>.
    *         Wraps an <tt>IllegalArgumentException</tt> instance.
    */    
   public Object instantiate(String className, Object[] params, String[] signature)
            throws ReflectionException, MBeanException;
   
   /**
    * Instantiates an object using the given class loader. If the loader name contains
    * a <tt>null</tt> reference, the class loader of the MBean server implementation
    * will be used. The object must contain a constructor with a matching signature
    * given as a parameter to this call.
    *
    * @param   className   class to instantiate
    * @param   loaderName  object name of a registered class loader in the agent.
    * @param   params      argument values for the constructor call
    * @param   signature   signature of the constructor as fully qualified class name strings
    *
    * @return instantiated object
    *
    * @throws ReflectionException If there was an error while trying to invoke the
    *         class's constructor or the given class was not found. this exception
    *         wraps the actual exception thrown.
    * @throws MBeanException If the object constructor raised a checked exception
    *         during the initialization. This exception wraps the actual exception thrown.
    * @throws InstanceNotFoundException if the specified class loader was not
    *         registered to the agent.
    * @throws RuntimeMBeanException If the class constructor raised a runtime
    *         exception. This exception wraps the actual exception thrown.
    * @throws RuntimeErrorException If the class constructor raised an error.
    *         This exception wraps the actual error thrown.
    * @throws RuntimeOperationsException if the <tt>className</tt> argument is <tt>null</tt>.
    *         Wraps an <tt>IllegalArgumentException</tt> instance.
    */
   public Object instantiate(String className, ObjectName loaderName, Object[] params, String[] signature)
            throws ReflectionException, MBeanException, InstanceNotFoundException;
   
   public ObjectInstance createMBean(String className, ObjectName name) 
            throws ReflectionException, InstanceAlreadyExistsException, MBeanRegistrationException, MBeanException, NotCompliantMBeanException;
   
   public ObjectInstance createMBean(String className, ObjectName name, ObjectName loaderName)
            throws ReflectionException, InstanceAlreadyExistsException, MBeanRegistrationException, MBeanException, NotCompliantMBeanException, InstanceNotFoundException;
   
   public ObjectInstance createMBean(String className, ObjectName name, Object[] params, String[] signature)
            throws ReflectionException, InstanceAlreadyExistsException, MBeanRegistrationException, MBeanException, NotCompliantMBeanException;
            
   public ObjectInstance createMBean(String className, ObjectName name, ObjectName loaderName, Object[] params, String[] signature)
            throws ReflectionException, InstanceAlreadyExistsException, MBeanRegistrationException, MBeanException, NotCompliantMBeanException, InstanceNotFoundException;

   public ObjectInstance registerMBean(Object object, ObjectName name) 
            throws InstanceAlreadyExistsException, MBeanRegistrationException, NotCompliantMBeanException;

   public void unregisterMBean(ObjectName name) 
            throws InstanceNotFoundException, MBeanRegistrationException;

   public ObjectInstance getObjectInstance(ObjectName name) 
            throws InstanceNotFoundException;

   public Set queryMBeans(ObjectName name, QueryExp query);

   public Set queryNames(ObjectName name, QueryExp query);

   public boolean isRegistered(ObjectName name);

   public Integer getMBeanCount();

   public Object getAttribute(ObjectName name, String attribute)
            throws MBeanException, AttributeNotFoundException, InstanceNotFoundException, ReflectionException;

   public AttributeList getAttributes(ObjectName name, String[] attributes) 
            throws InstanceNotFoundException, ReflectionException;

   public void setAttribute(ObjectName name, Attribute attribute)
            throws InstanceNotFoundException, AttributeNotFoundException, InvalidAttributeValueException, MBeanException, ReflectionException;

   public AttributeList setAttributes(ObjectName name, AttributeList attributes) 
            throws InstanceNotFoundException, ReflectionException;

   public Object invoke(ObjectName name, String operationName, Object[] params, String[] signature)
            throws InstanceNotFoundException, MBeanException, ReflectionException;

   public String getDefaultDomain();

   public void addNotificationListener(ObjectName name, NotificationListener listener, NotificationFilter filter, Object handback) 
            throws InstanceNotFoundException;

   public void addNotificationListener(ObjectName name, ObjectName listener, NotificationFilter filter, Object handback) 
            throws InstanceNotFoundException;

   public void removeNotificationListener(ObjectName name, NotificationListener listener)
            throws InstanceNotFoundException, ListenerNotFoundException;

   public void removeNotificationListener(ObjectName name, ObjectName listener) 
            throws InstanceNotFoundException, ListenerNotFoundException;

   public MBeanInfo getMBeanInfo(ObjectName name) 
            throws InstanceNotFoundException, IntrospectionException, ReflectionException;

   public boolean isInstanceOf(ObjectName name, String className) 
            throws InstanceNotFoundException;

   public ObjectInputStream deserialize(ObjectName name, byte[] data) 
            throws InstanceNotFoundException, OperationsException;

   public ObjectInputStream deserialize(String className, byte[] data) 
            throws OperationsException, ReflectionException;

   public ObjectInputStream deserialize(String className, ObjectName loaderName, byte[] data)
            throws InstanceNotFoundException, OperationsException, ReflectionException;

}

