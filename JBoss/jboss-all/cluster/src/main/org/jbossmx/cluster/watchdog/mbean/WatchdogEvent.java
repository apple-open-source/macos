/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean;

import java.io.Serializable;

/**
 * Class for representing problematic Watchdog events.
 *
 * @author Stacy Curl
 */
final public class WatchdogEvent implements Serializable
{
    /**
     * Constructor for WatchdogEvent
     *
     * @param    event a description of the event
     */
    public WatchdogEvent(String event)
    {
        m_event = event;
    }

    /**
     * Gets a description of the event.
     *
     * @return a description of the event.
     */
    public String getEvent()
    {
        return m_event;
    }

    /** A description of the event */
    private String m_event;
}
