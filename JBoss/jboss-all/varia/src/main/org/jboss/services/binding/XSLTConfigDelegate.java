/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.services.binding;

import java.io.StringReader;
import javax.management.Attribute;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.dom.DOMResult;
import javax.xml.transform.dom.DOMSource;
import javax.xml.transform.stream.StreamSource;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.NodeList;
import org.w3c.dom.Node;

import org.jboss.logging.Logger;
import org.jboss.metadata.MetaData;
import org.jboss.util.Strings;

/** An implementation of the ServicesConfigDelegate that expects a delegate-config
 element of the form:
    <delegate-config portName="portAttrName" hostName="hostAttrName">
      <xslt-config configName="ConfigurationElement"><![CDATA[
 XSL document contents...
]]>
      </xslt-config>
      <xslt-param name="p1">value1</xslt-param>
    </delegate-config>
 The portAttrName and hostAttrName are currently unused. Perhaps these should
 be used as the names of the host and port parameters in the XSL script. Currently
 the host and port bindings are passed into the XSL script as the 'host' and
 'port' global parameters.
 
 The xslt-param elements specify arbitrary XSL script parameter name/value pairs
 that will be set on the Transformer.

@version $Revision: 1.1.2.3 $
@author Scott.Stark@jboss.org
 */
public class XSLTConfigDelegate implements ServicesConfigDelegate
{
   private static Logger log = Logger.getLogger(XSLTConfigDelegate.class);

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

      // Get the XSL doc
      Element xslConfigElement = (Element) delegateConfig.getElementsByTagName("xslt-config").item(0);
      String configName = xslConfigElement.getAttribute("configName");
      Node xslContent = xslConfigElement.getFirstChild();
      if( configName.length() == 0 )
         throw new IllegalArgumentException("No valid configName attribute found");

      // Get the DOM config from the configName attribute
      ObjectName serviceName = new ObjectName(config.getServiceName());
      Element mbeanConfig = (Element) server.getAttribute(serviceName, configName);
      if( mbeanConfig == null )
      {
         log.debug("No value found for config attribute: "+configName);
         return;
      }

      // Create the XSL transformer
      String xslText = xslContent.getNodeValue();
      log.trace("XSL text:"+xslText);
      StreamSource xslSource = new StreamSource(new StringReader(xslText));
      TransformerFactory factory = TransformerFactory.newInstance();
      Transformer transformer = factory.newTransformer(xslSource);

      // Only the first binding is used as only one (host,port) pair is mapped
      ServiceBinding[] bindings = config.getBindings();
      if( bindings != null && bindings.length > 0 )
      {
         int port = bindings[0].getPort();
         String host = bindings[0].getHostName();
         // Set the host an port params to that binding values
         if( host != null )
         {
            transformer.setParameter("host", host);
            log.debug("set host parameter to:"+host);
         }
         transformer.setParameter("port", new Integer(port));
         log.debug("set port parameter to:"+port);

         // Check for any arbitrary attributes
         NodeList attributes = delegateConfig.getElementsByTagName("xslt-param");
         // xslt-param are transform parameters
         for(int a = 0; a < attributes.getLength(); a ++)
         {
            Element attr = (Element) attributes.item(a);
            String name = attr.getAttribute("name");
            if( name.length() == 0 )
               throw new IllegalArgumentException("attribute element #"
                            +a+" has no name attribute");
            String attrExp = MetaData.getElementContent(attr);
            String attrValue = Strings.replaceProperties(attrExp);
            transformer.setParameter(name, attrValue);

            log.debug("set "+name+" parameter to:"+attrValue);
         }

         // Transform the current config element
         DOMSource src = new DOMSource(mbeanConfig);
         DOMResult result = new DOMResult();
         transformer.transform(src, result);
         // Write the transformed config back to the mbean
         Document newMbeanDoc = (Document) result.getNode();
         Element newMbeanConfig = newMbeanDoc.getDocumentElement();
         log.debug("Updating DOM attribute to: "+newMbeanConfig);
         Attribute mbeanConfigAttr = new Attribute(configName, newMbeanConfig);
         server.setAttribute(serviceName, mbeanConfigAttr);
      }
   }

}
