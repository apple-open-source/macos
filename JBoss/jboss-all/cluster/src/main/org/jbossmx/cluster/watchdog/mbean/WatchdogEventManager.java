/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean;

import java.rmi.server.UnicastRemoteObject;
import java.rmi.RemoteException;

import javax.management.MBeanRegistration;
import javax.management.MBeanServer;
import javax.management.Notification;
import javax.management.NotificationListener;
import javax.management.ObjectName;

import com.sun.management.jmx.ServiceName;

/**
 * @author Stacy Curl
 */
public class WatchdogEventManager
    extends UnicastRemoteObject
    implements WatchdogEventManagerMBean, WatchdogEventManagerRemoteInterface
{
    /**
     *
     * @throws RemoteException
     */
    public WatchdogEventManager() throws RemoteException
    {
        try
        {
            m_watchdogEventDispatcherSupport = new WatchdogEventDispatcherSupport();
        }
        catch (Throwable t)
        {
            t.printStackTrace();
        }
    }

    /**
     * @param    recipientClass
     * @param    initialisation
     *
     * @throws RemoteException
     */
    public WatchdogEventManager(String recipientClass, String initialisation) throws RemoteException
    {
        this();

        addExternelEventRecipient(recipientClass, initialisation);
    }

    /**
     * @param    recipientClass
     * @param    initialisation
     *
     * @return
     */
    public boolean addExternelEventRecipient(String recipientClass, String initialisation)
    {
        return m_watchdogEventDispatcherSupport.addListener(recipientClass, initialisation);
    }

    /**
     *
     * @return
     */
    public WatchdogEventManagerRemoteInterface getRemoteInterface()
    {
        return this;
    }

    /**
     * @param    watchdogEvent
     * @throws RemoteException
     */
    public void publishEvent(WatchdogEvent watchdogEvent) throws RemoteException
    {
        m_watchdogEventDispatcherSupport.dispatchEvent(watchdogEvent);
    }

    /** */
    private WatchdogEventDispatcherSupport m_watchdogEventDispatcherSupport;
}
