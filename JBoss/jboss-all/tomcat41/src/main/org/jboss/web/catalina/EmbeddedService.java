package org.jboss.web.catalina;

import org.apache.catalina.*;
import org.apache.catalina.startup.Embedded;

/** A Service implementation that delegates its

 TOMCAT 4.1.12 UPDATE: Added getServer() and setServer(Server server) methods to comply
 with new Service interface, and
 renamed setServer(EmbeddedCatalina catalina) to setEmbeddedServer(EmbeddedCatalina catalina) to
 avoid confusion

@author Scott.Stark@jboss.org
*/
public class EmbeddedService implements Service
{
   private EmbeddedCatalina catalina;
   private Server server;
   private String name = "Default";

   /** Creates a new instance of EmbeddedService */
   public EmbeddedService()
   {
   }

   public void addConnector(Connector connector)
   {
      catalina.addConnector(connector);
   }
   public Connector[] findConnectors()
   {
      return catalina.findConnectors();
   }
   public void removeConnector(Connector connector)
   {
      catalina.removeConnector(connector);
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

   public void setEmbeddedServer(EmbeddedCatalina catalina)
   {
      this.catalina = catalina;
   }

   public void setServer(Server server)
   {
      this.server = server;
   }

    public Server getServer() {
        return server;
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
