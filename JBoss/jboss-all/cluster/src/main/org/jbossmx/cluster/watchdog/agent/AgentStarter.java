/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.agent;

// Hermes JMX Packages
import org.jbossmx.cluster.watchdog.Configuration;

import org.jbossmx.cluster.watchdog.util.HermesMachineProperties;

// Standard Java Packages
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import java.rmi.RemoteException;

import javax.management.MBeanException;


/**
 * Class for starting a series of JMX Agents using RMI Activation
 *
 * @author Stacy Curl
 */
public class AgentStarter
{
    /**
     * Starts a series of JMX Agents using RMI Activation
     *
     * @param    args an array of RMI Bindings of JMS Agents to start
     * @throws Exception
     */
    public static void main(String args[]) throws Exception
    {
        if(args.length == 0)
        {
            System.out.println("Usage: AgentStarter <FullRmiAgentBinding>*");
        }
        else
        {
            List list = new ArrayList();

            // Setup everything
            for(int aLoop = 0; aLoop < args.length; ++aLoop)
            {
                if(setupAgent(args[aLoop]))
                {
                    list.add(args[aLoop]);
                }
                else
                {
                    System.out.println("Failed to setup " + args[aLoop]);
                }
            }

            // Start the agents
            for(Iterator i = list.iterator(); i.hasNext(); )
            {
                final String fullRmiAgentBinding = (String) i.next();
                try
                {
                    System.out.println("startAgent(" + fullRmiAgentBinding + ")");

                    AgentRemoteInterface agentRemoteInterface = BaseAgent
                        .getAgent(fullRmiAgentBinding);
                    agentRemoteInterface.startAgent();
                    System.out.println("startAgent(" + fullRmiAgentBinding + ") - done");
                }
                catch(Exception e)
                {
                    System.err.println("startAgent(" + fullRmiAgentBinding + "), Exception thrown");
                    //e.printStackTrace(System.err);

                    printException(e);
                }
                catch(Throwable t)
                {
                    System.err.println("startAgent(" + fullRmiAgentBinding + "), Throwable thrown");
                    t.printStackTrace(System.err);
                }
            }
        }
    }

    private static void printException(Throwable t)
    {
        t.printStackTrace();

        if (t instanceof RemoteException)
        {
            printException(((RemoteException) t).detail);
        }
        else if (t instanceof MBeanException)
        {
            printException(((MBeanException) t).getTargetException());
        }
    }

    /**
     * Registers the machine on which the JMX Agent is running.
     *
     * @param    fullRmiAgentBinding The full RMI Binding of the JMX Agent, including the machine name.
     *
     * @return true if the machine name was registerd.
     */
    private static boolean setupAgent(String fullRmiAgentBinding)
    {
        System.out.println("setupAgent(" + fullRmiAgentBinding + ")");

        boolean succeeded = false;

        // rmi://host/binding

        // machine.properties
        //   /binding.Machine = host

        // Find last '/'

        final int lastSlashLocation = fullRmiAgentBinding.lastIndexOf("/");
        final int rmiLocation = fullRmiAgentBinding.indexOf(m_sRmiPrefix);

        if((lastSlashLocation != -1) && (rmiLocation != -1))
        {
            final int hostLocation = rmiLocation + m_sRmiPrefix.length();

            String host = fullRmiAgentBinding.substring(hostLocation, lastSlashLocation);
            String localBinding = fullRmiAgentBinding.substring(lastSlashLocation);

            succeeded = HermesMachineProperties.setMachineName(localBinding, host);
        }

        System.out.println("setupAgent(" + fullRmiAgentBinding + ") - done = " + succeeded);

        return succeeded;
    }

    /** The beginning of a RMI URL */
    private static String m_sRmiPrefix = "rmi://";
}


/*--- Formatted in Stacy XCT Convention Style on Thu, Feb 15, '01 ---*/


/*------ Formatted by Jindent 3.22 Basic 1.0 --- http://www.jindent.de ------*/
