/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean;

// Standard Java Packages

import java.util.ArrayList;
import java.util.List;
import java.util.Iterator;

/**
 * This class facilitates the distribution of WatchdogEvents to WatchdogListeners
 *
 * @author Stacy Curl
 */
public class WatchdogEventDispatcherSupport
    implements WatchdogEventDispatcher
{
    /**
     * Constructor for WatchdogEventDispatcherSupport
     */
    public WatchdogEventDispatcherSupport() {}

    /**
     * Adds a listener to Watchdog Events
     *
     * @param    watchdogListener the listener
     *
     * @return whether the listener was added
     */
    public boolean addListener(WatchdogListener watchdogListener)
    {
        boolean added = false;

        if(m_listeners == null)
        {
            m_listeners = new ArrayList();
        }

        if(m_listeners.contains(watchdogListener))
        {
            m_listeners.add(watchdogListener);

            added = true;
        }

        return added;
    }

    /**
     * Adds a listener to Watchdog Events
     *
     * @param    className the class of the listener
     * @param    param a constructor parameter for <code>className</code>
     *
     * @return whether the listener was added
     */
    public boolean addListener(String className, String constructorParams)
    {
        boolean added = false;

        try
        {
            Object listener = Class.forName(className).getConstructor(new Class[]{ String.class })
                .newInstance(new Object[]{ constructorParams });

            if(m_listeners == null)
            {
                m_listeners = new ArrayList();
            }

            m_listeners.add(listener);

            added = true;
        }
        catch(Exception e)
        {
            e.printStackTrace();

            added = false;
        }

        return added;
    }

    /**
     * Removes a listener
     *
     * @param    watchdogListener the listener to remove
     *
     * @return whether the listener was removed
     */
    public boolean removeListener(WatchdogListener watchdogListener)
    {
        boolean removed = false;

        if((m_listeners != null) && m_listeners.contains(watchdogListener))
        {
            m_listeners.remove(watchdogListener);

            removed = true;
        }

        return removed;
    }

    /**
     * Removes a listener
     *
     * @param    index the index of the listener
     *
     * @return whether the listener was removed
     */
    public boolean removeListener(int index)
    {
        boolean removed = false;

        if(m_listeners != null & m_listeners.size() > index)
        {
            m_listeners.remove(index);

            removed = true;
        }

        return removed;
    }

    /**
     * Get a list of the listeners
     *
     * @return a list of the listeners
     */
    public List getListeners()
    {
        return m_listeners;
    }

    /**
     * Dispatches a WatchdogEvent to all the listeners
     *
     * @param    watchdogEvent the WatchdogEvent
     */
    public void dispatchEvent(WatchdogEvent watchdogEvent)
    {
        if(m_listeners != null)
        {
            for(Iterator i = m_listeners.iterator(); i.hasNext(); )
            {
                WatchdogListener watchdogListener = (WatchdogListener) i.next();

                watchdogListener.receiveEvent(watchdogEvent);
            }
        }
    }

    /** The listeners */
    private List m_listeners;
}
