/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: AxisInvocationHandler.java,v 1.8.2.3 2003/11/06 15:36:04 cgjung Exp $

package org.jboss.net.axis;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.Serializable;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.net.URL;
import java.util.HashMap;
import java.util.Map;

import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import org.jboss.logging.Logger;
import org.apache.axis.ConfigurationException;
import org.apache.axis.EngineConfiguration;
import org.apache.axis.EngineConfigurationFactory;
import org.apache.axis.client.Call;
import org.apache.axis.client.Service;
import org.apache.axis.configuration.EngineConfigurationFactoryFinder;

/**
 * An invocation handler that allows typed and persistent 
 * client access to remote SOAP/Axis services. 
 * Adds method/interface to name resolution
 * to the axis client engine. Unfortunately the 
 * AxisClientProxy has a package-protected
 * constructor, otherwise we could inherit from there. 
 * @deprecated Due to the inherent deficiencies in 
 * using pure reflection meat-data to access web services, we recommend
 * using the axis "stub" way of creating web service references
 * 
 * @todo Reflection combined with metadata from a JavaBean BeanInfo or the
 * same wsdl descriptor passed to the WSDL2Java class is a better aproach
 * than stub generation. This class should be the preferred method once this
 * is added.
 * 
 * @created  1. Oktober 2001, 09:29
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.8.2.3 $
 */
public class AxisInvocationHandler implements InvocationHandler, Serializable
{
   private static final Logger log = Logger.getLogger(AxisInvocationHandler.class);

   //
   // Attributes
   //
	
   /** mapping of methods to interface names */
   protected Map methodToInterface;
   /** mapping of methods to method names */
   protected Map methodToName;
   /** the call object to which we delegate */
   transient protected Call call;
   /** if attached to a server-engine, we can reattach */
   protected String rootContext;
   /** the endpoint to which we are attached */
   protected String endPoint;
	
   //
   // Constructors
   //
	
   /** 
    * Creates a new AxisInvocationHandler and 
    * save some origin information to re-attach to the
    * engine after being serialized.
    * @param call the Axis call object
    * @param methodMap a map of Java method to service method names
    * @param interfaceMap a map of Java interface to service names 
    */
   public AxisInvocationHandler(Call call, Map methodMap, Map interfaceMap)
   {
      this.call = call;
      this.methodToInterface = interfaceMap;
      this.methodToName = methodMap;
      // we obtain the configuration of the service
      EngineConfiguration myEngineConfig = call.getService().getEngine().getConfig();
		
      try
      {
         rootContext = (String) myEngineConfig.getGlobalOptions().get(Constants.CONFIGURATION_CONTEXT);
      }
      catch (ConfigurationException e)
      {
         // access problem
      }
      catch (NullPointerException e)
      {
         // no global options
      }
      
// if the endpoint has already been specified,
      // save it for re-attachement
      if (call.getTargetEndpointAddress() != null)
      {
         endPoint = call.getTargetEndpointAddress().toString();
      }
   }

   /** 
    * Creates a new AxisInvocationHandler 
    * @param endpoint target address of the service
    * @param service an Axis service object
    * @param methodMap a map of Java method to service method names
    * @param interfaceMap a map of Java interface to service names 
    */
   public AxisInvocationHandler(
      URL endpoint,
      Service service,
      Map methodMap,
      Map interfaceMap)
   {
      this(new Call(service), methodMap, interfaceMap);
      call.setTargetEndpointAddress(endpoint);
      setBasicAuthentication(endpoint);
      endPoint = endpoint.toString();
   }

   /** 
    * Creates a new AxisInvocationHandler 
    * @param endPoint target url of the web service
    * @param methodMap a map of Java method to service method names
    * @param interfaceMap a map of Java interface to service names 
    * @param maintainSession a flag that indicates whether this handler 
    * 	should keep a persistent session with the service endpoint
    */
   public AxisInvocationHandler(
      URL endPoint,
      Map methodMap,
      Map interfaceMap,
      boolean maintainSession)
   {
      this(endPoint, new Service(), methodMap, interfaceMap);
      call.setMaintainSession(maintainSession);
   }

   /** 
    * Creates a new AxisInvocationHandler that keeps a persistent session with 
    * the service endpoint
    * @param endPoint target url of the web service
    * @param methodMap a map of Java method to service method names
    * @param interfaceMap a map of Java interface to service names 
    */
   public AxisInvocationHandler(URL endPoint, Map methodMap, Map interfaceMap)
   {
      this(endPoint, methodMap, interfaceMap, true);
   }

   /** 
    * Creates a new AxisInvocationHandler that keeps a persistent session with 
    * the service endpoint. The unqualified name of the 
    * intercepted Java interface will be the used service name.
    * @param endPoint target url of the web service
    * @param methodMap a map of Java method to service method names
    */
   public AxisInvocationHandler(URL endPoint, Map methodMap)
   {
      this(endPoint, methodMap, new DefaultInterfaceMap());
   }

   /** 
    * Creates a new AxisInvocationHandler that keeps a persistent session with 
    * the service endpoint. The unqualified name of the 
    * intercepted Java interface will be the used service name. 
    * Intercepted methods are mapped straightforwardly to web service names.
    * @param endPoint target url of the web service
    * @param methodMap a map of Java method to service method names
    */
   public AxisInvocationHandler(URL endPoint)
   {
      this(endPoint, new DefaultMethodMap());
   }

   //
   // Helpers
   //
	
   /** helper to transfer url authentication information into engine */
   protected void setBasicAuthentication(URL target)
   {
      String userInfo = target.getUserInfo();
      if (userInfo != null)
      {
         java.util.StringTokenizer tok = new java.util.StringTokenizer(userInfo, ":");
         if (tok.hasMoreTokens())
         {
            call.setUsername(tok.nextToken());
            if (tok.hasMoreTokens())
            {
               call.setPassword(tok.nextToken());
            }
         }
      }
   }

   //
   // Invocationhandling API
   //
	
   /** invoke given namespace+method+args */
   public Object invoke(String serviceName, String methodName, Object[] args)
      throws java.rmi.RemoteException
   {
      try
      {
         return call.invoke(serviceName, methodName, args);
      }
      finally
      {
         call.setReturnType(null);
         call.removeAllParameters();
      }
   }

   /** invoke with additional method parameter signature */
   public Object invoke(
      String serviceName,
      String methodName,
      Object[] args,
      Class[] parameters)
      throws java.rmi.RemoteException
   {
      // classes are normally ignored
      return invoke(serviceName, methodName, args);
   }

   /** generic invocation method */
   public Object invoke(
      Object target,
      java.lang.reflect.Method method,
      Object[] args)
      throws java.lang.Throwable
   {
      return invoke(
         (String) methodToInterface.get(method),
         (String) methodToName.get(method),
         args,
         method.getParameterTypes());
   }


   //
   // Static API
   //
	
   /** default creation of service */
   public static Object createAxisService(Class _interface, URL endpoint)
   {
      return createAxisService(_interface, new AxisInvocationHandler(endpoint));
   }

   /** default creation of service */
   public static Object createAxisService(
      Class _interface,
      URL endpoint, Service service)
   {
      return createAxisService(
         _interface,
         new AxisInvocationHandler(endpoint, service, new DefaultMethodMap(), new DefaultInterfaceMap()));
   }

   /** default creation of service */
   public static Object createAxisService(
      Class _interface,
      Call call)
   {
      return createAxisService(
         _interface,
         new AxisInvocationHandler(call, new DefaultMethodMap(), new DefaultInterfaceMap()));
   }

   /** default creation of service */
   public static Object createAxisService(
      Class _interface,
      AxisInvocationHandler handler)
   {
      return Proxy.newProxyInstance(
         _interface.getClassLoader(),
         new Class[]{_interface},
         handler);
   }

   /**
    * a tiny helper that does some default mapping of methods to interface names
    * we do not hash the actual reflection information because
    * we could collide with proxy serialisation holding fucking
    * old classes
    */

   public static class DefaultInterfaceMap extends HashMap
   {

      /** no entries is the default */
      public DefaultInterfaceMap()
      {
         super(0);
      }

      /** returns default interface if no mapping of method/interface is defined */
      public Object get(Object key)
      {

         // first try a direct lookup
         Object result = super.get(((Method) key).getName());

         if (result == null)
         {
            // if that is unsuccessful, we try to
            // lookup the class/interface itself
            result = super.get(((Method) key).getDeclaringClass().getName());

            // if that is not specified, we simply extract the
            // un-qualified classname
            if (result == null)
            {
               String sresult = ((Method) key).getDeclaringClass().getName();
               if (sresult.indexOf(".") != -1)
                  sresult = sresult.substring(sresult.lastIndexOf(".") + 1);
               result = sresult;
            }
         }

         return result;
      }

      /** registers an interface name for a given class or method */
      public Object put(Object key, Object value)
      {
         if (key instanceof Method)
         {
            return super.put(((Method) key).getName(), value);
         }
         else if (key instanceof Class)
         {
            return super.put(((Class) key).getName(), value);
         }
         else
         {
            return super.put(key, value);
         }
      }

   }

   /**
    * a tiny helper that does some default mapping of methods to method names
    * we do not hash the actual reflection information because
    * we could collide with proxy serialisation holding fucking
    * old classes
    */

   public static class DefaultMethodMap extends HashMap
   {

      /** no entries is the default */
      public DefaultMethodMap()
      {
         super(0);
      }

      /** returns default interface if no mapping is defined */
      public Object get(Object key)
      {

         Object result = super.get(((Method) key).getName());

         if (result == null)
         {
            result = ((Method) key).getName();
         }

         return result;
      }

      /** registers a new method */
      public Object put(Object key, Object value)
      {
         if (key instanceof Method)
         {
            return super.put(((Method) key).getName(), value);
         }
         else
         {
            return super.put(key, value);
         }
      }
   }
	
   /** 
    * serialization helper to reattach to engine, must be private
    * to be called correctly 
    */
   private void readObject(ObjectInputStream stream) throws IOException, ClassNotFoundException
   {
      stream.defaultReadObject();
		
		// try to find the engine that we were attached to
		try{
			EngineConfigurationFactory factory=EngineConfigurationFactoryFinder.
			newFactory(new ObjectName(rootContext));
		
			EngineConfiguration engine=null;
		
			if(factory!=null) {
				engine=factory.getClientEngineConfig();
			}
		
			if(engine!=null) {
				call=new Call(new Service(engine));
			} else {
				// not there, try the default config
				call=new Call(new Service());
			}
		} catch(MalformedObjectNameException e) {
			throw new IOException("Could not contact jmx configuration factory."+e);
		}
		
		URL endpoint=new URL(endPoint);
		call.setTargetEndpointAddress(endpoint);
		setBasicAuthentication(endpoint);
	}

}
