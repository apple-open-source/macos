/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.jmx.connector.invoker;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.UndeclaredThrowableException;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.jboss.invocation.Invocation;
import org.jboss.invocation.MarshalledInvocation;
import org.jboss.mx.server.ServerConstants;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.system.Registry;

/**
 * A JBoss service exposes an invoke(Invocation) operation that maps
 * calls to the ExposedInterface onto the MBeanServer this service
 * is registered with. It is used in conjunction with a proxy factory
 * to expose the MBeanServer to remote clients through arbitrary
 * protocols.<p>
 *
 * It sets up the correct classloader before unmarshalling the
 * arguments, this relies on the ObjectName being seperate from
 * from the other method arguments to avoid unmarshalling them
 * before the classloader is determined from the ObjectName.<p>
 *
 * The interface is configurable, it must be similar to MBeanServer,
 * though not necessarily derived from it<p>
 *
 * The invoker is configurable and must be specified
 *
 * @todo The interface should be MBeanServerConnection in jmx1.2
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.4 $
 *
 * @jmx:mbean name="jboss.jmx:type=adaptor,protocol=INVOKER"
 *            extends="org.jboss.system.ServiceMBean"
 **/
public class InvokerAdaptorService
   extends ServiceMBeanSupport
   implements InvokerAdaptorServiceMBean, ServerConstants
{
   private static ObjectName mbeanRegistry;

   static
   {
      try
      {
         mbeanRegistry = new ObjectName(MBEAN_REGISTRY);
      }
      catch (Exception e)
      {
         throw new RuntimeException(e.toString());
      }
   }

   private Map marshalledInvocationMapping = new HashMap();
   private Class exportedInterface;

   public InvokerAdaptorService()
   {
   }

   /**
    * @jmx:managed-attribute
    */
   public Class getExportedInterface()
   {
      return exportedInterface;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setExportedInterface(Class exportedInterface)
   {
      this.exportedInterface = exportedInterface;
   }

   protected void startService()
      throws Exception
   {
      // Build the interface method map
      Method[] methods = exportedInterface.getMethods();
      HashMap tmpMap = new HashMap(methods.length);
      for(int m = 0; m < methods.length; m ++)
      {
         Method method = methods[m];
         Long hash = new Long(MarshalledInvocation.calculateHash(method));
         tmpMap.put(hash, method);
      }
      marshalledInvocationMapping = Collections.unmodifiableMap(tmpMap);
      // Place our ObjectName hash into the Registry so invokers can resolve it
      Registry.bind(new Integer(serviceName.hashCode()), serviceName);
   }

   protected void stopService()
      throws Exception
   {
      Registry.unbind(new Integer(serviceName.hashCode()));
   }

   /** 
    * Expose the service interface mapping as a read-only attribute
    *
    * @jmx:managed-attribute
    *
    * @return A Map<Long hash, Method> of the MBeanServer
    */
   public Map getMethodMap()
   {
      return marshalledInvocationMapping;
   }

   /**
    * Expose the MBeanServer service via JMX to invokers.
    *
    * @jmx:managed-operation
    *
    * @param invocation    A pointer to the invocation object
    * @return              Return value of method invocation.
    * 
    * @throws Exception    Failed to invoke method.
    */
   public Object invoke(Invocation invocation)
       throws Exception
   {
      // Make sure we have the correct classloader before unmarshalling
      Thread thread = Thread.currentThread();
      ClassLoader oldCL = thread.getContextClassLoader();

      ClassLoader newCL = null;
      // Get the MBean this operation applies to
      ObjectName objectName = (ObjectName) invocation.getValue("JMX_OBJECT_NAME");
      if (objectName != null)
      {
         // Obtain the ClassLoader associated with the MBean deployment
         newCL = (ClassLoader) server.invoke
         (
            mbeanRegistry, "getValue",
            new Object[] { objectName, CLASSLOADER },
            new String[] { ObjectName.class.getName(), String.class.getName() }
         );
      }

      if (newCL != null && newCL != oldCL)
         thread.setContextClassLoader(newCL);

      try
      {
         // Set the method hash to Method mapping
         if (invocation instanceof MarshalledInvocation)
         {
            MarshalledInvocation mi = (MarshalledInvocation) invocation;
            mi.setMethodMap(marshalledInvocationMapping);
         }
         // Invoke the MBeanServer method via reflection
         Method method = invocation.getMethod();
         Object[] args = invocation.getArguments();
         Object value = null;
         try
         {
            String name = method.getName();
            Class[] paramTypes = method.getParameterTypes();
            Method mbeanServerMethod = MBeanServer.class.getMethod(name, paramTypes);
            value = mbeanServerMethod.invoke(server, args);
         }
         catch(InvocationTargetException e)
         {
            Throwable t = e.getTargetException();
            if( t instanceof Exception )
               throw (Exception) t;
            else
               throw new UndeclaredThrowableException(t, method.toString());
         }

         return value;
      }
      finally
      {
         if (newCL != null && newCL != oldCL)
            thread.setContextClassLoader(oldCL);
      }
   }
}
