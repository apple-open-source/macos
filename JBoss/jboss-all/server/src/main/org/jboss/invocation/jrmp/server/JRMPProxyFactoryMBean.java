/*
* JBoss, the OpenSource J2EE WebOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.invocation.jrmp.server;

import javax.management.ObjectName;

import org.jboss.system.Service;

import org.w3c.dom.Element;

/** An mbean interface for a proxy factory that can expose any interface
 * with RMI compatible semantics for access to remote clients using JRMP
 * as the transport protocol.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public interface JRMPProxyFactoryMBean extends Service
{
   /** Get the server side JRMPInvoker mbean that will be used as the
    * RMI/JRMP transport handler.
    */
   public ObjectName getInvokerName();
   /** Set the server side JRMPInvoker mbean that will be used as the
    * RMI/JRMP transport handler.
    */
   public void setInvokerName(ObjectName jmxInvokerName);

   /** Get the server side mbean that exposes the invoke operation for the
    exported interface */
   public ObjectName getTargetName();
   /** Set the server side mbean that exposes the invoke operation for the
    exported interface */
   public void setTargetName(ObjectName targetName);

   /** Get the JNDI name under which the HttpInvokerProxy will be bound */
   public String getJndiName();
   /** Set the JNDI name under which the HttpInvokerProxy will be bound */
   public void setJndiName(String jndiName);

   /** Get the RMI compatible interface that the HttpInvokerProxy implements */
   public Class getExportedInterface();
   /** Set the RMI compatible interface that the HttpInvokerProxy implements */
   public void setExportedInterface(Class exportedInterface);

   /** Get the proxy client side interceptor configuration
    * 
    * @return the proxy client side interceptor configuration
    */ 
   public Element getClientInterceptors();
   /** Set the proxy client side interceptor configuration
    * 
    * @param config the proxy client side interceptor configuration
    */ 
   public void setClientInterceptors(Element config) throws Exception;
   /** Get the proxy instance created by the factory.
    */
   public Object getProxy();
}
