/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.net.axis.server;

import org.apache.axis.AxisFault;
import org.apache.axis.transport.http.AdminServlet;
import org.apache.axis.server.AxisServer;

/**
 * slightly patched admin servlet to contact the right server
 * @created 9.9.2002
 * @author jung
 * @version $Revision: 1.1.2.2 $
 */

public class AxisAdminServlet extends AdminServlet {

   /** reference to the server */
   protected AxisServer server=null;
   
   /** Creates new AxisServlet */
   public AxisAdminServlet() {
   }

   /** override AxisServlet.getEngine() in order to redirect to
    *  the corresponding AxisEngine.
    */
   public AxisServer getEngine() throws AxisFault {
      if (server==null) {
         // we need to extract the engine from the 
         // rootcontext
         String installation = getInitParameter(org.jboss.net.axis.Constants.CONFIGURATION_CONTEXT);
         // call the static service method to find the installed engine
         try{
         	server=JMXEngineConfigurationFactory.newJMXFactory(installation).getAxisServer();
         } catch(NullPointerException e) {
         	throw new AxisFault("Could not access JMX configuration factory.",e);
         }
      }

      return server;
   }

}
