/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.catalina;

import java.util.Stack;
import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerException;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.dom.DOMSource;
import javax.xml.transform.sax.SAXResult;

import org.w3c.dom.Element;

import org.apache.catalina.startup.Embedded;
import org.apache.catalina.util.xml.SaxContext;
import org.apache.catalina.util.xml.XmlAction;

import org.jboss.logging.Logger;

/** This is a step toward supporting elements of the catalina server.xml config
 as child elements of the jboss.jcml mbean/config extended configuration element.
 
@author Scott.Stark@jboss.org
@author alain.coetmeur@caissedesdepots.fr
@version $Revision: 1.4 $
 */
public class ConfigHandler
{
   private Logger log;
   
   /** Creates new ConfigHandler */
   public ConfigHandler(Logger log)
   {
      this.log = log;
   }

   /** Handle the Connector configuration elements.
    @param config, the mbean/config jboss.jcml element
    @param root, the object to which the Connectors will be added using
    addConnector
    @param debug, a flag indicating if the XmlMapper debug level should be set
    */
   public void applyHostConfig(Element config, Embedded embedded, boolean debug)
      throws Exception
   {
      if( config == null )
         return;

      // Create an XmlMapper utility
      LoggedXmlMapper mapper = new LoggedXmlMapper(log);
      if (debug)
         mapper.setDebug(999);
      mapper.setValidating(false);

      mapper.addRule("Server/Service", new ServiceCreateAction());
      mapper.addRule("Server/Service", mapper.setProperties());

      // Setup the mapping rules for Connectors
      mapper.addRule("Server/Service/Connector", mapper.objectCreate
         ("org.apache.catalina.connector.http.HttpConnector", "className"));
      mapper.addRule("Server/Service/Connector", mapper.setProperties());
      mapper.addRule("Server/Service/Connector", mapper.addChild
         ("addConnector", "org.apache.catalina.Connector"));
      mapper.addRule("Server/Service/Connector/Factory", mapper.objectCreate
         ("org.apache.catalina.net.DefaultServerSocketFactory", "className"));
      mapper.addRule("Server/Service/Connector/Factory",
      mapper.setProperties());
      mapper.addRule("Server/Service/Connector/Factory", mapper.addChild
         ("setFactory", "org.apache.catalina.net.ServerSocketFactory"));

      // Setup the mapping rules for Engines
      mapper.addRule("Server/Service/Engine", new EngineCreateAction("org.apache.catalina.core.StandardEngine",
         "className"));
      mapper.addRule("Server/Service/Engine", mapper.setProperties());
      mapper.addRule("Server/Service/Engine", new AddEngineAction());

      // Setup the mapping rules for Engine virtual hosts
      mapper.addRule("Server/Service/Engine/Host",
      mapper.objectCreate("org.apache.catalina.core.StandardHost", "className"));
      mapper.addRule("Server/Service/Engine/Host", mapper.setProperties());
      mapper.addRule("Server/Service/Engine/Host",  mapper.addChild("addChild", "org.apache.catalina.Container"));
      mapper.addRule("Server/Service/Engine/Host/Alias", mapper.methodSetter("addAlias",0));

      // Setup the mapping rules common to both Engine and Host
      String engineOrHost[] = {"Server/Service/Engine", "Server/Service/Engine/Host"};
      for(int i=0;i<engineOrHost.length;i++)
      {
         String prefix=engineOrHost[i];
         // Logger rules
         mapper.addRule(prefix+"/Logger",
            mapper.objectCreate("org.jboss.web.catalina.Log4jLogger", "className"));
         mapper.addRule(prefix+"/Logger", mapper.setProperties());
         mapper.addRule(prefix+"/Logger", mapper.setParent("setContainer","org.apache.catalina.Container"));
         mapper.addRule(prefix+"/Logger", mapper.addChild("setLogger", "org.apache.catalina.Logger"));

         // DefaultContext rules
         mapper.addRule(prefix+"/DefaultContext",
            mapper.objectCreate("org.apache.catalina.core.DefaultContext", "className"));
         mapper.addRule(prefix+"/DefaultContext", mapper.setProperties());
         mapper.addRule(prefix+"/DefaultContext",
            mapper.setParent("setParent", "org.apache.catalina.Container"));
         mapper.addRule(prefix+"/DefaultContext",
            mapper.addChild("addDefaultContext", "org.apache.catalina.core.DefaultContext"));
         // add container default context various listeners
         mapper.addRule(prefix + "/DefaultContext/WrapperLifecycle",
            mapper.methodSetter("addWrapperLifecycle", 0));
         mapper.addRule(prefix + "/DefaultContext/InstanceListener",
            mapper.methodSetter("addInstanceListener", 0));
         mapper.addRule(prefix + "/DefaultContext/WrapperListener",
            mapper.methodSetter("addWrapperListener", 0));
         // Allow the context manager to be overriden
         mapper.addRule(prefix + "/DefaultContext/Manager",
            mapper.objectCreate("org.apache.catalina.session.StandardManager", "className"));
         mapper.addRule(prefix + "/DefaultContext/Manager", mapper.setProperties());
         mapper.addRule(prefix + "/DefaultContext/Manager",
            mapper.addChild("setManager", "org.apache.catalina.Manager"));

         // add container valve
         mapper.addRule(prefix+"/Valve", mapper.objectCreate(null, "className"));
         mapper.addRule(prefix+"/Valve", mapper.setProperties());
         mapper.addRule(prefix+"/Valve",
            mapper.setParent("setContainer", "org.apache.catalina.Container"));
         mapper.addRule(prefix+"/Valve",
            mapper.addChild("addValve", "org.apache.catalina.Valve"));
      }

      // Add the Listener rules to Engine, Engine/Host, and Connector
      String lifeCycling[] = {"Server/Service/Engine", "Server/Service/Engine/Host", "Server/Service/Connector"};
      for(int i = 0; i < lifeCycling.length; i ++)
      {
         String prefix = lifeCycling[i];
         mapper.addRule(prefix + "/Listener", mapper.objectCreate(null, "className"));
         mapper.addRule(prefix + "/Listener", mapper.setProperties());
         mapper.addRule(prefix + "/Listener", mapper.addChild("addLifecycleListener",
            "org.apache.catalina.LifecycleListener"));
      }

      /* Push the Embedded onto the config object stack and transform the DOM
       Config element into a SAX event stream so that the XmlMapper parses the
       configuration
      */
      Stack st = mapper.getObjectStack();
      st.push(embedded);
      try
      {
         TransformerFactory tFactory = TransformerFactory.newInstance();
         Transformer transformer = tFactory.newTransformer();
         transformer.transform(new DOMSource(config), new SAXResult(mapper));
      }
      catch(TransformerException e)
      {
         Throwable t = e.getException();
         throw e;
      }
   }

}
