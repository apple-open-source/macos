/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.services.binding;

import java.util.HashMap;
import java.beans.PropertyEditorManager;
import java.beans.PropertyEditor;
import javax.management.Attribute;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.MBeanInfo;
import javax.management.MBeanAttributeInfo;

import org.w3c.dom.Element;
import org.w3c.dom.NodeList;

import org.jboss.logging.Logger;
import org.jboss.metadata.MetaData;
import org.jboss.util.Strings;
import org.jboss.util.Classes;
import org.jboss.deployment.DeploymentException;

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

 Any mbeanAttrName attribute reference has the corresponding value replaced
 with any ${host} and ${port} references with the associated host and port
 bindings.
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.3.2.4 $
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
   public void applyConfig(ServiceConfig config, MBeanServer server)
      throws Exception
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
         // Build a mapping of the attribute names to their type name
         ObjectName serviceName = new ObjectName(config.getServiceName());
         MBeanInfo info = server.getMBeanInfo(serviceName);
         MBeanAttributeInfo[] attrInfo = info.getAttributes();
         HashMap attrTypeMap = new HashMap();
         for(int a = 0; a < attrInfo.length; a ++)
         {
            MBeanAttributeInfo attr = attrInfo[a];
            attrTypeMap.put(attr.getName(), attr.getType());
         }

         int port = bindings[0].getPort();
         String host = bindings[0].getHostName();
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
            Attribute hostAttr = createAtribute(port, host, attrTypeMap,
               hostAttrName, host);
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
            Attribute theAttr = createAtribute(port, host, attrTypeMap,
               name, attrExp);
            server.setAttribute(serviceName, theAttr);            
         }
      }
   }

   /** Create a JMX Attribute with the correct type value object. This
    * converts the given attrExp into an Attribute for attrName with
    * replacement of any ${host} ${port} references in the attrExp
    * replaced with the given port/host values.
    * @param port The binding port value
    * @param host The binding host value
    * @param attrTypeMap the name to type map for the service attributes
    * @param attrName the name of the attribute to create
    * @param attrExp the string exp for the attribute value
    * @return the JMX attribute instance
    * @throws Exception thrown on an invalid attribute name or inability
    *    to find a valid property editor 
    */ 
   private Attribute createAtribute(int port, String host,
      HashMap attrTypeMap, String attrName, String attrExp)
      throws Exception
   {
      String attrText = replaceHostAndPort(attrExp, host, ""+port);
      String typeName = (String) attrTypeMap.get(attrName);
      if( typeName == null )
      {
         throw new DeploymentException("No such attribute: " + attrName);
      }
      // Convert the type
      Class attrType = Classes.loadClass(typeName);
      PropertyEditor editor = PropertyEditorManager.findEditor(attrType);
      if( editor == null )
      {
         String msg = "No property editor for attribute: " + attrName +
            "; type=" + typeName;
         throw new DeploymentException(msg);
      }
      editor.setAsText(attrText);
      Object attrValue = editor.getValue();
      log.debug("setAttribute, name='"+attrName+"', text="+attrText
         +", value="+attrValue);
      Attribute theAttr = new Attribute(attrName, attrValue);
      return theAttr;
   }

   /** Loop over text and replace any ${host} and ${port} strings. If there are
    * any ${x} system property references in the resulting replacement string
    * these will be replaced with the corresponding System.getProperty("x")
    * value if one exists.
    * @param text the text exp with optional ${host} ${port} references
    * @param host the binding host value
    * @param port the binding port value
    */
   private String replaceHostAndPort(String text, String host, String port)
   {
      if( text == null )
         return null;

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
      return Strings.replaceProperties(replacement.toString());
   }
}
