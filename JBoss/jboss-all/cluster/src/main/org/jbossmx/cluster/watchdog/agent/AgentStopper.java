/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.agent;

import org.jbossmx.cluster.watchdog.Configuration;

/**
 * Class for stopping a series of JMX Agents using RMI Activation
 *
 * @author Stacy Curl
 */
public class AgentStopper
{
    /**
     * Stops a series of JMX Agents using RMI Activation
     *
     * @param    args an array of RMI Bindings of JMS Agents to stop
     */
    public static void main(String args[]) throws Exception
    {
        if(args.length == 0)
        {
            System.out.println("Usage: AgentStopper <Log4J Properties file> <FullRmiAgentBinding>*");
        }
        else
        {
            // Setup everything
            for(int aLoop = 0; aLoop < args.length; ++aLoop)
            {
                try
                {
                    System.out.println("stopAgent(" + args[aLoop] + ")");

                    AgentRemoteInterface agentRemoteInterface = BaseAgent.getAgent(args[aLoop]);
                    agentRemoteInterface.stopAgent();
                    System.out.println("stopAgent(" + args[aLoop] + ") - done");
                }
                catch(Exception e)
                {
                    System.err.println("stopAgent(" + args[aLoop] + "), Exception thrown");
                    e.printStackTrace(System.err);
                }
                catch(Throwable t)
                {
                    System.err.println("stopAgent(" + args[aLoop] + "), Throwable thrown");
                    t.printStackTrace(System.err);
                }

                //java.rmi.activation.Activatable.unexportObject(agentRemoteInterface, true);
            }
        }
    }
}


/*--- Formatted in Stacy XCT Convention Style on Thu, Feb 15, '01 ---*/


/*------ Formatted by Jindent 3.22 Basic 1.0 --- http://www.jindent.de ------*/
