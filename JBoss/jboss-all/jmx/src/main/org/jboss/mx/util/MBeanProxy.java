/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.util;

import java.lang.reflect.Proxy;

import java.io.Serializable;

import javax.management.DynamicMBean;
import javax.management.ObjectName;
import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.InstanceAlreadyExistsException;
import javax.management.ReflectionException;
import javax.management.MBeanException;
import javax.management.MBeanRegistrationException;
import javax.management.NotCompliantMBeanException;
import javax.management.InvalidAttributeValueException;

/**
 *
 * @see java.lang.reflect.Proxy
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.3.4.2 $
 *   
 */
public class MBeanProxy
{

   // Static --------------------------------------------------------
   
   /**
    * Creates a proxy to an MBean in the given MBean server.
    *
    * @param   intrface    the interface this proxy implements
    * @param   name        object name of the MBean this proxy connects to
    * @param   agentID     agent ID of the MBean server this proxy connects to
    *
    * @return  proxy instance
    *
    * @throws MBeanProxyCreationException if the proxy could not be created
    */
   public static Object get(Class intrface, ObjectName name, String agentID) throws MBeanProxyCreationException
   {
      return get(intrface, name, (MBeanServer)MBeanServerFactory.findMBeanServer(agentID).get(0));
   }

   /**
    * Creates a proxy to an MBean in the given MBean server.
    *
    * @param   intrface the interface this proxy implements
    * @param   name     object name of the MBean this proxy connects to
    * @param   server   MBean server this proxy connects to
    *
    * @return proxy instance
    *
    * @throws MBeanProxyCreationException if the proxy could not be created
    */
   public static Object get(Class intrface, ObjectName name, MBeanServer server)  throws MBeanProxyCreationException
   {
      return get(new Class[] { intrface, ProxyContext.class, DynamicMBean.class }, name, server);
   }

   /**
    */
   public static Object get(ObjectName name, MBeanServer server)  throws MBeanProxyCreationException
   {
      return get(new Class[] { ProxyContext.class, DynamicMBean.class }, name, server);
   }

   private static Object get(Class[] interfaces, ObjectName name, MBeanServer server) throws MBeanProxyCreationException
   {
      return Proxy.newProxyInstance(
            Thread.currentThread().getContextClassLoader(),
            interfaces, new JMXInvocationHandler(server, name)
      );
   }
   
   /**
    * Convenience method for registering an MBean and retrieving a proxy for it.
    *
    * @param   instance MBean instance to be registered
    * @param   intrface the interface this proxy implements
    * @param   name     object name of the MBean
    * @param   agentID  agent ID of the MBean server this proxy connects to
    *
    * @return proxy instance
    *
    * @throws MBeanProxyCreationException if the proxy could not be created
    */
   public static Object create(Class instance, Class intrface, ObjectName name, String agentID) throws MBeanProxyCreationException
   {
      return create(instance, intrface, name, (MBeanServer)MBeanServerFactory.findMBeanServer(agentID).get(0));
   }   
   
   /**
    * Convenience method for registering an MBean and retrieving a proxy for it.
    *
    * @param   instance MBean instance to be registered
    * @param   intrface the interface this proxy implements
    * @param   name     object name of the MBean
    * @param   server   MBean server this proxy connects to
    *
    * @throws MBeanProxyCreationException if the proxy could not be created
    */
   public static Object create(Class instance, Class intrface, ObjectName name, MBeanServer server) throws MBeanProxyCreationException
   {
      try
      {
         server.createMBean(instance.getName(), name);
         return get(intrface, name, server);
      }
      catch (ReflectionException e) {
         throw new MBeanProxyCreationException("Creating the MBean failed: " + e.toString());
      }
      catch (InstanceAlreadyExistsException e) {
         throw new MBeanProxyCreationException("Instance already exists: " + name);
      }
      catch (MBeanRegistrationException e) {
         throw new MBeanProxyCreationException("Error registering the MBean to the server: " + e.toString());
      }
      catch (MBeanException e) {
         throw new MBeanProxyCreationException(e.toString());
      }
      catch (NotCompliantMBeanException e) {
         throw new MBeanProxyCreationException("Not a compliant MBean " + instance.getClass().getName() + ": " + e.toString());
      }
   }
   
}



