/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.jmx.connector.notification;

import java.io.Serializable;
import java.rmi.Remote;
import java.rmi.RemoteException;

import javax.management.Notification;

/**
* This Interface defines the methods in the RMI Stub
* transferred to the server-side.
*
* @author <A href="mailto:andreas@jboss.org">Andreas &quot;Mad&quot; Schaefer</A>
**/
public interface RMIClientNotificationListenerInterface
	extends Remote, Serializable
{

	// Constants -----------------------------------------------------

	// Static --------------------------------------------------------

	// Public --------------------------------------------------------
	/**
	* Handles the given notifcation event and passed it to the registered
	* listener
	*
	* @param pNotification				NotificationEvent
	* @param pHandback					Handback object
	*
	* @throws RemoteException			If a Remote Exception occurred
	*/
	public void handleNotification(
		Notification pNotification,
		Object pHandback
	) throws 
		RemoteException;
}
