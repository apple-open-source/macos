/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog;

// Hermes JMX Packages
import org.jbossmx.cluster.watchdog.agent.AgentRemoteInterface;
import org.jbossmx.cluster.watchdog.agent.BaseAgent;

import org.jbossmx.cluster.watchdog.util.HermesMachineProperties;

// Standard Java Packages
import java.io.*;

import java.rmi.*;

import java.rmi.activation.*;

import java.rmi.registry.LocateRegistry;
import java.rmi.registry.Registry;

import java.util.Iterator;
import java.util.Set;

/**
 * Class for swapping the roles of active and failover machines, swaps the locations of all the
 * agents.
 *
 * @author Stacy Curl
 */
public class SwapMachines
    extends Activatable
    implements SwapMachinesRemoteInterface
{
// I can improve this class so that it doesn't need the parameters of which is the
// active and failover machines
//
// The identity of the active machine can be determined from the location of the Watchdog_Failover
// MBean, the identity of the failover machine can be determined from the location of the
// Watchdog_Active MBean.
//
// This program only needs as input the two machine names, the order doesn't matter
// It is important that this program be run from the failover machine, because the active
// machine will probably be rebooted during the execution of this program

    /**
     * @param    args
     * @throws Exception
     */
    public static void main(String args[]) throws Exception
    {
        if(args.length != 6)
        {
//            System.out.println("Usage: SwapMachines <projectName> <rebootingMachine> <safeMachine> <reboot> <putAgentsBackWhereTheyCameFrom> <switchOdin>");
            System.out.println(
                "Usage: SwapMachines <rebootingMachine> <safeMachine> <reboot> <putAgentsBackWhereTheyCameFrom> <switchOdin>");
        }
        else
        {
            int argIndex = 0;
//            final String projectName = args[argIndex++];
            final String rebootingMachine = args[argIndex++];
            final String safeMachine = args[argIndex++];
            final boolean reboot = Boolean.valueOf(args[argIndex++]).booleanValue();
            final boolean putAgentsBackWhereTheyCameFrom = Boolean.valueOf(args[argIndex++])
                .booleanValue();
            final boolean switchOdin = Boolean.valueOf(args[argIndex++]).booleanValue();

            SwapMachinesRemoteInterface smri = (SwapMachinesRemoteInterface) Naming
                .lookup(Configuration.getRmiSwapMachines(null));

            smri.swapMachines(rebootingMachine, safeMachine, reboot,
                              putAgentsBackWhereTheyCameFrom, switchOdin);
        }
    }

    /**
     * @param    id
     * @param    data
     *
     * @throws RemoteException
     */
    public SwapMachines(ActivationID id, MarshalledObject data) throws RemoteException
    {
        super(id, 0);

//        System.setOut(new DatePrintStream(System.out));
//        System.setErr(new DatePrintStream(System.err));
    }

    /**
     * Swaps the active and failover machines. Spawns the implementation in another thread.
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
                throws RemoteException
    {
        new Thread(new Runnable()
        {
            public void run()
            {
                swapMachinesImpl(rebootingMachine, safeMachine, reboot,
                                 putAgentsBackWhereTheyCameFrom, switchOdin);
            }
        }).start();
    }

    /**
     * Moves a set of Agents from one machine to another. Spawns the implementation in another
     * thread.
     *
     * @param    agentRmiBindings the Set of RMI Bindings of JMX Agents
     * @param    fromMachine the machine to move the JMX Agents from
     * @param    toMachine the machine to move the JMX Agents to
     *
     * @throws RemoteException
     */
    public void moveAgents(
        final Set agentRmiBindings, final String fromMachine, final String toMachine)
            throws RemoteException
    {
        new Thread(new Runnable()
        {
            public void run()
            {
                moveAgentsImpl(agentRmiBindings, fromMachine, toMachine);
            }
        }).start();
    }

    /**
     * Moves a JMX Agent from one machine to another. Spawns the implementation in another thread.
     *
     * @param    agentRmi the RMI Binding of a JMX Agent
     * @param    fromMachine the machine to move the JMX Agent from
     * @param    toMachine the machine to move the JMX Agent to
     *
     * @throws RemoteException
     */
    public void moveAgent(final String agentRmi, final String fromMachine, final String toMachine)
        throws RemoteException
    {
        new Thread(new Runnable()
        {
            public void run()
            {
                moveAgentImpl(agentRmi, fromMachine, toMachine);
            }
        }).start();
    }

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
     */
    private void swapMachinesImpl(final String rebootingMachine, final String safeMachine,
                                  final boolean reboot,
                                  final boolean putAgentsBackWhereTheyCameFrom,
                                  final boolean switchOdin)
    {
        // Q: Which points do I have to send emails, where is failure disasterous ?
        // The reasons for calling swapMachines are varied:
        // 1) Everything is working ok but someone just wants to swap the machines
        // 2) A software problem has occured too many times
        // 3) A hardware problem has occured.

        // We cannot count on all the resources to be available
        // For instance if the activeMachine has just died then we will not be able
        // to get access to the agents on it.

        // Send email now:
        // Machine 'oldActiveMachine' is being rebooted, 'newActiveMachine' will server as the active
        // machine from now on. Another email will follow indicating successful reboot of 'oldActiveMachine'
        // and placement of failover services, absence of this email indicates a serious situation:
        // only one machine is running.

        Set rebootingMachineAgents = HermesMachineProperties
            .getAgentsRunningOnMachine(rebootingMachine);
        Set safeMachineAgents = HermesMachineProperties.getAgentsRunningOnMachine(safeMachine);

        System.out.println("################## swapMachinesImpl(" + rebootingMachine + ", "
                           + safeMachine + ", " + reboot + ", " + putAgentsBackWhereTheyCameFrom
                           + ", " + switchOdin + ")");

        // Move all the agents that are running on the dying machine to the living one
        System.out.println("####### moving Agents from " + rebootingMachine + " to " + safeMachine);
        moveAgentsImpl(rebootingMachineAgents, rebootingMachine, safeMachine);

        if(switchOdin)
        {
            moveOdin(safeMachine);
        }

        // This is the most vulnerable time, all the agents are running on one machine

        if(reboot)
        {
            // Reboot the dying machine
            rebootMachine(rebootingMachine);

            while(!isMachineRunning(rebootingMachine))
            {
                Sleep(3000);
            }
        }

        System.out.println("####### moving Agents from " + safeMachine + " to " + rebootingMachine);

        if(putAgentsBackWhereTheyCameFrom)
        {
            moveAgentsImpl(safeMachineAgents, safeMachine, rebootingMachine);
        }
        else
        {
            moveAgentsImpl(rebootingMachineAgents, safeMachine, rebootingMachine);
        }

        System.out.println("################## swapMachinesImpl(" + rebootingMachine + ", "
                           + safeMachine + ", " + reboot + ", " + putAgentsBackWhereTheyCameFrom
                           + ", " + switchOdin + ") - done");

        // Send another email indicating either success or abject failure
    }

    /**
     * Moves a set of Agents from one machine to another.
     *
     * @param    agentRmiBindings the Set of RMI Bindings of JMX Agents
     * @param    fromMachine the machine to move the JMX Agents from
     * @param    toMachine the machine to move the JMX Agents to
     */
    private void moveAgentsImpl(final Set rmiBindings, final String fromMachine,
                                final String toMachine)
    {
        for(Iterator i = rmiBindings.iterator(); i.hasNext(); )
        {
            moveAgentImpl((String) i.next(), fromMachine, toMachine);
        }
    }

    /**
     * Moves a JMX Agent from one machine to another.
     *
     * @param    agentRmi the RMI Binding of a JMX Agent
     * @param    fromMachine the machine to move the JMX Agent from
     * @param    toMachine the machine to move the JMX Agent to
     */
    private void moveAgentImpl(String rmiAgentBinding, String fromMachine, String toMachine)
    {
        try
        {
            // Get fromMachine AgentRemoteInterface
            AgentRemoteInterface fromMachineAgent = null;
            boolean isAgentRunningOnFromMachine = false;

            try
            {
                fromMachineAgent = BaseAgent.getAgent(fromMachine, rmiAgentBinding);
                isAgentRunningOnFromMachine = fromMachineAgent.isRunning();
            }
            catch(Exception e)
            {
                fromMachineAgent = null;
                isAgentRunningOnFromMachine = false;
            }

            // Get toMachine AgentRemoteInterface
            final AgentRemoteInterface toMachineAgent = BaseAgent.getAgent(toMachine,
                                                            rmiAgentBinding);

            final boolean isAgentRunningOnToMachine = toMachineAgent.isRunning();

            if(!isAgentRunningOnToMachine)
            {
//                System.out.println("############## Changing " + rmiAgentBinding + " to " + toMachine);
                HermesMachineProperties.setMachineName(rmiAgentBinding, toMachine);

//                System.out.println("############## Changing " + rmiAgentBinding + " to " + toMachine + " - done");

                if(isAgentRunningOnFromMachine && (fromMachineAgent != null))
                {
                    try
                    {
//                        System.out.println("############## Stopping " + rmiAgentBinding + " on " + fromMachine);
                        fromMachineAgent.stopAgent();
//                        System.out.println("############## Stopping " + rmiAgentBinding + " on " + fromMachine + " - done");
                    }
                    catch(Exception e) {}
                }

//                System.out.println("############## Starting " + rmiAgentBinding + " on " + toMachine);
                toMachineAgent.startAgent();

//                System.out.println("############## Starting " + rmiAgentBinding + " on " + toMachine + " - done");

                // Now check that the watchdog is watching the agent on the new machine
//                final String[] watcherDetails = getWatcherDetails(fromMachineAgent);
                final String[] watcherDetails = HermesMachineProperties
                    .getAgentWatcherDetails(rmiAgentBinding);

                final String watchdogAgentRmiBinding = HermesMachineProperties
                    .getResolvedRmiAgentBinding(watcherDetails[0]);
                final String watchdogObjectName = watcherDetails[1];

//                System.out.println("watchdogAgentRmiBinding = " + watchdogAgentRmiBinding);

                final AgentRemoteInterface watchdogMachineAgent = BaseAgent.getAgent("rmi:"
                                                                      + watchdogAgentRmiBinding);

                String desiredWatchedRmiBinding = "//" + toMachine + rmiAgentBinding;

                String watchedRmiBinding = "";

                while(!desiredWatchedRmiBinding.equals(watchedRmiBinding))
                {
                    try
                    {
                        Thread.currentThread().sleep(500);
                    }
                    catch(Exception e) {}

//                    System.out.println("watchdogMachineAgent=" + watchdogMachineAgent);
//                    System.out.println("watchdogObjectName=" + watchdogObjectName);

                    watchedRmiBinding = (String) watchdogMachineAgent.getMBeanAttribute(
                        watchdogObjectName, "RmiAgentBinding");

//                    watchedRmiBinding = Configuration.getAgentWatcherDetails(rmiAgentBinding)[0];

//                    System.out.println("Watchdog for " + "//" + fromMachine + rmiAgentBinding +
//                        " is in agent "
//                        + watchdogAgentRmiBinding + " and has objectName = " + watchdogObjectName
//                        + " it is now watching " + watchedRmiBinding);
                }

//                System.out.println("I want it to be watching //" + toMachine + "/" + rmiAgentBinding);
            }
            else
            {
                System.out.println("Agent " + rmiAgentBinding + " isn't running on " + fromMachine);
            }
        }
        catch(Exception e)
        {
            e.printStackTrace();
        }
    }

    /**
     * Execute a script to change the machine that 'odin' points to
     *
     * @param    machineName the machine that odin should point to
     */
    private void moveOdin(String machineName)
    {
        execScript(Configuration.HERMES_DIRECTORY + "/bin/rebootMachine " + machineName);
    }

    /**
     * Execute a script to reboot a machine
     *
     * @param    machineName the machine to reboot
     */
    private void rebootMachine(String machineName)
    {
        execScript(Configuration.HERMES_DIRECTORY + "/bin/rebootMachine " + machineName);
    }

    /**
     * Execute a script
     *
     * @param    script the script to execute
     */
    private void execScript(String script)
    {
        try
        {
            Runtime.getRuntime().exec(script);
        }
        catch(Exception e) {}
    }

    /**
     * Gets whether a machine is running
     *
     * @param    machineName the machine to test
     *
     * @return whether a machine is running
     */
    private boolean isMachineRunning(String machineName)
    {
        Registry registry = null;

        try
        {
            registry = LocateRegistry.getRegistry();
        }
        catch(RemoteException re)
        {
            registry = null;
        }

        return (registry != null);
    }

    /**
     * Sleep for a while
     *
     * @param    millis the time in milliseconds to sleep.
     */
    private static void Sleep(int millis)
    {
        try
        {
            Thread.currentThread().sleep(millis);
        }
        catch(Exception e) {}
    }
}
