/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.util;

import java.rmi.Remote;
import java.rmi.RemoteException;

import javax.management.Notification;

/**
 * @author Stacy Curl
 */
public interface MirroringServiceRemoteInterface
    extends Remote
{
    /**
     * @param    notification
     * @param    handback
     * @throws RemoteException
     */
    public void handleRemoteNotification(Notification notification, Object handback)
        throws RemoteException;
}
