/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.mx.util;

import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.lang.reflect.InvocationHandler;

import java.util.HashMap;

import javax.management.Attribute;
import javax.management.InstanceNotFoundException;
import javax.management.MBeanAttributeInfo;
import javax.management.MBeanInfo;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.MalformedObjectNameException;

import org.jboss.util.NestedRuntimeException;
import org.jboss.mx.util.JMXExceptionDecoder;
import org.jboss.mx.util.MBeanServerLocator;
import org.jboss.mx.util.MBeanProxyInstance;

/**
 * A factory for producing MBean proxies.
 *
 * <p>Created proxies will also implement {@link org.jboss.mx.util.MBeanProxyInstance}
 *    allowing access to the proxies configuration.
 *
 * <p><b>Revisions:</b>
 * <p><b>20020321 Adrian Brock:</b>
 * <ul>
 * <li>Don't process attributes using invoke.
 * </ul>
 *
 * @version <tt>$Revision: 1.1.2.2 $</tt>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>.
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author <a href="mailto:adrian.brock@happeningtimes.com">Adrian Brock</a>.
 */
public class MBeanProxyExt
   implements InvocationHandler, MBeanProxyInstance
{
   /** The server to proxy invoke calls to. */
   private final MBeanServer server;

   /** The name of the object to invoke. */
   private final ObjectName name;

   /** The MBean's attributes */
   private final HashMap attributeMap = new HashMap();
   
   /**
    * Construct a MBeanProxy.
    */
   MBeanProxyExt(final ObjectName name, final MBeanServer server)
   {
      this.name = name;
      this.server = server;
        
      // The MBean's attributes
      try
      {
         MBeanInfo info = server.getMBeanInfo(name);
         MBeanAttributeInfo[] attributes = info.getAttributes();

         for (int i = 0; i < attributes.length; ++i)
            attributeMap.put(attributes[i].getName(), attributes[i]);
      }
      catch (Exception e)
      {
         throw new NestedRuntimeException("Error creating MBeanProxy: " + name, e);
      }
   }

   /** Used when args is null. */
   private static final Object EMPTY_ARGS[] = {};

   /**
    * Invoke the configured MBean via the target MBeanServer and decode
    * any resulting JMX exceptions that are thrown.
    */
   public Object invoke(final Object proxy,
                        final Method method,
                        final Object[] args)
      throws Throwable
   {
      // if the method belongs to ProxyInstance, then invoke locally
      Class type = method.getDeclaringClass();
      if (type == MBeanProxyInstance.class) {
         return method.invoke(this, args);
      }

      String methodName = method.getName();

      // Get attribute
      if (methodName.startsWith("get") && args == null)
      {
         String attrName = methodName.substring(3);
         MBeanAttributeInfo info = (MBeanAttributeInfo) attributeMap.get(attrName);
         if (info != null)
         {
            String retType = method.getReturnType().getName();
            if (retType.equals(info.getType()))
            {
               try
               {
                  return server.getAttribute(name, attrName);
               }
               catch (Exception e)
               {
                  throw JMXExceptionDecoder.decode(e);
               }
            }
         }
      }

      // Is attribute
      else if (methodName.startsWith("is") && args == null)
      {
         String attrName = methodName.substring(2);
         MBeanAttributeInfo info = (MBeanAttributeInfo) attributeMap.get(attrName);
         if (info != null && info.isIs())
         {
            Class retType = method.getReturnType();
            if (retType.equals(Boolean.class) || retType.equals(Boolean.TYPE))
            {
               try
               {
                  return server.getAttribute(name, attrName);
               }
               catch (Exception e)
               {
                  throw JMXExceptionDecoder.decode(e);
               }
            }
         }
      }

      // Set attribute
      else if (methodName.startsWith("set") && args != null && args.length == 1)
      {
         String attrName = methodName.substring(3);
         MBeanAttributeInfo info = (MBeanAttributeInfo) attributeMap.get(attrName);
         if (info != null && method.getReturnType() == Void.TYPE)
         {
            try
            {
               server.setAttribute(name, new Attribute(attrName, args[0]));
               return null;
            }
            catch (Exception e)
            {
               throw JMXExceptionDecoder.decode(e);
            }
         }
      }

      // Operation

      // convert the parameter types to strings for JMX
      Class[] types = method.getParameterTypes();
      String[] sig = new String[types.length];
      for (int i = 0; i < types.length; i++) {
         sig[i] = types[i].getName();
      }

      // invoke the server and decode JMX exceptions
      try {
         return server.invoke(name, methodName, args == null ? EMPTY_ARGS : args, sig);
      }
      catch (Exception e) {
         throw JMXExceptionDecoder.decode(e);
      }
   }


   ///////////////////////////////////////////////////////////////////////////
   //                          MBeanProxyInstance                           //
   ///////////////////////////////////////////////////////////////////////////

   public final ObjectName getMBeanProxyObjectName()
   {
      return name;
   }

   public final MBeanServer getMBeanProxyMBeanServer()
   {
      return server;
   }


   ///////////////////////////////////////////////////////////////////////////
   //                            Factory Methods                            //
   ///////////////////////////////////////////////////////////////////////////

   /**
    * Create an MBean proxy.
    *
    * @param intf    The interface which the proxy will implement.
    * @param name    A string used to construct the ObjectName of the
    *                MBean to proxy to.
    * @return        A MBean proxy.
    *
    * @throws javax.management.MalformedObjectNameException    Invalid object name.
    */
   public static Object create(final Class intf, final String name)
      throws MalformedObjectNameException
   {
      return create(intf, new ObjectName(name));
   }

   /**
    * Create an MBean proxy.
    *
    * @param intf      The interface which the proxy will implement.
    * @param name      A string used to construct the ObjectName of the
    *                  MBean to proxy to.
    * @param server    The MBeanServer that contains the MBean to proxy to.
    * @return          A MBean proxy.
    *
    * @throws javax.management.MalformedObjectNameException    Invalid object name.
    */
   public static Object create(final Class intf,
                               final String name,
                               final MBeanServer server)
      throws MalformedObjectNameException
   {
      return create(intf, new ObjectName(name), server);
   }    
   
   /**
    * Create an MBean proxy.
    *
    * @param intf    The interface which the proxy will implement.
    * @param name    The name of the MBean to proxy invocations to.
    * @return        A MBean proxy.
    */
   public static Object create(final Class intf, final ObjectName name)
   {
      return create(intf, name, MBeanServerLocator.locateJBoss());
   }

   /**
    * Create an MBean proxy.
    *
    * @param intf      The interface which the proxy will implement.
    * @param name      The name of the MBean to proxy invocations to.
    * @param server    The MBeanServer that contains the MBean to proxy to.
    * @return          A MBean proxy.
    */
   public static Object create(final Class intf,
                               final ObjectName name,
                               final MBeanServer server)
   {
      // make a which delegates to MBeanProxyInstance's cl for it's class resolution
      ClassLoader cl = new ClassLoader(intf.getClassLoader())
      {
         public Class loadClass(final String className) throws ClassNotFoundException
         {
            try {
               return super.loadClass(className);
            }
            catch (ClassNotFoundException e) {
               // only allow loading of MBeanProxyInstance from this loader
               if (className.equals(MBeanProxyInstance.class.getName())) {
                  return MBeanProxyInstance.class.getClassLoader().loadClass(className);
               }
               
               // was some other classname, throw the CNFE
               throw e;
            }
         }
      };

      return Proxy.newProxyInstance(cl,
                                    new Class[] { MBeanProxyInstance.class, intf },
                                    new MBeanProxyExt(name, server));
   }
}
/*
vim:tabstop=3:et:shiftwidth=3
*/
