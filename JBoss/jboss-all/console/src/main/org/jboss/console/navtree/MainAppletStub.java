package org.jboss.console.navtree;

import java.net.URL;
import java.applet.AppletStub;
import java.applet.AppletContext;
import java.util.Properties;

/** A simple AppletStub to allow for testing the applet as a java application
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class MainAppletStub implements AppletStub
{
   private URL docBase;
   private URL codeBase;
   private Properties params = new Properties(System.getProperties());

   public MainAppletStub() throws Exception
   {
      docBase = new URL("http://localhost:8080/web-console/");
      codeBase = new URL("http://localhost:8080/web-console/");
      params.setProperty("RefreshTime", "5");
      params.setProperty("PMJMXName", "jboss.admin:service=PluginManager");
   }

   public boolean isActive()
   {
      return true;
   }

   public String getParameter(String name)
   {
      return System.getProperty(name);
   }

   public AppletContext getAppletContext()
   {
      return null;
   }

   public void appletResize(int width, int height)
   {
   }

   public URL getDocumentBase()
   {
      return docBase;
   }
   public URL getCodeBase()
   {
      return codeBase;
   }
}
