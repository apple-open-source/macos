/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean.watchdog;

// Hermes Packages
import org.jbossmx.cluster.watchdog.agent.AgentRemoteInterface;

/**
 * CorrectiveAction for restarting a JMX Agent
 *
 * @author Stacy Curl
 */
public class RestartAgentCorrectiveAction
    extends BaseCorrectiveAction
{
    /**
     * Apply the RestartAgentCorrectiveAction
     *
     * @return whether the CorrectiveAction succeeded
     * @throws Exception
     */
    protected boolean applyImpl() throws Exception
    {
        AgentRemoteInterface agentRemoteInterface = (AgentRemoteInterface) getCorrectiveActionContext()
            .getContextObject(CorrectiveActionContextConstants.Agent_RemoteInterface);

        if(!agentRemoteInterface.isRunning())
        {
            agentRemoteInterface.stopAgent();
            agentRemoteInterface.startAgent();
        }

        return agentRemoteInterface.isRunning();
    }

    /**
     * Returns whether <code>correctiveAction</code> overrides this.
     *
     * @param    correctiveAction the CorrectiveAction to compare with this.
     *
     * @return whether <code>correctiveAction</code> overrides this.
     */
    public boolean isOverridenBy(final CorrectiveAction correctiveAction)
    {
        return (correctiveAction instanceof InvokeMethodCorrectiveAction);
    }
}
