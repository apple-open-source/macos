/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.agent;

import org.jboss.logging.Logger;

// Hermes JMX Packages
import org.jbossmx.cluster.watchdog.Configuration;

import org.jbossmx.cluster.watchdog.util.CompoundException;
import org.jbossmx.cluster.watchdog.util.HermesMachineProperties;

// 3rd Party Packages
import com.sun.management.jmx.ServiceName;
import com.sun.management.jmx.Trace;

import javax.management.InstanceNotFoundException;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanException;
import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectInstance;
import javax.management.ObjectName;
import javax.management.ReflectionException;

import org.jbossmx.cluster.watchdog.mbean.xmlet.SimpleFailedMBeanPacker;
import org.jbossmx.cluster.watchdog.mbean.xmlet.XMLet;
import org.jbossmx.cluster.watchdog.mbean.xmlet.XMLetEntry;

import javax.management.loading.MLet;
import javax.management.loading.MLetMBean;

// Standard Java Packages
import java.io.*;

import java.net.*;

import java.lang.reflect.Constructor;

import java.rmi.*;
import java.rmi.activation.*;

import java.util.Iterator;
import java.util.Properties;
import java.util.Set;

/**
 * I think that this class can be improved with custom initialisors, specified in the
 * MarshalledObject Properties object and access through reflection (thus isolating third parties).
 * Currently I'm using this technique to apply custom logging initialisation.
 *
 * @author Stacy Curl
 */
public class BaseAgent
    extends Activatable
    implements AgentRemoteInterface
{

    // The constructor for activation and export; this constructor is
    // called by the method ActivationInstantiator.newInstance during
    // activation, to construct the object.
    //

    /**
     * @param    id for activating object
     * @param    data the object's initialization data contains location of MLet Resource file
     *
     * @throws RemoteException
     */
    public BaseAgent(ActivationID id, MarshalledObject data) throws RemoteException
    {
        // Register the object with the activation system
        // then export it on an anonymous port
        //
        super(id, 0);


        try
        {
            Properties properties = (Properties) data.get();

            m_mletUrl = properties.getProperty(Configuration.MLET_JAR_PATH_PROPERTY);
            m_mletMBeansList = properties
                .getProperty(Configuration.MLET_RESOURCE_LOCATION_PROPERTY);

            m_tolerateSystemMBeanFailures = Boolean.valueOf(properties.getProperty(
                Configuration.TOLERATE_SYSTEM_MBEAN_FAILURE_PROPERTY, "false")).booleanValue();
            m_tolerateCustomMBeanFailures = Boolean.valueOf(properties.getProperty(
                Configuration.TOLERATE_CUSTOM_MBEAN_FAILURE_PROPERTY, "false")).booleanValue();

            configureAgent(properties);

            //LOG.debug("##########################################################################");
            //LOG.debug("BaseAgent.constructor.properties = " + properties);
        }
        catch(Exception e)
        {
            e.printStackTrace(System.out);

            //LOG.warning(e);

            throw new RemoteException("BaseAgent, RemoteException", e);
        }
    }

    /**
     * Configure the Agent by creating a class via reflection, the class is
     *
     * @param    agentProperties
     * @throws Exception
     */
    private void configureAgent(final Properties agentProperties) throws Exception
    {
        final String systemAgentConfiguratorClassName = System
            .getProperty(Configuration.AGENT_CONFIGURATOR_CLASS_PROPERTY);

        final String agentConfiguratorClassName = agentProperties
            .getProperty(Configuration
                .AGENT_CONFIGURATOR_CLASS_PROPERTY, systemAgentConfiguratorClassName);

        if(agentConfiguratorClassName != null)
        {
            final Class agentConfiguratorClass = Class.forName(agentConfiguratorClassName);
            final Constructor agentConfiguratorConstructor = agentConfiguratorClass
                .getConstructor(new Class[]{ java.util.Properties.class,
                                             BaseAgent.class });

            m_agentConfigurator = agentConfiguratorConstructor
                .newInstance(new Object[]{ agentProperties,
                                           this });
        }
    }

    // Implement the methods declared in AgentRemoteInterface
    //

    /**
     * Starts the Agent, loading the mbeans from the location specified during RMI registration
     *
     * @return true if the agent was started, false if the agent failed to start or was already running
     * @throws RemoteException
     */
    public boolean startAgent() throws RemoteException
    {
        try
        {
            //LOG.debug("BaseAgent.startAgent() called");

            return startAgent(m_mletUrl, m_mletMBeansList);
        }
        catch(RemoteException re)
        {
            System.out.println("RemoteException in BaseAgent.startAgent");

            re.printStackTrace();
            re.detail.printStackTrace();

            throw re;
        }
        catch(Throwable t)
        {
            System.out.println("Throwable in BaseAgent.startAgent");

            t.printStackTrace();

            return false;
        }
    }

    /**
     * Starts the Agent, instructing it to load it's mbeans from <code>mletMBeansList</code>
     *
     * @param    mletUrl classpath to search for when loading mbeans specified in <code>mletMBeansList</code>
     * @param    mletMBeansList URL of MLet resource containing mbeans to load
     *
     * @return true if the agent was started, false if the agent failed to start or was already running.
     * @throws RemoteException
     */
    public boolean startAgent(String mletUrl, String mletMBeansList) throws RemoteException
    {
        //LOG.debug("BaseAgent.startAgent(" + mletUrl + ", " + mletMBeansList + ") called");

        String[] mletUrls = { mletUrl };
        String[] mletMBeansLists = { mletMBeansList };
        return startAgent(mletUrls, mletMBeansLists);
    }

    /**
     * Starts the Agent, instructing it to load it's mbeans from <code>mletMBeansList</code>
     *
     * @param    mletUrls classpath to search for when loading mbeans specified in <code>mletMBeansLists</code>
     * @param    mletMBeansLists URL of MLet resource containing mbeans to load
     *
     * @return true if the agent was started, false if the agent failed to start or was already running.
     * @throws RemoteException
     */
    public boolean startAgent(String[] mletUrls, String[] mletMBeansLists) throws RemoteException
    {
        //LOG.debug("BaseAgent.startAgent(" + mletUrls + ", " + mletMBeansLists + ") called");

        boolean succeeded = false;

        try
        {
//            System.getProperties().setProperty("INFO_MLET", "blah");
//            System.getProperties().setProperty("INFO_ALL", "blah");
//            System.getProperties().setProperty("LEVE_DEBUG", "blah");

            Trace.parseTraceProperties();
        }
        catch(Exception ie)
        {
            //LOG.warning("startAgent, Exception thrown when examining Trace properties");
            //LOG.warning(ie);
        }

        try
        {
            if(!isRunning())
            {
                //LOG.debug("Creating server...");
                createServer();

                //LOG.debug("Registering MBeans");
                registerMBeans(mletUrls, mletMBeansLists);

                //LOG.debug("Starting MBeans");
                startMBeans();

                //LOG.debug("startAgent() - done");

                succeeded = true;
            }
        }
        catch(RemoteException re)
        {
            //LOG.warning(re);

            try
            {
                stopAgent();
            }
            catch(Exception e2) {}

            throw re;
        }
        catch(Exception e)
        {
            //LOG.warning(e);

            try
            {
                stopAgent();
            }
            catch(Exception e2) {}

            throw new RemoteException("", e);
        }

        return succeeded;
    }

    /**
     * Stops the agent.
     *
     * @return true if the agen was stopped, false if the agent failed to stop or was already stopped.
     * @throws RemoteException if the Agent called System.exit to terminate the JVM of the Agent
     */
    public boolean stopAgent() throws RemoteException
    {
        boolean succeeded = false;

        try
        {
            if(isRunning())
            {

//                //LOG.debug("Stop any Watchdog MBean that is watching me");
//                stopThingWatchingMe();

                //LOG.debug("Stopping MBeans");
                stopMBeans();

                //LOG.debug("Unregistering MBeans");
                unregisterMBeans();

                //LOG.debug("Removing Server");
                removeServer();

                //log("Deactivating object");
                //Activatable.unexportObject(this, true);
                //inactive(getID());

                succeeded = true;

                //LOG.debug("stopAgent() - done");

                System.exit(1);

                //Activatable.unexportObject(this, false);
            }
        }
        catch(RemoteException re)
        {
            //LOG.warning(re);

            throw re;
        }
        catch(Exception e)
        {
            //LOG.warning(e);

            throw new RemoteException("", e);
        }

        return succeeded;
    }

    /**
     * Returns the running state of the agent.
     *
     * @return the running state of the agent.
     * @throws RemoteException
     */
    public boolean isRunning() throws RemoteException
    {
        try
        {
            return isServerCreated() && areMBeansRegistered();    // &&

            //areMBeansRunning();
        }
        catch(RemoteException re)
        {
            throw re;
        }
        catch(Exception e)
        {
            throw new RemoteException("", e);
        }
    }

    /**
     * Returns the hostname of agents machine
     *
     * @return the hostname of agents machine
     * @throws RemoteException
     */
    public String getHostName() throws RemoteException
    {
        try
        {
            return java.net.InetAddress.getLocalHost().getHostName();
        }
        catch(java.net.UnknownHostException uhe)
        {
            throw new RemoteException("", uhe);
        }
    }

    /**
     *
     * @return
     * @throws RemoteException
     */
    public String getRmiAdaptorJNDIBinding() throws RemoteException
    {
        try
        {
            return (String) m_server.getAttribute(m_rmiConnectorObjectName, "JNDIName");
        }
        catch(Exception e)
        {
            return "";
        }
    }

    /**
     * Invokes an operation on an MBean.
     *
     * @param    objectName The object name of the MBean on which the method is to be invoked.
     * @param    methodName The name of the operation to be invoked.
     * @param    params An array containing the parameters to be set when the operation is invoked
     * @param    signature An array containing the signature of the operation. The class objects will be loaded using the same class loader as the one used for loading the MBean on which the operation was invoked.
     *
     * @return The object returned by the operation, which represents the result of invoking the operation on the MBean specified.
     * @throws RemoteException
     */
    public Object invokeMethodOnMBean(
        String objectName, String methodName, Object params[], String signature[])
            throws RemoteException
    {
        try
        {
            //LOG.debug("invokeMethodOnMBean(" + objectName + ", " + methodName + ")");

            Object returnValue = m_server.invoke(new ObjectName(objectName), methodName, params,
                                                 signature);
            //LOG.debug("invokeMethodOnMBean(" + objectName + ", " + methodName + ") - done");

            return returnValue;
        }
        catch(MalformedObjectNameException mone)
        {
            //LOG.warning(mone);

            throw new RemoteException("", mone);
        }
        catch(ReflectionException re)
        {
            //LOG.warning(re);

            throw new RemoteException("", re);
        }
        catch(MBeanException me)
        {
            //LOG.warning(me);

            throw new RemoteException("", me);
        }
        catch(InstanceNotFoundException infe)
        {
            //LOG.warning(infe);

            throw new RemoteException("", infe);
        }
    }

    /**
     * Get an attribute from an MBean.
     *
     * @param    objectName The object name of the MBean.
     * @param    attrName The name of the attribute.
     *
     * @return The object returned by the operation, which represents the value of the attribute.
     * @throws RemoteException
     */
    public Object getMBeanAttribute(String objectName, String attrName)
            throws RemoteException
    {
        try
        {
            return m_server.getAttribute(new ObjectName(objectName), attrName)
        }
        catch(MalformedObjectNameException mone)
        {
            throw new RemoteException("", mone);
        }
        catch(ReflectionException re)
        {
            throw new RemoteException("", re);
        }
        catch(MBeanException me)
        {
            throw new RemoteException("", me);
        }
        catch(InstanceNotFoundException infe)
        {
            throw new RemoteException("", infe);
        }
    }

    /**
     * Registers information with the agent on the location of a Watchdog MBean watching the agent.
     *
     * @param    watcherRmiAgentBinding The RMI Binding of the Agent containing the Watchdog MBean
     * @param    watcherObjectName The ObjectName of the Watchdog MBean
     * @throws RemoteException
     */
    public void setWatcherDetails(String watcherRmiAgentBinding, String watcherObjectName)
        throws RemoteException
    {
        m_watcherRmiAgentBinding = watcherRmiAgentBinding;
        m_watcherObjectName = watcherObjectName;
    }

//
//    public String[] getWatcherDetails() throws RemoteException
//    {
//        return new String[] {m_watcherRmiAgentBinding, m_watcherObjectName};
//    }

    /**
     * @param    machineName
     * @param    rmiAgentBinding
     *
     * @return
     * @throws MalformedURLException
     * @throws NotBoundException
     * @throws RemoteException
     */
    public static boolean isAgentRunning(String machineName, String rmiAgentBinding)
        throws RemoteException, MalformedURLException, NotBoundException
    {
        AgentRemoteInterface agentRemoteInterface = getAgent(machineName, rmiAgentBinding);

        return agentRemoteInterface.isRunning();
    }

    /**
     * @param    fullRmiAgentBinding
     *
     * @return
     * @throws MalformedURLException
     * @throws NotBoundException
     * @throws RemoteException
     */
    public static AgentRemoteInterface getAgent(String fullRmiAgentBinding)
        throws RemoteException, MalformedURLException, NotBoundException
    {
        AgentRemoteInterface agentRemoteInterface = (AgentRemoteInterface) Naming
            .lookup(fullRmiAgentBinding);

        return agentRemoteInterface;
    }

    /**
     * @param    machineName
     * @param    rmiAgentBinding
     *
     * @return
     * @throws MalformedURLException
     * @throws NotBoundException
     * @throws RemoteException
     */
    public static AgentRemoteInterface getAgent(String machineName, String rmiAgentBinding)
        throws RemoteException, MalformedURLException, NotBoundException
    {
        return getAgent("rmi://" + machineName + rmiAgentBinding);
    }

    /**
     * @param    rmiAgentBinding
     *
     * @return
     * @throws MalformedURLException
     * @throws NotBoundException
     * @throws RemoteException
     */
    public static AgentRemoteInterface getAgentFromConfiguration(String rmiAgentBinding)
        throws RemoteException, MalformedURLException, NotBoundException
    {
        return getAgent(HermesMachineProperties.getMachineName(rmiAgentBinding), rmiAgentBinding);
    }

//    private void setRmiAdaptorJNDIBinding(String rmiConnectorJNDIBinding)
//    {
//        m_rmiConnectorJNDIBinding = rmiConnectorJNDIBinding;
//    }

    /**
     * Returns true if the MBeanServer has been created
     *
     * @return true if the MBeanServer has been created
     */
    private boolean isServerCreated()
    {
        return (m_server != null);
    }

    /**
     *
     * @return
     * @throws Exception
     */
    private boolean areMBeansRunning() throws Exception
    {
        return false;
    }

    /**
     *
     * @return
     * @throws Exception
     */
    private boolean areMBeansRegistered() throws Exception
    {
        return true;
    }

    /**
     * Creates a new instance of the {@link MBeanServer}
     *
     * @return true if the {@link MBeanServer} could be created
     */
    private synchronized boolean createServer()
    {
        if(!isServerCreated())
        {
            m_server = MBeanServerFactory.createMBeanServer();

            return true;
        }
        else
        {
            return false;
        }
    }

    /**
     * Destroys the {@link MBeanServer}
     *
     * @return true if the {@link MBeanServer} could be destroyed
     */
    private synchronized boolean removeServer()
    {
        if(isServerCreated())
        {
            MBeanServerFactory.releaseMBeanServer(m_server);

            m_server = null;

            return true;
        }
        else
        {
            return false;
        }
    }

    /**
     * Loads all the MBeans into the MBeanServer.
     *
     * @param    mletUrls classpath to search for when loading mbeans specified in <code>mletMBeansLists</code>
     * @param    mletMBeansLists URL of MLet resource containing mbeans to load
     *
     * @return true if the MBeans were loaded correctly
     * @throws Exception
     */
    private boolean registerMBeans(String[] mletUrls, String[] mletMBeansLists) throws Exception
    {
        // It would be better to create the MLet MBean directly then insert, this way we will
        // catch incorrect method names.

        //LOG.debug("Creating MLet MBean");

        //MLetMBean mletMBean = new MLet();
        MLetMBean mletMBean = new XMLet();//.setFailedMBeanPacker(new SimpleFailedMBeanPacker());

        m_server.registerMBean(mletMBean,
                               new ObjectName(m_server.getDefaultDomain() + ":name="
                                              + mletMBean.getClass().getName()));

        //LOG.debug("Adding Urls");

        int numUrls = mletUrls.length;
        for(int uLoop = 0; uLoop < numUrls; ++uLoop)
        {
            mletMBean.addURL(mletUrls[uLoop]);
        }

        //LOG.debug("Adding MBeans via MLet");

        int numMLetMBeansLists = mletMBeansLists.length;
        for(int lLoop = 0; lLoop < numMLetMBeansLists; ++lLoop)
        {
            mletGetMBeansFromURL(mletMBean, mletMBeansLists[lLoop]);
        }

        return true;
    }

    /**
     * Creates the MBeans that are specified in <code>mletMBeansList</code> using the MLet MBean
     *
     * @param    mletMBean
     * @param    mletMBeansList the MLet resource file which contains the mbeans to load
     *
     * @throws Exception
     */
    private void mletGetMBeansFromURL(MLetMBean mletMBean, String mletMBeansList) throws Exception
    {
        //LOG.debug("mletGetMBeansFromURL(" + mletMBeansList + ")");

        Set objectInstanceSet = mletMBean.getMBeansFromURL(mletMBeansList);

        CompoundException compoundException = new CompoundException();
        boolean haveException = false;

        for(Iterator i = objectInstanceSet.iterator(); i.hasNext(); )
        {
            Object o = i.next();
            if(o instanceof ObjectInstance)
            {
                ObjectInstance objectInstance = (ObjectInstance) o;

                //System.out.println(objectInstance + ":" + objectInstance.getObjectName());
                if(isRmiAdaptorService(objectInstance.getClassName()))
                {
                    m_rmiConnectorObjectName = objectInstance.getObjectName();
                }
            }
            else
            {
                Object[] failedMBean = (Object[]) o;

                final XMLetEntry xmletEntry = (XMLetEntry) failedMBean[0];
                final Throwable throwable = (Throwable) failedMBean[1];

                if (checkTolerance(xmletEntry, throwable))
                {
                    haveException = true;
                    compoundException.addException((Exception) o);

                    if(o instanceof MBeanException)
                    {
                        compoundException.addException(((MBeanException) o).getTargetException());
                    }
                }
            }
        }

        if(haveException)
        {
            throw compoundException;
        }
    }

    private boolean checkTolerance(final XMLetEntry xmletEntry, final Throwable throwable)
    {
        boolean tolerance = true;

        if (!m_tolerateSystemMBeanFailures && !m_tolerateCustomMBeanFailures)
        {
            tolerance = false;
        }
        else
        {
            final boolean isSystemMBean = isSystemMean(xmletEntry.getProperty(XMLetEntry.CODE_ATTRIBUTE));

            tolerance = (isSystemMBean && m_tolerateSystemMBeanFailures) ||
                        (!isSystemMBean && m_tolerateCustomMBeanFailures);
        }

        return tolerance;
    }

    /**
     * Returns true if <code>className</code> is a System MBean
     *
     * @param    className the class name to be checked
     *
     * @return true if <code>className</code> is a System MBean
     */
    private boolean isSystemMean(String className)
    {
        return isRmiAdaptorService(className) || isHtmlAdaptorServer(className);
    }

    /**
     * Returns true if <code>className</code> is an RMI Adaptor MBean
     *
     * @param    className the class name to be checked
     *
     * @return true if <code>className</code> is an RMI Adaptor MBean
     */
    private boolean isRmiAdaptorService(String className)
    {
        return org.jboss.jmx.connector.rmi.RMIAdaptorService.class.getName().equals(className);    // ||
    }

    /**
     * Returns true if <code>className</code> is an HTMLAdaptor MBean
     *
     * @param    className the class name to be checked
     *
     * @return true if <code>className</code> is an HTMLAdaptor MBean
     */
    private boolean isHtmlAdaptorServer(String className)
    {
        return com.sun.jdmk.comm.HtmlAdaptorServer.class.getName().equals(className);
    }

    /**
     * Unregistered the MBeans from the MBeanServer
     *
     * @return true if the MBeans were unregistered
     * @throws Exception
     */
    private boolean unregisterMBeans() throws Exception
    {
        Set mbeans = m_server.queryMBeans(null, null);

        for(Iterator i = mbeans.iterator(); i.hasNext(); )
        {
            ObjectInstance objectInstance = (ObjectInstance) i.next();
            ObjectName objectName = objectInstance.getObjectName();
            if((objectName != null) && !objectName.toString().equals(ServiceName.DELEGATE))
            {
                unregisterMBean(objectName);
            }
        }

        return true;
    }

    /**
     * Starts the MBeans
     *
     * @return true if the MBean were started
     * @throws Exception
     */
    private boolean startMBeans() throws Exception
    {
        Set mbeans = m_server.queryMBeans(null, null);

        startNonStartableMBeans(mbeans);
        startStartableMBeans(mbeans);

        return true;
    }

    /**
     * Starts all the non StartableMBeans
     *
     * @param    mbeans all the MBeans
     */
    private void startNonStartableMBeans(Set mbeans)    // throws Exception
    {
        Object parameters[] = new Object[0];
        String signature[] = new String[0];

        for(Iterator i = mbeans.iterator(); i.hasNext(); )
        {
            ObjectInstance objectInstance = (ObjectInstance) i.next();

            if(!objectInstance.getClass()
                .isAssignableFrom(org.jbossmx.cluster.watchdog.mbean.StartableMBean.class))
            {
                try
                {
                    m_server.invoke(objectInstance.getObjectName(), "init", parameters, signature);
                }
                catch(Exception e) {}

                try
                {
                    m_server.invoke(objectInstance.getObjectName(), "start", parameters, signature);
                }
                catch(Exception e) {}
            }
        }
    }

    /**
     * Starts all the StartableMBeans
     *
     * @param    mbeans all the MBeans
     */
    private void startStartableMBeans(Set mbeans)    //throws Exception
    {
        Object parameters[] = new Object[0];
        String signature[] = new String[0];

        for(Iterator i = mbeans.iterator(); i.hasNext(); )
        {
            ObjectInstance objectInstance = (ObjectInstance) i.next();

            if(objectInstance.getClass()
                .isAssignableFrom(org.jbossmx.cluster.watchdog.mbean.StartableMBean.class))
            {
                try
                {
                    m_server.invoke(objectInstance.getObjectName(), "startMBean", parameters,
                                    signature);
                }
                catch(Exception e) {}
            }
        }
    }

    /**
     * Stops the MBeans
     *
     * @return true if the MBeans were stopped
     * @throws Exception
     */
    private boolean stopMBeans() throws Exception
    {
        Set mbeans = m_server.queryMBeans(null, null);

        stopNonStartableMBeans(mbeans);
        stopStartableMBeans(mbeans);

        return true;
    }

    /**
     * Stops all the StartableMBeans
     *
     * @param    mbeans all the MBeans
     */
    private void stopStartableMBeans(Set mbeans)    //throws Exception
    {
        Object parameters[] = new Object[0];
        String signature[] = new String[0];

        for(Iterator i = mbeans.iterator(); i.hasNext(); )
        {
            ObjectInstance objectInstance = (ObjectInstance) i.next();

            if(objectInstance.getClass()
                .isAssignableFrom(org.jbossmx.cluster.watchdog.mbean.StartableMBean.class))
            {
                try
                {
                    m_server.invoke(objectInstance.getObjectName(), "stopMBean", parameters,
                                    signature);
                }
                catch(Exception e) {}
            }
        }
    }

    /**
     * Stops all the non StartableMBeans
     *
     * @param    mbeans all the MBeans
     */
    private void stopNonStartableMBeans(Set mbeans)    //throws Exception
    {
        Object parameters[] = new Object[0];
        String signature[] = new String[0];

        for(Iterator i = mbeans.iterator(); i.hasNext(); )
        {
            ObjectInstance objectInstance = (ObjectInstance) i.next();

            if(!objectInstance.getClass()
                .isAssignableFrom(org.jbossmx.cluster.watchdog.mbean.StartableMBean.class))
            {
                try
                {
                    m_server.invoke(objectInstance.getObjectName(), "stop", parameters, signature);
                }
                catch(Exception e) {}
            }
        }
    }

    /**
     * Creates an MBean
     *
     * @param    className the class of the MBean
     * @param    objectName the ObjectName of the MBean
     *
     * @throws Exception
     */
    private void createMBean(String className, ObjectName objectName) throws Exception
    {
        try
        {
            m_server.createMBean(className, objectName);
        }
        catch(Exception e)
        {
            //LOG.warning("createMBean(" + className + ", " + objectName + "), Exception thrown");
            //LOG.warning(e);

            throw e;
        }
    }

    /**
     * Unregisters an MBean
     *
     * @param    objectName the Object of the MBean to unregister
     *
     * @return true if the MBean was unregistered
     * @throws Exception
     */
    private boolean unregisterMBean(ObjectName objectName) throws Exception
    {
        if(m_server.isRegistered(objectName))
        {
            m_server.unregisterMBean(objectName);

            return true;
        }
        else
        {
            return false;
        }
    }

    /**
     * Determines if the MBean referenced by <code>objectName</code> has be registered
     *
     * @param    objectName the ObjectName of the MBean
     *
     * @return the registration status of the <code>objectName</code> MBean
     *
     * @throws Exception
     */
    private boolean isMBeanRegistered(ObjectName objectName) throws Exception
    {
        return m_server.isRegistered(objectName);
    }

    /**
     * Stops the Watchdog MBean watching this Agent
     *
     * @return true if the Watchdog was stopped.
     */
    private boolean stopThingWatchingMe()
    {
        boolean succeeded = false;

        try
        {
            AgentRemoteInterface watcherAgent = (AgentRemoteInterface) Naming
                .lookup(HermesMachineProperties
                    .getResolvedRmiAgentBinding(m_watcherRmiAgentBinding));

            watcherAgent.invokeMethodOnMBean(m_watcherObjectName, "stopMBean", new Object[0],
                                             new String[0]);

            succeeded = true;
        }
        catch(Exception e)
        {
            //LOG.warning(e);
        }
        catch(Throwable t)
        {
            //LOG.warning(t);
        }

        return succeeded;
    }

    /** The MBeanServer */
    private MBeanServer m_server;
    /** The Classpath of the MLet MBean */
    private String m_mletUrl;
    /** The List of MLet resources files to use when booting the agent */
    private String m_mletMBeansList;
    /** */
    private boolean m_tolerateSystemMBeanFailures;
    /** */
    private boolean m_tolerateCustomMBeanFailures;
    /** The RMI Binding of the Agent which contains the Watchdog MBean watching this Agent */
    private String m_watcherRmiAgentBinding = "";
    /** The ObjectName of the Watchdog MBean watching this Agent */
    private String m_watcherObjectName = "";
    /** The Port on which the RMIAdaptor MBean is listening */
    private ObjectName m_rmiAdaptorObjectName;
//    private String m_rmiAdaptorJNDIBinding;

    /** */
    private Object m_agentConfigurator;

//    private static final Log LOG = Log.createLog(BaseAgent.class.getName());

    /** */
    private static Object[] m_sEmptyObject = new Object[0];
    /** */
    private static String[] m_sEmptyString = new String[0];
}


