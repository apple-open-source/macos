/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.util;

import org.jboss.jmx.interfaces.RMINotificationListener;
import org.jboss.logging.Logger;

import javax.management.Notification;

import java.util.Iterator;
import java.util.List;
import java.util.LinkedList;

import java.rmi.RemoteException;
import java.rmi.server.UnicastRemoteObject;

/**
 * @author Stacy Curl
 */
public class MirrorServiceRemoteListener
    extends UnicastRemoteObject
    implements RMINotificationListener
{
    /**
     * @param    mirroringServiceRemoteInterface
     */
    public MirrorServiceRemoteListener(MirroringServiceRemoteInterface mirroringServiceRemoteInterface)
        throws RemoteException
    {
        m_mirroringServiceRemoteInterface = mirroringServiceRemoteInterface;
    }

    /**
     * @param    notification
     * @param    handback
     * @throws RemoteException
     */
    public void handleNotification(Notification notification, Object handback)
        throws RemoteException
    {
        LOG.debug("handleNotification:" + notification + "(" + notification.getTimeStamp() + ")");
        m_mirroringServiceRemoteInterface.handleRemoteNotification(notification, handback);
        LOG.debug("handleNotification:" + notification + "(" + notification.getTimeStamp()
                  + ") - done");
    }

    /** */
    private MirroringServiceRemoteInterface m_mirroringServiceRemoteInterface;

    private static final Logger LOG = Logger.getLogger(MirrorServiceRemoteListener.class.getName());
}
