/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean;

import org.jboss.logging.Logger;

import org.jboss.jmx.adaptor.rmi.RMIAdaptor;

import org.jbossmx.cluster.watchdog.Configuration;
import org.jbossmx.cluster.watchdog.HermesException;

import org.jbossmx.cluster.watchdog.SwapMachinesRemoteInterface;

import org.jbossmx.cluster.watchdog.agent.AgentRemoteInterface;

import org.jbossmx.cluster.watchdog.util.HermesMachineProperties;
import org.jbossmx.cluster.watchdog.util.MirroringService;
import org.jbossmx.cluster.watchdog.util.MirroringServiceMBean;

// 3rd Party Packages
import com.sun.management.jmx.ServiceName;

import javax.management.InstanceAlreadyExistsException;
import javax.management.InstanceNotFoundException;
import javax.management.JMException;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanException;
import javax.management.MBeanRegistration;
import javax.management.MBeanRegistrationException;
import javax.management.MBeanServer;
import javax.management.MBeanServerNotification;
import javax.management.NotCompliantMBeanException;
import javax.management.Notification;
import javax.management.NotificationListener;
import javax.management.ObjectInstance;
import javax.management.ObjectName;
import javax.management.ReflectionException;

import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;

// Standard Java Packages
import java.util.Collections;
import java.util.Map;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

import java.rmi.*;

import java.net.*;

/**
 * The Watchdog class monitors all the StartableMBean MBeans on a JMX Agent.
 *
 * @author Stacy Curl
 */
public class Watchdog
    extends Startable
    implements WatchdogMBean, MBeanRegistration, NotificationListener
{
    /**
     * Constructor for Watchdog
     *
     * @param    unresolvedWatchedRmiAgentBinding
     * @param    watchedDomain
     * @param    myAgentRmiBinding
     *
     * @throws JMException
     * @throws MalformedURLException
     * @throws NotBoundException
     * @throws RemoteException
     */
    public Watchdog(
        String unresolvedWatchedRmiAgentBinding, String watchedDomain, String myAgentRmiBinding)
            throws JMException, RemoteException, MalformedURLException, NotBoundException
    {
        try
        {
            m_unresolvedRmiAgentBinding = unresolvedWatchedRmiAgentBinding;

            String resolvedRmiAgentBinding = HermesMachineProperties
                .getResolvedRmiAgentBinding(unresolvedWatchedRmiAgentBinding);

            setRmiAgentBinding(resolvedRmiAgentBinding);
            setWatchedDomainObjectName(new ObjectName(watchedDomain));

            setAgentRemoteInterface((AgentRemoteInterface) Naming.lookup(getRmiAgentBinding()));

            m_myAgentRmiBinding = myAgentRmiBinding;

            setNumTimesToAttemptMBeanRestart(DEFAULT_NUM_TIMES_TO_ATTEMPT_MBEAN_RESTART);
            setNumTimesToAttemptMBeanReregister(DEFAULT_NUM_TIMES_TO_ATTEMPT_MBEAN_REREGISTER);
            setNumTimesToAttemptAgentRestart(DEFAULT_NUM_TIMES_TO_ATTEMPT_AGENT_RESTART);
            setNumTimesToAttemptMachineRestart(DEFAULT_NUM_TIMES_TO_ATTEMPT_MACHINE_RESTART);

            m_mbeansWatched = new HashSet();

            setIsWatching(false);
        }
        catch(Throwable t)
        {
            LOG.warning("Watchdog(1), Throwable");
            LOG.warning(t);
        }
    }

    /**
     * Constructor for Watchdog
     *
     * @param    rmiAgentBinding
     * @param    watchedDomain
     * @param    myAgentRmiAgentBinding
     * @param    granularity
     *
     * @throws JMException
     * @throws MalformedURLException
     * @throws NotBoundException
     * @throws RemoteException
     */
    public Watchdog(
        String rmiAgentBinding, String watchedDomain, String myAgentRmiAgentBinding,
            long granularity)
                throws JMException, RemoteException, MalformedURLException, NotBoundException
    {
        this(rmiAgentBinding, watchedDomain, myAgentRmiAgentBinding);

        try
        {
            setGranularity(granularity);
        }
        catch(Throwable t)
        {
            LOG.warning("Watchdog(2), Throwable");
            LOG.warning(t);
        }
    }

    /**
     * Constructor for Watchdog
     *
     * @param    rmiAgentBinding
     * @param    watchedDomain
     * @param    myAgentRmiAgentBinding
     * @param    granularity
     * @param    numTimesToAttemptMBeanRestart
     * @param    numTimesToAttemptMBeanReregister
     * @param    numTimesToAttemptAgentRestart
     * @param    numTimesToAttemptMachineRestart
     *
     * @throws JMException
     * @throws MalformedURLException
     * @throws NotBoundException
     * @throws RemoteException
     */
    public Watchdog(
        String rmiAgentBinding, String watchedDomain, String myAgentRmiAgentBinding,
        long granularity, int numTimesToAttemptMBeanRestart, int numTimesToAttemptMBeanReregister,
                int numTimesToAttemptAgentRestart, int numTimesToAttemptMachineRestart)
                    throws JMException, RemoteException, MalformedURLException, NotBoundException
    {
        this(rmiAgentBinding, watchedDomain, myAgentRmiAgentBinding, granularity);

        try
        {
            setNumTimesToAttemptMBeanRestart(numTimesToAttemptMBeanRestart);
            setNumTimesToAttemptMBeanReregister(numTimesToAttemptMBeanReregister);
            setNumTimesToAttemptAgentRestart(numTimesToAttemptAgentRestart);
            setNumTimesToAttemptMachineRestart(numTimesToAttemptMachineRestart);
        }
        catch(Throwable t)
        {
            LOG.warning("Watchdog(1), Throwable");
            LOG.warning(t);
        }
    }

    public String retrieveOneLiner(final String defaultString)
    {
        return super.retrieveOneLiner(defaultString) + ", " + (this.getAllRunning() ? "ALL RUNNING" : "SOME NOT RUNNING");
    }

    // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Startable methods
    /**
     * Starts Watchdog, starts the Agent that is being watched (if necessary) then starts watching
     * it.
     *
     * @return whether the Watchdog MBean started.
     * @throws Exception
     */
    protected boolean startMBeanImpl() throws Exception
    {
        LOG.debug("Watchdog (" + m_objectName + "# Watching #" + getRmiAgentBinding()
                  + ").startMBeanImpl()");

        startAgent();
        startWatching();

        LOG.debug("Watchdog (" + m_objectName + "# Watching #" + getRmiAgentBinding()
                  + ").startMBeanImpl() - done");

        return true;
    }

    /**
     * Stops Watchdog. Doesn't not stop the Agent that is being watched.
     *
     * @return whether the Watchdog MBean stopped.
     * @throws Exception
     */
    protected boolean stopMBeanImpl() throws Exception
    {
        LOG.debug("Watchdog (" + m_objectName + "# Watching #" + getRmiAgentBinding()
                  + ").stopMBeanImpl()");

        stopWatching();

        LOG.debug("Watchdog (" + m_objectName + "# Watching #" + getRmiAgentBinding()
                  + ").stopMBeanImpl() - done");

        return true;
    }

    /**
     * Delegates to <code>startMBeanImpl</code>
     *
     * @return what <code>startMBeanImpl</code> returned.
     * @throws Exception
     */
    protected boolean restartMBeanImpl() throws Exception
    {
        LOG.debug("restartMBeanImpl()");

        return startMBeanImpl();
    }

    /**
     * Gets whether this Watchdog MBean has failed.
     *
     * @return whether this Watchdog MBean has failed.
     * @throws Exception
     */
    protected boolean hasMBeanFailed() throws Exception
    {
        return !isMBeanRunning();
    }

    /**
     * Gets whether this Watchdog MBean is running.
     *
     * @return whether this Watchdog MBean is running.
     * @throws Exception
     */
    protected boolean isMBeanRunning() throws Exception
    {
        long currentTime = System.currentTimeMillis();

        long timeLastDidSomething = getTimeLastDidSomething();
        long howLongSinceLastDidSomething = (currentTime - timeLastDidSomething);

        boolean isRunning = howLongSinceLastDidSomething < (3 * getGranularity());

        return isRunning;
    }

    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Startable methods

    // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< WatchdogMBean methods
    /**
     * Gets the RMI Binding of the JMX Agent that is being watched.
     *
     * @return the RMI Binding of the JMX Agent that is being watched.
     */
    public String getRmiAgentBinding()
    {
        return m_rmiAgentBinding;
    }

    /**
     * Gets the number of StartableMBeans that are being watched.
     *
     * @return the number of StartableMBeans that are being watched.
     */
    public int getNumWatched()
    {
        return m_numWatched;
    }

    /**
     * Gets the number of StartableMBeans that are running.
     *
     * @return the number of StartableMBeans that are running.
     */
    public int getNumRunning()
    {
        return m_numRunning;
    }

    /**
     * Gets the number of StartableMBeans that are not running.
     * TODO: Change name of this, not running doesn't imply stopped, the mbeans can be in either
     * FAILED, FAILED_TO_START, FAILED_TO_STOP, STARTING, STOPPING, RESTARTING, OR STOPPED states.
     *
     * @return the number of StartableMBeans that are not running.
     */
    public int getNumStopped()
    {
        return (m_numWatched - m_numRunning);
    }

    /**
     * Get whether all the StartableMBeans being watched are running.
     *
     * @return whether all the StartableMBeans being watched are running.
     */
    public boolean getAllRunning()
    {
        return (m_numRunning == m_numWatched);
    }

    /**
     * Get the amount of time in milliseconds between watching.
     *
     * @return the amount of time in milliseconds between watching.
     */
    public long getGranularity()
    {
        return m_granularity;
    }

    /**
     * Sets the amount of time in milliseconds between watching runs.
     *
     * @param    granularity the amount of time in milliseconds between watching.
     */
    public void setGranularity(long granularity)
    {
        m_granularity = granularity;
    }

    /**
     * Gets the System time that watching started.
     *
     * @return the System time that watching started.
     */
    public long getTimeStartedWatching()
    {
        return m_timeStartedWatching;
    }

    /**
     * Gets the System time that the last watching run started.
     *
     * @return the System time that the last watching run started.
     */
    public long getTimeLastWatched()
    {
        return m_timeLastWatched;
    }

    /**
     * Gets the number of times to attempt MBean restart
     *
     * @return the number of times to attempt MBean Restart
     */
    public int getNumTimesToAttemptMBeanRestart()
    {
        return m_numTimesToAttemptMBeanRestart;
    }

    /**
     * Gets the number of times to attempt MBean reregister
     *
     * @return the number of times to attempt MBean Reregister
     */
    public int getNumTimesToAttemptMBeanReregister()
    {
        return m_numTimesToAttemptMBeanReregister;
    }

    /**
     * Gets the number of times to attempt Agent restart
     *
     * @return the number of times to attempt Agent Restart
     */
    public int getNumTimesToAttemptAgentRestart()
    {
        return m_numTimesToAttemptAgentRestart;
    }

    /**
     * Gets the number of times to attempt machine restart
     *
     * @return the number of times to attempt machine restart
     */
    public int getNumTimesToAttemptMachineRestart()
    {
        return m_numTimesToAttemptMachineRestart;
    }

    /**
     * Sets the number of times to attempt MBean restart
     *
     * @param    numTimesToAttemptMBeanRestart the number of times to attempt MBean restart
     */
    public void setNumTimesToAttemptMBeanRestart(int numTimesToAttemptMBeanRestart)
    {
        m_numTimesToAttemptMBeanRestart = Math.min(numTimesToAttemptMBeanRestart,
                                                   MAX_NUM_TIMES_TO_ATTEMPT_MBEAN_RESTART);
    }

    /**
     * Sets the number of times to attempt MBean reregister
     *
     * @param    numTimesToAttemptMBeanReregister the number of times to attempt MBean reregister
     */
    public void setNumTimesToAttemptMBeanReregister(int numTimesToAttemptMBeanReregister)
    {
        m_numTimesToAttemptMBeanReregister = Math.min(numTimesToAttemptMBeanReregister,
            MAX_NUM_TIMES_TO_ATTEMPT_MBEAN_REREGISTER);
    }

    /**
     * Sets the number of times to attempt Agent restart
     *
     * @param    numTimesToAttemptAgentRestart the number of times to attempt Agent restart
     */
    public void setNumTimesToAttemptAgentRestart(int numTimesToAttemptAgentRestart)
    {
        m_numTimesToAttemptAgentRestart = Math.min(numTimesToAttemptAgentRestart,
                                                   MAX_NUM_TIMES_TO_ATTEMPT_AGENT_RESTART);
    }

    /**
     * Sets the number of times to attempt machine restart
     *
     * @param    numTimesToAttemptMachineRestart the number of times to attempt machine restart
     */
    public void setNumTimesToAttemptMachineRestart(int numTimesToAttemptMachineRestart)
    {
        m_numTimesToAttemptMachineRestart = Math.min(numTimesToAttemptMachineRestart,
            MAX_NUM_TIMES_TO_ATTEMPT_MACHINE_RESTART);
    }

    /**
     * Gets the maximum number of times to attempt an MBean restart
     *
     * @return the maximum number of times to attempt an MBean restart
     */
    public int getMaxNumTimesToAttemptMBeanRestart()
    {
        return MAX_NUM_TIMES_TO_ATTEMPT_MBEAN_RESTART;
    }

    /**
     * Gets the maximum number of times to attempt an MBean reregister
     *
     * @return the maximum number of times to attempt an MBean reregister
     */
    public int getMaxNumTimesToAttemptMBeanReregister()
    {
        return MAX_NUM_TIMES_TO_ATTEMPT_MBEAN_REREGISTER;
    }

    /**
     * Gets the maximum number of times to attempt an Agent restart
     *
     * @return the maximum number of times to attempt an Agent restart
     */
    public int getMaxNumTimesToAttemptAgentRestart()
    {
        return MAX_NUM_TIMES_TO_ATTEMPT_AGENT_RESTART;
    }

    /**
     * Gets the maximum number of times to attempt a machine restart
     *
     * @return the maximum number of times to attempt a machine restart
     */
    public int getMaxNumTimesToAttemptMachineRestart()
    {
        return MAX_NUM_TIMES_TO_ATTEMPT_MACHINE_RESTART;
    }

    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> WatchdogMBean methods

    // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< MBeanRegistration methods
    /**
     */
    public void postDeregister() {}

    /**
     * @param    registrationDone
     */
    public void postRegister(Boolean registrationDone) {}

    /**
     */
    public void preDeregister() {}

    /**
     * Pre registers this MBean
     *
     * @param    server the MBeanServer
     * @param    name the default ObjectName of this Watchdog MBean
     *
     * @return the ObjectName to use for this Watchdog MBean
     */
    public ObjectName preRegister(MBeanServer server, ObjectName name)
    {
        m_server = server;

        try
        {
            m_server.addNotificationListener(new ObjectName(com.sun.management.jmx.ServiceName.DELEGATE),
                this, null, "local");
        }
        catch (InstanceNotFoundException infe) {}
        catch (MalformedObjectNameException mone) {}

        m_objectName = name;

        return name;
    }

    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> MBeanRegistration methods

    // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< NotificationListener methods
    public void handleNotification(Notification notification, Object handback)
    {
        final String notificationType = notification.getType();

        LOG.debug("handleNotification:" + notification + "(" + notification.getTimeStamp()
                  + ") " + notificationType);

        try
        {
            // First I only case about registered + unregistered events
            if(notification instanceof MBeanServerNotification)
            {
                if (notificationType.equals("JMX.mbean.registered"))
                {
                    handleRegisterNotification((MBeanServerNotification) notification);
                }

                if (notificationType.equals("JMX.mbean.unregistered"))
                {
                    handleUnregisterNotification((MBeanServerNotification) notification);
                }
            }
        }
        catch (ClassNotFoundException cnfe) {}
        catch (InstanceNotFoundException infe) {}
        catch (MBeanException me) {}
        catch (ReflectionException re) {}

        LOG.debug("handleNotification:" + notification + "(" + notification.getTimeStamp()
                  + ") " + notificationType + " - done");
    }

    private Class getObjectNameClass(final ObjectName objectName)
        throws ClassNotFoundException, InstanceNotFoundException
    {
        final ObjectInstance objectInstance = m_server.getObjectInstance(objectName);

        final Class objectClass = Class.forName(objectInstance.getClassName());

        return objectClass;
    }

    private void handleRegisterNotification(MBeanServerNotification mbeanServerNotification)
        throws ClassNotFoundException, InstanceNotFoundException, MBeanException, ReflectionException
    {
        final ObjectName objectName = mbeanServerNotification.getMBeanName();

        final Class objectClass = getObjectNameClass(objectName);

        LOG.debug("handleRegisterNotification.oi.on = " + objectName);
        LOG.debug("handleRegisterNotification.wdon.d = " + m_watchedDomainObjectName.getDomain());
        LOG.debug("handleRegisterNotification.oi.c = " + objectClass);

        if (objectName.getDomain().equals(m_watchedDomainObjectName.getDomain()) &&
            StartableMBean.class.isAssignableFrom(objectClass))
        {
            LOG.debug("handleRegisterNotification.adding Startable = " + objectName);

            addWatchedMBean(objectName);
        }

        // Check notification to see if it's a WatchdogEventManager registration / unregistration
        if (WatchdogEventManagerMBean.class.isAssignableFrom(objectClass))
        {
            setupWatchdogEventManager(objectName);
        }
    }

    private void handleUnregisterNotification(MBeanServerNotification mbeanServerNotification)
    {
        final ObjectName objectName = mbeanServerNotification.getMBeanName();

        if (m_mbeansWatched.contains(objectName))
        {
            LOG.debug("handleUnregisterNotification.removing Startable = " + objectName);
            m_mbeansWatched.remove(objectName);
        }

        if (objectName.equals(m_watchdogEventManagerObjectName))
        {
            m_watchdogEventManagerRemoteInterface = null;
            m_watchdogEventManagerObjectName = null;
        }
    }

    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> NotificationListener methods

    /**
     * Dispatch a WatchdgoEvent to all WatchdogListener listening.
     *
     * @param    watchdogEvent the event to dispatch
     */
    public void dispatchEvent(WatchdogEvent watchdogEvent)
    {
        if (m_watchdogEventManagerRemoteInterface != null)
        {
            try
            {
                m_watchdogEventManagerRemoteInterface.publishEvent(watchdogEvent);
            }
            catch (RemoteException re) {}
        }
    }
    /**
     * Starts the JMX Agent that this Watchdog is watching, if the Agent is running nothing will be
     * done.
     *
     * @throws RemoteException
     */
    private void startAgent() throws RemoteException
    {
        LOG.debug("startAgent");

        boolean agentAlreadyRunning = true;

        try
        {
            agentAlreadyRunning = getAgentRemoteInterface().isRunning();
        }
        catch(RemoteException remoteException)
        {
            agentAlreadyRunning = false;
        }

        if(!agentAlreadyRunning)
        {
            resetRmiAgentBinding();

            while(!getAgentRemoteInterface().isRunning())
            {
                getAgentRemoteInterface().startAgent();
            }
        }

        HermesMachineProperties
            .setAgentWatcherDetails(HermesMachineProperties
                .removeBraces(m_unresolvedRmiAgentBinding), m_myAgentRmiBinding, m_objectName
                .toString());

        getAgentRemoteInterface().setWatcherDetails(m_myAgentRmiBinding, m_objectName.toString());

        LOG.debug("startAgent - done");
    }

    /**
     * Stops the JMX Agent that this watchdog is watching, if the Agent is not running then nothing
     * will be done.
     *
     * @throws RemoteException
     */
    private void stopAgent() throws RemoteException
    {
        while(getAgentRemoteInterface().isRunning())
        {
            getAgentRemoteInterface().stopAgent();
        }
    }

    /**
     * Start watching the JMX Agent.
     *
     * @throws JMException
     * @throws NamingException
     * @throws RemoteException
     */
    private void startWatching() throws RemoteException, JMException, NamingException
    {
        LOG.debug("startWatching");

        synchronized(m_watchingStartStopLock)
        {
            if(!isWatching())
            {
                startMirroring();
                updateTimeStartedWatching();
                initialiseCorrectiveActions();

                setIsWatching(true);
                setWatchingThread(new Thread(new Runnable()
                {
                    public void run()
                    {
                        doWatching();
                    }
                }, m_objectName.toString()));
                getWatchingThread().start();
            }
        }

        LOG.debug("startWatching - done");
    }

    /**
     * Stop Watching the JMX Agent.
     *
     * @throws InstanceNotFoundException
     * @throws MBeanRegistrationException
     */
    private void stopWatching() throws InstanceNotFoundException, MBeanRegistrationException
    {
        LOG.debug("stopWatching");

        synchronized(m_watchingStartStopLock)
        {
            if(isWatching())
            {
                setIsWatching(false);

                getWatchingThread().interrupt();

                try
                {

                    getWatchingThread().join();
                }
                catch(InterruptedException ie) {}

                stopMirroring();
            }
        }

        LOG.debug("stopWatching - done");
    }

    /**
     * Worker method called by <code>startWatching</code> to perform watching 'runs'.
     * Continues running as long as <code>isWatching</code> is true.
     */
    private void doWatching()
    {
        LOG.debug("doWatching(" + Thread.currentThread().getName() + ")");

        while(isWatching())
        {
            try
            {
                watchOnce();
                Thread.currentThread().sleep(getGranularity());
            }
            catch(InterruptedException ie) {}
        }

        LOG.debug("doneWatching(" + Thread.currentThread().getName() + ")");
    }

    /**
     */
    private void startMirroring() throws JMException, RemoteException, NamingException
    {
        setupTransientWatchdogData();
        getMirroringServiceMBean().startMirroring();
    }

    /**
     */
    private void stopMirroring() throws MBeanRegistrationException, InstanceNotFoundException
    {
        getMirroringServiceMBean().stopMirroring();
        tearDownTransientWatchdogData();
    }

    /**
     * Sets up some data used for watching which may change, for instance the RMI Binding of the
     * Agent to be watched - the Agent may have been moved to another machine so the RMI Binding
     * will have changed.
     *
     * @throws InstanceAlreadyExistsException
     * @throws JMException
     * @throws MBeanRegistrationException
     * @throws NamingException
     * @throws RemoteException
     */
    private void setupTransientWatchdogData()
        throws RemoteException, InstanceAlreadyExistsException, JMException,
               MBeanRegistrationException, NamingException
    {
        if(!isMirroringServiceMBeanRegistered())
        {
            resetRmiAgentBinding();

            RMIAdaptor rmiAdaptor = (RMIAdaptor) new InitialContext()
                .lookup(getAgentRemoteInterface().getRmiAdaptorJNDIBinding());
            setRMIAdaptor(rmiAdaptor);

//            testRMIAdaptor();

            MirroringServiceMBean mirroringServiceMBean = new MirroringService(getRMIAdaptor(),
                                                              getWatchedDomainObjectName(), null);
            setMirroringServiceMBean(mirroringServiceMBean);
            setMirroringServiceMBeanObjectName(generateMirroringServiceObjectName());
            registerMirroringServiceMBean();
        }
    }

    private void testRMIAdaptor()
    {
        for (int i = 0; i < 10; i++)
        {
            try
            {
                Set set = getRMIAdaptor().queryMBeans(null, null);

                for (Iterator iterator = set.iterator(); iterator.hasNext();)
                {
                    ObjectInstance objectInstance = (ObjectInstance) iterator.next();
                    LOG.debug("testRMIAdaptor.objectInstance = " + objectInstance + ", objectName = " + objectInstance.getObjectName());
                }
            }
            catch (RemoteException re)
            {
                LOG.debug("testRMIAdaptor");
                LOG.debug(re);
            }
        }
    }

    /**
     * Sets all the transient data to null, done to prevent clients for using old data.
     *
     * @throws InstanceNotFoundException
     * @throws MBeanRegistrationException
     */
    private void tearDownTransientWatchdogData()
        throws InstanceNotFoundException, MBeanRegistrationException
    {
        unregisterMirroringServiceMBean();
        setMirroringServiceMBeanObjectName(null);
        setMirroringServiceMBean(null);
        setRMIAdaptor(null);
    }

    /**
     *
     * @return
     */
    private boolean isMirroringServiceMBeanRegistered()
    {
        return (getMirroringServiceMBeanObjectName() != null)
               && m_server.isRegistered(getMirroringServiceMBeanObjectName());
    }

    /**
     */
    private void registerMirroringServiceMBean()
        throws NotCompliantMBeanException, MBeanRegistrationException,
               InstanceAlreadyExistsException
    {
        LOG.debug("registerMirroringServiceMBean.MirroringServiceMBean = "
                  + getMirroringServiceMBean() + ", MirroringServiceMBeanObjectName = "
                  + getMirroringServiceMBeanObjectName());

        try
        {
            m_server.registerMBean(getMirroringServiceMBean(),
                                   getMirroringServiceMBeanObjectName());
        }
        catch(MBeanRegistrationException mre)
        {
            LOG.warning("registerMirroringServiceMBean, MBeanRegistrationException thrown");
            LOG.warning(mre);
            LOG.warning("registerMirroringServiceMBean, MBeanRegistrationException thrown, target Exception");
            LOG.warning(mre.getTargetException());

            throw mre;
        }

        LOG.debug("registerMirroringServiceMBean - done");
    }

    /**
     */
    private void unregisterMirroringServiceMBean()
        throws MBeanRegistrationException, InstanceNotFoundException
    {
        if(isMirroringServiceMBeanRegistered())
        {
            m_server.unregisterMBean(getMirroringServiceMBeanObjectName());
        }
    }

    /**
     * Updates the time watching starts.
     */
    private void updateTimeStartedWatching()
    {
        m_timeStartedWatching = System.currentTimeMillis();
        m_timeLastWatched = m_timeStartedWatching;
        m_timeLastDidSomething = m_timeStartedWatching;
    }

    /**
     * Updates the time that something was last done.
     */
    private void updateTimeLastDidSomething()
    {
        m_timeLastDidSomething = System.currentTimeMillis();
    }

    /**
     * Updates the time watching last occured.
     */
    private void updateTimeLastWatched()
    {
        m_timeLastWatched = System.currentTimeMillis();
        m_timeLastDidSomething = m_timeLastWatched;
    }

    /**
     * Cleans up a String used to represent ObjectName domains
     *
     * @param    input the String to clean up
     *
     * @return the cleaned up String
     */
    private String trimDomain(String input)
    {
        int trimIndex = input.length();
        trimIndex = getNonNegativeMinimum(input.indexOf(':'), trimIndex);
        trimIndex = getNonNegativeMinimum(input.indexOf('*'), trimIndex);

        if(trimIndex == input.length())
        {
            return input;
        }
        else
        {
            return input.substring(0, trimIndex);
        }
    }

    /**
     * Utility function for <code>trimDomain</code>
     *
     * @param    a
     * @param    b
     *
     * @return yada yada
     */
    private int getNonNegativeMinimum(int a, int b)
    {
        return ((a < 0)
                ? b
                : Math.min(a, b));
    }

    /**
     * Sets the number of MBeans that are being watched.
     */
    private void setupWatchdogNumbers()
    {
        Set watchedMBeans = m_server.queryMBeans(m_watchedDomainObjectName, null);
        m_numWatched = watchedMBeans.size();
    }

    /**
     * Returns whether this Watchdog MBean is in a suitable state for Watching.
     *
     * @return whether this Watchdog MBean is in a suitable state for Watching.
     */
    private boolean isMBeanStateSuitableForWatching()
    {
        //int mbeanState = getMBeanState();
        int mbeanState = this.retrieveMBeanState();

        return ((mbeanState != Startable.STOPPED) && (mbeanState != Startable.STOPPING));
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * Wrapper class for storing a Watched MBean's ObjectInstance and running state. Does what it
     * says on the tin :)
     */
    private class ObjectNameAndValue
    {
        /**
         * Constructor for ObjectInstanceAndValue
         *
         * @param    objectInstance
         * @param    state
         */
        public ObjectNameAndValue(ObjectName objectName, int state)
        {
            m_objectName = objectName;
            m_state = state;
        }

        /**
         *
         * @return
         */
        public ObjectName getObjectName()
        {
            return m_objectName;
        }

        /**
         *
         * @return
         */
        public int getState()
        {
            return m_state;
        }

        /** */
        private ObjectName m_objectName;
        /** */
        private int m_state;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Performs a single watching run.
     *
     * @throws InterruptedException
     */
    private void watchOnce() throws InterruptedException
    {
        LOG.debug("watchOnce(" + Thread.currentThread().getName() + ")");

        if(!isMBeanStateSuitableForWatching())
        {
            return;
        }

        Set failingMBeans = new HashSet();

        try
        {

            // Dynamically obtain a list of all the objects in the CascadedDomain
            //Set watchedMBeans = m_server.queryMBeans(m_watchedDomainObjectName, null);
            Set watchedMBeans = m_mbeansWatched;

            int numWatched = watchedMBeans.size();
            int numRunning = 0;

            for(Iterator i = watchedMBeans.iterator(); i.hasNext(); )
            {
                ThrowInterruptedExceptionIfInterrupted();

                ObjectName objectName = (ObjectName) i.next();

                int result = Startable.FAILED;

                try
                {
                    result = ((Integer) m_server.invoke(objectName, "retrieveMBeanState",
                        new Object[0], new String[0])).intValue();
                }
                catch(Exception e)
                {
                    LOG.warning("watchOnce.m_server.invoke(" + objectName +
                        ", retrieveMBeanState)");
                    LOG.warning(e);

//                    e.printStackTrace();
                    result = Startable.FAILED;
                }

                LOG.debug("watchOnce." + objectName + " = " + Startable.getStateAsString(result));

                if(result == Startable.RUNNING)
                {
                    ++numRunning;
                }
                else if((result == Startable.FAILED) || (result == Startable.FAILED_TO_START))
                {
                    failingMBeans.add(new ObjectNameAndValue(objectName, result));

//                    failingMBeans.add(objectInstance);
                }
            }

            ThrowInterruptedExceptionIfInterrupted();

            if(!failingMBeans.isEmpty())
            {
                takeCorrectiveAction(failingMBeans);

                // Send an email ?
            }

            m_numWatched = numWatched;
            m_numRunning = numRunning;
        }
        catch(InterruptedException ie)
        {
            throw ie;
        }
        catch(Exception seriousException)
        {
            LOG.warning("watchOnce, Serious Failure");
            LOG.warning(seriousException);

//            seriousException.printStackTrace(System.out);

            updateTimeLastDidSomething();

            boolean remoteAgentRunning = true;

            try
            {

                // Check to see if the remoteAgent is accessible and running
                LOG.debug("Check to see if the remoteAgent is accessible and running");

                remoteAgentRunning = getAgentRemoteInterface().isRunning();
            }
            catch(Exception _ignore)
            {
                remoteAgentRunning = false;
            }

            if(!remoteAgentRunning)
            {
                LOG.debug("!remoteAgentRunning");

                try
                {
                    final String message = m_objectName + "--Starting Agent: "
                                           + m_unresolvedRmiAgentBinding;

                    this.dispatchEvent(new WatchdogEvent(message));
                    LOG.debug(message);

                    updateTimeLastDidSomething();
                    LOG.debug("stopMirroring");
                    stopMirroring();
                    updateTimeLastDidSomething();
                    LOG.debug("startAgent");
                    startAgent();
                    updateTimeLastDidSomething();
                    LOG.debug("startMirroring");
                    startMirroring();
                    updateTimeLastDidSomething();
                    initialiseCorrectiveActions();
                }
                catch(Exception e)
                {
                    LOG.warning("watchOnce, Exception thrown during attempt to fix Serious Failure");
                    LOG.warning(e);
                }
            }
        }

        updateTimeLastWatched();
    }

    /**
     * Takes corrective action on a Set of failed MBeans
     *
     * @param    failingMBeans
     * @throws InterruptedException
     * @throws MalformedURLException
     * @throws NotBoundException
     * @throws RemoteException
     */
    private void takeCorrectiveAction(Set failingMBeans)
        throws InterruptedException, RemoteException, MalformedURLException, NotBoundException
    {

        // If any of the the failing MBeans requires an agent restart then do this and
        // for all mbeans mark their corrective action as having succeeded if they are running
        // after this agent restart

        ThrowInterruptedExceptionIfInterrupted();

//        if(requireMachineReboot(failingMBeans))
//        {
//            rebootMachine();
//        } else
        if(requireAgentRestart(failingMBeans))
        {
            restartAgent();
        }
        else
        {
            for(Iterator i = failingMBeans.iterator(); i.hasNext(); )
            {
                ThrowInterruptedExceptionIfInterrupted();

                takeCorrectiveAction((ObjectNameAndValue) i.next());
            }
        }
    }

//    /**
//     * Reboots a machine.
//     *
//     * @throws MalformedURLException
//     * @throws NotBoundException
//     * @throws RemoteException
//     */
//    private void rebootMachine() throws RemoteException, MalformedURLException, NotBoundException
//    {
//
//        // Check if the machine on which the remote agent is running is the same
//        // machine as this watchdog mbean is running.
//
//        final String rebootingMachine = getAgentRemoteInterface().getHostName();
//
//        final boolean isOdinBeingRebooted = rebootingMachine
//            .equals(HermesMachineProperties.getOdin());
//
//        final String safeMachine = getSafeMachine(rebootingMachine);
//
//        SwapMachinesRemoteInterface swapMachinesRemoteInterface = (SwapMachinesRemoteInterface) Naming
//            .lookup("rmi://" + safeMachine + "/SwapMachines");
//
//        swapMachinesRemoteInterface.swapMachines(rebootingMachine, safeMachine, true,
//                                                 !isOdinBeingRebooted, isOdinBeingRebooted);
//    }

//    /**
//     * Obtains the name of the safe machine from the name of the machine that is being rebooted.
//     *
//     * @param    rebootingMachine the name of the machine that is being rebooted.
//     *
//     * @return the name of the safe machine.
//     * @throws MalformedURLException
//     * @throws NotBoundException
//     * @throws RemoteException
//     */
//    private String getSafeMachine(final String rebootingMachine)
//        throws RemoteException, MalformedURLException, NotBoundException
//    {
//        String safeMachine = null;
//
//        final String myResolvedRmiAgentBinding = HermesMachineProperties
//            .getResolvedRmiAgentBinding(m_myAgentRmiBinding);
//
//        final AgentRemoteInterface myAgentRemoteInterface = (AgentRemoteInterface) Naming
//            .lookup(myResolvedRmiAgentBinding);
//
//
//        final String myMachine = myAgentRemoteInterface.getHostName();
//
//        if(!myMachine.equals(rebootingMachine))
//        {
//            safeMachine = myMachine;
//        }
//        else if(myMachine.equals(Configuration.DEFAULT_ACTIVE_MACHINE))
//        {
//            safeMachine = Configuration.DEFAULT_FAILOVER_MACHINE;
//        }
//        else
//        {
//            safeMachine = Configuration.DEFAULT_ACTIVE_MACHINE;
//        }
//
//        return safeMachine;
//    }

    /**
     * Restart the JMX Agent that is being watched.
     */
    private void restartAgent()
    {
        try
        {
            LOG.debug("restartAgent(" + m_objectName + ", " + m_unresolvedRmiAgentBinding + ")");

            final String message = m_objectName + "--Restarting Agent: "
                                   + m_unresolvedRmiAgentBinding;

            dispatchEvent(new WatchdogEvent(message));

//            m_cascadingAgent.stop();
//            stopMirroring();
            stopAgent();
            startAgent();
//            m_cascadingAgent.start();
//            startMirroring();

            updateEntries(m_server.queryMBeans(m_watchedDomainObjectName, null));
        }
        catch(Exception e)
        {
            LOG.warning("restartAgent(" + m_objectName + ", " + m_unresolvedRmiAgentBinding
                     + ") - Exception thrown");
            LOG.warning(e);
        }
        finally
        {
            LOG.debug("restartAgent - done");
        }
    }

    /**
     * Take corrective action on a single ObjectInstance
     *
     * @param    objectInstanceAndValue
     */
    private void takeCorrectiveAction(ObjectNameAndValue objectNameAndValue)
    {
        try
        {
            final ObjectName objectName = objectNameAndValue.getObjectName();

            WatchdogCorrectiveAction watchdogCorrectiveAction = getEntry(objectName);

            final Class objectClass = getObjectNameClass(objectName);

            String message = m_objectName + "--Taking Corrective Action on \"" + objectName + "#" +
                objectClass + "\"";

            boolean correctiveActionResult = false;

            // switch, yuck, try polymorphism
            switch(watchdogCorrectiveAction.getCurrentCorrectiveAction())
            {
                case WatchdogCorrectiveAction.INVOKE_RESTART_METHOD:
                    correctiveActionResult = invokeRestartMethod(objectNameAndValue);
                    message = message + "\ninvokeRestartMethod -" + (correctiveActionResult
                                                                     ? "succeeded"
                                                                     : "failed");

                    dispatchEvent(new WatchdogEvent(message));
                    LOG.debug(message);
                    break;

                case WatchdogCorrectiveAction.REREGISTER_MBEAN:
                    correctiveActionResult = reregisterMBean(objectName);
                    message = message + "\nreregisterMBean -" + (correctiveActionResult
                                                                 ? "succeeded"
                                                                 : "failed");

                    dispatchEvent(new WatchdogEvent(message));
                    LOG.debug(message);
                    break;

                default:
                    break;
            }

            updateEntry(objectName);
        }
        catch(Exception e)
        {
            LOG.warning("takeCorrectiveAction, Exception thrown");
            LOG.warning(e);
        }
    }

    /**
     * Determines whether any of <code>failingMBeans</code> requires an Agent restart
     *
     * @param    failingMBeans the MBean to check
     *
     * @return whether any of <code>failingMBeans</code> requires an Agent restart
     */
    private boolean requireAgentRestart(Set failingMBeans)
    {
        return anyMBeanHasCorrectiveAction(failingMBeans, WatchdogCorrectiveAction.RESTART_AGENT);
    }

    /**
     * Determines whether any of <code>failingMBeans</code> requires a machine reboot
     *
     * @param    failingMBeans the MBeans to check
     *
     * @return whether any of <code>failingMBeans</code> requires a machine reboot
     */
    private boolean requireMachineReboot(Set failingMBeans)
    {
        return anyMBeanHasCorrectiveAction(failingMBeans, WatchdogCorrectiveAction.RESTART_MACHINE);
    }

    /**
     * Determines whether any of <code>mbean</code> has the <code>matchingCorrectiveAction</code>
     * Corrective Action
     *
     * @param    mbeans the MBean to check
     * @param    matchingCorrectiveAction the Corrective Action to look for
     *
     * @return whether any of <code>mbean</code> has the <code>matchingCorrectiveAction</code>
     * Corrective Action
     */
    private boolean anyMBeanHasCorrectiveAction(final Set mbeans,
                                                final int matchingCorrectiveAction)
    {
        boolean foundMatch = false;

        for(Iterator i = mbeans.iterator(); i.hasNext(); )
        {
            ObjectNameAndValue objectNameAndValue = (ObjectNameAndValue) i.next();

            try
            {
                final WatchdogCorrectiveAction correctiveAction = getEntry(objectNameAndValue.getObjectName());

                if(correctiveAction.getCurrentCorrectiveAction() == matchingCorrectiveAction)
                {
                    foundMatch = true;

                    break;
                }
            }
            catch(HermesException he) {}
        }

        return foundMatch;
    }

    /**
     * Invokes the startMBean method on an ObjectInstance
     *
     * @param    objectInstance the MBean to act on.
     *
     * @return the result from invoking startMBean
     */
    private boolean invokeStartMethod(ObjectName objectName)
    {
        return getBooleanValue(invokeMethod(objectName, "startMBean"));
    }

    /**
     * Invokes either restartMBean or startMBean methods on an ObjectInstance depending on whether
     * the MBean is in a FAILED state or not.
     *
     * @param    objectInstanceAndValue the MBean to act on.
     *
     * @return the result from invoking restartMBean or startMBean
     */
    private boolean invokeRestartMethod(ObjectNameAndValue objectNameAndValue)
    {
        if(objectNameAndValue.getState() == FAILED)
        {
            return getBooleanValue(invokeMethod(objectNameAndValue.getObjectName(),
                                                "restartMBean"));
        }
        else
        {
            return getBooleanValue(invokeMethod(objectNameAndValue.getObjectName(),
                                                "startMBean"));
        }
    }

    /**
     * Casts <code>object</code> to a Boolean, returns false if <code>object</code> isn't a Boolean
     *
     * @param    object the Object to convert
     *
     * @return the converted value.
     */
    private boolean getBooleanValue(Object object)
    {
        boolean booleanValue = false;

        if((object != null) && (object instanceof Boolean))
        {
            booleanValue = ((Boolean) object).booleanValue();
        }

        return booleanValue;
    }

    /**
     * Invokes a method on an ObjectInstance
     *
     * @param    objectInstance the MBean to act on.
     * @param    methodName the method to invoke
     *
     * @return the result from invoking <code>methodName</code>
     */
    private Object invokeMethod(ObjectName objectName, String methodName)
    {
        Object returnValue = null;

        try
        {
            returnValue = m_server.invoke(objectName, methodName,
                                          new Object[0], new String[0]);
        }
        catch(Exception e)
        {
            LOG.warning("invokeMethod(" + objectName + ", " + methodName + ")");
            LOG.warning(e);

//            e.printStackTrace();
            returnValue = null;
        }

        return returnValue;
    }

    /**
     * Reregisters an MBean
     *
     * @param    objectInstance the MBean to reregister
     *
     * @return whether MBean started up correctly after being reregistered.
     */
    private boolean reregisterMBean(ObjectName objectName)
    {
        return false;
    }

    /**
     * Obtains the CorrectiveAction associated with an ObjectInstance. Idea: derive from
     * ObjectInstance to store all this stuff in the Cascaded MBeans.
     *
     * @param    objectInstance
     *
     * @return the CorrectiveAction associated with an ObjectInstance.
     */
    private WatchdogCorrectiveAction getEntry(ObjectName objectName) throws HermesException
    {
        WatchdogCorrectiveAction watchdogCorrectiveAction = null;

        String key = objectName.toString();
        Object object = m_watchdogCorrectiveActions.get(key);

        if(object == null)
        {
            watchdogCorrectiveAction = new WatchdogCorrectiveAction(this);

            LOG.debug("getEntry(" + objectName
                      + ").created WatchdogCorrectiveAction("
                      + watchdogCorrectiveAction.getCurrentCorrectiveAction() + ")");

            m_watchdogCorrectiveActions.put(key, watchdogCorrectiveAction);
        }
        else
        {
            watchdogCorrectiveAction = (WatchdogCorrectiveAction) object;
        }

        return watchdogCorrectiveAction;
    }

    /**
     * Updates the associated Corrective Actions of all the MBeans in <code>objectInstances</code>
     *
     * @param    objectInstances the MBeans to update
     * @throws Exception
     */
    private void updateEntries(Set objectNames) throws Exception
    {
        for(Iterator i = objectNames.iterator(); i.hasNext(); )
        {
            updateEntry((ObjectName) i.next());
        }
    }

    /**
     * Updates the associated Corrective Action of <code>objectInstance</code> by obtaining the
     * running state of <code>objectInstance</code>
     *
     * @param    objectInstance the MBean whose Corrective Action is being updated.
     * @throws Exception
     */
    private void updateEntry(ObjectName objectName) throws Exception
    {
        getEntry(objectName).setCorrectiveActionSucceeded(isRunning(objectName));
    }

    /**
     * Determines whether <code>objectInstance</code> is running by calling its 'retrieveMBeanState'
     * method.
     *
     * @param    objectInstance the MBean to check
     *
     * @return whether <code>objectInstance</code> is running by calling its 'retrieveMBeanState'
     * method.
     */
    private boolean isRunning(ObjectName objectName)
    {
        boolean running = false;

        try
        {
            Integer result = (Integer) m_server.invoke(objectName,"retrieveMBeanState",
                new Object[0], new String[0]);

            running = (result.intValue() == Startable.RUNNING);
        }
        catch(Exception e)
        {

//            LOG.warning("isRunning", e);
            //e.printStackTrace();
        }

        return running;
    }

    /**
     * Throws an InterruptedException if isInterrupted is set on the current Thread.
     *
     * @throws InterruptedException
     */
    private void ThrowInterruptedExceptionIfInterrupted() throws InterruptedException
    {
        if(Thread.currentThread().isInterrupted())
        {
            LOG.debug("Have ThrowInterruptedExceptionIfInterrupted");

            throw new InterruptedException();
        }
    }


//    /**
//     * Creates an ObjectName for the CascadingAgent MBean for this Watchdog based on the RMI Binding
//     * of the Agent being watched and the Domain being watched. Necessary to ensure uniqueness of
//     * ObjectNames as more than one CascadingAgent will be created in an MBeanServer in which more
//     * than one Watchdog is running.
//     *
//     * @return the ObjectName
//     * @throws JMException
//     */
    /**
     *
     * @return
     * @throws JMException
     */
    private ObjectName generateMirroringServiceObjectName() throws JMException
    {
        Hashtable properties = new Hashtable();
        properties.put("name", "MirroringService");
        properties.put("mirroredDomain", trimDomain(getWatchedDomainObjectName().toString()));

//        properties.put("mirroredHost", getRmiAdaptorAddress().getHost());

        return new ObjectName("DefaultDomain", properties);
    }

    /**
     * Resolves the incomplete RMI Binding of the Agent being watched.
     */
    private synchronized void resetRmiAgentBinding()
    {
        try
        {
            String newResolvedRmiAgentBinding = HermesMachineProperties
                .getResolvedRmiAgentBinding(m_unresolvedRmiAgentBinding);

            while(newResolvedRmiAgentBinding == null)
            {
                try
                {
                    Thread.currentThread().sleep(500);
                }
                catch(Exception e) {}

                newResolvedRmiAgentBinding = HermesMachineProperties
                    .getResolvedRmiAgentBinding(m_unresolvedRmiAgentBinding);
            }

//            System.out.println("newResolvedRmiAgentBinding = " + newResolvedRmiAgentBinding);
            if(!newResolvedRmiAgentBinding.equals(getRmiAgentBinding()))
            {

//                System.out.println("previousResolvedRmiAgentBinding = " + getRmiAgentBinding());
                setRmiAgentBinding(newResolvedRmiAgentBinding);
                setAgentRemoteInterface((AgentRemoteInterface) Naming.lookup(getRmiAgentBinding()));
            }
        }
        catch(Exception e)
        {
            LOG.warning("resetRmiBinding");
            LOG.warning(e);

//            e.printStackTrace();
        }
    }

    private void initialiseEventManager()
    {
        try
        {
            Set mbeans = m_server.queryMBeans(null, null);

            final Class watchdogEventManagerClass = org.jbossmx.cluster.watchdog.mbean.WatchdogEventManager.class;

            ObjectInstance watchdogEventManagerOI = null;

            for (Iterator iterator = mbeans.iterator(); iterator.hasNext() && watchdogEventManagerOI == null;)
            {
                final ObjectInstance objectInstance = (ObjectInstance) iterator.next();

                if (watchdogEventManagerClass.isAssignableFrom(Class.forName(objectInstance.getClassName())))
                {
                    watchdogEventManagerOI = objectInstance;
                }
            }

            if (watchdogEventManagerOI != null)
            {
                setupWatchdogEventManager(watchdogEventManagerOI.getObjectName());
            }
        }
        catch (Throwable t) {}
    }

    private void setupWatchdogEventManager(ObjectName objectName)
        throws InstanceNotFoundException, MBeanException, ReflectionException
    {
        m_watchdogEventManagerObjectName = objectName;

        m_watchdogEventManagerRemoteInterface = (WatchdogEventManagerRemoteInterface)
            m_server.getAttribute(objectName, "RemoteInterface");
    }

    /**
     * Determines whether this Watchdog is watching
     *
     * @return whether this Watchdog is watching
     */
    private boolean isWatching()
    {
        synchronized(m_watchingLock)
        {
            return m_isWatching;
        }
    }

    /**
     * Sets whether this Watchdog is watching.
     *
     * @param    isWatching whether this Watchdog is watching.
     */
    private void setIsWatching(final boolean isWatching)
    {
        synchronized(m_watchingLock)
        {
            m_isWatching = isWatching;
        }
    }

    /**
     * Gets the Thread that is doing the watching.
     *
     * @return
     */
    private Thread getWatchingThread()
    {
        return m_watchingThread;
    }

    /**
     * Sets the Thread that is doing the watching.
     *
     * @param    watchingThread
     */
    private void setWatchingThread(Thread watchingThread)
    {
        m_watchingThread = watchingThread;
    }

    /**
     * Initialise the Corrective Actions
     */
    private void initialiseCorrectiveActions()
    {
        m_watchdogCorrectiveActions = new HashMap();
    }

    /**
     * Gets the Domain being watched
     *
     * @return the Domain being watched
     */
    private ObjectName getWatchedDomainObjectName()
    {
        return m_watchedDomainObjectName;
    }

    /**
     * Sets the RMI Binding of the Agent being watched
     *
     * @param    rmiAgentBinding the RMI Binding of the Agent being watched
     */
    private void setRmiAgentBinding(String rmiAgentBinding)
    {
        m_rmiAgentBinding = rmiAgentBinding;
    }

    /**
     * Sets the Domain being watched
     *
     * @param    objectName the Domain being watched
     */
    private void setWatchedDomainObjectName(ObjectName objectName)
    {
        m_watchedDomainObjectName = objectName;
    }

    /**
     * Sets the remote interface of the JMX Agent being watched.
     *
     * @param    agentRemoteInterface the remote interface of the JMX Agent being watched.
     */
    private void setAgentRemoteInterface(AgentRemoteInterface agentRemoteInterface)
    {
        m_agentRemoteInterface = agentRemoteInterface;
    }

    /**
     * Gets the remote interface of the JMX Agent being watched.
     *
     * @return the remote interface of the JMX Agent being watched.
     */
    private AgentRemoteInterface getAgentRemoteInterface()
    {
        return m_agentRemoteInterface;
    }

    /**
     * @param    rmiAdaptor
     */
    private void setRMIAdaptor(RMIAdaptor rmiAdaptor)
    {
        m_rmiAdaptor = rmiAdaptor;
    }

    /**
     *
     * @return
     */
    private RMIAdaptor getRMIAdaptor()
    {
        return m_rmiAdaptor;
    }

    /**
     * @param    mirroringServiceMBean
     */
    private void setMirroringServiceMBean(MirroringServiceMBean mirroringServiceMBean)
    {
        m_mirroringServiceMBean = mirroringServiceMBean;
    }

    /**
     *
     * @return
     */
    private MirroringServiceMBean getMirroringServiceMBean()
    {
        return m_mirroringServiceMBean;
    }


    /**
     * @param    objectName
     */
    private void setMirroringServiceMBeanObjectName(ObjectName objectName)
    {
        m_mirroringServiceMBeanObjectName = objectName;
    }

    /**
     *
     * @return
     */
    private ObjectName getMirroringServiceMBeanObjectName()
    {
        return m_mirroringServiceMBeanObjectName;
    }

    /**
     * Gets the last time something was done.
     *
     * @return the last time something was done.
     */
    public long getTimeLastDidSomething()
    {
        return m_timeLastDidSomething;
    }

    private Set getMBeanWatched()
    {
        synchronized (m_mbeansWatched)
        {
            return (Set) m_mbeansWatched.clone();
        }
    }

    private void addWatchedMBean(ObjectName objectName)
    {
        synchronized (m_mbeansWatched)
        {
            m_mbeansWatched.add(objectName);
        }
    }

    private void removeWatchedMBean(ObjectName objectName)
    {
        synchronized (m_mbeansWatched)
        {
            m_mbeansWatched.remove(objectName);
        }
    }

    // This data will not change if the watched agent dies
    /** The unresolved RMI Binding of the watched agent, eg "{/JMSAgent}" */
    String m_unresolvedRmiAgentBinding;
    /** The resolved RMI Binding of the watched agent, eg "/machineName/JMSAgent" */
    String m_rmiAgentBinding;
    /** The domain that is being watched on the remote JMX Agent */
    private ObjectName m_watchedDomainObjectName;
    /** The amount of time to wait between watching runs */
    private long m_granularity;
    /** The remote interface of the JMX Agent being watched */
    private AgentRemoteInterface m_agentRemoteInterface;

    /** The MBeanServer this watchdog is running in */
    private MBeanServer m_server;
    /** The ObjectName of this Watchdog */
    private ObjectName m_objectName;
    /** The RMI Binding of the JMX Agent this Watchdog is running in */
    private String m_myAgentRmiBinding;

//    // These variables need to be renewed every time the remote agent is restarted, and
//    // for greater safety every time this watchdog is restarted.

    /** */
    private RMIAdaptor m_rmiAdaptor;
    /** */
    private MirroringServiceMBean m_mirroringServiceMBean;
    /** */
    private ObjectName m_mirroringServiceMBeanObjectName;

    /** A map of watched MBeans to CorrectiveActions */
    private Map m_watchdogCorrectiveActions;

    /** The number of times to attempt MBean restart */
    private int m_numTimesToAttemptMBeanRestart;
    /** The number of times to attempt MBean reregister */
    private int m_numTimesToAttemptMBeanReregister;
    /** The number of times to attempt Agent restart */
    private int m_numTimesToAttemptAgentRestart;
    /** The number of times to attempt machine restart */
    private int m_numTimesToAttemptMachineRestart;

    /** The number of MBeans being watched */
    private int m_numWatched;
    /** The number of MBeans that are in a RUNNING state */
    private int m_numRunning;

    /** The System time that watching started */
    private long m_timeStartedWatching = -1;
    /** The System time that watching last occured */
    private long m_timeLastWatched = -1;
    /** The System time that something was last done */
    private long m_timeLastDidSomething = -1;

    /** An Object used for synchoronization on read/writes to <code>m_isWatching</code>*/
    private Object m_watchingLock = new Object();
    /** An Object used for synchoronization on calls to <code>startWatching</code> and
     *  <code>stopWatching</code> */
    private Object m_watchingStartStopLock = new Object();
    /** The current watching state */
    private boolean m_isWatching;
    /** The Thread that does the watching */
    private Thread m_watchingThread;

    private HashSet m_mbeansWatched;

    private WatchdogEventManagerRemoteInterface m_watchdogEventManagerRemoteInterface;
    private ObjectName m_watchdogEventManagerObjectName;

    /** The default number of times to attempt MBean restart */
    private static final int DEFAULT_NUM_TIMES_TO_ATTEMPT_MBEAN_RESTART = 2;
    /** The default number of times to attempt MBean reregister */
    private static final int DEFAULT_NUM_TIMES_TO_ATTEMPT_MBEAN_REREGISTER = 0;
    /** The default number of times to attempt agent restart */
    private static final int DEFAULT_NUM_TIMES_TO_ATTEMPT_AGENT_RESTART = 2;
    /** The default number of times to attempt machine restart */
    private static final int DEFAULT_NUM_TIMES_TO_ATTEMPT_MACHINE_RESTART = 0;

    /** The maximum number of times to attempt MBean restart */
    private static final int MAX_NUM_TIMES_TO_ATTEMPT_MBEAN_RESTART = 20;
    /** The maximum number of times to attempt MBean reregister */
    private static final int MAX_NUM_TIMES_TO_ATTEMPT_MBEAN_REREGISTER = 0;
    /** The maximum number of times to attempt agent restart */
    private static final int MAX_NUM_TIMES_TO_ATTEMPT_AGENT_RESTART = 5;
    /** The maximum number of times to attempt machine restart */
    private static final int MAX_NUM_TIMES_TO_ATTEMPT_MACHINE_RESTART = 0;

    private static Logger LOG = Logger.getLogger(Watchdog.class);
}
