package org.jboss.web.catalina;

import org.apache.catalina.Connector;
import org.apache.catalina.Container;
import org.apache.catalina.Service;
import org.apache.catalina.LifecycleException;

/** A Service implementation that delegates its 

@author Scott.Stark@jboss.org
*/
public class EmbeddedService implements Service
{
   private EmbeddedCatalina server;
   private String name = "Default";

   /** Creates a new instance of EmbeddedService */
   public EmbeddedService()
   {
   }

   public void addConnector(Connector connector)
   {
      server.addConnector(connector);
   }
   public Connector[] findConnectors()
   {
      return server.findConnectors();
   }
   public void removeConnector(Connector connector)
   {
      server.removeConnector(connector);
   }

   public Container getContainer()
   {
      return null;
   }
   public void setContainer(Container container)
   {
   }

   public String getInfo()
   {
      return "";
   }

   public String getName()
   {
      return name;
   }
   public void setName(String name)
   {
      this.name = name;
   }

   public void setServer(EmbeddedCatalina server)
   {
      this.server = server;
   }

   public void initialize() throws LifecycleException
   {
   }

   public String toString()
   {
      StringBuffer sb = new StringBuffer("EmbeddedService[");
      sb.append(getName());
      sb.append("]");
      return sb.toString();
   }

}
