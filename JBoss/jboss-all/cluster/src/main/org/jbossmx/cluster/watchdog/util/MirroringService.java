/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.util;


import org.jboss.logging.Logger;

import org.jboss.jmx.connector.notification.RMINotificationListener;
import org.jboss.jmx.adaptor.rmi.RMIAdaptor;

import javax.management.InstanceNotFoundException;
import javax.management.ListenerNotFoundException;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanRegistration;
import javax.management.MBeanRegistrationException;
import javax.management.MBeanServer;
import javax.management.MBeanServerNotification;
import javax.management.Notification;
import javax.management.NotificationListener;
import javax.management.ObjectInstance;
import javax.management.ObjectName;
import javax.management.QueryExp;

// Standard Java Classes
import java.io.Serializable;

import java.rmi.RemoteException;
import java.rmi.server.UnicastRemoteObject;

import java.util.Iterator;
import java.util.HashSet;
import java.util.Set;

/**
 * @author Stacy Curl
 */
public class MirroringService
    extends UnicastRemoteObject
    implements MBeanRegistration, MirroringServiceMBean, MirroringServiceRemoteInterface,
               NotificationListener, Serializable
{
    /**
     * @param    remoteMBeanServer
     */
    public MirroringService(RMIAdaptor remoteMBeanServer, ObjectName pattern, QueryExp query)
        throws RemoteException
    {
        m_remoteMBeanServer = remoteMBeanServer;
        m_mirroringState = false;
        m_pattern = pattern;
        m_query = query;
        m_RMINotificationListener = new MirrorServiceRemoteListener(this);
//        m_pendingNotifications = new LinkedList();
    }

    /**
     */
    public void postDeregister() {}

    /**
     * @param    p0
     */
    public void postRegister(Boolean p0) {}

    /**
     * @throws Exception
     */
    public void preDeregister() throws Exception {}

    /**
     * @param    mbeanServer
     * @param    objectName
     *
     * @return
     * @throws Exception
     */
    public ObjectName preRegister(MBeanServer mbeanServer, ObjectName objectName) throws Exception
    {
        m_mbeanServer = mbeanServer;

        return objectName;
    }

    /**
     *
     * @return
     */
    public boolean startMirroring()
    {
        LOG.debug("startMirroring");

        try
        {
            LOG.debug("startMirroring.addNotificationListener = " + m_RMINotificationListener);

//            startHandlingPendingNotifications();

            m_remoteMBeanServer.addNotificationListener(m_sDelegateObjectName,
                                                        m_RMINotificationListener, null, null);
            m_mbeanServer.addNotificationListener(m_sDelegateObjectName, this, null, "local");

            loadMirroredMBeans();

            return true;
        }
        catch(InstanceNotFoundException infe)
        {
            LOG.error("startMirroring");
            LOG.error(infe);
        }
        catch(RemoteException re)
        {
            LOG.error("startMirroring");
            LOG.error(re);
        }

        return false;
    }

    /**
     *
     * @return
     */
    public boolean stopMirroring()
    {
        LOG.debug("stopMirroring");

        try
        {
            LOG.debug("stopMirroring.removeNotificationListener = " + m_RMINotificationListener);

            unloadMirroredMBeans();

            m_remoteMBeanServer.removeNotificationListener(m_sDelegateObjectName,
                m_RMINotificationListener);
            m_mbeanServer.removeNotificationListener(m_sDelegateObjectName, this);

//            stopHandlingPendingNotifications();

            return true;
        }
        catch(InstanceNotFoundException infe)
        {
            LOG.error("stopMirroring");
            LOG.error(infe);
        }
        catch(ListenerNotFoundException lnfe)
        {
            LOG.error("stopMirroring");
            LOG.error(lnfe);
        }
        catch(RemoteException re)
        {
            LOG.error("stopMirroring");
            LOG.error(re);
        }

        return false;
    }

    /**
     * @param    notification
     * @param    handback
     */
    public void handleNotification(Notification notification, Object handback)
    {
        final String notificationType = notification.getType();

        LOG.debug("handleNotification:" + notification + "(" + notification.getTimeStamp()
                  + ") " + notificationType);

        try
        {
            if ((notification instanceof MBeanServerNotification) &&
                "JMX.mbean.unregistered".equals(notificationType))
            {
                final ObjectName objectName = ((MBeanServerNotification) notification).getMBeanName();

                if (m_mirroredMBeans.contains(objectName))
                {
                    LOG.debug("handleNotification.removing " + objectName);

                    m_mirroredMBeans.remove(objectName);

                    m_remoteMBeanServer.unregisterMBean(objectName);
                }
            }
        }
        catch (InstanceNotFoundException infe) {}
        catch (MBeanRegistrationException mre) {}
        catch (RemoteException re) {}

        LOG.debug("handleNotification:" + notification + "(" + notification.getTimeStamp()
                  + ") " + notificationType + " - done");
    }

    /**
     * @param    notification
     * @param    handback
     * @throws RemoteException
     */
    public void handleRemoteNotification(Notification notification, Object handback)
        throws RemoteException
    {
        handleRemoteNotificationImpl(notification, handback);
    }

    private void handleRemoteNotificationImpl(Notification notification, Object handback)
    {
        final String notificationType = notification.getType();

        LOG.debug("handleRemoteNotificationImpl:" + notification + "(" + notification.getTimeStamp()
                  + ") " + notificationType);

        try
        {
            if((notification instanceof MBeanServerNotification)
                && ("JMX.mbean.registered".equals(notificationType)
                    || "JMX.mbean.unregistered".equals(notificationType)))
            {
//                LOG.debug("handleRemoteNotificationImpl.m_remoteMBeanServer.queryNames");
//                Set set = m_remoteMBeanServer.queryNames(m_pattern, m_query);
//                LOG.debug("handleRemoteNotificationImpl.m_remoteMBeanServer.queryNames - done");

                ObjectName objectName = ((MBeanServerNotification) notification).getMBeanName();

                printParticipants(objectName, m_mirroredMBeans);

                //if(m_mirroredMBeans.contains(objectName))
                if (matchingObjectName(objectName))
                {
                    final boolean haveCopyAlready = m_mirroredMBeans.contains(objectName);

                    if("JMX.mbean.registered".equals(notificationType) && !haveCopyAlready)
                    {
                        registerMirroredMBean(objectName);
                    }
                    else if (haveCopyAlready)
                    {
                        unregisterMirroredMBean(objectName);
                    }
                }
            }
        }
        catch (Exception e) {}
//        catch (RemoteException re) {}

        LOG.debug("handleRemoteNotificationImpl:" + notification + "(" + notification.getTimeStamp()
                  + ") - done");

    }

    private boolean matchingObjectName(final ObjectName objectName)
    {
        return objectName.getDomain().equals(m_pattern.getDomain());
    }

    private void printParticipants(final ObjectName objectName, final Set set)
    {
        LOG.debug("printParticipants.objectName = " + objectName);

        for (Iterator iterator = set.iterator(); iterator.hasNext();)
        {
            final ObjectName sObjectName = (ObjectName) iterator.next();

            LOG.debug("printParticipants.sObjectName = " + sObjectName);
        }
    }

    private void loadMirroredMBeans()
    {
        try
        {
            Set set = m_remoteMBeanServer.queryNames(m_pattern, m_query);

            if (set != null)
            {
                for (Iterator iterator = set.iterator(); iterator.hasNext();)
                {
                    registerMirroredMBean((ObjectName) iterator.next());
                }
            }
        }
        catch (RemoteException re)
        {
        }
    }

    private void unloadMirroredMBeans()
    {
        synchronized (m_mirroredMBeans)
        {
            for (Iterator iterator = m_mirroredMBeans.iterator(); iterator.hasNext();)
            {
                ObjectName objectName = (ObjectName) iterator.next();

                try
                {
                    m_mbeanServer.unregisterMBean(objectName);
                }
                catch (Exception e)
                {
                }
            }
        }
    }

    private void registerMirroredMBean(ObjectName objectName)
    {
        LOG.debug("registerMirroredMBean = " + objectName);

        synchronized (m_mirroredMBeans)
        {
            try
            {
                LOG.debug("registerMirroredMBean.1");
                m_mirroredMBeans.add(objectName);
                LOG.debug("registerMirroredMBean.2");
                MBeanMirror mbeanMirror = new MBeanMirror(m_remoteMBeanServer, objectName);
                m_mbeanServer.registerMBean(mbeanMirror, objectName);
                LOG.debug("registerMirroredMBean.3");
            }
            catch (Exception e)
            {
                try
                {
                    m_mirroredMBeans.remove(objectName);
                } catch (Exception e2) {}
            }
        }

        LOG.debug("registerMirroredMBean = " + objectName + " - done");
    }

    private void unregisterMirroredMBean(ObjectName objectName)
    {
        LOG.debug("unregisterMirroredMBean = " + objectName);

        synchronized (m_mirroredMBeans)
        {
            try
            {
                LOG.debug("unregisterMirroredMBean.1");
                m_mirroredMBeans.remove(objectName);
                LOG.debug("unregisterMirroredMBean.2");
                m_mbeanServer.unregisterMBean(objectName);
                LOG.debug("unregisterMirroredMBean.3");
            }
            catch (Exception e)
            {
                try
                {
                    m_mirroredMBeans.add(objectName);
                } catch (Exception e2) {}
            }
        }

        LOG.debug("unregisterMirroredMBean = " + objectName + " - done");
    }

    /** */
    private MBeanServer m_mbeanServer;
    /** */
    private RMIAdaptor m_remoteMBeanServer;
    /** */
    private ObjectName m_pattern;
    /** */
    private QueryExp m_query;
    /** */
    private RMINotificationListener m_RMINotificationListener;
    /** */
    private boolean m_mirroringState;

    /** */
    private static ObjectName m_sDelegateObjectName;

    static
    {
        try
        {
            m_sDelegateObjectName = new ObjectName(com.sun.management.jmx.ServiceName.DELEGATE);
        }
        catch(MalformedObjectNameException mone) {}
    }

    /** */
    private Set m_mirroredMBeans = new HashSet();

    private static final Logger LOG = Logger.getLogger(MirroringService.class.getName());
}
