/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean;

/**
 * @author Stacy Curl
 */
public interface WatchdogEventManagerMBean
{
    /**
     * @param    recipientClass
     * @param    initialisation
     *
     * @return
     */
    public boolean addExternelEventRecipient(String recipientClass, String initialisation);

    /**
     *
     * @return
     */
    public WatchdogEventManagerRemoteInterface getRemoteInterface();
}
