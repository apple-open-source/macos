/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.mq.server;

import java.util.HashMap;
import java.util.Iterator;

import javax.jms.Destination;
import javax.jms.JMSException;
import javax.jms.Queue;
import javax.jms.TemporaryQueue;
import javax.jms.TemporaryTopic;
import javax.jms.Topic;

import org.jboss.logging.Logger;
import org.jboss.mq.AcknowledgementRequest;
import org.jboss.mq.ConnectionToken;
import org.jboss.mq.DurableSubscriptionID;
import org.jboss.mq.SpyDestination;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.Subscription;
import org.jboss.mq.TransactionRequest;
import org.jboss.mq.il.jvm.JVMClientIL;
/**
 * A pass through Interceptor, which keeps track of when a
 * client was last active.  If a client is inactive for too long,
 * then it is disconnected from the server.
 *
 * @author <a href="mailto:hchirino@jboss.org">Hiram Chirino</a>
 */
public class ClientMonitorInterceptor extends JMSServerInterceptorSupport {

	static protected Logger log =
		Logger.getLogger(ClientMonitorInterceptor.class);


	//The list of ClientConsumers hased by ConnectionTokens
	HashMap clients = new HashMap();

	private static class ClientStats {
		private long lastUsed = System.currentTimeMillis();
		boolean disconnectIfInactive=true;
	}

	public void disconnectInactiveClients(long disconnectTime) {
		log.debug("Checking for timedout clients.");
		Iterator i = clients.keySet().iterator();
		while (i.hasNext()) {
			ConnectionToken dc = (ConnectionToken) i.next();
			ClientStats cs = (ClientStats) clients.get(dc);
			if( cs.disconnectIfInactive  && cs.lastUsed < disconnectTime ) {
				try {
					log.debug("Disconnecting client due to inactivity timeout: "+dc);
					connectionClosing(dc);
				} catch (Throwable e ) {
				}
			}
		}
	}

	public ClientStats getClientStats(ConnectionToken dc) throws JMSException {
		ClientStats cq = (ClientStats) clients.get(dc);
		if (cq == null) {
			cq = new ClientStats();
			
			// The JVM clientil does not ping.
			if( dc.clientIL instanceof JVMClientIL )
				cq.disconnectIfInactive = false;
				 
			synchronized(clients) {
				HashMap m = new HashMap(clients);
				m.put(dc, cq);
				clients=m;
			}
		}
		return cq;
	}

	public TemporaryTopic getTemporaryTopic(ConnectionToken dc)
		throws JMSException {
		getClientStats(dc).lastUsed = System.currentTimeMillis();
		return getNext().getTemporaryTopic(dc);
	}

	/**
	 * Gets the TemporaryQueue attribute of the ServerIL object
	 *
	 * @param dc             Description of Parameter
	 * @return               The TemporaryQueue value
	 * @exception JMSException  Description of Exception
	 */
	public TemporaryQueue getTemporaryQueue(ConnectionToken dc)
		throws JMSException {
		getClientStats(dc).lastUsed = System.currentTimeMillis();
		return getNext().getTemporaryQueue(dc);
	}

	/**
	 * #Description of the Method
	 *
	 * @param dc             Description of Parameter
	 * @exception JMSException  Description of Exception
	 */
	public void connectionClosing(ConnectionToken dc) throws JMSException {
		synchronized (clients) {
			HashMap m = new HashMap(clients);
			m.remove(dc);
			clients = m;
		}
		getNext().connectionClosing(dc);
	}

	/**
	 * Add the message to the destination.
	 *
	 * @param dc             The feature to be added to the Message attribute
	 * @param message        The feature to be added to the Message attribute
	 * @exception JMSException  Description of Exception
	 */
	public void addMessage(ConnectionToken dc, SpyMessage message)
		throws JMSException {
		getClientStats(dc).lastUsed = System.currentTimeMillis();
		getNext().addMessage(dc, message);
	}

	/**
	 * #Description of the Method
	 *
	 * @param dc             Description of Parameter
	 * @param dest           Description of Parameter
	 * @return               Description of the Returned Value
	 * @exception JMSException  Description of Exception
	 */
	public Queue createQueue(ConnectionToken dc, String dest)
		throws JMSException {
		getClientStats(dc).lastUsed = System.currentTimeMillis();
		return getNext().createQueue(dc, dest);

	}

	/**
	 * #Description of the Method
	 *
	 * @param dc             Description of Parameter
	 * @param dest           Description of Parameter
	 * @return               Description of the Returned Value
	 * @exception JMSException  Description of Exception
	 */
	public Topic createTopic(ConnectionToken dc, String dest)
		throws JMSException {
		getClientStats(dc).lastUsed = System.currentTimeMillis();
		return getNext().createTopic(dc, dest);
	}

	/**
	 * #Description of the Method
	 *
	 * @param dc             Description of Parameter
	 * @param dest           Description of Parameter
	 * @exception JMSException  Description of Exception
	 */
	public void deleteTemporaryDestination(
		ConnectionToken dc,
		SpyDestination dest)
		throws JMSException {
		getClientStats(dc).lastUsed = System.currentTimeMillis();
		getNext().deleteTemporaryDestination(dc, dest);
	}

	/**
	 * #Description of the Method
	 *
	 * @param dc             Description of Parameter
	 * @param t              Description of Parameter
	 * @exception JMSException  Description of Exception
	 */
	public void transact(ConnectionToken dc, TransactionRequest t)
		throws JMSException {
		getClientStats(dc).lastUsed = System.currentTimeMillis();
		getNext().transact(dc, t);

	}

	/**
	 * #Description of the Method
	 *
	 * @param dc             Description of Parameter
	 * @param item           Description of Parameter
	 * @exception JMSException  Description of Exception
	 */
	public void acknowledge(ConnectionToken dc, AcknowledgementRequest item)
		throws JMSException {
		getClientStats(dc).lastUsed = System.currentTimeMillis();
		getNext().acknowledge(dc, item);
	}

	/**
	 * #Description of the Method
	 *
	 * @param dc             Description of Parameter
	 * @param dest           Description of Parameter
	 * @param selector       Description of Parameter
	 * @return               Description of the Returned Value
	 * @exception JMSException  Description of Exception
	 */
	public SpyMessage[] browse(
		ConnectionToken dc,
		Destination dest,
		String selector)
		throws JMSException {
		getClientStats(dc).lastUsed = System.currentTimeMillis();
		return getNext().browse(dc, dest, selector);
	}

	/**
	 * #Description of the Method
	 *
	 * @param dc             Description of Parameter
	 * @param subscriberId   Description of Parameter
	 * @param wait           Description of Parameter
	 * @return               Description of the Returned Value
	 * @exception JMSException  Description of Exception
	 */
	public SpyMessage receive(ConnectionToken dc, int subscriberId, long wait)
		throws JMSException {
		getClientStats(dc).lastUsed = System.currentTimeMillis();
		return getNext().receive(dc, subscriberId, wait);
	}

	/**
	 * Sets the Enabled attribute of the ServerIL object
	 *
	 * @param dc             The new Enabled value
	 * @param enabled        The new Enabled value
	 * @exception JMSException  Description of Exception
	 */
	public void setEnabled(ConnectionToken dc, boolean enabled)
		throws JMSException {
		getClientStats(dc).lastUsed = System.currentTimeMillis();
		getNext().setEnabled(dc, enabled);
	}

	/**
	 * #Description of the Method
	 *
	 * @param dc              Description of Parameter
	 * @param subscriptionId  Description of Parameter
	 * @exception JMSException   Description of Exception
	 */
	public void unsubscribe(ConnectionToken dc, int subscriptionId)
		throws JMSException {
		getClientStats(dc).lastUsed = System.currentTimeMillis();
		getNext().unsubscribe(dc, subscriptionId);
	}

	/**
	 * #Description of the Method
	 *
	 * @param id             Description of Parameter
	 * @exception JMSException  Description of Exception
	 */
	public void destroySubscription(
		ConnectionToken dc,
		DurableSubscriptionID id)
		throws JMSException {
		getClientStats(dc).lastUsed = System.currentTimeMillis();
		getNext().destroySubscription(dc, id);
	}

	/**
	 * @param dc                       org.jboss.mq.ConnectionToken
	 * @param s                        org.jboss.mq.Subscription
	 * @exception JMSException  The exception description.
	 */
	public void subscribe(
		org.jboss.mq.ConnectionToken dc,
		org.jboss.mq.Subscription s)
		throws JMSException {

		getClientStats(dc).lastUsed = System.currentTimeMillis();
		getNext().subscribe(dc, s);

	}

	/**
	 * #Description of the Method
	 *
	 * @param dc             Description of Parameter
	 * @param clientTime     Description of Parameter
	 * @exception JMSException  Description of Exception
	 */
	public void ping(ConnectionToken dc, long clientTime) throws JMSException {
		getClientStats(dc).lastUsed = System.currentTimeMillis();
		getNext().ping(dc, clientTime);
	}

	public Subscription getSubscription(ConnectionToken dc, int subscriberId)
		throws JMSException {
		getClientStats(dc).lastUsed = System.currentTimeMillis();
		return getNext().getSubscription(dc, subscriberId);

	}
}
