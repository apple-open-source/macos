/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean;

// Standard Java Packages
import java.util.List;

/**
 * JMX MBean interface for the Watchdog class
 *
 * @author Stacy Curl
 */
public interface WatchdogMBean
    extends StartableMBean
{
    /**
     * Gets the RMI Binding of the JMX Agent that is being watched.
     *
     * @return the RMI Binding of the JMX Agent that is being watched.
     */
    public String getRmiAgentBinding();

    /**
     * Gets the number of StartableMBeans that are being watched.
     *
     * @return the number of StartableMBeans that are being watched.
     */
    public int getNumWatched();

    /**
     * Gets the number of StartableMBeans that are running.
     *
     * @return the number of StartableMBeans that are running.
     */
    public int getNumRunning();

    /**
     * Gets the number of StartableMBeans that are not running.
     * TODO: Change name of this, not running doesn't imply stopped, the mbeans can be in either
     * FAILED, FAILED_TO_START, FAILED_TO_STOP, STARTING, STOPPING, RESTARTING, OR STOPPED states.
     *
     * @return the number of StartableMBeans that are not running.
     */
    public int getNumStopped();

    /**
     * Get whether all the StartableMBeans being watched are running.
     *
     * @return whether all the StartableMBeans being watched are running.
     */
    public boolean getAllRunning();

    /**
     * Get the amount of time in milliseconds between watching.
     *
     * @return the amount of time in milliseconds between watching.
     */
    public long getGranularity();

    /**
     * Sets the amount of time in milliseconds between watching runs.
     *
     * @param    granularity the amount of time in milliseconds between watching.
     */
    public void setGranularity(long granularity);

    /**
     * Gets the System time that watching started.
     *
     * @return the System time that watching started.
     */
    public long getTimeStartedWatching();

    /**
     * Gets the System time that the last watching run started.
     *
     * @return the System time that the last watching run started.
     */
    public long getTimeLastWatched();

    /**
     * Gets the number of times to attempt MBean restart
     *
     * @return the number of times to attempt MBean Restart
     */
    public int getNumTimesToAttemptMBeanRestart();

    /**
     * Gets the number of times to attempt MBean reregister
     *
     * @return the number of times to attempt MBean Reregister
     */
    public int getNumTimesToAttemptMBeanReregister();

    /**
     * Gets the number of times to attempt Agent restart
     *
     * @return the number of times to attempt Agent Restart
     */
    public int getNumTimesToAttemptAgentRestart();

    /**
     * Gets the number of times to attempt machine restart
     *
     * @return the number of times to attempt machine restart
     */
    public int getNumTimesToAttemptMachineRestart();

    /**
     * Sets the number of times to attempt MBean restart
     *
     * @param    numTimesToAttemptMBeanRestart the number of times to attempt MBean restart
     */
    public void setNumTimesToAttemptMBeanRestart(int numTimesToAttemptMBeanRestart);

    /**
     * Sets the number of times to attempt MBean reregister
     *
     * @param    numTimesToAttemptMBeanReregister the number of times to attempt MBean reregister
     */
    public void setNumTimesToAttemptMBeanReregister(int numTimesToAttemptMBeanReregister);

    /**
     * Sets the number of times to attempt Agent restart
     *
     * @param    numTimesToAttemptAgentRestart the number of times to attempt Agent restart
     */
    public void setNumTimesToAttemptAgentRestart(int numTimesToAttemptAgentRestart);

    /**
     * Sets the number of times to attempt machine restart
     *
     * @param    numTimesToAttemptMachineRestart the number of times to attempt machine restart
     */
    public void setNumTimesToAttemptMachineRestart(int numTimesToAttemptMachineRestart);

    /**
     * Gets the maximum number of times to attempt an MBean restart
     *
     * @return the maximum number of times to attempt an MBean restart
     */
    public int getMaxNumTimesToAttemptMBeanRestart();

    /**
     * Gets the maximum number of times to attempt an MBean reregister
     *
     * @return the maximum number of times to attempt an MBean reregister
     */
    public int getMaxNumTimesToAttemptMBeanReregister();

    /**
     * Gets the maximum number of times to attempt an Agent restart
     *
     * @return the maximum number of times to attempt an Agent restart
     */
    public int getMaxNumTimesToAttemptAgentRestart();

    /**
     * Gets the maximum number of times to attempt a machine restart
     *
     * @return the maximum number of times to attempt a machine restart
     */
    public int getMaxNumTimesToAttemptMachineRestart();
}
