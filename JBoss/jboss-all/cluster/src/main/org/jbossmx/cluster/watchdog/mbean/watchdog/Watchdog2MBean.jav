/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean.watchdog;

// Hermes JMX Packages
import org.jbossmx.cluster.watchdog.mbean.StartableMBean;

// Standard Java Packages
import java.util.List;

/**
 * JMX MBean interface for the refactored Watchdog class
 *
 * @author Stacy Curl
 */
public interface Watchdog2MBean
    extends StartableMBean
{
    /**
     *
     * @return
     */
    public String getRmiAgentBinding();

    /**
     *
     * @return
     */
    public int getNumWatched();

    /**
     *
     * @return
     */
    public int getNumRunning();

    /**
     *
     * @return
     */
    public int getNumStopped();

    /**
     *
     * @return
     */
    public boolean getAllRunning();

    /**
     *
     * @return
     */
    public long getGranularity();

    /**
     * @param    granularity
     */
    public void setGranularity(long granularity);

    /**
     *
     * @return
     */
    public long getTimeStartedWatching();

    /**
     *
     * @return
     */
    public long getTimeLastWatched();
}
