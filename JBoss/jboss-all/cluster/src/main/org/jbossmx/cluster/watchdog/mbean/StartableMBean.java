/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean;

/**
 * The MBean interface for a monitorable MBean
 *
 * @author Stacy Curl
 */
public interface StartableMBean extends org.jbossmx.cluster.watchdog.util.SkinableMBean
{
    /**
     * Starts the MBean, delegates implementation to the <code>startMBeanImpl</code> method. The
     * MBean will go through several states:
     *
     * <p>(STOPPED | FAILED_TO_START) -> STARTING -> (RUNNING | FAILED_TO_START)
     *
     * @return whether the MBean started
     */
    public boolean startMBean();

    /**
     * Stops the MBean, delegates implementation to the <code>stopMBeanImpl</code> method. The MBean
     * will go through several states:
     *
     * <p> (RUNNING | FAILED_TO_STOP) -> STOPPING -> (STOPPED | FAILED_TO_STOP)
     *
     * @return whether the MBean stopped.
     */
    public boolean stopMBean();

    /**
     * Restart an MBean, delegates implementation to <code>restartMBeanImpl</code>. The MBean will
     * go through several states:
     *
     * <p> FAILED -> RESTARTING -> (RUNNING | FAILED)
     *
     * @return
     */
    public boolean restartMBean();

    /**
     * Gets the state of the MBean
     *
     * @return the state of the MBean
     */
    public int retrieveMBeanState();

    /**
     * Gets the state of the MBean as a String
     *
     * @return the state of the MBean as a String
     */
    public String getMBeanStateString();

    /**
     * Simulates failure in the MBean.
     */
    public void simulateFailure();
}
