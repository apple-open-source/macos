/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.net.axis.server;

import org.apache.axis.transport.http.AxisServlet;
import org.apache.axis.server.AxisServer;
import org.apache.axis.AxisFault;

/**
 * An AxisServlet that is able to extract the corresponding AxisEngine 
 * from its installation context and builds the right message contexts for the
 * JBoss classloading and deployment architecture.
 *
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @created 7. September 2001, 19:17
 * @version $Revision: 1.2.4.3 $
 */
public class AxisServiceServlet extends AxisServlet
{

   /** reference to the server */
   protected AxisServer server = null;

   /** Creates new AxisServlet */
   public AxisServiceServlet()
   {
   }


	/** override AxisServlet.getEngine() in order to redirect to
	*  the corresponding AxisEngine.
	*/
	public AxisServer getEngine() throws AxisFault {
		if (server == null) {
			// we need to extract the engine from the 
			// rootcontext
			String installation =
				getInitParameter(
					org.jboss.net.axis.Constants.CONFIGURATION_CONTEXT);
			// call the static service method to find the installed engine
			try {
				server =
					JMXEngineConfigurationFactory
						.newJMXFactory(installation)
						.getAxisServer();
			} catch (NullPointerException e) {
				throw new AxisFault(
					"Could not access JMX configuration factory.",
					e);
			}
		}
		return server;
	}

}