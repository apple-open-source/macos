/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.metadata;


import org.jboss.deployment.DeploymentException;
import org.w3c.dom.Element;

/** The configuration information for invoker-proxy bindingss that may be
 * tied to a EJB container.
 *   @author <a href="mailto:bill@jboss.org">Bill Burke</a>
 *   @version $Revision: 1.1.2.2 $
 */
public class InvokerProxyBindingMetaData extends MetaData
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------
   /** The unique name of the invoker proxy binding */
   private String name;
   /** The detached invoker MBean service associated with the proxy */
   private String mbean;
   /** The class name of the org.jboss.ejb.EJBProxyFactory implementation used
    * to create proxies for this configuration
    */
   private String proxyFactory;
   /** An arbitary configuration to pass to the EJBProxyFactory implementation
    */
   private Element proxyFactoryConfig;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   public InvokerProxyBindingMetaData(String name)
   {
      this.name = name;
   }

   // Public --------------------------------------------------------

   /** Get the unique name of the invoker proxy binding */
   public String getName()
   {
      return name;
   }

   /** Get the detached invoker MBean service name associated with the proxy */
   public String getInvokerMBean()
   {
      return mbean;
   }

   /** Get the class name of the org.jboss.ejb.EJBProxyFactory implementation
    * used to create proxies for this configuration
    */
   public String getProxyFactory()
   {
      return proxyFactory;
   }

   /** An arbitary configuration to pass to the EJBProxyFactory implementation
    */
   public Element getProxyFactoryConfig()
   {
      return proxyFactoryConfig;
   }

   /** Import the jboss.xml jboss/invoker-proxy-bindings/invoker-proxy-binding
    * child elements
    * @param element jboss/invoker-proxy-bindings/invoker-proxy-binding
    * @throws DeploymentException
    */
   public void importJbossXml(Element element) throws DeploymentException
   {
      mbean = getUniqueChildContent(element, "invoker-mbean");
      proxyFactory = getUniqueChildContent(element, "proxy-factory");
      proxyFactoryConfig = getUniqueChild(element, "proxy-factory-config");
   }
}
