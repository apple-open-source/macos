/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.mq.server;

import java.util.Iterator;

import javax.jms.InvalidClientIDException;
import javax.jms.JMSException;

import org.jboss.logging.Logger;
import org.jboss.mq.ConnectionToken;
/**
 * The JMS spec does not let a second client with the same clientID connect to
 * the server.  The second client gets an InvalidClientIDException exception.
 * This interceptor modifies the server so that the first client gets
 * disconnected and then the second client can connect successfully.
 * 
 * Currently it only works if the client id is set by the client using the
 * setClient method call.
 *
 * @author <a href="mailto:hchirino@jboss.org">Hiram Chirino</a>
 */
public class ClientReconnectInterceptor extends JMSServerInterceptorSupport {

	static protected Logger log =
		Logger.getLogger(ClientReconnectInterceptor.class);

	private JMSDestinationManager destinationManager;
	private JMSDestinationManager getDestinationManager() {
		if(destinationManager!=null)
			return destinationManager;
			
		JMSServerInterceptor interceptor = getNext(); 
		while( ! (interceptor instanceof JMSDestinationManager) ) 
			interceptor = getNext();
		destinationManager = (JMSDestinationManager)interceptor;
		return destinationManager;
	}
	
	private ConnectionToken findConnectionTokenFor(String clientID) {
		Iterator iterator = getDestinationManager().clientConsumers.keySet().iterator();
		while (iterator.hasNext()) {
			ConnectionToken dc = (ConnectionToken) iterator.next();
			if( dc.getClientID().equals(clientID))
				return dc;
		}
		return null;
	}

	/**
	 * @see org.jboss.mq.server.JMSServerInterceptor#checkID(java.lang.String)
	 */
	public void checkID(String ID) throws JMSException {
		try {
			super.checkID(ID);
		} catch (InvalidClientIDException e) {
			ConnectionToken dc = findConnectionTokenFor(ID);
			// The InvalidClientIDException could have been thrown
			// due to another reason besides a double client connect.
			if( dc == null ) 
				throw e;
				
			// try to disconnect the previous client
			try {
				super.connectionClosing(dc);
			} catch (Throwable e2) {
				// Log it in case we ever care to find out the details.
				log.trace("Disconnect of previously connected client caused an error:",e);
			}
			
			// try it again...
			super.checkID(ID);				
		}
	}
}
