/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean;

import java.rmi.Remote;
import java.rmi.RemoteException;

/**
 *
 */
public interface WatchdogEventManagerRemoteInterface
    extends Remote
{
    /**
     * @param    watchdogEvent
     * @throws RemoteException
     */
    public void publishEvent(WatchdogEvent watchdogEvent) throws RemoteException;
}
