/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean.watchdog;

import org.jboss.jmx.adaptor.rmi.RMIAdaptor;

import org.jboss.logging.Log;

import org.jbossmx.cluster.watchdog.Configuration;
import org.jbossmx.cluster.watchdog.SwapMachinesRemoteInterface;

import org.jbossmx.cluster.watchdog.agent.AgentRemoteInterface;

import org.jbossmx.cluster.watchdog.mbean.Startable;

import org.jbossmx.cluster.watchdog.util.HermesMachineProperties;
import org.jbossmx.cluster.watchdog.util.MirroringService;
import org.jbossmx.cluster.watchdog.util.MirroringServiceMBean;

// 3rd Party Classes
import com.sun.management.jmx.ServiceName;

import javax.management.InstanceAlreadyExistsException;
import javax.management.InstanceNotFoundException;
import javax.management.JMException;
import javax.management.MBeanRegistration;
import javax.management.MBeanRegistrationException;
import javax.management.MBeanServer;
import javax.management.NotCompliantMBeanException;
import javax.management.ObjectInstance;
import javax.management.ObjectName;

import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;

// Standard Java Classes
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
 * Refactoring of Watchdog class, documentation doesn't need to be done yet, so neeer!
 *
 * @author Stacy Curl
 */
public class Watchdog2
    extends Startable
    implements Watchdog2MBean, MBeanRegistration
{
    /**
     * @param    unresolvedWatchedRmiAgentBinding
     * @param    watchedDomain
     * @param    myAgentRmiBinding
     *
     * @throws JMException
     * @throws MalformedURLException
     * @throws NotBoundException
     * @throws RemoteException
     */
    public Watchdog2(
        String unresolvedWatchedRmiAgentBinding, String watchedDomain, String myAgentRmiBinding)
            throws JMException, RemoteException, MalformedURLException, NotBoundException
    {
        this(unresolvedWatchedRmiAgentBinding, watchedDomain, myAgentRmiBinding, 5000);
    }

    /**
     * @param    unresolvedWatchedRmiAgentBinding
     * @param    watchedDomain
     * @param    myAgentRmiBinding
     * @param    granularity
     *
     * @throws JMException
     * @throws MalformedURLException
     * @throws NotBoundException
     * @throws RemoteException
     */
    public Watchdog2(
        String unresolvedWatchedRmiAgentBinding, String watchedDomain, String myAgentRmiBinding,
            long granularity)
                throws JMException, RemoteException, MalformedURLException, NotBoundException
    {
        m_unresolvedRmiAgentBinding = unresolvedWatchedRmiAgentBinding;

        String resolvedRmiAgentBinding = HermesMachineProperties
            .getResolvedRmiAgentBinding(unresolvedWatchedRmiAgentBinding);

        setRmiAgentBinding(resolvedRmiAgentBinding);
        setWatchedDomainObjectName(new ObjectName(watchedDomain));

        setAgentRemoteInterface((AgentRemoteInterface) Naming.lookup(getRmiAgentBinding()));

        m_myAgentRmiBinding = myAgentRmiBinding;

        setIsWatching(false);

        setGranularity(granularity);
    }

    // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Startable methods
    /**
     *
     * @return
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
     *
     * @return
     * @throws Exception
     */
    protected boolean stopMBeanImpl() throws Exception
    {
        LOG.debug("Watchdog (" + m_objectName + "# Watching #" + getRmiAgentBinding()
                  + ").stopMBeanImpl()");

        stopWatching();
//        stopAgent();

        LOG.debug("Watchdog (" + m_objectName + "# Watching #" + getRmiAgentBinding()
                  + ").stopMBeanImpl() - done");

        return true;
    }

    /**
     *
     * @return
     * @throws Exception
     */
    protected boolean restartMBeanImpl() throws Exception
    {
        LOG.debug("restartMBeanImpl()");

        return startMBeanImpl();
    }

    /**
     *
     * @return
     * @throws Exception
     */
    protected boolean hasMBeanFailed() throws Exception
    {
        return !isMBeanRunning();
    }

    /**
     *
     * @return
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
     *
     * @return
     */
    public String getRmiAgentBinding()
    {
        return m_rmiAgentBinding;
    }

    /**
     *
     * @return
     */
    public int getNumWatched()
    {
        return m_numWatched;
    }

    /**
     *
     * @return
     */
    public int getNumRunning()
    {
        return m_numRunning;
    }

    /**
     *
     * @return
     */
    public int getNumStopped()
    {
        return (m_numWatched - m_numRunning);
    }

    /**
     *
     * @return
     */
    public boolean getAllRunning()
    {
        return (m_numRunning == m_numWatched);
    }

    /**
     *
     * @return
     */
    public long getGranularity()
    {
        return m_granularity;
    }

    /**
     * @param    granularity
     */
    public void setGranularity(long granularity)
    {
        m_granularity = granularity;
    }

    /**
     *
     * @return
     */
    public long getTimeStartedWatching()
    {
        return m_timeStartedWatching;
    }

    /**
     *
     * @return
     */
    public long getTimeLastWatched()
    {
        return m_timeLastWatched;
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
     * @param    server
     * @param    name
     *
     * @return
     */
    public ObjectName preRegister(MBeanServer server, ObjectName name)
    {
        m_server = server;

        m_objectName = name;

        return name;
    }

    // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> MBeanRegistration methods

    /**
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
                initialiseCorrectiveActionSequences();

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
                .lookup(getAgentRemoteInterface().getRmiConnectorJNDIBinding());
            setRMIAdaptor(rmiAdaptor);

            MirroringServiceMBean mirroringServiceMBean = new MirroringService(getRMIAdaptor(),
                                                              getWatchedDomainObjectName(), null);
            setMirroringServiceMBean(mirroringServiceMBean);
            setMirroringServiceMBeanObjectName(generateMirroringServiceObjectName());
            registerMirroringServiceMBean();
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
            LOG.warning(
                "registerMirroringServiceMBean, MBeanRegistrationException thrown, target Exception");
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
     */
    private void updateTimeStartedWatching()
    {
        m_timeStartedWatching = System.currentTimeMillis();
        m_timeLastWatched = m_timeStartedWatching;
        m_timeLastDidSomething = m_timeStartedWatching;
    }

    /**
     */
    private void updateTimeLastDidSomething()
    {
        m_timeLastDidSomething = System.currentTimeMillis();
    }

    /**
     */
    private void updateTimeLastWatched()
    {
        m_timeLastWatched = System.currentTimeMillis();
        m_timeLastDidSomething = m_timeLastWatched;
    }

    // Remote ': *" from end
    /**
     * @param    input
     *
     * @return
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
     * @param    a
     * @param    b
     *
     * @return
     */
    private int getNonNegativeMinimum(int a, int b)
    {
        return ((a < 0)
                ? b
                : Math.min(a, b));
    }

    /**
     */
    private void setupWatchdogNumbers()
    {
        Set watchedMBeans = m_server.queryMBeans(m_watchedDomainObjectName, null);
        m_numWatched = watchedMBeans.size();
    }

    /**
     *
     * @return
     */
    private boolean isMBeanStateSuitableForWatching()
    {
        int mbeanState = retrieveMBeanState();

        return ((mbeanState != Startable.STOPPED) && (mbeanState != Startable.STOPPING));
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @author
     */
    private class ObjectInstanceAndValue
    {
        /**
         * @param    objectInstance
         * @param    state
         */
        public ObjectInstanceAndValue(ObjectInstance objectInstance, int state)
        {
            m_objectInstance = objectInstance;
            m_state = state;
        }

        /**
         *
         * @return
         */
        public ObjectInstance getObjectInstance()
        {
            return m_objectInstance;
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
        private ObjectInstance m_objectInstance;
        /** */
        private int m_state;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////

    /**
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
            Set watchedMBeans = m_server.queryMBeans(m_watchedDomainObjectName, null);

            int numWatched = watchedMBeans.size();
            int numRunning = 0;

            for(Iterator i = watchedMBeans.iterator(); i.hasNext(); )
            {
                ThrowInterruptedExceptionIfInterrupted();

                ObjectInstance objectInstance = (ObjectInstance) i.next();

                final int result = getResult(objectInstance);

                if(result == Startable.RUNNING)
                {
                    ++numRunning;
                }
                else if((result == Startable.FAILED) || (result == Startable.FAILED_TO_START))
                {
                    failingMBeans.add(new ObjectInstanceAndValue(objectInstance, result));
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
                    initialiseCorrectiveActionSequences();
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

    private int getResult(final ObjectInstance objectInstance)
    {
        int result = Startable.FAILED;

        try
        {
            result = ((Integer) m_server.invoke(objectInstance.getObjectName(),
                "retrieveMBeanState", new Object[0],
                new String[0])).intValue();
        }
        catch(Exception e)
        {
            LOG.warning("getResult.invoke("+ objectInstance.getObjectName().toString());
            LOG.warning(e);

            result = Startable.FAILED;
        }

        return result;
    }

    /**
     * @param    failingMBeans
     */
    private void takeCorrectiveAction(Set failingMBeans)
    {
        // Get all the corrective actions
        Set currentCorrectiveActionSequences = getCurrentCorrectiveActionSequences(failingMBeans);

        // Remove the corrective actions that are overiden by other corrective actions
        removeOverridenCorrectiveActionSequences(currentCorrectiveActionSequences);

        // Apply the corrective actions
        for(Iterator cIterator = currentCorrectiveActionSequences.iterator(); cIterator.hasNext(); )
        {
            CorrectiveActionSequence correctiveActionSequence = (CorrectiveActionSequence) cIterator
                .next();

            try
            {
                correctiveActionSequence.applyCurrentCorrectiveAction();
            }
            catch(Exception e)
            {
                LOG.warning(e);
            }
        }
    }

    /**
     * @param    objectInstances
     *
     * @return
     */
    private Set getCurrentCorrectiveActionSequences(Set objectInstances)
    {
        Set currentCorrectiveActionSequences = new HashSet();

        for(Iterator objectInstancesIterator = objectInstances.iterator();
            objectInstancesIterator.hasNext(); )
        {
            CorrectiveActionSequence correctiveActionSequence = getEntry(
                (ObjectInstance) objectInstancesIterator.next());

            currentCorrectiveActionSequences.add(correctiveActionSequence);
        }

        return currentCorrectiveActionSequences;
    }

    /**
     * @param    correctiveActionSequences
     */
    private void removeOverridenCorrectiveActionSequences(Set correctiveActionSequences)
    {
        Set overriderCorrectiveActionSequences = null;

        for(Iterator cIter = overriderCorrectiveActionSequences.iterator(); cIter.hasNext(); )
        {
            CorrectiveActionSequence correctiveActionSequence = (CorrectiveActionSequence) cIter
                .next();

            if(correctiveActionSequence.isOverridenBy(correctiveActionSequences))
            {
                if(overriderCorrectiveActionSequences == null)
                {
                    overriderCorrectiveActionSequences = new HashSet();
                }

                overriderCorrectiveActionSequences.add(correctiveActionSequence);
            }
        }

        if(overriderCorrectiveActionSequences != null)
        {
            correctiveActionSequences.removeAll(overriderCorrectiveActionSequences);
        }
    }

    /**
     * @param    object
     *
     * @return
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
     * @param    objectInstance
     *
     * @return
     */
    private CorrectiveActionSequence getEntry(ObjectInstance objectInstance)
    {
        CorrectiveActionSequence correctiveActionSequence = null;

        String key = objectInstance.getObjectName().toString();
        Object object = m_watchdogCorrectiveActionSequences.get(key);

        if(object == null)
        {
            CorrectiveActionContext correctiveActionContext = new CorrectiveActionContext(
                m_sharedCorrectiveActionContext, false);

            try
            {
                correctiveActionContext
                    .addContextObject(CorrectiveActionContextConstants
                        .MBean_ObjectName, objectInstance.getObjectName());
                correctiveActionContext
                    .addContextObject(CorrectiveActionContextConstants.MBean_Class, objectInstance
                        .getClassName());
            }
            catch(CorrectiveActionContextException cace) {}

            correctiveActionSequence = new CorrectiveActionSequence(correctiveActionContext);

            // InvokeMethodCorrectiveAction
            CorrectiveAction correctiveAction = new InvokeMethodCorrectiveAction("restartMBean",
                                                    new Object[0], new String[0],
                                                    new Boolean(true));
            correctiveAction.setNumberOfTimesToApply(4);
            correctiveActionSequence.addCorrectiveAction(correctiveAction);

            // RestartAgentCorrectiveAction
            correctiveAction = new RestartAgentCorrectiveAction();

            correctiveAction.setNumberOfTimesToApply(3);
            correctiveActionSequence.addCorrectiveAction(correctiveAction);

//            // CallScriptCorrectiveAction
//            correctiveAction = new CallScriptCorrectiveAction("/apps/hermes/bin/stopAgents " +
//                getRmiAgentBinding(), 10000);
//            correctiveAction.setNumberOfTimesToApply(2);
//            correctiveActionSequence.addCorrectiveAction(correctiveAction);
//            // TODO: Think about the Agent Binding changing when the agent that is being watched
//            //       moves to another machine, best bet, reset the entire watchdog mbean.

            LOG.debug("getEntry(" + objectInstance.toString()
                      + ").created CorrectiveActionSequence(" + correctiveActionSequence + ")");

            m_watchdogCorrectiveActionSequences.put(key, correctiveActionSequence);
        }
        else
        {
            correctiveActionSequence = (CorrectiveActionSequence) object;
        }

        return correctiveActionSequence;
    }

    /**
     * @param    objectInstance
     *
     * @return
     */
    private boolean isRunning(ObjectInstance objectInstance)
    {
        boolean running = false;

        try
        {
            Integer result = (Integer) m_server.invoke(objectInstance.getObjectName(),
                                                       "retrieveMBeanState", new Object[0],
                                                       new String[0]);
//                m_server.invoke(objectInstance.getObjectName(), "getMBeanState", new Object[0], new String[0]);

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

//        properties.put("mirroredHost", getRmiConnectorAddress().getHost());

        return new ObjectName("DefaultDomain", properties);
    }

    /**
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
            LOG.warning(e);
//            e.printStackTrace();
        }
    }

    /**
     *
     * @return
     */
    private boolean isWatching()
    {
        synchronized(m_watchingLock)
        {
            return m_isWatching;
        }
    }

    /**
     * @param    isWatching
     */
    private void setIsWatching(final boolean isWatching)
    {
        synchronized(m_watchingLock)
        {
            m_isWatching = isWatching;
        }
    }

    /**
     *
     * @return
     */
    private Thread getWatchingThread()
    {
        return m_watchingThread;
    }

    /**
     * @param    watchingThread
     */
    private void setWatchingThread(Thread watchingThread)
    {
        m_watchingThread = watchingThread;
    }

    /**
     */
    private void initialiseCorrectiveActionSequences()
    {
        m_watchdogCorrectiveActionSequences = new HashMap();
    }

    /**
     *
     * @return
     */
    private ObjectName getWatchedDomainObjectName()
    {
        return m_watchedDomainObjectName;
    }

    /**
     * @param    rmiAgentBinding
     */
    private void setRmiAgentBinding(String rmiAgentBinding)
    {
        m_rmiAgentBinding = rmiAgentBinding;
    }

    /**
     * @param    objectName
     */
    private void setWatchedDomainObjectName(ObjectName objectName)
    {
        m_watchedDomainObjectName = objectName;
    }

    /**
     * @param    agentRemoteInterface
     */
    private void setAgentRemoteInterface(AgentRemoteInterface agentRemoteInterface)
    {
        m_agentRemoteInterface = agentRemoteInterface;
    }

    /**
     *
     * @return
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
     *
     * @return
     */
    public long getTimeLastDidSomething()
    {
        return m_timeLastDidSomething;
    }

    // This data will not change if the watched agent dies
    /** The unresolved RMI Binding of the Agent being watched
     *  @see org.jbossmx.cluster.watchdog.util.HermesMachineProperties#getResolvedRmiAgentBinding(String unresolvedRmiAgentBinding)
     */
    String m_unresolvedRmiAgentBinding;
    /** */
    String m_rmiAgentBinding;
    /** The ObjectName of the Domain which contains the MBeans being watched */
    private ObjectName m_watchedDomainObjectName;
    /** The time delay in milliseconds between calls to {@link watchOnce} */
    private long m_granularity;
    /** The RMI Remote Interface of the agent which contains the MBeans that are being watched */
    private AgentRemoteInterface m_agentRemoteInterface;

    /** A reference to the MBeanServer in which this Watchdog is running */
    private MBeanServer m_server;
    /** The ObjectName of this Watchdog instance */
    private ObjectName m_objectName;
    /** The RMI Binding of the agent in which this Watchdog is running */
    private String m_myAgentRmiBinding;

//    // These variables need to be renewed every time the remote agent is restarted, and
//    // for greater safety every time this watchdog is restarted.
    /** */
    private RMIAdaptor m_rmiAdaptor;
    /** */
    private MirroringServiceMBean m_mirroringServiceMBean;
    /** */
    private ObjectName m_mirroringServiceMBeanObjectName;

    /** Store of Context information that pertains to all watched MBeans */
    private CorrectiveActionContext m_sharedCorrectiveActionContext;
    /** Store of CorrectiveActionSequences, contain one CorrectiveActionSequence for each MBean that
     *  is being watched that has failed at some time.
     */
    private Map m_watchdogCorrectiveActionSequences;

    /**
     * Sets up the shared CorrectiveActionContext object to contain context information that applies
     * to everything that this Watchdog is watching.
     *
     * @exception CorrectiveActionContextException
     */
    private void initialiseSharedCorrectiveActionContext() throws CorrectiveActionContextException
    {
        m_sharedCorrectiveActionContext = new CorrectiveActionContext();

        m_sharedCorrectiveActionContext
            .addContextObject(CorrectiveActionContextConstants
                .Agent_RmiBinding, getRmiAgentBinding());
        m_sharedCorrectiveActionContext
            .addContextObject(CorrectiveActionContextConstants.Agent_MBeanServer, m_server);
        m_sharedCorrectiveActionContext
            .addContextObject(CorrectiveActionContextConstants
                .Agent_RemoteInterface, getAgentRemoteInterface());
    }

    /** Number of MBeans watched */
    private int m_numWatched;
    /** Number of MBeans running*/
    private int m_numRunning;

    /** Time in milliseconds at which Watching begain*/
    private long m_timeStartedWatching = -1;
    /** Time in milliseconds that {@link watchOnce} last ran*/
    private long m_timeLastWatched = -1;
    /** Time in milliseconds that some part of the watching process occured */
    private long m_timeLastDidSomething = -1;

    /** Object used for synchronization of <code>m_isWatching</code> member variable */
    private Object m_watchingLock = new Object();
    /** Object used for synchronization of stopping and starting the Watchdog */
    private Object m_watchingStartStopLock = new Object();
    /** boolean indicating if that Watchdog is running */
    private boolean m_isWatching;
    /** Thread in which the watching occurs */
    private Thread m_watchingThread;

    private static Log LOG = Log.createLog(Watchdog2.class.getName());
}
