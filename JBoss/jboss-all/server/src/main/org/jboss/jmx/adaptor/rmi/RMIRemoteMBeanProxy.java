/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.jmx.adaptor.rmi;



import javax.management.ObjectName;


/**
 * A factory for producing MBean proxies that run on a distant node and access
 * the server through RMI. Most of the code comes from MBeanProxy.
 *
 * @version <tt>$Revision: 1.1.2.1 $</tt>
 * @author <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 */
public class RMIRemoteMBeanProxy 
   implements java.io.Serializable, java.lang.reflect.InvocationHandler
{
   /** The server to proxy invoke calls to. */
   private final RMIAdaptor remoteServer;

   /** The name of the object to invoke. */
   private final ObjectName name;
  
   /**
    * Construct a MBeanProxy.
    */

   RMIRemoteMBeanProxy (final ObjectName name, final javax.management.MBeanServer server) throws Exception
   {
      this.name = name;
      this.remoteServer = getRmiAdaptor ();        
   }

   /** Used when args is null. */
   private static final Object EMPTY_ARGS[] = {};

   /**
    * Invoke the configured MBean via the target MBeanServer and decode
    * any resulting JMX exceptions that are thrown.
    */
   public Object invoke (final Object proxy, final java.lang.reflect.Method method, final Object[] args) throws Throwable
   {
      String methodName = method.getName();

      // Get attribute
      if (methodName.startsWith("get") && args == null)
      {
         String attrName = methodName.substring(3);
         return remoteServer.getAttribute(name, attrName);
      }

      // Is attribute
      else if (methodName.startsWith("is") && args == null)
      {
         String attrName = methodName.substring(2);
         return remoteServer.getAttribute(name, attrName);
      }

      // Set attribute
      else if (methodName.startsWith("set") && args != null && args.length == 1)
      {
         String attrName = methodName.substring(3);
         remoteServer.setAttribute(name, new javax.management.Attribute(attrName, args[0]));
         return null;         
      }

      // Operation

      // convert the parameter types to strings for JMX
      Class[] types = method.getParameterTypes();
      String[] sig = new String[types.length];
      for (int i = 0; i < types.length; i++) {
         sig[i] = types[i].getName();
      }

      // invoke the server and decode JMX exceptions
      return remoteServer.invoke(name, methodName, args == null ? EMPTY_ARGS : args, sig);
   }
   
   protected RMIAdaptor getRmiAdaptor () throws Exception
   {
      return (RMIAdaptor)new javax.naming.InitialContext().lookup (RMIAdaptorService.DEFAULT_JNDI_NAME);
   }


   ///////////////////////////////////////////////////////////////////////////
   //                          MBeanProxyInstance                           //
   ///////////////////////////////////////////////////////////////////////////

   public final ObjectName getMBeanProxyObjectName()
   {
      return name;
   }

   public final RMIAdaptor getMBeanProxyRMIAdaptor()
   {
      return remoteServer;
   }


   ///////////////////////////////////////////////////////////////////////////
   //                            Factory Methods                            //
   ///////////////////////////////////////////////////////////////////////////

   /**
    * Create an MBean proxy.
    *
    * @param intf      The interface which the proxy will implement.
    * @param name      A string used to construct the ObjectName of the
    *                  MBean to proxy to.
    * @param server    The MBeanServer that contains the MBean to proxy to.
    * @return          A MBean proxy.
    *
    * @throws MalformedObjectNameException    Invalid object name.
    */
   public static Object create (final Class intf, final String name, final javax.management.MBeanServer server) throws Exception
   {
      return create(intf, new ObjectName(name), server);
   }    
   
   /**
    * Create an MBean proxy.
    *
    * @param intf      The interface which the proxy will implement.
    * @param name      The name of the MBean to proxy invocations to.
    * @param server    The MBeanServer that contains the MBean to proxy to.
    * @return          A MBean proxy.
    */
   public static Object create (final Class intf, final ObjectName name, final javax.management.MBeanServer server) throws Exception
   {
      return java.lang.reflect.Proxy.newProxyInstance(Thread.currentThread ().getContextClassLoader (), 
                                    new Class[] { intf },
                                    new RMIRemoteMBeanProxy(name, server));
   }
}
