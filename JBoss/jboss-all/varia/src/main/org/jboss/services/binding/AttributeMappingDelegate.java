/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.services.binding;

import javax.management.Attribute;
import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.w3c.dom.Element;
import org.w3c.dom.NodeList;

import org.jboss.logging.Logger;
import org.jboss.metadata.MetaData;

/** An implementation of the ServicesConfigDelegate that expects a delegate-config
 element of the form:
    <delegate-config portName="portAttrName" hostName="hostAttrName">
      <attribute name="mbeanAttrName">host-port-expr</attribute>
      ...
    </delegate-config>
 where the portAttrName is the attribute name of the mbean service
 to which the (int port) value should be applied and the hostAttrName
 is the attribute name of the mbean service to which the (String virtualHost)
 value should be applied.

@version $Revision: 1.3.2.1 $
@author Scott.Stark@jboss.org
 */
public class AttributeMappingDelegate 
   implements ServicesConfigDelegate
{
   private static Logger log = Logger.getLogger(AttributeMappingDelegate.class);

   /** Take the given config and map it onto the service specified in the
    config using JMX via the given server.
    @param config, the service name and its config bindings
    @param server, the JMX server to use to apply the config
    */
   public void applyConfig(ServiceConfig config, MBeanServer server) throws Exception
   {
      Element delegateConfig = (Element) config.getServiceConfigDelegateConfig();
      if( delegateConfig == null )
         throw new IllegalArgumentException("ServiceConfig.ServiceConfigDelegateConfig is null");
      // Check for a port and host name
      String portAttrName = delegateConfig.getAttribute("portName");
      if( portAttrName.length() == 0 )
         portAttrName = null;
      String hostAttrName = delegateConfig.getAttribute("hostName");
      if( hostAttrName.length() == 0 )
         hostAttrName = null;

      // Check for any arbitrary attributes
      NodeList attributes = delegateConfig.getElementsByTagName("attribute");

      // Only the first binding is used as only one (host,port) pair is mapped
      ServiceBinding[] bindings = config.getBindings();
      if( bindings != null && bindings.length > 0 )
      {
         int port = bindings[0].getPort();
         String host = bindings[0].getHostName();
         ObjectName serviceName = new ObjectName(config.getServiceName());
         // Apply the port setting override if the port name was given
         if( portAttrName != null )
         {
            Attribute portAttr = new Attribute(portAttrName, new Integer(port));
            log.debug("setPort, name='"+portAttrName+"' value="+port);
            server.setAttribute(serviceName, portAttr);
         }
         // Apply the host setting override if the port name was given
         if( hostAttrName != null )
         {
            Attribute hostAttr = new Attribute(hostAttrName, host);
            log.debug("setHost, name='"+hostAttrName+"' value="+host);
            server.setAttribute(serviceName, hostAttr);
         }

         /* Apply any other host/port based attributes with replacement of 
          the ${host} and ${port} strings.
         */
         for(int a = 0; a < attributes.getLength(); a ++)
         {
            Element attr = (Element) attributes.item(a);
            String name = attr.getAttribute("name");
            if( name.length() == 0 )
               throw new IllegalArgumentException("attribute element #"+a+" has no name attribute");
            String attrExp = MetaData.getElementContent(attr);
            String attrValue = replaceHostAndPort(attrExp, host, ""+port);
            log.debug("setAttribute, name='"+name+"' value="+attrValue);
            Attribute theAttr = new Attribute(name, attrValue);
            server.setAttribute(serviceName, theAttr);            
         }
      }
   }

   /** Loop over text and replace any ${host} and ${port} strings.
    */
   private String replaceHostAndPort(String text, String host, String port)
   {
      StringBuffer replacement = new StringBuffer(text);
      if( host == null )
         host = "localhost";
      // Simple looping should be replaced with regex package
      String test = replacement.toString();
      int index;
      while( (index = test.indexOf("${host}")) >= 0 )
      {
         replacement.replace(index, index+7, host);
         test = replacement.toString();
      }
      while( (index = test.indexOf("${port}")) >= 0 )
      {
         replacement.replace(index, index+7, port);
         test = replacement.toString();
      }
      return replacement.toString();
   }
}
