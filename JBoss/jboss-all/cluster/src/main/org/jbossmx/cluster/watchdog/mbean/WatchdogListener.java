/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean;

/**
 * Recipient of WatchdogEvents
 *
 * @author Stacy Curl
 */
public interface WatchdogListener
{
    /**
     * Receive a WatchdogEvent
     *
     * @param    watchdogEvent the WatchdogEvent
     *
     * @return whether the WatchdogEvent was received and delt with successfully.
     */
    public boolean receiveEvent(WatchdogEvent watchdogEvent);
}
