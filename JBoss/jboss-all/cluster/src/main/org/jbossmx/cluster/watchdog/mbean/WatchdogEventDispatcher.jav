/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean;

// 3rd Party Packages
import java.util.List;

/**
 * Interface for classes which send WatchdogEvents
 *
 * @author Stacy Curl
 */
public interface WatchdogEventDispatcher
{
    /**
     * Adds a listener to Watchdog Events
     *
     * @param    watchdogListener the listener
     *
     * @return whether the listener was added
     */
    public boolean addListener(WatchdogListener watchdogListener);

    /**
     * Adds a listener to Watchdog Events
     *
     * @param    className the class of the listener
     * @param    param a constructor parameter for <code>className</code>
     *
     * @return whether the listener was added
     */
    public boolean addListener(String className, String param);

    /**
     * Removes a listener
     *
     * @param    index the index of the listener
     *
     * @return whether the listener was removed
     */
    public boolean removeListener(int index);

    /**
     * Removes a listener
     *
     * @param    watchdogListener the listener to remove
     *
     * @return whether the listener was removed
     */
    public boolean removeListener(WatchdogListener watchdogListener);

    /**
     * Get a list of the listeners
     *
     * @return a list of the listeners
     */
    public List getListeners();

    /**
     * Dispatches a WatchdogEvent to all the listeners
     *
     * @param    watchdogEvent the WatchdogEvent
     */
    public void dispatchEvent(WatchdogEvent watchdogEvent);
}
