/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.agent;

// Standard Java Packages
import java.rmi.*;

/**
 * Remote interface for communicating with a JMX Agent
 *
 * @author Stacy Curl
 */
public interface AgentRemoteInterface
    extends Remote
{
    /**
     * Starts the Agent, loading the mbeans from the location specified during RMI registration
     *
     * @return true if the agent was started, false if the agent failed to start or was already running
     * @throws RemoteException
     */
    public boolean startAgent() throws RemoteException;

    /**
     * Starts the Agent, instructing it to load it's mbeans from <code>mletMBeansList</code>
     *
     * @param    mletUrl classpath to search for when loading mbeans specified in <code>mletMBeansList</code>
     * @param    mletMBeansList URL of MLet resource containing mbeans to load
     *
     * @return true if the agent was started, false if the agent failed to start or was already running.
     * @throws RemoteException
     */
    public boolean startAgent(String mletUrl, String mletMBeansList) throws RemoteException;

    /**
     * Starts the Agent, instructing it to load it's mbeans from <code>mletMBeansList</code>
     *
     * @param    mletUrls classpath to search for when loading mbeans specified in <code>mletMBeansLists</code>
     * @param    mletMBeansLists URL of MLet resource containing mbeans to load
     *
     * @return true if the agent was started, false if the agent failed to start or was already running.
     * @throws RemoteException
     */
    public boolean startAgent(String[] mletUrls, String[] mletMBeansLists) throws RemoteException;

    /**
     * Stops the agent.
     *
     * @return true if the agen was stopped, false if the agent failed to stop or was already stopped.
     * @throws RemoteException if the Agent called System.exit
     */
    public boolean stopAgent() throws RemoteException;

    /**
     * Returns the running state of the agent.
     *
     * @return the running state of the agent.
     * @throws RemoteException
     */
    public boolean isRunning() throws RemoteException;

    /**
     * Returns the hostname of agents machine
     *
     * @return the hostname of agents machine
     * @throws RemoteException
     */
    public String getHostName() throws RemoteException;

    public String getRmiConnectorJNDIBinding() throws RemoteException;

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
            throws RemoteException;

    /**
     * Registers information with the agent on the location of a Watchdog MBean watching the agent.
     *
     * @param    watcherRmiAgentBinding The RMI Binding of the Agent containing the Watchdog MBean
     * @param    watcherObjectName The ObjectName of the Watchdog MBean
     * @throws RemoteException
     */
    public void setWatcherDetails(String watcherRmiAgentBinding, String watcherObjectName)
        throws RemoteException;

//    public String[] getWatcherDetails() throws RemoteException;
}


/*--- Formatted in Stacy XCT Convention Style on Thu, Feb 15, '01 ---*/


/*------ Formatted by Jindent 3.22 Basic 1.0 --- http://www.jindent.de ------*/
