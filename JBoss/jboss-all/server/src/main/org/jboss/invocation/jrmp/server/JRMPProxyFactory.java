/*
* JBoss, the OpenSource J2EE WebOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.invocation.jrmp.server;

import java.util.ArrayList;
import java.util.Iterator;
import javax.management.ObjectName;
import javax.naming.InitialContext;

import org.jboss.invocation.InvokerInterceptor;
import org.jboss.naming.Util;
import org.jboss.proxy.ClientMethodInterceptor;
import org.jboss.proxy.GenericProxyFactory;
import org.jboss.system.Registry;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.metadata.MetaData;
import org.w3c.dom.Element;

/** Create an interface proxy that uses RMI/JRMP to communicate with the server
 * side object that exposes the corresponding JMX invoke operation. Requests
 * make through the proxy are sent to the JRMPInvoker instance the proxy
 * is bound to. 
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.5 $
 */
public class JRMPProxyFactory extends ServiceMBeanSupport
   implements JRMPProxyFactoryMBean
{
   /** The server side JRMPInvoker mbean that will handle RMI/JRMP transport */
   private ObjectName invokerName;
   /** The server side mbean that exposes the invoke operation for the
    exported interface */
   private ObjectName targetName;
   /** The Proxy object which uses the proxy as its handler */
   protected Object theProxy;
   /** The JNDI name under which the proxy will be bound */
   private String jndiName;
   /** The interface that the proxy implements */
   private Class exportedInterface;
   /** The optional definition */
   private Element interceptorConfig;
   /** The interceptor Classes defined in the interceptorConfig */
   private ArrayList interceptorClasses = new ArrayList();

   public JRMPProxyFactory()
   {
      interceptorClasses.add(ClientMethodInterceptor.class);
      interceptorClasses.add(InvokerInterceptor.class);
   }

   public ObjectName getInvokerName()
   {
      return invokerName;
   }
   public void setInvokerName(ObjectName invokerName)
   {
      this.invokerName = invokerName;
   }

   public ObjectName getTargetName()
   {
      return targetName;
   }
   public void setTargetName(ObjectName targetName)
   {
      this.targetName = targetName;
   }

   public String getJndiName()
   {
      return jndiName;
   }
   public void setJndiName(String jndiName)
   {
      this.jndiName = jndiName;
   }

   public Class getExportedInterface()
   {
      return exportedInterface;
   }
   public void setExportedInterface(Class exportedInterface)
   {
      this.exportedInterface = exportedInterface;
   }

   public Element getClientInterceptors()
   {
      return interceptorConfig;
   }
   public void setClientInterceptors(Element config) throws Exception
   {
      this.interceptorConfig = config;
      Iterator interceptorElements = MetaData.getChildrenByTagName(interceptorConfig, "interceptor");
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      interceptorClasses.clear();
      while( interceptorElements != null && interceptorElements.hasNext() )
      {
         Element ielement = (Element) interceptorElements.next();
         String className = null;
         className = MetaData.getElementContent(ielement);
         Class clazz = loader.loadClass(className);
         interceptorClasses.add(clazz);
         log.debug("added interceptor type: "+clazz);
      }
   }

   public Object getProxy()
   {
      return theProxy;
   }

   /** Initializes the servlet.
    */
   protected void startService() throws Exception
   {
      /* Create a binding between the invoker name hash and the jmx name
      This is used by the JRMPInvoker to map from the Invocation ObjectName
      hash value to the target JMX ObjectName.
      */
      Integer nameHash = new Integer(targetName.hashCode());
      Registry.bind(nameHash, targetName);

      // Create the service proxy
      Object cacheID = null;
      String proxyBindingName = null;
      Class[] ifaces = {exportedInterface};
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      createProxy(cacheID, proxyBindingName, loader, ifaces);
      log.debug("Created JRMPPRoxy for service="+targetName
         +", nameHash="+nameHash+", invoker="+invokerName);

      if( jndiName != null )
      {
         InitialContext iniCtx = new InitialContext();
         Util.bind(iniCtx, jndiName, theProxy);
         log.debug("Bound proxy under jndiName="+jndiName);
      }
   }

   protected void stopService() throws Exception
   {
      Integer nameHash = new Integer(targetName.hashCode());
      Registry.unbind(nameHash);
      if( jndiName != null )
      {
         InitialContext iniCtx = new InitialContext();
         Util.unbind(iniCtx, jndiName);
      }
      this.interceptorClasses.clear();
      this.theProxy = null;
   }

   protected void createProxy
   (
      Object cacheID, 
      String proxyBindingName,
      ClassLoader loader,
      Class[] ifaces
   )
   {
      GenericProxyFactory proxyFactory = new GenericProxyFactory();
      theProxy = proxyFactory.createProxy(cacheID, targetName, invokerName,
         jndiName, proxyBindingName, interceptorClasses, loader, ifaces);
   }

   protected void rebind() throws Exception
   {
      log.debug("(re-)Binding " + jndiName);
      Util.rebind(new InitialContext(), jndiName, theProxy);
   }

   protected ArrayList getInterceptorClasses()
   {
      return interceptorClasses;
   }
}
