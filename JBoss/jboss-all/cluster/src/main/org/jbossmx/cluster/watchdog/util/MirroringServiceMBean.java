/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.util;

/**
 * @author Stacy Curl
 */
public interface MirroringServiceMBean
{
    /**
     *
     * @return
     */
    public boolean startMirroring();

    /**
     *
     * @return
     */
    public boolean stopMirroring();
}
