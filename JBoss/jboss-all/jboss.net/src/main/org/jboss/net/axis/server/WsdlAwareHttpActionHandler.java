/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: WsdlAwareHttpActionHandler.java,v 1.1.2.1 2003/03/28 12:50:46 cgjung Exp $


package org.jboss.net.axis.server;

import org.apache.axis.AxisFault;
import org.apache.axis.MessageContext;
import org.apache.axis.handlers.http.HTTPActionHandler;

/**
 * <p>
 * This HttpActionHandler will influence the wsdl-generation of
 * action aware service providers, such as {@link org.jboss.net.axis.server.EJBProvider}.
 * </p>
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @since 27.03.2003
 * @version $Revision: 1.1.2.1 $
 */

public class WsdlAwareHttpActionHandler extends HTTPActionHandler {

	/**
	 * sets the presence flag in the context such that any provider
	 * that uses this flag will be able to generate soap-action 
	 * information.
	 * @see org.apache.axis.Handler#generateWSDL(org.apache.axis.MessageContext)
	 */
	public void generateWSDL(MessageContext arg0) throws AxisFault {
		arg0.setProperty(Constants.ACTION_HANDLER_PRESENT_PROPERTY,Boolean.TRUE);
		super.generateWSDL(arg0);
	}

}
