/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean.watchdog;

/**
 * CorrectiveAction for calling scripts and external programs
 */
public class CallScriptCorrectiveAction
    extends BaseCorrectiveAction
{
    /**
     * Constructor for CallScriptCorrectiveAction
     *
     * @param    scriptName the name of the script / external program to call
     * @param    timeout the amount of time (milliseconds) to wait for <code>scriptName</code> to finish.
     */
    public CallScriptCorrectiveAction(final String scriptName, final long timeout)
    {
        this(scriptName, timeout, true, true);
    }

    /**
     * Constructor for CallScriptCorrectiveAction
     *
     * @param    scriptName the name of the script / external program to call
     * @param    timeout the amount of time (milliseconds) to wait for <code>scriptName</code> to finish.
     * @param    overidesInvokeMethodCorrectiveAction true if this CorrectiveAction overides
     * {@link InvokeMethodCorrectiveAction}<code>s</code>
     * @param    overidesRestartAgentCorrectiveAction true if this CorrectiveAction overides
     * {@link RestartAgentCorrectiveAction}<code>s</code>
     */
    public CallScriptCorrectiveAction(final String scriptName, final long timeout,
                                      final boolean overidesInvokeMethodCorrectiveAction,
                                      final boolean overidesRestartAgentCorrectiveAction)
    {
        m_scriptName = scriptName;
        m_timeout = timeout;

        // Let the delay between checks of Process.exitValue be the minimum of 1/4 of timeout or
        // 5 seconds
        m_delayTime = java.lang.Math.min(m_timeout / 4, 5000);

        m_overidesInvokeMethodCorrectiveAction = overidesInvokeMethodCorrectiveAction;
        m_overidesRestartAgentCorrectiveAction = overidesRestartAgentCorrectiveAction;
    }

    /**
     * Apply this CorrectiveAction, i.e.&nbsp;calls a script / external program.
     *
     * @return the exit value of the script / external program
     * @throws Exception
     */
    protected boolean applyImpl() throws Exception
    {
        final long startTime = System.currentTimeMillis();

        Process process = Runtime.getRuntime().exec(m_scriptName);

        Integer exitValue = null;

        while(exitValue == null)
        {
            try
            {
                exitValue = new Integer(process.exitValue());
            }
            catch(IllegalThreadStateException itse)
            {
                exitValue = null;
            }

            if((System.currentTimeMillis() - startTime) <= m_timeout)
            {
                try
                {
                    Thread.currentThread().sleep(m_delayTime);
                }
                catch(Exception e)
                {
                    exitValue = null;
                }
            }
            else
            {
                break;
            }
        }

        return ((exitValue != null) && (exitValue.intValue() == 0));
    }

    /**
     * Returns true if <code>correctiveAction</code> overides this CorrectiveAction
     *
     * @param    correctiveAction the Corrective to compare to this one.
     *
     * @return true if <code>correctiveAction</code> overides this CorrectiveAction
     */
    public boolean isOverridenBy(final CorrectiveAction correctiveAction)
    {
        return false;
    }

    /**
     * Returns whether this CorrectiveAction overides
     * {@link InvokeMethodCorrectiveAction}<code>s</code>
     *
     * @return whether this CorrectiveAction overides
     * {@link InvokeMethodCorrectiveAction}<code>s</code>
     */
    public boolean getOveridesInvokeMethodCorrectiveAction()
    {
        return m_overidesInvokeMethodCorrectiveAction;
    }

    /**
     * Returns whether this CorrectiveAction overides
     * {@link RestartAgentCorrectiveAction}<code>s</code>
     *
     * @return whether this CorrectiveAction overides
     * {@link RestartAgentCorrectiveAction}<code>s</code>
     */
    public boolean getOveridesRestartAgentCorrectiveAction()
    {
        return m_overidesRestartAgentCorrectiveAction;
    }

    /** The script / external program to call */
    private String m_scriptName;
    /** The amount of time in milliseconds to wait for the script / external program to finish */
    private long m_timeout;
    /** The amount of time between checks to see if the script / external program has finished */
    private long m_delayTime;

    /** Flag indicating whether this CorrectiveAction overides
     *  {@link InvokeMethodCorrectiveAction}<code>s</code> */
    boolean m_overidesInvokeMethodCorrectiveAction;
    /** Flag indicating whether this CorrectiveAction overides
     *  {@link RestartAgentCorrectiveAction}<code>s</code> */
    boolean m_overidesRestartAgentCorrectiveAction;
}
