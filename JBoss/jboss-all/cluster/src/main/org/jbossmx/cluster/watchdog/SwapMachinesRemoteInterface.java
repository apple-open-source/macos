/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog;

// Standard Java Packages
import java.rmi.Remote;
import java.rmi.RemoteException;

import java.util.Set;

/**
 *
 * @author Stacy Curl
 */
public interface SwapMachinesRemoteInterface
    extends Remote
{
    /**
     * Swaps the active and failover machines.
     *
     * @param    rebootingMachine the machine that is being rebooted
     * @param    safeMachine the machine that is still alive
     * @param    reboot whether to reboot <code>rebootingMachine</code>
     * @param    putAgentsBackWhereTheyCameFrom whether to return the Agents back to original
     * location when the <code>rebootingMachine</code> is alive again
     * @param    switchOdin whether to call a Veritas script to change odin to point to
     * <code>safeMachine</code>
     * @throws RemoteException
     */
    public void swapMachines(
        final String rebootingMachine, final String safeMachine, final boolean reboot,
            final boolean putAgentsBackWhereTheyCameFrom, final boolean switchOdin)
                throws RemoteException;

    /**
     * Moves a set of Agents from one machine to another.
     *
     * @param    agentRmiBindings the Set of RMI Bindings of JMX Agents
     * @param    fromMachine the machine to move the JMX Agents from
     * @param    toMachine the machine to move the JMX Agents to
     *
     * @throws RemoteException
     */
    public void moveAgents(
        final Set agentRmiBindings, final String fromMachine, final String toMachine)
            throws RemoteException;

    /**
     * Moves a JMX Agent from one machine to another.
     *
     * @param    agentRmi the RMI Binding of a JMX Agent
     * @param    fromMachine the machine to move the JMX Agent from
     * @param    toMachine the machine to move the JMX Agent to
     *
     * @throws RemoteException
     */
    public void moveAgent(final String agentRmi, final String fromMachine, final String toMachine)
        throws RemoteException;
}
